/**
 * @file app_ski.c
 * @brief 滑雪大冒险（简化）：雪道从上往下滚，左右躲避树木，越跑越快。
 *   摇杆左右 / 编码器 = 左右移动
 *   KEY2 / JS / ENC  = 结束后重开
 *   KEY1             = 返回上一级
 *   3 个 LED 显示生命，撞树扣 1 命。
 */
#include "app_ski.h"
#include "lvgl.h"
#include "game_common.h"
#include "keys.h"
#include "encoder.h"
#include "joystick.h"

#define PERIOD      30
#define FX0         35
#define FY0         35
#define FW          170
#define PY          168
#define SKI_W       12
#define SKI_H       14
#define TREE_MAX    5
#define ENC_DETENT  4
#define SPEED_P     4
#define INVULN_TICK 20
#define SPAWN_TICK  30

typedef struct {
    int16_t x, y;
    uint8_t act;
} tree_t;

static lv_timer_t *s_tm;
static lv_obj_t   *s_cv, *s_score, *s_msg;

static int16_t s_px = 120;
static tree_t  s_tr[TREE_MAX];
static uint8_t s_over;
static uint32_t s_score_v;
static uint8_t s_speed;
static uint8_t s_spawn;
static uint8_t s_invuln;
static uint16_t s_tick;
static int32_t s_last_cnt;
static uint32_t s_rng = 0x13572468UL;

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

static void ski_draw(lv_event_t *e)
{
    lv_draw_ctx_t *ctx = lv_event_get_draw_ctx(e);

    for (uint8_t i = 0; i < TREE_MAX; i++) {
        if (s_tr[i].act == 0U) continue;
        box(ctx, (int16_t)(s_tr[i].x - 2), (int16_t)(s_tr[i].y + 4), 4, 8,
            lv_palette_main(LV_PALETTE_BROWN), 1);
        ball(ctx, s_tr[i].x, s_tr[i].y, 8, lv_palette_main(LV_PALETTE_GREEN));
    }

    if (s_invuln != 0U && ((s_invuln & 4U) != 0U)) {
        box(ctx, (int16_t)(s_px - SKI_W / 2), (int16_t)(PY - SKI_H / 2), SKI_W, SKI_H,
            lv_palette_main(LV_PALETTE_BLUE_GREY), 3);
    } else {
        box(ctx, (int16_t)(s_px - 4), (int16_t)(PY - 2), 8, SKI_H,
            lv_palette_main(LV_PALETTE_CYAN), 3);
        ball(ctx, s_px, (int16_t)(PY - 9), 4, lv_palette_main(LV_PALETTE_ORANGE));
        box(ctx, (int16_t)(s_px - 8), (int16_t)(PY + 6), 16, 2, lv_color_white(), 1);
    }
}

/* ---------------- 逻辑 ---------------- */
static void spawn_tree(void)
{
    for (uint8_t i = 0; i < TREE_MAX; i++) {
        if (s_tr[i].act != 0U) continue;
        s_tr[i].x   = (int16_t)(FX0 + 12 + (int16_t)(rnd() % (uint32_t)(FW - 24)));
        s_tr[i].y   = (int16_t)(FY0 + 2);
        s_tr[i].act = 1U;
        return;
    }
}

static void new_game(void)
{
    for (uint8_t i = 0; i < TREE_MAX; i++) s_tr[i].act = 0U;

    s_px       = 120;
    s_over     = 0U;
    s_score_v  = 0U;
    s_speed    = 2U;
    s_spawn    = 0U;
    s_invuln   = 0U;
    s_tick     = 0U;
    s_last_cnt = encoder_count();
    lv_obj_add_flag(s_msg, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(s_score, "得分 0");
    game_lives_set(3);
    lv_obj_invalidate(s_cv);
}

static void ski_cb(lv_timer_t *t)
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

    /* 移动 */
    d = encoder_count() - s_last_cnt;
    s_last_cnt += d;
    while (d >= ENC_DETENT)  { d -= ENC_DETENT; s_px += SPEED_P; }
    while (d <= -ENC_DETENT) { d += ENC_DETENT; s_px -= SPEED_P; }
    if ((joy_get_mask() & JOY_MASK_LEFT)  != 0U) s_px -= SPEED_P;
    if ((joy_get_mask() & JOY_MASK_RIGHT) != 0U) s_px += SPEED_P;
    if (s_px < FX0 + SKI_W / 2)        s_px = (int16_t)(FX0 + SKI_W / 2);
    if (s_px > FX0 + FW - SKI_W / 2)   s_px = (int16_t)(FX0 + FW - SKI_W / 2);

    /* 速度随距离提升 */
    s_tick++;
    if (s_tick >= 600U) { s_tick = 0U; if (s_speed < 4U) s_speed++; }
    s_score_v = (uint32_t)(s_score_v + (uint32_t)s_speed);
    lv_label_set_text_fmt(s_score, "得分 %u", (unsigned)(s_score_v / 20U));

    /* 生成 */
    if (++s_spawn >= SPAWN_TICK) { s_spawn = 0U; spawn_tree(); }

    /* 树木滚动 + 碰撞 */
    for (uint8_t i = 0; i < TREE_MAX; i++) {
        int16_t dx, dy;
        if (s_tr[i].act == 0U) continue;
        s_tr[i].y = (int16_t)(s_tr[i].y + s_speed);
        if (s_tr[i].y > FY0 + FW + 10) { s_tr[i].act = 0U; continue; }

        if (s_invuln != 0U) continue;
        dx = (int16_t)(s_tr[i].x - s_px);
        if (dx < 0) dx = (int16_t)-dx;
        dy = (int16_t)(s_tr[i].y - PY);
        if (dy < 0) dy = (int16_t)-dy;
        if (dx < 13 && dy < 12) {
            s_tr[i].act = 0U;
            s_invuln = INVULN_TICK;
            if (game_lives_dec() == 0U) {
                s_over = 1U;
                lv_label_set_text(s_msg, "游戏结束");
                lv_obj_clear_flag(s_msg, LV_OBJ_FLAG_HIDDEN);
            }
        }
    }

    if (s_invuln > 0U) s_invuln--;

    lv_obj_invalidate(s_cv);
}

static void app_ski_enter(void)
{
    lv_obj_t *scr = lv_scr_act();

    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

    s_cv = lv_obj_create(scr);
    lv_obj_remove_style_all(s_cv);
    lv_obj_set_size(s_cv, 240, 240);
    lv_obj_set_pos(s_cv, 0, 0);
    lv_obj_clear_flag(s_cv, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s_cv, ski_draw, LV_EVENT_DRAW_MAIN, NULL);

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
    s_tm = lv_timer_create(ski_cb, PERIOD, NULL);
}

static void app_ski_exit(void)
{
    if (s_tm != NULL) { lv_timer_del(s_tm); s_tm = NULL; }
    game_lives_clear();
}

const app_t app_ski = { "滑雪冒险", app_ski_enter, app_ski_exit };
