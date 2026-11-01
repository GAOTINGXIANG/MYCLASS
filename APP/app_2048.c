/**
 * @file app_2048.c
 * @brief 2048：4x4 棋盘，摇杆/编码器选方向，相同数字合并翻倍。
 *   摇杆       = 上/下/左/右滑动
 *   编码器     = 左/右滑动
 *   ENC/JS 键  = 结束后重开
 *   KEY1       = 返回主菜单
 */
#include "app_2048.h"
#include "lvgl.h"
#include "encoder.h"
#include "keys.h"
#include "joystick.h"

#define N          4
#define ORG        35
#define CELL       40
#define TILE       (CELL - 3)
#define PERIOD     30
#define ENC_DETENT 4

static lv_timer_t *s_tm;
static lv_obj_t   *s_tile[N][N];
static lv_obj_t   *s_score, *s_msg;
static uint16_t    s_v[N][N];
static uint32_t    s_rng = 0x12345678UL;
static uint32_t    s_score_val;
static uint8_t     s_over;
static int32_t     s_last_cnt;
static joy_mask_t  s_prev_mask = JOY_MASK_NONE;

static uint32_t rnd(void)
{
    s_rng = s_rng * 1103515245UL + 12345UL;
    return (s_rng >> 16) & 0x7FFFUL;
}

static lv_color_t tile_color(uint16_t v)
{
    switch (v) {
    case 0:      return lv_palette_darken(LV_PALETTE_GREY, 3);
    case 2:
    case 4:      return lv_palette_main(LV_PALETTE_GREY);
    case 8:      return lv_palette_main(LV_PALETTE_ORANGE);
    case 16:     return lv_palette_main(LV_PALETTE_DEEP_ORANGE);
    case 32:     return lv_palette_main(LV_PALETTE_RED);
    case 64:     return lv_palette_main(LV_PALETTE_PINK);
    case 128:    return lv_palette_main(LV_PALETTE_YELLOW);
    case 256:    return lv_palette_main(LV_PALETTE_LIME);
    case 512:    return lv_palette_main(LV_PALETTE_GREEN);
    case 1024:   return lv_palette_main(LV_PALETTE_CYAN);
    case 2048:   return lv_palette_main(LV_PALETTE_BLUE);
    default:     return lv_palette_darken(LV_PALETTE_BLUE, 2);
    }
}

static void render(void)
{
    for (uint8_t r = 0; r < N; r++) {
        for (uint8_t c = 0; c < N; c++) {
            uint16_t v = s_v[r][c];
            lv_obj_set_style_bg_color(s_tile[r][c], tile_color(v), 0);
            if (v != 0U) lv_label_set_text_fmt(s_tile[r][c], "%u", v);
            else         lv_label_set_text(s_tile[r][c], "");
        }
    }
    lv_label_set_text_fmt(s_score, "得分 %u", s_score_val);
}

static void msg_show(const char *txt)
{
    lv_label_set_text(s_msg, txt);
    lv_obj_clear_flag(s_msg, LV_OBJ_FLAG_HIDDEN);
}

static uint8_t spawn(void)
{
    uint8_t empties = 0;
    uint32_t pick;

    for (uint8_t r = 0; r < N; r++) {
        for (uint8_t c = 0; c < N; c++) {
            if (s_v[r][c] == 0U) empties++;
        }
    }
    if (empties == 0U) return 0U;

    pick = rnd() % empties;
    for (uint8_t r = 0; r < N; r++) {
        for (uint8_t c = 0; c < N; c++) {
            if (s_v[r][c] != 0U) continue;
            if (pick == 0U) {
                s_v[r][c] = ((rnd() % 10U) < 2U) ? 4U : 2U;
                return 1U;
            }
            pick--;
        }
    }
    return 0U;
}

/* dx/dy 取 -1/0/+1，表示滑动方向；返回 1 表示棋盘发生变化 */
static uint8_t slide(int8_t dx, int8_t dy)
{
    uint8_t moved = 0;

    for (uint8_t line = 0; line < N; line++) {
        uint16_t in[N] = { 0 }, out[N] = { 0 };
        uint8_t  rr[N], cc[N];
        uint8_t  n = 0;

        /* 按"离目标边界最近"的顺序取一行/列 */
        for (uint8_t k = 0; k < N; k++) {
            uint8_t r, c;
            if (dx != 0) {
                r = line;
                c = (dx > 0) ? (uint8_t)(N - 1 - k) : k;
            } else {
                c = line;
                r = (dy > 0) ? (uint8_t)(N - 1 - k) : k;
            }
            rr[k] = r;
            cc[k] = c;
            in[k] = s_v[r][c];
        }

        /* 紧凑 */
        for (uint8_t k = 0; k < N; k++) {
            if (in[k] != 0U) out[n++] = in[k];
        }

        /* 相邻相同合并 */
        for (uint8_t k = 0; k + 1U < n; k++) {
            if (out[k] == out[k + 1U]) {
                out[k] = (uint16_t)(out[k] * 2U);
                s_score_val += out[k];
                for (uint8_t j = (uint8_t)(k + 1U); j + 1U < n; j++) {
                    out[j] = out[j + 1U];
                }
                n--;
                out[n] = 0U;
            }
        }

        /* 写回 */
        for (uint8_t k = 0; k < N; k++) {
            if (in[k] != out[k]) moved = 1U;
            s_v[rr[k]][cc[k]] = out[k];
        }
    }
    return moved;
}

static uint8_t can_move(void)
{
    for (uint8_t r = 0; r < N; r++) {
        for (uint8_t c = 0; c < N; c++) {
            if (s_v[r][c] == 0U) return 1U;
            if (c + 1U < N && s_v[r][c] == s_v[r][c + 1U]) return 1U;
            if (r + 1U < N && s_v[r][c] == s_v[r + 1U][c]) return 1U;
        }
    }
    return 0U;
}

static void new_game(void)
{
    for (uint8_t r = 0; r < N; r++) {
        for (uint8_t c = 0; c < N; c++) s_v[r][c] = 0U;
    }
    s_score_val = 0;
    s_over = 0;
    s_prev_mask = JOY_MASK_NONE;
    s_last_cnt = encoder_count();
    lv_obj_add_flag(s_msg, LV_OBJ_FLAG_HIDDEN);
    spawn();
    spawn();
    render();
}

static void do_move(int8_t dx, int8_t dy)
{
    if (slide(dx, dy) == 0U) return;
    spawn();
    render();
    if (can_move() == 0U) {
        s_over = 1;
        msg_show("游戏结束");
    }
}

static void g2048_cb(lv_timer_t *t)
{
    int8_t  dx = 0, dy = 0;
    uint8_t edge;
    int32_t d;
    (void)t;

    edge = keys_scan();
    if (s_over != 0U) {
        if ((edge & (KEY_MASK(KEY_ID_JS) | KEY_MASK(KEY_ID_ENC))) != 0U) new_game();
        return;
    }

    /* 编码器：左右滑动 */
    d = encoder_count() - s_last_cnt;
    s_last_cnt += d;
    if (d >= ENC_DETENT)       dx = 1;
    else if (d <= -ENC_DETENT) dx = -1;

    /* 摇杆：方向沿（横向优先） */
    {
        joy_mask_t m = joy_get_mask();
        if (m != s_prev_mask) {
            if ((m & JOY_MASK_RIGHT) != 0U)      { dx = 1;  dy = 0; }
            else if ((m & JOY_MASK_LEFT) != 0U)  { dx = -1; dy = 0; }
            else if ((m & JOY_MASK_DOWN) != 0U)  { dx = 0;  dy = 1; }
            else if ((m & JOY_MASK_UP) != 0U)    { dx = 0;  dy = -1; }
            s_prev_mask = m;
        }
    }

    if (dx != 0 || dy != 0) do_move(dx, dy);
}

static void app_2048_enter(void)
{
    lv_obj_t *scr = lv_scr_act();

    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

    for (uint8_t r = 0; r < N; r++) {
        for (uint8_t c = 0; c < N; c++) {
            lv_obj_t *o = lv_label_create(scr);
            lv_obj_set_size(o, TILE, TILE);
            lv_obj_set_style_radius(o, 4, 0);
            lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
            lv_obj_set_style_text_align(o, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_set_style_text_color(o, lv_color_white(), 0);
            lv_obj_set_pos(o, ORG + c * CELL, ORG + r * CELL);
            s_tile[r][c] = o;
        }
    }

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
    s_tm = lv_timer_create(g2048_cb, PERIOD, NULL);
}

static void app_2048_exit(void)
{
    if (s_tm != NULL) { lv_timer_del(s_tm); s_tm = NULL; }
}

const app_t app_2048 = { "2048", app_2048_enter, app_2048_exit };
