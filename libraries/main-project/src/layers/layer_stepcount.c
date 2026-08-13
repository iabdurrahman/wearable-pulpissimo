#include "layer.h"
#include "dedicated_layer.h"
#include "gui_widgets.h"
#include "stepcount.h"
#include "i2c_shared.h"
#include "step_counter.h"
#include "lis3dhtr.h"
#include "rtc.h"
#include <stdio.h>

extern volatile uint32_t g_tick_ms;

#define ACCEL_SAMPLE_PERIOD_MS   100UL   /* ~10Hz */
#define DAY_CHECK_PERIOD_MS      1000UL
#define LOG_SUMMARY_PERIOD_MS    5000UL

static bool           s_initialized = false;
static i2c_t         *s_accel_i2c = NULL;
static step_counter_t s_sc;
static day_tracker_t  s_day_tracker;

static uint32_t s_last_sample_ms = 0;
static uint32_t s_last_day_check_ms = 0;
static uint32_t s_last_log_ms = 0;
static uint32_t s_last_logged_steps = 0;
static uint32_t s_read_ok_count = 0;
static uint32_t s_read_err_count = 0;

static int32_t s_last_accel_x = 0, s_last_accel_y = 0, s_last_accel_z = 0;
static bool    s_have_last_accel = false;

static bool ensure_init(void)
{
    if (s_initialized) return true;

    i2c_shared_select(LIS3DHTR_ADDR >> 1, 400000);

    printf("[stepcount] lis3dhtr_open()...\n\r");
    s_accel_i2c = lis3dhtr_open();
    if (s_accel_i2c == NULL) {
        printf("[stepcount][ERROR] lis3dhtr_open() gagal (return NULL)\n\r");
        return false;
    }

    int init_ret = lis3dhtr_init(s_accel_i2c);
    if (init_ret != LIS3DHTR_OK) {
        printf("[stepcount][ERROR] lis3dhtr_init() gagal, code=%d\n\r", init_ret);
        return false;
    }
    printf("[stepcount][OK] lis3dhtr_init() sukses\n\r");

    step_counter_init(&s_sc);
    day_tracker_init(&s_day_tracker);
    printf("[stepcount][OK] step_counter_init() & day_tracker_init() sukses\n\r");
    printf("[stepcount] Mulai deteksi langkah...\n\r");

    s_initialized = true;
    return true;
}

void layer_stepcount_poll(void)
{
    if (!ensure_init()) return;

    uint32_t now = g_tick_ms;

    /* day rollover check */
    if ((now - s_last_day_check_ms) >= DAY_CHECK_PERIOD_MS) {
        i2c_shared_select_rtc();
        i2c_t *rtc_i2c = i2c_shared_rtc_handle();
        if (rtc_i2c != NULL) {
            int rollover = day_tracker_check_rollover(&s_day_tracker, rtc_i2c);
            if (rollover == 1) {
                step_counter_reset(&s_sc);
                printf("[stepcount][INFO] Pergantian hari terdeteksi, step count direset\n\r");
            } else if (rollover < 0) {
                printf("[stepcount][ERROR] day_tracker_check_rollover() gagal, code=%d\n\r", rollover);
            }
            i2c_shared_select(LIS3DHTR_ADDR >> 1, 400000);
        } else {
            printf("[stepcount][WARN] i2c_shared_rtc_handle() NULL, skip cek rollover hari\n\r");
        }
        s_last_day_check_ms = now;
    }

    /* sample lis3dhtr */
    if ((now - s_last_sample_ms) >= ACCEL_SAMPLE_PERIOD_MS) {
        i2c_shared_select(LIS3DHTR_ADDR >> 1, 400000);

        accel_data_t accel;
        int ret = lis3dhtr_read_accel(s_accel_i2c, &accel);

        if (ret == LIS3DHTR_OK) {
            s_read_ok_count++;
            s_last_accel_x = accel.x;
            s_last_accel_y = accel.y;
            s_last_accel_z = accel.z;
            s_have_last_accel = true;

            int stepped = step_counter_update(&s_sc, accel.x, accel.y, accel.z, (long)now);
            if (stepped) {
                printf("[stepcount][STEP] total=%lu (accel x=%ld y=%ld z=%ld)\n\r",
                       (unsigned long)step_counter_get(&s_sc),
                       (long)accel.x, (long)accel.y, (long)accel.z);
            }
        } else {
            s_read_err_count++;
            printf("[stepcount][ERROR] lis3dhtr_read_accel() gagal, code=%d\n\r", ret);
        }

        i2c_shared_select_oled();
        s_last_sample_ms = now;
    }

    /* 5 sec logging */
    if ((now - s_last_log_ms) >= LOG_SUMMARY_PERIOD_MS) {
        uint32_t current_steps = step_counter_get(&s_sc);
        uint32_t steps_in_window = current_steps - s_last_logged_steps;

        if (s_have_last_accel) {
            printf("[stepcount] --- 5s summary: total=%lu steps_in_window=%lu read_ok=%lu read_err=%lu | last_accel x=%ld y=%ld z=%ld ---\n\r",
                   (unsigned long)current_steps, (unsigned long)steps_in_window,
                   (unsigned long)s_read_ok_count, (unsigned long)s_read_err_count,
                   (long)s_last_accel_x, (long)s_last_accel_y, (long)s_last_accel_z);
        } else {
            printf("[stepcount] --- 5s summary: total=%lu steps_in_window=%lu read_ok=%lu read_err=%lu | belum pernah baca accel sukses ---\n\r",
                   (unsigned long)current_steps, (unsigned long)steps_in_window,
                   (unsigned long)s_read_ok_count, (unsigned long)s_read_err_count);
        }

        s_last_logged_steps = current_steps;
        s_last_log_ms = now;
    }
}

static uint32_t get_steps(void)
{
    return s_initialized ? step_counter_get(&s_sc) : 0;
}

void layer_stepcount_draw(void)
{
    ug_put_string_centered(2, FONT_8X8, CHAR_W_8X8, "Step Count");

    int16_t foot_cx  = 26;
    int16_t foot_top = 18;
    draw_foot_icon(foot_cx, foot_top, 18, 28);

    char buf[12];
    snprintf(buf, sizeof(buf), "%lu", (unsigned long)get_steps());

    UG_FontSelect(&FONT_12X16);
    UG_PutString(58, 30, buf);

    UG_FontSelect(&FONT_6X8);
    UG_PutString(58, 48, "steps");
}

void layer_stepcount_interact(layer_event_t event)
{
    /* Nothing */
    (void)event;
}
