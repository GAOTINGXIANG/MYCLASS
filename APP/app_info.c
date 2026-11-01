/**
 * @file app_info.c
 * @brief 系统信息 app：显示运行时长、LVGL 内存占用，并提供音效开关。
 *        KEY2 切换蜂鸣器音效（默认开，不掉电保存），KEY1 返回上一级菜单。
 */
#include "app_info.h"
#include "lvgl.h"
#include "keys.h"
#include "sys_notify.h"

static lv_timer_t *s_tm;
static lv_timer_t *s_tm_key;
static lv_obj_t   *s_lbl_up;
static lv_obj_t   *s_lbl_mem;
static lv_obj_t   *s_lbl_snd;

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

static void info_sound_show(void)
{
    lv_label_set_text(s_lbl_snd,
        (sys_notify_sound_get() != 0U) ? "音效 开" : "音效 关");
}

static void info_key_cb(lv_timer_t *t)
{
    (void)t;

    if ((keys_scan() & KEY_MASK(KEY_ID_2)) != 0U) {
        uint8_t on = (sys_notify_sound_get() != 0U) ? 0U : 1U;
        sys_notify_sound_set(on);
        info_sound_show();
        if (on != 0U) notify_send(NOTIFY_BEEP_SHORT);   /* 开启时试听一声 */
    }
}

static void app_info_enter(void)
{
    lv_obj_t *scr = lv_scr_act();

    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "系统信息");
    lv_obj_set_style_text_color(title, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -58);

    s_lbl_up = lv_label_create(scr);
    lv_obj_set_style_text_color(s_lbl_up, lv_color_white(), 0);
    lv_obj_align(s_lbl_up, LV_ALIGN_CENTER, 0, -25);

    s_lbl_mem = lv_label_create(scr);
    lv_obj_set_style_text_color(s_lbl_mem, lv_color_white(), 0);
    lv_obj_align(s_lbl_mem, LV_ALIGN_CENTER, 0, 3);

    s_lbl_snd = lv_label_create(scr);
    lv_obj_set_style_text_color(s_lbl_snd, lv_color_white(), 0);
    lv_obj_align(s_lbl_snd, LV_ALIGN_CENTER, 0, 38);

    lv_obj_t *hint = lv_label_create(scr);
    lv_label_set_text(hint, "KEY2 切换音效");
    lv_obj_set_style_text_color(hint, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_align(hint, LV_ALIGN_CENTER, 0, 66);

    info_timer_cb(NULL);
    info_sound_show();
    s_tm = lv_timer_create(info_timer_cb, 500, NULL);
    s_tm_key = lv_timer_create(info_key_cb, 30, NULL);
}

static void app_info_exit(void)
{
    if (s_tm != NULL) {
        lv_timer_del(s_tm);
        s_tm = NULL;
    }
    if (s_tm_key != NULL) {
        lv_timer_del(s_tm_key);
        s_tm_key = NULL;
    }
}

const app_t app_info = { "系统信息", app_info_enter, app_info_exit };
