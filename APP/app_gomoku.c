/**
 * @file app_gomoku.c
 * @brief 五子棋（双人轮流）：13x13 棋盘，自定义绘制棋盘与棋子。
 *   摇杆       = 移动光标
 *   编码器     = 左右移动光标
 *   ENC/JS 键  = 落子
 *   KEY1       = 返回主菜单
 *   底部小圆点显示当前轮到谁（白 / 蓝）。
 */
#include "app_gomoku.h"
#include "lvgl.h"
#include "encoder.h"
#include "keys.h"
#include "joystick.h"

#define GO_N      13
#define GO_STEP   13
#define GO_PAD    2
#define GO_SIZE   (GO_STEP * (GO_N - 1) + GO_PAD * 2)   /* 160 */
#define GO_ORG    ((240 - GO_SIZE) / 2)
#define GO_STONE  5                                     /* 棋子半径 */
#define PERIOD    30
#define ENC_DETENT 4

static lv_timer_t *s_tm;
static lv_obj_t   *s_board, *s_turn, *s_msg;
static uint8_t     s_b[GO_N][GO_N];     /* 0 空，1 白，2 蓝 */
static uint8_t     s_cx = GO_N / 2, s_cy = GO_N / 2;
static uint8_t     s_cur = 1;           /* 当前落子方 */
static uint8_t     s_over;
static int32_t     s_last_cnt;
static joy_mask_t  s_prev_mask = JOY_MASK_NONE;

static lv_coord_t cell_x(lv_coord_t x0, uint8_t c) { return (lv_coord_t)(x0 + GO_PAD + c * GO_STEP); }
static lv_coord_t cell_y(lv_coord_t y0, uint8_t r) { return (lv_coord_t)(y0 + GO_PAD + r * GO_STEP); }

/* ---------------- 绘制 ---------------- */
static void go_draw(lv_event_t *e)
{
    lv_obj_t      *obj = lv_event_get_target(e);
    lv_draw_ctx_t *ctx = lv_event_get_draw_ctx(e);
    lv_area_t      co;
    lv_draw_rect_dsc_t rd;
    lv_draw_line_dsc_t ld;
    lv_point_t p1, p2;

    lv_obj_get_coords(obj, &co);

    lv_draw_rect_dsc_init(&rd);
    rd.bg_opa = LV_OPA_TRANSP;
    rd.border_color = lv_palette_main(LV_PALETTE_BLUE_GREY);
    rd.border_width = 1;
    rd.radius = 4;
    lv_draw_rect(ctx, &rd, &co);

    lv_draw_line_dsc_init(&ld);
    ld.color = lv_palette_darken(LV_PALETTE_GREY, 1);
    ld.width = 1;
    ld.opa = LV_OPA_COVER;

    for (uint8_t i = 0; i < GO_N; i++) {
        p1.x = cell_x(co.x1, i);
        p1.y = cell_y(co.y1, 0);
        p2.x = p1.x;
        p2.y = cell_y(co.y1, (uint8_t)(GO_N - 1));
        lv_draw_line(ctx, &ld, &p1, &p2);

        p1.x = cell_x(co.x1, 0);
        p1.y = cell_y(co.y1, i);
        p2.x = cell_x(co.x1, (uint8_t)(GO_N - 1));
        p2.y = p1.y;
        lv_draw_line(ctx, &ld, &p1, &p2);
    }

    lv_draw_rect_dsc_init(&rd);
    rd.bg_opa = LV_OPA_COVER;
    rd.border_width = 0;
    rd.radius = LV_RADIUS_CIRCLE;

    for (uint8_t r = 0; r < GO_N; r++) {
        for (uint8_t c = 0; c < GO_N; c++) {
            lv_area_t a;
            lv_coord_t x, y;
            if (s_b[r][c] == 0U) continue;
            x = cell_x(co.x1, c);
            y = cell_y(co.y1, r);
            a.x1 = (lv_coord_t)(x - GO_STONE);
            a.y1 = (lv_coord_t)(y - GO_STONE);
            a.x2 = (lv_coord_t)(x + GO_STONE);
            a.y2 = (lv_coord_t)(y + GO_STONE);
            rd.bg_color = (s_b[r][c] == 1U) ? lv_color_white()
                                            : lv_palette_main(LV_PALETTE_LIGHT_BLUE);
            lv_draw_rect(ctx, &rd, &a);
        }
    }

    /* 光标：方框 */
    lv_draw_rect_dsc_init(&rd);
    rd.bg_opa = LV_OPA_TRANSP;
    rd.border_color = lv_palette_main(LV_PALETTE_AMBER);
    rd.border_width = 1;
    rd.radius = 2;
    {
        lv_area_t a;
        lv_coord_t x = cell_x(co.x1, s_cx);
        lv_coord_t y = cell_y(co.y1, s_cy);
        a.x1 = (lv_coord_t)(x - GO_STEP / 2);
        a.y1 = (lv_coord_t)(y - GO_STEP / 2);
        a.x2 = (lv_coord_t)(x + GO_STEP / 2);
        a.y2 = (lv_coord_t)(y + GO_STEP / 2);
        lv_draw_rect(ctx, &rd, &a);
    }
}

/* ---------------- 逻辑 ---------------- */
static uint8_t check_win(uint8_t r, uint8_t c)
{
    static const int8_t dirs[4][2] = { { 1, 0 }, { 0, 1 }, { 1, 1 }, { 1, -1 } };
    uint8_t me = s_b[r][c];

    for (uint8_t i = 0; i < 4; i++) {
        uint8_t cnt = 1;
        for (uint8_t s = 0; s < 2U; s++) {
            int8_t dr = dirs[i][0];
            int8_t dc = dirs[i][1];
            if (s == 1U) { dr = (int8_t)-dr; dc = (int8_t)-dc; }
            int8_t rr = (int8_t)r;
            int8_t cc = (int8_t)c;
            while (1) {
                rr = (int8_t)(rr + dr);
                cc = (int8_t)(cc + dc);
                if (rr < 0 || cc < 0 || rr >= GO_N || cc >= GO_N) break;
                if (s_b[rr][cc] != me) break;
                cnt++;
            }
        }
        if (cnt >= 5U) return 1U;
    }
    return 0U;
}

static void render_turn(void)
{
    lv_obj_set_style_bg_color(s_turn,
        (s_cur == 1U) ? lv_color_white() : lv_palette_main(LV_PALETTE_LIGHT_BLUE), 0);
}

static void new_game(void)
{
    for (uint8_t r = 0; r < GO_N; r++) {
        for (uint8_t c = 0; c < GO_N; c++) s_b[r][c] = 0U;
    }
    s_cx = GO_N / 2;
    s_cy = GO_N / 2;
    s_cur = 1;
    s_over = 0;
    s_prev_mask = JOY_MASK_NONE;
    s_last_cnt = encoder_count();
    lv_obj_add_flag(s_msg, LV_OBJ_FLAG_HIDDEN);
    render_turn();
    lv_obj_invalidate(s_board);
}

static void place(void)
{
    if (s_b[s_cy][s_cx] != 0U) return;
    s_b[s_cy][s_cx] = s_cur;
    lv_obj_invalidate(s_board);

    if (check_win(s_cy, s_cx) != 0U) {
        s_over = 1;
        lv_obj_set_style_text_color(s_msg, lv_color_white(), 0);
        lv_label_set_text(s_msg, "你赢了");
        lv_obj_clear_flag(s_msg, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    s_cur = (s_cur == 1U) ? 2U : 1U;
    render_turn();
}

static void go_cb(lv_timer_t *t)
{
    uint8_t edge, moved = 0;
    int32_t d;
    (void)t;

    edge = keys_scan();
    if (s_over != 0U) {
        if ((edge & (KEY_MASK(KEY_ID_JS) | KEY_MASK(KEY_ID_ENC))) != 0U) {
            new_game();
        }
        return;
    }

    d = encoder_count() - s_last_cnt;
    s_last_cnt += d;

    {
        joy_mask_t m = joy_get_mask();
        if (m != s_prev_mask) {
            if ((m & JOY_MASK_UP) != 0U && s_cy > 0U)                   { s_cy--; moved = 1; }
            else if ((m & JOY_MASK_DOWN) != 0U && s_cy < GO_N - 1U)     { s_cy++; moved = 1; }
            else if ((m & JOY_MASK_LEFT) != 0U && s_cx > 0U)            { s_cx--; moved = 1; }
            else if ((m & JOY_MASK_RIGHT) != 0U && s_cx < GO_N - 1U)    { s_cx++; moved = 1; }
            s_prev_mask = m;
        }
    }

    while (d >= ENC_DETENT)  { d -= ENC_DETENT; if (s_cx < GO_N - 1U) { s_cx++; moved = 1; } }
    while (d <= -ENC_DETENT) { d += ENC_DETENT; if (s_cx > 0U)        { s_cx--; moved = 1; } }

    if (moved != 0U) lv_obj_invalidate(s_board);

    if ((edge & (KEY_MASK(KEY_ID_JS) | KEY_MASK(KEY_ID_ENC))) != 0U) place();
}

static void app_gomoku_enter(void)
{
    lv_obj_t *scr = lv_scr_act();

    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

    s_board = lv_obj_create(scr);
    lv_obj_remove_style_all(s_board);
    lv_obj_set_size(s_board, GO_SIZE, GO_SIZE);
    lv_obj_set_pos(s_board, GO_ORG, GO_ORG);
    lv_obj_clear_flag(s_board, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s_board, go_draw, LV_EVENT_DRAW_MAIN, NULL);

    s_turn = lv_obj_create(scr);
    lv_obj_remove_style_all(s_turn);
    lv_obj_set_size(s_turn, 12, 12);
    lv_obj_set_style_radius(s_turn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(s_turn, LV_OPA_COVER, 0);
    lv_obj_set_pos(s_turn, 114, 210);

    s_msg = lv_label_create(scr);
    lv_obj_set_style_text_color(s_msg, lv_color_white(), 0);
    lv_obj_set_style_bg_color(s_msg, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_msg, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(s_msg, 6, 0);
    lv_obj_align(s_msg, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(s_msg, LV_OBJ_FLAG_HIDDEN);

    new_game();
    s_tm = lv_timer_create(go_cb, PERIOD, NULL);
}

static void app_gomoku_exit(void)
{
    if (s_tm != NULL) { lv_timer_del(s_tm); s_tm = NULL; }
}

const app_t app_gomoku = { "五子棋", app_gomoku_enter, app_gomoku_exit };
