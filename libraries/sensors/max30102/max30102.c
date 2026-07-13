/**
 * @file max30102.c
 * @brief MAX30102 Driver Implementation
 *
 * Implements the MAX30102 library API for PULP/Nusacore FPGA boards.
 *
 * Single responsibility: sensor configuration and raw ADC acquisition.
 * PPG signal processing (filtering, HR, SpO2, etc.) is out of scope
 * and lives in a separate library that consumes this driver.
 */

#include "max30102.h"
#include "max30102_regs.h"

// PULP Runtime includes
#ifdef SOFTWARE_TEST
    // Mock I2C structures for host PC testing
    typedef struct {
        int id;
        char cs;
        unsigned int max_baudrate;
    } i2c_dev_t;
    typedef void i2c_t;

    extern i2c_t *i2c_open(i2c_dev_t *dev);
    extern int i2c_write(i2c_t *dev, unsigned char *data, int length, int send_stop);
    extern int i2c_read(i2c_t *dev_i2c, unsigned char *rx_buff, int length, int pending);
    extern void i2c_dev_init(i2c_dev_t *dev);
#else
    #include "pulp.h"
#endif

// -------------------------------------------------------------------------
// INTERNAL I2C HELPER FUNCTIONS (unchanged)
// -------------------------------------------------------------------------

static max30102_status_t i2c_write_reg(max30102_t *dev, uint8_t reg, uint8_t value) {
    if (!dev || !dev->i2c_dev) return MAX30102_ERROR_INVALID_PARAM;

    // In PULP runtime, to write a register, we send the register address followed by the data
    uint8_t tx_data[2] = {reg, value};

    // Write 2 bytes: register address and value. send_stop = 1 (send stop bit)
    int ret = i2c_write((i2c_t *)dev->i2c_dev, tx_data, 2, 1);

    if (ret != 0) {
        return MAX30102_ERROR_I2C;
    }

    return MAX30102_OK;
}

static max30102_status_t i2c_read_reg(max30102_t *dev, uint8_t reg, uint8_t *value) {
    if (!dev || !dev->i2c_dev || !value) return MAX30102_ERROR_INVALID_PARAM;

    // Write register address without stop (send_stop = 0)
    int ret = i2c_write((i2c_t *)dev->i2c_dev, &reg, 1, 0);
    if (ret != 0) {
        return MAX30102_ERROR_I2C;
    }

    // Read 1 byte and send stop (pending = 0)
    ret = i2c_read((i2c_t *)dev->i2c_dev, value, 1, 0);
    if (ret <= 0) {
        return MAX30102_ERROR_I2C;
    }

    return MAX30102_OK;
}

static max30102_status_t i2c_read_regs(max30102_t *dev, uint8_t reg, uint8_t *data, int len) {
    if (!dev || !dev->i2c_dev || !data || len <= 0) return MAX30102_ERROR_INVALID_PARAM;

    // Write register address without stop (send_stop = 0)
    int ret = i2c_write((i2c_t *)dev->i2c_dev, &reg, 1, 0);
    if (ret != 0) {
        return MAX30102_ERROR_I2C;
    }

    // Read len bytes and send stop (pending = 0)
    ret = i2c_read((i2c_t *)dev->i2c_dev, data, len, 0);
    if (ret <= 0) {
        return MAX30102_ERROR_I2C;
    }

    return MAX30102_OK;
}

// -------------------------------------------------------------------------
// PUBLIC API IMPLEMENTATION
// -------------------------------------------------------------------------

max30102_status_t max30102_init(max30102_t *dev, int i2c_port) {
    if (!dev) return MAX30102_ERROR_INVALID_PARAM;

    dev->i2c_port = i2c_port;
    dev->i2c_addr = MAX30102_I2C_ADDR;

    i2c_dev_t conf;
    i2c_dev_init(&conf);
    conf.id = i2c_port;
    conf.cs = dev->i2c_addr << 1;
    conf.max_baudrate = 100000;

    dev->i2c_dev = (void *)i2c_open(&conf);
    if (!dev->i2c_dev) {
        return MAX30102_ERROR_I2C;
    }

    // Check Part ID to verify connection
    uint8_t part_id = 0;
    if (max30102_get_part_id(dev, &part_id) != MAX30102_OK || part_id != MAX30102_EXPECTED_PART_ID) {
        return MAX30102_ERROR_NOT_FOUND;
    }

    // Soft Reset. Sensor configuration is applied separately via
    // max30102_configure().
    return max30102_reset(dev);
}

max30102_config_t max30102_get_default_config(void) {
    max30102_config_t config;
    config.sample_rate     = MAX30102_SR_400;
    config.adc_range       = MAX30102_ADC_RANGE_4096;
    config.pulse_width     = MAX30102_PULSE_WIDTH_18BIT;
    config.fifo_avg        = MAX30102_FIFO_AVG_4;
    config.red_led_current = 0x24; // ~7 mA
    config.ir_led_current  = 0x24; // ~7 mA
    return config;
}

max30102_status_t max30102_configure(max30102_t *dev, const max30102_config_t *config) {
    if (!dev || !config) return MAX30102_ERROR_INVALID_PARAM;

    max30102_status_t status;

    // Mode: SpO2 (RED + IR)
    status = i2c_write_reg(dev, MAX30102_REG_MODE_CONFIG, MAX30102_MODE_SPO2);
    if (status != MAX30102_OK) return status;

    // SpO2 configuration: enum values already hold the register bits,
    // so they are combined directly with no translation step.
    uint8_t spo2_cfg = config->adc_range | config->sample_rate | config->pulse_width;
    status = i2c_write_reg(dev, MAX30102_REG_SPO2_CONFIG, spo2_cfg);
    if (status != MAX30102_OK) return status;

    // FIFO configuration: sample averaging + rollover enabled
    uint8_t fifo_cfg = config->fifo_avg | MAX30102_FIFO_ROLLOVER_EN;
    status = i2c_write_reg(dev, MAX30102_REG_FIFO_CONFIG, fifo_cfg);
    if (status != MAX30102_OK) return status;

    // LED drive current (hardware setting, not an algorithmic calibration)
    status = i2c_write_reg(dev, MAX30102_REG_LED1_PA, config->red_led_current);
    if (status != MAX30102_OK) return status;

    status = i2c_write_reg(dev, MAX30102_REG_LED2_PA, config->ir_led_current);
    if (status != MAX30102_OK) return status;

    // Enable A_FULL (FIFO Almost Full) and PPG_RDY interrupts
    status = i2c_write_reg(dev, MAX30102_REG_INT_ENABLE_1, MAX30102_INT_A_FULL_EN | MAX30102_INT_PPG_RDY_EN);
    if (status != MAX30102_OK) return status;

    status = i2c_write_reg(dev, MAX30102_REG_INT_ENABLE_2, 0x00);
    if (status != MAX30102_OK) return status;

    dev->config = *config;

    return max30102_clear_fifo(dev);
}

max30102_status_t max30102_reset(max30102_t *dev) {
    max30102_status_t status = i2c_write_reg(dev, MAX30102_REG_MODE_CONFIG, MAX30102_MODE_RESET);
    if (status != MAX30102_OK) return status;

    // Wait for reset bit to clear
    uint8_t mode = MAX30102_MODE_RESET;
    while (mode & MAX30102_MODE_RESET) {
        status = i2c_read_reg(dev, MAX30102_REG_MODE_CONFIG, &mode);
        if (status != MAX30102_OK) return status;

        // Add a small delay if possible, or busy wait
        for (volatile int i = 0; i < 10000; i++);
    }

    return MAX30102_OK;
}

max30102_status_t max30102_get_part_id(max30102_t *dev, uint8_t *part_id) {
    return i2c_read_reg(dev, MAX30102_REG_PART_ID, part_id);
}

max30102_status_t max30102_clear_fifo(max30102_t *dev) {
    max30102_status_t status;

    status = i2c_write_reg(dev, MAX30102_REG_FIFO_WR_PTR, 0x00);
    if (status != MAX30102_OK) return status;

    status = i2c_write_reg(dev, MAX30102_REG_OVF_COUNTER, 0x00);
    if (status != MAX30102_OK) return status;

    status = i2c_write_reg(dev, MAX30102_REG_FIFO_RD_PTR, 0x00);
    if (status != MAX30102_OK) return status;

    return MAX30102_OK;
}

max30102_status_t max30102_read_sample(max30102_t *dev, max30102_sample_t *sample) {
    if (!dev || !sample) return MAX30102_ERROR_INVALID_PARAM;

    uint8_t wr_ptr, rd_ptr;
    max30102_status_t status;

    status = i2c_read_reg(dev, MAX30102_REG_FIFO_WR_PTR, &wr_ptr);
    if (status != MAX30102_OK) return status;

    status = i2c_read_reg(dev, MAX30102_REG_FIFO_RD_PTR, &rd_ptr);
    if (status != MAX30102_OK) return status;

    // Check if new data is available
    if (wr_ptr == rd_ptr) {
        return MAX30102_ERROR_NO_DATA;
    }

    // Read 6 bytes of data (3 bytes RED, 3 bytes IR)
    uint8_t buffer[6];
    status = i2c_read_regs(dev, MAX30102_REG_FIFO_DATA, buffer, 6);
    if (status != MAX30102_OK) return status;

    // Combine 3 bytes into 18-bit data for RED
    sample->red = ((uint32_t)buffer[0] << 16) | ((uint32_t)buffer[1] << 8) | buffer[2];
    sample->red &= 0x03FFFF; // Mask to 18-bit

    // Combine 3 bytes into 18-bit data for IR
    sample->ir = ((uint32_t)buffer[3] << 16) | ((uint32_t)buffer[4] << 8) | buffer[5];
    sample->ir &= 0x03FFFF; // Mask to 18-bit

    return MAX30102_OK;
}
