#ifndef __SYS_SENSOR_H
#define __SYS_SENSOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* ================= 传感器共享数据 =================
 * sensor_task 周期采样写入，UI/app 只读。
 * 字段为简单整型，读写原子；显示用"最新值"语义，
 * 读到上一拍数据无害。xxx_ok=0 表示最近一次读取失败。
 */
typedef struct {
    /* DHT11（1 秒一拍） */
    uint8_t  dht_ok;
    uint8_t  temp;          /* 整数温度 */
    uint8_t  humi;          /* 整数湿度 */
    uint32_t dht_ms;        /* 最近一次采样时刻 */

    /* HC-SR04（250ms 一拍） */
    uint8_t  dist_ok;
    uint16_t dist_cm;       /* 距离 cm，无效时看 dist_ok */
    uint32_t dist_ms;
} sensor_data_t;

extern volatile sensor_data_t g_sensor;

/* sensor_task 主循环周期调用（内部按采样周期节流） */
void sys_sensor_poll(void);

#ifdef __cplusplus
}
#endif

#endif /* __SYS_SENSOR_H */
