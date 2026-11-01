/**
 * @file demo_clock.h
 * @brief 钟表 demo：圆屏表盘 + 编码器/按键调时
 */

#ifndef __DEMO_CLOCK_H__
#define __DEMO_CLOCK_H__

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 创建表盘界面并启动走时/输入定时器
 *  @note  需在 lv_init()、lv_port_disp_init() 之后调用 */
void demo_clock_init(void);

#ifdef __cplusplus
}
#endif

#endif /* __DEMO_CLOCK_H__ */
