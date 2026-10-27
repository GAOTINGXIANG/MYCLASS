/**
 * @file gc9a01.h
 * @brief GC9A01 1.28 寸 240x240 圆形 RGB-TFT 显示屏驱动层（初始化 + 绘图 + 字符显示）
 *
 * 依赖 lcd_spi.h（SPI 传输层）与 font.h（字模接口，字模数据需自行取模后填入）。
 */

#ifndef __GC9A01_H__
#define __GC9A01_H__

#include <stdint.h>
#include "font.h"
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
#define BLACK   0x0000
#define WHITE   0xFFFF
#define RED     0xF800
#define GREEN   0x07E0
#define BLUE    0x001F
#define YELLOW  0xFFE0
#define CYAN    0x07FF
#define MAGENTA 0xF81F

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

/**
 * @brief 以 (cx, cy) 为圆心、r 为半径画实心圆（即半径 r 的“大点”）
 * @note  自动裁剪到屏幕范围；r = 0 时退化为画单点
 */
void gc9a01_fill_circle(uint16_t cx, uint16_t cy, uint16_t r, uint16_t color);

/** @brief 显示一个字符 / 一串字符（字模来自 font.h，需自行取模填入） */
void gc9a01_draw_char(uint16_t x, uint16_t y, char ch, uint16_t fg, uint16_t bg);
void gc9a01_draw_string(uint16_t x, uint16_t y, const char *str, uint16_t fg, uint16_t bg);

/* 字号（字符高度，像素） */
typedef enum {
    GC9A01_FONT_16 = 0,     /* 8x16 字模（默认） */
    GC9A01_FONT_24,         /* 12x24 字模 */
    GC9A01_FONT_32          /* 16x32 字模 */
} gc9a01_font_size_t;

/** @brief 按字号切换后续 draw_char/draw_string/printf 使用的字模 */
void gc9a01_set_font_size(gc9a01_font_size_t size);

/** @brief 用指定字号显示字符串（只影响本次绘制，不改变当前字模） */
void gc9a01_draw_string_size(uint16_t x, uint16_t y, gc9a01_font_size_t size,
                             const char *str, uint16_t fg, uint16_t bg);

/**
 * @brief 切换字符/字符串/printf 使用的字模
 * @param font 字模描述（如 &font_ascii_8x16 / &font_ascii_12x24 / &font_ascii_16x32）
 * @note  传空或数据无效时保持原字模；默认为 font_ascii_8x16
 */
void gc9a01_set_font(const font_t *font);

/** @brief 获取当前字模 */
const font_t *gc9a01_get_font(void);

/* gc9a01_printf 内部格式化缓冲区大小，超出部分会被截断 */
#define GC9A01_PRINTF_BUF   64

/**
 * @brief 格式化显示字符串（与 printf 用法一致，默认使用 font_ascii_8x16）
 * @param x,y 左上角坐标（像素）
 * @param fg  文字颜色（RGB565）
 * @param bg  背景颜色（RGB565）
 * @param fmt 格式串，如 "T:%d C H:%d%%"
 * @note  使用静态缓冲区，非线程安全；不支持换行，超出屏幕右边缘的字符会被裁剪
 */
void gc9a01_printf(uint16_t x, uint16_t y, uint16_t fg, uint16_t bg, const char *fmt, ...);

/**
 * @brief 用指定字号格式化显示（只影响本次绘制，不改变当前字模）
 * @param size 字号：GC9A01_FONT_16 / GC9A01_FONT_24 / GC9A01_FONT_32
 */
void gc9a01_printf_size(uint16_t x, uint16_t y, gc9a01_font_size_t size,
                        uint16_t fg, uint16_t bg, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* __GC9A01_H__ */
