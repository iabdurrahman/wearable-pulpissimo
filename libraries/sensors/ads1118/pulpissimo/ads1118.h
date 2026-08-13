/*
 * Copyright (C) 2026 ICDeC
 *
 * Driver library for the Texas Instruments ADS1118 SPI ADC
 * on the ICDeC PULPissimo FPGA board.
 *
 * Used to read battery voltage through a single-ended channel (AIN0)
 * behind an external voltage divider.
 */
#ifndef __ADS1118_H__
#define __ADS1118_H__

#include <stdint.h>
#include "ads1118_regs.h"  /* MUX/PGA/DR constants used to fill ads1118_config_t */

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Status Codes
 * ============================================================================ */

typedef enum {
    ADS1118_OK        = 0,   /**< Operation completed successfully */
    ADS1118_ERR_SPI    = -1, /**< SPI communication error */
    ADS1118_ERR_NULL   = -2, /**< NULL pointer passed as argument */
    ADS1118_ERR_CONFIG = -3  /**< Invalid configuration / not initialized */
} ads1118_status_t;

/* ============================================================================
 * Configuration Structure
 * ============================================================================ */

typedef struct {
    int          spi_id;         /**< SPI peripheral ID */
    int          cs_pin;         /**< Chip-select line for this device */
    unsigned int spi_freq;       /**< SPI clock frequency in Hz */
    uint16_t     mux;            /**< Input multiplexer config, already bit-shifted
                                       (see ADS1118_MUX_* in ads1118_regs.h) */
    uint16_t     pga;            /**< Full-scale range / gain config, already bit-shifted
                                       (see ADS1118_PGA_* in ads1118_regs.h) */
    uint16_t     data_rate;      /**< Conversion data rate config, already bit-shifted
                                       (see ADS1118_DR_* in ads1118_regs.h) */
    float        vdivider_ratio; /**< R2/(R1+R2) of the external voltage divider.
                                       Use 1.0f if the battery is wired directly
                                       to AIN0 with no divider. */
} ads1118_config_t;

/* ============================================================================
 * API Functions
 * ============================================================================ */

/**
 * @brief Load a sensible default configuration.
 *
 * Default assumes: single-ended AIN0 vs GND, PGA=+-4.096V (safety margin
 * above an expected ~2.1V at AIN0), 128SPS, and a 2:1 voltage divider
 * (vdivider_ratio=0.5). Adjust vdivider_ratio to match your actual
 * hardware before calling ads1118_init().
 */
ads1118_status_t ads1118_default_config(ads1118_config_t *cfg);

/**
 * @brief Initialize the ADS1118 driver and open the SPI bus.
 */
ads1118_status_t ads1118_init(const ads1118_config_t *cfg);

/**
 * @brief Trigger a single-shot conversion and read back the raw 16-bit
 *        two's-complement ADC code.
 */
ads1118_status_t ads1118_read_raw(int16_t *raw);

/**
 * @brief Read the battery voltage, compensating for the configured
 *        voltage divider ratio.
 */
ads1118_status_t ads1118_read_battery_voltage(float *voltage);

/**
 * @brief Close the SPI bus and reset driver state.
 */
ads1118_status_t ads1118_deinit(void);

/* ============================================================================
 * Pure helper functions (exposed for direct unit testing, no hardware access)
 * ============================================================================ */

/**
 * @brief Build the 16-bit Config Register word for a single-shot conversion.
 */
uint16_t ads1118_encode_config(uint16_t mux, uint16_t pga, uint16_t data_rate);

/**
 * @brief Convert a raw two's-complement ADC code to volts, given a PGA setting.
 */
float ads1118_raw_to_voltage(int16_t raw, uint16_t pga);

#ifdef __cplusplus
}
#endif

#endif /* __ADS1118_H__ */