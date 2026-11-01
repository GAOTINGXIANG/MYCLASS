/**
 * @file hmc5883l.h
 * @brief HMC5883L 三轴磁力计驱动（I2C2，硬件 I2C）
 *
 * 硬件：
 *   SCL = PB10 (I2C2_SCL)，SDA = PB11 (I2C2_SDA)，DRDY = PE14 (MAG_READY，低电平=数据就绪)
 *   7 位从机地址 0x1E，标准模式 100kHz
 *
 * 寄存器要点：
 *   数据寄存器 0x03~0x08 的顺序为 X, Z, Y（不是 X, Y, Z）
 *   ID 寄存器 0x0A~0x0C 固定为 'H' '4' '3'，可用于检测芯片在线
 */

#ifndef __HMC5883L_H__
#define __HMC5883L_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    HMC5883L_OK = 0,
    HMC5883L_ERR_I2C,       /* I2C 通信失败（未接/地址错/总线异常） */
    HMC5883L_ERR_ID         /* ID 寄存器不匹配，不是 HMC5883L */
} hmc5883l_status_t;

/* 一次读取的三轴原始值（LSB，增益 1090 LSB/Gauss） */
typedef struct {
    int16_t x;
    int16_t y;
    int16_t z;
} hmc5883l_data_t;

/** @brief 初始化：校验芯片 ID，配置为 8 次平均 / 15Hz / 连续测量
 *  @note  需在 MX_I2C2_Init() / MX_GPIO_Init() 之后调用 */
hmc5883l_status_t hmc5883l_init(void);

/** @brief DRDY 引脚状态：1 = 数据就绪 */
uint8_t hmc5883l_data_ready(void);

/** @brief 读取三轴原始值（已按 X,Y,Z 顺序整理） */
hmc5883l_status_t hmc5883l_read(hmc5883l_data_t *data);

/** @brief 由原始值计算地磁方位角（自动应用校准参数）
 *  @param data 原始三轴值
 *  @return 0~3599，单位 0.1 度（0 = 磁北，顺时针增大）
 *  @note  水平放置时有效；倾斜需结合加速度计做倾斜补偿 */
int16_t hmc5883l_heading_deg10(const hmc5883l_data_t *data);

/* ---- 校准（min-max 法：硬铁偏移 + 各轴比例，参考 DOC/磁力计参考算法） ----
 * 用法：calib_reset() → 旋转模块并反复 calib_feed() → calib_finish()。
 * 校准前 offset=0、scale=1，等效未校准。 */

/** @brief 清空 min/max 记录，开始一轮校准采样 */
void hmc5883l_calib_reset(void);

/** @brief 喂入一个原始样本，更新三轴 min/max（校准时每次读取后调用） */
void hmc5883l_calib_feed(const hmc5883l_data_t *raw);

/** @brief 结束校准：由 min/max 计算 offset/scale 并生效
 *  @retval HMC5883L_OK 成功；HMC5883L_ERR_ID 表示某轴变化量太小（旋转不充分），参数未生效 */
hmc5883l_status_t hmc5883l_calib_finish(void);

#ifdef __cplusplus
}
#endif

#endif /* __HMC5883L_H__ */
