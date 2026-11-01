/**
 * @file keys.h
 * @brief 板载按键读取（KEY1~KEY4、JS_KEY、ENCODER_KEY）
 *
 * 硬件：6 个按键均为输入 + 上拉，按下为低电平。
 *       消抖基于 HAL_GetTick 时间戳，周期调用 keys_state()/keys_scan() 即可。
 */

#ifndef __KEYS_H__
#define __KEYS_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 按键编号（同时作为位掩码的位序号） */
typedef enum {
    KEY_ID_1 = 0,
    KEY_ID_2,
    KEY_ID_3,
    KEY_ID_4,
    KEY_ID_JS,      /* 摇杆按下 */
    KEY_ID_ENC,     /* 编码器按下 */
    KEY_ID_MAX
} key_id_t;

/* 按键位掩码：KEY_MASK(KEY_ID_1) 等 */
#define KEY_MASK(id)   ((uint8_t)(1U << (id)))

/** @brief 读取某按键当前是否按下（消抖后状态） */
uint8_t key_pressed(key_id_t id);

/** @brief 所有按键当前消抖后状态的位掩码（1 = 按下） */
uint8_t keys_state(void);

/** @brief 自上次调用以来“新按下”的按键位掩码（上升沿，读取后清零） */
uint8_t keys_scan(void);

/* LVGL 编码器确认键钩子：由本文件提供强定义（覆盖 lv_port_indev.c 的 weak 版本） */
uint8_t lv_port_encoder_key(void);

#ifdef __cplusplus
}
#endif

#endif /* __KEYS_H__ */
