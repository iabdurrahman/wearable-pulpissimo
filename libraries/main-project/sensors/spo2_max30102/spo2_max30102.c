/**
 * This is an implementation of the SpO2 estimation algorithm for
 * MAX30102 (reflective PPG).
 *
 * This algorithm uses DC tracking with two speeds, AC amplitude
 * per beat cycle (peak-valley), perfusion-index gating, ratio-of-ratios
 * calibration, and median + EMA smoothing.
 *
 * AC amplitude is approximated from the difference between the max and min
 * RAW samples within a beat cycle (irMax - irMin / redMax - redMin).
 */
#include "spo2_max30102.h"
#include <stddef.h>

static inline float absf(float x) { return (x < 0.0f) ? -x : x; }

spo2_max30102_config_t spo2_max30102_get_default_config(void)
{
    spo2_max30102_config_t config;

    config.dc_alpha_fast          = 0.1f;
    config.dc_alpha_slow          = 0.01f;
    config.dc_alpha_fast_samples  = 100u;

    config.min_dc_counts          = 1000.0f;
    config.min_ac_counts          = 10.0f;
    config.min_perfusion_index    = 0.01f;
    config.ratio_min              = 0.4f;
    config.ratio_max              = 2.5f;

    config.cal_a                  = 110.0f;
    config.cal_b                  = 25.0f;
    config.spo2_clamp_min         = 70.0f;
    config.spo2_clamp_max         = 100.0f;

    config.min_beats_before_output = 3u;
    config.ema_alpha               = 0.1f;

    return config;
}

void spo2_max30102_init(spo2_max30102_t *spo2, const spo2_max30102_config_t *config)
{
    if (spo2 == NULL) return;

    spo2->config = (config != NULL) ? *config : spo2_max30102_get_default_config();

    spo2->dcIr = 0.0f;
    spo2->dcRed = 0.0f;
    spo2->dc_initialized = false;
    spo2->sample_count = 0;

    spo2->irMin = 0;
    spo2->irMax = 0;
    spo2->redMin = 0;
    spo2->redMax = 0;
    spo2->cycle_has_samples = false;

    spo2->beats_since_start = 0;

    for (uint32_t i = 0; i < SPO2_MAX30102_MEDIAN_HISTORY; i++) {
        spo2->median_history[i] = 0.0f;
    }
    spo2->median_count = 0;
    spo2->median_index = 0;
    spo2->ema_value = 0.0f;
    spo2->ema_initialized = false;

    spo2->last_spo2_raw = 0.0f;
    spo2->last_spo2 = 0.0f;
    spo2->has_valid_output = false;
}

static float median_of_history(const float *arr, uint32_t count)
{
    if (count == 0) return 0.0f;
    float tmp[SPO2_MAX30102_MEDIAN_HISTORY];
    uint32_t n = (count < SPO2_MAX30102_MEDIAN_HISTORY) ? count : SPO2_MAX30102_MEDIAN_HISTORY;
    for (uint32_t i = 0; i < n; i++) tmp[i] = arr[i];
    for (uint32_t i = 0; i < n - 1; i++)
        for (uint32_t j = i + 1; j < n; j++)
            if (tmp[j] < tmp[i]) { float t = tmp[i]; tmp[i] = tmp[j]; tmp[j] = t; }
    return tmp[n / 2];
}

spo2_max30102_result_t spo2_max30102_process(spo2_max30102_t *spo2,
                                              max30102_sample_t sample,
                                              bool beat_boundary)
{
    spo2_max30102_result_t result;
    result.updated = false;
    result.spo2 = 0.0f;
    result.spo2_raw = 0.0f;
    result.ratio_r = 0.0f;
    result.has_valid_output = false;

    if (spo2 == NULL) return result;

    spo2->sample_count++;

    /* DC tracking */
    float dcAlpha = (spo2->sample_count <= spo2->config.dc_alpha_fast_samples)
        ? spo2->config.dc_alpha_fast
        : spo2->config.dc_alpha_slow;

    if (!spo2->dc_initialized) {
        spo2->dcIr = (float)sample.ir;
        spo2->dcRed = (float)sample.red;
        spo2->dc_initialized = true;
    } else {
        spo2->dcIr  = dcAlpha * (float)sample.ir  + (1.0f - dcAlpha) * spo2->dcIr;
        spo2->dcRed = dcAlpha * (float)sample.red + (1.0f - dcAlpha) * spo2->dcRed;
    }

    /* Peak/valley RAW in cycle */
    if (!spo2->cycle_has_samples) {
        spo2->irMin = sample.ir;  spo2->irMax = sample.ir;
        spo2->redMin = sample.red; spo2->redMax = sample.red;
        spo2->cycle_has_samples = true;
    } else {
        if (sample.ir > spo2->irMax) spo2->irMax = sample.ir;
        if (sample.ir < spo2->irMin) spo2->irMin = sample.ir;
        if (sample.red > spo2->redMax) spo2->redMax = sample.red;
        if (sample.red < spo2->redMin) spo2->redMin = sample.red;
    }

    result.spo2 = spo2->last_spo2;
    result.has_valid_output = spo2->has_valid_output;

    if (!beat_boundary) {
        return result;
    }

    /* calculate SpO2 if beat boundary is detected */
    spo2->beats_since_start++;

    float acIrAmp  = (float)(spo2->irMax  - spo2->irMin);
    float acRedAmp = (float)(spo2->redMax - spo2->redMin);
    float absDcIr  = absf(spo2->dcIr);
    float absDcRed = absf(spo2->dcRed);

    float piIr  = (absDcIr  > 0.0f) ? acIrAmp  / absDcIr  : 0.0f;
    float piRed = (absDcRed > 0.0f) ? acRedAmp / absDcRed : 0.0f;

    bool quality_gate_pass =
        (absDcIr  > spo2->config.min_dc_counts) &&
        (absDcRed > spo2->config.min_dc_counts) &&
        (acIrAmp  > spo2->config.min_ac_counts) &&
        (acRedAmp > spo2->config.min_ac_counts) &&
        (piIr  > spo2->config.min_perfusion_index) &&
        (piRed > spo2->config.min_perfusion_index);

    if (quality_gate_pass) {
        float ratioR = piIr / piRed;
        if (ratioR >= spo2->config.ratio_min && ratioR <= spo2->config.ratio_max) {
            result.ratio_r = ratioR;

            if (spo2->beats_since_start >= spo2->config.min_beats_before_output) {
                float spo2Raw = spo2->config.cal_a - spo2->config.cal_b * ratioR;
                if (spo2Raw < spo2->config.spo2_clamp_min) spo2Raw = spo2->config.spo2_clamp_min;
                if (spo2Raw > spo2->config.spo2_clamp_max) spo2Raw = spo2->config.spo2_clamp_max;

                spo2->median_history[spo2->median_index] = spo2Raw;
                spo2->median_index = (spo2->median_index + 1) % SPO2_MAX30102_MEDIAN_HISTORY;
                if (spo2->median_count < SPO2_MAX30102_MEDIAN_HISTORY) spo2->median_count++;

                float spo2Med = median_of_history(spo2->median_history, spo2->median_count);

                if (!spo2->ema_initialized) {
                    spo2->ema_value = spo2Med;
                    spo2->ema_initialized = true;
                } else {
                    spo2->ema_value = spo2->config.ema_alpha * spo2Med
                                     + (1.0f - spo2->config.ema_alpha) * spo2->ema_value;
                }

                spo2->last_spo2_raw = spo2Raw;
                spo2->last_spo2 = spo2->ema_value;
                spo2->has_valid_output = true;

                result.updated = true;
                result.spo2 = spo2->last_spo2;
                result.spo2_raw = spo2Raw;
                result.has_valid_output = true;
            }
        }
    }

    spo2->cycle_has_samples = false;

    return result;
}