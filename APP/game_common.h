#ifndef __GAME_COMMON_H
#define __GAME_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* ================= 游戏公共模块 =================
 * - 生命值：6 个 LED 显示剩余生命，扣血蜂鸣提醒
 * 注意：一次只运行一个游戏，模块内部状态为单实例。
 */

void    game_lives_set(uint8_t n);     /* 进入游戏时设置（<=6），LED 常亮显示 */
uint8_t game_lives_dec(void);          /* 扣 1 点，蜂鸣提醒，返回剩余 */
uint8_t game_lives_get(void);
void    game_lives_clear(void);        /* 退出游戏时调用，熄灭全部 LED */

#ifdef __cplusplus
}
#endif

#endif /* __GAME_COMMON_H */
