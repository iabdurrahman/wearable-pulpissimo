#include "layer.h"
#include "dedicated_layer.h"
#include "gui_widgets.h"
#include "heartrate.h"
#include "i2c_shared.h"
#include "hr_max30102.h"
#include "ppg_afe4400.h"
#include <stdio.h>

extern volatile uint32_t g_tick_ms;

#define I2C_PORT_HR          0
#define SPI_PORT_HR          0
#define HR_SAMPLE_PERIOD_MS  20UL    /* ~50Hz */
#define HR_LOG_PERIOD_MS     2000UL

#define MAX30102_ADDR 0x57
#define HR_TIMEOUT_MS        20000UL

typedef enum {
    PPG_SOURCE_REFLECTIVE = 0,
    PPG_SOURCE_PASSTHROUGH
} ppg_source_t;

static ppg_source_t s_source = PPG_SOURCE_REFLECTIVE;
static bool         s_have_result = false;
static bool         s_timed_out = false;
static int32_t      s_bpm = 0;

static hr_max30102_t s_reflective_ctx;
static ppg_afe4400_t s_passthrough_ctx;

static uint32_t s_last_sample_ms = 0;
static uint32_t s_last_log_ms = 0;
static uint32_t s_wait_start_ms = 0;
static uint32_t s_read_ok_count = 0;
static uint32_t s_read_err_count = 0;

/* Latest sensor readings */
static uint32_t s_last_raw_red = 0, s_last_raw_ir = 0;
static bool     s_last_finger_detected = false;
static bool     s_last_beat_detected = false;

static const char *source_label(void)
{
    return (s_source == PPG_SOURCE_REFLECTIVE) ? "reflective" : "passthrough";
}

static void start_reflective(void)
{
    i2c_shared_select(MAX30102_ADDR, 400000);
    printf("[heartrate] hr_max30102_init(port=%d)...\n\r", I2C_PORT_HR);
    hr_max30102_init(&s_reflective_ctx, I2C_PORT_HR);
    printf("[heartrate][OK] hr_max30102_init() selesai\n\r");
}

static void start_passthrough(void)
{
    printf("[heartrate] ppg_afe4400_init(port=%d)...\n\r", SPI_PORT_HR);
    ppg_afe4400_init(&s_passthrough_ctx, SPI_PORT_HR);
    /* This is dummy */
}

void layer_heartrate_reset(void)
{
    s_have_result = false;
    s_timed_out = false;
    s_bpm = 0;
    s_wait_start_ms = g_tick_ms;
    s_read_ok_count = 0;
    s_read_err_count = 0;

    printf("[heartrate] layer_heartrate_reset(), sumber=%s\n\r", source_label());

    if (s_source == PPG_SOURCE_REFLECTIVE) {
        start_reflective();
    } else {
        start_passthrough();
    }
}

void layer_heartrate_poll(void)
{
    uint32_t now = g_tick_ms;

    /* Throttle for bug fixing */
    if ((now - s_last_sample_ms) < HR_SAMPLE_PERIOD_MS) return;
    s_last_sample_ms = now;

    if (!s_have_result && !s_timed_out && (now - s_wait_start_ms) >= HR_TIMEOUT_MS) {
        printf("[heartrate][TIMEOUT] %lums tanpa data valid (sumber=%s) - tampilkan 0 bpm\n\r",
               (unsigned long)HR_TIMEOUT_MS, source_label());
        s_bpm = 0;
        s_have_result = true;
        s_timed_out = true;
    }

    if (s_source == PPG_SOURCE_REFLECTIVE) {
        i2c_shared_select(MAX30102_ADDR, 400000);
        hr_max30102_result_t r = hr_max30102_process(&s_reflective_ctx, now);

        if (r.sample_available) {
            s_read_ok_count++;
            s_last_raw_red = r.raw.red;
            s_last_raw_ir  = r.raw.ir;
            s_last_finger_detected = r.finger_detected;
            s_last_beat_detected   = r.beat_detected;

            if (r.avg_bpm > 0) {
                if (!s_have_result) {
                    printf("[heartrate][RESULT] bpm valid pertama = %ld (reflective)\n\r", (long)r.avg_bpm);
                }
                s_bpm = r.avg_bpm;
                s_have_result = true;
                s_timed_out = false;
            }
        } else {
            s_read_err_count++;
        }
    } else {
        ppg_afe4400_result_t r = ppg_afe4400_process(&s_passthrough_ctx, now);

        if (r.sample_available) {
            s_read_ok_count++;
            s_last_raw_red = r.raw.red;
            s_last_raw_ir  = r.raw.ir;
            s_last_finger_detected = r.finger_detected;
            s_last_beat_detected   = r.beat_detected;

            if (r.avg_bpm > 0) {
                if (!s_have_result) {
                    printf("[heartrate][RESULT] bpm valid pertama = %ld (passthrough, dummy)\n\r", (long)r.avg_bpm);
                }
                s_bpm = r.avg_bpm;
                s_have_result = true;
                s_timed_out = false;
            }
        } else {
            s_read_err_count++;
        }
    }

    if ((now - s_last_log_ms) >= HR_LOG_PERIOD_MS) {
        printf("[heartrate] sumber=%s sample_ok=%lu sample_no_data=%lu have_result=%d bpm=%ld | raw red=%lu ir=%lu finger=%d beat=%d\n\r",
               source_label(), (unsigned long)s_read_ok_count, (unsigned long)s_read_err_count,
               (int)s_have_result, (long)s_bpm,
               (unsigned long)s_last_raw_red, (unsigned long)s_last_raw_ir,
               (int)s_last_finger_detected, (int)s_last_beat_detected);
        s_last_log_ms = now;
    }
}

void layer_heartrate_draw(void)
{
    int16_t cx = CONTENT_AREA_WIDTH / 2;

    if (!s_have_result) {
        int16_t period_ms = 1400;
        int16_t reveal_x = (int16_t)((g_tick_ms % (uint32_t)period_ms) * 70 / period_ms);
        draw_ecg_waveform(cx, 8, 70, 24, reveal_x);
        ug_put_string_centered(42, FONT_6X8, CHAR_W_6X8, "Measuring");
        ug_put_string_centered(52, FONT_6X8, CHAR_W_6X8, "Heart Rate...");
        return;
    }

    ug_put_string_centered(2, FONT_8X8, CHAR_W_8X8, "Heart Rate");

    draw_heart_icon(cx, 22, 22);

    char buf[16];
    snprintf(buf, sizeof(buf), "%ld bpm", (long)s_bpm);
    ug_put_string_centered(38, FONT_12X16, CHAR_W_12X16, buf);

    const char *src_label = (s_source == PPG_SOURCE_REFLECTIVE) ? "PPG: reflect" : "PPG: passthru";
    ug_put_string_centered(56, FONT_6X8, CHAR_W_6X8, src_label);
}

void layer_heartrate_interact(layer_event_t event)
{
    if (event != LAYER_EVENT_SHORT) return;

    s_source = (s_source == PPG_SOURCE_REFLECTIVE) ? PPG_SOURCE_PASSTHROUGH : PPG_SOURCE_REFLECTIVE;
    layer_heartrate_reset();
}