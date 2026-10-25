/**
 * @file beep.c
 * @brief 有源蜂鸣器控制实现
 */

#include "beep.h"
#include "main.h"

/* 当前逻辑状态：1 = 响，0 = 停 */
static uint8_t beep_state = 0U;

/* 逻辑状态换算为实际电平并输出 */
static void beep_set_pin(uint8_t on)
{
    GPIO_PinState state;

    beep_state = (on != 0U) ? 1U : 0U;

    if (beep_state != 0U) {
        state = (BEEP_ACTIVE_LEVEL != 0) ? GPIO_PIN_SET : GPIO_PIN_RESET;
    } else {
        state = (BEEP_ACTIVE_LEVEL != 0) ? GPIO_PIN_RESET : GPIO_PIN_SET;
    }

    HAL_GPIO_WritePin(BEEP_Y_GPIO_Port, BEEP_Y_Pin, state);
}

void beep_init(void)
{
    beep_set_pin(0U);
}

void beep_on(void)
{
    beep_set_pin(1U);
}

void beep_off(void)
{
    beep_set_pin(0U);
}

void beep_toggle(void)
{
    beep_set_pin((beep_state != 0U) ? 0U : 1U);
}

void beep_write(uint8_t on)
{
    beep_set_pin(on);
}

void beep(uint32_t ms)
{
    if (ms == 0U) {
        return;
    }

    beep_on();
    HAL_Delay(ms);
    beep_off();
}
