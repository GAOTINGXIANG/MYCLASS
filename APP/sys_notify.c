/**
 * @file sys_notify.c
 * @brief 声光提示服务：命令队列 + 表驱动效果序列执行器。
 *        每个效果是一张步骤表 {操作, 参数, 到下一步的延时ms}，
 *        执行器按时间戳推进，新命令立即抢占。
 *        NOTIFY_LIVES 为参数化效果：LED1..arg 常亮。
 */
#include "sys_notify.h"
#include "led.h"
#include "beep.h"
#include "cmsis_os.h"

/* ---------------- 步骤表定义 ---------------- */
enum {
    FX_BEEP,        /* arg: 1 响 / 0 停 */
    FX_LED,         /* arg: (led_id << 1) | on */
    FX_LED_ONLY,    /* arg: led_id，只亮这一颗（其余全灭） */
    FX_END,         /* 效果结束（LED/蜂鸣器保持最后状态） */
    FX_REPEAT,      /* 回到表头循环 */
};

typedef struct {
    uint8_t  op;
    uint8_t  arg;
    uint16_t ms;
} fx_step_t;

#define LED_ARG(id, on)  (uint8_t)(((id) << 1) | ((on) ? 1U : 0U))

/* ---- 蜂鸣时长（ms），按需调整 ---- */
#define BEEP_SHORT_MS   20
#define BEEP_LONG_MS    500

/* ---- 菜单切换流水灯 ----
 * MARQUEE_ONCE_TOTAL_MS = 流水灯完整跑一遍（LED1→LED6）的总时间 ms。
 * 调这个值相当于同时调整流水速度：时间越短，每颗灯停留越短、流动越快。
 */
#define MARQUEE_ONCE_TOTAL_MS   360
#define MARQUEE_ONCE_STEP_MS    (MARQUEE_ONCE_TOTAL_MS / LED_COUNT)

static const fx_step_t fx_beep_short[] = {
    { FX_BEEP, 1, BEEP_SHORT_MS }, { FX_BEEP, 0, 0 }, { FX_END, 0, 0 },
};
static const fx_step_t fx_beep_long[] = {
    { FX_BEEP, 1, BEEP_LONG_MS }, { FX_BEEP, 0, 0 }, { FX_END, 0, 0 },
};
static const fx_step_t fx_ok[] = {
    { FX_BEEP, 1, 100 }, { FX_BEEP, 0, 0 },
    { FX_LED, LED_ARG(LED_1, 1), 150 }, { FX_LED, LED_ARG(LED_1, 0), 150 },
    { FX_LED, LED_ARG(LED_1, 1), 150 }, { FX_LED, LED_ARG(LED_1, 0), 0 },
    { FX_END, 0, 0 },
};
static const fx_step_t fx_error[] = {
    { FX_BEEP, 1, 80 }, { FX_BEEP, 0, 80 },
    { FX_BEEP, 1, 80 }, { FX_BEEP, 0, 80 },
    { FX_BEEP, 1, 80 }, { FX_BEEP, 0, 0 },
    { FX_LED, LED_ARG(LED_6, 1), 150 }, { FX_LED, LED_ARG(LED_6, 0), 150 },
    { FX_LED, LED_ARG(LED_6, 1), 150 }, { FX_LED, LED_ARG(LED_6, 0), 150 },
    { FX_LED, LED_ARG(LED_6, 1), 150 }, { FX_LED, LED_ARG(LED_6, 0), 0 },
    { FX_END, 0, 0 },
};
static const fx_step_t fx_marquee[] = {
    { FX_LED_ONLY, LED_1, 80 }, { FX_LED_ONLY, LED_2, 80 },
    { FX_LED_ONLY, LED_3, 80 }, { FX_LED_ONLY, LED_4, 80 },
    { FX_LED_ONLY, LED_5, 80 }, { FX_LED_ONLY, LED_6, 80 },
    { FX_REPEAT, 0, 0 },
};

/* 切换选项反馈：先短滴一声，再流水跑一遍，末尾全灭。
 * 蜂鸣放在第一步，因此即使连续快速切换被新命令抢占，每次也一定会响。 */
static const fx_step_t fx_marquee_once[] = {
    { FX_BEEP,     1, BEEP_SHORT_MS },
    { FX_BEEP,     0, 0 },
    { FX_LED_ONLY, LED_1, MARQUEE_ONCE_STEP_MS },
    { FX_LED_ONLY, LED_2, MARQUEE_ONCE_STEP_MS },
    { FX_LED_ONLY, LED_3, MARQUEE_ONCE_STEP_MS },
    { FX_LED_ONLY, LED_4, MARQUEE_ONCE_STEP_MS },
    { FX_LED_ONLY, LED_5, MARQUEE_ONCE_STEP_MS },
    { FX_LED_ONLY, LED_6, MARQUEE_ONCE_STEP_MS },
    { FX_LED,      LED_ARG(LED_6, 0), 0 },
    { FX_END, 0, 0 },
};

static const fx_step_t *const fx_table[] = {
    NULL,            /* NOTIFY_STOP 特殊处理 */
    fx_beep_short,
    fx_beep_long,
    fx_ok,
    fx_error,
    fx_marquee,
    NULL,            /* NOTIFY_LIVES 动态生成 */
    fx_marquee_once,
};

#define FX_TABLE_COUNT  (sizeof(fx_table) / sizeof(fx_table[0]))

/* 队列消息 */
typedef struct {
    uint8_t cmd;
    uint8_t arg;
} notify_msg_t;

/* ---------------- 执行器状态 ---------------- */
static osMessageQueueId_t s_q;
static const fx_step_t   *s_seq;        /* 当前效果表，NULL=空闲 */
static uint8_t            s_idx;
static uint32_t           s_next_tick;
static volatile uint8_t   s_sound_on = 1U;   /* 蜂鸣器音效开关，默认开 */

static void fx_apply(const fx_step_t *st)
{
    switch (st->op) {
    case FX_BEEP:
        beep_write((s_sound_on != 0U) ? st->arg : 0U);
        break;
    case FX_LED:
        led_write((led_id_t)(st->arg >> 1), st->arg & 1U);
        break;
    case FX_LED_ONLY:
        led_all(0);
        led_on((led_id_t)st->arg);
        break;
    default:
        break;
    }
}

static void fx_start(notify_cmd_t cmd, uint8_t arg)
{
    /* 抢占任何效果前先关蜂鸣器：蜂鸣器只在效果步骤内短暂开启，
     * 不允许跨效果遗留"响"的状态；LED 刻意保持（生命显示需跨效果） */
    beep_write(0);

    if (cmd == NOTIFY_STOP || cmd >= (notify_cmd_t)FX_TABLE_COUNT) {
        s_seq = NULL;
        led_all(0);
        return;
    }

    if (cmd == NOTIFY_LIVES) {
        /* 即时效果：直接设置 LED 并返回，不打断正在播放的效果
         *（避免把刚开始的蜂鸣提示抢占成不可闻的短促电流声） */
        uint8_t n = (arg > LED_COUNT) ? LED_COUNT : arg;
        for (uint8_t i = 0; i < LED_COUNT; i++) {
            led_write((led_id_t)i, i < n);
        }
        return;
    }
    s_seq = fx_table[cmd];

    s_idx = 0;
    s_next_tick = osKernelGetTickCount();
    fx_apply(&s_seq[0]);               /* 第一步立即执行 */
    s_next_tick += s_seq[0].ms;
    s_idx = 1;
}

static void fx_advance(void)
{
    if (s_seq == NULL) return;

    while ((int32_t)(osKernelGetTickCount() - s_next_tick) >= 0) {
        const fx_step_t *st = &s_seq[s_idx];

        if (st->op == FX_END) {
            s_seq = NULL;
            return;
        }
        if (st->op == FX_REPEAT) {
            s_idx = 0;
            st = &s_seq[0];
        } else {
            s_idx++;
        }
        fx_apply(st);
        s_next_tick += st->ms;
        if (st->ms == 0U) continue;    /* 0 延时的步骤连发 */
        break;
    }
}

static uint32_t fx_wait_ms(void)
{
    int32_t diff;

    if (s_seq == NULL) return 1000U;
    diff = (int32_t)(s_next_tick - osKernelGetTickCount());
    return (diff <= 0) ? 0U : (uint32_t)diff;
}

/* ---------------- 对外接口 ---------------- */
void sys_notify_init(void)
{
    s_q = osMessageQueueNew(8U, sizeof(notify_msg_t), NULL);
    beep_write(0);
    led_all(0);
}

uint8_t notify_send_ex(notify_cmd_t cmd, uint8_t arg)
{
    notify_msg_t m;

    if (s_q == NULL) return 0U;
    m.cmd = (uint8_t)cmd;
    m.arg = arg;
    return (osMessageQueuePut(s_q, &m, 0U, 0U) == osOK) ? 1U : 0U;
}

uint8_t notify_send(notify_cmd_t cmd)
{
    return notify_send_ex(cmd, 0U);
}

void sys_notify_sound_set(uint8_t on)
{
    s_sound_on = (on != 0U) ? 1U : 0U;
    if (s_sound_on == 0U) beep_write(0);   /* 关闭时立即停声 */
}

uint8_t sys_notify_sound_get(void)
{
    return s_sound_on;
}

void sys_notify_loop(void)
{
    notify_msg_t m;

    for (;;) {
        if (osMessageQueueGet(s_q, &m, NULL, fx_wait_ms()) == osOK) {
            fx_start((notify_cmd_t)m.cmd, m.arg);   /* 新命令抢占 */
        }
        fx_advance();
    }
}
