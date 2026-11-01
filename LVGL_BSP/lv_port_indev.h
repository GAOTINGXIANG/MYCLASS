/**
 * @file lv_port_indev.h
 * @brief LVGL 输入设备移植层（编码器 + 摇杆）
 */

#ifndef __LV_PORT_INDEV_H__
#define __LV_PORT_INDEV_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 注册输入设备（编码器 + 摇杆）
 *  @note  需在 lv_init() 之后调用；编码器会自动绑定到默认 group */
void lv_port_indev_init(void);

/**
 * @brief 编码器按键状态钩子（__weak 定义）
 * @return 1 = 按下（产生 LV_KEY_ENTER），0 = 释放
 * @note   默认无按键，返回 0；如需确认键，在应用层重新定义此函数读取对应 GPIO
 */
uint8_t lv_port_encoder_key(void);

#ifdef __cplusplus
}
#endif

#endif /* __LV_PORT_INDEV_H__ */
