/**
 * @file demo_encoder.c
 * @brief 编码器圆环小球演示
 *
 * 屏幕边缘画一个圆环，小球沿圆环轨道做圆周运动；
 * 转动编码器（TIM5），小球随之转动。
 *
 * 说明：直接读取 encoder_count() 绝对计数，不经过 LVGL indev，
 *       因此不受 lv_port_indev 注册情况影响。
 */

#include "demo_encoder.h"
#include "lvgl.h"
#include "encoder.h"

/*---------------- 可调参数（修改后重新编译生效） ----------------*/

/* 编码器转一整圈对应的计数：档位数 × 4（TI12 四倍频）。
 * 常见旋转编码器 20 档/圈 -> 80；按实际模块调整 */
#define DEMO_ENC_COUNTS_PER_TURN   80

/* 几何参数（240x240 圆形屏，圆心 120,120，可视半径 120） */
#define DEMO_ENC_CX        120     /* 圆心 X */
#define DEMO_ENC_CY        120     /* 圆心 Y */
#define DEMO_ENC_RING_OUT  118     /* 圆环外半径 */
#define DEMO_ENC_RING_W    4       /* 圆环线宽 */
#define DEMO_ENC_PATH_R    114     /* 小球轨道半径（球心到圆心距离） */
#define DEMO_ENC_BALL_R    6       /* 小球半径：PATH_R + BALL_R = 120 恰好贴屏边 */

/* 刷新周期（ms） */
#define DEMO_ENC_PERIOD    20

/*---------------------------------------------------------------*/

static lv_obj_t *ball;

/* 根据编码器绝对计数计算角度（0~359 度，负计数自动折回正角度） */
static uint16_t enc_angle_deg(void)
{
    int32_t c = encoder_count();
    int32_t m = c % DEMO_ENC_COUNTS_PER_TURN;
    if (m < 0) {
        m += DEMO_ENC_COUNTS_PER_TURN;
    }
    return (uint16_t)((m * 360) / DEMO_ENC_COUNTS_PER_TURN);
}

static void ball_update_cb(lv_timer_t *timer)
{
    (void)timer;

    uint16_t deg = enc_angle_deg();

    /* lv_trigo_sin/cos：输入角度(度)，输出 ±32767 定标值 */
    lv_coord_t x = DEMO_ENC_CX + (DEMO_ENC_PATH_R * lv_trigo_cos((int16_t)deg)) / LV_TRIGO_SIN_MAX;
    lv_coord_t y = DEMO_ENC_CY + (DEMO_ENC_PATH_R * lv_trigo_sin((int16_t)deg)) / LV_TRIGO_SIN_MAX;

    lv_obj_set_pos(ball, x - DEMO_ENC_BALL_R, y - DEMO_ENC_BALL_R);
}

void demo_encoder_init(void)
{
    /* 屏幕底色：黑色 */
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_black(), 0);

    /* 圆环：透明底 + 圆形描边，位于屏幕边缘 */
    lv_obj_t *ring = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(ring);
    lv_obj_set_size(ring, 2 * DEMO_ENC_RING_OUT, 2 * DEMO_ENC_RING_OUT);
    lv_obj_center(ring);
    lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(ring, DEMO_ENC_RING_W, 0);
    lv_obj_set_style_border_color(ring, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_clear_flag(ring, LV_OBJ_FLAG_SCROLLABLE);

    /* 小球：实心圆 */
    ball = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(ball);
    lv_obj_set_size(ball, 2 * DEMO_ENC_BALL_R, 2 * DEMO_ENC_BALL_R);
    lv_obj_set_style_radius(ball, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(ball, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(ball, lv_palette_main(LV_PALETTE_CYAN), 0);
    lv_obj_clear_flag(ball, LV_OBJ_FLAG_SCROLLABLE);

    /* 初始位置 + 定时刷新 */
    ball_update_cb(NULL);
    lv_timer_create(ball_update_cb, DEMO_ENC_PERIOD, NULL);
}
