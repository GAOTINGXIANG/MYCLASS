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
 * 轴序/方向调整，按“先交换轴序、再按逻辑轴反向”的顺序处理：
 *   JOY_XY_SWAP   = 1 时，X、Y 两轴互换（原本判 X 的轴改判 Y，反之亦然，
 *                   相当于摇杆旋转 90°）
 *   JOY_X_INVERT  = 1 时，左右互换
 *   JOY_Y_INVERT  = 1 时，“推上”才判为 JOY_DIR_UP
 *   JOY_XY_INVERT = 1 时，X、Y 同时翻转（相当于整个摇杆旋转 180°）
 */
#define JOY_XY_SWAP         1
#define JOY_X_INVERT        1
#define JOY_Y_INVERT        0
#define JOY_XY_INVERT       0

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

/* 两轴独立判定，得到方向位掩码（斜推时两位同时为 1） */
static joy_mask_t joy_mask_from_xy(int16_t x, int16_t y)
{
    uint8_t m = (uint8_t)JOY_MASK_NONE;

    if (y <= -(int16_t)JOY_DIR_THRESHOLD) {
        m |= (uint8_t)JOY_MASK_UP;
    }
    if (y >= (int16_t)JOY_DIR_THRESHOLD) {
        m |= (uint8_t)JOY_MASK_DOWN;
    }
    if (x <= -(int16_t)JOY_DIR_THRESHOLD) {
        m |= (uint8_t)JOY_MASK_LEFT;
    }
    if (x >= (int16_t)JOY_DIR_THRESHOLD) {
        m |= (uint8_t)JOY_MASK_RIGHT;
    }

    return (joy_mask_t)m;
}

/* 掩码 -> 8 方向枚举 */
static joy_dir_t joy_dir_from_mask(joy_mask_t mask)
{
    switch (mask) {
    case JOY_MASK_UP:                       return JOY_DIR_UP;
    case JOY_MASK_DOWN:                     return JOY_DIR_DOWN;
    case JOY_MASK_LEFT:                     return JOY_DIR_LEFT;
    case JOY_MASK_RIGHT:                    return JOY_DIR_RIGHT;
    case (JOY_MASK_UP | JOY_MASK_LEFT):     return JOY_DIR_UP_LEFT;
    case (JOY_MASK_UP | JOY_MASK_RIGHT):    return JOY_DIR_UP_RIGHT;
    case (JOY_MASK_DOWN | JOY_MASK_LEFT):   return JOY_DIR_DOWN_LEFT;
    case (JOY_MASK_DOWN | JOY_MASK_RIGHT):  return JOY_DIR_DOWN_RIGHT;
    default:                                return JOY_DIR_NONE;
    }
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
        st->x    = 0;
        st->y    = 0;
        st->dir  = JOY_DIR_NONE;
        st->mask = JOY_MASK_NONE;
        return;
    }

    x = joy_normalize(st->raw_x, joy_center_x);
    y = joy_normalize(st->raw_y, joy_center_y);

#if JOY_XY_SWAP
    {
        int16_t tmp = x;
        x = y;
        y = tmp;
    }
#endif

#if (JOY_X_INVERT || JOY_XY_INVERT)
    x = (int16_t)(-x);
#endif
#if (JOY_Y_INVERT || JOY_XY_INVERT)
    y = (int16_t)(-y);
#endif

    st->x    = x;
    st->y    = y;
    st->mask = joy_mask_from_xy(x, y);
    st->dir  = joy_dir_from_mask(st->mask);
}

joy_dir_t joy_get_dir(void)
{
    joy_state_t st;

    joy_read(&st);

    return st.dir;
}

joy_mask_t joy_get_mask(void)
{
    joy_state_t st;

    joy_read(&st);

    return st.mask;
}
