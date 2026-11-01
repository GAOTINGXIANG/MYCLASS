/**
 * @file app_tank.c
 * @brief 坦克射击：底部坦克左右移动，按 KEY2 向上开炮，击毁上方敌方坦克。
 *   摇杆左右 / 编码器 = 移动
 *   KEY2 / JS / ENC  = 开炮（结束后重开）
 *   KEY1             = 返回上一级
 *   3 个 LED 显示生命，中弹扣 1 命。
 */
#include "app_tank.h"
#include "lvgl.h"
#include "game_common.h"
#include "keys.h"
#include "encoder.h"
#include "joystick.h"

#define PERIOD      30
#define FX0         35
#define FY0         35
#define FW          170
#define PY          178         /* 我方坦克中心 y */
#define TANK_W      18
#define TANK_H      12
#define ENEMY_N     3
#define PB_MAX      4
#define EB_MAX      3
#define PB_SPEED    5
#define EB_SPEED    3
#define ENC_DETENT  4
#define SPEED_P     4
#define FIRE_TICK   42
#define INVULN_TICK 24

typedef struct {
    int16_t x, y;
    uint8_t act;
} bullet_t;

typedef struct {
    int16_t x, y;
    int8_t  dir;
    uint8_t act;
    uint8_t wait;
} enemy_t;

static lv_timer_t *s_tm;
static lv_obj_t   *s_cv, *s_score, *s_msg;

static int16_t  s_px = 120;
static bullet_t s_pb[PB_MAX];
static bullet_t s_eb[EB_MAX];
static enemy_t  s_en[ENEMY_N];
static uint8_t  s_over;
static uint16_t s_score_v;
static uint16_t s_fire_acc;
static uint8_t  s_invuln;
static int32_t  s_last_cnt;
static uint32_t s_rng = 0x31415926UL;

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

static void tank(lv_draw_ctx_t *ctx, int16_t cx, int16_t cy, lv_color_t c, uint8_t up)
{
    box(ctx, (int16_t)(cx - TANK_W / 2), (int16_t)(cy - TANK_H / 2),
        TANK_W, TANK_H, c, 2);
    if (up != 0U) box(ctx, (int16_t)(cx - 1), (int16_t)(cy - TANK_H / 2 - 7), 3, 8, c, 1);
    else          box(ctx, (int16_t)(cx - 1), (int16_t)(cy + TANK_H / 2 - 1), 3, 8, c, 1);
}

static void tank_draw(lv_event_t *e)
{
    lv_draw_ctx_t *ctx = lv_event_get_draw_ctx(e);

    for (uint8_t i = 0; i < ENEMY_N; i++) {
        if (s_en[i].act == 0U) continue;
        tank(ctx, s_en[i].x, s_en[i].y, lv_palette_main(LV_PALETTE_RED), 0U);
    }
    for (uint8_t i = 0; i < PB_MAX; i++) {
        if (s_pb[i].act == 0U) continue;
        box(ctx, (int16_t)(s_pb[i].x - 1), (int16_t)(s_pb[i].y - 4), 3, 6,
            lv_palette_main(LV_PALETTE_YELLOW), 1);
    }
    for (uint8_t i = 0; i < EB_MAX; i++) {
        if (s_eb[i].act == 0U) continue;
        box(ctx, (int16_t)(s_eb[i].x - 1), (int16_t)(s_eb[i].y - 4), 3, 6,
            lv_palette_main(LV_PALETTE_ORANGE), 1);
    }
    if (s_invuln != 0U && ((s_invuln & 4U) != 0U)) {
        tank(ctx, s_px, PY, lv_palette_main(LV_PALETTE_BLUE_GREY), 1U);
    } else {
        tank(ctx, s_px, PY, lv_palette_main(LV_PALETTE_CYAN), 1U);
    }
}

/* ---------------- 逻辑 ---------------- */
static uint8_t hit(int16_t ax, int16_t ay, int16_t aw, int16_t ah,
                   int16_t bx, int16_t by, int16_t bw, int16_t bh)
{
    if (ax + aw <= bx || bx + bw <= ax) return 0U;
    if (ay + ah <= by || by + bh <= ay) return 0U;
    return 1U;
}

static void enemy_reset(enemy_t *en, uint8_t row)
{
    en->x    = (int16_t)(FX0 + 20 + (int16_t)(rnd() % (uint32_t)(FW - 40)));
    en->y    = (int16_t)(FY0 + 16 + row * 30);
    en->dir  = (int8_t)((rnd() & 1U) ? 1 : -1);
    en->act  = 1U;
    en->wait = 0U;
}

static void fire_player(void)
{
    for (uint8_t i = 0; i < PB_MAX; i++) {
        if (s_pb[i].act != 0U) continue;
        s_pb[i].x   = s_px;
        s_pb[i].y   = (int16_t)(PY - TANK_H / 2 - 6);
        s_pb[i].act = 1U;
        return;
    }
}

static void new_game(void)
{
    for (uint8_t i = 0; i < PB_MAX; i++) s_pb[i].act = 0U;
    for (uint8_t i = 0; i < EB_MAX; i++) s_eb[i].act = 0U;
    for (uint8_t i = 0; i < ENEMY_N; i++) enemy_reset(&s_en[i], i);

    s_px      = 120;
    s_over    = 0U;
    s_score_v = 0U;
    s_fire_acc = 0U;
    s_invuln  = 0U;
    s_last_cnt = encoder_count();
    lv_obj_add_flag(s_msg, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(s_score, "得分 0");
    game_lives_set(3);
    lv_obj_invalidate(s_cv);
}

static void tank_cb(lv_timer_t *t)
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
    if (s_px < FX0 + TANK_W / 2)         s_px = (int16_t)(FX0 + TANK_W / 2);
    if (s_px > FX0 + FW - TANK_W / 2)    s_px = (int16_t)(FX0 + FW - TANK_W / 2);

    /* 开炮 */
    if ((edge & (KEY_MASK(KEY_ID_2) | KEY_MASK(KEY_ID_JS) |
                 KEY_MASK(KEY_ID_ENC))) != 0U) fire_player();

    /* 我方子弹 */
    for (uint8_t i = 0; i < PB_MAX; i++) {
        if (s_pb[i].act == 0U) continue;
        s_pb[i].y = (int16_t)(s_pb[i].y - PB_SPEED);
        if (s_pb[i].y < FY0) { s_pb[i].act = 0U; continue; }

        for (uint8_t k = 0; k < ENEMY_N; k++) {
            if (s_en[k].act == 0U) continue;
            if (hit((int16_t)(s_pb[i].x - 1), (int16_t)(s_pb[i].y - 4), 3, 6,
                    (int16_t)(s_en[k].x - TANK_W / 2),
                    (int16_t)(s_en[k].y - TANK_H / 2), TANK_W, TANK_H) != 0U) {
                s_pb[i].act = 0U;
                s_en[k].act = 0U;
                s_en[k].wait = 50U;
                s_score_v = (uint16_t)(s_score_v + 10U);
                lv_label_set_text_fmt(s_score, "得分 %u", (unsigned)s_score_v);
                break;
            }
        }
    }

    /* 敌方坦克移动 / 复活 */
    for (uint8_t i = 0; i < ENEMY_N; i++) {
        enemy_t *en = &s_en[i];
        if (en->act == 0U) {
            if (en->wait > 0U && --en->wait == 0U) enemy_reset(en, i);
            continue;
        }
        en->x = (int16_t)(en->x + en->dir * 2);
        if (en->x < FX0 + TANK_W / 2)          { en->x = (int16_t)(FX0 + TANK_W / 2);          en->dir = 1; }
        if (en->x > FX0 + FW - TANK_W / 2)     { en->x = (int16_t)(FX0 + FW - TANK_W / 2);     en->dir = -1; }
    }

    /* 敌方开炮 */
    if (++s_fire_acc >= FIRE_TICK) {
        uint8_t cnt = 0U, pick = 0U;
        int8_t  slot = -1;
        uint8_t src = 0xFFU;

        s_fire_acc = 0U;
        for (uint8_t i = 0; i < EB_MAX; i++) {
            if (s_eb[i].act == 0U) { slot = (int8_t)i; break; }
        }
        for (uint8_t k = 0; k < ENEMY_N; k++) {
            if (s_en[k].act != 0U) cnt++;
        }
        if (slot >= 0 && cnt > 0U) {
            pick = (uint8_t)(rnd() % cnt);
            for (uint8_t k = 0; k < ENEMY_N; k++) {
                if (s_en[k].act == 0U) continue;
                if (pick == 0U) { src = k; break; }
                pick--;
            }
        }
        if (slot >= 0 && src != 0xFFU) {
            s_eb[slot].x   = s_en[src].x;
            s_eb[slot].y   = (int16_t)(s_en[src].y + TANK_H / 2 + 6);
            s_eb[slot].act = 1U;
        }
    }

    /* 敌方子弹 */
    for (uint8_t i = 0; i < EB_MAX; i++) {
        if (s_eb[i].act == 0U) continue;
        s_eb[i].y = (int16_t)(s_eb[i].y + EB_SPEED);
        if (s_eb[i].y > FY0 + FW) { s_eb[i].act = 0U; continue; }

        if (s_invuln == 0U &&
            hit((int16_t)(s_eb[i].x - 1), (int16_t)(s_eb[i].y - 4), 3, 6,
                (int16_t)(s_px - TANK_W / 2), (int16_t)(PY - TANK_H / 2), TANK_W, TANK_H) != 0U) {
            s_eb[i].act = 0U;
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

static void app_tank_enter(void)
{
    lv_obj_t *scr = lv_scr_act();

    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

    s_cv = lv_obj_create(scr);
    lv_obj_remove_style_all(s_cv);
    lv_obj_set_size(s_cv, 240, 240);
    lv_obj_set_pos(s_cv, 0, 0);
    lv_obj_clear_flag(s_cv, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s_cv, tank_draw, LV_EVENT_DRAW_MAIN, NULL);

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
    s_tm = lv_timer_create(tank_cb, PERIOD, NULL);
}

static void app_tank_exit(void)
{
    if (s_tm != NULL) { lv_timer_del(s_tm); s_tm = NULL; }
    game_lives_clear();
}

const app_t app_tank = { "坦克射击", app_tank_enter, app_tank_exit };
