/**
 * @file app_breakout.c
 * @brief 打砖块：编码器/摇杆控制挡板，活动区为圆屏内接正方形。
 *   编码器 / 摇杆左右 = 移动挡板（摇杆为比例控制：推得越多走得越快）
 *   KEY2/KEY3/JS/ENC  = 发球 / 结束后重开
 *   KEY1              = 返回主菜单
 *   6 个 LED 显示生命，丢球扣 1 命并蜂鸣提醒。
 */
#include "app_breakout.h"
#include "lvgl.h"
#include "encoder.h"
#include "keys.h"
#include "joystick.h"
#include "game_common.h"

#define BK_ORG    35
#define BK_W      170
#define PAD_W     36
#define PAD_H     6
#define PAD_Y     (BK_ORG + BK_W - 14)
#define BALL_R    3
#define BRICK_C   5
#define BRICK_R   4
#define BRICK_N   (BRICK_C * BRICK_R)
#define BRICK_W   30
#define BRICK_H   8
#define BRICK_GAP 3
#define BRICK_Y0  55
#define PERIOD    30
#define JOY_DEAD  150   /* 摇杆死区（归一化 -1000~1000） */
#define JOY_SPEED 6     /* 满推时每拍最大位移（px） */

static lv_timer_t *s_tm;
static lv_obj_t   *s_pad, *s_ball, *s_msg;
static lv_obj_t   *s_bricks[BRICK_N];
static int16_t  s_px = 120, s_bx, s_by;
static int8_t   s_vx = 2, s_vy = -3;
static uint8_t  s_launch, s_over, s_left;
static int32_t  s_last_cnt;

static void msg_show(const char *txt)
{
    lv_label_set_text(s_msg, txt);
    lv_obj_clear_flag(s_msg, LV_OBJ_FLAG_HIDDEN);
}

static void ball_render(void)
{
    lv_obj_set_pos(s_ball, s_bx - BALL_R, s_by - BALL_R);
}

static void ball_reset(void)
{
    s_launch = 0;
    s_vx = 2;
    s_vy = -3;
    s_bx = s_px;
    s_by = PAD_Y - BALL_R - 1;
    ball_render();
}

static void bricks_reset(void)
{
    for (uint8_t i = 0; i < BRICK_N; i++) {
        lv_obj_clear_flag(s_bricks[i], LV_OBJ_FLAG_HIDDEN);
    }
    s_left = BRICK_N;
}

static void breakout_cb(lv_timer_t *t)
{
    int32_t c;
    uint8_t edge;
    (void)t;

    /* 编码器控拍 */
    c = encoder_count();
    s_px += (int16_t)(c - s_last_cnt);
    s_last_cnt = c;

    /* 摇杆控拍（比例控制：偏移越大速度越快） */
    {
        joy_state_t js;
        joy_read(&js);
        if (js.x > JOY_DEAD) {
            s_px += (int16_t)((js.x - JOY_DEAD) * JOY_SPEED / (1000 - JOY_DEAD));
        } else if (js.x < -JOY_DEAD) {
            s_px += (int16_t)((js.x + JOY_DEAD) * JOY_SPEED / (1000 - JOY_DEAD));
        }
    }

    if (s_px < BK_ORG + PAD_W / 2)            s_px = BK_ORG + PAD_W / 2;
    if (s_px > BK_ORG + BK_W - PAD_W / 2)     s_px = BK_ORG + BK_W - PAD_W / 2;
    lv_obj_set_pos(s_pad, s_px - PAD_W / 2, PAD_Y);

    if (s_launch == 0U && s_over == 0U) {
        s_bx = s_px;
        s_by = PAD_Y - BALL_R - 1;
        ball_render();
    }

    /* 发球 / 重开 */
    edge = keys_scan();
    if ((edge & (KEY_MASK(KEY_ID_2) | KEY_MASK(KEY_ID_3) |
                 KEY_MASK(KEY_ID_JS) | KEY_MASK(KEY_ID_ENC))) != 0U) {
        if (s_over != 0U) {
            s_over = 0;
            game_lives_set(6);
            bricks_reset();
            ball_reset();
            lv_obj_add_flag(s_msg, LV_OBJ_FLAG_HIDDEN);
        } else if (s_launch == 0U) {
            s_launch = 1;
            lv_obj_add_flag(s_msg, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (s_launch == 0U || s_over != 0U) return;

    /* 球运动 */
    s_bx += s_vx;
    s_by += s_vy;

    if (s_bx < BK_ORG + BALL_R)          { s_bx = BK_ORG + BALL_R;          s_vx = (int8_t)-s_vx; }
    if (s_bx > BK_ORG + BK_W - BALL_R)   { s_bx = BK_ORG + BK_W - BALL_R;   s_vx = (int8_t)-s_vx; }
    if (s_by < BK_ORG + BALL_R)          { s_by = BK_ORG + BALL_R;          s_vy = (int8_t)-s_vy; }

    /* 丢球 */
    if (s_by > BK_ORG + BK_W - BALL_R) {
        if (game_lives_dec() == 0U) {
            s_over = 1;
            msg_show("游戏结束");
        }
        ball_reset();
        return;
    }

    /* 挡板碰撞（按击中位置改变反弹角） */
    if (s_vy > 0 && s_by + BALL_R >= PAD_Y && s_by < PAD_Y + PAD_H + 4 &&
        (s_bx - s_px <= PAD_W / 2 + BALL_R) && (s_px - s_bx <= PAD_W / 2 + BALL_R)) {
        int16_t off = (int16_t)((s_bx - s_px) * 3 / (PAD_W / 2));
        s_vy = (int8_t)-s_vy;
        s_by = PAD_Y - BALL_R - 1;
        s_vx = (int8_t)((off == 0) ? (s_vx > 0 ? 1 : -1) : off);
    }

    /* 砖块碰撞（每拍最多一块） */
    for (uint8_t i = 0; i < BRICK_N; i++) {
        if (lv_obj_has_flag(s_bricks[i], LV_OBJ_FLAG_HIDDEN)) continue;
        lv_coord_t bx = lv_obj_get_x(s_bricks[i]);
        lv_coord_t by = lv_obj_get_y(s_bricks[i]);
        if (s_bx + BALL_R >= bx && s_bx - BALL_R <= bx + BRICK_W &&
            s_by + BALL_R >= by && s_by - BALL_R <= by + BRICK_H) {
            lv_obj_add_flag(s_bricks[i], LV_OBJ_FLAG_HIDDEN);
            s_vy = (int8_t)-s_vy;
            if (--s_left == 0U) {
                s_over = 1;
                msg_show("你赢了");
            }
            break;
        }
    }

    ball_render();
}

static void app_breakout_enter(void)
{
    static const lv_palette_t row_pal[BRICK_R] = {
        LV_PALETTE_RED, LV_PALETTE_ORANGE, LV_PALETTE_YELLOW, LV_PALETTE_GREEN,
    };
    lv_obj_t *scr = lv_scr_act();

    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

    /* 活动区边框 */
    lv_obj_t *bd = lv_obj_create(scr);
    lv_obj_remove_style_all(bd);
    lv_obj_set_size(bd, BK_W, BK_W);
    lv_obj_set_pos(bd, BK_ORG, BK_ORG);
    lv_obj_set_style_border_width(bd, 1, 0);
    lv_obj_set_style_border_color(bd, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_clear_flag(bd, LV_OBJ_FLAG_SCROLLABLE);

    /* 砖块 */
    for (uint8_t r = 0; r < BRICK_R; r++) {
        for (uint8_t c = 0; c < BRICK_C; c++) {
            uint8_t i = (uint8_t)(r * BRICK_C + c);
            s_bricks[i] = lv_obj_create(scr);
            lv_obj_remove_style_all(s_bricks[i]);
            lv_obj_set_size(s_bricks[i], BRICK_W, BRICK_H);
            lv_obj_set_pos(s_bricks[i],
                BK_ORG + BRICK_GAP + c * (BRICK_W + BRICK_GAP),
                BRICK_Y0 + r * (BRICK_H + 4));
            lv_obj_set_style_radius(s_bricks[i], 2, 0);
            lv_obj_set_style_bg_opa(s_bricks[i], LV_OPA_COVER, 0);
            lv_obj_set_style_bg_color(s_bricks[i], lv_palette_main(row_pal[r]), 0);
        }
    }
    s_left = BRICK_N;

    /* 挡板 + 球 */
    s_pad = lv_obj_create(scr);
    lv_obj_remove_style_all(s_pad);
    lv_obj_set_size(s_pad, PAD_W, PAD_H);
    lv_obj_set_style_radius(s_pad, 3, 0);
    lv_obj_set_style_bg_opa(s_pad, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(s_pad, lv_color_white(), 0);

    s_ball = lv_obj_create(scr);
    lv_obj_remove_style_all(s_ball);
    lv_obj_set_size(s_ball, 2 * BALL_R, 2 * BALL_R);
    lv_obj_set_style_radius(s_ball, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(s_ball, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(s_ball, lv_palette_main(LV_PALETTE_CYAN), 0);

    /* 提示文本 */
    s_msg = lv_label_create(scr);
    lv_obj_set_style_text_color(s_msg, lv_color_white(), 0);
    lv_obj_align(s_msg, LV_ALIGN_CENTER, 0, 20);
    msg_show("按确认键开始");

    s_px = 120;
    s_over = 0;
    s_last_cnt = encoder_count();
    ball_reset();
    game_lives_set(6);
    s_tm = lv_timer_create(breakout_cb, PERIOD, NULL);
}

static void app_breakout_exit(void)
{
    if (s_tm != NULL) { lv_timer_del(s_tm); s_tm = NULL; }
    game_lives_clear();
}

const app_t app_breakout = { "打砖块", app_breakout_enter, app_breakout_exit };
