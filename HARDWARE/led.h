/**
 * @file led.h
 * @brief 6 个用户 LED 的统一控制
 *
 * 引脚（见 main.h）：
 *   LED1 = PC9    LED2 = PC12   LED3 = PC11
 *   LED4 = PD0    LED5 = PA8    LED6 = PC7
 */

#ifndef __LED_H__
#define __LED_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 点亮电平：1 = 高电平点亮（灌电流接法为 0，即低电平点亮） */
#define LED_ACTIVE_LEVEL   1

/* LED 编号 */
typedef enum {
    LED_1 = 0,
    LED_2,
    LED_3,
    LED_4,
    LED_5,
    LED_6,
    LED_COUNT
} led_id_t;

/** @brief 初始化：全部熄灭（需在 MX_GPIO_Init() 之后调用） */
void led_init(void);

/** @brief 点亮指定 LED */
void led_on(led_id_t id);

/** @brief 熄灭指定 LED */
void led_off(led_id_t id);

/** @brief 翻转指定 LED */
void led_toggle(led_id_t id);

/**
 * @brief 按状态写指定 LED
 * @param on 非 0 点亮，0 熄灭
 */
void led_write(led_id_t id, uint8_t on);

/** @brief 全部点亮 / 全部熄灭：on 非 0 点亮 */
void led_all(uint8_t on);

#ifdef __cplusplus
}
#endif

#endif /* __LED_H__ */
