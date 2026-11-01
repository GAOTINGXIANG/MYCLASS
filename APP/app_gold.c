/**
 * @file app_gold.c
 * @brief 黄金矿工：矿钩左右摆动，按 KEY2 下钩抓金块，限时挖完再刷新一轮。
 *   KEY2 / JS / ENC  = 下钩（结束后重开）
 *   KEY1             = 返回上一级
 */
#include "app_gold.h"
#include "lvgl.h"
#include "keys.h"
#include "joystick.h"

#define PERIOD      30
#define PIVOT_X     120
#define PIVOT_Y     58
#define LMAX        100
#define GOLD_N      8
#define ANG_MAX     72
#define ANG_STEP    2
#define EXT_SPD     4
#define RET_SPD     4
#define TIME_TICK   2400        /* 约 72 秒 */
#define HIT_R       5
#define FX0         35
#define FY0         35
#define FW          170

typedef struct {
    int16_t x, y;
    uint8_t r;
    uint8_t act;
} gold_t;

static lv_timer_t *s_tm;
static lv_obj_t   *s_cv, *s_score, *s_msg;

static gold_t  s_g[GOLD_N];
static int16_t s_ang;
static int8_t  s_dir;
static int16_t s_len;
static uint8_t s_state;        /* 0=摆动 1=下钩 2=收回 */
static uint8_t s_over;
static uint16_t s_score_v;
static uint16_t s_time;
static uint16_t s_last_sec;
static uint32_t s_rng = 0x5A5A1234UL;

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

static void line(lv_draw_ctx_t *ctx, int16_t x1, int16_t y1, int16_t x2, int16_t y2)
{
    lv_draw_line_dsc_t d;
    lv_point_t p1, p2;

    lv_draw_line_dsc_init(&d);
    d.color   = lv_color_white();
    d.width   = 2;
    d.opa     = LV_OPA_COVER;
    p1.x = x1; p1.y = y1;
    p2.x = x2; p2.y = y2;
    lv_draw_line(ctx, &d, &p1, &p2);
}

static void tip_pos(int16_t *tx, int16_t *ty)
{
    *tx = (int16_t)(PIVOT_X + (int16_t)((int32_t)s_len *
                lv_trigo_sin((int16_t)s_ang) / LV_TRIGO_SIN_MAX));
    *ty = (int16_t)(PIVOT_Y + (int16_t)((int32_t)s_len *
                lv_trigo_cos((int16_t)s_ang) / LV_TRIGO_SIN_MAX));
}

static void gold_draw(lv_event_t *e)
{
    lv_draw_ctx_t *ctx = lv_event_get_draw_ctx(e);
    int16_t tx, ty;

    for (uint8_t i = 0; i < GOLD_N; i++) {
        if (s_g[i].act == 0U) continue;
        ball(ctx, s_g[i].x, s_g[i].y, s_g[i].r,
             lv_palette_main(LV_PALETTE_YELLOW));
        ball(ctx, (int16_t)(s_g[i].x - 2), (int16_t)(s_g[i].y - 2), 2,
             lv_palette_main(LV_PALETTE_ORANGE));
    }

    tip_pos(&tx, &ty);
    line(ctx, PIVOT_X, PIVOT_Y, tx, ty);
    ball(ctx, tx, ty, 3, lv_palette_main(LV_PALETTE_GREY));

    /* 卷扬机 */
    box(ctx, (int16_t)(PIVOT_X - 10), (int16_t)(PIVOT_Y - 8), 20, 10,
        lv_palette_main(LV_PALETTE_BLUE_GREY), 3);
}

/* ---------------- 逻辑 ---------------- */
static void gold_place(void)
{
    for (uint8_t i = 0; i < GOLD_N; i++) {
        s_g[i].r   = (uint8_t)(6U + rnd() % 5U);
        s_g[i].x   = (int16_t)(FX0 + s_g[i].r + 4 +
                               (int16_t)(rnd() % (uint32_t)(FW - 2 * s_g[i].r - 8)));
        s_g[i].y   = (int16_t)(FY0 + 92 + (int16_t)(rnd() % 44U));
        s_g[i].act = 1U;
    }
}

static uint8_t gold_left(void)
{
    uint8_t n = 0U;
    for (uint8_t i = 0; i < GOLD_N; i++) {
        if (s_g[i].act != 0U) n++;
    }
    return n;
}

static void new_game(void)
{
    gold_place();
    s_ang      = 0;
    s_dir      = 1;
    s_len      = 0;
    s_state    = 0U;
    s_over     = 0U;
    s_score_v  = 0U;
    s_time     = TIME_TICK;
    s_last_sec = 0xFFFFU;
    lv_obj_add_flag(s_msg, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(s_score, "得分 0  时间 72");
    lv_obj_invalidate(s_cv);
}

static void gold_cb(lv_timer_t *t)
{
    uint8_t edge;
    (void)t;

    edge = keys_scan();
    if (s_over != 0U) {
        if ((edge & (KEY_MASK(KEY_ID_2) | KEY_MASK(KEY_ID_JS) |
                     KEY_MASK(KEY_ID_ENC))) != 0U) new_game();
        return;
    }

    /* 倒计时 */
    if (s_time > 0U) s_time--;
    if (s_time == 0U) {
        s_over = 1U;
        lv_label_set_text(s_msg, "游戏结束");
        lv_obj_clear_flag(s_msg, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    if (s_state == 0U) {
        s_ang = (int16_t)(s_ang + s_dir * ANG_STEP);
        if (s_ang > ANG_MAX)   { s_ang = ANG_MAX;  s_dir = -1; }
        if (s_ang < -ANG_MAX)  { s_ang = -ANG_MAX; s_dir = 1;  }

        if ((edge & (KEY_MASK(KEY_ID_2) | KEY_MASK(KEY_ID_JS) |
                     KEY_MASK(KEY_ID_ENC))) != 0U) s_state = 1U;
    } else if (s_state == 1U) {
        int16_t tx, ty;

        s_len = (int16_t)(s_len + EXT_SPD);
        tip_pos(&tx, &ty);

        for (uint8_t i = 0; i < GOLD_N; i++) {
            int16_t dx, dy;
            if (s_g[i].act == 0U) continue;
            dx = (int16_t)(tx - s_g[i].x);
            if (dx < 0) dx = (int16_t)-dx;
            dy = (int16_t)(ty - s_g[i].y);
            if (dy < 0) dy = (int16_t)-dy;
            if (dx < s_g[i].r + HIT_R && dy < s_g[i].r + HIT_R) {
                s_g[i].act = 0U;
                s_score_v = (uint16_t)(s_score_v + s_g[i].r);
                s_state = 2U;
                break;
            }
        }
        if (s_state == 1U && s_len >= LMAX) s_state = 2U;
    } else {
        s_len = (int16_t)(s_len - RET_SPD);
        if (s_len <= 0) { s_len = 0; s_state = 0U; }
    }

    /* 本轮到齐：刷新一轮 */
    if (gold_left() == 0U) {
        gold_place();
        s_time = (uint16_t)(s_time + 600U);
    }

    /* 每 33 拍刷新一次分数/时间文本 */
    {
        uint16_t sec = (uint16_t)((s_time * 30U) / 1000U);
        if (sec != s_last_sec) {
            s_last_sec = sec;
            lv_label_set_text_fmt(s_score, "得分 %u  时间 %u",
                                  (unsigned)s_score_v, (unsigned)sec);
        }
    }

    lv_obj_invalidate(s_cv);
}

static void app_gold_enter(void)
{
    lv_obj_t *scr = lv_scr_act();

    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

    s_cv = lv_obj_create(scr);
    lv_obj_remove_style_all(s_cv);
    lv_obj_set_size(s_cv, 240, 240);
    lv_obj_set_pos(s_cv, 0, 0);
    lv_obj_clear_flag(s_cv, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s_cv, gold_draw, LV_EVENT_DRAW_MAIN, NULL);

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
    s_tm = lv_timer_create(gold_cb, PERIOD, NULL);
}

static void app_gold_exit(void)
{
    if (s_tm != NULL) { lv_timer_del(s_tm); s_tm = NULL; }
}

const app_t app_gold = { "黄金矿工", app_gold_enter, app_gold_exit };
