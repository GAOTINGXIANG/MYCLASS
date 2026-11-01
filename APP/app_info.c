/**
 * @file app_info.c
 * @brief 系统信息 app：显示运行时长和 LVGL 内存占用。
 *        结构是标准 app 模板：enter 建界面+定时器，exit 删定时器，
 *        屏幕对象由启动器统一清理。按 KEY4 退回菜单。
 */
#include "app_info.h"
#include "lvgl.h"

static lv_timer_t *s_tm;
static lv_obj_t   *s_lbl_up;
static lv_obj_t   *s_lbl_mem;

static void info_timer_cb(lv_timer_t *t)
{
    char buf[24];
    lv_mem_monitor_t mon;
    (void)t;

    lv_snprintf(buf, sizeof(buf), "运行 %lu 秒",
                (unsigned long)(lv_tick_get() / 1000U));
    lv_label_set_text(s_lbl_up, buf);

    lv_mem_monitor(&mon);
    lv_snprintf(buf, sizeof(buf), "内存 %u%%", (unsigned int)mon.used_pct);
    lv_label_set_text(s_lbl_mem, buf);
}

static void app_info_enter(void)
{
    lv_obj_t *scr = lv_scr_act();

    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "SYS INFO");
    lv_obj_set_style_text_color(title, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -30);

    s_lbl_up = lv_label_create(scr);
    lv_obj_set_style_text_color(s_lbl_up, lv_color_white(), 0);
    lv_obj_align(s_lbl_up, LV_ALIGN_CENTER, 0, 0);

    s_lbl_mem = lv_label_create(scr);
    lv_obj_set_style_text_color(s_lbl_mem, lv_color_white(), 0);
    lv_obj_align(s_lbl_mem, LV_ALIGN_CENTER, 0, 24);

    info_timer_cb(NULL);
    s_tm = lv_timer_create(info_timer_cb, 500, NULL);
}

static void app_info_exit(void)
{
    if (s_tm != NULL) {
        lv_timer_del(s_tm);
        s_tm = NULL;
    }
}

const app_t app_info = { "SYS INFO", app_info_enter, app_info_exit };
