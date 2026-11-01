/**
 * @file encoder.c
 * @brief 增量式编码器驱动实现（TIM5 编码器接口）
 */

#include "encoder.h"
#include "tim.h"

/* 上次读取的计数值（32 位，做差时按无符号运算处理回绕） */
static int32_t s_last_count = 0;
/* 最近一次增量及其方向 */
static int32_t s_last_delta = 0;
static encoder_dir_t s_last_dir = ENC_DIR_NONE;

void encoder_init(void)
{
    /* 清零计数并启动编码器接口（CC1/CC2 同时使能，计数器开始工作） */
    __HAL_TIM_SET_COUNTER(&htim5, 0U);
    (void)HAL_TIM_Encoder_Start(&htim5, TIM_CHANNEL_ALL);

    s_last_count = 0;
    s_last_delta = 0;
    s_last_dir = ENC_DIR_NONE;
}

int32_t encoder_count(void)
{
    int32_t count = (int32_t)__HAL_TIM_GET_COUNTER(&htim5);

#if ENCODER_DIR_INVERT
    count = -count;
#endif

    return count;
}

int32_t encoder_delta(void)
{
    uint32_t now = __HAL_TIM_GET_COUNTER(&htim5);
    int32_t delta;

    /* 无符号做差后再转有符号，32 位计数器回绕时结果仍正确 */
    delta = (int32_t)(now - (uint32_t)s_last_count);

    s_last_count = (int32_t)now;

#if ENCODER_DIR_INVERT
    delta = -delta;
#endif

    s_last_delta = delta;
    if (delta > 0) {
        s_last_dir = ENC_DIR_CW;
    } else if (delta < 0) {
        s_last_dir = ENC_DIR_CCW;
    } else {
        s_last_dir = ENC_DIR_NONE;
    }

    return delta;
}

encoder_dir_t encoder_dir(void)
{
    return s_last_dir;
}

void encoder_reset(void)
{
    __HAL_TIM_SET_COUNTER(&htim5, 0U);
    s_last_count = 0;
    s_last_delta = 0;
    s_last_dir = ENC_DIR_NONE;
}
