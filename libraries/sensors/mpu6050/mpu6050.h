//Driver library for the InvenSense MPU-6050 gyroscope sensor
//on the ICDeC PULPissimo FPGA board via I2C interface.
//Focuses on gyroscope + I2C functionality from the MPU-6000/MPU-6050 datasheet.

#ifndef __MPU6050_H__
#define __MPU6050_H__

#include "gyro_common.h"

#ifdef __cplusplus
extern "C" {
#endif

//Register Addresses

//Configuration Registers
#define MPU6050_REG_SMPLRT_DIV    0x19  //Sample Rate Divider
#define MPU6050_REG_CONFIG        0x1A  //Configuration (DLPF, EXT_SYNC)
#define MPU6050_REG_GYRO_CONFIG   0x1B  //Gyroscope Configuration
#define MPU6050_REG_ACCEL_CONFIG  0x1C  //Accelerometer Configuration

//Interrupt Registers
#define MPU6050_REG_INT_PIN_CFG   0x37  //Interrupt pin configuration
#define MPU6050_REG_INT_ENABLE    0x38  //Interrupt enable
#define MPU6050_REG_INT_STATUS    0x3A  //Interrupt status

//Data Output Registers
#define MPU6050_REG_ACCEL_XOUT_H  0x3B  //Accelerometer X-axis high byte
#define MPU6050_REG_TEMP_OUT_H    0x41  //Temperature high byte
#define MPU6050_REG_GYRO_XOUT_H   0x43  //Gyro X-axis High Byte
#define MPU6050_REG_GYRO_XOUT_L   0x44  //Gyro X-axis Low Byte
#define MPU6050_REG_GYRO_YOUT_H   0x45  //Gyro Y-axis High Byte
#define MPU6050_REG_GYRO_YOUT_L   0x46  //Gyro Y-axis Low Byte
#define MPU6050_REG_GYRO_ZOUT_H   0x47  //Gyro Z-axis High Byte
#define MPU6050_REG_GYRO_ZOUT_L   0x48  //Gyro Z-axis Low Byte

//Power Management
#define MPU6050_REG_PWR_MGMT_1    0x6B  //Power Management 1
#define MPU6050_REG_PWR_MGMT_2    0x6C  //Power Management 2

//Device Identification
#define MPU6050_REG_WHO_AM_I      0x75  //Who Am I Register

//Expected WHO_AM_I value
#define MPU6050_WHO_AM_I_VALUE    0x68

//Gyro Config - Full-Scale Range (FS_SEL) raw bit values
//Note: Pre-shifted enum values (for direct register write) are in gyro_common.h
#define MPU6050_GYRO_FS_250DPS    0x00  //±250 °/s  (FS_SEL = 0)
#define MPU6050_GYRO_FS_500DPS    0x01  //±500 °/s  (FS_SEL = 1)
#define MPU6050_GYRO_FS_1000DPS   0x02  //±1000 °/s (FS_SEL = 2)
#define MPU6050_GYRO_FS_2000DPS   0x03  //±2000 °/s (FS_SEL = 3)

//Gyro Sensitivity Scaling Factors (LSB/°/s)
#define MPU6050_GYRO_SENSITIVITY_250    131.0f  //LSB/°/s at ±250 dps
#define MPU6050_GYRO_SENSITIVITY_500    65.5f   //LSB/°/s at ±500 dps
#define MPU6050_GYRO_SENSITIVITY_1000   32.8f   //LSB/°/s at ±1000 dps
#define MPU6050_GYRO_SENSITIVITY_2000   16.4f   //LSB/°/s at ±2000 dps

//DLPF Configuration (CONFIG register bits [2:0])
#define MPU6050_DLPF_BW_256HZ     0x00  //Accel 260Hz, Gyro 256Hz, Fs=8kHz
#define MPU6050_DLPF_BW_188HZ     0x01  //Accel 184Hz, Gyro 188Hz, Fs=1kHz
#define MPU6050_DLPF_BW_98HZ      0x02  //Accel 94Hz,  Gyro 98Hz,  Fs=1kHz
#define MPU6050_DLPF_BW_42HZ      0x03  //Accel 44Hz,  Gyro 42Hz,  Fs=1kHz
#define MPU6050_DLPF_BW_20HZ      0x04  //Accel 21Hz,  Gyro 20Hz,  Fs=1kHz
#define MPU6050_DLPF_BW_10HZ      0x05  //Accel 10Hz,  Gyro 10Hz,  Fs=1kHz
#define MPU6050_DLPF_BW_5HZ       0x06  //Accel 5Hz,   Gyro 5Hz,   Fs=1kHz

//Clock Source Selection (PWR_MGMT_1 register bits [2:0])
#define MPU6050_CLK_INTERNAL_8MHZ 0x00  //Internal 8MHz oscillator
#define MPU6050_CLK_GYRO_X_REF    0x01  //PLL with X-axis gyro reference
#define MPU6050_CLK_GYRO_Y_REF    0x02  //PLL with Y-axis gyro reference
#define MPU6050_CLK_GYRO_Z_REF    0x03  //PLL with Z-axis gyro reference

//PWR_MGMT_1 Bit Definitions
#define MPU6050_PWR1_DEVICE_RESET 0x80  //Device reset (bit 7)
#define MPU6050_PWR1_SLEEP        0x40  //Sleep mode (bit 6)
#define MPU6050_PWR1_CYCLE        0x20  //Cycle mode (bit 5)
#define MPU6050_PWR1_TEMP_DIS     0x08  //Disable temperature sensor (bit 3)

//Configuration Structure
typedef struct {
    uint8_t              i2c_addr;    //7-bit I2C slave address (0x68 or 0x69)
    int                  i2c_id;      //I2C peripheral ID (usually 0)
    unsigned int         i2c_freq;    //I2C clock frequency in Hz
    mpu6050_gyro_range_t gyro_range;  //Gyroscope full-scale range selection
    uint8_t              dlpf_cfg;    //Digital Low Pass Filter config (0-6)
    uint8_t              smplrt_div;  //Sample rate divider (sample rate = gyro_rate / (1 + div))
} mpu6050_config_t;

//API Functions

gyro_status_t mpu6050_default_config(mpu6050_config_t *cfg);
gyro_status_t mpu6050_init(const mpu6050_config_t *cfg);
gyro_status_t mpu6050_who_am_i(uint8_t *id);
gyro_status_t mpu6050_read_gyro_raw(gyro_raw_t *raw);
gyro_status_t mpu6050_read_gyro_dps(gyro_dps_t *dps);
gyro_status_t mpu6050_set_gyro_range(mpu6050_gyro_range_t range);
gyro_status_t mpu6050_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* __MPU6050_H__ */
