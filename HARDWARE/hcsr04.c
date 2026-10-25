/**
 * @file hcsr04.c
 * @brief HC-SR04 超声波测距驱动实现（TIM4_CH2 输入捕获，1 us 分辨率）
 *
 * 流程：触发 12 us 高脉冲 → 捕获 Echo 上升沿 → 切下降沿捕获 → 两者差值即回波宽度。
 * 超时基于 DWT 周期计数器，不依赖 SysTick，可在任务中或较低优先级中断中调用。
 */

#include "hcsr04.h"
#include "main.h"
#include "tim.h"

/* 单次等待回波的超时（us）。HC-SR04 3 m 量程回波约 17.6 ms，40 ms 足够 */
#define HCSR04_TIMEOUT_US     40000U

/* 触发脉冲宽度（us），手册要求 >= 10 us */
#define HCSR04_TRIG_US        12U

/* 距离换算：cm = us / 58（声速 343 m/s） */
#define HCSR04_US_PER_CM      58U

/* DWT 每个 us 的计数个数 */
#define HCSR04_TICKS_PER_US   (SystemCoreClock / 1000000U)

static uint8_t hcsr04_inited = 0U;

/* 基于 DWT 的微秒级忙等延时（不依赖 SysTick / 调度器） */
static void hcsr04_delay_us(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = us * HCSR04_TICKS_PER_US;

    while ((DWT->CYCCNT - start) < ticks) {
    }
}

/* 等待 TIM4 CC2 捕获事件，超时返回 HCSR04_NO_ECHO */
static hcsr04_status_t hcsr04_wait_capture(uint32_t timeout_us, uint32_t *value)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t max_ticks = timeout_us * HCSR04_TICKS_PER_US;

    while (__HAL_TIM_GET_FLAG(&htim4, TIM_FLAG_CC2) == RESET) {
        if ((DWT->CYCCNT - start) > max_ticks) {
            return HCSR04_NO_ECHO;
        }
    }

    __HAL_TIM_CLEAR_FLAG(&htim4, TIM_FLAG_CC2);
    *value = HAL_TIM_ReadCapturedValue(&htim4, TIM_CHANNEL_2);

    return HCSR04_OK;
}

void hcsr04_init(void)
{
    /* 触发脚保持低电平 */
    HAL_GPIO_WritePin(US_Trig_GPIO_Port, US_Trig_Pin, GPIO_PIN_RESET);

    /* 使能 DWT 周期计数器（用于 us 级延时与超时） */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    hcsr04_inited = 1U;
}

hcsr04_status_t hcsr04_measure_us(uint32_t *pulse_us)
{
    uint32_t t_start = 0U;
    uint32_t t_end = 0U;
    hcsr04_status_t st;

    if (pulse_us == NULL) {
        return HCSR04_ERROR;
    }
    *pulse_us = 0U;

    if (hcsr04_inited == 0U) {
        return HCSR04_ERROR;
    }

    /* 先配上升沿捕获，清标志、清计数器后启动捕获 */
    __HAL_TIM_SET_CAPTUREPOLARITY(&htim4, TIM_CHANNEL_2, TIM_INPUTCHANNELPOLARITY_RISING);
    __HAL_TIM_CLEAR_FLAG(&htim4, TIM_FLAG_CC2);
    __HAL_TIM_SET_COUNTER(&htim4, 0U);

    if (HAL_TIM_IC_Start(&htim4, TIM_CHANNEL_2) != HAL_OK) {
        return HCSR04_ERROR;
    }

    /* 发出 12 us 触发脉冲 */
    HAL_GPIO_WritePin(US_Trig_GPIO_Port, US_Trig_Pin, GPIO_PIN_SET);
    hcsr04_delay_us(HCSR04_TRIG_US);
    HAL_GPIO_WritePin(US_Trig_GPIO_Port, US_Trig_Pin, GPIO_PIN_RESET);

    /* 等上升沿 */
    st = hcsr04_wait_capture(HCSR04_TIMEOUT_US, &t_start);

    if (st == HCSR04_OK) {
        /* 切成下降沿捕获，等回波结束 */
        __HAL_TIM_SET_CAPTUREPOLARITY(&htim4, TIM_CHANNEL_2, TIM_INPUTCHANNELPOLARITY_FALLING);
        st = hcsr04_wait_capture(HCSR04_TIMEOUT_US, &t_end);
    }

    (void)HAL_TIM_IC_Stop(&htim4, TIM_CHANNEL_2);

    if (st != HCSR04_OK) {
        return st;
    }

    /* 16 位差值自然处理计数器回绕（回波 < 65.5 ms，最多绕一次） */
    *pulse_us = (uint32_t)(uint16_t)(t_end - t_start);

    return HCSR04_OK;
}

hcsr04_status_t hcsr04_measure_cm(uint16_t *distance_cm)
{
    uint32_t pulse_us = 0U;
    hcsr04_status_t st;

    if (distance_cm == NULL) {
        return HCSR04_ERROR;
    }
    *distance_cm = 0U;

    st = hcsr04_measure_us(&pulse_us);
    if (st != HCSR04_OK) {
        return st;
    }

    /* 四舍五入到 cm */
    *distance_cm = (uint16_t)((pulse_us + (HCSR04_US_PER_CM / 2U)) / HCSR04_US_PER_CM);

    return HCSR04_OK;
}
