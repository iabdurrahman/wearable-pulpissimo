/**
 * @file hr_max30102.h
 * @brief Heart-Rate Module for MAX30102 - Public API
 *
 * This module is a thin integration layer that wires together two
 * pieces that already exist in this codebase and are deliberately kept
 * single-responsibility:
 *
 *   - max30102.c/.h  : raw I2C driver, returns raw RED/IR ADC counts
 *   - ppg_hr.c/.h     : SparkFun PBA beat-detection + BPM averaging,
 *                        ported unmodified, operates on a raw IR stream
 *
 * Neither of those two files know about each other. This module is the
 * "glue": it owns one max30102_t + one ppg_hr_t, configures the sensor
 * with sane defaults for HR sensing, and exposes a single per-tick call
 * (hr_max30102_process) that a main loop can call at whatever cadence
 * new FIFO data may be available.
 */

#ifndef HR_MAX30102_H
#define HR_MAX30102_H

#include <stdint.h>
#include <stdbool.h>

#include "max30102.h"
#include "ppg_hr.h"

#ifdef __cplusplus
extern "C" {
#endif

// -------------------------------------------------------------------------
// CONTEXT STRUCTURE
// -------------------------------------------------------------------------

typedef struct {
    max30102_t   sensor;      // owns the raw driver context
    ppg_hr_t     hr;          // owns the ported SparkFun beat detector
    max30102_config_t config; // last config applied (kept for reference)
} hr_max30102_t;

// -------------------------------------------------------------------------
// RESULT STRUCTURE
// -------------------------------------------------------------------------

typedef struct {
    bool sample_available;   // true if a new FIFO sample was read this call
    bool finger_detected;    // crude "IR high enough" gate (see .c file)
    bool beat_detected;      // true if a heartbeat was detected this call
    int32_t bpm;              // instantaneous BPM (0 if none yet)
    int32_t avg_bpm;          // moving-average BPM over PPG_HR_RATE_SIZE beats
    max30102_sample_t raw;   // raw RED/IR of this tick (0,0 if none available)
} hr_max30102_result_t;

// -------------------------------------------------------------------------
// PUBLIC API
// -------------------------------------------------------------------------

/**
 * @brief Bring up the MAX30102 and the beat detector with HR-oriented
 *        defaults (matches SparkFun's Example5_HeartRate.ino settings,
 *        i.e. build_sparkfun_example_config() from test_ppg_hr.c: 400
 *        sps / 4096 ADC range / 18-bit pulse width / 4x FIFO averaging,
 *        IR LED at the driver's default 0x1F, RED LED dimmed to 0x0A
 *        purely as a visual indicator - checkForBeat() never reads RED).
 *
 * @param hr        Pointer to module context
 * @param i2c_port  I2C port number to pass to max30102_init()
 * @return MAX30102_OK on success, error code otherwise (init/config
 *         failure). On error, hr->hr is still ppg_hr_init()'d so the
 *         struct is in a safe, defined state either way.
 */
max30102_status_t hr_max30102_init(hr_max30102_t *hr, int i2c_port);

/**
 * @brief Bring up the MAX30102 and beat detector with a caller-supplied
 *        hardware configuration (e.g. if SpO2 mode / a shared config is
 *        already being driven elsewhere - see spo2_max30102.h for why
 *        HR and SpO2 can safely share one sensor config).
 *
 * @param hr        Pointer to module context
 * @param i2c_port  I2C port number to pass to max30102_init()
 * @param config    Hardware configuration to apply
 * @return MAX30102_OK on success, error code otherwise
 */
max30102_status_t hr_max30102_init_with_config(hr_max30102_t *hr, int i2c_port,
                                                const max30102_config_t *config);

/**
 * @brief Poll the FIFO once and, if a new sample is available, run it
 *        through the beat detector.
 *
 * Safe to call as fast as the main loop allows: if no new FIFO entry is
 * ready yet, this returns immediately with sample_available = false and
 * leaves all detector state untouched (mirrors max30102_read_sample()'s
 * MAX30102_ERROR_NO_DATA being a normal, expected condition, not a
 * fault - see max30102.c).
 *
 * @param hr      Pointer to module context
 * @param now_ms  Current time in ms (monotonic, caller-supplied - see
 *                TIMING NOTE above)
 * @return Populated result. On a hard I2C error (rare - the FIFO is
 *         polled, not interrupt-driven, so transient NO_DATA is not an
 *         error), sample_available is false and raw is {0,0}.
 */
hr_max30102_result_t hr_max30102_process(hr_max30102_t *hr, uint32_t now_ms);

/**
 * @brief Reset only the beat-detector state (e.g. after a "finger
 *        removed" event upstream), without touching the sensor
 *        configuration or re-initializing I2C.
 *
 * @param hr Pointer to module context
 */
void hr_max30102_reset(hr_max30102_t *hr);

#ifdef __cplusplus
}
#endif

#endif