//Copyright (C) 2026 ICDeC
//Driver library for the STMicroelectronics L3G4200D 3-Axis Digital gyroscope sensor
 
#ifndef __L3G4200D_H__
#define __L3G4200D_H__

#include "gyro_common.h"

#ifdef __cplusplus
extern "C" {
#endif

//I2C Slave Address (8-bit write/read format)
// 7-bit addresses (0x68/0x69) are defined in gyro_common.h

#define L3G4200D_I2C_WRITE_ADDR_GND 0xD0  //Write address (SDO to GND)
#define L3G4200D_I2C_READ_ADDR_GND  0xD1  //Read address (SDO to GND)
#define L3G4200D_I2C_WRITE_ADDR_VDD 0xD2  //Write address (SDO to Vdd)
#define L3G4200D_I2C_READ_ADDR_VDD  0xD3  //Read address (SDO to Vdd)

//Register Address Map

//Device Identification
#define L3G4200D_REG_WHO_AM_I       0x0F  //Device ID register (read-only, default: 0xD3)

//Control Registers
#define L3G4200D_REG_CTRL_REG1      0x20  //ODR, bandwidth, power mode, axis enable
#define L3G4200D_REG_CTRL_REG2      0x21  //High-pass filter config
#define L3G4200D_REG_CTRL_REG3      0x22  //Interrupt control
#define L3G4200D_REG_CTRL_REG4      0x23  //Full-scale, self-test, SPI mode
#define L3G4200D_REG_CTRL_REG5      0x24  //FIFO, HPF enable, INT/OUT selection

//Reference and Data Capture
#define L3G4200D_REG_REFERENCE      0x25  //Interrupt reference value

//Temperature Output
#define L3G4200D_REG_OUT_TEMP       0x26  //Temperature output (read-only)

//Status Register
#define L3G4200D_REG_STATUS         0x27  //Data availability, overrun flags

//Angular Rate Output (16-bit, two's complement)
#define L3G4200D_REG_OUT_X_L        0x28  //X-axis angular rate - low byte (read-only)
#define L3G4200D_REG_OUT_X_H        0x29  //X-axis angular rate - high byte (read-only)
#define L3G4200D_REG_OUT_Y_L        0x2A  //Y-axis angular rate - low byte (read-only)
#define L3G4200D_REG_OUT_Y_H        0x2B  //Y-axis angular rate - high byte (read-only)
#define L3G4200D_REG_OUT_Z_L        0x2C  //Z-axis angular rate - low byte (read-only)
#define L3G4200D_REG_OUT_Z_H        0x2D  //Z-axis angular rate - high byte (read-only)

//FIFO Registers
#define L3G4200D_REG_FIFO_CTRL_REG  0x2E  //FIFO mode and watermark level
#define L3G4200D_REG_FIFO_SRC_REG   0x2F  //FIFO status and data level

//Interrupt Configuration Registers
#define L3G4200D_REG_INT1_CFG       0x30  //Interrupt 1 event enable, AND/OR logic
#define L3G4200D_REG_INT1_SRC       0x31  //Interrupt 1 source (read-only)
#define L3G4200D_REG_INT1_THS_XH    0x32  //Interrupt 1 threshold X - high byte
#define L3G4200D_REG_INT1_THS_XL    0x33  //Interrupt 1 threshold X - low byte
#define L3G4200D_REG_INT1_THS_YH    0x34  //Interrupt 1 threshold Y - high byte
#define L3G4200D_REG_INT1_THS_YL    0x35  //Interrupt 1 threshold Y - low byte
#define L3G4200D_REG_INT1_THS_ZH    0x36  //Interrupt 1 threshold Z - high byte
#define L3G4200D_REG_INT1_THS_ZL    0x37  //Interrupt 1 threshold Z - low byte
#define L3G4200D_REG_INT1_DURATION  0x38  //Interrupt 1 minimum event duration

//CTRL_REG1 Bit Definitions (0x20)

//Output Data Rate (ODR) and Bandwidth Selection [bits 7:4]
#define L3G4200D_ODR_100HZ_BW12P5   0x00  //100 Hz ODR, 12.5 Hz BW
#define L3G4200D_ODR_100HZ_BW25     0x10  //100 Hz ODR, 25 Hz BW
#define L3G4200D_ODR_200HZ_BW12P5   0x40  //200 Hz ODR, 12.5 Hz BW
#define L3G4200D_ODR_200HZ_BW25     0x44  //200 Hz ODR, 25 Hz BW
#define L3G4200D_ODR_200HZ_BW50     0x48  //200 Hz ODR, 50 Hz BW
#define L3G4200D_ODR_200HZ_BW70     0x4C  //200 Hz ODR, 70 Hz BW
#define L3G4200D_ODR_400HZ_BW20     0x80  /**< 400 Hz ODR, 20 Hz BW   */
#define L3G4200D_ODR_400HZ_BW25     0x84  //400 Hz ODR, 25 Hz BW
#define L3G4200D_ODR_400HZ_BW50     0x88  //400 Hz ODR, 50 Hz BW
#define L3G4200D_ODR_400HZ_BW110    0x8C  //400 Hz ODR, 110 Hz BW
#define L3G4200D_ODR_800HZ_BW30     0xC0  //800 Hz ODR, 30 Hz BW
#define L3G4200D_ODR_800HZ_BW35     0xC4  //800 Hz ODR, 35 Hz BW
#define L3G4200D_ODR_800HZ_BW50     0xC8  //800 Hz ODR, 50 Hz BW
#define L3G4200D_ODR_800HZ_BW110    0xCC  //800 Hz ODR, 110 Hz BW

//Power Mode Control [bit 3]
#define L3G4200D_POWER_DOWN         0x00  //Power-down mode (PD=0)
#define L3G4200D_NORMAL_MODE        0x08  //Normal mode (PD=1)
#define L3G4200D_SLEEP_MODE         0x08  //Sleep mode (PD=1, axes disabled)

//Axis Enable/Disable [bits 2:0]
#define L3G4200D_AXIS_X_ENABLE      0x01  //Enable X-axis
#define L3G4200D_AXIS_Y_ENABLE      0x02  //Enable Y-axis
#define L3G4200D_AXIS_Z_ENABLE      0x04  //Enable Z-axis
#define L3G4200D_ALL_AXES_ENABLE    0x07  //Enable all axes

//CTRL_REG4 Bit Definitions (0x23)
//Full-scale range enum (L3G4200D_RANGE_*) is in gyro_common.h

//Full Scale Selection [bits 5:4]
#define L3G4200D_FULLSCALE_250      0x00  //±250 dps
#define L3G4200D_FULLSCALE_500      0x10  //±500 dps
#define L3G4200D_FULLSCALE_2000     0x20  //±2000 dps

//Sensitivity Values (mdps/digit)
#define L3G4200D_SENSITIVITY_250    8.75f  //8.75 mdps/digit at ±250 dps
#define L3G4200D_SENSITIVITY_500    17.50f //17.50 mdps/digit at ±500 dps
#define L3G4200D_SENSITIVITY_2000   70.0f  //70 mdps/digit at ±2000 dps

//Block Data Update [bit 7]
#define L3G4200D_BDU_CONTINUOUS     0x00  //Continuous update
#define L3G4200D_BDU_BLOCKED        0x80  //Registers not updated until MSB+LSB read

//Endianness [bit 6]
#define L3G4200D_ENDIAN_LSB_LOW     0x00  //Data LSB @ lower address
#define L3G4200D_ENDIAN_MSB_LOW     0x40  //Data MSB @ lower address

//Self-Test [bits 2:1]
#define L3G4200D_SELFTEST_DISABLED  0x00  //Self-test disabled
#define L3G4200D_SELFTEST_0_POS     0x02  //Self-test 0 (+)
#define L3G4200D_SELFTEST_1_NEG     0x06  //Self-test 1 (-)

//SPI Interface Mode [bit 0]
#define L3G4200D_SPI_4WIRE          0x00  //4-wire SPI interface
#define L3G4200D_SPI_3WIRE          0x01  //3-wire SPI interface

//CTRL_REG5 Bit Definitions (0x24)

#define L3G4200D_REBOOT_DISABLED    0x00  //Normal mode
#define L3G4200D_REBOOT_ENABLED     0x80  //Reboot memory content
#define L3G4200D_FIFO_DISABLED      0x00  //FIFO disabled
#define L3G4200D_FIFO_ENABLED       0x40  //FIFO enabled
#define L3G4200D_HPF_DISABLED       0x00  //High-pass filter disabled
#define L3G4200D_HPF_ENABLED        0x10  //High-pass filter enabled

//FIFO_CTRL_REG Bit Definitions (0x2E)

//FIFO Mode Selection [bits 7:5]
#define L3G4200D_FIFO_BYPASS            0x00  //Bypass mode
#define L3G4200D_FIFO_FIFO              0x20  //FIFO mode
#define L3G4200D_FIFO_STREAM            0x40  //Stream mode
#define L3G4200D_FIFO_STREAM_TO_FIFO    0x60  //Stream-to-FIFO mode
#define L3G4200D_FIFO_BYPASS_TO_STREAM  0x80  //Bypass-to-Stream mode

//FIFO Watermark Level mask [bits 4:0]
#define L3G4200D_FIFO_WATERMARK_MASK    0x1F

//INT1_CFG Bit Definitions (0x30)

//Interrupt Logic [bit 7]
#define L3G4200D_INT_OR_LOGIC       0x00  //OR combination of events
#define L3G4200D_INT_AND_LOGIC      0x80  //AND combination of events

//Interrupt Latch [bit 6]
#define L3G4200D_INT_LATCH_DISABLED 0x00  //Interrupt not latched
#define L3G4200D_INT_LATCH_ENABLED  0x40  //Interrupt latched (cleared by reading SRC)

//Interrupt Events [bits 5:0]
#define L3G4200D_INT_ZHIE           0x20  //Z-axis high event interrupt
#define L3G4200D_INT_ZLIE           0x10  //Z-axis low event interrupt
#define L3G4200D_INT_YHIE           0x08  //Y-axis high event interrupt
#define L3G4200D_INT_YLIE           0x04  //Y-axis low event interrupt
#define L3G4200D_INT_XHIE           0x02  //X-axis high event interrupt
#define L3G4200D_INT_XLIE           0x01  //X-axis low event interrupt

//I2C Protocol Constants

//Auto-Increment Address Control for Multi-byte Read/Write
#define L3G4200D_AUTO_INCREMENT_ADDR 0x80 //Set bit 7 of sub-address for auto-increment
#define L3G4200D_NO_AUTO_INCREMENT   0x00 //No auto-increment

//I2C Read/Write Bit
#define L3G4200D_I2C_READ_BIT        0x01 //Read operation
#define L3G4200D_I2C_WRITE_BIT       0x00 //Write operation

//Device Specifications and Constants

//Expected WHO_AM_I value
#define L3G4200D_WHO_AM_I_VALUE     0xD3

//Device identification (alias)
#define L3G4200D_DEVICE_ID          0xD3

//I2C Speed Modes
#define L3G4200D_I2C_STD_MODE       100000 //Standard mode: 100 kHz
#define L3G4200D_I2C_FAST_MODE      400000 //Fast mode: 400 kHz

//Operating specifications
#define L3G4200D_SUPPLY_VOLTAGE_MIN 2.4f   //Minimum supply voltage (V)
#define L3G4200D_SUPPLY_VOLTAGE_MAX 3.6f   //Maximum supply voltage (V)
#define L3G4200D_TEMP_MIN           (-40)  //Min operating temperature (°C)
#define L3G4200D_TEMP_MAX           85     //Max operating temperature (°C)

// FIFO and axis constants
#define L3G4200D_FIFO_MAX_SIZE      32     //Maximum FIFO buffer size
#define L3G4200D_AXES               3      //Number of axes (X, Y, Z)

//Configuration Structure
typedef struct {
    uint8_t            i2c_addr;   /**< 7-bit I2C slave address (0x68 or 0x69) */
    int                i2c_id;     /**< I2C peripheral ID (usually 0) */
    unsigned int       i2c_freq;   /**< I2C clock frequency in Hz */
    l3g4200d_range_t   range;      /**< Full-scale range selection */
} l3g4200d_config_t;

//API Functions

gyro_status_t l3g4200d_default_config(l3g4200d_config_t *cfg);
gyro_status_t l3g4200d_init(const l3g4200d_config_t *cfg);
gyro_status_t l3g4200d_who_am_i(uint8_t *id);
gyro_status_t l3g4200d_read_raw(gyro_raw_t *raw);
gyro_status_t l3g4200d_read_dps(gyro_dps_t *dps);
gyro_status_t l3g4200d_set_range(l3g4200d_range_t range);
gyro_status_t l3g4200d_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* __L3G4200D_H__ */
