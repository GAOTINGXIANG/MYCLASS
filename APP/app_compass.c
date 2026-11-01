/**
 * @file app_compass.c
 * @brief 指南针：指针固定朝上，刻度盘随方位旋转。
 *   正指北方时蜂鸣提醒一声（离开 10° 后重新允许提醒）
 *   KEY2 = 开始/结束校准（校准时全方位旋转模块，再按一次结束）
 *   KEY1 = 返回主菜单
 */
#include "app_compass.h"
#include "lvgl.h"
#include "hmc5883l.h"
#include "keys.h"
#include "sys_notify.h"

#define COMPASS_PERIOD    100
#define DIAL_R_TICK0      88
#define DIAL_R_TICK1      100
#define DIAL_R_DIR        62
#define NORTH_BEEP_D10    30      /* ±3° 视为正北 */
#define NORTH_REARM_D10   100     /* 离开 10° 后重新允许 */

static lv_timer_t *s_tm;
static lv_obj_t   *s_ticks[12];
static lv_point_t  s_tick_pts[12][2];
static lv_obj_t   *s_lbl_dir[4];
static lv_obj_t   *s_lbl_txt;
static uint8_t  s_cal, s_north_hit;
static uint16_t s_cal_cnt;

static void dial_update(int16_t heading_d10)
{
    static const char *const names[4] = { "N", "E", "S", "W" };

    for (uint8_t i = 0; i < 12U; i++) {
        int16_t deg = (int16_t)(((int32_t)i * 300 - heading_d10) / 10);
        s_tick_pts[i][0].x = 120 + (DIAL_R_TICK0 * lv_trigo_sin(deg)) / LV_TRIGO_SIN_MAX;
        s_tick_pts[i][0].y = 120 - (DIAL_R_TICK0 * lv_trigo_cos(deg)) / LV_TRIGO_SIN_MAX;
        s_tick_pts[i][1].x = 120 + (DIAL_R_TICK1 * lv_trigo_sin(deg)) / LV_TRIGO_SIN_MAX;
        s_tick_pts[i][1].y = 120 - (DIAL_R_TICK1 * lv_trigo_cos(deg)) / LV_TRIGO_SIN_MAX;
        lv_line_set_points(s_ticks[i], s_tick_pts[i], 2);
    }
    for (uint8_t i = 0; i < 4U; i++) {
        int16_t deg = (int16_t)(((int32_t)i * 900 - heading_d10) / 10);
        lv_coord_t x = 120 + (DIAL_R_DIR * lv_trigo_sin(deg)) / LV_TRIGO_SIN_MAX;
        lv_coord_t y = 120 - (DIAL_R_DIR * lv_trigo_cos(deg)) / LV_TRIGO_SIN_MAX;
        lv_obj_set_pos(s_lbl_dir[i], x - 8, y - 9);
        (void)names;
    }
}

static void compass_cb(lv_timer_t *t)
{
    hmc5883l_data_t d;
    int16_t h;
    (void)t;

    /* KEY2：校准开始/结束 */
    if ((keys_scan() & KEY_MASK(KEY_ID_2)) != 0U) {
        if (s_cal == 0U) {
            s_cal = 1;
            s_cal_cnt = 0;
            hmc5883l_calib_reset();
        } else {
            s_cal = 0;
            notify_send(hmc5883l_calib_finish() == HMC5883L_OK
                        ? NOTIFY_BEEP_SHORT : NOTIFY_ERROR);
        }
    }

    if (hmc5883l_read(&d) != HMC5883L_OK) {
        lv_label_set_text(s_lbl_txt, "ERR");
        return;
    }

    if (s_cal != 0U) {
        hmc5883l_calib_feed(&d);
        lv_label_set_text_fmt(s_lbl_txt, "CAL %u", (unsigned)++s_cal_cnt);
        return;
    }

    h = hmc5883l_heading_deg10(&d);
    dial_update(h);
    lv_label_set_text_fmt(s_lbl_txt, "%d", (int)((h + 5) / 10));

    /* 正北提醒（边沿触发，离开 rearm 区后复位） */
    if (h <= NORTH_BEEP_D10 || h >= 3600 - NORTH_BEEP_D10) {
        if (s_north_hit == 0U) {
            s_north_hit = 1;
            notify_send(NOTIFY_BEEP_SHORT);
        }
    } else if (h > NORTH_REARM_D10 && h < 3600 - NORTH_REARM_D10) {
        s_north_hit = 0;
    }
}

static void app_compass_enter(void)
{
    lv_obj_t *scr = lv_scr_act();
    static lv_point_t ndl[2]  = { {120, 42}, {120, 120} };
    static lv_point_t tail[2] = { {120, 120}, {120, 152} };

    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

    /* 外圈 */
    lv_obj_t *ring = lv_obj_create(scr);
    lv_obj_remove_style_all(ring);
    lv_obj_set_size(ring, 222, 222);
    lv_obj_center(ring);
    lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(ring, 2, 0);
    lv_obj_set_style_border_color(ring, lv_palette_main(LV_PALETTE_BLUE_GREY), 0);
    lv_obj_clear_flag(ring, LV_OBJ_FLAG_SCROLLABLE);

    /* 刻度 + 方位字母 */
    for (uint8_t i = 0; i < 12U; i++) {
        s_ticks[i] = lv_line_create(scr);
        lv_obj_set_style_line_width(s_ticks[i], (i % 3U == 0U) ? 3 : 1, 0);
        lv_obj_set_style_line_color(s_ticks[i], lv_palette_main(LV_PALETTE_GREY), 0);
        lv_line_set_points(s_ticks[i], s_tick_pts[i], 2);
    }
    {
        static const char *const names[4] = { "N", "E", "S", "W" };
        for (uint8_t i = 0; i < 4U; i++) {
            s_lbl_dir[i] = lv_label_create(scr);
            lv_label_set_text(s_lbl_dir[i], names[i]);
            lv_obj_set_style_text_color(s_lbl_dir[i],
                i == 0U ? lv_palette_main(LV_PALETTE_RED) : lv_color_white(), 0);
        }
    }

    /* 固定指针（上红下白） + 中心点 */
    lv_obj_t *ln = lv_line_create(scr);
    lv_obj_set_style_line_width(ln, 4, 0);
    lv_obj_set_style_line_color(ln, lv_palette_main(LV_PALETTE_RED), 0);
    lv_obj_set_style_line_rounded(ln, true, 0);
    lv_line_set_points(ln, ndl, 2);

    ln = lv_line_create(scr);
    lv_obj_set_style_line_width(ln, 2, 0);
    lv_obj_set_style_line_color(ln, lv_color_white(), 0);
    lv_line_set_points(ln, tail, 2);

    lv_obj_t *dot = lv_obj_create(scr);
    lv_obj_remove_style_all(dot);
    lv_obj_set_size(dot, 8, 8);
    lv_obj_center(dot);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(dot, lv_color_white(), 0);

    /* 方位角文本 */
    s_lbl_txt = lv_label_create(scr);
    lv_obj_set_style_text_color(s_lbl_txt, lv_color_white(), 0);
    lv_obj_align(s_lbl_txt, LV_ALIGN_CENTER, 0, 60);
    lv_label_set_text(s_lbl_txt, "--");

    s_cal = 0;
    s_north_hit = 0;
    dial_update(0);
    s_tm = lv_timer_create(compass_cb, COMPASS_PERIOD, NULL);
}

static void app_compass_exit(void)
{
    if (s_tm != NULL) { lv_timer_del(s_tm); s_tm = NULL; }
}

const app_t app_compass = { "COMPASS", app_compass_enter, app_compass_exit };
