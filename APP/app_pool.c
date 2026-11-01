/**
 * @file app_pool.c
 * @brief 台球（简化）：一张小桌，1 白球 + 3 彩球，瞄准后按 KEY2 击球，打进洞加分。
 *   编码器 / 摇杆左右 / KEY3 / KEY4 = 调整瞄准角度
 *   KEY2 / JS / ENC  = 击球（结束后重开）
 *   KEY1             = 返回上一级
 *   3 个 LED 显示生命，白球进洞扣 1 命。
 */
#include "app_pool.h"
#include "lvgl.h"
#include "game_common.h"
#include "keys.h"
#include "encoder.h"
#include "joystick.h"

#define PERIOD      20
#define TBX0        44          /* 台面内沿 */
#define TBY0        59
#define TBX1        196
#define TBY1        171
#define BALL_R      5
#define BALL_N      3          /* 彩球数（不含白球） */
#define POCKET_R    8
#define POCKET_N    6
#define MAX_V       10
#define ENC_DETENT  4
#define ANG_MAX     80
#define FRIC_TICK   3

typedef struct {
    int16_t x, y;
    int8_t  vx, vy;
    uint8_t act;
} pball_t;

static lv_timer_t *s_tm;
static lv_obj_t   *s_cv, *s_score, *s_msg;

static pball_t  s_bb[BALL_N + 1];      /* [0] = 白球 */
static int16_t  s_ang;
static uint8_t  s_fric;
static uint8_t  s_over;
static uint16_t s_score_v;
static int32_t  s_last_cnt;

static const int16_t POCKET_X[POCKET_N] = { TBX0, 120, TBX1, TBX0, 120, TBX1 };
static const int16_t POCKET_Y[POCKET_N] = { TBY0, TBY0, TBY0, TBY1, TBY1, TBY1 };

/* ---------------- 绘制 ---------------- */
static void box(lv_draw_ctx_t *ctx, int16_t x, int16_t y, int16_t w, int16_t h,
                lv_color_t c, int16_t r)
{
    lv_draw_rect_dsc_t d;
    lv_area_t a;

    lv_draw_rect_dsc_init(&d);
    d.bg_opa = LV_OPA_COVER;
    d.bg_color = c;
    d.radius = r;
    a.x1 = x;
    a.y1 = y;
    a.x2 = (lv_coord_t)(x + w - 1);
    a.y2 = (lv_coord_t)(y + h - 1);
    lv_draw_rect(ctx, &d, &a);
}

static void ball(lv_draw_ctx_t *ctx, int16_t cx, int16_t cy, int16_t r, lv_color_t c)
{
    box(ctx, (int16_t)(cx - r), (int16_t)(cy - r),
        (int16_t)(2 * r), (int16_t)(2 * r), c, LV_RADIUS_CIRCLE);
}

static void line(lv_draw_ctx_t *ctx, int16_t x1, int16_t y1, int16_t x2, int16_t y2,
                 lv_color_t c)
{
    lv_draw_line_dsc_t d;
    lv_point_t p1, p2;

    lv_draw_line_dsc_init(&d);
    d.color = c;
    d.width = 1;
    d.opa   = LV_OPA_COVER;
    p1.x = x1; p1.y = y1;
    p2.x = x2; p2.y = y2;
    lv_draw_line(ctx, &d, &p1, &p2);
}

static void pool_draw(lv_event_t *e)
{
    lv_draw_ctx_t *ctx = lv_event_get_draw_ctx(e);

    /* 桌框 + 台面 */
    box(ctx, (int16_t)(TBX0 - 6), (int16_t)(TBY0 - 6), (int16_t)(TBX1 - TBX0 + 12),
        (int16_t)(TBY1 - TBY0 + 12), lv_palette_main(LV_PALETTE_BROWN), 4);
    box(ctx, TBX0, TBY0, (int16_t)(TBX1 - TBX0), (int16_t)(TBY1 - TBY0),
        lv_palette_main(LV_PALETTE_GREEN), 2);

    for (uint8_t i = 0; i < POCKET_N; i++) {
        ball(ctx, POCKET_X[i], POCKET_Y[i], POCKET_R, lv_color_black());
    }

    /* 瞄准线（白球静止时） */
    if (s_bb[0].vx == 0 && s_bb[0].vy == 0 && s_over == 0U) {
        int16_t dx = (int16_t)((int32_t)30 * lv_trigo_sin((int16_t)s_ang) / LV_TRIGO_SIN_MAX);
        int16_t dy = (int16_t)((int32_t)30 * lv_trigo_cos((int16_t)s_ang) / LV_TRIGO_SIN_MAX);
        line(ctx, s_bb[0].x, s_bb[0].y, (int16_t)(s_bb[0].x + dx), (int16_t)(s_bb[0].y - dy),
             lv_color_white());
    }

    ball(ctx, s_bb[0].x, s_bb[0].y, BALL_R, lv_color_white());
    for (uint8_t i = 1; i <= BALL_N; i++) {
        if (s_bb[i].act == 0U) continue;
        ball(ctx, s_bb[i].x, s_bb[i].y, BALL_R,
             (i == 1) ? lv_palette_main(LV_PALETTE_RED)
                      : ((i == 2) ? lv_palette_main(LV_PALETTE_YELLOW)
                                  : lv_palette_main(LV_PALETTE_BLUE)));
    }
}

/* ---------------- 逻辑 ---------------- */
static int8_t decay(int8_t v)
{
    if (v > 0) return (int8_t)(v - 1);
    if (v < 0) return (int8_t)(v + 1);
    return 0;
}

static void balls_reset(void)
{
    for (uint8_t i = 0; i <= BALL_N; i++) {
        s_bb[i].vx = 0;
        s_bb[i].vy = 0;
        s_bb[i].act = 1U;
    }
    s_bb[0].x = 120;  s_bb[0].y = 150;
    s_bb[1].x = 92;   s_bb[1].y = 96;
    s_bb[2].x = 120;  s_bb[2].y = 84;
    s_bb[3].x = 148;  s_bb[3].y = 96;
}

static void cue_reset_pos(void)
{
    s_bb[0].x = 120;
    s_bb[0].y = 150;
    s_bb[0].vx = 0;
    s_bb[0].vy = 0;
}

static void new_game(void)
{
    balls_reset();
    s_ang     = 0;
    s_fric    = 0U;
    s_over    = 0U;
    s_score_v = 0U;
    s_last_cnt = encoder_count();
    lv_obj_add_flag(s_msg, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(s_score, "得分 0");
    game_lives_set(3);
    lv_obj_invalidate(s_cv);
}

static void move_all(void)
{
    /* 摩擦 */
    if (++s_fric >= FRIC_TICK) {
        s_fric = 0U;
        for (uint8_t i = 0; i <= BALL_N; i++) {
            s_bb[i].vx = decay(s_bb[i].vx);
            s_bb[i].vy = decay(s_bb[i].vy);
        }
    }

    for (uint8_t i = 0; i <= BALL_N; i++) {
        pball_t *b = &s_bb[i];
        if (b->act == 0U) continue;

        b->x = (int16_t)(b->x + b->vx);
        b->y = (int16_t)(b->y + b->vy);

        if (b->x < TBX0 + BALL_R) { b->x = (int16_t)(TBX0 + BALL_R); b->vx = (int8_t)-b->vx; }
        if (b->x > TBX1 - BALL_R) { b->x = (int16_t)(TBX1 - BALL_R); b->vx = (int8_t)-b->vx; }
        if (b->y < TBY0 + BALL_R) { b->y = (int16_t)(TBY0 + BALL_R); b->vy = (int8_t)-b->vy; }
        if (b->y > TBY1 - BALL_R) { b->y = (int16_t)(TBY1 - BALL_R); b->vy = (int8_t)-b->vy; }

        /* 落袋 */
        for (uint8_t p = 0; p < POCKET_N; p++) {
            int16_t dx = (int16_t)(b->x - POCKET_X[p]);
            int16_t dy = (int16_t)(b->y - POCKET_Y[p]);
            if (dx < 0) dx = (int16_t)-dx;
            if (dy < 0) dy = (int16_t)-dy;
            if (dx * dx + dy * dy < POCKET_R * POCKET_R) {
                b->act = 0U;
                b->vx = 0;
                b->vy = 0;
                if (i == 0U) {
                    cue_reset_pos();
                    s_bb[0].act = 1U;
                    if (game_lives_dec() == 0U) {
                        s_over = 1U;
                        lv_label_set_text(s_msg, "游戏结束");
                        lv_obj_clear_flag(s_msg, LV_OBJ_FLAG_HIDDEN);
                    }
                } else {
                    s_score_v = (uint16_t)(s_score_v + 10U);
                    lv_label_set_text_fmt(s_score, "得分 %u", (unsigned)s_score_v);
                }
                break;
            }
        }
    }

    /* 白球撞彩球：把速度传给彩球 */
    if (s_bb[0].act != 0U) {
        for (uint8_t i = 1; i <= BALL_N; i++) {
            int16_t dx, dy;
            if (s_bb[i].act == 0U) continue;
            dx = (int16_t)(s_bb[0].x - s_bb[i].x);
            if (dx < 0) dx = (int16_t)-dx;
            dy = (int16_t)(s_bb[0].y - s_bb[i].y);
            if (dy < 0) dy = (int16_t)-dy;
            if (dx < 2 * BALL_R && dy < 2 * BALL_R &&
                (s_bb[0].vx != 0 || s_bb[0].vy != 0)) {
                s_bb[i].vx = s_bb[0].vx;
                s_bb[i].vy = s_bb[0].vy;
                s_bb[0].vx = (int8_t)(s_bb[0].vx / 3);
                s_bb[0].vy = (int8_t)(s_bb[0].vy / 3);
            }
        }
    }

    /* 彩球清空：补球 */
    {
        uint8_t left = 0U;
        for (uint8_t i = 1; i <= BALL_N; i++) {
            if (s_bb[i].act != 0U) left++;
        }
        if (left == 0U) {
            s_bb[1].x = 92;  s_bb[1].y = 96;  s_bb[1].act = 1U;
            s_bb[2].x = 120; s_bb[2].y = 84;  s_bb[2].act = 1U;
            s_bb[3].x = 148; s_bb[3].y = 96;  s_bb[3].act = 1U;
        }
    }
}

static void pool_cb(lv_timer_t *t)
{
    uint8_t edge;
    int32_t d;
    (void)t;

    edge = keys_scan();
    if (s_over != 0U) {
        if ((edge & (KEY_MASK(KEY_ID_2) | KEY_MASK(KEY_ID_JS) |
                     KEY_MASK(KEY_ID_ENC))) != 0U) new_game();
        return;
    }

    /* 瞄准 */
    d = encoder_count() - s_last_cnt;
    s_last_cnt += d;
    while (d >= ENC_DETENT)  { d -= ENC_DETENT; s_ang = (int16_t)(s_ang + 4); }
    while (d <= -ENC_DETENT) { d += ENC_DETENT; s_ang = (int16_t)(s_ang - 4); }
    if ((joy_get_mask() & JOY_MASK_LEFT)  != 0U) s_ang = (int16_t)(s_ang - 3);
    if ((joy_get_mask() & JOY_MASK_RIGHT) != 0U) s_ang = (int16_t)(s_ang + 3);
    if ((edge & KEY_MASK(KEY_ID_4)) != 0U) s_ang = (int16_t)(s_ang + 6);
    if ((edge & KEY_MASK(KEY_ID_3)) != 0U) s_ang = (int16_t)(s_ang - 6);
    if (s_ang > ANG_MAX)   s_ang = ANG_MAX;
    if (s_ang < -ANG_MAX)  s_ang = -ANG_MAX;

    /* 击球 */
    if ((edge & (KEY_MASK(KEY_ID_2) | KEY_MASK(KEY_ID_JS) |
                 KEY_MASK(KEY_ID_ENC))) != 0U &&
        s_bb[0].vx == 0 && s_bb[0].vy == 0) {
        s_bb[0].vx = (int8_t)((int32_t)MAX_V * lv_trigo_sin((int16_t)s_ang) / LV_TRIGO_SIN_MAX);
        s_bb[0].vy = (int8_t)(-(int32_t)MAX_V * lv_trigo_cos((int16_t)s_ang) / LV_TRIGO_SIN_MAX);
    }

    move_all();
    lv_obj_invalidate(s_cv);
}

static void app_pool_enter(void)
{
    lv_obj_t *scr = lv_scr_act();

    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

    s_cv = lv_obj_create(scr);
    lv_obj_remove_style_all(s_cv);
    lv_obj_set_size(s_cv, 240, 240);
    lv_obj_set_pos(s_cv, 0, 0);
    lv_obj_clear_flag(s_cv, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s_cv, pool_draw, LV_EVENT_DRAW_MAIN, NULL);

    s_score = lv_label_create(scr);
    lv_obj_set_style_text_color(s_score, lv_color_white(), 0);
    lv_obj_align(s_score, LV_ALIGN_BOTTOM_MID, 0, -10);

    s_msg = lv_label_create(scr);
    lv_obj_set_style_text_color(s_msg, lv_color_white(), 0);
    lv_obj_set_style_bg_color(s_msg, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_msg, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(s_msg, 6, 0);
    lv_obj_align(s_msg, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(s_msg, LV_OBJ_FLAG_HIDDEN);

    new_game();
    s_tm = lv_timer_create(pool_cb, PERIOD, NULL);
}

static void app_pool_exit(void)
{
    if (s_tm != NULL) { lv_timer_del(s_tm); s_tm = NULL; }
    game_lives_clear();
}

const app_t app_pool = { "台球", app_pool_enter, app_pool_exit };
