#ifndef INC_MAGNETOMETER_H_
#define INC_MAGNETOMETER_H_

#include "stm32f4xx_hal.h"

#define HMC5883L_ADDRESS         0x1E
#define HMC5883L_DEFAULT_ADDRESS (HMC5883L_ADDRESS << 1)

// Register Map
#define HMC5883L_REG_CONFIG_A    0x00
#define HMC5883L_REG_CONFIG_B    0x01
#define HMC5883L_REG_MODE        0x02
#define HMC5883L_REG_DATA_X_M    0x03
#define HMC5883L_REG_DATA_X_L    0x04
#define HMC5883L_REG_DATA_Z_M    0x05
#define HMC5883L_REG_DATA_Z_L    0x06
#define HMC5883L_REG_DATA_Y_M    0x07
#define HMC5883L_REG_DATA_Y_L    0x08
#define HMC5883L_REG_STATUS      0x09

// Mode Options
#define HMC5883L_MODE_CONTINUOUS 0x00
#define HMC5883L_MODE_SINGLE     0x01
#define HMC5883L_MODE_IDLE       0x03

// Gain Settings
typedef enum {
    GAIN_1370 = 0x00,  // ±0.88 Ga (0.73 mG/LSB)
    GAIN_1090 = 0x20,  // ±1.3  Ga (0.92 mG/LSB) - Default
    GAIN_820  = 0x40,  // ±1.9  Ga (1.22 mG/LSB)
    GAIN_660  = 0x60,  // ±2.5  Ga (1.52 mG/LSB)
    GAIN_440  = 0x80,  // ±4.0  Ga (2.27 mG/LSB)
    GAIN_390  = 0xA0,  // ±4.7  Ga (2.56 mG/LSB)
    GAIN_330  = 0xC0,  // ±5.6  Ga (3.03 mG/LSB)
    GAIN_230  = 0xE0   // ±8.1  Ga (4.35 mG/LSB)
} hmc5883l_gain_t;

typedef struct {
    I2C_HandleTypeDef *i2c_handle;
    float mag_scale[3];
    int16_t mag_offset[3];
    hmc5883l_gain_t gain;
    float declination_angle;  // Magnetic declination (radians)
} HMC5883L;

typedef struct {
    float x;
    float y;
    float z;
    float f1;
} MagnetometerData;

// Function prototypes
void HMC5883L_Init(HMC5883L *dev, I2C_HandleTypeDef *i2c_handle, hmc5883l_gain_t gain);
void HMC5883L_SetDeclination(HMC5883L *dev, float declination_deg);
HAL_StatusTypeDef HMC5883L_ReadRaw(HMC5883L *dev, int16_t *raw_data);
HAL_StatusTypeDef HMC5883L_ReadCalibrated(HMC5883L *dev, MagnetometerData *data);
float HMC5883L_GetHeading(HMC5883L *dev, MagnetometerData *data);
HAL_StatusTypeDef HMC5883L_Calibrate(HMC5883L *dev, uint32_t samples);

#endif /* INC_MAGNETOMETER_H_ */
