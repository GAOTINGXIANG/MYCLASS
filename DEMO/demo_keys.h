/**
 * @file demo_keys.h
 * @brief 按键验证 demo：6 个指示灯对应 6 个按键，按下点亮
 */

#ifndef __DEMO_KEYS_H__
#define __DEMO_KEYS_H__

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 创建按键验证界面并启动刷新定时器
 *  @note  需在 lv_init() 和 lv_port_disp_init() 之后调用 */
void demo_keys_init(void);

#ifdef __cplusplus
}
#endif

#endif /* __DEMO_KEYS_H__ */
