/**
 * @file app_tetris.c
 * @brief 俄罗斯方块：10x20 竖屏，自定义绘制（不占对象内存）。
 *   摇杆 / 编码器 = 左右移动
 *   摇杆下        = 加速下落
 *   ENC/JS 键     = 旋转
 *   KEY2          = 直接落底
 *   KEY1          = 返回主菜单
 */
#include "app_tetris.h"
#include "lvgl.h"
#include "encoder.h"
#include "keys.h"
#include "joystick.h"

#define TW         10
#define TH         20
#define CELL       8
#define ORG_X      ((240 - TW * CELL) / 2)
#define ORG_Y      35
#define PERIOD     20
#define ENC_DETENT 4
#define FALL_MS    400
#define MOVE_REP   8      /* 左右长按重复间隔（拍数，8*20ms = 160ms） */
#define SOFT_REP   3      /* 加速下落间隔（拍数） */

static lv_timer_t *s_tm;
static lv_obj_t   *s_field, *s_score, *s_msg;
static uint8_t     s_b[TH][TW];        /* 0 = 空，1~7 = 颜色 */
static uint16_t    s_sh[4];            /* 当前方块 4 行，bit0 = 最左列 */
static uint8_t     s_pid;              /* 方块形状号 0~6（决定颜色） */
static int8_t      s_px, s_py;
static uint8_t     s_over;
static uint16_t    s_fall_acc;
static uint8_t     s_move_dir, s_move_acc, s_soft_acc;
static uint32_t    s_rng = 0x13579BDFUL;
static uint32_t    s_lines;
static int32_t     s_last_cnt;
static joy_mask_t  s_prev_mask = JOY_MASK_NONE;

static const uint16_t SHAPES[7][4] = {
    { 0x000F, 0x0000, 0x0000, 0x0000 },   /* I */
    { 0x0003, 0x0003, 0x0000, 0x0000 },   /* O */
    { 0x0007, 0x0002, 0x0000, 0x0000 },   /* T */
    { 0x0006, 0x0003, 0x0000, 0x0000 },   /* S */
    { 0x0003, 0x0006, 0x0000, 0x0000 },   /* Z */
    { 0x0007, 0x0001, 0x0000, 0x0000 },   /* J */
    { 0x0007, 0x0004, 0x0000, 0x0000 },   /* L */
};

static const lv_palette_t PAL[7] = {
    LV_PALETTE_CYAN, LV_PALETTE_YELLOW, LV_PALETTE_PURPLE,
    LV_PALETTE_GREEN, LV_PALETTE_RED, LV_PALETTE_BLUE, LV_PALETTE_ORANGE,
};

static uint32_t rnd(void)
{
    s_rng = s_rng * 1103515245UL + 12345UL;
    return (s_rng >> 16) & 0x7FFFUL;
}

/* ---------------- 绘制 ---------------- */
static void block_draw(lv_draw_ctx_t *ctx, lv_coord_t x0, lv_coord_t y0,
                       int8_t c, int8_t r, uint8_t idx)
{
    lv_draw_rect_dsc_t rd;
    lv_area_t a;

    if (r < 0) return;
    lv_draw_rect_dsc_init(&rd);
    rd.bg_opa = LV_OPA_COVER;
    rd.bg_color = lv_palette_main(PAL[idx % 7U]);
    rd.radius = 1;

    a.x1 = (lv_coord_t)(x0 + c * CELL + 1);
    a.y1 = (lv_coord_t)(y0 + r * CELL + 1);
    a.x2 = (lv_coord_t)(x0 + c * CELL + CELL - 2);
    a.y2 = (lv_coord_t)(y0 + r * CELL + CELL - 2);
    lv_draw_rect(ctx, &rd, &a);
}

static void tet_draw(lv_event_t *e)
{
    lv_obj_t      *obj = lv_event_get_target(e);
    lv_draw_ctx_t *ctx = lv_event_get_draw_ctx(e);
    lv_area_t      co;
    lv_draw_rect_dsc_t rd;

    lv_obj_get_coords(obj, &co);

    lv_draw_rect_dsc_init(&rd);
    rd.bg_opa = LV_OPA_TRANSP;
    rd.border_color = lv_palette_main(LV_PALETTE_BLUE_GREY);
    rd.border_width = 1;
    lv_draw_rect(ctx, &rd, &co);

    for (uint8_t r = 0; r < TH; r++) {
        for (uint8_t c = 0; c < TW; c++) {
            if (s_b[r][c] != 0U) block_draw(ctx, co.x1, co.y1, (int8_t)c, (int8_t)r, s_b[r][c]);
        }
    }

    if (s_over == 0U) {
        for (uint8_t r = 0; r < 4U; r++) {
            for (uint8_t c = 0; c < 4U; c++) {
                if ((s_sh[r] & (uint16_t)(1U << c)) != 0U) {
                    block_draw(ctx, co.x1, co.y1,
                               (int8_t)(s_px + (int8_t)c), (int8_t)(s_py + (int8_t)r),
                               (uint8_t)(s_pid + 1U));
                }
            }
        }
    }
}

/* ---------------- 逻辑 ---------------- */
static uint8_t collide(int8_t px, int8_t py, const uint16_t sh[4])
{
    for (uint8_t r = 0; r < 4U; r++) {
        for (uint8_t c = 0; c < 4U; c++) {
            if ((sh[r] & (uint16_t)(1U << c)) == 0U) continue;
            int8_t x = (int8_t)(px + (int8_t)c);
            int8_t y = (int8_t)(py + (int8_t)r);
            if (x < 0 || x >= TW || y >= TH) return 1U;
            if (y < 0) continue;
            if (s_b[y][x] != 0U) return 1U;
        }
    }
    return 0U;
}

static void normalize(uint16_t sh[4])
{
    while (sh[0] == 0U) {
        sh[0] = sh[1]; sh[1] = sh[2]; sh[2] = sh[3]; sh[3] = 0U;
    }
    while (((sh[0] | sh[1] | sh[2] | sh[3]) & 0x0001U) == 0U) {
        for (uint8_t i = 0; i < 4U; i++) sh[i] = (uint16_t)(sh[i] >> 1);
    }
}

static void try_rotate(void)
{
    uint16_t nt[4];

    for (uint8_t r = 0; r < 4U; r++) {
        nt[r] = 0U;
        for (uint8_t c = 0; c < 4U; c++) {
            if ((s_sh[3U - c] & (uint16_t)(1U << r)) != 0U) nt[r] |= (uint16_t)(1U << c);
        }
    }
    normalize(nt);

    if (collide(s_px, s_py, nt) == 0U) {
        for (uint8_t i = 0; i < 4U; i++) s_sh[i] = nt[i];
    } else if (collide((int8_t)(s_px - 1), s_py, nt) == 0U) {
        s_px--;
        for (uint8_t i = 0; i < 4U; i++) s_sh[i] = nt[i];
    } else if (collide((int8_t)(s_px + 1), s_py, nt) == 0U) {
        s_px++;
        for (uint8_t i = 0; i < 4U; i++) s_sh[i] = nt[i];
    }
}

static void spawn(void)
{
    s_pid = (uint8_t)(rnd() % 7U);
    for (uint8_t i = 0; i < 4U; i++) s_sh[i] = SHAPES[s_pid][i];
    s_px = 3;
    s_py = 0;
    s_fall_acc = 0;
    s_move_dir = 0;
    s_move_acc = 0;
    s_soft_acc = 0;

    if (collide(s_px, s_py, s_sh) != 0U) {
        s_over = 1;
        lv_label_set_text(s_msg, "游戏结束");
        lv_obj_clear_flag(s_msg, LV_OBJ_FLAG_HIDDEN);
    }
}

static void clear_lines(void)
{
    for (uint8_t r = 0; r < TH; r++) {
        uint8_t full = 1U;
        for (uint8_t c = 0; c < TW; c++) {
            if (s_b[r][c] == 0U) { full = 0U; break; }
        }
        if (full == 0U) continue;

        for (uint8_t rr = r; rr > 0U; rr--) {
            for (uint8_t c = 0; c < TW; c++) s_b[rr][c] = s_b[rr - 1U][c];
        }
        for (uint8_t c = 0; c < TW; c++) s_b[0][c] = 0U;
        s_lines++;
        if (r > 0U) r--;
    }
    lv_label_set_text_fmt(s_score, "得分 %u", s_lines);
}

static void lock_piece(void)
{
    for (uint8_t r = 0; r < 4U; r++) {
        for (uint8_t c = 0; c < 4U; c++) {
            if ((s_sh[r] & (uint16_t)(1U << c)) == 0U) continue;
            int8_t x = (int8_t)(s_px + (int8_t)c);
            int8_t y = (int8_t)(s_py + (int8_t)r);
            if (y >= 0 && y < TH && x >= 0 && x < TW) {
                s_b[y][x] = (uint8_t)(s_pid + 1U);
            }
        }
    }
    clear_lines();
    spawn();
    lv_obj_invalidate(s_field);
}

static void fall_one(void)
{
    if (collide(s_px, (int8_t)(s_py + 1), s_sh) != 0U) lock_piece();
    else                                              s_py++;
    lv_obj_invalidate(s_field);
}

static void new_game(void)
{
    for (uint8_t r = 0; r < TH; r++) {
        for (uint8_t c = 0; c < TW; c++) s_b[r][c] = 0U;
    }
    s_lines = 0;
    s_over = 0;
    s_prev_mask = JOY_MASK_NONE;
    s_last_cnt = encoder_count();
    lv_obj_add_flag(s_msg, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text_fmt(s_score, "得分 0");
    spawn();
    lv_obj_invalidate(s_field);
}

static void tet_cb(lv_timer_t *t)
{
    uint8_t edge;
    int32_t d;
    (void)t;

    edge = keys_scan();
    if (s_over != 0U) {
        if ((edge & (KEY_MASK(KEY_ID_JS) | KEY_MASK(KEY_ID_ENC))) != 0U) new_game();
        return;
    }

    if ((edge & (KEY_MASK(KEY_ID_JS) | KEY_MASK(KEY_ID_ENC))) != 0U) try_rotate();

    if ((edge & KEY_MASK(KEY_ID_2)) != 0U) {          /* 直接落底 */
        while (collide(s_px, (int8_t)(s_py + 1), s_sh) == 0U) s_py++;
        lock_piece();
        return;
    }

    /* 编码器：左右移动 */
    d = encoder_count() - s_last_cnt;
    s_last_cnt += d;
    while (d >= ENC_DETENT) {
        d -= ENC_DETENT;
        if (collide((int8_t)(s_px + 1), s_py, s_sh) == 0U) s_px++;
    }
    while (d <= -ENC_DETENT) {
        d += ENC_DETENT;
        if (collide((int8_t)(s_px - 1), s_py, s_sh) == 0U) s_px--;
    }

    /* 摇杆左右（长按重复） */
    {
        joy_mask_t m = joy_get_mask();
        if (m != s_prev_mask) { s_move_dir = 0; s_move_acc = 0; }

        if ((m & JOY_MASK_LEFT) != 0U) {
            if (s_move_dir != 1U || ++s_move_acc >= MOVE_REP) {
                s_move_dir = 1U;
                s_move_acc = 0;
                if (collide((int8_t)(s_px - 1), s_py, s_sh) == 0U) s_px--;
            }
        } else if ((m & JOY_MASK_RIGHT) != 0U) {
            if (s_move_dir != 2U || ++s_move_acc >= MOVE_REP) {
                s_move_dir = 2U;
                s_move_acc = 0;
                if (collide((int8_t)(s_px + 1), s_py, s_sh) == 0U) s_px++;
            }
        }
        s_prev_mask = m;
    }

    /* 自然下落 / 摇杆下加速 */
    s_fall_acc += PERIOD;
    if ((joy_get_mask() & JOY_MASK_DOWN) != 0U) {
        if (++s_soft_acc >= SOFT_REP) { s_soft_acc = 0; fall_one(); s_fall_acc = 0; }
    } else if (s_fall_acc >= FALL_MS) {
        s_fall_acc = 0;
        fall_one();
    }

    lv_obj_invalidate(s_field);
}

static void app_tetris_enter(void)
{
    lv_obj_t *scr = lv_scr_act();

    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

    s_field = lv_obj_create(scr);
    lv_obj_remove_style_all(s_field);
    lv_obj_set_size(s_field, TW * CELL, TH * CELL);
    lv_obj_set_pos(s_field, ORG_X, ORG_Y);
    lv_obj_clear_flag(s_field, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s_field, tet_draw, LV_EVENT_DRAW_MAIN, NULL);

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
    s_tm = lv_timer_create(tet_cb, PERIOD, NULL);
}

static void app_tetris_exit(void)
{
    if (s_tm != NULL) { lv_timer_del(s_tm); s_tm = NULL; }
}

const app_t app_tetris = { "俄罗斯方块", app_tetris_enter, app_tetris_exit };
