/**
 * @file app_sniper.c
 * @brief 狙击外星人（简化）：移动准星，按 KEY2 开枪，打中加分，外星人跑掉扣命。
 *   摇杆（四方向） / 编码器 = 移动准星
 *   KEY2 / JS / ENC  = 开枪（结束后重开）
 *   KEY1             = 返回上一级
 *   3 个 LED 显示生命。
 */
#include "app_sniper.h"
#include "lvgl.h"
#include "game_common.h"
#include "keys.h"
#include "encoder.h"
#include "joystick.h"

#define PERIOD      30
#define FX0         35
#define FY0         35
#define FW          170
#define STEP        4
#define ENC_DETENT  4
#define LIFE_TICK   60          /* 外星人停留时间 */
#define HIT_R       14
#define FLASH_TICK  5

static lv_timer_t *s_tm;
static lv_obj_t   *s_cv, *s_score, *s_msg;

static int16_t  s_cx, s_cy;
static int16_t  s_ax, s_ay;
static uint8_t  s_alive;
static uint8_t  s_life;
static uint8_t  s_over;
static uint16_t s_score_v;
static uint8_t  s_flash;
static int32_t  s_last_cnt;
static uint32_t s_rng = 0x24681357UL;

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

static void ring(lv_draw_ctx_t *ctx, int16_t cx, int16_t cy, int16_t r, lv_color_t c)
{
    lv_draw_rect_dsc_t d;
    lv_area_t a;

    lv_draw_rect_dsc_init(&d);
    d.bg_opa = LV_OPA_TRANSP;
    d.border_width = 1;
    d.border_opa = LV_OPA_COVER;
    d.border_color = c;
    d.radius = LV_RADIUS_CIRCLE;
    a.x1 = (lv_coord_t)(cx - r);
    a.y1 = (lv_coord_t)(cy - r);
    a.x2 = (lv_coord_t)(cx + r);
    a.y2 = (lv_coord_t)(cy + r);
    lv_draw_rect(ctx, &d, &a);
}

static void sniper_draw(lv_event_t *e)
{
    lv_draw_ctx_t *ctx = lv_event_get_draw_ctx(e);

    if (s_alive != 0U) {
        ball(ctx, s_ax, s_ay, 11, lv_palette_main(LV_PALETTE_GREEN));
        ball(ctx, (int16_t)(s_ax - 4), (int16_t)(s_ay - 3), 3, lv_color_black());
        ball(ctx, (int16_t)(s_ax + 4), (int16_t)(s_ay - 3), 3, lv_color_black());
        box(ctx, (int16_t)(s_ax - 1), (int16_t)(s_ay + 6), 2, 3,
            lv_palette_main(LV_PALETTE_LIME), 0);
    }

    /* 准星 */
    ring(ctx, s_cx, s_cy, 12,
         (s_flash != 0U) ? lv_palette_main(LV_PALETTE_RED) : lv_color_white());
    box(ctx, (int16_t)(s_cx - 2), s_cy, 5, 1, lv_color_white(), 0);
    box(ctx, s_cx, (int16_t)(s_cy - 2), 1, 5, lv_color_white(), 0);
}

/* ---------------- 逻辑 ---------------- */
static void alien_spawn(void)
{
    s_ax    = (int16_t)(FX0 + 24 + (int16_t)(rnd() % (uint32_t)(FW - 48)));
    s_ay    = (int16_t)(FY0 + 24 + (int16_t)(rnd() % (uint32_t)(FW - 48)));
    s_life  = LIFE_TICK;
    s_alive = 1U;
}

static void new_game(void)
{
    s_cx      = 120;
    s_cy      = 120;
    s_alive   = 0U;
    s_life    = 0U;
    s_over    = 0U;
    s_score_v = 0U;
    s_flash   = 0U;
    s_last_cnt = encoder_count();
    lv_obj_add_flag(s_msg, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(s_score, "得分 0");
    game_lives_set(3);
    alien_spawn();
    lv_obj_invalidate(s_cv);
}

static void sniper_cb(lv_timer_t *t)
{
    uint8_t edge;
    joy_mask_t m;
    int32_t d;
    (void)t;

    edge = keys_scan();
    if (s_over != 0U) {
        if ((edge & (KEY_MASK(KEY_ID_2) | KEY_MASK(KEY_ID_JS) |
                     KEY_MASK(KEY_ID_ENC))) != 0U) new_game();
        return;
    }

    /* 移动准星 */
    m = joy_get_mask();
    if ((m & JOY_MASK_LEFT)  != 0U) s_cx = (int16_t)(s_cx - STEP);
    if ((m & JOY_MASK_RIGHT) != 0U) s_cx = (int16_t)(s_cx + STEP);
    if ((m & JOY_MASK_UP)    != 0U) s_cy = (int16_t)(s_cy - STEP);
    if ((m & JOY_MASK_DOWN)  != 0U) s_cy = (int16_t)(s_cy + STEP);

    d = encoder_count() - s_last_cnt;
    s_last_cnt += d;
    while (d >= ENC_DETENT)  { d -= ENC_DETENT; s_cx = (int16_t)(s_cx + STEP); }
    while (d <= -ENC_DETENT) { d += ENC_DETENT; s_cx = (int16_t)(s_cx - STEP); }
    if ((edge & KEY_MASK(KEY_ID_4)) != 0U) s_cy = (int16_t)(s_cy + STEP);
    if ((edge & KEY_MASK(KEY_ID_3)) != 0U) s_cy = (int16_t)(s_cy - STEP);

    if (s_cx < FX0 + 12)      s_cx = (int16_t)(FX0 + 12);
    if (s_cx > FX0 + FW - 12) s_cx = (int16_t)(FX0 + FW - 12);
    if (s_cy < FY0 + 12)      s_cy = (int16_t)(FY0 + 12);
    if (s_cy > FY0 + FW - 12) s_cy = (int16_t)(FY0 + FW - 12);

    if (s_flash > 0U) s_flash--;

    /* 开枪 */
    if ((edge & (KEY_MASK(KEY_ID_2) | KEY_MASK(KEY_ID_JS) |
                 KEY_MASK(KEY_ID_ENC))) != 0U) {
        int16_t dx, dy;

        s_flash = FLASH_TICK;
        dx = (int16_t)(s_ax - s_cx);
        if (dx < 0) dx = (int16_t)-dx;
        dy = (int16_t)(s_ay - s_cy);
        if (dy < 0) dy = (int16_t)-dy;
        if (s_alive != 0U && dx < HIT_R && dy < HIT_R) {
            s_alive = 0U;
            s_score_v = (uint16_t)(s_score_v + 10U);
            lv_label_set_text_fmt(s_score, "得分 %u", (unsigned)s_score_v);
            alien_spawn();
        }
    }

    /* 外星人停留计时 */
    if (s_alive != 0U) {
        if (--s_life == 0U) {
            s_alive = 0U;
            if (game_lives_dec() == 0U) {
                s_over = 1U;
                lv_label_set_text(s_msg, "游戏结束");
                lv_obj_clear_flag(s_msg, LV_OBJ_FLAG_HIDDEN);
                return;
            }
            alien_spawn();
        }
    } else {
        alien_spawn();
    }

    lv_obj_invalidate(s_cv);
}

static void app_sniper_enter(void)
{
    lv_obj_t *scr = lv_scr_act();

    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

    s_cv = lv_obj_create(scr);
    lv_obj_remove_style_all(s_cv);
    lv_obj_set_size(s_cv, 240, 240);
    lv_obj_set_pos(s_cv, 0, 0);
    lv_obj_clear_flag(s_cv, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s_cv, sniper_draw, LV_EVENT_DRAW_MAIN, NULL);

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
    s_tm = lv_timer_create(sniper_cb, PERIOD, NULL);
}

static void app_sniper_exit(void)
{
    if (s_tm != NULL) { lv_timer_del(s_tm); s_tm = NULL; }
    game_lives_clear();
}

const app_t app_sniper = { "狙击外星", app_sniper_enter, app_sniper_exit };
