/**
 * @file app_fish.c
 * @brief 大鱼吃小鱼：底部小鱼左右移动，吃比自己小的鱼变大加分，碰到大鱼扣命。
 *   摇杆左右 / 编码器 = 左右移动
 *   KEY2 / JS / ENC  = 开始 / 结束后重开
 *   KEY1             = 返回上一级
 *   3 个 LED 显示生命。
 */
#include "app_fish.h"
#include "lvgl.h"
#include "game_common.h"
#include "keys.h"
#include "encoder.h"
#include "joystick.h"

#define PERIOD     30
#define FX0        35          /* 活动区左上角 */
#define FY0        35
#define FW         170
#define PY         168         /* 我方鱼所在 y */
#define FISH_MAX   8
#define PR_MIN     7
#define PR_MAX     16
#define ENC_DETENT 4
#define SPAWN_TICK 26          /* 约 0.8s 生成一条鱼 */
#define SPEED_P    4           /* 我方移动速度 px/拍 */

typedef struct {
    int16_t x, y;
    int16_t vy;
    uint8_t r;
    uint8_t act;
} fish_t;

static lv_timer_t *s_tm;
static lv_obj_t   *s_cv, *s_score, *s_msg;

static fish_t   s_f[FISH_MAX];
static int16_t  s_px;
static uint8_t  s_pr;
static uint8_t  s_over;
static uint16_t s_score_v;
static uint8_t  s_eat;
static uint16_t s_spawn;
static int32_t  s_last_cnt;
static uint32_t s_rng = 0x2468ACE1UL;

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

static void fish_body(lv_draw_ctx_t *ctx, int16_t x, int16_t y, int16_t r, lv_color_t c)
{
    ball(ctx, x, y, r, c);                                      /* 身体 */
    box(ctx, (int16_t)(x - r - r / 2), (int16_t)(y - r / 3),
        (int16_t)(r), (int16_t)(2 * r / 3 + 1), c, 2);          /* 尾巴 */
    ball(ctx, (int16_t)(x + r / 2), (int16_t)(y - r / 3), 2, lv_color_white()); /* 眼睛 */
}

static void fish_draw(lv_event_t *e)
{
    lv_draw_ctx_t *ctx = lv_event_get_draw_ctx(e);

    for (uint8_t i = 0; i < FISH_MAX; i++) {
        if (s_f[i].act == 0U) continue;
        fish_body(ctx, s_f[i].x, s_f[i].y, s_f[i].r,
                  (s_f[i].r <= PR_MIN + 1) ? lv_palette_main(LV_PALETTE_GREEN)
                                           : lv_palette_main(LV_PALETTE_RED));
    }
    fish_body(ctx, s_px, PY, s_pr, lv_palette_main(LV_PALETTE_CYAN));
}

/* ---------------- 逻辑 ---------------- */
static void spawn_fish(void)
{
    for (uint8_t i = 0; i < FISH_MAX; i++) {
        if (s_f[i].act != 0U) continue;

        s_f[i].r   = ((rnd() % 100U) < 35U) ? (uint8_t)(11U + rnd() % 4U)
                                            : (uint8_t)(5U + rnd() % 4U);
        s_f[i].x   = (int16_t)(FX0 + s_f[i].r + 2 +
                               (int16_t)(rnd() % (uint32_t)(FW - 2 * s_f[i].r - 4)));
        s_f[i].y   = (int16_t)(FY0 + 4);
        s_f[i].vy  = (s_f[i].r >= 11U) ? 1 : 2;
        s_f[i].act = 1U;
        return;
    }
}

static void new_game(void)
{
    for (uint8_t i = 0; i < FISH_MAX; i++) s_f[i].act = 0U;
    s_px      = 120;
    s_pr      = PR_MIN;
    s_over    = 0U;
    s_score_v = 0U;
    s_eat     = 0U;
    s_spawn   = 0U;
    s_last_cnt = encoder_count();
    lv_obj_add_flag(s_msg, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(s_score, "得分 0");
    game_lives_set(3);
    lv_obj_invalidate(s_cv);
}

static void eat_ok(void)
{
    s_score_v++;
    s_eat++;
    lv_label_set_text_fmt(s_score, "得分 %u", (unsigned)s_score_v);
    if ((s_eat % 3U) == 0U && s_pr < PR_MAX) s_pr++;
}

static void fish_cb(lv_timer_t *t)
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

    /* 左右移动 */
    d = encoder_count() - s_last_cnt;
    s_last_cnt += d;
    while (d >= ENC_DETENT)  { d -= ENC_DETENT; s_px += SPEED_P; }
    while (d <= -ENC_DETENT) { d += ENC_DETENT; s_px -= SPEED_P; }
    if ((joy_get_mask() & JOY_MASK_LEFT)  != 0U) s_px -= SPEED_P;
    if ((joy_get_mask() & JOY_MASK_RIGHT) != 0U) s_px += SPEED_P;

    if (s_px < FX0 + s_pr)            s_px = (int16_t)(FX0 + s_pr);
    if (s_px > FX0 + FW - s_pr)       s_px = (int16_t)(FX0 + FW - s_pr);

    if (++s_spawn >= SPAWN_TICK) { s_spawn = 0U; spawn_fish(); }

    for (uint8_t i = 0; i < FISH_MAX; i++) {
        int16_t dx, dy, rr;
        fish_t *f = &s_f[i];

        if (f->act == 0U) continue;
        f->y = (int16_t)(f->y + f->vy);
        if (f->y > FY0 + FW) { f->act = 0U; continue; }

        dx = (int16_t)(f->x - s_px);
        if (dx < 0) dx = (int16_t)-dx;
        dy = (int16_t)(f->y - PY);
        if (dy < 0) dy = (int16_t)-dy;
        rr = (int16_t)(s_pr + f->r - 2);

        if (dx < rr && dy < rr) {
            if (f->r <= s_pr) {
                f->act = 0U;
                eat_ok();
            } else {
                f->act = 0U;
                if (game_lives_dec() == 0U) {
                    s_over = 1U;
                    lv_label_set_text(s_msg, "游戏结束");
                    lv_obj_clear_flag(s_msg, LV_OBJ_FLAG_HIDDEN);
                }
            }
        }
    }

    lv_obj_invalidate(s_cv);
}

static void app_fish_enter(void)
{
    lv_obj_t *scr = lv_scr_act();

    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

    s_cv = lv_obj_create(scr);
    lv_obj_remove_style_all(s_cv);
    lv_obj_set_size(s_cv, 240, 240);
    lv_obj_set_pos(s_cv, 0, 0);
    lv_obj_clear_flag(s_cv, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s_cv, fish_draw, LV_EVENT_DRAW_MAIN, NULL);

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
    s_tm = lv_timer_create(fish_cb, PERIOD, NULL);
}

static void app_fish_exit(void)
{
    if (s_tm != NULL) { lv_timer_del(s_tm); s_tm = NULL; }
    game_lives_clear();
}

const app_t app_fish = { "大鱼小鱼", app_fish_enter, app_fish_exit };
