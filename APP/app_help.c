/**
 * @file app_help.c
 * @brief 说明 app：分页展示各 APP 的使用方法（指南针校准、游戏玩法等）。
 *   KEY4 / 摇杆右 / 编码器顺时针 = 下一页
 *   KEY3 / 摇杆左 / 编码器逆时针 = 上一页
 *   KEY1 = 返回上一级菜单
 */
#include "app_help.h"
#include "lvgl.h"
#include "keys.h"
#include "encoder.h"
#include "joystick.h"

#define PAGE_N       16
#define ENC_DETENT   4
#define QR_TEXT      "https://gitee.com/GAOTINGXIANG/myclass"

static lv_timer_t *s_tm;
static lv_obj_t   *s_title, *s_body, *s_idx, *s_qr;
static uint8_t     s_page;
static int32_t     s_last_cnt;
static joy_dir_t   s_prev_dir = JOY_DIR_NONE;

static const char *const PAGE_TITLE[PAGE_N] = {
    "操作",
    "指南针",
    "小恐龙跳跃",
    "打地鼠",
    "大鱼小鱼",
    "坦克射击",
    "黄金矿工",
    "切水果",
    "飞行射击",
    "滑雪冒险",
    "台球",
    "地铁跑酷",
    "狙击外星",
    "游戏",
    "音效",
    "关于",
};

static const char *const PAGE_BODY[PAGE_N] = {
    "编码器旋转\n切换菜单\n\n"
    "摇杆左右\n同样切换\n\n"
    "KEY2 确认进入\nKEY1 返回上一级",

    "按 KEY2 开始校准\n"
    "水平旋转模块一周\n"
    "再按 KEY2 结束\n\n"
    "指针固定朝上\n"
    "刻度盘随方位转动\n"
    "正北时蜂鸣一声",

    "手靠近超声波\n"
    "小于 12 厘米起跳\n"
    "退到 20 厘米才能再跳\n\n"
    "编码器旋转调速度\n"
    "KEY4 加速 KEY3 减速\n"
    "摇杆推一下也能跳",

    "手离传感器越近\n"
    "锤子越靠左\n"
    "4 厘米最左 40 厘米最右\n\n"
    "摇杆左右移动锤子\n"
    "编码器旋转同效\n"
    "KEY4 右移 KEY3 左移",

    "摇杆或编码器\n"
    "左右移动\n\n"
    "吃小鱼变大\n"
    "每吃三条长一级\n\n"
    "碰到大鱼扣一点\n"
    "生命归零结束",

    "摇杆或编码器\n"
    "左右开车\n\n"
    "按 KEY2 开炮\n"
    "打中敌坦克加分\n\n"
    "被敌方炮弹打中\n"
    "扣一点生命",

    "矿钩自动摆动\n"
    "按 KEY2 下钩\n\n"
    "钩到金块得分\n"
    "金块越大分越高\n\n"
    "时间用完结束\n"
    "抓完一轮会刷新",

    "摇杆或编码器\n"
    "左右移动刀锋\n"
    "按 KEY2 挥刀\n\n"
    "切到水果得十分\n"
    "切到炸弹扣一点\n\n"
    "水果落地不扣分",

    "摇杆或编码器\n"
    "左右移动\n"
    "飞机自动开火\n\n"
    "打落敌机加分\n"
    "撞到敌机扣一点",

    "摇杆或编码器\n"
    "左右躲树\n\n"
    "撞树扣一点\n"
    "跑得越远越快\n"
    "得分越多",

    "编码器旋转瞄准\n"
    "KEY3 KEY4 也可\n\n"
    "按 KEY2 击球\n"
    "彩球进洞得十分\n\n"
    "白球进洞扣一点\n"
    "彩球清空会补球",

    "三条赛道\n"
    "摇杆左右换道\n"
    "编码器同效\n\n"
    "撞到障碍扣一点\n"
    "越跑越快",

    "摇杆移动准星\n"
    "上下左右都行\n"
    "编码器左右移动\n\n"
    "按 KEY2 开枪\n"
    "打中外星人加分\n"
    "外星人跑掉扣一点",

    "方向类游戏用摇杆\n"
    "确认键开始或重开\n"
    "KEY1 返回上一级\n\n"
    "LED 灯显示生命\n"
    "撞到扣一点\n"
    "归零即游戏结束",

    "系统信息页\n"
    "按 KEY2 开关音效\n"
    "默认开启\n\n"
    "关闭后蜂鸣器静音\n"
    "LED 照常工作",

    "扫码",
};

static void page_show(uint8_t p)
{
    s_page = p;
    lv_label_set_text(s_title, PAGE_TITLE[p]);
    lv_label_set_text(s_body, PAGE_BODY[p]);
    lv_label_set_text_fmt(s_idx, "%u/%u", (unsigned)(p + 1U), (unsigned)PAGE_N);

    /* 末页：正文下沉到底部当二维码图注，显示二维码 */
    if (p == (PAGE_N - 1U)) {
        lv_obj_align(s_body, LV_ALIGN_BOTTOM_MID, 0, -36);
        lv_obj_clear_flag(s_qr, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_align(s_body, LV_ALIGN_CENTER, 0, 8);
        lv_obj_add_flag(s_qr, LV_OBJ_FLAG_HIDDEN);
    }
}

static void help_cb(lv_timer_t *t)
{
    uint8_t edge;
    int32_t move = 0;
    (void)t;

    edge = keys_scan();
    if ((edge & KEY_MASK(KEY_ID_4)) != 0U) move++;
    if ((edge & KEY_MASK(KEY_ID_3)) != 0U) move--;

    /* 编码器 */
    {
        int32_t c = encoder_count();
        int32_t d = c - s_last_cnt;
        s_last_cnt = c;
        while (d >= ENC_DETENT)  { d -= ENC_DETENT; move++; }
        while (d <= -ENC_DETENT) { d += ENC_DETENT; move--; }
    }

    /* 摇杆方向沿 */
    {
        joy_dir_t dir = joy_get_dir();
        if (dir != s_prev_dir) {
            if (dir == JOY_DIR_RIGHT || dir == JOY_DIR_DOWN ||
                dir == JOY_DIR_DOWN_RIGHT) {
                move++;
            } else if (dir == JOY_DIR_LEFT || dir == JOY_DIR_UP ||
                       dir == JOY_DIR_UP_LEFT) {
                move--;
            }
            s_prev_dir = dir;
        }
    }

    if (move != 0) {
        int32_t p = (int32_t)s_page + move;
        while (p < 0) p += PAGE_N;
        page_show((uint8_t)((uint32_t)p % PAGE_N));
    }
}

static void app_help_enter(void)
{
    lv_obj_t *scr = lv_scr_act();

    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

    s_title = lv_label_create(scr);
    lv_obj_set_style_text_color(s_title, lv_color_white(), 0);
    lv_obj_align(s_title, LV_ALIGN_TOP_MID, 0, 24);

    s_body = lv_label_create(scr);
    lv_obj_set_width(s_body, 186);
    lv_obj_set_style_text_color(s_body, lv_color_white(), 0);
    lv_obj_set_style_text_align(s_body, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_line_space(s_body, 3, 0);
    lv_label_set_long_mode(s_body, LV_LABEL_LONG_WRAP);
    lv_obj_align(s_body, LV_ALIGN_CENTER, 0, 8);

    s_idx = lv_label_create(scr);
    lv_obj_set_style_text_color(s_idx, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_align(s_idx, LV_ALIGN_BOTTOM_MID, 0, -20);

    /* 末页二维码：黑模块白底，白底 pad 充当静区 */
    s_qr = lv_qrcode_create(scr, 100, lv_color_black(), lv_color_white());
    lv_qrcode_update(s_qr, QR_TEXT, (uint32_t)(sizeof(QR_TEXT) - 1U));
    lv_obj_set_style_bg_color(s_qr, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(s_qr, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(s_qr, 8, 0);
    lv_obj_align(s_qr, LV_ALIGN_CENTER, 0, 4);
    lv_obj_add_flag(s_qr, LV_OBJ_FLAG_HIDDEN);

    s_last_cnt = encoder_count();
    s_prev_dir = joy_get_dir();
    page_show(0);
    s_tm = lv_timer_create(help_cb, 30, NULL);
}

static void app_help_exit(void)
{
    if (s_tm != NULL) { lv_timer_del(s_tm); s_tm = NULL; }
}

const app_t app_help = { "说明", app_help_enter, app_help_exit };
