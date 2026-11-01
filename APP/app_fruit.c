/**
 * @file app_fruit.c
 * @brief 切水果：左右移动刀锋，按 KEY2 挥刀切开飞起的水果，切到炸弹扣命。
 *   摇杆左右 / 编码器 = 移动刀锋
 *   KEY2 / JS / ENC  = 挥刀（结束后重开）
 *   KEY1             = 返回上一级
 *   3 个 LED 显示生命，只有炸弹扣命。
 */
#include "app_fruit.h"
#include "lvgl.h"
#include "game_common.h"
#include "keys.h"
#include "encoder.h"
#include "joystick.h"

#define PERIOD     30
#define FX0        35
#define FY0        35
#define FW         170
#define FRUIT_MAX  6
#define SPAWN_TICK 34
#define GRAV       1
#define CUT_HALF   20          /* 挥刀横向判定半宽 */
#define ENC_DETENT 4
#define SPEED_P    4
#define FLASH_TICK 5

typedef struct {
    int16_t x, y;
    int16_t vx, vy;
    uint8_t bomb;
    uint8_t act;
} fruit_t;

static lv_timer_t *s_tm;
static lv_obj_t   *s_cv, *s_score, *s_msg;

static fruit_t  s_f[FRUIT_MAX];
static int16_t  s_bx;
static uint8_t  s_over;
static uint16_t s_score_v;
static uint8_t  s_spawn;
static uint8_t  s_flash;
static int32_t  s_last_cnt;
static uint32_t s_rng = 0x7A32BEEFUL;

static uint32_t rnd(void)
{
    s_rng = s_rng * 1103515245UL + 12345UL;
    return (s_rng >> 16) & 0x7FFFUL;
}

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

static void fruit_draw(lv_event_t *e)
{
    lv_draw_ctx_t *ctx = lv_event_get_draw_ctx(e);

    for (uint8_t i = 0; i < FRUIT_MAX; i++) {
        if (s_f[i].act == 0U) continue;
        if (s_f[i].bomb != 0U) {
            ball(ctx, s_f[i].x, s_f[i].y, 8, lv_palette_main(LV_PALETTE_RED));
            ball(ctx, s_f[i].x, s_f[i].y, 4, lv_color_black());
        } else {
            ball(ctx, s_f[i].x, s_f[i].y, 8,
                 (i & 1U) ? lv_palette_main(LV_PALETTE_GREEN)
                          : lv_palette_main(LV_PALETTE_ORANGE));
        }
    }

    /* 刀锋 */
    box(ctx, (int16_t)(s_bx - 1), 52, 2, 152,
        (s_flash != 0U) ? lv_palette_main(LV_PALETTE_RED) : lv_color_white(), 0);
}

/* ---------------- 逻辑 ---------------- */
static void spawn_fruit(void)
{
    for (uint8_t i = 0; i < FRUIT_MAX; i++) {
        if (s_f[i].act != 0U) continue;

        s_f[i].bomb = ((rnd() % 100U) < 22U) ? 1U : 0U;
        s_f[i].x    = (int16_t)(FX0 + 16 + (int16_t)(rnd() % (uint32_t)(FW - 32)));
        s_f[i].y    = (int16_t)(FY0 + FW - 4);
        s_f[i].vy   = (int16_t)(-(int16_t)(11 + rnd() % 4));
        s_f[i].vx   = (int16_t)((int16_t)(rnd() % 7) - 3);
        s_f[i].act  = 1U;
        return;
    }
}

static void new_game(void)
{
    for (uint8_t i = 0; i < FRUIT_MAX; i++) s_f[i].act = 0U;

    s_bx      = 120;
    s_over    = 0U;
    s_score_v = 0U;
    s_spawn   = 0U;
    s_flash   = 0U;
    s_last_cnt = encoder_count();
    lv_obj_add_flag(s_msg, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(s_score, "得分 0");
    game_lives_set(3);
    lv_obj_invalidate(s_cv);
}

static void fruit_cb(lv_timer_t *t)
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

    /* 移动刀锋 */
    d = encoder_count() - s_last_cnt;
    s_last_cnt += d;
    while (d >= ENC_DETENT)  { d -= ENC_DETENT; s_bx += SPEED_P; }
    while (d <= -ENC_DETENT) { d += ENC_DETENT; s_bx -= SPEED_P; }
    if ((joy_get_mask() & JOY_MASK_LEFT)  != 0U) s_bx -= SPEED_P;
    if ((joy_get_mask() & JOY_MASK_RIGHT) != 0U) s_bx += SPEED_P;
    if (s_bx < FX0 + 2)      s_bx = (int16_t)(FX0 + 2);
    if (s_bx > FX0 + FW - 2) s_bx = (int16_t)(FX0 + FW - 2);

    if (s_flash > 0U) s_flash--;

    /* 挥刀 */
    if ((edge & (KEY_MASK(KEY_ID_2) | KEY_MASK(KEY_ID_JS) |
                 KEY_MASK(KEY_ID_ENC))) != 0U) {
        s_flash = FLASH_TICK;
        for (uint8_t i = 0; i < FRUIT_MAX; i++) {
            int16_t dx;
            if (s_f[i].act == 0U) continue;
            dx = (int16_t)(s_f[i].x - s_bx);
            if (dx < 0) dx = (int16_t)-dx;
            if (dx > CUT_HALF) continue;

            s_f[i].act = 0U;
            if (s_f[i].bomb != 0U) {
                if (game_lives_dec() == 0U) {
                    s_over = 1U;
                    lv_label_set_text(s_msg, "游戏结束");
                    lv_obj_clear_flag(s_msg, LV_OBJ_FLAG_HIDDEN);
                }
            } else {
                s_score_v = (uint16_t)(s_score_v + 10U);
                lv_label_set_text_fmt(s_score, "得分 %u", (unsigned)s_score_v);
            }
        }
    }

    /* 生成 */
    if (++s_spawn >= SPAWN_TICK) { s_spawn = 0U; spawn_fruit(); }

    /* 飞行 */
    for (uint8_t i = 0; i < FRUIT_MAX; i++) {
        if (s_f[i].act == 0U) continue;
        s_f[i].vy = (int16_t)(s_f[i].vy + GRAV);
        s_f[i].x  = (int16_t)(s_f[i].x + s_f[i].vx);
        s_f[i].y  = (int16_t)(s_f[i].y + s_f[i].vy);
        if (s_f[i].x < FX0 + 8)         { s_f[i].x = (int16_t)(FX0 + 8);         s_f[i].vx = (int16_t)-s_f[i].vx; }
        if (s_f[i].x > FX0 + FW - 8)    { s_f[i].x = (int16_t)(FX0 + FW - 8);    s_f[i].vx = (int16_t)-s_f[i].vx; }
        if (s_f[i].y > FY0 + FW + 10)   { s_f[i].act = 0U; }
    }

    lv_obj_invalidate(s_cv);
}

static void app_fruit_enter(void)
{
    lv_obj_t *scr = lv_scr_act();

    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

    s_cv = lv_obj_create(scr);
    lv_obj_remove_style_all(s_cv);
    lv_obj_set_size(s_cv, 240, 240);
    lv_obj_set_pos(s_cv, 0, 0);
    lv_obj_clear_flag(s_cv, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s_cv, fruit_draw, LV_EVENT_DRAW_MAIN, NULL);

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
    s_tm = lv_timer_create(fruit_cb, PERIOD, NULL);
}

static void app_fruit_exit(void)
{
    if (s_tm != NULL) { lv_timer_del(s_tm); s_tm = NULL; }
    game_lives_clear();
}

const app_t app_fruit = { "切水果", app_fruit_enter, app_fruit_exit };
