/**
 * @file app_temp.c
 * @brief 温湿度显示：读取 sys_sensor 共享数据（g_sensor），不直接碰硬件。
 * KEY1 返回主菜单（启动器全局处理）。
 */
#include "app_temp.h"
#include "lvgl.h"
#include "sys_sensor.h"

#define APP_TEMP_PERIOD   500

static lv_timer_t *s_tm;
static lv_obj_t   *s_val_t, *s_val_h;

static void temp_cb(lv_timer_t *t)
{
    (void)t;
    if (g_sensor.dht_ok) {
        lv_label_set_text_fmt(s_val_t, "%u ℃", (unsigned)g_sensor.temp);
        lv_label_set_text_fmt(s_val_h, "%u %%", (unsigned)g_sensor.humi);
    } else {
        lv_label_set_text(s_val_t, "--");
        lv_label_set_text(s_val_h, "--");
    }
}

static lv_obj_t *make_row(const char *name, lv_coord_t dy, lv_obj_t **val)
{
    lv_obj_t *lb = lv_label_create(lv_scr_act());
    lv_label_set_text(lb, name);
    lv_obj_set_style_text_color(lb, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_align(lb, LV_ALIGN_CENTER, 0, dy);

    *val = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(*val, &lv_font_cn_14, 0);
    lv_obj_set_style_text_color(*val, lv_color_white(), 0);
    lv_obj_align(*val, LV_ALIGN_CENTER, 0, dy + 22);
    lv_label_set_text(*val, "--");
    return lb;
}

static void app_temp_enter(void)
{
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_black(), 0);
    make_row("温度", -52, &s_val_t);
    make_row("湿度",  20, &s_val_h);
    temp_cb(NULL);
    s_tm = lv_timer_create(temp_cb, APP_TEMP_PERIOD, NULL);
}

static void app_temp_exit(void)
{
    if (s_tm != NULL) { lv_timer_del(s_tm); s_tm = NULL; }
}

const app_t app_temp = { "温湿度", app_temp_enter, app_temp_exit };
