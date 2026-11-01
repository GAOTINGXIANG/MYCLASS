/**
 * @file demo_clock.c
 * @brief 钟表 demo：表盘 + 时/分/秒针 + 编码器与按键调时
 *
 * 界面：240x240 圆屏，圆心 (120,120)。表盘为 222 px 圆环，
 *       12 个刻度、12/3/6/9 数字、时/分/秒三根指针、中心圆点，
 *       表盘下方显示数字时间。
 *
 * 操作：
 *   - 编码器旋转 / KEY1(-1) / KEY2(+1)：调整当前选中的字段
 *   - ENCODER_KEY / KEY3 / JS_KEY：切换字段，顺序 时 → 分 → 秒 → 走时
 *   - 选中的指针变黄色，数字时间同步变黄色；回到“走时”后恢复本色
 *
 * 输入读取方式：编码器用 encoder_count() 绝对计数做差，
 *              按键用 keys_scan() 取按下沿，均不与已注册的 LVGL indev 抢数据。
 */

#include "demo_clock.h"
#include "lvgl.h"
#include "keys.h"
#include "encoder.h"

/*---------------- 可调参数（修改后重新编译生效） ----------------*/

#define CLOCK_CX           120     /* 圆心 X */
#define CLOCK_CY           120     /* 圆心 Y */
#define CLOCK_RING_D       222     /* 表盘圆环直径 */
#define CLOCK_RING_W       3       /* 圆环线宽 */
#define CLOCK_TICK_OUT     104     /* 刻度外端半径 */
#define CLOCK_TICK_IN      93      /* 刻度内端半径 */
#define CLOCK_TICK_W       2       /* 刻度线宽 */
#define CLOCK_NUM_R        74      /* 数字距圆心半径 */
#define CLOCK_HOUR_LEN     58      /* 时针长度 */
#define CLOCK_MIN_LEN      84      /* 分针长度 */
#define CLOCK_SEC_LEN       94      /* 秒针长度 */
#define CLOCK_HOUR_W       6       /* 时针线宽 */
#define CLOCK_MIN_W        3       /* 分针线宽 */
#define CLOCK_SEC_W        2       /* 秒针线宽 */
#define CLOCK_HUB_D        10      /* 中心圆点直径 */
#define CLOCK_DIGITAL_DY   40      /* 数字时间相对圆心的垂直偏移 */
#define CLOCK_INPUT_PERIOD 50      /* 输入扫描周期（ms） */

/*---------------------------------------------------------------*/

/* 编辑目标 */
enum {
    EDIT_HOUR = 0,
    EDIT_MIN,
    EDIT_SEC,
    EDIT_RUN          /* 走时（不调） */
};

static uint8_t hour   = 12U;
static uint8_t minute = 0U;
static uint8_t second = 0U;
static uint8_t editing = EDIT_RUN;

static lv_obj_t *hour_hand;
static lv_obj_t *min_hand;
static lv_obj_t *sec_hand;
static lv_obj_t *digital_lbl;

static lv_point_t hour_pts[2];
static lv_point_t min_pts[2];
static lv_point_t sec_pts[2];

/* 圆周取点：deg 为从 12 点方向顺时针的角度 */
static void angle_point(lv_coord_t r, int16_t deg, lv_point_t *p)
{
    int32_t s = lv_trigo_sin(deg);
    int32_t c = lv_trigo_cos(deg);
    p->x = (lv_coord_t)(CLOCK_CX + (r * s) / LV_TRIGO_SIN_MAX);
    p->y = (lv_coord_t)(CLOCK_CY - (r * c) / LV_TRIGO_SIN_MAX);
}

/* 取模到 [0, range)（负数也能正确折回） */
static uint8_t wrap_i(int32_t v, uint8_t range)
{
    int32_t m = v % (int32_t)range;
    if (m < 0) {
        m += (int32_t)range;
    }
    return (uint8_t)m;
}

/* 刷新指针与数字时间 */
static void clock_refresh(void)
{
    int16_t a_h = (int16_t)((int32_t)(hour % 12U) * 30 + minute / 2);
    int16_t a_m = (int16_t)(minute * 6U);
    int16_t a_s = (int16_t)(second * 6U);

    lv_color_t c_yellow = lv_palette_main(LV_PALETTE_YELLOW);
    lv_color_t c_white  = lv_color_white();

    lv_obj_set_style_line_color(hour_hand, (editing == EDIT_HOUR) ? c_yellow : c_white, 0);
    lv_obj_set_style_line_color(min_hand,  (editing == EDIT_MIN)  ? c_yellow : c_white, 0);
    lv_obj_set_style_line_color(sec_hand,  (editing == EDIT_SEC)  ? c_yellow
                                                              : lv_palette_main(LV_PALETTE_RED), 0);

    angle_point(CLOCK_HOUR_LEN, a_h, &hour_pts[1]);
    angle_point(CLOCK_MIN_LEN,  a_m, &min_pts[1]);
    angle_point(CLOCK_SEC_LEN,  a_s, &sec_pts[1]);

    lv_line_set_points(hour_hand, hour_pts, 2);
    lv_line_set_points(min_hand,  min_pts,  2);
    lv_line_set_points(sec_hand,  sec_pts,  2);

    lv_label_set_text_fmt(digital_lbl, "%02d:%02d:%02d",
                          (int)hour, (int)minute, (int)second);
    lv_obj_set_style_text_color(digital_lbl,
                                (editing == EDIT_RUN) ? c_white : c_yellow, 0);
}

/* 编码器增量（基于绝对计数，与 LVGL 编码器 indev 互不干扰） */
static int32_t encoder_step(void)
{
    static int32_t last = 0;
    static uint8_t inited = 0;

    int32_t now = encoder_count();
    if (!inited) {
        inited = 1;
        last = now;
        return 0;
    }
    int32_t d = now - last;
    last = now;
    return d;
}

/* 输入扫描 */
static void input_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    int32_t step = encoder_step();
    uint8_t edges = keys_scan();

    /* 切换编辑字段：时 → 分 → 秒 → 走时 */
    if (edges & (KEY_MASK(KEY_ID_ENC) | KEY_MASK(KEY_ID_3) | KEY_MASK(KEY_ID_JS))) {
        editing = (uint8_t)((editing + 1U) % 4U);
        clock_refresh();
    }

    /* 方向键微调 */
    if (edges & KEY_MASK(KEY_ID_2)) {
        step += 1;
    }
    if (edges & KEY_MASK(KEY_ID_1)) {
        step -= 1;
    }

    if ((step != 0) && (editing != EDIT_RUN)) {
        switch (editing) {
        case EDIT_HOUR: hour   = (uint8_t)(wrap_i((int32_t)hour - 1 + step, 12) + 1); break;
        case EDIT_MIN:  minute = wrap_i((int32_t)minute + step, 60); break;
        case EDIT_SEC:  second = wrap_i((int32_t)second + step, 60); break;
        default: break;
        }
        clock_refresh();
    }
}

/* 走时：仅“走时”模式自动进位 */
static void time_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    if (editing == EDIT_RUN) {
        second++;
        if (second >= 60U) {
            second = 0U;
            minute++;
            if (minute >= 60U) {
                minute = 0U;
                hour = (uint8_t)((hour % 12U) + 1U);
            }
        }
        clock_refresh();
    }
}

/* 表盘：圆环 + 刻度 + 数字 + 指针 */
static void dial_create(void)
{
    /* 外圈圆环 */
    lv_obj_t *ring = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(ring);
    lv_obj_set_size(ring, CLOCK_RING_D, CLOCK_RING_D);
    lv_obj_center(ring);
    lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(ring, CLOCK_RING_W, 0);
    lv_obj_set_style_border_color(ring, lv_palette_main(LV_PALETTE_BLUE_GREY), 0);
    lv_obj_clear_flag(ring, LV_OBJ_FLAG_SCROLLABLE);

    /* 12 个刻度（静态点数组，LVGL 只保存指针） */
    static lv_point_t tick_pts[12][2];
    for (uint8_t i = 0; i < 12U; i++) {
        int16_t deg = (int16_t)(i * 30U);
        angle_point(CLOCK_TICK_OUT, deg, &tick_pts[i][0]);
        angle_point(CLOCK_TICK_IN,  deg, &tick_pts[i][1]);

        lv_obj_t *tick = lv_line_create(lv_scr_act());
        lv_line_set_points(tick, tick_pts[i], 2);
        lv_obj_set_style_line_width(tick, CLOCK_TICK_W, 0);
        lv_obj_set_style_line_color(tick, lv_palette_main(LV_PALETTE_GREY), 0);
        lv_obj_set_style_line_rounded(tick, true, 0);
    }

    /* 12 / 3 / 6 / 9 数字 */
    static const struct {
        const char *txt;
        int16_t     deg;
    } nums[] = {
        { "12",   0 },
        { "3",   90 },
        { "6",  180 },
        { "9",  270 },
    };

    for (uint8_t i = 0; i < (uint8_t)(sizeof(nums) / sizeof(nums[0])); i++) {
        lv_point_t p;
        angle_point(CLOCK_NUM_R, nums[i].deg, &p);

        lv_obj_t *num = lv_label_create(lv_scr_act());
        lv_label_set_text(num, nums[i].txt);
        lv_obj_set_size(num, 30, 30);
        lv_obj_set_style_text_align(num, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(num, lv_color_white(), 0);
        lv_obj_set_pos(num, p.x - 15, p.y - 15);
    }

    /* 三根指针：起点固定在圆心 */
    hour_pts[0].x = CLOCK_CX;  hour_pts[0].y = CLOCK_CY;
    min_pts[0].x  = CLOCK_CX;  min_pts[0].y  = CLOCK_CY;
    sec_pts[0].x  = CLOCK_CX;  sec_pts[0].y  = CLOCK_CY;
    hour_pts[1] = hour_pts[0];
    min_pts[1]  = min_pts[0];
    sec_pts[1]  = sec_pts[0];

    hour_hand = lv_line_create(lv_scr_act());
    lv_obj_set_style_line_width(hour_hand, CLOCK_HOUR_W, 0);
    lv_obj_set_style_line_rounded(hour_hand, true, 0);
    lv_line_set_points(hour_hand, hour_pts, 2);

    min_hand = lv_line_create(lv_scr_act());
    lv_obj_set_style_line_width(min_hand, CLOCK_MIN_W, 0);
    lv_obj_set_style_line_rounded(min_hand, true, 0);
    lv_line_set_points(min_hand, min_pts, 2);

    sec_hand = lv_line_create(lv_scr_act());
    lv_obj_set_style_line_width(sec_hand, CLOCK_SEC_W, 0);
    lv_obj_set_style_line_rounded(sec_hand, true, 0);
    lv_obj_set_style_line_color(sec_hand, lv_palette_main(LV_PALETTE_RED), 0);
    lv_line_set_points(sec_hand, sec_pts, 2);

    /* 中心圆点 */
    lv_obj_t *hub = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(hub);
    lv_obj_set_size(hub, CLOCK_HUB_D, CLOCK_HUB_D);
    lv_obj_center(hub);
    lv_obj_set_style_radius(hub, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(hub, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(hub, lv_color_white(), 0);
    lv_obj_clear_flag(hub, LV_OBJ_FLAG_SCROLLABLE);

    /* 数字时间 */
    digital_lbl = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_color(digital_lbl, lv_color_white(), 0);
    lv_obj_align(digital_lbl, LV_ALIGN_CENTER, 0, CLOCK_DIGITAL_DY);
}

void demo_clock_init(void)
{
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_black(), 0);

    dial_create();
    clock_refresh();

    lv_timer_create(input_timer_cb, CLOCK_INPUT_PERIOD, NULL);
    lv_timer_create(time_timer_cb, 1000, NULL);
}
