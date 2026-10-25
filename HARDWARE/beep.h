/**
 * @file beep.h
 * @brief 有源蜂鸣器控制（PB11 / BEEP_Y）
 *
 * 有源蜂鸣器只需给通断电平即可发声，无需 PWM；PB11 电流驱动能力有限，
 * 实际板子建议经三极管/MOS 管驱动。
 */

#ifndef __BEEP_H__
#define __BEEP_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 驱动电平：1 = 高电平响（三极管/MOS 驱动），若接法相反改为 0 */
#define BEEP_ACTIVE_LEVEL   1

/** @brief 初始化：蜂鸣器关闭 */
void beep_init(void);

/** @brief 打开蜂鸣器 */
void beep_on(void);

/** @brief 关闭蜂鸣器 */
void beep_off(void);

/** @brief 翻转蜂鸣器状态 */
void beep_toggle(void);

/**
 * @brief 按状态控制蜂鸣器
 * @param on 非 0 响，0 停
 */
void beep_write(uint8_t on);

/**
 * @brief 响 ms 毫秒后自动关闭（阻塞式，内部用 HAL_Delay）
 * @note  在 FreeRTOS 任务中调用会占用当前任务，长鸣请改用 beep_on/beep_off 配合延时
 */
void beep(uint32_t ms);

#ifdef __cplusplus
}
#endif

#endif /* __BEEP_H__ */
