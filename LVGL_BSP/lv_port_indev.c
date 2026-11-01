/**
 * @file lv_port_indev.c
 * @brief LVGL 输入设备移植层实现
 *
 * 编码器：LV_INDEV_TYPE_ENCODER，转动移动焦点，按下确认（默认无按键）
 * 摇杆：LV_INDEV_TYPE_KEYPAD，方向映射为上下左右
 */

#include "lv_port_indev.h"
#include "lvgl.h"
#include "encoder.h"
#include "joystick.h"
#include "keys.h"

/* 默认无按键：用户可在应用层重新定义此函数读取自己的按键 GPIO */
__attribute__((weak)) uint8_t lv_port_encoder_key(void)
{
    return 0;
}

/* ---------------- 编码器 ---------------- */
static void enc_read_cb(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    (void)drv;

    data->enc_diff = (int16_t)encoder_delta();

    if (lv_port_encoder_key()) {
        data->state = LV_INDEV_STATE_PRESSED;   /* 按下 -> LV_KEY_ENTER */
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

/* ---------------- 摇杆（方向键） ---------------- */
static uint32_t joy_to_key(joy_dir_t dir)
{
    switch (dir) {
    case JOY_DIR_UP:        return LV_KEY_UP;
    case JOY_DIR_DOWN:      return LV_KEY_DOWN;
    case JOY_DIR_LEFT:      return LV_KEY_LEFT;
    case JOY_DIR_RIGHT:     return LV_KEY_RIGHT;
    case JOY_DIR_UP_LEFT:   return LV_KEY_LEFT;     /* 斜向优先水平 */
    case JOY_DIR_UP_RIGHT:  return LV_KEY_RIGHT;
    case JOY_DIR_DOWN_LEFT: return LV_KEY_LEFT;
    case JOY_DIR_DOWN_RIGHT:return LV_KEY_RIGHT;
    default:                return 0;
    }
}

static void joy_read_cb(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    (void)drv;

    joy_dir_t dir = joy_get_dir();
    uint32_t key = joy_to_key(dir);

    if (key != 0U) {
        data->key   = key;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->key   = 0;
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

/* ---------------- 实体按键（KEY1~4 / JS_KEY） ----------------
 * 映射：KEY1=上一个焦点  KEY2=下一个焦点  KEY3=确认  KEY4=返回  JS_KEY=确认 */
static void keys_read_cb(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    (void)drv;

    uint8_t st = keys_state();
    uint32_t key = 0;

    if      (st & KEY_MASK(KEY_ID_1))  key = LV_KEY_PREV;
    else if (st & KEY_MASK(KEY_ID_2))  key = LV_KEY_NEXT;
    else if (st & KEY_MASK(KEY_ID_3))  key = LV_KEY_ENTER;
    else if (st & KEY_MASK(KEY_ID_4))  key = LV_KEY_ESC;
    else if (st & KEY_MASK(KEY_ID_JS)) key = LV_KEY_ENTER;

    if (key != 0U) {
        data->key   = key;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->key   = 0;
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

void lv_port_indev_init(void)
{
    static lv_indev_drv_t enc_drv;
    static lv_indev_drv_t joy_drv;
    static lv_indev_drv_t key_drv;
    lv_indev_t *enc_indev;
    lv_group_t *g;

    /* 创建默认 group，后续控件自动入组；编码器绑定该 group */
    g = lv_group_create();
    lv_group_set_default(g);

    /* 编码器 */
    lv_indev_drv_init(&enc_drv);
    enc_drv.type    = LV_INDEV_TYPE_ENCODER;
    enc_drv.read_cb = enc_read_cb;
    enc_indev = lv_indev_drv_register(&enc_drv);
    lv_indev_set_group(enc_indev, g);

    /* 摇杆方向键（绑定同一 group） */
    lv_indev_drv_init(&joy_drv);
    joy_drv.type    = LV_INDEV_TYPE_KEYPAD;
    joy_drv.read_cb = joy_read_cb;
    lv_indev_t *joy_indev = lv_indev_drv_register(&joy_drv);
    lv_indev_set_group(joy_indev, g);

    /* 实体按键（绑定同一 group） */
    lv_indev_drv_init(&key_drv);
    key_drv.type    = LV_INDEV_TYPE_KEYPAD;
    key_drv.read_cb = keys_read_cb;
    lv_indev_t *key_indev = lv_indev_drv_register(&key_drv);
    lv_indev_set_group(key_indev, g);
}
