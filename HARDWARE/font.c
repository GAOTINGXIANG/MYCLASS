/**
 * @file font.c
 * @brief 字模数据（当前留空，占位全为 0，显示为空白）
 *
 * ============================ 自行取模说明 ============================
 * 1. 使用取模软件（如 PCtoLCD2002）生成 ASCII 字模，参数选择：
 *      阴码 / 逐行式 / 逆向（低位在前需自行调整位序）/ 十六进制 / C51 格式
 *    本驱动按“高位在左、行优先”解析，即一行内 bit7 对应最左像素。
 * 2. 常见 8x16 字模：每字符 16 字节，从 0x20 到 0x7E 共 95 个字符。
 * 3. 把下面 font_ascii_8x16_data 的 95 组 16 字节替换为你生成的数组即可，
 *    并保持 font_ascii_8x16 的 width / height / bytes_per_char / char_count 一致。
 * 4. 若换用 16x16 等宽字模：width=16、height=16、bytes_per_char=32、
 *    char_count 按实际字符数，驱动会自动按 (width+7)/8 计算行字节数。
 * =====================================================================
 */

#include "font.h"
#include "stdlib.h"
#define FONT_W          8U
#define FONT_H          16U
#define FONT_BYTES      16U   /* height * ((width + 7) / 8) */
#define FONT_COUNT      95U   /* 0x20 ~ 0x7E */

/* TODO: 用自行取模的字模数据替换此占位数组 */
static const uint8_t font_ascii_8x16_data[FONT_COUNT][FONT_BYTES] = {{0}};

const font_t font_ascii_8x16 = {
    .width          = FONT_W,
    .height         = FONT_H,
    .bytes_per_char = FONT_BYTES,
    .char_count     = FONT_COUNT,
    .data           = &font_ascii_8x16_data[0][0],
};

const uint8_t *font_get_glyph(const font_t *font, char ch)
{
    uint8_t idx;

    if ((font == NULL) || (font->data == NULL) || (font->bytes_per_char == 0U)) {
        return NULL;
    }

    idx = (uint8_t)((uint8_t)ch - 0x20U);
    if (idx >= font->char_count) {
        idx = 0U;   /* 超出范围按空格处理 */
    }

    return &font->data[(uint32_t)idx * font->bytes_per_char];
}
