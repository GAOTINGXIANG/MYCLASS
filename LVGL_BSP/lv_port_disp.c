/**
 * @file lv_port_disp.c
 * @brief LVGL 显示移植层实现（GC9A01 240x240 + SPI1 DMA 双部分缓冲）
 *
 * 刷新流程：
 *   LVGL 渲染到缓冲 -> flush_cb 设置窗口 + 启动 SPI DMA -> DMA 完成中断
 *   -> HAL_SPI_TxCpltCallback 拉高 CS 并调用 lv_disp_flush_ready() 通知 LVGL
 */

#include "lv_port_disp.h"
#include "lvgl.h"
#include "spi.h"        /* SPI_HandleTypeDef / SPI1 */
#include "lcd_spi.h"
#include "gc9a01.h"

/* 单块缓冲高度（行）：240 * 40 * 2B = 19.2 KB，两块共 38.4 KB */
#define BUF_LINES   40

/* 两块部分缓冲（必须放主 SRAM，不能放 CCM，DMA 无法访问 CCM） */
static lv_color_t buf1[GC9A01_WIDTH * BUF_LINES];
static lv_color_t buf2[GC9A01_WIDTH * BUF_LINES];

/* 显示驱动句柄，供 DMA 完成回调使用 */
static lv_disp_drv_t disp_drv;

static void disp_flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p)
{
    uint32_t len;

    /* 设置绘图窗口，坐标闭区间 */
    gc9a01_set_window((uint16_t)area->x1, (uint16_t)area->y1,
                      (uint16_t)area->x2, (uint16_t)area->y2);

    /* 像素数据长度（字节） */
    len = (uint32_t)((area->x2 - area->x1 + 1) * (area->y2 - area->y1 + 1)) * sizeof(lv_color_t);

    /* 启动 DMA 发送（CS 在回调中拉高） */
    lcd_spi_write_dma((const uint8_t *)color_p, len);
}

void lv_port_disp_init(void)
{
    static lv_disp_draw_buf_t draw_buf;

    /* 双缓冲：LVGL 渲染一块时，另一块可在 DMA 传输 */
    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, GC9A01_WIDTH * BUF_LINES);

    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res  = GC9A01_WIDTH;
    disp_drv.ver_res  = GC9A01_HEIGHT;
    disp_drv.flush_cb = disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);
}

/* SPI 发送完成回调（HAL 弱定义，本工程仅此一处实现） */
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance == SPI1) {
        lcd_spi_cs_high();                           /* 结束本次传输 */
        lv_disp_flush_ready(&disp_drv);              /* 通知 LVGL 该块已刷完 */
    }
}
