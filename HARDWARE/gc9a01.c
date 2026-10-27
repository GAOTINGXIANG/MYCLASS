/**
 * @file gc9a01.c
 * @brief GC9A01 显示屏驱动实现（移植自 DOC/屏幕参考驱动，改写为 STM32 HAL 版本）
 *
 * 初始化序列按 16bit(RGB565) 模式整理，延时使用 HAL_Delay（初始化阶段不依赖调度器）。
 */

#include "gc9a01.h"
#include "lcd_spi.h"
#include "font.h"
#include "main.h"
#include <stdarg.h>
#include <stdio.h>

/* ---- GC9A01 命令码 ---- */
#define GC9A01_CASET    0x2A   /* 列地址设置 */
#define GC9A01_RASET    0x2B   /* 行地址设置 */
#define GC9A01_RAMWR    0x2C   /* 内存写 */
#define GC9A01_MADCTL   0x36   /* 内存数据访问控制（扫描方向） */
#define GC9A01_COLMOD   0x3A   /* 接口像素格式 */
#define GC9A01_INVON    0x21   /* 显示反相开 */
#define GC9A01_SLPOUT   0x11   /* 退出睡眠 */
#define GC9A01_DISPON   0x29   /* 开显示 */

/*
 * 扫描方向（MADCTL）：默认 0x48（MX=1, BGR=1），与参考驱动一致。
 * 若显示方向/颜色不对，可改：0x00 / 0x08 / 0xC8 / 0x48 等尝试。
 */
#define GC9A01_MADCTL_VALUE 0x48

/* 初始化表项：命令 + 参数字节数 + 参数 */
typedef struct {
    uint8_t cmd;
    uint8_t len;
    uint8_t data[12];
} gc9a01_init_cmd_t;

/* 寄存器初始化序列（逐条对应参考驱动，延时命令在表后单独处理） */
static const gc9a01_init_cmd_t gc9a01_init_cmds[] = {
    {0xFE, 0, {0}},
    {0xEF, 0, {0}},
    {0xEB, 1, {0x14}},

    {0x84, 1, {0x40}},
    {0x85, 1, {0xF1}},
    {0x86, 1, {0x98}},
    {0x87, 1, {0x28}},
    {0x88, 1, {0x0A}},
    {0x89, 1, {0x21}},
    {0x8A, 1, {0x00}},
    {0x8B, 1, {0x80}},
    {0x8C, 1, {0x01}},
    {0x8D, 1, {0x00}},
    {0x8E, 1, {0xDF}},
    {0x8F, 1, {0x52}},

    {0xB6, 1, {0x20}},
    {GC9A01_MADCTL, 1, {GC9A01_MADCTL_VALUE}},
    {GC9A01_COLMOD, 1, {0x05}},            /* RGB565 */

    {0x90, 4, {0x08, 0x08, 0x08, 0x08}},
    {0xBD, 1, {0x06}},
    {0xBF, 1, {0x1C}},
    {0xA7, 1, {0x45}},
    {0xA9, 1, {0xBB}},
    {0xB8, 1, {0x63}},
    {0xBC, 1, {0x00}},
    {0xFF, 3, {0x60, 0x01, 0x04}},
    {0xC3, 1, {0x17}},
    {0xC4, 1, {0x17}},
    {0xC9, 1, {0x25}},
    {0xBE, 1, {0x11}},
    {0xE1, 2, {0x10, 0x0E}},
    {0xDF, 3, {0x21, 0x10, 0x02}},

    {0xF0, 6, {0x45, 0x09, 0x08, 0x08, 0x26, 0x2A}},
    {0xF1, 6, {0x43, 0x70, 0x72, 0x36, 0x37, 0x6F}},
    {0xF2, 6, {0x45, 0x09, 0x08, 0x08, 0x26, 0x2A}},
    {0xF3, 6, {0x43, 0x70, 0x72, 0x36, 0x37, 0x6F}},

    {0xED, 2, {0x1B, 0x0B}},
    {0xAC, 1, {0x47}},
    {0xAE, 1, {0x77}},
    {0xCB, 1, {0x02}},
    {0xCD, 1, {0x63}},

    {0x70, 9, {0x07, 0x09, 0x04, 0x0E, 0x0F, 0x09, 0x07, 0x08, 0x03}},
    {0xE8, 1, {0x34}},

    {0x62, 12, {0x18, 0x0D, 0x71, 0xED, 0x70, 0x70, 0x18, 0x0F, 0x71, 0xEF, 0x70, 0x70}},
    {0x63, 12, {0x18, 0x11, 0x71, 0xF1, 0x70, 0x70, 0x18, 0x13, 0x71, 0xF3, 0x70, 0x70}},
    {0x64, 7, {0x28, 0x29, 0xF1, 0x01, 0xF1, 0x00, 0x07}},
    {0x66, 10, {0x3C, 0x00, 0xCD, 0x67, 0x45, 0x45, 0x10, 0x00, 0x00, 0x00}},
    {0x67, 10, {0x00, 0x3C, 0x00, 0x00, 0x00, 0x01, 0x54, 0x10, 0x32, 0x98}},
    {0x74, 7, {0x10, 0x85, 0x80, 0x00, 0x00, 0x4E, 0x00}},
};

/* 写命令及可选参数 */
static void gc9a01_wr(uint8_t cmd, const uint8_t *data, uint8_t len)
{
    lcd_write_cmd(cmd);
    if ((data != NULL) && (len > 0U)) {
        lcd_write_data_buf(data, len);
    }
}

/* 寄存器初始化序列 */
static void gc9a01_init_sequence(void)
{
    uint32_t i;

    for (i = 0U; i < (sizeof(gc9a01_init_cmds) / sizeof(gc9a01_init_cmds[0])); i++) {
        gc9a01_wr(gc9a01_init_cmds[i].cmd,
                  gc9a01_init_cmds[i].data,
                  gc9a01_init_cmds[i].len);
    }

    /* ---- 撕裂输出 / 显示反相 ---- */
    lcd_write_cmd(0x35);          /* 撕裂输出（无参数） */
    lcd_write_cmd(GC9A01_INVON);  /* 显示反相开 */
    HAL_Delay(120);

    /* ---- 退出睡眠 ---- */
    lcd_write_cmd(GC9A01_SLPOUT);
    HAL_Delay(120);

    /* ---- 开显示 ---- */
    lcd_write_cmd(GC9A01_DISPON);
    lcd_write_cmd(GC9A01_RAMWR);  /* 内存写 */
}

void gc9a01_init(void)
{
    lcd_spi_init();          /* SPI + GPIO 初始化，内含硬件复位 */
    gc9a01_init_sequence();  /* 寄存器初始化序列 */
    gc9a01_fill_screen(BLACK);   /* 清屏为黑色 */
}

void gc9a01_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    lcd_write_cmd(GC9A01_CASET);   /* 列地址 */
    lcd_write_data16(x0);
    lcd_write_data16(x1);

    lcd_write_cmd(GC9A01_RASET);   /* 行地址 */
    lcd_write_data16(y0);
    lcd_write_data16(y1);

    lcd_write_cmd(GC9A01_RAMWR);   /* 准备写内存 */
}

void gc9a01_draw_pixel(uint16_t x, uint16_t y, uint16_t color)
{
    if ((x >= GC9A01_WIDTH) || (y >= GC9A01_HEIGHT)) {
        return;
    }

    gc9a01_set_window(x, y, x, y);
    lcd_write_data16(color);

}


void gc9a01_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    static uint8_t line[GC9A01_WIDTH * 2];   /* 一行像素缓冲（每像素 2 字节） */
    uint16_t r;

    if ((x >= GC9A01_WIDTH) || (y >= GC9A01_HEIGHT) || (w == 0U) || (h == 0U)) {
        return;
    }
    if ((x + w) > GC9A01_WIDTH) {
        w = (uint16_t)(GC9A01_WIDTH - x);
    }
    if ((y + h) > GC9A01_HEIGHT) {
        h = (uint16_t)(GC9A01_HEIGHT - y);
    }

    gc9a01_set_window(x, y, (uint16_t)(x + w - 1U), (uint16_t)(y + h - 1U));

    /* 预填充一行像素数据（高字节在前） */
    for (r = 0U; r < w; r++) {
        line[r * 2U]      = (uint8_t)(color >> 8);
        line[(r * 2U) + 1U] = (uint8_t)(color & 0xFFU);
    }

    for (r = 0U; r < h; r++) {
        lcd_write_data_buf(line, (size_t)w * 2U);
    }
}

void gc9a01_fill_screen(uint16_t color)
{
    gc9a01_fill_rect(0U, 0U, GC9A01_WIDTH, GC9A01_HEIGHT, color);
}

/* 整数开方（牛顿迭代），避免引入浮点库 */
static uint32_t gc9a01_isqrt(uint32_t v)
{
    uint32_t x;
    uint32_t y;

    if (v < 2U) {
        return v;
    }

    x = v;
    y = (x + 1U) / 2U;
    while (y < x) {
        x = y;
        y = ((v / x) + x) / 2U;
    }

    return x;
}

void gc9a01_fill_circle(uint16_t cx, uint16_t cy, uint16_t r, uint16_t color)
{
    int32_t dy;
    int32_t rr;

    if (r == 0U) {
        gc9a01_draw_pixel(cx, cy, color);
        return;
    }

    rr = (int32_t)r * (int32_t)r;

    /* 逐行求该行半宽 dx，再用 fill_rect 画水平线 */
    for (dy = -(int32_t)r; dy <= (int32_t)r; dy++) {
        int32_t y = (int32_t)cy + dy;
        int32_t dx;
        int32_t x_start;
        int32_t x_end;

        if ((y < 0) || (y >= (int32_t)GC9A01_HEIGHT)) {
            continue;
        }

        dx = (int32_t)gc9a01_isqrt((uint32_t)(rr - (dy * dy)));

        x_start = (int32_t)cx - dx;
        x_end   = (int32_t)cx + dx;

        if ((x_end < 0) || (x_start >= (int32_t)GC9A01_WIDTH)) {
            continue;
        }
        if (x_start < 0) {
            x_start = 0;
        }
        if (x_end >= (int32_t)GC9A01_WIDTH) {
            x_end = (int32_t)GC9A01_WIDTH - 1;
        }

        gc9a01_fill_rect((uint16_t)x_start, (uint16_t)y,
                         (uint16_t)(x_end - x_start + 1), 1U, color);
    }
}

/* 当前使用的字模，默认 font_ascii_8x16 */
static const font_t *gc9a01_font = &font_ascii_8x16;

void gc9a01_set_font(const font_t *font)
{
    if ((font != NULL) && (font->data != NULL) && (font->bytes_per_char != 0U)) {
        gc9a01_font = font;
    }
}

const font_t *gc9a01_get_font(void)
{
    return gc9a01_font;
}

void gc9a01_draw_char(uint16_t x, uint16_t y, char ch, uint16_t fg, uint16_t bg)
{
    const font_t *font = gc9a01_font;
    const uint8_t *glyph;
    uint8_t row_bytes;
    uint8_t line[64];       /* 一行像素缓冲，支持行宽 <= 32 像素的字模 */
    uint8_t row;
    uint8_t col;

    if ((x + font->width > GC9A01_WIDTH) || (y + font->height > GC9A01_HEIGHT)) {
        return;
    }
    if (((uint16_t)font->width * 2U) > sizeof(line)) {
        return;             /* 字模过宽，暂不支持 */
    }

    glyph = font_get_glyph(font, ch);
    if (glyph == NULL) {
        return;
    }

    row_bytes = (uint8_t)((font->width + 7U) / 8U);

    gc9a01_set_window(x, y,
                      (uint16_t)(x + font->width - 1U),
                      (uint16_t)(y + font->height - 1U));

    for (row = 0U; row < font->height; row++) {
        for (col = 0U; col < font->width; col++) {
            uint8_t bits = glyph[row * row_bytes + (col >> 3)];
            uint16_t c = ((bits & (0x80U >> (col & 0x07U))) != 0U) ? fg : bg;
            line[col * 2U]        = (uint8_t)(c >> 8);
            line[(col * 2U) + 1U] = (uint8_t)(c & 0xFFU);
        }
        lcd_write_data_buf(line, (size_t)font->width * 2U);
    }
}

void gc9a01_draw_string(uint16_t x, uint16_t y, const char *str, uint16_t fg, uint16_t bg)
{
    if (str == NULL) {
        return;
    }

    while (*str != '\0') {
        gc9a01_draw_char(x, y, *str, fg, bg);
        str++;
        x = (uint16_t)(x + gc9a01_font->width);
    }
}

/* 字号 -> 字模指针（字号即字符高度） */
static const font_t *gc9a01_font_from_size(gc9a01_font_size_t size)
{
    switch (size) {
    case GC9A01_FONT_24:
        return &font_ascii_12x24;
    case GC9A01_FONT_32:
        return &font_ascii_16x32;
    case GC9A01_FONT_16:
    default:
        return &font_ascii_8x16;
    }
}

void gc9a01_set_font_size(gc9a01_font_size_t size)
{
    gc9a01_set_font(gc9a01_font_from_size(size));
}

/* 临时切换字模绘制字符串，绘制完成后恢复原字模 */
static void gc9a01_draw_string_font(const font_t *font, uint16_t x, uint16_t y,
                                    const char *str, uint16_t fg, uint16_t bg)
{
    const font_t *saved = gc9a01_font;

    gc9a01_set_font(font);
    gc9a01_draw_string(x, y, str, fg, bg);
    gc9a01_font = saved;
}

void gc9a01_draw_string_size(uint16_t x, uint16_t y, gc9a01_font_size_t size,
                             const char *str, uint16_t fg, uint16_t bg)
{
    gc9a01_draw_string_font(gc9a01_font_from_size(size), x, y, str, fg, bg);
}

static void gc9a01_vprintf_font(const font_t *font, uint16_t x, uint16_t y,
                                uint16_t fg, uint16_t bg, const char *fmt, va_list ap)
{
    static char buf[GC9A01_PRINTF_BUF];
    int len;

    len = vsnprintf(buf, sizeof(buf), fmt, ap);
    if (len < 0) {
        return;
    }

    buf[sizeof(buf) - 1U] = '\0';   /* 确保截断后仍以 0 结尾 */
    gc9a01_draw_string_font(font, x, y, buf, fg, bg);
}

void gc9a01_printf(uint16_t x, uint16_t y, uint16_t fg, uint16_t bg, const char *fmt, ...)
{
    va_list ap;

    if (fmt == NULL) {
        return;
    }

    va_start(ap, fmt);
    gc9a01_vprintf_font(gc9a01_font, x, y, fg, bg, fmt, ap);
    va_end(ap);
}

void gc9a01_printf_size(uint16_t x, uint16_t y, gc9a01_font_size_t size,
                        uint16_t fg, uint16_t bg, const char *fmt, ...)
{
    va_list ap;

    if (fmt == NULL) {
        return;
    }

    va_start(ap, fmt);
    gc9a01_vprintf_font(gc9a01_font_from_size(size), x, y, fg, bg, fmt, ap);
    va_end(ap);
}
