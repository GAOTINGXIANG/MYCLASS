/**
 * @file demo_compass.h
 * @brief 指南针 demo：表盘 + 红色指北针 + 方位角数字显示
 */

#ifndef __DEMO_COMPASS_H__
#define __DEMO_COMPASS_H__

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 创建指南针界面并启动采样定时器
 *  @note  需在 lv_init()、lv_port_disp_init()、hmc5883l_init() 之后调用 */
void demo_compass_init(void);

#ifdef __cplusplus
}
#endif

#endif /* __DEMO_COMPASS_H__ */
