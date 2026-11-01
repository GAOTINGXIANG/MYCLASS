/**
 * @file demo_sensor.h
 * @brief 温湿度 + 超声波测距显示 demo
 */

#ifndef __DEMO_SENSOR_H__
#define __DEMO_SENSOR_H__

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 创建温湿度/距离显示界面并启动采样定时器
 *  @note  需在 lv_init()、lv_port_disp_init()、dht11_init()、hcsr04_init() 之后调用 */
void demo_sensor_init(void);

#ifdef __cplusplus
}
#endif

#endif /* __DEMO_SENSOR_H__ */
