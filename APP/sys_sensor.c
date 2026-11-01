/**
 * @file sys_sensor.c
 * @brief 阻塞型传感器集中采样：DHT11(1s) + HC-SR04(250ms)。
 *        只在 sensor_task 中调用 sys_sensor_poll()，
 *        避免阻塞 LVGL 任务的界面刷新。
 */
#include "sys_sensor.h"
#include "dht11.h"
#include "hcsr04.h"
#include "main.h"   /* HAL_GetTick */

/* ---------------- 可调参数 ---------------- */
#define SYS_SENSOR_DHT_PERIOD    1000U   /* DHT11 最小间隔 1s */
#define SYS_SENSOR_DIST_PERIOD   250U    /* 常规距离采样周期 */
#define SYS_SENSOR_DIST_FAST_MS  70U     /* 游戏快速距离采样周期（HC-SR04 要求 >=60ms） */

volatile sensor_data_t g_sensor;

static volatile uint8_t s_dist_fast = 0U;

void sys_sensor_set_fast(uint8_t on)
{
    s_dist_fast = (on != 0U) ? 1U : 0U;
}

void sys_sensor_poll(void)
{
    static uint32_t last_dht  = 0;
    static uint32_t last_dist = 0;
    uint32_t now = HAL_GetTick();

    /* HC-SR04（游戏可切到 90ms 快采样） */
    if ((uint32_t)(now - last_dist) >=
        (s_dist_fast ? SYS_SENSOR_DIST_FAST_MS : SYS_SENSOR_DIST_PERIOD)) {
        uint16_t cm = 0;
        last_dist = now;
        if (hcsr04_measure_cm(&cm) == HCSR04_OK) {
            g_sensor.dist_cm = cm;
            g_sensor.dist_ok = 1U;
        } else {
            g_sensor.dist_ok = 0U;
        }
        g_sensor.dist_ms = now;
    }

    /* DHT11 */
    if ((uint32_t)(now - last_dht) >= SYS_SENSOR_DHT_PERIOD) {
        dht11_data_t d;
        last_dht = now;
        if (dht11_read(&d) == DHT11_OK) {
            g_sensor.temp   = d.temperature;
            g_sensor.humi   = d.humidity;
            g_sensor.dht_ok = 1U;
        } else {
            g_sensor.dht_ok = 0U;
        }
        g_sensor.dht_ms = now;
    }
}
