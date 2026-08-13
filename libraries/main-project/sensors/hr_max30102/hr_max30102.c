#include "hr_max30102.h"

// Crude finger-presence gate
#define HR_MAX30102_FINGER_IR_FLOOR 50000u

static max30102_config_t hr_max30102_default_config(void) {
    // build_sparkfun_example_config()
    max30102_config_t config = max30102_get_default_config();
    config.sample_rate     = MAX30102_SR_400;
    config.adc_range       = MAX30102_ADC_RANGE_4096;
    config.pulse_width     = MAX30102_PULSE_WIDTH_18BIT; // ~411 us
    config.fifo_avg        = MAX30102_FIFO_AVG_4;
    config.ir_led_current  = 0x1F; // untouched driver default - what checkForBeat() sees
    config.red_led_current = 0x0A; // indicator only, unused by the beat detector
    return config;
}

max30102_status_t hr_max30102_init(hr_max30102_t *hr, int i2c_port) {
    if (!hr) return MAX30102_ERROR_INVALID_PARAM;
    max30102_config_t config = hr_max30102_default_config();
    return hr_max30102_init_with_config(hr, i2c_port, &config);
}

max30102_status_t hr_max30102_init_with_config(hr_max30102_t *hr, int i2c_port,
                                                const max30102_config_t *config) {
    if (!hr || !config) return MAX30102_ERROR_INVALID_PARAM;

    ppg_hr_init(&hr->hr);

    max30102_status_t status = max30102_init(&hr->sensor, i2c_port);
    if (status != MAX30102_OK) return status;

    status = max30102_configure(&hr->sensor, config);
    if (status != MAX30102_OK) return status;

    hr->config = *config;
    return MAX30102_OK;
}

hr_max30102_result_t hr_max30102_process(hr_max30102_t *hr, uint32_t now_ms) {
    hr_max30102_result_t result;
    result.sample_available = false;
    result.finger_detected  = false;
    result.beat_detected    = false;
    result.bpm              = 0;
    result.avg_bpm          = 0;
    result.raw.red          = 0;
    result.raw.ir            = 0;

    if (!hr) return result;

    max30102_sample_t sample;
    max30102_status_t status = max30102_read_sample(&hr->sensor, &sample);

    // MAX30102_ERROR_NO_DATA is the expected/common case between FIFO
    // fills, just report "nothing new yet" and leave detector state alone.
    if (status != MAX30102_OK) {
        return result;
    }

    result.sample_available = true;
    result.raw = sample;
    result.finger_detected = sample.ir >= HR_MAX30102_FINGER_IR_FLOOR;

    ppg_hr_result_t hr_result = ppg_hr_process(&hr->hr, (int32_t)sample.ir, now_ms);
    result.beat_detected = hr_result.beat_detected;
    result.bpm = hr_result.bpm;
    result.avg_bpm = hr_result.avg_bpm;

    return result;
}

void hr_max30102_reset(hr_max30102_t *hr) {
    if (!hr) return;
    ppg_hr_init(&hr->hr);
}