#include "magnetometer.h"
#include <math.h>

// Gain to LSB/Gauss conversion
static const float gain_scales[] = {
    [GAIN_1370] = 0.73,
    [GAIN_1090] = 0.92,
    [GAIN_820]  = 1.22,
    [GAIN_660]  = 1.52,
    [GAIN_440]  = 2.27,
    [GAIN_390]  = 2.56,
    [GAIN_330]  = 3.03,
    [GAIN_230]  = 4.35
};

void HMC5883L_Init(HMC5883L *dev, I2C_HandleTypeDef *i2c_handle, hmc5883l_gain_t gain) {
    dev->i2c_handle = i2c_handle;
    dev->gain = gain;
    dev->declination_angle = 0.0f;

    // Default calibration values
    for(int i = 0; i < 3; i++) {
        dev->mag_scale[i] = 1.0f;
        dev->mag_offset[i] = 0;
    }

    // Configuration sequence
    uint8_t config;

    // Config A: 8-average, 15 Hz, normal measurement
    config = 0x70;
    HAL_I2C_Mem_Write(dev->i2c_handle, HMC5883L_DEFAULT_ADDRESS,
                     HMC5883L_REG_CONFIG_A, 1, &config, 1, 50);

    // Config B: Set gain
    config = (uint8_t)gain;
    HAL_I2C_Mem_Write(dev->i2c_handle, HMC5883L_DEFAULT_ADDRESS,
                     HMC5883L_REG_CONFIG_B, 1, &config, 1, 50);

    // Set continuous measurement mode
    config = HMC5883L_MODE_CONTINUOUS;
    HAL_I2C_Mem_Write(dev->i2c_handle, HMC5883L_DEFAULT_ADDRESS,
                     HMC5883L_REG_MODE, 1, &config, 1, 50);
}

HAL_StatusTypeDef HMC5883L_ReadRaw(HMC5883L *dev, int16_t *raw_data) {
    uint8_t buffer[6];
    HAL_StatusTypeDef status;

    // Read all axis data in one transaction (X, Z, Y order)
    status = HAL_I2C_Mem_Read(dev->i2c_handle, HMC5883L_DEFAULT_ADDRESS,
                            HMC5883L_REG_DATA_X_M, 1, buffer, 6, 50);

    if(status != HAL_OK) return status;

    // Combine registers (X, Z, Y order)
    raw_data[0] = (int16_t)((buffer[0] << 8) | buffer[1]);  // X
    raw_data[1] = (int16_t)((buffer[4] << 8) | buffer[5]);  // Y
    raw_data[2] = (int16_t)((buffer[2] << 8) | buffer[3]);  // Z

    return HAL_OK;
}

HAL_StatusTypeDef HMC5883L_ReadCalibrated(HMC5883L *dev, MagnetometerData *data) {
    int16_t raw[3];
    HAL_StatusTypeDef status = HMC5883L_ReadRaw(dev, raw);

    if(status != HAL_OK) return status;

    // Apply calibration offsets and scaling
    data->x = (raw[0] - dev->mag_offset[0]) * dev->mag_scale[0] * gain_scales[dev->gain];
    data->y = (raw[1] - dev->mag_offset[1]) * dev->mag_scale[1] * gain_scales[dev->gain];
    data->z = (raw[2] - dev->mag_offset[2]) * dev->mag_scale[2] * gain_scales[dev->gain];

    return HAL_OK;
}

float HMC5883L_GetHeading(HMC5883L *dev, MagnetometerData *data) {
    // Calculate heading in radians
    float heading = atan2f(data->y, data->x);

    // Adjust for declination
    heading += dev->declination_angle;

    // Normalize to 0-2π
    if(heading < 0) heading += 2 * M_PI;
    if(heading > 2 * M_PI) heading -= 2 * M_PI;
    return heading * (180 / M_PI);  // Convert to degrees
}

HAL_StatusTypeDef HMC5883L_Calibrate(HMC5883L *dev, uint32_t samples) {
    int16_t min[3] = {32767, 32767, 32767};
    int16_t max[3] = {-32768, -32768, -32768};
    int16_t raw[3];

    for(uint32_t i = 0; i < samples; i++) {
        if(HMC5883L_ReadRaw(dev, raw) != HAL_OK) return HAL_ERROR;

        for(int j = 0; j < 3; j++) {
            if(raw[j] < min[j]) min[j] = raw[j];
            if(raw[j] > max[j]) max[j] = raw[j];
        }
        HAL_Delay(10);
    }

    // Calculate offsets and scale factors
    for(int j = 0; j < 3; j++) {
        dev->mag_offset[j] = (max[j] + min[j]) / 2;
        float avg_delta = (max[j] - min[j]) / 2.0f;
        dev->mag_scale[j] = avg_delta ? 1.0f / avg_delta : 1.0f;
    }

    return HAL_OK;
}

void HMC5883L_SetDeclination(HMC5883L *dev, float declination_deg) {
    dev->declination_angle = declination_deg * (M_PI / 180.0f);
}
