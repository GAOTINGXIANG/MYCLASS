/**
 * @file dht11.h
 * @brief DHT11 温湿度传感器驱动（单总线，PC6 / DHT11_DATA）
 *
 * 硬件：PC6 已在 CubeMX 中配置为 开漏输出 + 内部上拉（GPIO_MODE_OUTPUT_OD / PULLUP），
 *       正好满足 DHT11 单总线要求：写 1 释放总线，直接读 IDR 即可。
 * 时序：全部用 DWT 周期计数器做 us 级延时与超时，不依赖 SysTick。
 *
 * 注意：
 *   - DHT11 采样率上限 1 Hz，两次读取间隔应 ≥ 1 s；
 *   - 内部上拉约 40 kΩ 偏弱，建议数据线外接 4.7~10 kΩ 上拉到 3.3 V；
 *   - 一次读取约 5 ms，期间不要有长时间高优先级中断，否则易校验失败。
 */

#ifndef __DHT11_H__
#define __DHT11_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    DHT11_OK = 0,           /* 读取成功且校验通过 */
    DHT11_ERR_PARAM,        /* 参数为空 */
    DHT11_ERR_NO_RESPONSE,  /* 传感器无响应（未接/上拉不足） */
    DHT11_ERR_TIMEOUT,      /* 时序超时 */
    DHT11_ERR_CHECKSUM      /* 校验和错误 */
} dht11_status_t;

/* 一次读取的结果（DHT11 小数位恒为 0） */
typedef struct {
    uint8_t humidity;       /* 湿度整数部分，%RH */
    uint8_t humidity_dec;   /* 湿度小数部分 */
    uint8_t temperature;    /* 温度整数部分，℃ */
    uint8_t temperature_dec;/* 温度小数部分 */
} dht11_data_t;

/**
 * @brief 初始化：释放数据线（拉高）、使能 DWT 计数器
 * @note  需在 MX_GPIO_Init() 之后调用；上电后建议等 ≥1 s 再首次读取
 */
void dht11_init(void);

/**
 * @brief 读取一次温湿度（阻塞，约 5 ms）
 * @param data 输出结果，成功时有效
 * @retval DHT11_OK / 对应错误码
 */
dht11_status_t dht11_read(dht11_data_t *data);

#ifdef __cplusplus
}
#endif

#endif /* __DHT11_H__ */
