#include <stdint.h>
#include <string.h>
#include "pulp.h"
#include "mpu6050.h"

//Internal driver state
static struct {
    i2c_t               *i2c;           
    i2c_dev_t            i2c_dev;       
    uint8_t              i2c_addr;      
    mpu6050_gyro_range_t gyro_range;    
    float                sensitivity;  
    int                  initialized;  
} mpu6050_state = { .initialized = 0 };

//Sensitivity Lookup

/**
 * @brief Get sensitivity in LSB/(°/s) for the given gyro range.
 */
static float mpu6050_get_sensitivity(mpu6050_gyro_range_t range)
{
    switch (range) {
        case MPU6050_GYRO_RANGE_250DPS:   return 131.0f;
        case MPU6050_GYRO_RANGE_500DPS:   return 65.5f;
        case MPU6050_GYRO_RANGE_1000DPS:  return 32.8f;
        case MPU6050_GYRO_RANGE_2000DPS:  return 16.4f;
        default:                          return 131.0f;
    }
}

//Low-Level I2C Helpers

/**
 * @brief Write a single byte to a register on the MPU-6050.
 *
 * @param reg   Register address.
 * @param value Byte value to write.
 * @return GYRO_OK on success, GYRO_ERR_I2C on failure.
 */
static gyro_status_t mpu6050_write_reg(uint8_t reg, uint8_t value)
{
    unsigned char data[2];
    data[0] = reg;
    data[1] = value;

    int ret = i2c_write(mpu6050_state.i2c, data, 2, 1);
    return (ret == 0) ? GYRO_OK : GYRO_ERR_I2C;
}

/**
 * @brief Read one or more bytes from a register on the MPU-6050.
 *
 * The MPU-6050 auto-increments the register address on sequential reads,
 * so no special bit manipulation is needed (unlike L3G4200D).
 *
 * @param reg    Starting register address.
 * @param buffer Pointer to buffer for received data.
 * @param len    Number of bytes to read.
 * @return GYRO_OK on success, GYRO_ERR_I2C on failure.
 */
static gyro_status_t mpu6050_read_reg(uint8_t reg, uint8_t *buffer, int len)
{
    unsigned char reg_addr = reg;

    /* Write register address (no STOP, repeated start) */
    int ret = i2c_write(mpu6050_state.i2c, &reg_addr, 1, 0);
    if (ret != 0) {
        return GYRO_ERR_I2C;
    }

    /* Read data bytes */
    ret = i2c_read(mpu6050_state.i2c, buffer, len, 0);
    if (ret != len) {
        return GYRO_ERR_I2C;
    }

    return GYRO_OK;
}

//Public API Implementation

gyro_status_t mpu6050_default_config(mpu6050_config_t *cfg)
{
    if (cfg == NULL) {
        return GYRO_ERR_NULL;
    }

    cfg->i2c_addr   = MPU6050_I2C_ADDR_DEFAULT;
    cfg->i2c_id     = GYRO_DEFAULT_I2C_ID;
    cfg->i2c_freq   = GYRO_DEFAULT_I2C_BAUDRATE;
    cfg->gyro_range = MPU6050_GYRO_RANGE_250DPS;
    cfg->dlpf_cfg   = 0;    /* DLPF disabled, gyro output rate = 8kHz */
    cfg->smplrt_div = 79;   /* Sample rate = 8000 / (1 + 79) = 100 Hz */

    return GYRO_OK;
}

gyro_status_t mpu6050_init(const mpu6050_config_t *cfg)
{
    gyro_status_t status;
    uint8_t       who_am_i;

    if (cfg == NULL) {
        return GYRO_ERR_NULL;
    }

    /* Configure I2C device */
    i2c_dev_init(&mpu6050_state.i2c_dev);
    mpu6050_state.i2c_dev.id           = cfg->i2c_id;
    mpu6050_state.i2c_dev.cs           = (cfg->i2c_addr << 1);  /* 8-bit address */
    mpu6050_state.i2c_dev.max_baudrate = cfg->i2c_freq;

    /* Open I2C bus */
    mpu6050_state.i2c = i2c_open(&mpu6050_state.i2c_dev);
    if (mpu6050_state.i2c == NULL) {
        return GYRO_ERR_I2C;
    }

    /* Set I2C timeout (5000 µs) */
    i2c_settimeout(5000, 1);

    /* Store configuration */
    mpu6050_state.i2c_addr    = cfg->i2c_addr;
    mpu6050_state.gyro_range  = cfg->gyro_range;
    mpu6050_state.sensitivity = mpu6050_get_sensitivity(cfg->gyro_range);

    /*
     * Wake up the MPU-6050 from sleep mode.
     * PWR_MGMT_1: clear SLEEP bit, select internal 8MHz clock.
     */
    status = mpu6050_write_reg(MPU6050_REG_PWR_MGMT_1, MPU6050_CLK_INTERNAL_8MHZ);
    if (status != GYRO_OK) {
        i2c_close(mpu6050_state.i2c);
        return GYRO_ERR_CONFIG;
    }

    /* Small delay after wake-up to let the sensor stabilize */
    for (volatile int i = 0; i < 10000; i++);

    /* Verify WHO_AM_I */
    status = mpu6050_read_reg(MPU6050_REG_WHO_AM_I, &who_am_i, 1);
    if (status != GYRO_OK) {
        i2c_close(mpu6050_state.i2c);
        return GYRO_ERR_I2C;
    }

    if (who_am_i != MPU6050_WHO_AM_I_VALUE) {
        printf("[MPU6050] WHO_AM_I mismatch: expected 0x%02X, got 0x%02X\n",
               MPU6050_WHO_AM_I_VALUE, who_am_i);
        i2c_close(mpu6050_state.i2c);
        return GYRO_ERR_ID;
    }

    /*
     * Configure sample rate divider:
     * Sample Rate = Gyroscope Output Rate / (1 + SMPLRT_DIV)
     * With DLPF disabled: Gyroscope Output Rate = 8kHz
     * With DLPF enabled:  Gyroscope Output Rate = 1kHz
     */
    status = mpu6050_write_reg(MPU6050_REG_SMPLRT_DIV, cfg->smplrt_div);
    if (status != GYRO_OK) {
        i2c_close(mpu6050_state.i2c);
        return GYRO_ERR_CONFIG;
    }

    /*
     * Configure DLPF (Digital Low Pass Filter):
     * CONFIG register bits [2:0] = DLPF_CFG
     */
    status = mpu6050_write_reg(MPU6050_REG_CONFIG, cfg->dlpf_cfg & 0x07);
    if (status != GYRO_OK) {
        i2c_close(mpu6050_state.i2c);
        return GYRO_ERR_CONFIG;
    }

    /*
     * Configure gyroscope full-scale range:
     * GYRO_CONFIG register bits [4:3] = FS_SEL
     */
    status = mpu6050_write_reg(MPU6050_REG_GYRO_CONFIG, cfg->gyro_range);
    if (status != GYRO_OK) {
        i2c_close(mpu6050_state.i2c);
        return GYRO_ERR_CONFIG;
    }

    mpu6050_state.initialized = 1;
    printf("[MPU6050] Initialized successfully (WHO_AM_I=0x%02X, gyro_range=%d)\n",
           who_am_i, cfg->gyro_range);

    return GYRO_OK;
}

gyro_status_t mpu6050_who_am_i(uint8_t *id)
{
    if (id == NULL) {
        return GYRO_ERR_NULL;
    }
    if (!mpu6050_state.initialized) {
        return GYRO_ERR_CONFIG;
    }

    return mpu6050_read_reg(MPU6050_REG_WHO_AM_I, id, 1);
}

gyro_status_t mpu6050_read_gyro_raw(gyro_raw_t *raw)
{
    uint8_t buffer[6];
    gyro_status_t status;

    if (raw == NULL) {
        return GYRO_ERR_NULL;
    }
    if (!mpu6050_state.initialized) {
        return GYRO_ERR_CONFIG;
    }

    /* Read 6 bytes starting from GYRO_XOUT_H */
    status = mpu6050_read_reg(MPU6050_REG_GYRO_XOUT_H, buffer, 6);
    if (status != GYRO_OK) {
        return status;
    }

    /* MPU-6050 stores data in big-endian (high byte first) */
    raw->x = (int16_t)((buffer[0] << 8) | buffer[1]);
    raw->y = (int16_t)((buffer[2] << 8) | buffer[3]);
    raw->z = (int16_t)((buffer[4] << 8) | buffer[5]);

    return GYRO_OK;
}

gyro_status_t mpu6050_read_gyro_dps(gyro_dps_t *dps)
{
    gyro_raw_t raw;
    gyro_status_t status;

    if (dps == NULL) {
        return GYRO_ERR_NULL;
    }

    status = mpu6050_read_gyro_raw(&raw);
    if (status != GYRO_OK) {
        return status;
    }

    /*
     * Convert raw data to degrees per second.
     * Sensitivity is in LSB/(°/s), so divide raw value by sensitivity.
     */
    dps->x = (float)raw.x / mpu6050_state.sensitivity;
    dps->y = (float)raw.y / mpu6050_state.sensitivity;
    dps->z = (float)raw.z / mpu6050_state.sensitivity;

    return GYRO_OK;
}

gyro_status_t mpu6050_set_gyro_range(mpu6050_gyro_range_t range)
{
    gyro_status_t status;

    if (!mpu6050_state.initialized) {
        return GYRO_ERR_CONFIG;
    }

    status = mpu6050_write_reg(MPU6050_REG_GYRO_CONFIG, range);
    if (status != GYRO_OK) {
        return GYRO_ERR_I2C;
    }

    mpu6050_state.gyro_range  = range;
    mpu6050_state.sensitivity = mpu6050_get_sensitivity(range);

    return GYRO_OK;
}

gyro_status_t mpu6050_deinit(void)
{
    if (!mpu6050_state.initialized) {
        return GYRO_OK;
    }

    /*
     * Put the MPU-6050 back to sleep mode:
     * Set SLEEP bit (bit 6) in PWR_MGMT_1 register.
     */
    mpu6050_write_reg(MPU6050_REG_PWR_MGMT_1, MPU6050_PWR1_SLEEP);

    /* Close I2C */
    if (mpu6050_state.i2c != NULL) {
        i2c_close(mpu6050_state.i2c);
        mpu6050_state.i2c = NULL;
    }

    mpu6050_state.initialized = 0;

    return GYRO_OK;
}
