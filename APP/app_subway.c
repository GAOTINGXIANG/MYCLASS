/**
 * @file app_subway.c
 * @brief 地铁跑酷（简化）：三条赛道，左右换道躲避障碍，速度越来越快。
 *   摇杆左右 / 编码器 / KEY3 / KEY4 = 换道
 *   KEY2 / JS / ENC  = 结束后重开
 *   KEY1             = 返回上一级
 *   3 个 LED 显示生命，撞障碍扣 1 命。
 */
#include "app_subway.h"
#include "lvgl.h"
#include "game_common.h"
#include "keys.h"
#include "encoder.h"
#include "joystick.h"

#define PERIOD      30
#define FX0         35
#define FY0         35
#define FW          170
#define PY          166
#define LANE_N      3
#define OBST_MAX    4
#define ENC_DETENT  4
#define INVULN_TICK 20
#define SPAWN_TICK  26

static const int16_t LANE_X[LANE_N] = { 78, 120, 162 };

typedef struct {
    int16_t y;
    uint8_t lane;
    uint8_t act;
} obst_t;

static lv_timer_t *s_tm;
static lv_obj_t   *s_cv, *s_score, *s_msg;

static uint8_t  s_lane;
static int16_t  s_x;
static obst_t   s_ob[OBST_MAX];
static uint8_t  s_over;
static uint32_t s_score_v;
static uint8_t  s_speed;
static uint8_t  s_spawn;
static uint8_t  s_invuln;
static int32_t  s_last_cnt;
static joy_dir_t s_prev_dir = JOY_DIR_NONE;
static uint32_t s_rng = 0x0FEEDBEEUL;

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

static void subway_draw(lv_event_t *e)
{
    lv_draw_ctx_t *ctx = lv_event_get_draw_ctx(e);

    /* 赛道分隔线 */
    box(ctx, (int16_t)(LANE_X[0] + LANE_X[1]) / 2, 40, 1, 160,
        lv_palette_main(LV_PALETTE_BLUE_GREY), 0);
    box(ctx, (int16_t)(LANE_X[1] + LANE_X[2]) / 2, 40, 1, 160,
        lv_palette_main(LV_PALETTE_BLUE_GREY), 0);

    for (uint8_t i = 0; i < OBST_MAX; i++) {
        if (s_ob[i].act == 0U) continue;
        box(ctx, (int16_t)(LANE_X[s_ob[i].lane] - 12), (int16_t)(s_ob[i].y - 9),
            24, 18, lv_palette_main(LV_PALETTE_RED), 3);
    }

    /* 跑者 */
    if (s_invuln != 0U && ((s_invuln & 4U) != 0U)) {
        box(ctx, (int16_t)(s_x - 7), (int16_t)(PY - 8), 14, 16,
            lv_palette_main(LV_PALETTE_BLUE_GREY), 3);
    } else {
        box(ctx, (int16_t)(s_x - 7), (int16_t)(PY - 4), 14, 14,
            lv_palette_main(LV_PALETTE_CYAN), 3);
        ball(ctx, s_x, (int16_t)(PY - 10), 4, lv_palette_main(LV_PALETTE_ORANGE));
    }
}

/* ---------------- 逻辑 ---------------- */
static void spawn_obst(void)
{
    uint8_t free_lane = (uint8_t)(rnd() % LANE_N);
    for (uint8_t i = 0; i < OBST_MAX; i++) {
        if (s_ob[i].act != 0U) continue;
        s_ob[i].lane = free_lane;
        s_ob[i].y    = (int16_t)(FY0 + 2);
        s_ob[i].act  = 1U;
        return;
    }
}

static void new_game(void)
{
    for (uint8_t i = 0; i < OBST_MAX; i++) s_ob[i].act = 0U;

    s_lane     = 1U;
    s_x        = LANE_X[1];
    s_over     = 0U;
    s_score_v  = 0U;
    s_speed    = 2U;
    s_spawn    = 0U;
    s_invuln   = 0U;
    s_last_cnt = encoder_count();
    s_prev_dir = joy_get_dir();
    lv_obj_add_flag(s_msg, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(s_score, "得分 0");
    game_lives_set(3);
    lv_obj_invalidate(s_cv);
}

static void subway_cb(lv_timer_t *t)
{
    uint8_t edge;
    int32_t d;
    int8_t  move = 0;
    (void)t;

    edge = keys_scan();
    if (s_over != 0U) {
        if ((edge & (KEY_MASK(KEY_ID_2) | KEY_MASK(KEY_ID_JS) |
                     KEY_MASK(KEY_ID_ENC))) != 0U) new_game();
        return;
    }

    /* 换道 */
    d = encoder_count() - s_last_cnt;
    s_last_cnt += d;
    while (d >= ENC_DETENT)  { d -= ENC_DETENT; move++; }
    while (d <= -ENC_DETENT) { d += ENC_DETENT; move--; }
    if ((edge & KEY_MASK(KEY_ID_4)) != 0U) move++;
    if ((edge & KEY_MASK(KEY_ID_3)) != 0U) move--;

    {
        joy_dir_t dir = joy_get_dir();
        if (dir != s_prev_dir) {
            if (dir == JOY_DIR_RIGHT || dir == JOY_DIR_UP_RIGHT ||
                dir == JOY_DIR_DOWN_RIGHT) move++;
            else if (dir == JOY_DIR_LEFT || dir == JOY_DIR_UP_LEFT ||
                     dir == JOY_DIR_DOWN_LEFT) move--;
            s_prev_dir = dir;
        }
    }

    if (move != 0) {
        int32_t nl = (int32_t)s_lane + move;
        if (nl < 0) nl = 0;
        if (nl >= LANE_N) nl = LANE_N - 1;
        s_lane = (uint8_t)nl;
    }

    /* 平滑移动到所在车道 */
    if (s_x < LANE_X[s_lane])       s_x += 6;
    if (s_x > LANE_X[s_lane])       s_x -= 6;
    if (s_x > LANE_X[s_lane] - 3 && s_x < LANE_X[s_lane] + 3) s_x = LANE_X[s_lane];

    /* 速度随距离提升 */
    s_score_v++;
    if ((s_score_v % 400U) == 0U && s_speed < 4U) s_speed++;
    lv_label_set_text_fmt(s_score, "得分 %u", (unsigned)(s_score_v / 4U));

    if (++s_spawn >= SPAWN_TICK) {
        s_spawn = 0U;
        if (s_speed < 4U || (rnd() & 1U) != 0U) spawn_obst();
    }

    /* 障碍滚动 + 碰撞 */
    for (uint8_t i = 0; i < OBST_MAX; i++) {
        if (s_ob[i].act == 0U) continue;
        s_ob[i].y = (int16_t)(s_ob[i].y + s_speed);
        if (s_ob[i].y > FY0 + FW + 10) { s_ob[i].act = 0U; continue; }

        if (s_invuln != 0U) continue;
        if (s_ob[i].lane == s_lane) {
            int16_t dy = (int16_t)(s_ob[i].y - PY);
            if (dy < 0) dy = (int16_t)-dy;
            if (dy < 15) {
                s_ob[i].act = 0U;
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

static void app_subway_enter(void)
{
    lv_obj_t *scr = lv_scr_act();

    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

    s_cv = lv_obj_create(scr);
    lv_obj_remove_style_all(s_cv);
    lv_obj_set_size(s_cv, 240, 240);
    lv_obj_set_pos(s_cv, 0, 0);
    lv_obj_clear_flag(s_cv, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s_cv, subway_draw, LV_EVENT_DRAW_MAIN, NULL);

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
    s_tm = lv_timer_create(subway_cb, PERIOD, NULL);
}

static void app_subway_exit(void)
{
    if (s_tm != NULL) { lv_timer_del(s_tm); s_tm = NULL; }
    game_lives_clear();
}

const app_t app_subway = { "地铁跑酷", app_subway_enter, app_subway_exit };
