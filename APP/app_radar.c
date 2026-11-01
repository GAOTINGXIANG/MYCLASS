/**
 * @file app_radar.c
 * @brief 超声波雷达：扫描线旋转，障碍物按"距离=半径"显示为亮点并渐隐。
 *        距离来自 sys_sensor 共享数据（g_sensor）。
 * KEY1 返回主菜单。
 */
#include "app_radar.h"
#include "lvgl.h"
#include "sys_sensor.h"

#define RADAR_R         108     /* 雷达外圈半径 */
#define RADAR_MAX_CM    200     /* 满半径对应距离 */
#define RADAR_PERIOD    40      /* 扫描周期 ms */
#define RADAR_STEP      3       /* 每拍转过角度 */
#define BLIP_NUM        16
#define BLIP_LIFE_MS    4000    /* 亮点存留时间 */

static lv_timer_t *s_tm;
static lv_obj_t   *s_sweep;
static lv_point_t  s_sweep_pts[2];
static lv_obj_t   *s_blips[BLIP_NUM];
static uint32_t    s_blip_at[BLIP_NUM];
static uint8_t     s_blip_head;
static int16_t     s_angle;

static void radar_cb(lv_timer_t *t)
{
    (void)t;

    /* 扫描线 */
    s_angle = (int16_t)((s_angle + RADAR_STEP) % 360);
    s_sweep_pts[0].x = 120;
    s_sweep_pts[0].y = 120;
    s_sweep_pts[1].x = 120 + (RADAR_R * lv_trigo_sin(s_angle)) / LV_TRIGO_SIN_MAX;
    s_sweep_pts[1].y = 120 - (RADAR_R * lv_trigo_cos(s_angle)) / LV_TRIGO_SIN_MAX;
    lv_line_set_points(s_sweep, s_sweep_pts, 2);

    /* 扫到障碍 → 在当前角度落一个亮点 */
    if (g_sensor.dist_ok && g_sensor.dist_cm >= 2U &&
        g_sensor.dist_cm <= RADAR_MAX_CM) {
        lv_coord_t r = (lv_coord_t)((uint32_t)g_sensor.dist_cm * RADAR_R / RADAR_MAX_CM);
        lv_obj_t *b = s_blips[s_blip_head];

        lv_obj_set_pos(b,
            120 + (r * lv_trigo_sin(s_angle)) / LV_TRIGO_SIN_MAX - 3,
            120 - (r * lv_trigo_cos(s_angle)) / LV_TRIGO_SIN_MAX - 3);
        lv_obj_clear_flag(b, LV_OBJ_FLAG_HIDDEN);
        s_blip_at[s_blip_head] = lv_tick_get();
        s_blip_head = (uint8_t)((s_blip_head + 1U) % BLIP_NUM);
    }

    /* 亮点寿命到期渐隐 */
    for (uint8_t i = 0; i < BLIP_NUM; i++) {
        if (s_blip_at[i] != 0U &&
            lv_tick_elaps(s_blip_at[i]) > BLIP_LIFE_MS) {
            lv_obj_add_flag(s_blips[i], LV_OBJ_FLAG_HIDDEN);
            s_blip_at[i] = 0U;
        }
    }
}

static void app_radar_enter(void)
{
    lv_obj_t *scr = lv_scr_act();

    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

    /* 三圈距离环 */
    for (uint8_t i = 1; i <= 3U; i++) {
        lv_obj_t *ring = lv_obj_create(scr);
        lv_coord_t r = (lv_coord_t)(RADAR_R * (int16_t)i / 3);
        lv_obj_remove_style_all(ring);
        lv_obj_set_size(ring, 2 * r, 2 * r);
        lv_obj_center(ring);
        lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(ring, 1, 0);
        lv_obj_set_style_border_color(ring, lv_palette_main(LV_PALETTE_GREY), 0);
        lv_obj_clear_flag(ring, LV_OBJ_FLAG_SCROLLABLE);
    }

    /* 亮点池 */
    for (uint8_t i = 0; i < BLIP_NUM; i++) {
        s_blips[i] = lv_obj_create(scr);
        lv_obj_remove_style_all(s_blips[i]);
        lv_obj_set_size(s_blips[i], 7, 7);
        lv_obj_set_style_radius(s_blips[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(s_blips[i], LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(s_blips[i], lv_palette_main(LV_PALETTE_GREEN), 0);
        lv_obj_add_flag(s_blips[i], LV_OBJ_FLAG_HIDDEN);
        s_blip_at[i] = 0U;
    }
    s_blip_head = 0U;

    /* 扫描线（最后创建，置于上层） */
    s_sweep = lv_line_create(scr);
    lv_obj_set_style_line_width(s_sweep, 2, 0);
    lv_obj_set_style_line_color(s_sweep, lv_palette_main(LV_PALETTE_CYAN), 0);

    s_angle = 0;
    s_tm = lv_timer_create(radar_cb, RADAR_PERIOD, NULL);
}

static void app_radar_exit(void)
{
    if (s_tm != NULL) { lv_timer_del(s_tm); s_tm = NULL; }
}

const app_t app_radar = { "雷达", app_radar_enter, app_radar_exit };
