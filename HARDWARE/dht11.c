/**
 * @file dht11.c
 * @brief DHT11 温湿度传感器驱动实现
 *
 * 时序（DHT11 手册）：
 *   主机拉低 ≥18 ms → 释放 20~40 us
 *   传感器应答：拉低 80 us → 拉高 80 us
 *   之后 40 位：每位 50 us 低电平 + 高电平（26~28 us=0，70 us=1）
 */

#include "dht11.h"
#include "main.h"

#define DHT11_START_LOW_US     18000U   /* 起始低脉冲 18 ms */
#define DHT11_RELEASE_US       30U      /* 释放后等待 30 us */
#define DHT11_BIT_THRESHOLD_US 40U      /* 高电平 >40 us 判为 1，否则判 0 */
#define DHT11_EDGE_TIMEOUT_US  100U     /* 单个边沿等待超时 */

#define DHT11_TICKS_PER_US     (SystemCoreClock / 1000000U)

static uint8_t dht11_inited = 0U;

static void dht11_delay_us(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = us * DHT11_TICKS_PER_US;

    while ((DWT->CYCCNT - start) < ticks) {
    }
}

/* 开漏输出写 1 即释放总线，由外部/内部上拉拉高 */
static void dht11_write(uint8_t level)
{
    HAL_GPIO_WritePin(DHT11_DATA_GPIO_Port, DHT11_DATA_Pin,
                      (level != 0U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/* 读总线实际电平（开漏输出模式下 IDR 仍反映引脚状态） */
static uint8_t dht11_read_pin(void)
{
    return (HAL_GPIO_ReadPin(DHT11_DATA_GPIO_Port, DHT11_DATA_Pin) == GPIO_PIN_SET) ? 1U : 0U;
}

/* 等待总线到达指定电平，超时返回 0 */
static uint8_t dht11_wait_level(uint8_t level, uint32_t timeout_us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t max_ticks = timeout_us * DHT11_TICKS_PER_US;

    while (dht11_read_pin() != level) {
        if ((DWT->CYCCNT - start) > max_ticks) {
            return 0U;
        }
    }

    return 1U;
}

void dht11_init(void)
{
    /* 释放总线，避免上电后被一直拉低 */
    dht11_write(1U);

    /* 使能 DWT 周期计数器，用于 us 级延时/超时 */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    dht11_inited = 1U;
}

dht11_status_t dht11_read(dht11_data_t *data)
{
    uint8_t  buf[5] = {0U, 0U, 0U, 0U, 0U};
    uint8_t  i;
    uint32_t t0;

    if (data == NULL) {
        return DHT11_ERR_PARAM;
    }

    if (dht11_inited == 0U) {
        dht11_init();
    }

    /* 1. 起始信号：拉低 18 ms 后释放 */
    dht11_write(0U);
    dht11_delay_us(DHT11_START_LOW_US);
    dht11_write(1U);
    dht11_delay_us(DHT11_RELEASE_US);

    /* 2. 应答：传感器先拉低 80 us，再拉高 80 us */
    if (dht11_wait_level(0U, DHT11_EDGE_TIMEOUT_US) == 0U) {
        return DHT11_ERR_NO_RESPONSE;
    }
    if (dht11_wait_level(1U, DHT11_EDGE_TIMEOUT_US) == 0U) {
        return DHT11_ERR_NO_RESPONSE;
    }
    if (dht11_wait_level(0U, DHT11_EDGE_TIMEOUT_US) == 0U) {
        return DHT11_ERR_NO_RESPONSE;
    }

    /* 3. 连续读 40 位，按高电平宽度判 0/1，高位在前 */
    for (i = 0U; i < 40U; i++) {
        uint32_t high_us;

        /* 等待当前数据位的高电平开始 */
        if (dht11_wait_level(1U, DHT11_EDGE_TIMEOUT_US) == 0U) {
            return DHT11_ERR_TIMEOUT;
        }

        t0 = DWT->CYCCNT;

        /* 等待高电平结束 */
        if (dht11_wait_level(0U, DHT11_EDGE_TIMEOUT_US) == 0U) {
            return DHT11_ERR_TIMEOUT;
        }

        high_us = (DWT->CYCCNT - t0) / DHT11_TICKS_PER_US;

        buf[i / 8U] = (uint8_t)(buf[i / 8U] << 1);
        if (high_us > DHT11_BIT_THRESHOLD_US) {
            buf[i / 8U] |= 0x01U;
        }
    }

    /* 4. 校验：前 4 字节之和的低 8 位应等于第 5 字节 */
    if ((uint8_t)(buf[0] + buf[1] + buf[2] + buf[3]) != buf[4]) {
        return DHT11_ERR_CHECKSUM;
    }

    data->humidity        = buf[0];
    data->humidity_dec    = buf[1];
    data->temperature     = buf[2];
    data->temperature_dec = buf[3];

    return DHT11_OK;
}
