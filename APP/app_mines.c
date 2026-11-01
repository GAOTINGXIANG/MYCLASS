/**
 * @file app_mines.c
 * @brief 扫雷：8x8 棋盘 / 10 颗雷，自定义绘制（不占对象内存）。
 *   摇杆 / 编码器 = 移动光标
 *   ENC/JS 键     = 翻开格子
 *   KEY2          = 插旗 / 取消插旗
 *   KEY1          = 返回主菜单
 *   首次翻开的位置及其 8 邻域保证无雷。
 */
#include "app_mines.h"
#include "lvgl.h"
#include "lv_font_cn.h"
#include "encoder.h"
#include "keys.h"
#include "joystick.h"

#define MW         8
#define MH         8
#define MCELL      20
#define MORG_X     ((240 - MW * MCELL) / 2)
#define MORG_Y     40
#define MINE_N     10
#define PERIOD     30
#define ENC_DETENT 4

static lv_timer_t  *s_tm;
static lv_obj_t    *s_board, *s_hud, *s_msg;

static uint8_t      s_mine[MW * MH];
static uint8_t      s_open[MW * MH];
static uint8_t      s_flag[MW * MH];
static int8_t       s_cx, s_cy;
static uint8_t      s_placed;          /* 雷已布下 */
static uint8_t      s_over;            /* 0 进行 / 1 踩雷 / 2 通关 */
static uint8_t      s_open_cnt, s_flag_cnt;
static int32_t      s_last_cnt;
static joy_mask_t   s_prev_mask = JOY_MASK_NONE;
static uint32_t     s_rng = 0x2468ACE1UL;

/* 1~8 的数字配色 */
static const uint32_t DIGC[9] = {
    0x000000, 0x3399FF, 0x22CC66, 0xFF4444, 0xFF9900,
    0xAA66FF, 0x00CCCC, 0xFFD700, 0xFFFFFF
};

static uint32_t rnd(void)
{
    s_rng = s_rng * 1103515245UL + 12345UL;
    return (s_rng >> 16) & 0x7FFFUL;
}

static uint8_t around_mines(int8_t c, int8_t r)
{
    uint8_t n = 0;
    for (uint8_t i = 0; i < 9U; i++) {
        int8_t x = (int8_t)(c + (int8_t)(i % 3U) - 1);
        int8_t y = (int8_t)(r + (int8_t)(i / 3U) - 1);
        if (x < 0 || y < 0 || x >= MW || y >= MH) continue;
        if (s_mine[y * MW + x] != 0U) n++;
    }
    return n;
}

/* ---------------- 绘制 ---------------- */
static void cell_rect(lv_draw_ctx_t *ctx, lv_coord_t x0, lv_coord_t y0,
                      int8_t c, int8_t r, uint32_t color)
{
    lv_draw_rect_dsc_t rd;
    lv_area_t a;

    lv_draw_rect_dsc_init(&rd);
    rd.bg_opa    = LV_OPA_COVER;
    rd.bg_color  = lv_color_hex(color);
    rd.radius    = 2;
    a.x1 = (lv_coord_t)(x0 + c * MCELL + 1);
    a.y1 = (lv_coord_t)(y0 + r * MCELL + 1);
    a.x2 = (lv_coord_t)(x0 + c * MCELL + MCELL - 2);
    a.y2 = (lv_coord_t)(y0 + r * MCELL + MCELL - 2);
    lv_draw_rect(ctx, &rd, &a);
}

static void cell_text(lv_draw_ctx_t *ctx, lv_coord_t x0, lv_coord_t y0,
                      int8_t c, int8_t r, const char *txt, uint32_t color)
{
    lv_draw_label_dsc_t ld;
    lv_area_t a;

    lv_draw_label_dsc_init(&ld);
    ld.font  = &lv_font_cn_14;
    ld.color = lv_color_hex(color);
    ld.align = LV_TEXT_ALIGN_CENTER;

    a.x1 = (lv_coord_t)(x0 + c * MCELL);
    a.y1 = (lv_coord_t)(y0 + r * MCELL + 1);
    a.x2 = (lv_coord_t)(x0 + c * MCELL + MCELL - 1);
    a.y2 = (lv_coord_t)(y0 + r * MCELL + MCELL);
    lv_draw_label(ctx, &ld, &a, txt, NULL);
}

static void mines_draw(lv_event_t *e)
{
    lv_obj_t      *obj = lv_event_get_target(e);
    lv_draw_ctx_t *ctx = lv_event_get_draw_ctx(e);
    lv_area_t      co;
    lv_draw_rect_dsc_t rd;
    char           buf[2];

    lv_obj_get_coords(obj, &co);

    for (uint8_t r = 0; r < MH; r++) {
        for (uint8_t c = 0; c < MW; c++) {
            uint8_t k = (uint8_t)(r * MW + c);

            if (s_open[k] == 0U) {
                cell_rect(ctx, co.x1, co.y1, (int8_t)c, (int8_t)r, 0x303030);
                if (s_flag[k] != 0U) {
                    cell_text(ctx, co.x1, co.y1, (int8_t)c, (int8_t)r, "!", 0xFFD700);
                }
            } else if (s_mine[k] != 0U) {
                cell_rect(ctx, co.x1, co.y1, (int8_t)c, (int8_t)r, 0xCC2222);
            } else {
                uint8_t n = around_mines((int8_t)c, (int8_t)r);
                cell_rect(ctx, co.x1, co.y1, (int8_t)c, (int8_t)r, 0x101010);
                if (n != 0U) {
                    buf[0] = (char)('0' + n);
                    buf[1] = '\0';
                    cell_text(ctx, co.x1, co.y1, (int8_t)c, (int8_t)r, buf, DIGC[n]);
                }
            }
        }
    }

    /* 光标 */
    if (s_over == 0U) {
        lv_draw_rect_dsc_init(&rd);
        rd.bg_opa       = LV_OPA_TRANSP;
        rd.border_color = lv_color_hex(0xFFFFFF);
        rd.border_width = 2;
        rd.radius       = 2;
        lv_area_t a;
        a.x1 = (lv_coord_t)(co.x1 + s_cx * MCELL);
        a.y1 = (lv_coord_t)(co.y1 + s_cy * MCELL);
        a.x2 = (lv_coord_t)(a.x1 + MCELL - 1);
        a.y2 = (lv_coord_t)(a.y1 + MCELL - 1);
        lv_draw_rect(ctx, &rd, &a);
    }
}

/* ---------------- 逻辑 ---------------- */
static void hud_refresh(void)
{
    lv_label_set_text_fmt(s_hud, "剩余 %u", (unsigned)(MINE_N - s_flag_cnt));
}

static void msg_show(const char *txt)
{
    lv_label_set_text(s_msg, txt);
    lv_obj_clear_flag(s_msg, LV_OBJ_FLAG_HIDDEN);
}

static void place_mines(int8_t ac, int8_t ar)
{
    uint8_t n = 0;

    for (uint8_t i = 0; i < MW * MH; i++) s_mine[i] = 0U;
    while (n < MINE_N) {
        int8_t c = (int8_t)(rnd() % MW);
        int8_t r = (int8_t)(rnd() % MH);
        if (c >= ac - 1 && c <= ac + 1 && r >= ar - 1 && r <= ar + 1) continue;
        if (s_mine[r * MW + c] != 0U) continue;
        s_mine[r * MW + c] = 1U;
        n++;
    }
    s_placed = 1U;
}

static void flood_open(int8_t c0, int8_t r0)
{
    uint8_t st[MW * MH];
    uint8_t top = 0;
    uint8_t k = (uint8_t)(r0 * MW + c0);

    s_open[k] = 1U;
    s_open_cnt++;
    if (around_mines(c0, r0) != 0U) return;

    st[top++] = k;
    while (top > 0U) {
        k = st[--top];
        for (uint8_t i = 0; i < 9U; i++) {
            int8_t x = (int8_t)((int8_t)(k % MW) + (int8_t)(i % 3U) - 1);
            int8_t y = (int8_t)((int8_t)(k / MW) + (int8_t)(i / 3U) - 1);
            uint8_t nk;
            if (x < 0 || y < 0 || x >= MW || y >= MH) continue;
            nk = (uint8_t)(y * MW + x);
            if (s_open[nk] != 0U || s_flag[nk] != 0U) continue;
            s_open[nk] = 1U;
            s_open_cnt++;
            if (around_mines(x, y) == 0U) st[top++] = nk;
        }
    }
}

static void dig(void)
{
    uint8_t k = (uint8_t)(s_cy * MW + s_cx);

    if (s_open[k] != 0U || s_flag[k] != 0U) return;
    if (s_placed == 0U) place_mines(s_cx, s_cy);

    if (s_mine[k] != 0U) {
        s_open[k] = 1U;
        s_open_cnt++;
        s_over = 1U;
        msg_show("游戏结束");
        return;
    }
    flood_open(s_cx, s_cy);

    if (s_open_cnt >= (uint8_t)(MW * MH - MINE_N)) {
        s_over = 2U;
        msg_show("你赢了");
    }
}

static void toggle_flag(void)
{
    uint8_t k = (uint8_t)(s_cy * MW + s_cx);

    if (s_open[k] != 0U) return;
    if (s_flag[k] != 0U) {
        s_flag[k] = 0U;
        s_flag_cnt--;
    } else if (s_flag_cnt < MINE_N) {
        s_flag[k] = 1U;
        s_flag_cnt++;
    }
    hud_refresh();
}

static void new_game(void)
{
    for (uint8_t i = 0; i < MW * MH; i++) {
        s_mine[i] = 0U;
        s_open[i] = 0U;
        s_flag[i] = 0U;
    }
    s_cx = MW / 2;
    s_cy = MH / 2;
    s_placed   = 0U;
    s_over     = 0U;
    s_open_cnt = 0U;
    s_flag_cnt = 0U;
    s_prev_mask = JOY_MASK_NONE;
    s_last_cnt = encoder_count();
    lv_obj_add_flag(s_msg, LV_OBJ_FLAG_HIDDEN);
    hud_refresh();
    lv_obj_invalidate(s_board);
}

static void cursor_move(int8_t dx, int8_t dy)
{
    int8_t nx = (int8_t)(s_cx + dx);
    int8_t ny = (int8_t)(s_cy + dy);

    if (nx < 0) nx = 0;
    if (nx >= MW) nx = MW - 1;
    if (ny < 0) ny = 0;
    if (ny >= MH) ny = MH - 1;
    s_cx = nx;
    s_cy = ny;
}

static void mines_cb(lv_timer_t *t)
{
    uint8_t edge;
    int32_t d;
    joy_mask_t m;
    (void)t;

    edge = keys_scan();
    if (s_over != 0U) {
        if ((edge & (KEY_MASK(KEY_ID_JS) | KEY_MASK(KEY_ID_ENC))) != 0U) new_game();
        return;
    }

    /* 摇杆：方向沿 */
    m = joy_get_mask();
    if (m != s_prev_mask) {
        if ((m & JOY_MASK_LEFT) != 0U)  cursor_move(-1, 0);
        if ((m & JOY_MASK_RIGHT) != 0U) cursor_move(1, 0);
        if ((m & JOY_MASK_UP) != 0U)    cursor_move(0, -1);
        if ((m & JOY_MASK_DOWN) != 0U)  cursor_move(0, 1);
        s_prev_mask = m;
    }

    /* 编码器：左右移动光标 */
    d = encoder_count() - s_last_cnt;
    s_last_cnt += d;
    while (d >= ENC_DETENT)  { d -= ENC_DETENT; cursor_move(1, 0); }
    while (d <= -ENC_DETENT) { d += ENC_DETENT; cursor_move(-1, 0); }

    if ((edge & (KEY_MASK(KEY_ID_JS) | KEY_MASK(KEY_ID_ENC))) != 0U) dig();
    if ((edge & KEY_MASK(KEY_ID_2)) != 0U) toggle_flag();

    lv_obj_invalidate(s_board);
}

static void app_mines_enter(void)
{
    lv_obj_t *scr = lv_scr_act();

    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

    s_board = lv_obj_create(scr);
    lv_obj_remove_style_all(s_board);
    lv_obj_set_size(s_board, MW * MCELL, MH * MCELL);
    lv_obj_set_pos(s_board, MORG_X, MORG_Y);
    lv_obj_clear_flag(s_board, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s_board, mines_draw, LV_EVENT_DRAW_MAIN, NULL);

    s_hud = lv_label_create(scr);
    lv_obj_set_style_text_color(s_hud, lv_color_white(), 0);
    lv_obj_align(s_hud, LV_ALIGN_TOP_MID, 0, 6);

    s_msg = lv_label_create(scr);
    lv_obj_set_style_text_color(s_msg, lv_color_white(), 0);
    lv_obj_set_style_bg_color(s_msg, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_msg, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(s_msg, 6, 0);
    lv_obj_align(s_msg, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(s_msg, LV_OBJ_FLAG_HIDDEN);

    new_game();
    s_tm = lv_timer_create(mines_cb, PERIOD, NULL);
}

static void app_mines_exit(void)
{
    if (s_tm != NULL) { lv_timer_del(s_tm); s_tm = NULL; }
}

const app_t app_mines = { "扫雷", app_mines_enter, app_mines_exit };
