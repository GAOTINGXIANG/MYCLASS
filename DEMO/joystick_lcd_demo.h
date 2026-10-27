/**
 * @file joystick_lcd_demo.h
 * @brief 摇杆 + GC9A01 联动演示（240x240 圆屏，圆心 120,120）
 *
 * 现象：
 *   - 屏幕黑色背景 + 灰色外圈；
 *   - 上下左右 4 个指示点，推杆时对应方向点亮，斜推时两个同时点亮；
 *   - 青色小球随摇杆偏移量在圆内平滑移动，并限制在屏幕可见圆范围内。
 */

#ifndef __JOYSTICK_LCD_DEMO_H__
#define __JOYSTICK_LCD_DEMO_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 演示任务入口（死循环，需由 CubeMX/FreeRTOS 创建任务后调用）
 * @note  任务栈建议 >= 256 words；内部会调用 joy_init() 与 gc9a01_init()
 */
void joystick_lcd_demo_task(void *argument);

#ifdef __cplusplus
}
#endif

#endif /* __JOYSTICK_LCD_DEMO_H__ */
