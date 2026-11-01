/**
 * @file demo_compass.c
 * @brief 指南针 demo（HMC5883L，旋转刻度盘样式）
 *
 * 界面：红色指北针固定朝上（代表机头方向），N/E/S/W 字母与 12 个刻度
 *       组成的刻度盘随方位角反向旋转——机头转向哪边，对应方位的
 *       字母就转到指针下方。圆心下方显示方位角（0~359）与 8 方位缩写。
 *
 * 操作：KEY3 开始/结束校准（校准时全方位旋转模块）。
 * 采样：每 100 ms 读一次磁力计；读取失败显示 "--" 并冻结表盘。
 * 注意：水平放置时方位角才准确，倾斜未做补偿。
 */

#include "demo_compass.h"
#include "lvgl.h"
#include "hmc5883l.h"
#include "keys.h"

/*---------------- 可调参数（修改后重新编译生效） ----------------*/

#define COMPASS_CX          120
#define COMPASS_CY          120
#define COMPASS_RING_D      222
#define COMPASS_RING_W      3
#define COMPASS_TICK_OUT    104
#define COMPASS_TICK_IN     96
#define COMPASS_TICK_W      2
#define COMPASS_CARD_R      74      /* N/E/S/W 字母所在半径 */
#define COMPASS_NEEDLE_LEN  88      /* 指针长度 */
#define COMPASS_NEEDLE_W    5       /* 指针线宽 */
#define COMPASS_TAIL_LEN    30      /* 针尾长度 */
#define COMPASS_HUB_D       10
#define COMPASS_TEXT_DY     40      /* 数字相对圆心垂直偏移 */
#define COMPASS_PERIOD      100     /* 采样周期（ms） */

/*---------------------------------------------------------------*/

static lv_obj_t *text_lbl;

/* 旋转刻度盘元素 */
static lv_obj_t  *tick_lines[12];
static lv_point_t tick_pts[12][2];
static lv_obj_t  *card_lbl[4];

/* 校准状态：KEY3 开始/结束，校准时全方位旋转模块 */
static uint8_t  calibrating = 0;
static uint32_t cal_count   = 0;

/* 8 方位缩写 */
static const char *const card8[8] = { "N", "NE", "E", "SE", "S", "SW", "W", "NW" };

/* 圆周取点：deg 为从 12 点方向顺时针的角度 */
static void angle_point(lv_coord_t r, int16_t deg, lv_point_t *p)
{
    int32_t s = lv_trigo_sin(deg);
    int32_t c = lv_trigo_cos(deg);
    p->x = (lv_coord_t)(COMPASS_CX + (r * s) / LV_TRIGO_SIN_MAX);
    p->y = (lv_coord_t)(COMPASS_CY - (r * c) / LV_TRIGO_SIN_MAX);
}

/* 采样 + 刷新 */
static void compass_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    /* KEY3：开始 / 结束校准 */
    if (keys_scan() & KEY_MASK(KEY_ID_3)) {
        if (!calibrating) {
            calibrating = 1;
            cal_count = 0;
            hmc5883l_calib_reset();
            lv_label_set_text(text_lbl, "CAL");
        } else {
            calibrating = 0;
            lv_label_set_text(text_lbl,
                (hmc5883l_calib_finish() == HMC5883L_OK) ? "CAL OK" : "CAL ERR");
            return;    /* 结果显示一帧，下个周期恢复正常显示 */
        }
    }

    hmc5883l_data_t d;
    if (hmc5883l_read(&d) != HMC5883L_OK) {
        lv_label_set_text(text_lbl, "--");
        return;
    }

    /* 校准中：喂样本、显示进度，表盘冻结 */
    if (calibrating) {
        hmc5883l_calib_feed(&d);
        cal_count++;
        lv_label_set_text_fmt(text_lbl, "CAL %u", (unsigned)cal_count);
        return;
    }

    int16_t h10 = hmc5883l_heading_deg10(&d);   /* 0~3599，0.1 度 */
    int16_t deg = (int16_t)((h10 + 5) / 10);    /* 四舍五入到整度 */

    /* 表盘反向旋转：方位角为 H 时，盘上角度 A 的刻度转到 A - H 位置 */
    int16_t rot = (int16_t)((360 - deg) % 360);

    /* 12 个刻度重算端点 */
    for (uint8_t i = 0; i < 12U; i++) {
        int16_t pos = (int16_t)((i * 30 + rot) % 360);
        angle_point(COMPASS_TICK_OUT, pos, &tick_pts[i][0]);
        angle_point(COMPASS_TICK_IN,  pos, &tick_pts[i][1]);
        lv_line_set_points(tick_lines[i], tick_pts[i], 2);
    }

    /* N/E/S/W 字母跟随表盘（保持正立，只挪位置） */
    for (uint8_t i = 0; i < 4U; i++) {
        lv_point_t p;
        int16_t pos = (int16_t)((i * 90 + rot) % 360);
        angle_point(COMPASS_CARD_R, pos, &p);
        lv_obj_set_pos(card_lbl[i], p.x - 15, p.y - 15);
    }

    /* 数字 + 8 方位 */
    uint8_t ci = (uint8_t)(((int32_t)h10 + 225) / 450 % 8);
    lv_label_set_text_fmt(text_lbl, "%d %s", (int)deg, card8[ci]);
}

/* 静态部分：圆环 + 固定指针 + 中心圆点 */
static void dial_create(void)
{
    /* 外圈圆环（各向对称，不随表盘转） */
    lv_obj_t *ring = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(ring);
    lv_obj_set_size(ring, COMPASS_RING_D, COMPASS_RING_D);
    lv_obj_center(ring);
    lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(ring, COMPASS_RING_W, 0);
    lv_obj_set_style_border_color(ring, lv_palette_main(LV_PALETTE_BLUE_GREY), 0);
    lv_obj_clear_flag(ring, LV_OBJ_FLAG_SCROLLABLE);

    /* 12 个刻度（初始位置，刷新时重算） */
    for (uint8_t i = 0; i < 12U; i++) {
        int16_t deg = (int16_t)(i * 30U);
        angle_point(COMPASS_TICK_OUT, deg, &tick_pts[i][0]);
        angle_point(COMPASS_TICK_IN,  deg, &tick_pts[i][1]);

        tick_lines[i] = lv_line_create(lv_scr_act());
        lv_line_set_points(tick_lines[i], tick_pts[i], 2);
        lv_obj_set_style_line_width(tick_lines[i], COMPASS_TICK_W, 0);
        lv_obj_set_style_line_color(tick_lines[i], lv_palette_main(LV_PALETTE_GREY), 0);
    }

    /* N / E / S / W 方位字母（N 红色，初始正位） */
    static const char *const card_txt[4] = { "N", "E", "S", "W" };
    for (uint8_t i = 0; i < 4U; i++) {
        lv_point_t p;
        angle_point(COMPASS_CARD_R, (int16_t)(i * 90U), &p);

        card_lbl[i] = lv_label_create(lv_scr_act());
        lv_label_set_text(card_lbl[i], card_txt[i]);
        lv_obj_set_size(card_lbl[i], 30, 30);
        lv_obj_set_style_text_align(card_lbl[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(card_lbl[i],
            (i == 0U) ? lv_palette_main(LV_PALETTE_RED) : lv_color_white(), 0);
        lv_obj_set_pos(card_lbl[i], p.x - 15, p.y - 15);
    }

    /* 固定指北针：红色朝上（机头方向），白色针尾朝下 */
    static lv_point_t needle_pts[2] = {
        { COMPASS_CX, COMPASS_CY },
        { COMPASS_CX, COMPASS_CY - COMPASS_NEEDLE_LEN },
    };
    lv_obj_t *needle = lv_line_create(lv_scr_act());
    lv_line_set_points(needle, needle_pts, 2);
    lv_obj_set_style_line_width(needle, COMPASS_NEEDLE_W, 0);
    lv_obj_set_style_line_color(needle, lv_palette_main(LV_PALETTE_RED), 0);
    lv_obj_set_style_line_rounded(needle, true, 0);

    static lv_point_t tail_pts[2] = {
        { COMPASS_CX, COMPASS_CY },
        { COMPASS_CX, COMPASS_CY + COMPASS_TAIL_LEN },
    };
    lv_obj_t *tail = lv_line_create(lv_scr_act());
    lv_line_set_points(tail, tail_pts, 2);
    lv_obj_set_style_line_width(tail, COMPASS_NEEDLE_W, 0);
    lv_obj_set_style_line_color(tail, lv_color_white(), 0);
    lv_obj_set_style_line_rounded(tail, true, 0);

    /* 中心圆点 */
    lv_obj_t *hub = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(hub);
    lv_obj_set_size(hub, COMPASS_HUB_D, COMPASS_HUB_D);
    lv_obj_center(hub);
    lv_obj_set_style_radius(hub, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(hub, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(hub, lv_color_white(), 0);
    lv_obj_clear_flag(hub, LV_OBJ_FLAG_SCROLLABLE);

    /* 方位角数字 */
    text_lbl = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_color(text_lbl, lv_color_white(), 0);
    lv_obj_align(text_lbl, LV_ALIGN_CENTER, 0, COMPASS_TEXT_DY);
    lv_label_set_text(text_lbl, "--");
}

void demo_compass_init(void)
{
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_black(), 0);

    dial_create();

    lv_timer_create(compass_timer_cb, COMPASS_PERIOD, NULL);
}
