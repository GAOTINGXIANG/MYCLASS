/**
 * @file keys.c
 * @brief 板载按键读取实现（时间戳消抖）
 */

#include "keys.h"
#include "main.h"

/*---------------- 可调参数（修改后重新编译生效） ----------------*/

/* 消抖时间（ms）：电平保持该时长后才认为状态真正变化 */
#define KEYS_DEBOUNCE_MS   20U

/*---------------------------------------------------------------*/

typedef struct {
    GPIO_TypeDef *port;
    uint16_t      pin;
} key_hw_t;

static const key_hw_t key_hw[KEY_ID_MAX] = {
    { KEY1_GPIO_Port,       KEY1_Pin       },   /* KEY_ID_1   PC5  */
    { KEY2_GPIO_Port,       KEY2_Pin       },   /* KEY_ID_2   PB1  */
    { KEY3_GPIO_Port,       KEY3_Pin       },   /* KEY_ID_3   PE8  */
    { KEY4_GPIO_Port,       KEY4_Pin       },   /* KEY_ID_4   PE10 */
    { JS_KEY_GPIO_Port,     JS_KEY_Pin     },   /* KEY_ID_JS  PD9  */
    { ENCODER_KEY_GPIO_Port,ENCODER_KEY_Pin},   /* KEY_ID_ENC PD3  */
};

static uint8_t  stable;                        /* 消抖后稳定状态 */
static uint8_t  last_raw;                      /* 上次采样电平 */
static uint8_t  edges;                         /* 待取走的按下沿 */
static uint32_t change_tick[KEY_ID_MAX];       /* 每键电平最近变化时刻 */

/* 读取原始电平：按下（低电平）返回 1 */
static uint8_t key_raw(key_id_t id)
{
    return (HAL_GPIO_ReadPin(key_hw[id].port, key_hw[id].pin) == GPIO_PIN_RESET) ? 1U : 0U;
}

/* 内部更新：采样 + 消抖，多次调用幂等 */
static void keys_update(void)
{
    uint32_t now = HAL_GetTick();

    for (uint8_t i = 0; i < (uint8_t)KEY_ID_MAX; i++) {
        uint8_t raw = key_raw((key_id_t)i);
        uint8_t mask = KEY_MASK(i);

        if (raw != ((last_raw & mask) ? 1U : 0U)) {
            /* 电平刚变化，记录时刻，等待消抖 */
            change_tick[i] = now;
            if (raw) { last_raw |= mask; } else { last_raw &= (uint8_t)~mask; }
        } else if ((raw != ((stable & mask) ? 1U : 0U)) &&
                   ((now - change_tick[i]) >= KEYS_DEBOUNCE_MS)) {
            /* 电平稳定超过消抖时间，确认状态切换 */
            if (raw) {
                stable |= mask;
                edges  |= mask;          /* 记录按下沿 */
            } else {
                stable &= (uint8_t)~mask;
            }
        }
    }
}

uint8_t key_pressed(key_id_t id)
{
    keys_update();
    return (stable & KEY_MASK(id)) ? 1U : 0U;
}

uint8_t keys_state(void)
{
    keys_update();
    return stable;
}

uint8_t keys_scan(void)
{
    keys_update();
    uint8_t e = edges;
    edges = 0;
    return e;
}

/* 编码器确认键：覆盖 lv_port_indev.c 中的 weak 默认实现 */
uint8_t lv_port_encoder_key(void)
{
    keys_update();
    return (stable & KEY_MASK(KEY_ID_ENC)) ? 1U : 0U;
}
