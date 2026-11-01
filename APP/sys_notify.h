#ifndef __SYS_NOTIFY_H
#define __SYS_NOTIFY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* ================= 声光提示命令 =================
 * 任何任务/回调通过 notify_send() 发送命令，notify_task
 * 异步执行效果序列；新命令会立即打断正在播放的效果。
 * 效果生效期间 LED/蜂鸣器由 notify_task 独占，app 不要
 * 再直接调用 led / beep 系列函数。
 */
typedef enum {
    NOTIFY_STOP = 0,    /* 停止当前效果，LED 全灭、蜂鸣器关 */
    NOTIFY_BEEP_SHORT,  /* 滴 80ms */
    NOTIFY_BEEP_LONG,   /* 滴 300ms */
    NOTIFY_OK,          /* 滴一声 + LED1 闪 2 次 */
    NOTIFY_ERROR,       /* 三短滴 + LED6 闪 3 次 */
    NOTIFY_MARQUEE,     /* 流水灯循环（直到 STOP 或新命令） */
    NOTIFY_LIVES,       /* arg=剩余生命 0~6：LED1..n 常亮，其余灭 */
    NOTIFY_MARQUEE_ONCE,/* 流水灯完整跑一遍（时长由 MARQUEE_ONCE_TOTAL_MS 决定） */
} notify_cmd_t;

/* 发送命令；返回 1 成功（队列满或未初始化返回 0） */
uint8_t notify_send(notify_cmd_t cmd);

/* 带参数发送（NOTIFY_LIVES 用） */
uint8_t notify_send_ex(notify_cmd_t cmd, uint8_t arg);

/* notify_task 启动时调用一次（创建命令队列） */
void sys_notify_init(void);

/* 蜂鸣器音效总开关：默认开启；关闭后所有效果只做 LED、不出声（不掉电保存） */
void    sys_notify_sound_set(uint8_t on);
uint8_t sys_notify_sound_get(void);

/* notify_task 主循环，不返回 */
void sys_notify_loop(void);

#ifdef __cplusplus
}
#endif

#endif /* __SYS_NOTIFY_H */
