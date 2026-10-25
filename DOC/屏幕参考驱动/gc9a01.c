/**
 * @file gc9a01.c
 * @brief GC9A01 显示屏驱动层实现
 *
 * 初始化序列参考 USER_doc/参考驱动2/1.28寸初始化HSD+GC9A01.txt，
 * 按 16bit(RGB565) 模式整理。
 */
#include "gc9a01.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "lcd_spi.h"
#include "font.h"

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

static inline void gc9a01_delay_ms(uint32_t ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
}

/* 写一条命令及可选参数数据 */
static void gc9a01_wr(uint8_t cmd, const uint8_t *data, uint8_t len)
{
    lcd_write_cmd(cmd);
    if (data && len) {
        lcd_write_data_buf(data, len);
    }
}

/* 寄存器初始化序列（参考驱动 2 逐条移植） */
static void gc9a01_init_sequence(void)
{
    gc9a01_wr(0xFE, NULL, 0);
    gc9a01_wr(0xEF, NULL, 0);

    gc9a01_wr(0xEB, (const uint8_t[]){0x14}, 1);

    gc9a01_wr(0x84, (const uint8_t[]){0x40}, 1);
    gc9a01_wr(0x85, (const uint8_t[]){0xF1}, 1);
    gc9a01_wr(0x86, (const uint8_t[]){0x98}, 1);
    gc9a01_wr(0x87, (const uint8_t[]){0x28}, 1);
    gc9a01_wr(0x88, (const uint8_t[]){0x0A}, 1);
    gc9a01_wr(0x89, (const uint8_t[]){0x21}, 1);
    gc9a01_wr(0x8A, (const uint8_t[]){0x00}, 1);
    gc9a01_wr(0x8B, (const uint8_t[]){0x80}, 1);
    gc9a01_wr(0x8C, (const uint8_t[]){0x01}, 1);
    gc9a01_wr(0x8D, (const uint8_t[]){0x00}, 1);
    gc9a01_wr(0x8E, (const uint8_t[]){0xDF}, 1);
    gc9a01_wr(0x8F, (const uint8_t[]){0x52}, 1);

    gc9a01_wr(0xB6, (const uint8_t[]){0x20}, 1);
    gc9a01_wr(GC9A01_MADCTL, (const uint8_t[]){GC9A01_MADCTL_VALUE}, 1);
    gc9a01_wr(GC9A01_COLMOD, (const uint8_t[]){0x05}, 1);   /* RGB565 */

    gc9a01_wr(0x90, (const uint8_t[]){0x08, 0x08, 0x08, 0x08}, 4);
    gc9a01_wr(0xBD, (const uint8_t[]){0x06}, 1);
    gc9a01_wr(0xBF, (const uint8_t[]){0x1C}, 1);
    gc9a01_wr(0xA7, (const uint8_t[]){0x45}, 1);
    gc9a01_wr(0xA9, (const uint8_t[]){0xBB}, 1);
    gc9a01_wr(0xB8, (const uint8_t[]){0x63}, 1);
    gc9a01_wr(0xBC, (const uint8_t[]){0x00}, 1);
    gc9a01_wr(0xFF, (const uint8_t[]){0x60, 0x01, 0x04}, 3);
    gc9a01_wr(0xC3, (const uint8_t[]){0x17}, 1);
    gc9a01_wr(0xC4, (const uint8_t[]){0x17}, 1);
    gc9a01_wr(0xC9, (const uint8_t[]){0x25}, 1);
    gc9a01_wr(0xBE, (const uint8_t[]){0x11}, 1);
    gc9a01_wr(0xE1, (const uint8_t[]){0x10, 0x0E}, 2);
    gc9a01_wr(0xDF, (const uint8_t[]){0x21, 0x10, 0x02}, 3);

    gc9a01_wr(0xF0, (const uint8_t[]){0x45, 0x09, 0x08, 0x08, 0x26, 0x2A}, 6);
    gc9a01_wr(0xF1, (const uint8_t[]){0x43, 0x70, 0x72, 0x36, 0x37, 0x6F}, 6);
    gc9a01_wr(0xF2, (const uint8_t[]){0x45, 0x09, 0x08, 0x08, 0x26, 0x2A}, 6);
    gc9a01_wr(0xF3, (const uint8_t[]){0x43, 0x70, 0x72, 0x36, 0x37, 0x6F}, 6);

    gc9a01_wr(0xED, (const uint8_t[]){0x1B, 0x0B}, 2);
    gc9a01_wr(0xAC, (const uint8_t[]){0x47}, 1);
    gc9a01_wr(0xAE, (const uint8_t[]){0x77}, 1);
    gc9a01_wr(0xCB, (const uint8_t[]){0x02}, 1);
    gc9a01_wr(0xCD, (const uint8_t[]){0x63}, 1);

    gc9a01_wr(0x70, (const uint8_t[]){0x07, 0x09, 0x04, 0x0E, 0x0F, 0x09, 0x07, 0x08, 0x03}, 9);
    gc9a01_wr(0xE8, (const uint8_t[]){0x34}, 1);

    gc9a01_wr(0x62, (const uint8_t[]){0x18, 0x0D, 0x71, 0xED, 0x70, 0x70,
                                      0x18, 0x0F, 0x71, 0xEF, 0x70, 0x70}, 12);
    gc9a01_wr(0x63, (const uint8_t[]){0x18, 0x11, 0x71, 0xF1, 0x70, 0x70,
                                      0x18, 0x13, 0x71, 0xF3, 0x70, 0x70}, 12);
    gc9a01_wr(0x64, (const uint8_t[]){0x28, 0x29, 0xF1, 0x01, 0xF1, 0x00, 0x07}, 7);
    gc9a01_wr(0x66, (const uint8_t[]){0x3C, 0x00, 0xCD, 0x67, 0x45, 0x45,
                                      0x10, 0x00, 0x00, 0x00}, 10);
    gc9a01_wr(0x67, (const uint8_t[]){0x00, 0x3C, 0x00, 0x00, 0x00, 0x01,
                                      0x54, 0x10, 0x32, 0x98}, 10);
    gc9a01_wr(0x74, (const uint8_t[]){0x10, 0x85, 0x80, 0x00, 0x00, 0x4E, 0x00}, 7);

    /* ---- 撕裂输出 / 显示反相 ---- */
    lcd_write_cmd(0x35);          /* 撕裂输出（无参数） */
    lcd_write_cmd(GC9A01_INVON);  /* 显示反相开 */
    gc9a01_delay_ms(120);

    /* ---- 退出睡眠 ---- */
    lcd_write_cmd(GC9A01_SLPOUT);
    gc9a01_delay_ms(120);

    /* ---- 开显示 ---- */
    lcd_write_cmd(GC9A01_DISPON);
    lcd_write_cmd(GC9A01_RAMWR);  /* 内存写 */
}

void gc9a01_init(void)
{
    lcd_spi_init();          /* SPI + GPIO 初始化，内含硬件复位 */
    gc9a01_init_sequence();  /* 寄存器初始化序列 */
    gc9a01_fill_screen(GC9A01_BLACK);   /* 清屏为黑色 */
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
    if (x >= GC9A01_WIDTH || y >= GC9A01_HEIGHT) {
        return;
    }
    gc9a01_set_window(x, y, x, y);
    lcd_write_data16(color);
}

void gc9a01_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    if (x >= GC9A01_WIDTH || y >= GC9A01_HEIGHT) {
        return;
    }
    if (x + w > GC9A01_WIDTH)  w = GC9A01_WIDTH - x;
    if (y + h > GC9A01_HEIGHT) h = GC9A01_HEIGHT - y;
    if (w == 0 || h == 0) {
        return;
    }

    gc9a01_set_window(x, y, x + w - 1, y + h - 1);

    /* 预填充一行像素数据（每像素 2 字节，高字节在前） */
    static uint8_t line[GC9A01_WIDTH * 2];
    for (uint16_t i = 0; i < w; i++) {
        line[i * 2]     = (uint8_t)(color >> 8);
        line[i * 2 + 1] = (uint8_t)(color & 0xFF);
    }

    for (uint16_t r = 0; r < h; r++) {
        lcd_write_data_buf(line, (size_t)w * 2);
    }
}

void gc9a01_fill_screen(uint16_t color)
{
    gc9a01_fill_rect(0, 0, GC9A01_WIDTH, GC9A01_HEIGHT, color);
}

void gc9a01_draw_char(uint16_t x, uint16_t y, char ch, uint16_t fg, uint16_t bg)
{
    const font_t *font = &font_ascii_8x16;
    const uint8_t *glyph = font_get_glyph(font, ch);

    gc9a01_set_window(x, y, x + font->width - 1, y + font->height - 1);

    /* 当前字模宽 8 像素，每像素 2 字节，共 16 字节 */
    uint8_t line[16];
    for (uint8_t row = 0; row < font->height; row++) {
        uint8_t bits = glyph[row];
        for (uint8_t col = 0; col < font->width; col++) {
            uint16_t c = (bits & (0x80 >> col)) ? fg : bg;
            line[col * 2]     = (uint8_t)(c >> 8);
            line[col * 2 + 1] = (uint8_t)(c & 0xFF);
        }
        lcd_write_data_buf(line, sizeof(line));
    }
}

void gc9a01_draw_string(uint16_t x, uint16_t y, const char *str, uint16_t fg, uint16_t bg)
{
    while (*str) {
        gc9a01_draw_char(x, y, *str++, fg, bg);
        x += font_ascii_8x16.width;
    }
}
