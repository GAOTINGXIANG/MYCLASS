/**
 * @file lcd_spi.c
 * @brief GC9A01 显示屏 SPI 传输层实现（基于 STM32 HAL，SPI1 1-line 主机）
 */

#include "lcd_spi.h"
#include "main.h"
#include "spi.h"        /* MX_SPI1_Init() 生成的 hspi1 句柄 */

/* 单次传输超时（ms）：42 MHz 下 512 字节约 98 us，100 ms 足够 */
#define LCD_SPI_TIMEOUT   100U

/* 控制引脚电平操作（引脚定义见 main.h） */
#define LCD_CS_LOW()      HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_RESET)
#define LCD_CS_HIGH()     HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET)
#define LCD_DC_CMD()      HAL_GPIO_WritePin(LCD_A0_GPIO_Port, LCD_A0_Pin, GPIO_PIN_RESET)
#define LCD_DC_DATA()     HAL_GPIO_WritePin(LCD_A0_GPIO_Port, LCD_A0_Pin, GPIO_PIN_SET)
#define LCD_RES_LOW()     HAL_GPIO_WritePin(LCD_RES_GPIO_Port, LCD_RES_Pin, GPIO_PIN_RESET)
#define LCD_RES_HIGH()    HAL_GPIO_WritePin(LCD_RES_GPIO_Port, LCD_RES_Pin, GPIO_PIN_SET)
#define LCD_BLK_ON()      HAL_GPIO_WritePin(LCD_LED_GPIO_Port, LCD_LED_Pin, GPIO_PIN_SET)
#define LCD_BLK_OFF()     HAL_GPIO_WritePin(LCD_LED_GPIO_Port, LCD_LED_Pin, GPIO_PIN_RESET)

/* 底层发送：阻塞式 SPI 发送（1-line 发送模式，只写） */
static void lcd_spi_tx(const uint8_t *buf, uint16_t len)
{
    (void)HAL_SPI_Transmit(&hspi1, (uint8_t *)buf, len, LCD_SPI_TIMEOUT);
}

void lcd_spi_init(void)
{
    /* 初始电平：片选拉高（不选中）、DC 为命令模式、背光先关 */
    LCD_CS_HIGH();
    LCD_DC_CMD();
    LCD_BLK_OFF();

    HAL_Delay(10);

    /* 硬件复位：低 20 ms 保持，释放后等待 120 ms 让芯片稳定 */
    LCD_RES_HIGH();
    HAL_Delay(10);
    LCD_RES_LOW();
    HAL_Delay(20);
    LCD_RES_HIGH();
    HAL_Delay(120);

    LCD_BLK_ON();
}

void lcd_write_cmd(uint8_t cmd)
{
    LCD_DC_CMD();
    LCD_CS_LOW();
    lcd_spi_tx(&cmd, 1);
    LCD_CS_HIGH();
}

void lcd_write_data(uint8_t data)
{
    LCD_DC_DATA();
    LCD_CS_LOW();
    lcd_spi_tx(&data, 1);
    LCD_CS_HIGH();
}

void lcd_write_data16(uint16_t data)
{
    uint8_t buf[2];

    buf[0] = (uint8_t)(data >> 8);      /* 高字节在前 */
    buf[1] = (uint8_t)(data & 0xFFU);

    LCD_DC_DATA();
    LCD_CS_LOW();
    lcd_spi_tx(buf, 2);
    LCD_CS_HIGH();
}

void lcd_write_data_buf(const uint8_t *buf, size_t len)
{
    if (buf == NULL || len == 0U) {
        return;
    }

    LCD_DC_DATA();
    LCD_CS_LOW();

    /* HAL_SPI_Transmit 长度参数为 uint16_t，超长时分段发送 */
    while (len > 0U) {
        uint16_t chunk = (len > 0xFFFFU) ? 0xFFFFU : (uint16_t)len;
        lcd_spi_tx(buf, chunk);
        buf += chunk;
        len -= chunk;
    }

    LCD_CS_HIGH();
}

void lcd_backlight(uint8_t on)
{
    if (on) {
        LCD_BLK_ON();
    } else {
        LCD_BLK_OFF();
    }
}
