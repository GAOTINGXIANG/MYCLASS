/**
 * @file app_beans.c
 * @brief 打豆豆：8x8 彩色豆子，同一颜色相连（>=2）即可消除，上方下落、空列左移。
 *   摇杆 / 编码器 = 移动光标
 *   ENC/JS 键     = 消除
 *   KEY1          = 返回主菜单
 *   消除得分 = 豆子数 x 10，无可消组时游戏结束。
 */
#include "app_beans.h"
#include "lvgl.h"
#include "encoder.h"
#include "keys.h"
#include "joystick.h"

#define BN         8
#define CELL       20
#define ORG        35
#define PERIOD     30
#define ENC_DETENT 4
#define COLORS     4
#define EMPTY      0xFFU

static lv_timer_t *s_tm;
static lv_obj_t   *s_cell[BN][BN];
static lv_obj_t   *s_cursor, *s_score, *s_msg;
static uint8_t     s_col[BN][BN];
static uint8_t     s_cx, s_cy;
static uint32_t    s_rng = 0x2468ACE0UL;
static uint32_t    s_score_val;
static uint8_t     s_over;
static int32_t     s_last_cnt;
static joy_mask_t  s_prev_mask = JOY_MASK_NONE;

static const lv_palette_t PAL[COLORS] = {
    LV_PALETTE_RED, LV_PALETTE_YELLOW, LV_PALETTE_GREEN, LV_PALETTE_CYAN,
};

static uint32_t rnd(void)
{
    s_rng = s_rng * 1103515245UL + 12345UL;
    return (s_rng >> 16) & 0x7FFFUL;
}

static void render(void)
{
    for (uint8_t r = 0; r < BN; r++) {
        for (uint8_t c = 0; c < BN; c++) {
            if (s_col[r][c] == EMPTY) {
                lv_obj_set_style_bg_opa(s_cell[r][c], LV_OPA_TRANSP, 0);
            } else {
                lv_obj_set_style_bg_opa(s_cell[r][c], LV_OPA_COVER, 0);
                lv_obj_set_style_bg_color(s_cell[r][c],
                    lv_palette_main(PAL[s_col[r][c]]), 0);
            }
        }
    }
    lv_label_set_text_fmt(s_score, "得分 %u", s_score_val);
}

static void cursor_render(void)
{
    lv_obj_set_pos(s_cursor, ORG + s_cx * CELL + 1, ORG + s_cy * CELL + 1);
}

/* 统计 (sr,sc) 起同色相连块，并标记 drop[] 待消 */
static uint8_t collect(uint8_t sr, uint8_t sc, uint8_t *drop)
{
    uint8_t stack[BN * BN];
    uint8_t seen[BN * BN];
    uint8_t sp = 0, n = 0;
    uint8_t col = s_col[sr][sc];

    for (uint8_t i = 0; i < BN * BN; i++) { seen[i] = 0U; drop[i] = 0U; }
    if (col == EMPTY) return 0U;

    stack[sp++] = (uint8_t)(sr * BN + sc);
    seen[sr * BN + sc] = 1U;

    while (sp > 0U) {
        uint8_t idx = stack[--sp];
        uint8_t r = (uint8_t)(idx / BN);
        uint8_t c = (uint8_t)(idx % BN);
        drop[idx] = 1U;
        n++;

        if (r > 0U && seen[idx - BN] == 0U && s_col[r - 1U][c] == col) {
            seen[idx - BN] = 1U; stack[sp++] = (uint8_t)(idx - BN);
        }
        if (r + 1U < BN && seen[idx + BN] == 0U && s_col[r + 1U][c] == col) {
            seen[idx + BN] = 1U; stack[sp++] = (uint8_t)(idx + BN);
        }
        if (c > 0U && seen[idx - 1U] == 0U && s_col[r][c - 1U] == col) {
            seen[idx - 1U] = 1U; stack[sp++] = (uint8_t)(idx - 1U);
        }
        if (c + 1U < BN && seen[idx + 1U] == 0U && s_col[r][c + 1U] == col) {
            seen[idx + 1U] = 1U; stack[sp++] = (uint8_t)(idx + 1U);
        }
    }
    return n;
}

/* 列内下落 + 空列左移 */
static void gravity(void)
{
    uint8_t tmp[BN][BN];
    uint8_t cols[BN], nc = 0;

    for (uint8_t c = 0; c < BN; c++) {
        uint8_t rw = BN;
        for (uint8_t r = BN; r > 0U; r--) {
            if (s_col[r - 1U][c] != EMPTY) {
                rw--;
                tmp[rw][c] = s_col[r - 1U][c];
            }
        }
        for (uint8_t r = 0; r < rw; r++) tmp[r][c] = EMPTY;
    }

    for (uint8_t c = 0; c < BN; c++) {
        uint8_t full = 0;
        for (uint8_t r = 0; r < BN; r++) {
            if (tmp[r][c] != EMPTY) { full = 1U; break; }
        }
        if (full != 0U) cols[nc++] = c;
    }

    for (uint8_t c = 0; c < BN; c++) {
        for (uint8_t r = 0; r < BN; r++) {
            s_col[r][c] = (c < nc) ? tmp[r][cols[c]] : EMPTY;
        }
    }
}

/* 是否还存在可消组（>=2 同色相邻） */
static uint8_t has_group(void)
{
    for (uint8_t r = 0; r < BN; r++) {
        for (uint8_t c = 0; c < BN; c++) {
            if (s_col[r][c] == EMPTY) continue;
            if (c + 1U < BN && s_col[r][c] == s_col[r][c + 1U]) return 1U;
            if (r + 1U < BN && s_col[r][c] == s_col[r + 1U][c]) return 1U;
        }
    }
    return 0U;
}

static void new_game(void)
{
    do {
        for (uint8_t r = 0; r < BN; r++) {
            for (uint8_t c = 0; c < BN; c++) {
                s_col[r][c] = (uint8_t)(rnd() % COLORS);
            }
        }
        gravity();      /* 避免开局就有空列 */
    } while (has_group() == 0U);

    s_cx = BN / 2;
    s_cy = BN / 2;
    s_score_val = 0;
    s_over = 0;
    s_prev_mask = JOY_MASK_NONE;
    s_last_cnt = encoder_count();
    lv_obj_add_flag(s_msg, LV_OBJ_FLAG_HIDDEN);
    render();
    cursor_render();
}

static void pop(void)
{
    uint8_t drop[BN * BN];
    uint8_t n = collect(s_cy, s_cx, drop);

    if (n < 2U) return;

    for (uint8_t i = 0; i < BN * BN; i++) {
        if (drop[i] != 0U) s_col[i / BN][i % BN] = EMPTY;
    }
    s_score_val += (uint32_t)n * 10U;
    gravity();
    render();

    if (has_group() == 0U) {
        s_over = 1;
        lv_label_set_text(s_msg, "游戏结束");
        lv_obj_clear_flag(s_msg, LV_OBJ_FLAG_HIDDEN);
    }
}

static void beans_cb(lv_timer_t *t)
{
    uint8_t edge, moved = 0;
    int32_t d;
    (void)t;

    edge = keys_scan();
    if (s_over != 0U) {
        if ((edge & (KEY_MASK(KEY_ID_JS) | KEY_MASK(KEY_ID_ENC))) != 0U) new_game();
        return;
    }

    d = encoder_count() - s_last_cnt;
    s_last_cnt += d;

    {
        joy_mask_t m = joy_get_mask();
        if (m != s_prev_mask) {
            if ((m & JOY_MASK_UP) != 0U && s_cy > 0U)                { s_cy--; moved = 1; }
            else if ((m & JOY_MASK_DOWN) != 0U && s_cy < BN - 1U)    { s_cy++; moved = 1; }
            else if ((m & JOY_MASK_LEFT) != 0U && s_cx > 0U)         { s_cx--; moved = 1; }
            else if ((m & JOY_MASK_RIGHT) != 0U && s_cx < BN - 1U)   { s_cx++; moved = 1; }
            s_prev_mask = m;
        }
    }

    while (d >= ENC_DETENT)  { d -= ENC_DETENT; if (s_cx < BN - 1U) { s_cx++; moved = 1; } }
    while (d <= -ENC_DETENT) { d += ENC_DETENT; if (s_cx > 0U)      { s_cx--; moved = 1; } }

    if (moved != 0U) cursor_render();

    if ((edge & (KEY_MASK(KEY_ID_JS) | KEY_MASK(KEY_ID_ENC))) != 0U) pop();
}

static void app_beans_enter(void)
{
    lv_obj_t *scr = lv_scr_act();

    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

    for (uint8_t r = 0; r < BN; r++) {
        for (uint8_t c = 0; c < BN; c++) {
            lv_obj_t *o = lv_obj_create(scr);
            lv_obj_remove_style_all(o);
            lv_obj_set_size(o, CELL - 2, CELL - 2);
            lv_obj_set_pos(o, ORG + c * CELL + 1, ORG + r * CELL + 1);
            lv_obj_set_style_radius(o, 4, 0);
            lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
            s_cell[r][c] = o;
        }
    }

    s_cursor = lv_obj_create(scr);
    lv_obj_remove_style_all(s_cursor);
    lv_obj_set_size(s_cursor, CELL - 4, CELL - 4);
    lv_obj_set_style_radius(s_cursor, 4, 0);
    lv_obj_set_style_border_width(s_cursor, 2, 0);
    lv_obj_set_style_border_color(s_cursor, lv_color_white(), 0);

    s_score = lv_label_create(scr);
    lv_obj_set_style_text_color(s_score, lv_color_white(), 0);
    lv_obj_align(s_score, LV_ALIGN_BOTTOM_MID, 0, -12);

    s_msg = lv_label_create(scr);
    lv_obj_set_style_text_color(s_msg, lv_color_white(), 0);
    lv_obj_set_style_bg_color(s_msg, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_msg, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(s_msg, 6, 0);
    lv_obj_align(s_msg, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(s_msg, LV_OBJ_FLAG_HIDDEN);

    new_game();
    s_tm = lv_timer_create(beans_cb, PERIOD, NULL);
}

static void app_beans_exit(void)
{
    if (s_tm != NULL) { lv_timer_del(s_tm); s_tm = NULL; }
}

const app_t app_beans = { "打豆豆", app_beans_enter, app_beans_exit };
