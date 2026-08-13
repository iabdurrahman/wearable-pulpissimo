#ifndef PPG_AFE4400_H
#define PPG_AFE4400_H

#include <stdint.h>
#include <stdbool.h>
#include "afe4400.h"

typedef struct {
    afe4400_t sensor;
    uint32_t  sample_count;
    uint32_t  beat_count;
    uint32_t  last_beat_ms;
} ppg_afe4400_t;

typedef struct {
    bool     sample_available;
    bool     finger_detected;
    bool     beat_detected;
    int32_t  bpm;
    int32_t  avg_bpm;
    bool     spo2_valid;
    float    spo2;
    afe4400_sample_t raw;
} ppg_afe4400_result_t;

#define PPG_AFE4400_WARMUP_SAMPLES   40u
#define PPG_AFE4400_MIN_BEATS_SPO2   3u

afe4400_status_t ppg_afe4400_init(ppg_afe4400_t *ctx, int spi_port);
ppg_afe4400_result_t ppg_afe4400_process(ppg_afe4400_t *ctx, uint32_t now_ms);
void ppg_afe4400_reset(ppg_afe4400_t *ctx);

#endif
