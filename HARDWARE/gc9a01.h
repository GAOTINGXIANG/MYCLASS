/**
 * @file gc9a01.h
 * @brief GC9A01 1.28 寸 240x240 圆形 RGB-TFT 显示屏驱动层（初始化 + 绘图 + 字符显示）
 *
 * 依赖 lcd_spi.h（SPI 传输层）与 font.h（字模接口，字模数据需自行取模后填入）。
 */

#ifndef __GC9A01_H__
#define __GC9A01_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 屏幕分辨率 */
#define GC9A01_WIDTH   240
#define GC9A01_HEIGHT  240

/* RGB888 -> RGB565 颜色转换宏 */
#define GC9A01_RGB565(r, g, b) \
    ((((uint16_t)(r) & 0xF8) << 8) | (((uint16_t)(g) & 0xFC) << 3) | (((uint16_t)(b) & 0xF8) >> 3))

/* 常用颜色（RGB565） */
#define GC9A01_BLACK   0x0000
#define GC9A01_WHITE   0xFFFF
#define GC9A01_RED     0xF800
#define GC9A01_GREEN   0x07E0
#define GC9A01_BLUE    0x001F
#define GC9A01_YELLOW  0xFFE0
#define GC9A01_CYAN    0x07FF
#define GC9A01_MAGENTA 0xF81F

/** @brief 初始化显示屏（内部完成传输层初始化 + 寄存器初始化 + 清屏为黑色）
 *  @note  调用前需已完成 MX_GPIO_Init() / MX_SPI1_Init() */
void gc9a01_init(void);

/** @brief 设置绘图窗口（坐标为 0 起，GC9A01 无面板偏移） */
void gc9a01_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);

/** @brief 画一个点 */
void gc9a01_draw_pixel(uint16_t x, uint16_t y, uint16_t color);

/** @brief 填充矩形 / 填充整屏 */
void gc9a01_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void gc9a01_fill_screen(uint16_t color);

/** @brief 显示一个字符 / 一串字符（字模来自 font.h，需自行取模填入） */
void gc9a01_draw_char(uint16_t x, uint16_t y, char ch, uint16_t fg, uint16_t bg);
void gc9a01_draw_string(uint16_t x, uint16_t y, const char *str, uint16_t fg, uint16_t bg);

#ifdef __cplusplus
}
#endif

#endif /* __GC9A01_H__ */
