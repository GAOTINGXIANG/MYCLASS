/**
 * @file demo_sensor.c
 * @brief 温湿度 + 超声波测距显示 demo
 *
 * 界面：三行大字（TEMP / HUMI / DIST），全部位于 240x240 圆屏的内接安全区。
 * 采样：DHT11 每 1 s 一次（传感器本身限速 1 Hz），HC-SR04 每 250 ms 一次。
 *
 * 注意：两个驱动都是阻塞式（DHT11 约 5 ms，HC-SR04 无回波时最长约 40 ms），
 *       采样发生在 LVGL 任务内，极端情况下会短暂影响刷新，演示场景可接受；
 *       正式界面建议把采样挪到独立任务，只把结果变量交给这里显示。
 */

#include "demo_sensor.h"
#include "lvgl.h"
#include "dht11.h"
#include "hcsr04.h"

/*---------------- 可调参数（修改后重新编译生效） ----------------*/

#define DEMO_SENSOR_DHT_PERIOD    1000    /* 温湿度采样周期（ms），不可低于 1000 */
#define DEMO_SENSOR_DIST_PERIOD   250     /* 测距采样周期（ms） */
#define DEMO_SENSOR_DIST_MAX      400     /* 超过该值视为超出量程（cm） */

/* 三行文字的坐标（圆屏安全区内） */
#define DEMO_SENSOR_LABEL_X       50
#define DEMO_SENSOR_VALUE_X       100
#define DEMO_SENSOR_VALUE_W       105
#define DEMO_SENSOR_ROW0_Y        40
#define DEMO_SENSOR_ROW1_Y        92
#define DEMO_SENSOR_ROW2_Y        144

/*---------------------------------------------------------------*/

static lv_obj_t *val_temp;
static lv_obj_t *val_humi;
static lv_obj_t *val_dist;

/* 建一行：左侧灰色小标签 + 右侧白色大数值 */
static void make_row(const char *name, lv_coord_t y_label, lv_coord_t y_value, lv_obj_t **val_out)
{
    lv_obj_t *name_lbl = lv_label_create(lv_scr_act());
    lv_label_set_text(name_lbl, name);
    lv_obj_set_style_text_color(name_lbl, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_set_pos(name_lbl, DEMO_SENSOR_LABEL_X, y_label);

    lv_obj_t *v = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(v, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(v, lv_color_white(), 0);
    lv_obj_set_style_text_align(v, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_width(v, DEMO_SENSOR_VALUE_W);
    lv_obj_set_pos(v, DEMO_SENSOR_VALUE_X, y_value);
    lv_label_set_text(v, "--");

    *val_out = v;
}

/* 温湿度采样 */
static void dht_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    dht11_data_t d;
    if (dht11_read(&d) == DHT11_OK) {
        lv_label_set_text_fmt(val_temp, "%d C", (int)d.temperature);
        lv_label_set_text_fmt(val_humi, "%d %%", (int)d.humidity);
    } else {
        /* 读取失败（未接、上拉不足、时序被打断）显示占位 */
        lv_label_set_text(val_temp, "--");
        lv_label_set_text(val_humi, "--");
    }
}

/* 距离采样 */
static void dist_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    uint16_t cm = 0;
    if ((hcsr04_measure_cm(&cm) == HCSR04_OK) && (cm >= 2U) && (cm <= DEMO_SENSOR_DIST_MAX)) {
        lv_label_set_text_fmt(val_dist, "%u cm", (unsigned)cm);
    } else {
        lv_label_set_text(val_dist, "--");
    }
}

void demo_sensor_init(void)
{
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_black(), 0);

    make_row("TEMP", DEMO_SENSOR_ROW0_Y + 7, DEMO_SENSOR_ROW0_Y, &val_temp);
    make_row("HUMI", DEMO_SENSOR_ROW1_Y + 7, DEMO_SENSOR_ROW1_Y, &val_humi);
    make_row("DIST", DEMO_SENSOR_ROW2_Y + 7, DEMO_SENSOR_ROW2_Y, &val_dist);

    /* 先立刻采一次距离，温湿度等 1 s（DHT11 上电需稳定时间） */
    dist_timer_cb(NULL);

    lv_timer_create(dht_timer_cb,  DEMO_SENSOR_DHT_PERIOD,  NULL);
    lv_timer_create(dist_timer_cb, DEMO_SENSOR_DIST_PERIOD, NULL);
}
