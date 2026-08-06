/*
 * Copyright (C) 2026 ICDeC
 *
 * Driver library for the Texas Instruments ADS1118 SPI ADC
 * on the ICDeC PULPissimo FPGA board.
 *
 * Uses the project's real SPI library (spim_open/spim_transfer/spim_close,
 * spim_conf_t/spim_t) as provided by the mentor (see spi.c, and the SPI
 * section of include/pulp.h). This is a simplified, renamed variant of
 * pulp-rt's rt_spim_* API, consistent with how i2c_open/i2c_write/i2c_read
 * simplify rt_i2c_* for the I2C-based drivers in this codebase (e.g.
 * mpu6050.c).
 *
 * CONFIRMED against include/pulp.h (SPI section):
 * - spim_conf_t fields: max_baudrate, wordsize, big_endian, polarity,
 *   phase, cs_gpio, cs, id, bitOrder.
 * - spim_cs_e: SPIM_CS_AUTO (release CS after transfer, used below since
 *   every ADS1118 exchange here is self-contained), SPIM_CS_KEEP (hold CS
 *   low for a following transfer), SPIM_CS_NONE (leave CS untouched).
 * - spim_transfer()'s len parameter is in bits (confirmed via spi.c's
 *   internal buffer_size = len/8 computation), hence 32 for our 4-byte
 *   tx/rx buffers.
 *
 * NOTE (no transfer error reporting): spim_transfer() returns void, so a
 * failed/incomplete SPI transaction cannot be detected from its return
 * value the way the mock SPI layer in the unit tests can simulate. Only
 * spim_open() failure (NULL handle) is observable as an error here.
 */

#include <stdio.h>
#include "pulp.h"
#include "ads1118.h"  /* also brings in ads1118_regs.h */

/* -------------------------------------------------------------------------
 * Internal driver state (singleton, following the mpu6050.c convention
 * used elsewhere in this codebase)
 * ------------------------------------------------------------------------- */
static struct {
    spim_t     *spim;
    spim_conf_t spim_conf;
    uint16_t    mux;
    uint16_t    pga;
    uint16_t    data_rate;
    float       vdivider_ratio;
    int         initialized;
} ads1118_state = { .initialized = 0 };

/* -------------------------------------------------------------------------
 * Internal helpers
 * ------------------------------------------------------------------------- */

/**
 * @brief Return the full-scale voltage range (Volts) for a given PGA setting.
 */
static float ads1118_pga_to_fs_voltage(uint16_t pga)
{
    switch (pga) {
        case ADS1118_PGA_FS_6144: return ADS1118_FS_VOLTAGE_6144;
        case ADS1118_PGA_FS_4096: return ADS1118_FS_VOLTAGE_4096;
        case ADS1118_PGA_FS_2048: return ADS1118_FS_VOLTAGE_2048;
        case ADS1118_PGA_FS_1024: return ADS1118_FS_VOLTAGE_1024;
        case ADS1118_PGA_FS_0512: return ADS1118_FS_VOLTAGE_0512;
        case ADS1118_PGA_FS_0256: return ADS1118_FS_VOLTAGE_0256;
        default:                  return ADS1118_FS_VOLTAGE_2048;
    }
}

/**
 * @brief Rough busy-wait scaled to the selected data rate.
 *
 * This is a placeholder delay, not calibrated against a real timer.
 * Replace with a proper timer API (e.g. rt_time_wait_us, if available in
 * the SDK) once running on real hardware/GVSoC, so conversion timing is
 * accurate instead of an arbitrary busy loop.
 */
static void ads1118_delay_for_datarate(uint16_t data_rate)
{
    volatile int iterations;

    switch (data_rate) {
        case ADS1118_DR_8SPS:   iterations = 400000; break;
        case ADS1118_DR_16SPS:  iterations = 200000; break;
        case ADS1118_DR_32SPS:  iterations = 100000; break;
        case ADS1118_DR_64SPS:  iterations = 50000;  break;
        case ADS1118_DR_128SPS: iterations = 25000;  break;
        case ADS1118_DR_250SPS: iterations = 13000;  break;
        case ADS1118_DR_475SPS: iterations = 7000;   break;
        case ADS1118_DR_860SPS: iterations = 4000;   break;
        default:                iterations = 25000;  break;
    }

    for (volatile int i = 0; i < iterations; i++);
}

/**
 * @brief Perform one 32-bit SPI exchange with the ADS1118.
 *
 * Sends the same 16-bit config word twice (per the datasheet's 32-bit
 * transfer cycle), and receives 4 bytes back: DATA_MSB, DATA_LSB,
 * CONFIG_MSB, CONFIG_LSB. The DATA bytes reflect whichever conversion
 * result was already buffered when this transfer began - see
 * ads1118_read_raw() for the two-step trigger + retrieve sequence this
 * requires to get a fresh reading.
 *
 * spim_transfer()'s length argument is in BITS, not bytes, hence 32 for
 * our 4-byte tx/rx buffers. SPIM_CS_AUTO releases CS at the end of this
 * transfer, since each ADS1118 exchange here is self-contained.
 */
static ads1118_status_t ads1118_spi_exchange(uint16_t config_word,
                                              int16_t *out_data,
                                              uint16_t *out_config_readback)
{
    uint8_t tx[4];
    uint8_t rx[4];

    if (ads1118_state.spim == NULL) {
        return ADS1118_ERR_SPI;
    }

    tx[0] = (uint8_t)(config_word >> 8);
    tx[1] = (uint8_t)(config_word & 0xFF);
    tx[2] = (uint8_t)(config_word >> 8);
    tx[3] = (uint8_t)(config_word & 0xFF);

    spim_transfer(ads1118_state.spim, tx, rx, 32, SPIM_CS_AUTO);

    if (out_data != NULL) {
        *out_data = (int16_t)((rx[0] << 8) | rx[1]);
    }
    if (out_config_readback != NULL) {
        *out_config_readback = (uint16_t)((rx[2] << 8) | rx[3]);
    }

    return ADS1118_OK;
}

/* -------------------------------------------------------------------------
 * Public API Implementation
 * ------------------------------------------------------------------------- */

uint16_t ads1118_encode_config(uint16_t mux, uint16_t pga, uint16_t data_rate)
{
    uint16_t cfg = 0;

    cfg |= ADS1118_OS_START;         /* request a new single-shot conversion */
    cfg |= mux;
    cfg |= pga;
    cfg |= ADS1118_MODE_SINGLESHOT;
    cfg |= data_rate;
    cfg |= ADS1118_TS_MODE_ADC;
    cfg |= ADS1118_PULLUP_DISABLE;
    cfg |= ADS1118_NOP_VALID;
    cfg |= ADS1118_RESERVED_BIT0;

    return cfg;
}

float ads1118_raw_to_voltage(int16_t raw, uint16_t pga)
{
    float fs = ads1118_pga_to_fs_voltage(pga);
    return ((float)raw / 32767.0f) * fs;        /* Perbaikan selanjutnya: ubah pembagi jadi 32768*/
}

ads1118_status_t ads1118_default_config(ads1118_config_t *cfg)
{
    if (cfg == NULL) {
        return ADS1118_ERR_NULL;
    }

    cfg->spi_id         = 0;
    cfg->cs_pin         = 0;
    cfg->spi_freq       = 1000000;               /* 1MHz, within SCLK period spec (>=250ns) */
    cfg->mux            = ADS1118_MUX_AIN0_GND;  /* single-ended, battery via divider */
    cfg->pga            = ADS1118_PGA_FS_4096;   /* margin above ~2.1V expected at AIN0 */
    cfg->data_rate      = ADS1118_DR_128SPS;     /* chip default rate */
    cfg->vdivider_ratio = 0.5f;                  /* ASSUMED 2:1 divider - confirm with hardware! */

    return ADS1118_OK;
}

ads1118_status_t ads1118_init(const ads1118_config_t *cfg)
{
    if (cfg == NULL) {
        return ADS1118_ERR_NULL;
    }

    if (cfg->vdivider_ratio <= 0.0f) {
        return ADS1118_ERR_CONFIG;
    }

    spim_conf_init(&ads1118_state.spim_conf);
    ads1118_state.spim_conf.id           = cfg->spi_id;
    ads1118_state.spim_conf.cs           = cfg->cs_pin;
    ads1118_state.spim_conf.max_baudrate = cfg->spi_freq;
    /* ADS1118 requires SPI mode 1 per datasheet (CPOL=0, CPHA=1) */
    ads1118_state.spim_conf.polarity     = 0;
    ads1118_state.spim_conf.phase        = 1;

    ads1118_state.spim = spim_open(&ads1118_state.spim_conf);
    if (ads1118_state.spim == NULL) {
        return ADS1118_ERR_SPI;
    }

    ads1118_state.mux            = cfg->mux;
    ads1118_state.pga            = cfg->pga;
    ads1118_state.data_rate      = cfg->data_rate;
    ads1118_state.vdivider_ratio = cfg->vdivider_ratio;
    ads1118_state.initialized    = 1;

    printf("[ADS1118] Initialized (mux=0x%02X, pga=0x%02X, dr=0x%02X, divider=%d/1000)\n",
           cfg->mux, cfg->pga, cfg->data_rate, (int)(cfg->vdivider_ratio * 1000));

    return ADS1118_OK;
}

ads1118_status_t ads1118_read_raw(int16_t *raw)
{
    uint16_t cfg, readback1, readback2;
    ads1118_status_t status;

    if (raw == NULL) {
        return ADS1118_ERR_NULL;
    }
    if (!ads1118_state.initialized) {
        return ADS1118_ERR_CONFIG;
    }

    cfg = ads1118_encode_config(ads1118_state.mux, ads1118_state.pga, ads1118_state.data_rate);

    /* Step 1: trigger a new single-shot conversion (returned data is stale) */
    status = ads1118_spi_exchange(cfg, raw, &readback1);
    if (status != ADS1118_OK) {
        return status;
    }

    /* Step 2: wait for the conversion to complete */
    ads1118_delay_for_datarate(ads1118_state.data_rate);

    /* Step 3: clock out the conversion result triggered in step 1 */
    status = ads1118_spi_exchange(cfg, raw, &readback2);
    if (status != ADS1118_OK) {
        return status;
    }

    return ADS1118_OK;
}

ads1118_status_t ads1118_read_battery_voltage(float *voltage)
{
    int16_t raw;
    float measured;
    ads1118_status_t status;

    if (voltage == NULL) {
        return ADS1118_ERR_NULL;
    }
    if (!ads1118_state.initialized) {
        return ADS1118_ERR_CONFIG;
    }

    status = ads1118_read_raw(&raw);
    if (status != ADS1118_OK) {
        return status;
    }

    measured = ads1118_raw_to_voltage(raw, ads1118_state.pga);
    *voltage = measured / ads1118_state.vdivider_ratio;

    return ADS1118_OK;
}

ads1118_status_t ads1118_deinit(void)
{
    if (!ads1118_state.initialized) {
        return ADS1118_OK;
    }

    if (ads1118_state.spim != NULL) {
        spim_close(ads1118_state.spim);
        ads1118_state.spim = NULL;
    }

    ads1118_state.initialized = 0;

    return ADS1118_OK;
}