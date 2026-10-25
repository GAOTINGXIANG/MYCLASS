/**
 * @file led.c
 * @brief 6 个用户 LED 的统一控制实现
 */

#include "led.h"
#include "main.h"

/* 引脚映射表，顺序与 led_id_t 一致 */
typedef struct {
    GPIO_TypeDef *port;
    uint16_t      pin;
} led_pin_t;

static const led_pin_t led_table[LED_COUNT] = {
    {LED1_GPIO_Port, LED1_Pin},
    {LED2_GPIO_Port, LED2_Pin},
    {LED3_GPIO_Port, LED3_Pin},
    {LED4_GPIO_Port, LED4_Pin},
    {LED5_GPIO_Port, LED5_Pin},
    {LED6_GPIO_Port, LED6_Pin},
};

/* 按“点亮/熄灭”换算实际电平并输出 */
static void led_set(led_id_t id, uint8_t on)
{
    GPIO_PinState state;

    if ((uint32_t)id >= (uint32_t)LED_COUNT) {
        return;
    }

    if (on) {
        state = (LED_ACTIVE_LEVEL != 0) ? GPIO_PIN_SET : GPIO_PIN_RESET;
    } else {
        state = (LED_ACTIVE_LEVEL != 0) ? GPIO_PIN_RESET : GPIO_PIN_SET;
    }

    HAL_GPIO_WritePin(led_table[id].port, led_table[id].pin, state);
}

void led_init(void)
{
    led_all(0U);
}

void led_on(led_id_t id)
{
    led_set(id, 1U);
}

void led_off(led_id_t id)
{
    led_set(id, 0U);
}

void led_toggle(led_id_t id)
{
    if ((uint32_t)id >= (uint32_t)LED_COUNT) {
        return;
    }

    HAL_GPIO_TogglePin(led_table[id].port, led_table[id].pin);
}

void led_write(led_id_t id, uint8_t on)
{
    led_set(id, on);
}

void led_all(uint8_t on)
{
    uint32_t i;

    for (i = 0U; i < (uint32_t)LED_COUNT; i++) {
        led_set((led_id_t)i, on);
    }
}
