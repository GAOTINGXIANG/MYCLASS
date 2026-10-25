/**
 * @file joystick.c
 * @brief 摇杆读取与数字方向判断实现（ADC1 + DMA 循环采集）
 */

#include "joystick.h"
#include "adc.h"
#include "main.h"

/* DMA 缓冲区长度 = ADC 规则组转换个数（2） */
#define JOY_ADC_LEN         2U
#define JOY_ADC_CENTER_DEF  2048U   /* 12 位 ADC 默认中点 */
#define JOY_CAL_SAMPLES     16U     /* 上电求零点的采样次数 */

/*
 * 轴方向反转：接线或安装方向不同导致方向相反时改成 1。
 *   JOY_Y_INVERT = 1 时，“推上”才判为 JOY_DIR_UP
 *   JOY_X_INVERT = 1 时，左右互换
 */
#define JOY_X_INVERT        0
#define JOY_Y_INVERT        0

/*
 * 缓冲区内容：buf[0] = Rank1 = IN11 = VRy (PC1)
 *             buf[1] = Rank2 = IN13 = VRx (PC3)
 */
static uint16_t joy_adc_buf[JOY_ADC_LEN];
static uint16_t joy_center_x = JOY_ADC_CENTER_DEF;
static uint16_t joy_center_y = JOY_ADC_CENTER_DEF;
static uint8_t  joy_started   = 0U;

/* 原始值 -> 相对中心的归一化值（-1000 ~ +1000） */
static int16_t joy_normalize(uint16_t raw, uint16_t center)
{
    int32_t delta = (int32_t)raw - (int32_t)center;
    int32_t value = (delta * 1000) / 2048;

    if (value > 1000)  {
        value = 1000;
    }
    if (value < -1000) {
        value = -1000;
    }

    return (int16_t)value;
}

/* 按主轴（偏移较大的一轴）判定方向 */
static joy_dir_t joy_dir_from_xy(int16_t x, int16_t y)
{
    int16_t ax = (x < 0) ? (int16_t)(-x) : x;
    int16_t ay = (y < 0) ? (int16_t)(-y) : y;

    if ((ax < JOY_DIR_THRESHOLD) && (ay < JOY_DIR_THRESHOLD)) {
        return JOY_DIR_NONE;
    }

    if (ax >= ay) {
        return (x > 0) ? JOY_DIR_RIGHT : JOY_DIR_LEFT;
    }
    return (y > 0) ? JOY_DIR_DOWN : JOY_DIR_UP;
}

void joy_init(void)
{
    uint32_t sum_x = 0U;
    uint32_t sum_y = 0U;
    uint8_t  i;

    if (joy_started == 0U) {
        if (HAL_ADC_Start_DMA(&hadc1, (uint32_t *)joy_adc_buf, JOY_ADC_LEN) != HAL_OK) {
            return;     /* ADC 启动失败 */
        }
        joy_started = 1U;
    }

    /* 上电时摇杆应居中，连续采样求零点 */
    HAL_Delay(5);
    for (i = 0U; i < JOY_CAL_SAMPLES; i++) {
        sum_y += joy_adc_buf[0];
        sum_x += joy_adc_buf[1];
        HAL_Delay(2);
    }
    joy_center_x = (uint16_t)(sum_x / JOY_CAL_SAMPLES);
    joy_center_y = (uint16_t)(sum_y / JOY_CAL_SAMPLES);
}

void joy_read(joy_state_t *st)
{
    int16_t x;
    int16_t y;

    if (st == NULL) {
        return;
    }

    st->raw_x = joy_adc_buf[1];
    st->raw_y = joy_adc_buf[0];

    if (joy_started == 0U) {
        /* 未初始化时按中位处理，避免误判 */
        st->x   = 0;
        st->y   = 0;
        st->dir = JOY_DIR_NONE;
        return;
    }

    x = joy_normalize(st->raw_x, joy_center_x);
    y = joy_normalize(st->raw_y, joy_center_y);

#if JOY_X_INVERT
    x = (int16_t)(-x);
#endif
#if JOY_Y_INVERT
    y = (int16_t)(-y);
#endif

    st->x   = x;
    st->y   = y;
    st->dir = joy_dir_from_xy(x, y);
}

joy_dir_t joy_get_dir(void)
{
    joy_state_t st;

    joy_read(&st);

    return st.dir;
}
