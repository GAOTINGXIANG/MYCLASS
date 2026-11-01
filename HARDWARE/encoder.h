/**
 * @file encoder.h
 * @brief 增量式编码器驱动（TIM5 编码器接口）
 *
 * 硬件：
 *   PA0 (ENCODER_B) -> TIM5_CH1
 *   PA1 (ENCODER_A) -> TIM5_CH2
 *
 * 说明：TIM5 已由 CubeMX 配置为编码器模式 TI12（A/B 相双边沿，4 倍计数），
 *       计数时钟 84 MHz、PSC=0、ARR=0xFFFFFFFF（32 位满量程）。
 *       使用 HAL_TIM_Encoder_Start() 启动后，CNT 随编码器旋转自动增减，
 *       读取 CNT 即可，无需中断。
 *
 * 使用示例：
 *   encoder_init();
 *   int32_t total = encoder_count();          // 绝对计数
 *   int32_t step  = encoder_delta();          // 本次与上次调用之间的增量，适合放在循环里
 *   if (encoder_dir() == ENC_DIR_CW) { ... }  // 最近一次增量方向
 */

#ifndef __ENCODER_H__
#define __ENCODER_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 计数方向反转：1 = 反转（不动机架接线/极性，直接在软件上反向） */
#define ENCODER_DIR_INVERT      0

/* 方向枚举 */
typedef enum {
    ENC_DIR_NONE = 0,   /* 无变化 */
    ENC_DIR_CW,         /* 正转（计数增加） */
    ENC_DIR_CCW         /* 反转（计数减少） */
} encoder_dir_t;

/** @brief 初始化：计数清零并启动 TIM5 编码器接口
 *  @note  需在 MX_TIM5_Init() 之后调用，只需调用一次 */
void encoder_init(void);

/** @brief 读取当前绝对计数（32 位有符号，上电从 0 开始，溢出自动回绕） */
int32_t encoder_count(void);

/**
 * @brief 读取自上次调用以来的增量（内部自动记录上次计数值）
 * @retval 正数为正转、负数为反转，0 表示未转动
 * @note  同一个循环里只调用一次，多处调用会“吃掉”增量；如需同时取计数请先调 encoder_count()
 */
int32_t encoder_delta(void);

/** @brief 最近一次 encoder_delta() 的方向 */
encoder_dir_t encoder_dir(void);

/** @brief 计数清零（同时清零增量基准） */
void encoder_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* __ENCODER_H__ */
