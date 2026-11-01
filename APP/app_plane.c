/**
 * @file app_plane.c
 * @brief 飞行射击：底部战机左右移动、自动开火，击落俯冲的敌机。
 *   摇杆左右 / 编码器 = 移动
 *   KEY2 / JS / ENC  = 结束后重开
 *   KEY1             = 返回上一级
 *   3 个 LED 显示生命，撞到敌机扣 1 命。
 */
#include "app_plane.h"
#include "lvgl.h"
#include "game_common.h"
#include "keys.h"
#include "encoder.h"
#include "joystick.h"

#define PERIOD      30
#define FX0         35
#define FY0         35
#define FW          170
#define PY          176
#define PLANE_W     20
#define PLANE_H     12
#define ENEMY_MAX   4
#define BUL_MAX     5
#define BUL_SPEED   6
#define ENC_DETENT  4
#define SPEED_P     4
#define FIRE_TICK   7
#define SPAWN_TICK  40
#define INVULN_TICK 24

typedef struct {
    int16_t x, y;
    uint8_t act;
} bullet_t;

typedef struct {
    int16_t x, y;
    int16_t vy;
    uint8_t act;
} enemy_t;

static lv_timer_t *s_tm;
static lv_obj_t   *s_cv, *s_score, *s_msg;

static int16_t  s_px = 120;
static bullet_t s_bul[BUL_MAX];
static enemy_t  s_en[ENEMY_MAX];
static uint8_t  s_over;
static uint16_t s_score_v;
static uint8_t  s_fire_acc;
static uint8_t  s_spawn;
static uint8_t  s_invuln;
static int32_t  s_last_cnt;
static uint32_t s_rng = 0x0BADC0DEUL;

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

static void plane_draw(lv_event_t *e)
{
    lv_draw_ctx_t *ctx = lv_event_get_draw_ctx(e);

    for (uint8_t i = 0; i < ENEMY_MAX; i++) {
        if (s_en[i].act == 0U) continue;
        box(ctx, (int16_t)(s_en[i].x - PLANE_W / 2), (int16_t)(s_en[i].y - 5),
            PLANE_W, 10, lv_palette_main(LV_PALETTE_RED), 3);
        box(ctx, (int16_t)(s_en[i].x - 4), (int16_t)(s_en[i].y + 4), 8, 6,
            lv_palette_main(LV_PALETTE_ORANGE), 2);
    }

    for (uint8_t i = 0; i < BUL_MAX; i++) {
        if (s_bul[i].act == 0U) continue;
        box(ctx, (int16_t)(s_bul[i].x - 1), (int16_t)(s_bul[i].y - 5), 3, 7,
            lv_palette_main(LV_PALETTE_YELLOW), 1);
    }

    if (s_invuln != 0U && ((s_invuln & 4U) != 0U)) {
        box(ctx, (int16_t)(s_px - PLANE_W / 2), (int16_t)(PY - 3), PLANE_W, 7,
            lv_palette_main(LV_PALETTE_BLUE_GREY), 3);
    } else {
        box(ctx, (int16_t)(s_px - PLANE_W / 2), (int16_t)(PY - 3), PLANE_W, 7,
            lv_palette_main(LV_PALETTE_CYAN), 3);
        box(ctx, (int16_t)(s_px - 3), (int16_t)(PY - 9), 6, 8,
            lv_palette_main(LV_PALETTE_CYAN), 3);
    }
}

/* ---------------- 逻辑 ---------------- */
static void spawn_enemy(void)
{
    for (uint8_t i = 0; i < ENEMY_MAX; i++) {
        if (s_en[i].act != 0U) continue;
        s_en[i].x   = (int16_t)(FX0 + 14 + (int16_t)(rnd() % (uint32_t)(FW - 28)));
        s_en[i].y   = (int16_t)(FY0 + 2);
        s_en[i].vy  = (int16_t)(2 + rnd() % 2U);
        s_en[i].act = 1U;
        return;
    }
}

static void new_game(void)
{
    for (uint8_t i = 0; i < BUL_MAX; i++) s_bul[i].act = 0U;
    for (uint8_t i = 0; i < ENEMY_MAX; i++) s_en[i].act = 0U;

    s_px       = 120;
    s_over     = 0U;
    s_score_v  = 0U;
    s_fire_acc = 0U;
    s_spawn    = 0U;
    s_invuln   = 0U;
    s_last_cnt = encoder_count();
    lv_obj_add_flag(s_msg, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(s_score, "得分 0");
    game_lives_set(3);
    lv_obj_invalidate(s_cv);
}

static void plane_cb(lv_timer_t *t)
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
    if (s_px < FX0 + PLANE_W / 2)        s_px = (int16_t)(FX0 + PLANE_W / 2);
    if (s_px > FX0 + FW - PLANE_W / 2)   s_px = (int16_t)(FX0 + FW - PLANE_W / 2);

    /* 自动开火 */
    if (++s_fire_acc >= FIRE_TICK) {
        s_fire_acc = 0U;
        for (uint8_t i = 0; i < BUL_MAX; i++) {
            if (s_bul[i].act != 0U) continue;
            s_bul[i].x   = s_px;
            s_bul[i].y   = (int16_t)(PY - 12);
            s_bul[i].act = 1U;
            break;
        }
    }

    /* 子弹 */
    for (uint8_t i = 0; i < BUL_MAX; i++) {
        if (s_bul[i].act == 0U) continue;
        s_bul[i].y = (int16_t)(s_bul[i].y - BUL_SPEED);
        if (s_bul[i].y < FY0) { s_bul[i].act = 0U; continue; }

        for (uint8_t k = 0; k < ENEMY_MAX; k++) {
            int16_t dx, dy;
            if (s_en[k].act == 0U) continue;
            dx = (int16_t)(s_bul[i].x - s_en[k].x);
            if (dx < 0) dx = (int16_t)-dx;
            dy = (int16_t)(s_bul[i].y - s_en[k].y);
            if (dy < 0) dy = (int16_t)-dy;
            if (dx <= PLANE_W / 2 + 1 && dy <= 8) {
                s_bul[i].act = 0U;
                s_en[k].act  = 0U;
                s_score_v = (uint16_t)(s_score_v + 10U);
                lv_label_set_text_fmt(s_score, "得分 %u", (unsigned)s_score_v);
                break;
            }
        }
    }

    /* 敌机 */
    if (++s_spawn >= SPAWN_TICK) { s_spawn = 0U; spawn_enemy(); }

    for (uint8_t i = 0; i < ENEMY_MAX; i++) {
        if (s_en[i].act == 0U) continue;
        s_en[i].y = (int16_t)(s_en[i].y + s_en[i].vy);
        if (s_en[i].y > FY0 + FW) { s_en[i].act = 0U; continue; }

        if (s_invuln == 0U) {
            int16_t dx, dy;
            dx = (int16_t)(s_en[i].x - s_px);
            if (dx < 0) dx = (int16_t)-dx;
            dy = (int16_t)(s_en[i].y - PY);
            if (dy < 0) dy = (int16_t)-dy;
            if (dx < PLANE_W / 2 + 2 && dy < 8) {
                s_en[i].act = 0U;
                s_invuln = INVULN_TICK;
                if (game_lives_dec() == 0U) {
                    s_over = 1U;
                    lv_label_set_text(s_msg, "游戏结束");
                    lv_obj_clear_flag(s_msg, LV_OBJ_FLAG_HIDDEN);
                }
            }
        }
    }

    if (s_invuln > 0U) s_invuln--;

    lv_obj_invalidate(s_cv);
}

static void app_plane_enter(void)
{
    lv_obj_t *scr = lv_scr_act();

    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

    s_cv = lv_obj_create(scr);
    lv_obj_remove_style_all(s_cv);
    lv_obj_set_size(s_cv, 240, 240);
    lv_obj_set_pos(s_cv, 0, 0);
    lv_obj_clear_flag(s_cv, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s_cv, plane_draw, LV_EVENT_DRAW_MAIN, NULL);

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
    s_tm = lv_timer_create(plane_cb, PERIOD, NULL);
}

static void app_plane_exit(void)
{
    if (s_tm != NULL) { lv_timer_del(s_tm); s_tm = NULL; }
    game_lives_clear();
}

const app_t app_plane = { "飞行射击", app_plane_enter, app_plane_exit };
