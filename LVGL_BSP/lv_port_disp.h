/**
 * @file lv_port_disp.h
 * @brief LVGL 显示移植层（GC9A01 240x240 + SPI1 DMA）
 */

#ifndef __LV_PORT_DISP_H__
#define __LV_PORT_DISP_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 注册显示驱动并分配双缓冲，需在 lv_init() 之后、创建界面前调用
 *  @note  调用前 gc9a01_init() 必须已执行（完成 LCD 复位与背光开启） */
void lv_port_disp_init(void);

#ifdef __cplusplus
}
#endif

#endif /* __LV_PORT_DISP_H__ */
