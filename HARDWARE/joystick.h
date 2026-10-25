/**
 * @file joystick.h
 * @brief 摇杆（双轴电位器）读取与数字方向判断
 *
 * 硬件：VRx = PC3 / ADC1_IN13（规则组 Rank 2）
 *       VRy = PC1 / ADC1_IN11（规则组 Rank 1）
 * ADC1 已配置为“扫描 + 连续转换 + DMA 循环”，本模块启动一次 DMA 后
 * 由硬件持续刷新缓冲区，读取时直接取最新采样值。
 */

#ifndef __JOYSTICK_H__
#define __JOYSTICK_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 方向判定死区（归一化值，范围 -1000 ~ +1000，值越大越不灵敏） */
#define JOY_DIR_THRESHOLD   300

/* 数字方向 */
typedef enum {
    JOY_DIR_NONE = 0,   /* 中位 */
    JOY_DIR_UP,
    JOY_DIR_DOWN,
    JOY_DIR_LEFT,
    JOY_DIR_RIGHT
} joy_dir_t;

/* 一次读取的结果 */
typedef struct {
    uint16_t raw_x;     /* VRx 原始 ADC 值（0~4095） */
    uint16_t raw_y;     /* VRy 原始 ADC 值（0~4095） */
    int16_t  x;         /* 相对中心的归一化值：-1000（左）~ +1000（右） */
    int16_t  y;         /* 相对中心的归一化值：-1000（上）~ +1000（下） */
    joy_dir_t dir;      /* 数字方向判断结果 */
} joy_state_t;

/**
 * @brief 初始化：启动 ADC DMA，并在中位采集若干次求零点
 * @note  需在 MX_ADC1_Init() 之后调用；初始化期间摇杆须处于中位
 */
void joy_init(void);

/**
 * @brief 读取当前摇杆状态（原始值、归一化值、方向）
 * @param st 输出结果，可为 NULL（此时不输出）
 */
void joy_read(joy_state_t *st);

/**
 * @brief 仅获取当前数字方向
 */
joy_dir_t joy_get_dir(void);

#ifdef __cplusplus
}
#endif

#endif /* __JOYSTICK_H__ */
