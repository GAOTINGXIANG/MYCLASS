/**
 * @file demo_keys.c
 * @brief 按键验证 demo
 *
 * 屏幕中央 2x3 排布 6 个指示灯：KEY1~KEY4、JS_KEY、ENCODER_KEY，
 * 按键按下时对应指示灯点亮（绿色），松开熄灭。
 * 布局约束在圆屏内接安全区（约 170x170 居中）。
 */

#include "demo_keys.h"
#include "lvgl.h"
#include "keys.h"

/*---------------- 可调参数（修改后重新编译生效） ----------------*/

#define DEMO_KEYS_PERIOD   30      /* 刷新周期（ms） */
#define DEMO_KEYS_LED_SZ   22      /* 指示灯直径 */

/*---------------------------------------------------------------*/

/* 单元格中心坐标（圆屏安全区内 3 列 2 行） */
static const lv_coord_t cell_x[3] = { 60, 120, 180 };
static const lv_coord_t cell_y[2] = { 75, 165 };

/* 显示名与按键编号的对应表（显示顺序 = 位序号顺序） */
static const char *const key_name[KEY_ID_MAX] = {
    "KEY1", "KEY2", "KEY3", "KEY4", "JS", "ENC"
};

static lv_obj_t *leds[KEY_ID_MAX];

static void keys_refresh_cb(lv_timer_t *timer)
{
    (void)timer;

    uint8_t st = keys_state();
    for (uint8_t i = 0; i < (uint8_t)KEY_ID_MAX; i++) {
        if (st & KEY_MASK(i)) {
            lv_led_on(leds[i]);
        } else {
            lv_led_off(leds[i]);
        }
    }
}

void demo_keys_init(void)
{
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_black(), 0);

    for (uint8_t i = 0; i < (uint8_t)KEY_ID_MAX; i++) {
        lv_coord_t cx = cell_x[i % 3];
        lv_coord_t cy = cell_y[i / 3];

        /* 指示灯 */
        lv_obj_t *led = lv_led_create(lv_scr_act());
        lv_obj_set_size(led, DEMO_KEYS_LED_SZ, DEMO_KEYS_LED_SZ);
        lv_obj_set_pos(led, cx - DEMO_KEYS_LED_SZ / 2, cy - DEMO_KEYS_LED_SZ / 2 - 10);
        lv_led_set_color(led, lv_palette_main(LV_PALETTE_GREEN));
        lv_led_off(led);
        leds[i] = led;

        /* 名称标签（指示灯下方） */
        lv_obj_t *label = lv_label_create(lv_scr_act());
        lv_label_set_text(label, key_name[i]);
        lv_obj_set_style_text_color(label, lv_color_white(), 0);
        lv_obj_set_pos(label, cx - 14, cy + 12);
    }

    lv_timer_create(keys_refresh_cb, DEMO_KEYS_PERIOD, NULL);
}
