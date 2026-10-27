/**
 * @file joystick_lcd_demo.c
 * @brief 摇杆 + GC9A01 联动演示实现
 */

#include "joystick_lcd_demo.h"
#include "joystick.h"
#include "gc9a01.h"
#include "cmsis_os.h"

/* 圆屏几何参数 */
#define DEMO_CX             120U
#define DEMO_CY             120U
#define DEMO_RING_OUT       116U    /* 外圈半径 */
#define DEMO_RING_IN        111U    /* 外圈内边（可见圆边界） */
#define DEMO_IND_R          98U     /* 方向指示点距圆心距离 */
#define DEMO_IND_DOT_R      5U      /* 方向指示点半径 */
#define DEMO_FIELD_R        78       /* 小球活动半径（圆屏内留出边距） */
#define DEMO_DOT_R          7U      /* 小球半径 */

/* 摇杆归一化值 -> 每次移动像素数（1000 / 40 = 25 px/帧 上限） */
#define DEMO_STEP_DIV       40

#define DEMO_LOOP_MS        40U

/* 颜色 */
#define DEMO_COLOR_RING     0x2104U     /* 深灰外圈 */
#define DEMO_COLOR_IDLE     0x4228U     /* 指示点未激活 */
#define DEMO_COLOR_ACTIVE   WHITE       /* 指示点激活 */
#define DEMO_COLOR_DOT      CYAN        /* 小球 */

/* 整数开方（牛顿迭代），用于把小球限制在圆内 */
static int32_t demo_isqrt(int32_t v)
{
    int32_t x;
    int32_t y;

    if (v <= 0) {
        return 0;
    }

    x = v;
    y = (x + 1) / 2;
    while (y < x) {
        x = y;
        y = ((v / x) + x) / 2;
    }

    return x;
}

/* 把小球位置限制在半径 DEMO_FIELD_R 的圆内 */
static void demo_clamp_to_field(int16_t *px, int16_t *py)
{
    int32_t dx = (int32_t)(*px) - (int32_t)DEMO_CX;
    int32_t dy = (int32_t)(*py) - (int32_t)DEMO_CY;
    int32_t d2 = (dx * dx) + (dy * dy);

    if (d2 <= ((int32_t)DEMO_FIELD_R * DEMO_FIELD_R)) {
        return;
    }

    {
        int32_t d = demo_isqrt(d2);
        *px = (int16_t)((int32_t)DEMO_CX + (dx * DEMO_FIELD_R) / d);
        *py = (int16_t)((int32_t)DEMO_CY + (dy * DEMO_FIELD_R) / d);
    }
}

/* 按方向画一个指示点：on 非 0 为激活色 */
static void demo_set_indicator(joy_mask_t dir_bit, uint8_t on)
{
    uint16_t color = (on != 0U) ? DEMO_COLOR_ACTIVE : DEMO_COLOR_IDLE;

    switch (dir_bit) {
    case JOY_MASK_UP:
        gc9a01_fill_circle(DEMO_CX, (uint16_t)(DEMO_CY - DEMO_IND_R), DEMO_IND_DOT_R, color);
        break;
    case JOY_MASK_DOWN:
        gc9a01_fill_circle(DEMO_CX, (uint16_t)(DEMO_CY + DEMO_IND_R), DEMO_IND_DOT_R, color);
        break;
    case JOY_MASK_LEFT:
        gc9a01_fill_circle((uint16_t)(DEMO_CX - DEMO_IND_R), DEMO_CY, DEMO_IND_DOT_R, color);
        break;
    case JOY_MASK_RIGHT:
        gc9a01_fill_circle((uint16_t)(DEMO_CX + DEMO_IND_R), DEMO_CY, DEMO_IND_DOT_R, color);
        break;
    default:
        break;
    }
}

/* 画静态界面：底色 + 外圈 + 4 个未激活指示点 */
static void demo_draw_background(void)
{
    gc9a01_fill_screen(BLACK);

    gc9a01_fill_circle(DEMO_CX, DEMO_CY, DEMO_RING_OUT, DEMO_COLOR_RING);
    gc9a01_fill_circle(DEMO_CX, DEMO_CY, DEMO_RING_IN, BLACK);

    demo_set_indicator(JOY_MASK_UP, 0U);
    demo_set_indicator(JOY_MASK_DOWN, 0U);
    demo_set_indicator(JOY_MASK_LEFT, 0U);
    demo_set_indicator(JOY_MASK_RIGHT, 0U);
}

void joystick_lcd_demo_task(void *argument)
{
    joy_state_t st;
    joy_mask_t  last_mask = JOY_MASK_NONE;
    int16_t     px = (int16_t)DEMO_CX;
    int16_t     py = (int16_t)DEMO_CY;

    (void)argument;

    joy_init();          /* 需摇杆居中，内部阻塞约 40 ms */
    gc9a01_init();       /* 内部阻塞约 250 ms */
    demo_draw_background();

    gc9a01_fill_circle((uint16_t)px, (uint16_t)py, DEMO_DOT_R, DEMO_COLOR_DOT);

    for (;;) {
        joy_read(&st);

        /* 方向指示点：只在掩码变化时重画，避免闪烁 */
        if (st.mask != last_mask) {
            if ((st.mask & JOY_MASK_UP) != (last_mask & JOY_MASK_UP)) {
                demo_set_indicator(JOY_MASK_UP, (st.mask & JOY_MASK_UP) != 0U);
            }
            if ((st.mask & JOY_MASK_DOWN) != (last_mask & JOY_MASK_DOWN)) {
                demo_set_indicator(JOY_MASK_DOWN, (st.mask & JOY_MASK_DOWN) != 0U);
            }
            if ((st.mask & JOY_MASK_LEFT) != (last_mask & JOY_MASK_LEFT)) {
                demo_set_indicator(JOY_MASK_LEFT, (st.mask & JOY_MASK_LEFT) != 0U);
            }
            if ((st.mask & JOY_MASK_RIGHT) != (last_mask & JOY_MASK_RIGHT)) {
                demo_set_indicator(JOY_MASK_RIGHT, (st.mask & JOY_MASK_RIGHT) != 0U);
            }
            last_mask = st.mask;
        }

        /* 位置按偏移量成比例移动，两轴独立（可同时移动，即斜向） */
        {
            int16_t dx = ((st.mask & JOY_MASK_LEFT) || (st.mask & JOY_MASK_RIGHT)) ?
                         (int16_t)(st.x / DEMO_STEP_DIV) : 0;
            int16_t dy = ((st.mask & JOY_MASK_UP) || (st.mask & JOY_MASK_DOWN)) ?
                         (int16_t)(st.y / DEMO_STEP_DIV) : 0;
            int16_t nx;
            int16_t ny;

            nx = (int16_t)(px + dx);
            ny = (int16_t)(py + dy);
            demo_clamp_to_field(&nx, &ny);

            if ((nx != px) || (ny != py)) {
                /* 擦除旧点 + 画新点 */
                gc9a01_fill_circle((uint16_t)px, (uint16_t)py, DEMO_DOT_R, BLACK);
                gc9a01_fill_circle((uint16_t)nx, (uint16_t)ny, DEMO_DOT_R, DEMO_COLOR_DOT);
                px = nx;
                py = ny;
            }
        }

        osDelay(DEMO_LOOP_MS);
    }
}
