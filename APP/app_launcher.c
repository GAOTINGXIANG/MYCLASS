/**
 * @file app_launcher.c
 * @brief 应用启动器：圆环菜单 + 游戏子菜单 + app 生命周期管理。
 *
 * 菜单：条目沿圆周均匀排布，中心大字显示当前选中项。
 *   编码器旋转 / 摇杆左右  = 切换选中
 *   ENCODER_KEY / JS_KEY   = 确认进入
 *   KEY1                   = 返回（子菜单回主菜单；app 退回主菜单）
 * 适配 240x240 圆屏（圆心 120,120）。
 */
#include "app_launcher.h"
#include "lvgl.h"
#include "keys.h"
#include "encoder.h"
#include "joystick.h"
#include "sys_notify.h"

/* ---------------- 可调参数 ---------------- */
#define APP_MAX          8        /* 每类最多 app 数 */
#define MENU_R           88       /* 菜单项圆周半径 */
#define MENU_ITEM_W      72       /* 菜单项标签宽（用于居中） */
#define MENU_ITEM_H      18
#define MENU_SCAN_MS     30
#define ENC_DETENT       4        /* 编码器每档计数（TI12 四倍频） */

/* ---------------- 状态 ---------------- */
typedef enum { ST_MAIN = 0, ST_GAMES, ST_APP } state_t;

static const app_t *s_main[APP_MAX];
static const app_t *s_game[APP_MAX];
static uint8_t      s_main_cnt;
static uint8_t      s_game_cnt;

static state_t      s_state = ST_MAIN;
static const app_t *s_running;              /* 运行中的 app */
static uint8_t      s_sel;                  /* 菜单高亮下标 */
static uint8_t      s_item_cnt;             /* 当前菜单条目数（含 GAMES 伪条目） */
static const char  *s_names[APP_MAX + 1];

static lv_obj_t    *s_items[APP_MAX + 1];
static lv_obj_t    *s_lbl_center;

static uint8_t      s_prev_back;
static int32_t      s_last_cnt;
static int16_t      s_enc_acc;
static joy_dir_t    s_prev_dir = JOY_DIR_NONE;

/* ---------------- 前向声明 ---------------- */
static void menu_open(state_t st);

/* ---------------- 注册 ---------------- */
uint8_t app_register(const app_t *app)
{
    if (app == NULL || app->enter == NULL || s_main_cnt >= APP_MAX) return 0U;
    s_main[s_main_cnt++] = app;
    return 1U;
}

uint8_t app_register_game(const app_t *app)
{
    if (app == NULL || app->enter == NULL || s_game_cnt >= APP_MAX) return 0U;
    s_game[s_game_cnt++] = app;
    return 1U;
}

/* ---------------- 内置 app 注册区 ----------------
 * 新 app：1) app_xxx.c 中定义 const app_t app_xxx
 *        2) 这里 extern + register 各加一行              */
#include "app_temp.h"
#include "app_radar.h"
#include "app_clock.h"
#include "app_compass.h"
#include "app_info.h"
#include "app_snake.h"
#include "app_breakout.h"
#include "app_2048.h"
#include "app_puzzle.h"
#include "app_gomoku.h"
#include "app_beans.h"
#include "app_tetris.h"

static void register_builtin_apps(void)
{
    app_register(&app_temp);
    app_register(&app_radar);
    app_register(&app_clock);
    app_register(&app_compass);
    app_register(&app_info);

    app_register_game(&app_snake);
    app_register_game(&app_breakout);
    app_register_game(&app_2048);
    app_register_game(&app_puzzle);
    app_register_game(&app_gomoku);
    app_register_game(&app_beans);
    app_register_game(&app_tetris);
}

/* ---------------- 圆环菜单 ---------------- */
static void polar(lv_coord_t r, int32_t deg, lv_coord_t *x, lv_coord_t *y)
{
    /* 屏幕坐标：0° 在正上方，顺时针 */
    *x = 120 + (r * lv_trigo_sin((int16_t)deg)) / LV_TRIGO_SIN_MAX;
    *y = 120 - (r * lv_trigo_cos((int16_t)deg)) / LV_TRIGO_SIN_MAX;
}

static void menu_refresh_sel(void)
{
    for (uint8_t i = 0; i < s_item_cnt; i++) {
        lv_obj_set_style_text_color(s_items[i],
            (i == s_sel) ? lv_color_white()
                         : lv_palette_main(LV_PALETTE_GREY), 0);
    }
    lv_label_set_text(s_lbl_center, s_names[s_sel]);
}

static void menu_open(state_t st)
{
    lv_obj_t *scr = lv_scr_act();
    uint8_t   cnt = 0;

    s_state = st;
    if (st == ST_MAIN) {
        for (uint8_t i = 0; i < s_main_cnt; i++) s_names[cnt++] = s_main[i]->name;
        if (s_game_cnt > 0U) s_names[cnt++] = "游戏";
    } else {
        for (uint8_t i = 0; i < s_game_cnt; i++) s_names[cnt++] = s_game[i]->name;
    }
    s_item_cnt = cnt;
    if (s_sel >= cnt) s_sel = 0;

    lv_obj_clean(scr);
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    /* 圆周排布的条目 */
    for (uint8_t i = 0; i < cnt; i++) {
        lv_coord_t x, y;
        lv_obj_t *lb = lv_label_create(scr);

        polar(MENU_R, (int32_t)i * 360 / (int32_t)cnt, &x, &y);
        lv_label_set_text(lb, s_names[i]);
        lv_obj_set_size(lb, MENU_ITEM_W, MENU_ITEM_H);
        lv_obj_set_style_text_align(lb, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(lb, x - MENU_ITEM_W / 2, y - MENU_ITEM_H / 2);
        s_items[i] = lb;
    }

    /* 中心大字显示选中项 */
    s_lbl_center = lv_label_create(scr);
    lv_obj_set_style_text_font(s_lbl_center, &lv_font_cn_14, 0);
    lv_obj_set_style_text_color(s_lbl_center, lv_color_white(), 0);
    lv_obj_center(s_lbl_center);

    menu_refresh_sel();
}

/* ---------------- app 启动 / 退出 ---------------- */
static void app_launch(const app_t *app)
{
    lv_obj_clean(lv_scr_act());
    s_state     = ST_APP;
    s_running   = app;
    s_prev_back = key_pressed(KEY_ID_1) ? 1U : 0U;
    app->enter();
}

void app_launcher_exit_to_menu(void)
{
    if (s_state != ST_APP || s_running == NULL) return;
    if (s_running->exit != NULL) s_running->exit();
    s_running = NULL;
    s_sel = 0;
    menu_open(ST_MAIN);
}

static void menu_confirm(void)
{
    if (s_state == ST_MAIN) {
        if (s_sel < s_main_cnt) app_launch(s_main[s_sel]);
        else                    menu_open(ST_GAMES);   /* GAMES 伪条目 */
    } else if (s_state == ST_GAMES) {
        if (s_sel < s_game_cnt) app_launch(s_game[s_sel]);
    }
}

/* ---------------- 输入扫描 ---------------- */
static void launcher_timer_cb(lv_timer_t *t)
{
    uint8_t back, back_edge;
    (void)t;

    /* KEY1 返回：电平沿检测（不被 app 的 keys_scan 抢走） */
    back = key_pressed(KEY_ID_1) ? 1U : 0U;
    back_edge = (back != 0U && s_prev_back == 0U) ? 1U : 0U;
    s_prev_back = back;

    if (s_state == ST_APP) {
        if (back_edge != 0U) app_launcher_exit_to_menu();
        return;
    }
    if (back_edge != 0U && s_state == ST_GAMES) {
        menu_open(ST_MAIN);
        return;
    }

    /* 编码器：累计计数换档 */
    {
        int32_t c = encoder_count();
        s_enc_acc += (int16_t)(c - s_last_cnt);
        s_last_cnt = c;
    }
    int move = 0;
    while (s_enc_acc >= ENC_DETENT)  { s_enc_acc -= ENC_DETENT; move++; }
    while (s_enc_acc <= -ENC_DETENT) { s_enc_acc += ENC_DETENT; move--; }

    /* 摇杆：方向沿（左右/上下均切换，上=前一个，下=后一个） */
    {
        joy_dir_t dir = joy_get_dir();
        if (dir != s_prev_dir) {
            if (dir == JOY_DIR_RIGHT || dir == JOY_DIR_UP_RIGHT ||
                dir == JOY_DIR_DOWN_RIGHT || dir == JOY_DIR_DOWN) {
                move++;
            } else if (dir == JOY_DIR_LEFT || dir == JOY_DIR_UP_LEFT ||
                       dir == JOY_DIR_DOWN_LEFT || dir == JOY_DIR_UP) {
                move--;
            }
            s_prev_dir = dir;
        }
    }

    if (move != 0 && s_item_cnt > 0U) {
        int32_t ns = (int32_t)s_sel + move;
        while (ns < 0) ns += s_item_cnt;
        s_sel = (uint8_t)((uint32_t)ns % s_item_cnt);
        menu_refresh_sel();
        notify_send(NOTIFY_MARQUEE_ONCE);        /* 切换选项：短滴一声 + LED 流水一遍 */
    }

    /* 确认：ENCODER_KEY / JS_KEY 按下沿 */
    if ((keys_scan() & (KEY_MASK(KEY_ID_JS) | KEY_MASK(KEY_ID_ENC))) != 0U) {
        menu_confirm();
    }
}

/* ---------------- 入口 ---------------- */
void app_launcher_init(void)
{
    register_builtin_apps();
    s_last_cnt = encoder_count();
    s_prev_back = key_pressed(KEY_ID_1) ? 1U : 0U;
    menu_open(ST_MAIN);
    lv_timer_create(launcher_timer_cb, MENU_SCAN_MS, NULL);
}
