#ifndef __APP_LAUNCHER_H
#define __APP_LAUNCHER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* ================= app 生命周期接口 =================
 * name : 菜单中显示的名字（中文，建议 <= 5 字，圆环排布）
 * enter: 进入 app 时调用，创建界面对象和 lv_timer
 * exit : 退出 app 时调用，必须删除自己创建的 lv_timer；
 *        挂在屏幕上的对象由框架统一 lv_obj_clean 清理
 *
 * 全局按键约定：
 *   编码器旋转 / 摇杆左右 = 菜单切换
 *   ENCODER_KEY / JS_KEY  = 确认
 *   KEY1                  = 返回上一级菜单（app 内随时可用）
 *   KEY2 / KEY3           = 用户键，归 app 自由使用
 *   KEY4                  = 用户键（菜单内为"下一个"）
 *
 * app 编写约定：
 * 1. app 运行期间 KEY1 归启动器所有，app 不要消费 KEY1。
 * 2. app 内读取输入：编码器用 encoder_count() 绝对值做差，
 *    按键用 keys_scan() 取按下沿。
 * 3. app 结构体用 const 全局定义：
 *      const app_t app_xxx = { "NAME", enter_fn, exit_fn };
 */
typedef struct {
    const char *name;
    void (*enter)(void);
    void (*exit)(void);
} app_t;

/* LVGL 任务启动时调用一次 */
void app_launcher_init(void);

/* 注册主菜单 app / 游戏 app（游戏进入 GAMES 子菜单） */
uint8_t app_register(const app_t *app);
uint8_t app_register_game(const app_t *app);

/* app 内部想主动退回主菜单时调用（可选） */
void app_launcher_exit_to_menu(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_LAUNCHER_H */
