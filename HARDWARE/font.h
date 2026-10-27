/**
 * @file font.h
 * @brief 字模接口（字模数据留空，由使用者自行取模后填入 font.c）
 *
 * 数据布局约定：
 *   - 字符按 ASCII 从 0x20（空格）开始连续排列，每个字符占 bytes_per_char 字节；
 *   - 每个字节按行优先排列，一行内高位在左（MSB 表示最左像素）；
 *   - 行字节数 = (width + 7) / 8，bytes_per_char = height * 行字节数。
 *   例：8x16 字模 → 每行 1 字节，每字符 16 字节。
 */

#ifndef __FONT_H__
#define __FONT_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t width;          /* 字符宽（像素） */
    uint8_t height;         /* 字符高（像素） */
    uint8_t bytes_per_char; /* 每个字符占用的字节数 */
    uint8_t char_count;     /* 字模数量（从 0x20 起连续） */
    const uint8_t *data;    /* 字模数据首地址 */
} font_t;

/* ASCII 8x16 字模（数据待自行取模填入 font.c） */
extern const font_t font_ascii_8x16;

/* ASCII 12x24 字模（24 号，数据留空待填充） */
extern const font_t font_ascii_12x24;

/* ASCII 16x32 字模（32 号，数据留空待填充） */
extern const font_t font_ascii_16x32;

/**
 * @brief 取指定字符的字模首地址
 * @param font 字模描述
 * @param ch   字符（超出范围时返回空格字模）
 * @retval 指向该字符 bytes_per_char 字节数据的指针
 */
const uint8_t *font_get_glyph(const font_t *font, char ch);

#ifdef __cplusplus
}
#endif

#endif /* __FONT_H__ */
