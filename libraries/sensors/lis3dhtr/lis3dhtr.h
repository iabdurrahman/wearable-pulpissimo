#ifndef LIS3DHTR_H
#define LIS3DHTR_H
 
#include <stdint.h>

#define LIS3DHTR_ADDR 0x32   // config for SD0 connected to VCC (0011001b)

#define REG_WHO_AM_I   0x0F

#define REG_CTRL_REG1  0x20
#define REG_CTRL_REG4  0x23

#define REG_STATUS     0x27
#define REG_OUT_X_L    0x28

#define WHO_AM_I_VALUE 0x33

// CTRL_REG1: ODR=100Hz (0110), Lpen=0, ZEN=1, YEN=1, XEN=1 → 0x67
#define CTRL_REG1_VAL 0x57
// CTRL_REG4: BDU=1, FS=±2g (000), HR=0, ST=0 → 0x80
#define CTRL_REG4_VAL 0x80

#define SENSITIVITY_2G 4    // mg/digit for ±2g

// ERROR CODES
#define LIS3DHTR_OK                 0
#define LIS3DHTR_ERR_I2C_OPEN       -1
#define LIS3DHTR_ERR_COMM           -2
#define LIS3DHTR_ERR_I2C_TIMEOUT    -3
#define LIS3DHTR_ERR_WHO_AM_I       -4
#define LIS3DHTR_ERR_CFG_REG1       -5
#define LIS3DHTR_ERR_CFG_REG4       -6
#define LIS3DHTR_ERR_STATUS         -7
#define LIS3DHTR_ERR_READ           -8

typedef struct {
    int16_t x;
    int16_t y;
    int16_t z;
} accel_data_t;

i2c_t *lis3dhtr_open(void);
int lis3dhtr_init(i2c_t *i2c);
int lis3dhtr_read_accel(i2c_t *i2c, accel_data_t *out);
 
#endif