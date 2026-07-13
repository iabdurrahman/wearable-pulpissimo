#include <stdint.h>
#include <stdio.h>
#include "pulp.h"
#include "lis3dhtr.h"

static int lis3dhtr_write_reg(i2c_t *i2c, uint8_t reg, uint8_t value)
{
    uint8_t buf[2] = { reg, value };
    return i2c_write(i2c, buf, 2, 1);
}

static int lis3dhtr_read_reg(i2c_t *i2c, uint8_t reg, uint8_t *out, int len)
{
    int ret = i2c_write(i2c, &reg, 1, 0);
    if (ret != 0) return ret;
    int bytes = i2c_read(i2c, out, len, 0);
    return (bytes == len) ? 0 : -1;
}

i2c_t *lis3dhtr_open(void)
{
    i2c_dev_t dev;
    i2c_dev_init(&dev);
    dev.id           = 0;
    dev.cs           = LIS3DHTR_ADDR;
    dev.max_baudrate = 400000;

    i2c_t *i2c = i2c_open(&dev);
    if (i2c == NULL) {
        printf("[ERROR] i2c_open failed\n\r");
        return NULL;
    }

    i2c_settimeout(10000, true);
    return i2c;
}

int lis3dhtr_init(i2c_t *i2c)
{
    uint8_t who_am_i = 0;

    if (lis3dhtr_read_reg(i2c, REG_WHO_AM_I, &who_am_i, 1) != 0) {
        printf("[ERROR] failed i2c comm on WHO_AM_I\n\r");
        return LIS3DHTR_ERR_COMM;
    }
    if (who_am_i != WHO_AM_I_VALUE) {
        printf("[ERROR] wrong WHO_AM_I (got 0x%02X, expected 0x%02X)\n\r",
               who_am_i, WHO_AM_I_VALUE);
        return LIS3DHTR_ERR_WHO_AM_I;
    }
    printf("[OK] WHO_AM_I OK (0x%02X)\n\r", who_am_i);

    if (lis3dhtr_write_reg(i2c, REG_CTRL_REG1, CTRL_REG1_VAL) != 0) {
        printf("[ERROR] failed to write CTRL_REG1\n\r");
        return LIS3DHTR_ERR_CFG_REG1;
    }
    if (lis3dhtr_write_reg(i2c, REG_CTRL_REG4, CTRL_REG4_VAL) != 0) {
        printf("[ERROR] failed to write CTRL_REG4\n\r");
        return LIS3DHTR_ERR_CFG_REG4;
    }

    printf("[OK] Init succeeded\n\r");
    return LIS3DHTR_OK;
}

int lis3dhtr_read_accel(i2c_t *i2c, accel_data_t *out)
{
    uint8_t status = 0;
    int poll_count = 0;

    while (!(status & 0x08) && poll_count < 1000) {
        if (lis3dhtr_read_reg(i2c, REG_STATUS, &status, 1) != 0) {
            printf("[ERROR] failed to read STATUS\n\r");
            return LIS3DHTR_ERR_STATUS;
        }
        poll_count++;
    }

    if (!(status & 0x08)) {
        printf("[ERROR] ZYXDA never set, STATUS=0x%02X\n\r", status);
        return LIS3DHTR_ERR_STATUS;
    }

    uint8_t raw[6] = {0};
    uint8_t reg_addr = REG_OUT_X_L | 0x80;
    int ret = i2c_write(i2c, &reg_addr, 1, 0);
    if (ret != 0) {
        printf("[ERROR] burst read addr write failed\n\r");
        return LIS3DHTR_ERR_READ;
    }
    int bytes = i2c_read(i2c, raw, 6, 0);
    if (bytes != 6) {
        printf("[ERROR] burst read failed, got %d bytes\n\r", bytes);
        return LIS3DHTR_ERR_READ;
    }

    // 10-bit right-justified, shift 6
    int16_t x_raw = (int16_t)((raw[1] << 8) | raw[0]) >> 6;
    int16_t y_raw = (int16_t)((raw[3] << 8) | raw[2]) >> 6;
    int16_t z_raw = (int16_t)((raw[5] << 8) | raw[4]) >> 6;

    out->x = x_raw * SENSITIVITY_2G;
    out->y = y_raw * SENSITIVITY_2G;
    out->z = z_raw * SENSITIVITY_2G;

    return LIS3DHTR_OK;
}