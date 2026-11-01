/**
 * @file app_clock.c
 * @brief 模拟表盘 + 数字时间，可调表。
 *   编码器旋转        = 调整当前字段
 *   ENCODER_KEY/JS_KEY= 切换字段：时→分→秒→走时（按下蜂鸣提示）
 *   KEY1              = 返回主菜单
 */
#include "app_clock.h"
#include "lvgl.h"
#include "encoder.h"
#include "keys.h"
#include "sys_notify.h"

#define CLK_LEN_H     46
#define CLK_LEN_M     74
#define CLK_LEN_S     88
#define CLK_TICK_R0   92
#define CLK_TICK_R1   104
#define CLK_INPUT_MS  50
#define ENC_DETENT    4

enum { F_HOUR = 0, F_MIN, F_SEC, F_RUN };

static lv_timer_t *s_tm_in, *s_tm_tick;
static lv_obj_t   *s_hand_h, *s_hand_m, *s_hand_s, *s_lbl;
static lv_point_t  s_pts_h[2], s_pts_m[2], s_pts_s[2];
static lv_point_t  s_tick_pts[12][2];
static uint8_t  s_h = 12, s_m, s_s;
static uint8_t  s_field = F_RUN;
static int32_t  s_last_cnt;
static int16_t  s_acc;

static void hand_set(lv_obj_t *hand, lv_point_t *pts, int32_t deg10, lv_coord_t len)
{
    int16_t deg = (int16_t)(deg10 / 10);
    pts[0].x = 120;
    pts[0].y = 120;
    pts[1].x = 120 + (len * lv_trigo_sin(deg)) / LV_TRIGO_SIN_MAX;
    pts[1].y = 120 - (len * lv_trigo_cos(deg)) / LV_TRIGO_SIN_MAX;
    lv_line_set_points(hand, pts, 2);
}

static void clock_refresh(void)
{
    lv_color_t white = lv_color_white();
    lv_color_t yel   = lv_palette_main(LV_PALETTE_YELLOW);

    hand_set(s_hand_h, s_pts_h, (int32_t)(s_h % 12) * 300 + (int32_t)s_m * 5, CLK_LEN_H);
    hand_set(s_hand_m, s_pts_m, (int32_t)s_m * 60, CLK_LEN_M);
    hand_set(s_hand_s, s_pts_s, (int32_t)s_s * 60, CLK_LEN_S);

    lv_obj_set_style_line_color(s_hand_h, s_field == F_HOUR ? yel : white, 0);
    lv_obj_set_style_line_color(s_hand_m, s_field == F_MIN  ? yel : white, 0);
    lv_obj_set_style_line_color(s_hand_s,
        s_field == F_SEC ? yel : lv_palette_main(LV_PALETTE_RED), 0);

    lv_label_set_text_fmt(s_lbl, "%02u:%02u:%02u", s_h, s_m, s_s);
    lv_obj_set_style_text_color(s_lbl, s_field == F_RUN ? white : yel, 0);
}

static void tick_cb(lv_timer_t *t)
{
    (void)t;
    if (s_field != F_RUN) return;
    if (++s_s >= 60U) {
        s_s = 0;
        if (++s_m >= 60U) {
            s_m = 0;
            s_h = (uint8_t)((s_h + 1U) % 24U);
        }
    }
    clock_refresh();
}

static void input_cb(lv_timer_t *t)
{
    int32_t c, steps = 0;
    uint8_t edge;
    (void)t;

    c = encoder_count();
    s_acc += (int16_t)(c - s_last_cnt);
    s_last_cnt = c;
    while (s_acc >= ENC_DETENT)  { s_acc -= ENC_DETENT; steps++; }
    while (s_acc <= -ENC_DETENT) { s_acc += ENC_DETENT; steps--; }

    if (steps != 0 && s_field != F_RUN) {
        int32_t v;
        if (s_field == F_HOUR) {
            v = ((int32_t)s_h + steps) % 24;
            if (v < 0) v += 24;
            s_h = (uint8_t)v;
        } else if (s_field == F_MIN) {
            v = ((int32_t)s_m + steps) % 60;
            if (v < 0) v += 60;
            s_m = (uint8_t)v;
        } else {
            v = ((int32_t)s_s + steps) % 60;
            if (v < 0) v += 60;
            s_s = (uint8_t)v;
        }
        clock_refresh();
    }

    edge = keys_scan();
    if ((edge & (KEY_MASK(KEY_ID_JS) | KEY_MASK(KEY_ID_ENC))) != 0U) {
        s_field = (uint8_t)((s_field + 1U) % 4U);
        notify_send(NOTIFY_BEEP_SHORT);          /* 编码器/摇杆按键提示音 */
        clock_refresh();
    }
}

static lv_obj_t *make_hand(lv_coord_t w, lv_point_t *pts)
{
    lv_obj_t *ln = lv_line_create(lv_scr_act());
    lv_obj_set_style_line_width(ln, w, 0);
    lv_obj_set_style_line_rounded(ln, true, 0);
    lv_line_set_points(ln, pts, 2);
    return ln;
}

static void app_clock_enter(void)
{
    lv_obj_t *scr = lv_scr_act();

    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

    /* 12 刻度 */
    for (uint8_t i = 0; i < 12U; i++) {
        int16_t deg = (int16_t)(i * 30);
        lv_obj_t *ln = lv_line_create(scr);
        s_tick_pts[i][0].x = 120 + (CLK_TICK_R0 * lv_trigo_sin(deg)) / LV_TRIGO_SIN_MAX;
        s_tick_pts[i][0].y = 120 - (CLK_TICK_R0 * lv_trigo_cos(deg)) / LV_TRIGO_SIN_MAX;
        s_tick_pts[i][1].x = 120 + (CLK_TICK_R1 * lv_trigo_sin(deg)) / LV_TRIGO_SIN_MAX;
        s_tick_pts[i][1].y = 120 - (CLK_TICK_R1 * lv_trigo_cos(deg)) / LV_TRIGO_SIN_MAX;
        lv_obj_set_style_line_width(ln, (i % 3U == 0U) ? 3 : 1, 0);
        lv_obj_set_style_line_color(ln, lv_palette_main(LV_PALETTE_GREY), 0);
        lv_line_set_points(ln, s_tick_pts[i], 2);
    }

    /* 指针 */
    s_hand_h = make_hand(6, s_pts_h);
    s_hand_m = make_hand(3, s_pts_m);
    s_hand_s = make_hand(2, s_pts_s);

    /* 中心圆点 */
    lv_obj_t *dot = lv_obj_create(scr);
    lv_obj_remove_style_all(dot);
    lv_obj_set_size(dot, 10, 10);
    lv_obj_center(dot);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(dot, lv_color_white(), 0);

    /* 数字时间 */
    s_lbl = lv_label_create(scr);
    lv_obj_set_style_text_color(s_lbl, lv_color_white(), 0);
    lv_obj_align(s_lbl, LV_ALIGN_CENTER, 0, 58);

    s_field = F_RUN;
    s_last_cnt = encoder_count();
    s_acc = 0;
    clock_refresh();
    s_tm_in   = lv_timer_create(input_cb, CLK_INPUT_MS, NULL);
    s_tm_tick = lv_timer_create(tick_cb, 1000, NULL);
}

static void app_clock_exit(void)
{
    if (s_tm_in   != NULL) { lv_timer_del(s_tm_in);   s_tm_in   = NULL; }
    if (s_tm_tick != NULL) { lv_timer_del(s_tm_tick); s_tm_tick = NULL; }
}

const app_t app_clock = { "CLOCK", app_clock_enter, app_clock_exit };
