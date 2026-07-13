/**
 * @file max30102.h
 * @brief MAX30102 Driver - Public API
 *
 * This driver is responsible ONLY for:
 *   - Hardware initialization (I2C bring-up, Part ID verification)
 *   - Hardware configuration (sample rate, ADC range, pulse width,
 *     FIFO averaging, LED drive current)
 *   - Raw PPG data acquisition (RED & IR ADC counts from the FIFO)
 *
 * This driver is explicitly NOT responsible for any PPG signal
 * processing. The following belong in a separate, higher-level
 * library that consumes this driver's raw output:
 *   - DC removal / band-pass filtering
 *   - Moving average / smoothing
 *   - Peak detection
 *   - Heart rate estimation
 *   - SpO2 estimation
 *   - Motion artifact removal
 *   - Automatic calibration
 *   - Signal quality assessment
 *
 * The driver has a single responsibility: configure the sensor and
 * return raw ADC samples.
 */

#ifndef MAX30102_H
#define MAX30102_H

#include <stdint.h>
#include <stdbool.h>
#include "max30102_regs.h"

#ifdef __cplusplus
extern "C" {
#endif

// -------------------------------------------------------------------------
// STATUS CODES
// -------------------------------------------------------------------------

typedef enum {
    MAX30102_OK = 0,
    MAX30102_ERROR_I2C = -1,
    MAX30102_ERROR_NOT_FOUND = -2,
    MAX30102_ERROR_INVALID_PARAM = -3,
    MAX30102_ERROR_NO_DATA = -4
} max30102_status_t;

// -------------------------------------------------------------------------
// CONFIGURATION ENUMS
// -------------------------------------------------------------------------
// These enums give the user friendly, self-documenting names while their
// underlying values ARE the actual register bits from max30102_regs.h.
// This means max30102_configure() can simply OR the fields of
// max30102_config_t together with no lookup/translation step, keeping the
// driver a thin, lightweight hardware layer. The user still never needs
// to open max30102_regs.h or know what the bits mean.

/** SpO2 sample rate (samples per second). */
typedef enum {
    MAX30102_SR_50   = MAX30102_SPO2_SR_50,
    MAX30102_SR_100  = MAX30102_SPO2_SR_100,
    MAX30102_SR_200  = MAX30102_SPO2_SR_200,
    MAX30102_SR_400  = MAX30102_SPO2_SR_400,
    MAX30102_SR_800  = MAX30102_SPO2_SR_800,
    MAX30102_SR_1000 = MAX30102_SPO2_SR_1000,
    MAX30102_SR_1600 = MAX30102_SPO2_SR_1600,
    MAX30102_SR_3200 = MAX30102_SPO2_SR_3200
} max30102_sample_rate_t;

/** ADC full-scale range. */
typedef enum {
    MAX30102_ADC_RANGE_2048  = MAX30102_SPO2_ADC_2048,
    MAX30102_ADC_RANGE_4096  = MAX30102_SPO2_ADC_4096,
    MAX30102_ADC_RANGE_8192  = MAX30102_SPO2_ADC_8192,
    MAX30102_ADC_RANGE_16384 = MAX30102_SPO2_ADC_16384
} max30102_adc_range_t;

/** LED pulse width (also determines ADC resolution). */
typedef enum {
    MAX30102_PULSE_WIDTH_15BIT = MAX30102_SPO2_PW_69,  // ~69 us
    MAX30102_PULSE_WIDTH_16BIT = MAX30102_SPO2_PW_118, // ~118 us
    MAX30102_PULSE_WIDTH_17BIT = MAX30102_SPO2_PW_215, // ~215 us
    MAX30102_PULSE_WIDTH_18BIT = MAX30102_SPO2_PW_411  // ~411 us
} max30102_pulse_width_t;

/** Number of samples averaged per FIFO entry. */
typedef enum {
    MAX30102_FIFO_AVG_1  = MAX30102_SMP_AVE_1,
    MAX30102_FIFO_AVG_2  = MAX30102_SMP_AVE_2,
    MAX30102_FIFO_AVG_4  = MAX30102_SMP_AVE_4,
    MAX30102_FIFO_AVG_8  = MAX30102_SMP_AVE_8,
    MAX30102_FIFO_AVG_16 = MAX30102_SMP_AVE_16,
    MAX30102_FIFO_AVG_32 = MAX30102_SMP_AVE_32
} max30102_fifo_avg_t;

// -------------------------------------------------------------------------
// CONFIGURATION STRUCTURE
// -------------------------------------------------------------------------

/**
 * @brief Hardware configuration for the MAX30102.
 *
 * Red/IR LED current is hardware drive strength, not an algorithmic
 * calibration parameter, and is therefore part of this configuration.
 */
typedef struct {
    max30102_sample_rate_t sample_rate;
    max30102_adc_range_t   adc_range;
    max30102_pulse_width_t pulse_width;
    max30102_fifo_avg_t    fifo_avg;
    uint8_t red_led_current; // Red LED drive current (0x00 - 0xFF)
    uint8_t ir_led_current;  // IR LED drive current (0x00 - 0xFF)
} max30102_config_t;

// -------------------------------------------------------------------------
// SAMPLE STRUCTURE
// -------------------------------------------------------------------------

/** One raw PPG sample: RED and IR ADC counts (18-bit, in a 32-bit field). */
typedef struct {
    uint32_t red;
    uint32_t ir;
} max30102_sample_t;

// -------------------------------------------------------------------------
// DEVICE CONTEXT
// -------------------------------------------------------------------------

typedef struct {
    int i2c_port;              // I2C port number
    uint8_t i2c_addr;           // I2C address (default 0x57)
    void *i2c_dev;              // Pointer to PULP I2C device handle
    max30102_config_t config;   // Last configuration applied to the sensor
} max30102_t;

// -------------------------------------------------------------------------
// PUBLIC API
// -------------------------------------------------------------------------

/**
 * @brief Bring up the I2C link and verify the sensor is present.
 *
 * Opens the I2C port, stores it in the device context, verifies the
 * Part ID, and performs a soft reset. Does NOT apply any sensor
 * configuration - call max30102_configure() afterward.
 *
 * @param dev Pointer to MAX30102 context
 * @param i2c_port I2C port number
 * @return MAX30102_OK on success, error code otherwise
 */
max30102_status_t max30102_init(max30102_t *dev, int i2c_port);

/**
 * @brief Apply a hardware configuration to the sensor.
 *
 * Writes mode, sample rate, ADC range, pulse width, FIFO averaging,
 * LED drive current, and interrupt enables based on the supplied
 * configuration, then clears the FIFO. The enum fields in
 * max30102_config_t hold real register bit values, so they are
 * combined directly with no translation step.
 *
 * @param dev Pointer to MAX30102 context
 * @param config Pointer to the desired configuration
 * @return MAX30102_OK on success, MAX30102_ERROR_INVALID_PARAM if dev
 *         or config is NULL, or an I2C error code
 */
max30102_status_t max30102_configure(max30102_t *dev, const max30102_config_t *config);

/**
 * @brief Return a reasonable default hardware configuration.
 *
 * Default: 400 sps, 4096 ADC range, 18-bit pulse width, 4x FIFO
 * averaging, ~7 mA LED drive current on both channels.
 *
 * @return A populated max30102_config_t
 */
max30102_config_t max30102_get_default_config(void);

/**
 * @brief Soft reset the MAX30102.
 *
 * @param dev Pointer to MAX30102 context
 * @return MAX30102_OK on success, error code otherwise
 */
max30102_status_t max30102_reset(max30102_t *dev);

/**
 * @brief Read the Part ID of the sensor.
 *
 * @param dev Pointer to MAX30102 context
 * @param part_id Pointer to store the read part ID (should be 0x15)
 * @return MAX30102_OK on success, error code otherwise
 */
max30102_status_t max30102_get_part_id(max30102_t *dev, uint8_t *part_id);

/**
 * @brief Clear the FIFO read/write pointers and overflow counter.
 *
 * @param dev Pointer to MAX30102 context
 * @return MAX30102_OK on success, error code otherwise
 */
max30102_status_t max30102_clear_fifo(max30102_t *dev);

/**
 * @brief Read one raw PPG sample (RED and IR ADC counts).
 *
 * FIFO is an internal implementation detail of the driver; the caller
 * only needs to know that this returns one raw sample.
 *
 * @param dev Pointer to MAX30102 context
 * @param sample Pointer to store the RED/IR sample
 * @return MAX30102_OK on success,
 *         MAX30102_ERROR_NO_DATA if no new sample is available yet,
 *         error code otherwise
 */
max30102_status_t max30102_read_sample(max30102_t *dev, max30102_sample_t *sample);

#ifdef __cplusplus
}
#endif

#endif // MAX30102_H
