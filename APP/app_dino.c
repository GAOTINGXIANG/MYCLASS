/**
 * @file app_dino.c
 * @brief 小恐龙跳跃避障：超声波测距触发跳跃（手靠近传感器 = 跳）。
 *   距离 <= 12cm 起跳，需退到 >= 20cm 才允许下一跳（迟滞，防连跳）
 *   KEY2 / ENC / JS 也可手动跳（没有超声波时调试用）
 *   编码器旋转调节障碍速度（KEY4 加速 / KEY3 减速），范围 1 ~ SPEED_MAX
 *   KEY1 返回主菜单
 *   距离数据来自 g_sensor（sensor_task 以 70ms 一拍快采样），本文件不碰 TIM4。
 */
#include "app_dino.h"
#include "lvgl.h"
#include "game_common.h"
#include "keys.h"
#include "encoder.h"
#include "joystick.h"
#include "sys_sensor.h"

#define TICK_MS     30
#define SCR_W       240
#define GY          198        /* 地面线 y */
#define DX          34         /* 恐龙左边界 */
#define DW          18
#define DH          20
#define V0          116        /* 起跳初速（0.1px / 拍） */
#define GRAV        8          /* 重力（0.1px / 拍²）：顶点约 84px，滞空约 870ms */
#define NEAR_CM     12         /* 小于此距离触发跳跃 */
#define FAR_CM      20         /* 大于此距离才重新武装 */
#define SPEED0      2          /* 障碍初始速度 px/拍（≈67px/s） */
#define SPEED_MAX   20         /* 手动调速上限 */
#define ENC_DETENT  4          /* 编码器每档计数（四倍频） */
#define INVULN      25         /* 受伤后无敌拍数 */

static lv_timer_t *s_tm;
static lv_obj_t   *s_cv, *s_score, *s_msg;

static int16_t  s_ox;                 /* 障碍左边界 */
static uint8_t  s_ow, s_oh;
static int16_t  s_h;                  /* 离地高度 0.1px */
static int16_t  s_vy;                 /* 垂直速度 0.1px/拍 */
static uint8_t  s_speed, s_score_v, s_over, s_armed, s_invuln;
static uint32_t s_rng = 0x12345678UL;

static int32_t   s_last_cnt;          /* 编码器上次计数（增量调速基准） */
static joy_dir_t s_prev_dir;          /* 摇杆上次方向（边沿检测，推一下跳一次） */

static uint32_t rnd(void)
{
    s_rng = s_rng * 1103515245UL + 12345UL;
    return (s_rng >> 16) & 0x7FFFUL;
}

/* ---------------- 绘制 ---------------- */
static void dino_box(lv_draw_ctx_t *ctx, lv_coord_t x1, lv_coord_t y1,
                     lv_coord_t w, lv_coord_t h, uint32_t color, lv_coord_t radius)
{
    lv_draw_rect_dsc_t rd;
    lv_area_t a;

    lv_draw_rect_dsc_init(&rd);
    rd.bg_opa   = LV_OPA_COVER;
    rd.bg_color = lv_color_hex(color);
    rd.radius   = radius;
    a.x1 = x1;
    a.y1 = y1;
    a.x2 = (lv_coord_t)(x1 + w - 1);
    a.y2 = (lv_coord_t)(y1 + h - 1);
    lv_draw_rect(ctx, &rd, &a);
}

static void dino_draw(lv_event_t *e)
{
    lv_draw_ctx_t *ctx = lv_event_get_draw_ctx(e);
    lv_coord_t     dy;

    /* 地面 */
    dino_box(ctx, 0, GY, SCR_W, 2, 0x606060, 0);

    /* 障碍 */
    if (s_ox < SCR_W) {
        dino_box(ctx, s_ox, (lv_coord_t)(GY - (lv_coord_t)s_oh),
                 (lv_coord_t)s_ow, (lv_coord_t)s_oh, 0x22AA44, 3);
    }

    /* 恐龙（受伤期闪烁） */
    if (s_invuln == 0U || (s_invuln % 6U) < 3U) {
        dy = (lv_coord_t)(GY - DH - (lv_coord_t)(s_h / 10));
        dino_box(ctx, DX, dy, DW, DH, 0x44DD66, 4);
        dino_box(ctx, (lv_coord_t)(DX + 11), (lv_coord_t)(dy + 4), 3, 3, 0x101010, 1);
    }
}

/* ---------------- 逻辑 ---------------- */
static void msg_show(const char *txt)
{
    lv_label_set_text(s_msg, txt);
    lv_obj_clear_flag(s_msg, LV_OBJ_FLAG_HIDDEN);
}

static void hud_update(void)
{
    lv_label_set_text_fmt(s_score, "得分 %u 速度 %u",
                          (unsigned)s_score_v, (unsigned)s_speed);
}

static void speed_step(int8_t dir)
{
    int16_t s = (int16_t)s_speed + (int16_t)dir;

    if (s < 1) s = 1;
    if (s > SPEED_MAX) s = SPEED_MAX;
    s_speed = (uint8_t)s;
    hud_update();
}

static void spawn_obstacle(void)
{
    s_ow = (uint8_t)(10U + rnd() % 7U);
    s_oh = (uint8_t)(16U + rnd() % 11U);
    s_ox = SCR_W;
}

static void new_game(void)
{
    s_h  = 0;
    s_vy = 0;
    s_score_v = 0;
    s_speed   = SPEED0;
    s_over    = 0;
    s_armed   = 1;
    s_invuln  = 0;
    s_last_cnt = encoder_count();     /* 重同步，避免误吃旧增量 */
    s_prev_dir = joy_get_dir();
    game_lives_set(3);
    hud_update();
    lv_obj_add_flag(s_msg, LV_OBJ_FLAG_HIDDEN);
    spawn_obstacle();
    lv_obj_invalidate(s_cv);
}

static void do_jump(void)
{
    if (s_h > 0 || s_vy > 0) return;      /* 空中不能二段跳 */
    s_vy = V0;
}

static void dino_cb(lv_timer_t *t)
{
    uint8_t edge;
    int16_t d;
    (void)t;

    edge = keys_scan();
    if (s_over != 0U) {
        if ((edge & (KEY_MASK(KEY_ID_2) | KEY_MASK(KEY_ID_JS) |
                     KEY_MASK(KEY_ID_ENC))) != 0U) new_game();
        return;
    }

    /* 超声波：手靠近就跳，退远才重新武装 */
    d = (g_sensor.dist_ok != 0U) ? (int16_t)g_sensor.dist_cm : -1;
    if (d >= 0 && d <= NEAR_CM) {
        if (s_armed != 0U) { s_armed = 0U; do_jump(); }
    } else if (d < 0 || d >= FAR_CM) {
        s_armed = 1U;
    }

    /* 按键手动跳（调试） */
    if ((edge & (KEY_MASK(KEY_ID_2) | KEY_MASK(KEY_ID_JS) |
                 KEY_MASK(KEY_ID_ENC))) != 0U) do_jump();

    /* 摇杆：任意方向推一下 = 跳一次（边沿触发，按住不会连跳） */
    {
        joy_dir_t dir = joy_get_dir();
        if (dir != JOY_DIR_NONE && dir != s_prev_dir) do_jump();
        s_prev_dir = dir;
    }

    /* 编码器旋转 = 障碍调速；KEY4 加速 / KEY3 减速 */
    {
        int32_t cnt = encoder_count();
        int32_t d2  = cnt - s_last_cnt;
        s_last_cnt = cnt;
        while (d2 >= ENC_DETENT)  { d2 -= ENC_DETENT; speed_step(1); }
        while (d2 <= -ENC_DETENT) { d2 += ENC_DETENT; speed_step(-1); }
    }
    if ((edge & KEY_MASK(KEY_ID_4)) != 0U) speed_step(1);
    if ((edge & KEY_MASK(KEY_ID_3)) != 0U) speed_step(-1);

    /* 物理 */
    if (s_h > 0 || s_vy > 0) {
        s_vy = (int16_t)(s_vy - GRAV);
        s_h  = (int16_t)(s_h + s_vy);
        if (s_h <= 0) { s_h = 0; s_vy = 0; }
    }

    /* 障碍移动 */
    s_ox = (int16_t)(s_ox - (int16_t)s_speed);
    if (s_ox + (int16_t)s_ow <= DX) {          /* 安全通过 */
        spawn_obstacle();
        s_score_v++;
        hud_update();
    }

    /* 碰撞 */
    if (s_invuln > 0U) {
        s_invuln--;
    } else if (s_ox < DX + DW && s_ox + (int16_t)s_ow > DX &&
               (s_h / 10) < (int16_t)s_oh) {
        if (game_lives_dec() == 0U) {
            s_over = 1U;
            msg_show("游戏结束");
        } else {
            s_invuln = INVULN;
            spawn_obstacle();
        }
    }

    lv_obj_invalidate(s_cv);
}

static void app_dino_enter(void)
{
    lv_obj_t *scr = lv_scr_act();

    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

    s_cv = lv_obj_create(scr);
    lv_obj_remove_style_all(s_cv);
    lv_obj_set_size(s_cv, SCR_W, 240);
    lv_obj_set_pos(s_cv, 0, 0);
    lv_obj_clear_flag(s_cv, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s_cv, dino_draw, LV_EVENT_DRAW_MAIN, NULL);

    s_score = lv_label_create(scr);
    lv_obj_set_style_text_color(s_score, lv_color_white(), 0);
    lv_obj_align(s_score, LV_ALIGN_TOP_MID, 0, 6);

    s_msg = lv_label_create(scr);
    lv_obj_set_style_text_color(s_msg, lv_color_white(), 0);
    lv_obj_set_style_bg_color(s_msg, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_msg, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(s_msg, 6, 0);
    lv_obj_align(s_msg, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(s_msg, LV_OBJ_FLAG_HIDDEN);

    s_rng = lv_tick_get() | 1UL;
    sys_sensor_set_fast(1U);          /* 距离采样提速到 70ms */
    new_game();
    s_tm = lv_timer_create(dino_cb, TICK_MS, NULL);
}

static void app_dino_exit(void)
{
    if (s_tm != NULL) { lv_timer_del(s_tm); s_tm = NULL; }
    sys_sensor_set_fast(0U);          /* 恢复 250ms */
    game_lives_clear();
}

const app_t app_dino = { "小恐龙跳跃", app_dino_enter, app_dino_exit };
