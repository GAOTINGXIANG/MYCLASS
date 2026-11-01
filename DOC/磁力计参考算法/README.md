
#Connect with Me:
* GitHub: [NOX069](https://github.com/nox069)
* Mail: imshohan1x2@gmail.com
* LinkedIn: https://www.linkedin.com/in/toorikul/

# STM32-HMC5883L-HAL-Enhanced-Precise-Calibration

## Overview
This is an **STM32 HAL-based driver** for the **HMC5883L 3-axis digital magnetometer**. The library provides functions for initializing the sensor, reading raw and calibrated data, performing calibration, and computing heading direction with **declination angle compensation**.

## Features
- **I2C Communication** with STM32 HAL
- **Continuous & Single Measurement Modes**
- **Calibration Support** (Offset and Scaling Correction)
- **Magnetic Declination Compensation**
- **Heading Calculation (0° - 360°)**

## Hardware Requirements
- STM32 Development Board (e.g., STM32F103C8, STM32F4xx, etc.)
- HMC5883L Magnetometer Module
- I2C Pull-up Resistors (4.7kΩ recommended)

## Wiring (I2C Connection)
| HMC5883L Pin | STM32 Pin |
|-------------|-----------|
| VCC        | 3.3V / 5V |
| GND        | GND       |
| SCL        | SCL (PB6 for STM32F1) |
| SDA        | SDA (PB7 for STM32F1) |

## Installation
1. **Copy the files** `HMC5883L.c` and `HMC5883L.h` to your project.
2. **Include the header file** in your main source file:
   ```c
   #include "HMC5883L.h"
   ```
3. **Initialize the sensor in your main function:**
   ```c
   HMC5883L mag;
   HMC5883L_Init(&mag, &hi2c1, GAIN_1090);
   ```

## Usage
### **1. Initialize the Sensor**
```c
HMC5883L mag;
MagnetometerData data;

HMC5883L_Init(&mag, &hi2c1, GAIN_1090);
HMC5883L_SetDeclination(&mag, -2.5f);
HMC5883L_Calibrate(&mag, 300);

```


### **2. Inside While loop**
```c
While(1)
{
   if(HMC5883L_ReadCalibrated(&mag, &data) == HAL_OK) 
       {
            float heading = HMC5883L_GetHeading(&mag, &data);
            printf("X: %.2f µT\tY: %.2f µT\tZ: %.2f µT\tHeading: %.2f°\n",
                   data.x, data.y, data.z, heading);
       }
       HAL_Delay(68);
}
```

## Calibration Guide
1. Rotate the sensor **in all directions** while collecting samples.
2. The function `HMC5883L_Calibrate()` stores **offset and scaling** values.
3. The corrected values are used in `HMC5883L_ReadCalibrated()`.

## Notes
- The **magnetic declination angle** should be set based on your location using `HMC5883L_SetDeclination()`.
- The **Z and Y axes are swapped** due to HMC5883L's internal orientation.
- The **STM32 HAL library is required** for I2C communication.

## Contributions
Feel free to contribute by submitting issues and pull requests!

Enjoy coding with STM32 and HMC5883L! 🚀
