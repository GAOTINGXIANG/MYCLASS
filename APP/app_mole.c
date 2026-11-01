/**
 * @file app_mole.c
 * @brief 打地鼠：超声波测距控制锤子横向位置，锤子盖住地鼠即自动敲中。
 *   手离传感器越近，锤子越靠左（4cm = 最左，40cm = 最右，超出范围保持不动）
 *   辅助输入（任一操作都会接管锤子 600ms，随后交回超声波）：
 *     摇杆左右按住 = 平移，编码器旋转 = 平移，KEY4 右移 / KEY3 左移
 *   地鼠随机从 5 个洞口冒出，超过 1.4 秒未敲到算漏，扣 1 点生命（3 命）
 *   KEY2 / ENC / JS 结束重开，KEY1 返回主菜单
 *   距离数据来自 g_sensor（sensor_task 以 70ms 一拍快采样），本文件不碰 TIM4。
 */
#include "app_mole.h"
#include "lvgl.h"
#include "game_common.h"
#include "keys.h"
#include "encoder.h"
#include "joystick.h"
#include "sys_sensor.h"
#include "sys_notify.h"

#define TICK_MS      30
#define HOLE_N       5
#define HOLE_Y       190        /* 洞口所在的地面线 y */
#define HOLE_W       30
#define MOLE_W       26
#define MOLE_H       22
#define MOLE_UPT     46         /* 冒出停留 46 拍 ≈ 1.4s */
#define HIT_W        16         /* 锤子判定半宽 */
#define D_LO         4          /* 最左对应的距离 cm */
#define D_HI         40         /* 最右对应的距离 cm */
#define X_LO         22
#define X_HI         218
#define ENC_DETENT   4          /* 编码器每档计数（四倍频） */
#define MANUAL_STEP  6          /* 编码器 / 按键单次平移像素 */
#define JOY_STEP     4          /* 摇杆按住时每拍平移像素 */
#define MANUAL_TICKS 20         /* 手动接管 20 拍 ≈ 600ms 后交回超声波 */

static const lv_coord_t HOLE_X[HOLE_N] = { 30, 75, 120, 165, 210 };

static lv_timer_t *s_tm;
static lv_obj_t   *s_cv, *s_score, *s_msg;

static int16_t  s_hx = 120;         /* 锤子中心 x */
static int8_t   s_hole = -1;        /* 当前出鼠洞口，-1 = 无 */
static int8_t   s_last_hole = -1;
static uint8_t  s_mole_t;           /* 出鼠已持续拍数 */
static uint8_t  s_next_t;           /* 下一只出鼠倒计时（拍） */
static uint8_t  s_score_v, s_over;
static uint8_t  s_manual;           /* 手动接管剩余拍数，>0 时忽略距离 */
static int32_t  s_last_cnt;         /* 编码器上次计数 */
static uint32_t s_rng = 0x5A5A1234UL;

static uint32_t rnd(void)
{
    s_rng = s_rng * 1103515245UL + 12345UL;
    return (s_rng >> 16) & 0x7FFFUL;
}

/* ---------------- 绘制 ---------------- */
static void mole_box(lv_draw_ctx_t *ctx, lv_coord_t x1, lv_coord_t y1,
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

static void mole_draw(lv_event_t *e)
{
    lv_draw_ctx_t *ctx = lv_event_get_draw_ctx(e);

    /* 地面 */
    mole_box(ctx, 0, HOLE_Y, 240, 2, 0x606060, 0);

    /* 5 个洞口 */
    for (uint8_t i = 0; i < HOLE_N; i++) {
        mole_box(ctx, (lv_coord_t)(HOLE_X[i] - HOLE_W / 2), HOLE_Y,
                 HOLE_W, 8, 0x303030, 3);
    }

    /* 出鼠 */
    if (s_hole >= 0) {
        lv_coord_t x = (lv_coord_t)(HOLE_X[s_hole] - MOLE_W / 2);
        lv_coord_t y = (lv_coord_t)(HOLE_Y - MOLE_H);
        mole_box(ctx, x, y, MOLE_W, MOLE_H, 0xCC8844, 6);
        mole_box(ctx, (lv_coord_t)(x + 5), (lv_coord_t)(y + 6), 3, 3, 0x101010, 1);
        mole_box(ctx, (lv_coord_t)(x + MOLE_W - 8), (lv_coord_t)(y + 6), 3, 3, 0x101010, 1);
    }

    /* 锤子导轨 */
    mole_box(ctx, (lv_coord_t)(s_hx - 1), 100, 2, (lv_coord_t)(HOLE_Y - 100), 0x303030, 0);
    /* 锤子：锤身 + 锤柄 */
    mole_box(ctx, (lv_coord_t)(s_hx - 8), 112, 16, 12, 0xFFAA22, 3);
    mole_box(ctx, (lv_coord_t)(s_hx - 2), 124, 4, 12, 0x995522, 1);
}

/* ---------------- 逻辑 ---------------- */
static void msg_show(const char *txt)
{
    lv_label_set_text(s_msg, txt);
    lv_obj_clear_flag(s_msg, LV_OBJ_FLAG_HIDDEN);
}

/* 手动平移锤子：夹在导轨范围内，并接管 600ms */
static void manual_move(int16_t dx)
{
    int32_t x = (int32_t)s_hx + dx;

    if (x < X_LO) x = X_LO;
    if (x > X_HI) x = X_HI;
    s_hx     = (int16_t)x;
    s_manual = MANUAL_TICKS;
}

static void new_game(void)
{
    s_hx        = 120;
    s_hole      = -1;
    s_last_hole = -1;
    s_mole_t    = 0;
    s_next_t    = 10;
    s_score_v   = 0;
    s_over      = 0;
    s_manual    = 0;
    s_last_cnt  = encoder_count();
    game_lives_set(3);
    lv_label_set_text_fmt(s_score, "得分 %u", (unsigned)s_score_v);
    lv_obj_add_flag(s_msg, LV_OBJ_FLAG_HIDDEN);
    lv_obj_invalidate(s_cv);
}

static void mole_cb(lv_timer_t *t)
{
    uint8_t edge;
    (void)t;

    edge = keys_scan();
    if (s_over != 0U) {
        if ((edge & (KEY_MASK(KEY_ID_2) | KEY_MASK(KEY_ID_JS) |
                     KEY_MASK(KEY_ID_ENC))) != 0U) new_game();
        return;
    }

    /* 辅助输入：摇杆 / 编码器 / KEY3 KEY4 手动平移锤子 */
    {
        joy_mask_t m = joy_get_mask();
        int32_t cnt, d2;

        if ((m & JOY_MASK_LEFT)  != 0) manual_move(-JOY_STEP);
        if ((m & JOY_MASK_RIGHT) != 0) manual_move(JOY_STEP);

        cnt = encoder_count();
        d2  = cnt - s_last_cnt;
        s_last_cnt = cnt;
        while (d2 >= ENC_DETENT)  { d2 -= ENC_DETENT; manual_move(MANUAL_STEP); }
        while (d2 <= -ENC_DETENT) { d2 += ENC_DETENT; manual_move(-MANUAL_STEP); }

        if ((edge & KEY_MASK(KEY_ID_4)) != 0U) manual_move(MANUAL_STEP);
        if ((edge & KEY_MASK(KEY_ID_3)) != 0U) manual_move(-MANUAL_STEP);
    }

    /* 距离 → 锤子位置（手动接管期间不覆盖） */
    if (s_manual > 0U) {
        s_manual--;
    } else if (g_sensor.dist_ok != 0U) {
        int16_t d = (int16_t)g_sensor.dist_cm;
        int32_t x;
        if (d < D_LO) d = D_LO;
        if (d > D_HI) d = D_HI;
        x = X_LO + ((int32_t)(d - D_LO) * (X_HI - X_LO)) / (D_HI - D_LO);
        s_hx = (int16_t)x;
    }

    if (s_hole >= 0) {
        int16_t diff = (int16_t)(s_hx - HOLE_X[s_hole]);
        if (diff < 0) diff = (int16_t)-diff;

        if (diff <= HIT_W) {                 /* 自动敲中 */
            s_score_v++;
            lv_label_set_text_fmt(s_score, "得分 %u", (unsigned)s_score_v);
            notify_send(NOTIFY_BEEP_SHORT);
            s_hole   = -1;
            s_next_t = 12;                   /* 360ms 后下一只 */
        } else if (++s_mole_t >= MOLE_UPT) { /* 漏掉 */
            s_mole_t = 0;
            s_hole   = -1;
            if (game_lives_dec() == 0U) {
                s_over = 1U;
                msg_show("游戏结束");
            } else {
                s_next_t = 10;
            }
        }
    } else if (s_next_t > 0U) {
        s_next_t--;
    } else {
        uint8_t h = (uint8_t)(rnd() % HOLE_N);
        if ((int8_t)h == s_last_hole) h = (uint8_t)((h + 1U) % HOLE_N);
        s_last_hole = (int8_t)h;
        s_hole      = (int8_t)h;
        s_mole_t    = 0;
    }

    lv_obj_invalidate(s_cv);
}

static void app_mole_enter(void)
{
    lv_obj_t *scr = lv_scr_act();

    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

    s_cv = lv_obj_create(scr);
    lv_obj_remove_style_all(s_cv);
    lv_obj_set_size(s_cv, 240, 240);
    lv_obj_set_pos(s_cv, 0, 0);
    lv_obj_clear_flag(s_cv, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s_cv, mole_draw, LV_EVENT_DRAW_MAIN, NULL);

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
    s_tm = lv_timer_create(mole_cb, TICK_MS, NULL);
}

static void app_mole_exit(void)
{
    if (s_tm != NULL) { lv_timer_del(s_tm); s_tm = NULL; }
    sys_sensor_set_fast(0U);          /* 恢复 250ms */
    game_lives_clear();
}

const app_t app_mole = { "打地鼠", app_mole_enter, app_mole_exit };
