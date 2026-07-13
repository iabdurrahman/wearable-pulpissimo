#include <stdint.h>
#include <string.h>
#include "pulp.h"
#include "l3g4200d.h"

//Internal driver state
static struct {
    i2c_t          *i2c;           
    i2c_dev_t       i2c_dev;      
    uint8_t         i2c_addr;      
    l3g4200d_range_t range;        
    float           sensitivity;   
    int             initialized;  
} l3g4200d_state = { .initialized = 0 };

//Sensitivity Lookup
static float l3g4200d_get_sensitivity(l3g4200d_range_t range)
{
    switch (range) {
        case L3G4200D_RANGE_250DPS:   return 8.75f;
        case L3G4200D_RANGE_500DPS:   return 17.50f;
        case L3G4200D_RANGE_2000DPS:  return 70.00f;
        default:                      return 8.75f;
    }
}

//Low-Level I2C Helpers (with timeout + retry)

/**
 * @brief Write a single byte to a register on the L3G4200D.
 *
 * Includes retry mechanism with timeout detection to prevent
 * infinite blocking when the sensor does not respond.
 *
 * @param reg   Register address.
 * @param value Byte value to write.
 * @return GYRO_OK on success, GYRO_ERR_TIMEOUT after all retries exhausted.
 */
static gyro_status_t l3g4200d_write_reg(uint8_t reg, uint8_t value)
{
    unsigned char data[2];
    data[0] = reg;
    data[1] = value;

    for (int retry = 0; retry < GYRO_I2C_MAX_RETRIES; retry++) {
        int ret = i2c_write(l3g4200d_state.i2c, data, 2, 1);

        /* Check if I2C hardware timeout occurred */
        if (i2c_managetimeoutflag(true)) {
            printf("[L3G4200D] I2C write timeout (reg=0x%02X, retry=%d/%d)\n",
                   reg, retry + 1, GYRO_I2C_MAX_RETRIES);
            continue;
        }

        if (ret == 0) {
            return GYRO_OK;
        }

        printf("[L3G4200D] I2C write error (reg=0x%02X, ret=%d, retry=%d/%d)\n",
               reg, ret, retry + 1, GYRO_I2C_MAX_RETRIES);
    }

    return GYRO_ERR_TIMEOUT;
}

/**
 * @brief Read one or more bytes from a register on the L3G4200D.
 *
 * Includes retry mechanism with timeout detection. For multi-byte
 * reads, sets the MSB of the register address to enable auto-increment
 * (L3G4200D datasheet requirement).
 *
 * @param reg    Starting register address.
 * @param buffer Pointer to buffer for received data.
 * @param len    Number of bytes to read.
 * @return GYRO_OK on success, GYRO_ERR_TIMEOUT after all retries exhausted.
 */
static gyro_status_t l3g4200d_read_reg(uint8_t reg, uint8_t *buffer, int len)
{
    unsigned char reg_addr;

    /*
     * For multi-byte reads, set the MSB of the register address
     * to enable auto-increment (L3G4200D datasheet requirement).
     */
    if (len > 1) {
        reg_addr = reg | L3G4200D_AUTO_INCREMENT_ADDR;
    } else {
        reg_addr = reg;
    }

    for (int retry = 0; retry < GYRO_I2C_MAX_RETRIES; retry++) {
        /* Write register address (no STOP, so the sensor keeps the bus) */
        int ret = i2c_write(l3g4200d_state.i2c, &reg_addr, 1, 0);

        if (i2c_managetimeoutflag(true)) {
            printf("[L3G4200D] I2C addr write timeout (reg=0x%02X, retry=%d/%d)\n",
                   reg, retry + 1, GYRO_I2C_MAX_RETRIES);
            continue;
        }

        if (ret != 0) {
            printf("[L3G4200D] I2C addr write error (reg=0x%02X, ret=%d, retry=%d/%d)\n",
                   reg, ret, retry + 1, GYRO_I2C_MAX_RETRIES);
            continue;
        }

        /* Read data bytes */
        ret = i2c_read(l3g4200d_state.i2c, buffer, len, 0);

        if (i2c_managetimeoutflag(true)) {
            printf("[L3G4200D] I2C read timeout (reg=0x%02X, retry=%d/%d)\n",
                   reg, retry + 1, GYRO_I2C_MAX_RETRIES);
            continue;
        }

        /* Success */
        return GYRO_OK;
    }

    return GYRO_ERR_TIMEOUT;
}

//Public API Implementation

gyro_status_t l3g4200d_default_config(l3g4200d_config_t *cfg)
{
    if (cfg == NULL) {
        return GYRO_ERR_NULL;
    }

    cfg->i2c_addr = L3G4200D_I2C_ADDR_DEFAULT;
    cfg->i2c_id   = GYRO_DEFAULT_I2C_ID;
    cfg->i2c_freq = GYRO_DEFAULT_I2C_BAUDRATE;
    cfg->range    = L3G4200D_RANGE_250DPS;

    return GYRO_OK;
}

gyro_status_t l3g4200d_init(const l3g4200d_config_t *cfg)
{
    gyro_status_t status;
    uint8_t       who_am_i;
    uint8_t       read_val;

    if (cfg == NULL) {
        return GYRO_ERR_NULL;
    }

    printf("\n--- Test: Initialization ---\n");
    printf("[INIT] Opening I2C...\n");

    /* Configure I2C device */
    i2c_dev_init(&l3g4200d_state.i2c_dev);
    l3g4200d_state.i2c_dev.id           = cfg->i2c_id;
    /* 
     * pulp-runtime i2c.c expects the 8-bit base address in dev->cs.
     * It uses dev->cs directly for write and (dev->cs | 0x1) for read.
     * Thus, we must shift the 7-bit address left by 1.
     */
    l3g4200d_state.i2c_dev.cs           = (cfg->i2c_addr << 1); 
    l3g4200d_state.i2c_dev.max_baudrate = cfg->i2c_freq;

    printf("I2C Port : %d\n", l3g4200d_state.i2c_dev.id);
    printf("Address  : 0x%02X\n", (uint8_t)l3g4200d_state.i2c_dev.cs);
    printf("Baudrate : %d\n", l3g4200d_state.i2c_dev.max_baudrate);

    /* Open I2C bus */
    l3g4200d_state.i2c = i2c_open(&l3g4200d_state.i2c_dev);
    if (l3g4200d_state.i2c == NULL) {
        printf("FAIL\n");
        return GYRO_ERR_I2C;
    }
    printf("OK\n\n");

    printf("OK bang1\n\n");
    /* Set I2C timeout */
    i2c_settimeout(GYRO_I2C_TIMEOUT_US, 1);

    printf("OK bang\n\n");

    /* Store configuration */
    printf("sebelumnya");
    l3g4200d_state.i2c_addr   = cfg->i2c_addr;
    l3g4200d_state.range      = cfg->range;
    l3g4200d_state.sensitivity = l3g4200d_get_sensitivity(cfg->range);
    printf("disini");

    printf("[INIT] Waiting for sensor boot...\n");
    for (volatile int i = 0; i < GYRO_BOOT_DELAY_CYCLES; i++);
    printf("OK\n\n");

    /* Verify WHO_AM_I */
    printf("[INIT] Reading Part ID...\n");
    printf("[READ] Reg 0x%02X\n", L3G4200D_REG_WHO_AM_I);
    status = l3g4200d_read_reg(L3G4200D_REG_WHO_AM_I, &who_am_i, 1);
    if (status != GYRO_OK) {
        printf("FAIL: I2C Read Error (err=%d)\n", status);
        i2c_close(l3g4200d_state.i2c);
        return status;
    }
    printf("[READ] Value = 0x%02X\n", who_am_i);
    
    if (who_am_i != L3G4200D_WHO_AM_I_VALUE) {
        printf("FAIL: Mismatch (expected 0x%02X)\n", L3G4200D_WHO_AM_I_VALUE);
        i2c_close(l3g4200d_state.i2c);
        return GYRO_ERR_ID;
    }
    printf("OK\n\n");

    /* Configure CTRL_REG1 */
    uint8_t ctrl_reg1_val = L3G4200D_ODR_100HZ_BW12P5 | L3G4200D_NORMAL_MODE | L3G4200D_ALL_AXES_ENABLE;
    printf("[INIT] Configuring CTRL_REG1...\n");
    printf("[WRITE] Reg 0x%02X <- 0x%02X\n", L3G4200D_REG_CTRL_REG1, ctrl_reg1_val);
    status = l3g4200d_write_reg(L3G4200D_REG_CTRL_REG1, ctrl_reg1_val);
    if (status != GYRO_OK) {
        printf("FAIL: Write Error\n");
        i2c_close(l3g4200d_state.i2c);
        return GYRO_ERR_CONFIG;
    }

    /* Read back to verify */
    printf("[READ] Reg 0x%02X\n", L3G4200D_REG_CTRL_REG1);
    status = l3g4200d_read_reg(L3G4200D_REG_CTRL_REG1, &read_val, 1);
    if (status != GYRO_OK) {
        printf("FAIL: Read Error\n");
        i2c_close(l3g4200d_state.i2c);
        return GYRO_ERR_CONFIG;
    }
    printf("[READ] Value = 0x%02X\n", read_val);
    if (read_val != ctrl_reg1_val) {
        printf("FAIL: Verification mismatch\n");
        i2c_close(l3g4200d_state.i2c);
        return GYRO_ERR_CONFIG;
    }
    printf("OK\n\n");

    /* Configure CTRL_REG4 */
    uint8_t ctrl_reg4_val = L3G4200D_BDU_BLOCKED | cfg->range;
    printf("[INIT] Configuring CTRL_REG4...\n");
    printf("[WRITE] Reg 0x%02X <- 0x%02X\n", L3G4200D_REG_CTRL_REG4, ctrl_reg4_val);
    status = l3g4200d_write_reg(L3G4200D_REG_CTRL_REG4, ctrl_reg4_val);
    if (status != GYRO_OK) {
        printf("FAIL: Write Error\n");
        i2c_close(l3g4200d_state.i2c);
        return GYRO_ERR_CONFIG;
    }

    /* Read back to verify */
    printf("[READ] Reg 0x%02X\n", L3G4200D_REG_CTRL_REG4);
    status = l3g4200d_read_reg(L3G4200D_REG_CTRL_REG4, &read_val, 1);
    if (status != GYRO_OK) {
        printf("FAIL: Read Error\n");
        i2c_close(l3g4200d_state.i2c);
        return GYRO_ERR_CONFIG;
    }
    printf("[READ] Value = 0x%02X\n", read_val);
    if (read_val != ctrl_reg4_val) {
        printf("FAIL: Verification mismatch\n");
        i2c_close(l3g4200d_state.i2c);
        return GYRO_ERR_CONFIG;
    }
    printf("OK\n\n");

    l3g4200d_state.initialized = 1;
    printf("[PASS] l3g4200d_init returns OK\n");

    return GYRO_OK;
}

gyro_status_t l3g4200d_who_am_i(uint8_t *id)
{
    if (id == NULL) {
        return GYRO_ERR_NULL;
    }
    if (!l3g4200d_state.initialized) {
        return GYRO_ERR_CONFIG;
    }

    return l3g4200d_read_reg(L3G4200D_REG_WHO_AM_I, id, 1);
}

gyro_status_t l3g4200d_read_raw(gyro_raw_t *raw)
{
    uint8_t buffer[6];
    gyro_status_t status;

    if (raw == NULL) {
        return GYRO_ERR_NULL;
    }
    if (!l3g4200d_state.initialized) {
        return GYRO_ERR_CONFIG;
    }

    /* Read 6 bytes starting from OUT_X_L with auto-increment */
    status = l3g4200d_read_reg(L3G4200D_REG_OUT_X_L, buffer, 6);
    if (status != GYRO_OK) {
        return status;
    }

    /* L3G4200D stores data in little-endian (low byte first) */
    raw->x = (int16_t)((buffer[1] << 8) | buffer[0]);
    raw->y = (int16_t)((buffer[3] << 8) | buffer[2]);
    raw->z = (int16_t)((buffer[5] << 8) | buffer[4]);

    return GYRO_OK;
}

gyro_status_t l3g4200d_read_dps(gyro_dps_t *dps)
{
    gyro_raw_t raw;
    gyro_status_t status;

    if (dps == NULL) {
        return GYRO_ERR_NULL;
    }

    status = l3g4200d_read_raw(&raw);
    if (status != GYRO_OK) {
        return status;
    }

    /* Convert raw data to degrees per second.
     * Sensitivity is in mdps/digit, so divide by 1000 to get dps. */
    dps->x = (float)raw.x * l3g4200d_state.sensitivity / 1000.0f;
    dps->y = (float)raw.y * l3g4200d_state.sensitivity / 1000.0f;
    dps->z = (float)raw.z * l3g4200d_state.sensitivity / 1000.0f;

    return GYRO_OK;
}

gyro_status_t l3g4200d_set_range(l3g4200d_range_t range)
{
    gyro_status_t status;

    if (!l3g4200d_state.initialized) {
        return GYRO_ERR_CONFIG;
    }

    /* Write new range to CTRL_REG4 (keep BDU=1) */
    status = l3g4200d_write_reg(L3G4200D_REG_CTRL_REG4,
                                L3G4200D_BDU_BLOCKED | range);
    if (status != GYRO_OK) {
        return GYRO_ERR_I2C;
    }

    l3g4200d_state.range       = range;
    l3g4200d_state.sensitivity = l3g4200d_get_sensitivity(range);

    return GYRO_OK;
}

gyro_status_t l3g4200d_deinit(void)
{
    if (!l3g4200d_state.initialized) {
        return GYRO_OK;
    }

    /*
     * Power down the sensor:
     * Set CTRL_REG1 PD bit to 0 (power-down mode), disable all axes.
     */
    l3g4200d_write_reg(L3G4200D_REG_CTRL_REG1, L3G4200D_POWER_DOWN);

    /* Close I2C */
    if (l3g4200d_state.i2c != NULL) {
        i2c_close(l3g4200d_state.i2c);
        l3g4200d_state.i2c = NULL;
    }

    l3g4200d_state.initialized = 0;

    return GYRO_OK;
}
