/**
 * @file hcsr04.h
 * @brief HC-SR04 超声波测距模块驱动
 *
 * 硬件：
 *   Trig = PB9（GPIO 推挽输出，≥10 us 高脉冲触发）
 *   Echo = PB7（TIM4_CH2 输入捕获，1 us 计数分辨率）
 *
 * 说明：TIM4 已由 CubeMX 配置为 PSC=83（84 MHz → 1 MHz）、ARR=65535，
 *       计数 1 次 = 1 us，满量程 65.5 ms，足以覆盖 HC-SR04 最大约 23 ms 的回波。
 */

#ifndef __HCSR04_H__
#define __HCSR04_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    HCSR04_OK = 0,      /* 测量成功 */
    HCSR04_NO_ECHO,     /* 超时无回波（无障碍/超出量程/接线异常） */
    HCSR04_ERROR        /* 外设启动失败 */
} hcsr04_status_t;

/** @brief 初始化：Trig 拉低、启动 DWT 周期计数器用于 us 级延时
 *  @note  需在 MX_TIM4_Init() / MX_GPIO_Init() 之后调用 */
void hcsr04_init(void);

/**
 * @brief 测一次回波高电平宽度（阻塞式）
 * @param pulse_us 输出回波宽度（us），失败时为 0
 * @retval HCSR04_OK / HCSR04_NO_ECHO / HCSR04_ERROR
 * @note  阻塞时间最长约 40 ms；建议两次测量间隔 ≥ 60 ms，避免上一次回波未消失
 */
hcsr04_status_t hcsr04_measure_us(uint32_t *pulse_us);

/**
 * @brief 测一次距离（阻塞式）
 * @param distance_cm 输出距离（cm），失败时为 0
 * @retval 同 hcsr04_measure_us
 */
hcsr04_status_t hcsr04_measure_cm(uint16_t *distance_cm);

#ifdef __cplusplus
}
#endif

#endif /* __HCSR04_H__ */
