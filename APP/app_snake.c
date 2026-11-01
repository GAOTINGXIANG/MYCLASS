/**
 * @file app_snake.c
 * @brief 贪吃蛇：17x17 网格，活动区为圆屏内接正方形。
 *   摇杆              = 方向
 *   ENCODER_KEY/JS_KEY= 开始 / 死亡后重开
 *   KEY1              = 返回主菜单
 *   6 个 LED 显示生命，撞墙/撞自己扣 1 命并蜂鸣提醒。
 */
#include "app_snake.h"
#include "lvgl.h"
#include "joystick.h"
#include "keys.h"
#include "game_common.h"

#define CELL      10
#define GRID      17
#define ORG       (120 - GRID * CELL / 2)
#define SNAKE_MAX 80
#define PERIOD    220
#define JOY_DEAD  400

static lv_timer_t *s_tm;
static lv_obj_t   *s_cells[SNAKE_MAX];
static lv_obj_t   *s_food, *s_msg;
static int8_t  s_x[SNAKE_MAX + 1], s_y[SNAKE_MAX + 1];
static int8_t  s_fx, s_fy;
static uint8_t s_len, s_shown, s_running, s_over;
static int8_t  s_dx = 1, s_dy, s_ndx = 1, s_ndy;
static uint32_t s_rng;

static uint32_t rng_next(void)
{
    s_rng = s_rng * 1103515245UL + 12345UL;
    return (s_rng >> 16) & 0x7FFFUL;
}

static uint8_t on_snake(int8_t gx, int8_t gy, uint8_t upto)
{
    for (uint8_t i = 0; i < upto; i++) {
        if (s_x[i] == gx && s_y[i] == gy) return 1U;
    }
    return 0U;
}

static void food_place(void)
{
    uint8_t tries = 0;
    do {
        s_fx = (int8_t)(rng_next() % GRID);
        s_fy = (int8_t)(rng_next() % GRID);
    } while (on_snake(s_fx, s_fy, s_len) != 0U && ++tries < 100U);
    lv_obj_set_pos(s_food, ORG + s_fx * CELL + 1, ORG + s_fy * CELL + 1);
}

static void render(void)
{
    for (uint8_t i = s_len; i < s_shown; i++) {
        lv_obj_add_flag(s_cells[i], LV_OBJ_FLAG_HIDDEN);
    }
    for (uint8_t i = 0; i < s_len; i++) {
        lv_obj_set_pos(s_cells[i], ORG + s_x[i] * CELL, ORG + s_y[i] * CELL);
        lv_obj_clear_flag(s_cells[i], LV_OBJ_FLAG_HIDDEN);
    }
    s_shown = s_len;
}

static void round_reset(void)
{
    s_len = 3;
    s_x[0] = GRID / 2;  s_y[0] = GRID / 2;
    s_x[1] = s_x[0] - 1; s_y[1] = s_y[0];
    s_x[2] = s_x[0] - 2; s_y[2] = s_y[0];
    s_dx = 1; s_dy = 0; s_ndx = 1; s_ndy = 0;
    food_place();
}

static void msg_show(const char *txt)
{
    lv_label_set_text(s_msg, txt);
    lv_obj_clear_flag(s_msg, LV_OBJ_FLAG_HIDDEN);
}

static void snake_cb(lv_timer_t *t)
{
    uint8_t edge;
    (void)t;

    /* 摇杆方向（取主导轴，禁止反向） */
    {
        joy_state_t st;
        int8_t nx = s_ndx, ny = s_ndy;
        joy_read(&st);
        if (st.x > JOY_DEAD || st.x < -JOY_DEAD ||
            st.y > JOY_DEAD || st.y < -JOY_DEAD) {
            if (st.x > JOY_DEAD || st.x < -JOY_DEAD) {
                if (st.y > JOY_DEAD || st.y < -JOY_DEAD) {
                    /* 斜向：取绝对值大的轴 */
                    if ((st.x > 0 ? st.x : -st.x) > (st.y > 0 ? st.y : -st.y)) {
                        nx = (st.x > 0) ? 1 : -1; ny = 0;
                    } else {
                        nx = 0; ny = (st.y > 0) ? 1 : -1;
                    }
                } else {
                    nx = (st.x > 0) ? 1 : -1; ny = 0;
                }
            } else {
                nx = 0; ny = (st.y > 0) ? 1 : -1;
            }
        }
        if (nx != -s_dx || ny != -s_dy) { s_ndx = nx; s_ndy = ny; }
    }

    edge = keys_scan();
    if ((edge & (KEY_MASK(KEY_ID_JS) | KEY_MASK(KEY_ID_ENC))) != 0U) {
        if (s_over != 0U) {                    /* 游戏结束 → 重开 */
            s_over = 0;
            game_lives_set(6);
            round_reset();
            s_running = 1;
            lv_obj_add_flag(s_msg, LV_OBJ_FLAG_HIDDEN);
        } else if (s_running == 0U) {          /* 首次开始 */
            round_reset();
            s_running = 1;
            lv_obj_add_flag(s_msg, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (s_running == 0U) return;

    /* 移动 */
    s_dx = s_ndx; s_dy = s_ndy;
    int8_t nx = s_x[0] + s_dx;
    int8_t ny = s_y[0] + s_dy;
    uint8_t eat = (nx == s_fx && ny == s_fy) ? 1U : 0U;

    if (nx < 0 || ny < 0 || nx >= GRID || ny >= GRID ||
        on_snake(nx, ny, (uint8_t)(s_len - (eat ? 0U : 1U))) != 0U) {
        if (game_lives_dec() == 0U) {
            s_running = 0;
            s_over = 1;
            msg_show("游戏结束");
        } else {
            round_reset();
        }
        render();
        return;
    }

    for (uint8_t i = s_len; i > 0U; i--) {
        s_x[i] = s_x[i - 1];
        s_y[i] = s_y[i - 1];
    }
    s_x[0] = nx;
    s_y[0] = ny;
    if (eat != 0U && s_len < SNAKE_MAX) {
        s_len++;
        food_place();
    }
    render();
}

static void app_snake_enter(void)
{
    lv_obj_t *scr = lv_scr_act();

    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

    /* 活动区边框 */
    lv_obj_t *bd = lv_obj_create(scr);
    lv_obj_remove_style_all(bd);
    lv_obj_set_size(bd, GRID * CELL, GRID * CELL);
    lv_obj_set_pos(bd, ORG, ORG);
    lv_obj_set_style_border_width(bd, 1, 0);
    lv_obj_set_style_border_color(bd, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_clear_flag(bd, LV_OBJ_FLAG_SCROLLABLE);

    /* 蛇身对象池 */
    for (uint8_t i = 0; i < SNAKE_MAX; i++) {
        s_cells[i] = lv_obj_create(scr);
        lv_obj_remove_style_all(s_cells[i]);
        lv_obj_set_size(s_cells[i], CELL - 1, CELL - 1);
        lv_obj_set_style_radius(s_cells[i], 2, 0);
        lv_obj_set_style_bg_opa(s_cells[i], LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(s_cells[i], lv_palette_main(LV_PALETTE_GREEN), 0);
        lv_obj_add_flag(s_cells[i], LV_OBJ_FLAG_HIDDEN);
    }
    s_shown = 0;

    /* 食物 */
    s_food = lv_obj_create(scr);
    lv_obj_remove_style_all(s_food);
    lv_obj_set_size(s_food, CELL - 2, CELL - 2);
    lv_obj_set_style_radius(s_food, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(s_food, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(s_food, lv_palette_main(LV_PALETTE_RED), 0);

    /* 提示文本 */
    s_msg = lv_label_create(scr);
    lv_obj_set_style_text_color(s_msg, lv_color_white(), 0);
    lv_obj_align(s_msg, LV_ALIGN_CENTER, 0, 0);
    msg_show("按确认键开始");

    s_rng = lv_tick_get() | 1UL;
    s_running = 0;
    s_over = 0;
    game_lives_set(6);
    s_tm = lv_timer_create(snake_cb, PERIOD, NULL);
}

static void app_snake_exit(void)
{
    if (s_tm != NULL) { lv_timer_del(s_tm); s_tm = NULL; }
    game_lives_clear();
}

const app_t app_snake = { "贪吃蛇", app_snake_enter, app_snake_exit };
