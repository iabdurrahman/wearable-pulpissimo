#include "layer.h"
#include "dedicated_layer.h"
#include "gui_widgets.h"
#include "spo2.h"
#include "i2c_shared.h"
#include "hr_max30102.h"
#include "spo2_max30102.h"
#include "ppg_afe4400.h"
#include <stdio.h>

extern volatile uint32_t g_tick_ms;

static void print_f2(float v)
{
    int neg = (v < 0.0f);
    float av = neg ? -v : v;
    int ip = (int)av;
    int fp = (int)((av - (float)ip) * 100.0f + 0.5f);
    if (fp >= 100) { fp -= 100; ip += 1; }
    printf("%s%d.%02d", neg ? "-" : "", ip, fp);
}

#define I2C_PORT_SPO2          0
#define SPI_PORT_SPO2          0
#define SPO2_SAMPLE_PERIOD_MS  20UL
#define SPO2_LOG_PERIOD_MS     2000UL
#define SPO2_TIMEOUT_MS        20000UL

#define MAX30102_ADDR 0x57

typedef enum {
    PPG_SOURCE_REFLECTIVE = 0,
    PPG_SOURCE_PASSTHROUGH
} ppg_source_t;

static ppg_source_t s_source = PPG_SOURCE_REFLECTIVE;
static bool         s_have_result = false;
static bool         s_timed_out = false;
static float        s_spo2 = 0.0f;

static hr_max30102_t   s_hr_ctx;
static spo2_max30102_t s_spo2_ctx;

static ppg_afe4400_t s_passthrough_ctx;

static uint32_t s_last_sample_ms = 0;
static uint32_t s_last_log_ms = 0;
static uint32_t s_wait_start_ms = 0;
static uint32_t s_read_ok_count = 0;
static uint32_t s_read_err_count = 0;

static uint32_t s_last_raw_red = 0, s_last_raw_ir = 0;
static bool     s_last_beat_detected = false;
static float    s_last_ratio_r = 0.0f;
static float    s_last_spo2_raw = 0.0f;
static uint32_t s_beat_detected_count = 0;

static const char *source_label(void)
{
    return (s_source == PPG_SOURCE_REFLECTIVE) ? "reflective" : "passthrough";
}

static void log_spo2_gate_debug(const spo2_max30102_t *spo2, const spo2_max30102_result_t *r)
{
    float dcIr = spo2->dcIr, dcRed = spo2->dcRed;
    float absDcIr  = (dcIr  < 0.0f) ? -dcIr  : dcIr;
    float absDcRed = (dcRed < 0.0f) ? -dcRed : dcRed;
    float acIrAmp  = (float)(spo2->irMax  - spo2->irMin);
    float acRedAmp = (float)(spo2->redMax - spo2->redMin);
    float piIr  = (absDcIr  > 0.0f) ? acIrAmp  / absDcIr  : 0.0f;
    float piRed = (absDcRed > 0.0f) ? acRedAmp / absDcRed : 0.0f;

    bool g_dc_ir  = absDcIr  > spo2->config.min_dc_counts;
    bool g_dc_red = absDcRed > spo2->config.min_dc_counts;
    bool g_ac_ir  = acIrAmp  > spo2->config.min_ac_counts;
    bool g_ac_red = acRedAmp > spo2->config.min_ac_counts;
    bool g_pi_ir  = piIr  > spo2->config.min_perfusion_index;
    bool g_pi_red = piRed > spo2->config.min_perfusion_index;
    bool gate_pass = g_dc_ir && g_dc_red && g_ac_ir && g_ac_red && g_pi_ir && g_pi_red;

    printf("[spo2][GATE] dcIr="); print_f2(dcIr);
    printf(" dcRed="); print_f2(dcRed);
    printf(" acIrAmp="); print_f2(acIrAmp);
    printf(" acRedAmp="); print_f2(acRedAmp);
    printf(" piIr="); print_f2(piIr);
    printf(" piRed="); print_f2(piRed);
    printf("\n\r");

    printf("[spo2][GATE] dc_ir=%d(need>%d) dc_red=%d(need>%d) ac_ir=%d(need>%d) ac_red=%d(need>%d) pi_ir=%d(need>0.01ish) pi_red=%d(need>0.01ish) -> gate_pass=%d\n\r",
           (int)g_dc_ir,  (int)spo2->config.min_dc_counts,
           (int)g_dc_red, (int)spo2->config.min_dc_counts,
           (int)g_ac_ir,  (int)spo2->config.min_ac_counts,
           (int)g_ac_red, (int)spo2->config.min_ac_counts,
           (int)g_pi_ir, (int)g_pi_red,
           (int)gate_pass);

    printf("[spo2][GATE] beats_since_start=%lu (butuh >= %lu) ratio_r=",
           (unsigned long)spo2->beats_since_start, (unsigned long)spo2->config.min_beats_before_output);
    print_f2(r->ratio_r);
    printf(" updated=%d has_valid_output=%d\n\r", (int)r->updated, (int)r->has_valid_output);
}

static void start_reflective(void)
{
    i2c_shared_select(MAX30102_ADDR, 400000);
    printf("[spo2] hr_max30102_init(port=%d)...\n\r", I2C_PORT_SPO2);
    hr_max30102_init(&s_hr_ctx, I2C_PORT_SPO2);
    spo2_max30102_init(&s_spo2_ctx, NULL);
    printf("[spo2][OK] hr_max30102_init() & spo2_max30102_init() selesai\n\r");
}

static void start_passthrough(void)
{
    printf("[spo2] ppg_afe4400_init(port=%d)...\n\r", SPI_PORT_SPO2);
    ppg_afe4400_init(&s_passthrough_ctx, SPI_PORT_SPO2);
}

void layer_spo2_reset(void)
{
    s_have_result = false;
    s_timed_out = false;
    s_spo2 = 0.0f;
    s_wait_start_ms = g_tick_ms;
    s_read_ok_count = 0;
    s_read_err_count = 0;
    s_beat_detected_count = 0;

    printf("[spo2] layer_spo2_reset(), sumber=%s\n\r", source_label());

    if (s_source == PPG_SOURCE_REFLECTIVE) {
        start_reflective();
    } else {
        start_passthrough();
    }
}

void layer_spo2_poll(void)
{
    uint32_t now = g_tick_ms;

    /* Throttle */
    if ((now - s_last_sample_ms) < SPO2_SAMPLE_PERIOD_MS) return;
    s_last_sample_ms = now;

    if (!s_have_result && !s_timed_out && (now - s_wait_start_ms) >= SPO2_TIMEOUT_MS) {
        printf("[spo2][TIMEOUT] %lums tanpa data valid (sumber=%s) - tampilkan 0%%\n\r",
               (unsigned long)SPO2_TIMEOUT_MS, source_label());
        s_spo2 = 0.0f;
        s_have_result = true;
        s_timed_out = true;
    }

    if (s_source == PPG_SOURCE_REFLECTIVE) {
        i2c_shared_select(MAX30102_ADDR, 400000);
        hr_max30102_result_t hr_r = hr_max30102_process(&s_hr_ctx, now);

        if (hr_r.sample_available) {
            s_read_ok_count++;
            s_last_raw_red = hr_r.raw.red;
            s_last_raw_ir  = hr_r.raw.ir;
            s_last_beat_detected = hr_r.beat_detected;
            if (hr_r.beat_detected) s_beat_detected_count++;

            spo2_max30102_result_t spo2_r = spo2_max30102_process(&s_spo2_ctx, hr_r.raw, hr_r.beat_detected);
            s_last_ratio_r  = spo2_r.ratio_r;
            s_last_spo2_raw = spo2_r.spo2_raw;

            if (hr_r.beat_detected) {
                log_spo2_gate_debug(&s_spo2_ctx, &spo2_r);
            }

            if (spo2_r.has_valid_output) {
                if (!s_have_result) {
                    printf("[spo2][RESULT] SpO2 valid pertama = %d%% (reflective)\n\r", (int)(spo2_r.spo2 + 0.5f));
                }
                s_spo2 = spo2_r.spo2;
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
            s_last_beat_detected = r.beat_detected;

            if (r.spo2_valid) {
                if (!s_have_result) {
                    printf("[spo2][RESULT] SpO2 valid pertama = %d%% (passthrough, dummy)\n\r", (int)(r.spo2 + 0.5f));
                }
                s_spo2 = r.spo2;
                s_have_result = true;
                s_timed_out = false;
            }
        } else {
            s_read_err_count++;
        }
    }

    if ((now - s_last_log_ms) >= SPO2_LOG_PERIOD_MS) {
        printf("[spo2] sumber=%s sample_ok=%lu sample_no_data=%lu have_result=%d spo2=%d%% | raw red=%lu ir=%lu beat=%d beat_count=%lu ratio_r=%d.%02d spo2_raw=%d.%02d\n\r",
               source_label(), (unsigned long)s_read_ok_count, (unsigned long)s_read_err_count,
               (int)s_have_result, (int)(s_spo2 + 0.5f),
               (unsigned long)s_last_raw_red, (unsigned long)s_last_raw_ir, (int)s_last_beat_detected,
               (unsigned long)s_beat_detected_count,
               (int)s_last_ratio_r, (int)((s_last_ratio_r - (int)s_last_ratio_r) * 100),
               (int)s_last_spo2_raw, (int)((s_last_spo2_raw - (int)s_last_spo2_raw) * 100));

        if (s_source == PPG_SOURCE_REFLECTIVE && s_beat_detected_count == 0) {
            printf("[spo2][WARN] belum ada beat_detected sama sekali dari hr_max30102 - kalau ini terus 0,\n\r");
            printf("[spo2][WARN] masalahnya di DETEKSI DETAK (sama seperti Heart Rate), BUKAN di quality gate\n\r");
            printf("[spo2][WARN] spo2_max30102 (gate breakdown [spo2][GATE] cuma muncul saat ada beat boundary)\n\r");
        }

        s_last_log_ms = now;
    }
}

void layer_spo2_draw(void)
{
    int16_t cx = CONTENT_AREA_WIDTH / 2;

    if (!s_have_result) {
        int16_t period_ms = 1400;
        int16_t reveal_x = (int16_t)((g_tick_ms % (uint32_t)period_ms) * 70 / period_ms);
        draw_ecg_waveform(cx, 8, 70, 24, reveal_x);
        ug_put_string_centered(42, FONT_6X8, CHAR_W_6X8, "Measuring");
        ug_put_string_centered(52, FONT_6X8, CHAR_W_6X8, "SpO2...");
        return;
    }

    ug_put_string_centered(2, FONT_8X8, CHAR_W_8X8, "SpO2");

    draw_droplet_icon(cx, 22, 22);

    char buf[16];
    snprintf(buf, sizeof(buf), "%d%%", (int)(s_spo2 + 0.5f));
    ug_put_string_centered(38, FONT_12X16, CHAR_W_12X16, buf);

    const char *src_label = (s_source == PPG_SOURCE_REFLECTIVE) ? "PPG: reflect" : "PPG: passthru";
    ug_put_string_centered(56, FONT_6X8, CHAR_W_6X8, src_label);
}

void layer_spo2_interact(layer_event_t event)
{
    if (event != LAYER_EVENT_SHORT) return;

    s_source = (s_source == PPG_SOURCE_REFLECTIVE) ? PPG_SOURCE_PASSTHROUGH : PPG_SOURCE_REFLECTIVE;
    layer_spo2_reset();
}