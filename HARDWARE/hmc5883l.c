/**
 * @file hmc5883l.c
 * @brief HMC5883L 三轴磁力计驱动实现（HAL I2C，阻塞式）
 */

#include "hmc5883l.h"
#include "i2c.h"
#include "main.h"
#include <math.h>

/*---------------- 可调参数（修改后重新编译生效） ----------------*/

#define HMC5883L_I2C_TIMEOUT   100U    /* 单次 I2C 事务超时（ms） */
#define HMC5883L_MAG_DECL_DEG10  -900     /* 磁偏角补偿，单位 0.1 度（东正西负） */
#define HMC5883L_CALIB_MIN_DELTA 100   /* 校准最小半幅（LSB），低于此值视为旋转不充分 */

/* 安装方位：1 = 模块在板背面、丝印 Y 轴指向屏幕上方（机头方向）。
 * 背面安装从屏幕侧看坐标系镜像，此时 heading = atan2(x, y)。
 * 若实测方向整体反了或有固定偏差，先检查本宏，再微调 MAG_DECL_DEG10 验证。 */
#define HMC5883L_MOUNT_BACK_Y_UP  1

/*---------------------------------------------------------------*/

/* 7 位地址 0x1E，HAL 需要左移后的 8 位地址 */
#define HMC5883L_ADDR        (0x1EU << 1)

/* 寄存器 */
#define HMC5883L_REG_CFGA    0x00U    /* 配置寄存器 A */
#define HMC5883L_REG_CFGB    0x01U    /* 配置寄存器 B（增益） */
#define HMC5883L_REG_MODE    0x02U    /* 模式寄存器 */
#define HMC5883L_REG_DATA    0x03U    /* 数据起始（X,Z,Y 顺序） */
#define HMC5883L_REG_ID_A    0x0AU    /* ID 起始，应为 'H' '4' '3' */

/* 配置值 */
#define HMC5883L_CFGA_VAL    0x70U    /* 8 次平均，15Hz 输出，正常测量 */
#define HMC5883L_CFGB_VAL    0x20U    /* 增益 1090 LSB/Gauss（±1.3 Ga） */
#define HMC5883L_MODE_VAL    0x00U    /* 连续测量模式 */

static hmc5883l_status_t reg_write(uint8_t reg, uint8_t val)
{
    return (HAL_I2C_Mem_Write(&hi2c2, HMC5883L_ADDR, reg,
                              I2C_MEMADD_SIZE_8BIT, &val, 1,
                              HMC5883L_I2C_TIMEOUT) == HAL_OK)
           ? HMC5883L_OK : HMC5883L_ERR_I2C;
}

static hmc5883l_status_t reg_read(uint8_t reg, uint8_t *buf, uint16_t len)
{
    return (HAL_I2C_Mem_Read(&hi2c2, HMC5883L_ADDR, reg,
                             I2C_MEMADD_SIZE_8BIT, buf, len,
                             HMC5883L_I2C_TIMEOUT) == HAL_OK)
           ? HMC5883L_OK : HMC5883L_ERR_I2C;
}

hmc5883l_status_t hmc5883l_init(void)
{
    uint8_t id[3];

    /* 校验 ID：'H' '4' '3' */
    if (reg_read(HMC5883L_REG_ID_A, id, 3) != HMC5883L_OK) {
        return HMC5883L_ERR_I2C;
    }
    if ((id[0] != 'H') || (id[1] != '4') || (id[2] != '3')) {
        return HMC5883L_ERR_ID;
    }

    /* 配置 + 连续测量模式 */
    if (reg_write(HMC5883L_REG_CFGA, HMC5883L_CFGA_VAL) != HMC5883L_OK) {
        return HMC5883L_ERR_I2C;
    }
    if (reg_write(HMC5883L_REG_CFGB, HMC5883L_CFGB_VAL) != HMC5883L_OK) {
        return HMC5883L_ERR_I2C;
    }
    if (reg_write(HMC5883L_REG_MODE, HMC5883L_MODE_VAL) != HMC5883L_OK) {
        return HMC5883L_ERR_I2C;
    }
    return HMC5883L_OK;
}

uint8_t hmc5883l_data_ready(void)
{
    /* DRDY 低电平 = 数据就绪 */
    return (HAL_GPIO_ReadPin(MAG_READY_GPIO_Port, MAG_READY_Pin) == GPIO_PIN_RESET) ? 1U : 0U;
}

hmc5883l_status_t hmc5883l_read(hmc5883l_data_t *data)
{
    uint8_t buf[6];

    if (data == 0) {
        return HMC5883L_ERR_I2C;
    }
    if (reg_read(HMC5883L_REG_DATA, buf, 6) != HMC5883L_OK) {
        return HMC5883L_ERR_I2C;
    }

    /* 寄存器顺序为 X, Z, Y */
    data->x = (int16_t)(((uint16_t)buf[0] << 8) | buf[1]);
    data->z = (int16_t)(((uint16_t)buf[2] << 8) | buf[3]);
    data->y = (int16_t)(((uint16_t)buf[4] << 8) | buf[5]);
    return HMC5883L_OK;
}

/* ---- 校准状态：min-max 法（参考 DOC/磁力计参考算法） ---- */
static int16_t cal_offset[3] = { 0, 0, 0 };           /* 硬铁偏移 */
static float   cal_scale[3]  = { 1.0f, 1.0f, 1.0f };  /* 各轴比例 */
static int16_t cal_min[3];
static int16_t cal_max[3];

void hmc5883l_calib_reset(void)
{
    for (uint8_t j = 0; j < 3U; j++) {
        cal_min[j] = 32767;
        cal_max[j] = -32768;
    }
}

void hmc5883l_calib_feed(const hmc5883l_data_t *raw)
{
    int16_t v[3] = { raw->x, raw->y, raw->z };

    for (uint8_t j = 0; j < 3U; j++) {
        if (v[j] < cal_min[j]) {
            cal_min[j] = v[j];
        }
        if (v[j] > cal_max[j]) {
            cal_max[j] = v[j];
        }
    }
}

hmc5883l_status_t hmc5883l_calib_finish(void)
{
    int16_t offset[3];
    float   scale[3];

    for (uint8_t j = 0; j < 3U; j++) {
        float delta = (float)(cal_max[j] - cal_min[j]) / 2.0f;
        if (delta < (float)HMC5883L_CALIB_MIN_DELTA) {
            return HMC5883L_ERR_ID;    /* 旋转不充分，参数不生效 */
        }
        offset[j] = (int16_t)(((int32_t)cal_max[j] + cal_min[j]) / 2);
        scale[j]  = 1.0f / delta;
    }

    for (uint8_t j = 0; j < 3U; j++) {
        cal_offset[j] = offset[j];
        cal_scale[j]  = scale[j];
    }
    return HMC5883L_OK;
}

int16_t hmc5883l_heading_deg10(const hmc5883l_data_t *data)
{
    /* 应用校准：(raw - offset) * scale */
    float x = ((float)data->x - (float)cal_offset[0]) * cal_scale[0];
    float y = ((float)data->y - (float)cal_offset[1]) * cal_scale[1];

#if HMC5883L_MOUNT_BACK_Y_UP
    /* 背面安装 + Y 轴指屏幕上方（机头）：heading = atan2(X, Y) */
    float deg = atan2f(x, y) * 57.29578f;  /* 180/PI */
#else
    /* 默认（正面丝印朝上，X 轴指机头）：heading = atan2(Y, X) */
    float deg = atan2f(y, x) * 57.29578f;
#endif

    int32_t d10 = (int32_t)(deg * 10.0f) + HMC5883L_MAG_DECL_DEG10;
    d10 %= 3600;
    if (d10 < 0) {
        d10 += 3600;
    }
    return (int16_t)d10;
}
