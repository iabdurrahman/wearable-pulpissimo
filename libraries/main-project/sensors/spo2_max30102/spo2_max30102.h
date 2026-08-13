#ifndef SPO2_MAX30102_H
#define SPO2_MAX30102_H

#include <stdint.h>
#include <stdbool.h>
#include "max30102.h"

typedef struct {
    float dc_alpha_fast;      // used for the first dc_alpha_fast_samples samples
    float dc_alpha_slow;      // used afterward (slower = more stable baseline)
    uint32_t dc_alpha_fast_samples;

    float min_dc_counts;      // both |dcIr| and |dcRed| must exceed this
    float min_ac_counts;      // both acIrAmp and acRedAmp must exceed this
    float min_perfusion_index; // both piIr and piRed must exceed this
    float ratio_min;          // ratioR must be within [ratio_min, ratio_max]
    float ratio_max;

    float cal_a;              // SpO2 = cal_a - cal_b * ratioR
    float cal_b;
    float spo2_clamp_min;
    float spo2_clamp_max;

    uint32_t min_beats_before_output; // SPO2_MIN_BEATS in the source (there: 3)

    float ema_alpha;          // exponential smoothing factor applied last
} spo2_max30102_config_t;

/**
 * @brief Return the AFE4400-source starting configuration, unmodified
 *        in shape but explicitly documented as needing re-calibration
 *        on MAX30102 hardware.
 */
spo2_max30102_config_t spo2_max30102_get_default_config(void);

// CONTEXT STRUCTURE
#define SPO2_MAX30102_MEDIAN_HISTORY 5

typedef struct {
    spo2_max30102_config_t config;

    // DC estimators (source section 1)
    float dcIr;
    float dcRed;
    bool dc_initialized;
    uint32_t sample_count;

    // Running peak/valley within the current beat cycle (this port's
    // own addition - see file header point 3)
    uint32_t irMin, irMax;
    uint32_t redMin, redMax;
    bool cycle_has_samples;

    // Beat-count gate
    uint32_t beats_since_start;

    // Output smoothing history
    float median_history[SPO2_MAX30102_MEDIAN_HISTORY];
    uint32_t median_count; // number of valid entries written so far (caps at MEDIAN_HISTORY)
    uint32_t median_index; // next write position (circular)
    float ema_value;
    bool ema_initialized;

    // Last computed values
    float last_spo2_raw;   // before smoothing
    float last_spo2;       // after median + EMA smoothing
    bool has_valid_output;
} spo2_max30102_t;

// RESULT STRUCTURE
typedef struct {
    bool updated;       // true if spo2/spo2_raw were freshly computed this
                         // beat cycle; false means the previous values were
                         // held (frozen) because the quality gate failed
                         // or not enough beats have been seen yet
    float spo2;          // smoothed output, valid only if has_valid_output
    float spo2_raw;       // pre-smoothing value from this cycle (0 if not updated)
    float ratio_r;        // last computed R = PI_ir / PI_red (0 if gate failed)
    bool has_valid_output; // becomes true once at least one value has ever
                            // passed the quality gate
} spo2_max30102_result_t;

/**
 * @brief Initialize/reset the SpO2 estimator state.
 *
 * @param spo2   Pointer to module context
 * @param config Configuration to use, or NULL to use
 *               spo2_max30102_get_default_config()
 */
void spo2_max30102_init(spo2_max30102_t *spo2, const spo2_max30102_config_t *config);

/**
 * @brief Feed one raw RED/IR sample into the estimator.
 *
 * Call this once per raw sample coming out of max30102_read_sample(),
 * on the SAME samples fed to hr_max30102_process() / ppg_hr_process()
 * (both channels need to stay time-aligned for the ratio math to be
 * meaningful). Every call updates the DC estimate and the running
 * min/max for the current beat cycle; only calls where
 * beat_boundary == true trigger an SpO2 recomputation (using the
 * min/max accumulated since the previous boundary), matching the
 * source's beat-gated update cadence (SPO2_MIN_BEATS beats required
 * before the first output).
 *
 * @param spo2           Pointer to module context
 * @param sample         Raw RED/IR sample for this tick
 * @param beat_boundary  true on the same sample where the caller's beat
 *                        detector (e.g. hr_max30102_result_t.beat_detected)
 *                        reported a new heartbeat - this closes out the
 *                        current AC cycle and (if the quality gate and
 *                        beat-count gate both pass) produces a new SpO2
 *                        estimate
 * @return Result for this call (see spo2_max30102_result_t)
 */
spo2_max30102_result_t spo2_max30102_process(spo2_max30102_t *spo2,
                                              max30102_sample_t sample,
                                              bool beat_boundary);

#endif