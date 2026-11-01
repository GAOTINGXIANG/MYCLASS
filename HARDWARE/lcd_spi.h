/**
 * @file lcd_spi.h
 * @brief GC9A01 显示屏 SPI 传输层（STM32 HAL / SPI1 1-line 主机）
 *
 * 依赖：CubeMX 已生成 MX_SPI1_Init()（SPI1 主机、1-line 发送、8bit、Mode0），
 *       且 PA5=SCK、PA7=MOSI、PE7=DC(A0)、PE9=RES、PE11=CS、PA2=背光 已配置。
 */

#ifndef __LCD_SPI_H__
#define __LCD_SPI_H__

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief LCD 传输层初始化：片选/DC 初始电平、硬件复位、开背光
 * @note  必须在 MX_SPI1_Init() 之后调用
 */
void lcd_spi_init(void);

/** @brief 写 1 字节命令（DC = 0） */
void lcd_write_cmd(uint8_t cmd);

/** @brief 写 1 字节数据（DC = 1） */
void lcd_write_data(uint8_t data);

/** @brief 写 2 字节数据，高字节在前（RGB565 像素） */
void lcd_write_data16(uint16_t data);

/** @brief 连续写 len 字节数据（DC = 1） */
void lcd_write_data_buf(const uint8_t *buf, size_t len);

/** @brief DMA 发送 len 字节像素数据：拉低 CS、置 DC=数据，启动 DMA（不等待完成）
 *  @note  传输完成后会进入 HAL_SPI_TxCpltCallback，需在其中拉高 CS 并调用 lv_disp_flush_ready() */
void lcd_spi_write_dma(const uint8_t *buf, uint32_t len);

/** @brief 拉高片选，结束一次 DMA 传输 */
void lcd_spi_cs_high(void);

/** @brief 背光开关：on 非 0 点亮 */
void lcd_backlight(uint8_t on);

#ifdef __cplusplus
}
#endif

#endif /* __LCD_SPI_H__ */
