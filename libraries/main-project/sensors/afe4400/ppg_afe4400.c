/* This is a simple dummy implementation of the PPG Module
 * AFE4400 for pulp-runtime usage */

#include "ppg_afe4400.h"
#include <stddef.h>

/* Simulation variables */
#define SIM_BEAT_INTERVAL_MS  830u
#define SIM_BPM_VALUE         72
#define SIM_SPO2_VALUE        98.0f

afe4400_status_t ppg_afe4400_init(ppg_afe4400_t *ctx, int spi_port)
{
    if (ctx == NULL) return AFE4400_ERROR_INVALID_PARAM;

    afe4400_status_t st = afe4400_init(&ctx->sensor, spi_port);
    if (st != AFE4400_OK) return st;

    afe4400_config_t cfg = afe4400_get_default_config();
    afe4400_configure(&ctx->sensor, &cfg);

    ctx->sample_count = 0;
    ctx->beat_count   = 0;
    ctx->last_beat_ms = 0;
    return AFE4400_OK;
}

void ppg_afe4400_reset(ppg_afe4400_t *ctx)
{
    if (ctx == NULL) return;
    ctx->sample_count = 0;
    ctx->beat_count   = 0;
    ctx->last_beat_ms = 0;
}

ppg_afe4400_result_t ppg_afe4400_process(ppg_afe4400_t *ctx, uint32_t now_ms)
{
    ppg_afe4400_result_t result;
    result.sample_available = false;
    result.finger_detected  = false;
    result.beat_detected    = false;
    result.bpm              = 0;
    result.avg_bpm          = 0;
    result.spo2_valid       = false;
    result.spo2             = 0.0f;
    result.raw.red          = 0;
    result.raw.ir            = 0;

    if (ctx == NULL) return result;

    afe4400_sample_t sample;
    if (afe4400_read_sample(&ctx->sensor, &sample) != AFE4400_OK) {
        return result;
    }

    result.sample_available = true;
    result.raw = sample;
    result.finger_detected = true; /* this is dummy */

    if (ctx->sample_count < PPG_AFE4400_WARMUP_SAMPLES) {
        ctx->sample_count++;
        return result; /* this is dummy */
    }

    /* Periodic beat simulation */
    if (ctx->last_beat_ms == 0 || (now_ms - ctx->last_beat_ms) >= SIM_BEAT_INTERVAL_MS) {
        result.beat_detected = true;
        ctx->last_beat_ms = now_ms;
        if (ctx->beat_count < 0xFFFFFFFFu) ctx->beat_count++;
    }

    result.bpm = SIM_BPM_VALUE;
    result.avg_bpm = SIM_BPM_VALUE;

    if (ctx->beat_count >= PPG_AFE4400_MIN_BEATS_SPO2) {
        result.spo2_valid = true;
        result.spo2 = SIM_SPO2_VALUE;
    }

    return result;
}