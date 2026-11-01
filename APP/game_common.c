/**
 * @file game_common.c
 * @brief 游戏公共模块：LED 生命值显示。
 */
#include "game_common.h"
#include "sys_notify.h"
#include "led.h"

static uint8_t s_lives;

/* ---------------- 生命值 ---------------- */
void game_lives_set(uint8_t n)
{
    s_lives = (n > LED_COUNT) ? LED_COUNT : n;
    notify_send_ex(NOTIFY_LIVES, s_lives);
}

uint8_t game_lives_dec(void)
{
    if (s_lives > 0U) s_lives--;
    notify_send(NOTIFY_BEEP_SHORT);            /* 扣血提醒 */
    notify_send_ex(NOTIFY_LIVES, s_lives);     /* 刷新 LED（效果不改灯，保持） */
    return s_lives;
}

uint8_t game_lives_get(void)
{
    return s_lives;
}

void game_lives_clear(void)
{
    s_lives = 0;
    notify_send(NOTIFY_STOP);
}
