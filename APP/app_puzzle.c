/**
 * @file app_puzzle.c
 * @brief 数字华容道：4x4 棋盘，1~15 打乱，滑动拼回顺序。
 *   摇杆 / 编码器 = 沿该方向滑动（离空格最近的块被推入空格）
 *   ENC/JS 键     = 胜利后重开
 *   KEY1          = 返回主菜单
 */
#include "app_puzzle.h"
#include "lvgl.h"
#include "encoder.h"
#include "keys.h"
#include "joystick.h"

#define N          4
#define ORG        35
#define CELL       40
#define TILE        (CELL - 3)
#define PERIOD      30
#define ENC_DETENT  4
#define SHUFFLE_N   300

static lv_timer_t *s_tm;
static lv_obj_t   *s_lbl[N * N];
static lv_obj_t   *s_msg;
static uint8_t     s_b[N * N];          /* 0 = 空格，1~15 = 数字块 */
static uint8_t     s_blank;
static uint32_t    s_rng = 0x87654321UL;
static uint8_t     s_win;
static int32_t     s_last_cnt;
static joy_mask_t  s_prev_mask = JOY_MASK_NONE;

static uint32_t rnd(void)
{
    s_rng = s_rng * 1103515245UL + 12345UL;
    return (s_rng >> 16) & 0x7FFFUL;
}

static void render(void)
{
    for (uint8_t i = 0; i < N * N; i++) {
        uint8_t v = s_b[i];
        lv_obj_set_style_bg_color(s_lbl[i],
            (v != 0U) ? lv_palette_main(LV_PALETTE_BLUE) : lv_color_black(), 0);
        lv_obj_set_style_bg_opa(s_lbl[i], (v != 0U) ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
        if (v != 0U) lv_label_set_text_fmt(s_lbl[i], "%u", v);
        else         lv_label_set_text(s_lbl[i], "");
    }
}

static uint8_t solved(void)
{
    for (uint8_t i = 0; i < N * N - 1U; i++) {
        if (s_b[i] != (uint8_t)(i + 1U)) return 0U;
    }
    return (s_b[N * N - 1U] == 0U) ? 1U : 0U;
}

/* 交换空格与 idx 位置的块 */
static void swap_blank(uint8_t idx)
{
    s_b[s_blank] = s_b[idx];
    s_b[idx] = 0U;
    s_blank = idx;
}

/* 把空格移到指定位置（dir 为"块移动方向"） */
static uint8_t move_dir(uint8_t up, uint8_t down, uint8_t left, uint8_t right)
{
    uint8_t r = (uint8_t)(s_blank / N);
    uint8_t c = (uint8_t)(s_blank % N);

    if (up != 0U && r < N - 1U)    { swap_blank((uint8_t)((r + 1U) * N + c)); return 1U; }
    if (down != 0U && r > 0U)      { swap_blank((uint8_t)((r - 1U) * N + c)); return 1U; }
    if (left != 0U && c < N - 1U)  { swap_blank((uint8_t)(r * N + c + 1U));   return 1U; }
    if (right != 0U && c > 0U)     { swap_blank((uint8_t)(r * N + c - 1U));   return 1U; }
    return 0U;
}

static void shuffle(void)
{
    do {
        for (uint8_t i = 0; i < N * N - 1U; i++) s_b[i] = (uint8_t)(i + 1U);
        s_b[N * N - 1U] = 0U;
        s_blank = N * N - 1U;

        for (uint16_t i = 0; i < SHUFFLE_N; i++) {
            uint8_t d = (uint8_t)(rnd() % 4U);
            (void)move_dir((d == 0U), (d == 1U), (d == 2U), (d == 3U));
        }
    } while (solved() != 0U);
}

static void new_game(void)
{
    s_win = 0;
    s_prev_mask = JOY_MASK_NONE;
    s_last_cnt = encoder_count();
    lv_obj_add_flag(s_msg, LV_OBJ_FLAG_HIDDEN);
    shuffle();
    render();
}

static void pz_cb(lv_timer_t *t)
{
    uint8_t moved = 0;
    uint8_t edge;
    int32_t d;
    (void)t;

    edge = keys_scan();
    if (s_win != 0U) {
        if ((edge & (KEY_MASK(KEY_ID_JS) | KEY_MASK(KEY_ID_ENC))) != 0U) new_game();
        return;
    }

    d = encoder_count() - s_last_cnt;
    s_last_cnt += d;
    if (d >= ENC_DETENT)       moved = move_dir(0U, 0U, 0U, 1U);
    else if (d <= -ENC_DETENT) moved = move_dir(0U, 0U, 1U, 0U);

    {
        joy_mask_t m = joy_get_mask();
        if (m != s_prev_mask) {
            if ((m & JOY_MASK_UP) != 0U)         moved = move_dir(1U, 0U, 0U, 0U);
            else if ((m & JOY_MASK_DOWN) != 0U)  moved = move_dir(0U, 1U, 0U, 0U);
            else if ((m & JOY_MASK_LEFT) != 0U)  moved = move_dir(0U, 0U, 1U, 0U);
            else if ((m & JOY_MASK_RIGHT) != 0U) moved = move_dir(0U, 0U, 0U, 1U);
            s_prev_mask = m;
        }
    }

    if (moved != 0U) {
        render();
        if (solved() != 0U) {
            s_win = 1;
            lv_label_set_text(s_msg, "你赢了");
            lv_obj_clear_flag(s_msg, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static void app_puzzle_enter(void)
{
    lv_obj_t *scr = lv_scr_act();

    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

    for (uint8_t r = 0; r < N; r++) {
        for (uint8_t c = 0; c < N; c++) {
            uint8_t i = (uint8_t)(r * N + c);
            lv_obj_t *o = lv_label_create(scr);
            lv_obj_set_size(o, TILE, TILE);
            lv_obj_set_style_radius(o, 4, 0);
            lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
            lv_obj_set_style_text_align(o, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_set_style_text_color(o, lv_color_white(), 0);
            lv_obj_set_pos(o, ORG + c * CELL, ORG + r * CELL);
            s_lbl[i] = o;
        }
    }

    s_msg = lv_label_create(scr);
    lv_obj_set_style_text_color(s_msg, lv_color_white(), 0);
    lv_obj_set_style_bg_color(s_msg, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_msg, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(s_msg, 6, 0);
    lv_obj_align(s_msg, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(s_msg, LV_OBJ_FLAG_HIDDEN);

    new_game();
    s_tm = lv_timer_create(pz_cb, PERIOD, NULL);
}

static void app_puzzle_exit(void)
{
    if (s_tm != NULL) { lv_timer_del(s_tm); s_tm = NULL; }
}

const app_t app_puzzle = { "数字华容道", app_puzzle_enter, app_puzzle_exit };
