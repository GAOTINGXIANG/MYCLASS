/**
 * @file demo_encoder.h
 * @brief 编码器圆环小球演示：转动编码器，小球沿屏幕边缘圆环做圆周运动
 */

#ifndef __DEMO_ENCODER_H__
#define __DEMO_ENCODER_H__

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 创建圆环 + 小球界面并启动更新定时器
 *  @note  需在 lv_init() 和 lv_port_disp_init() 之后调用 */
void demo_encoder_init(void);

#ifdef __cplusplus
}
#endif

#endif /* __DEMO_ENCODER_H__ */
