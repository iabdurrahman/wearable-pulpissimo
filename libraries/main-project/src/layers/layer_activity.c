#include "layer.h"
#include "dedicated_layer.h"
#include "gui_widgets.h"
#include "activity.h"
#include "i2c_shared.h"
#include "classifier.h"
#include "mpu6050.h"
#include "notification_state.h"
#include <stdio.h>

extern volatile uint32_t g_tick_ms;

#define SAMPLE_PERIOD_MS   20UL   /* ~50Hz */
#define LOG_PERIOD_MS       2000UL

#define MPU_RECONNECT_ERR_THRESHOLD  10U


static bool       s_initialized = false;
static i2c_t     *s_mpu_i2c = NULL;
static bool       s_have_result = false;
static activity_t s_current = ACTIVITY_UNKNOWN;
static activity_t s_fall_edge_prev = ACTIVITY_UNKNOWN;
static uint32_t   s_last_sample_ms = 0;
static uint32_t   s_last_log_ms = 0;
static uint32_t   s_read_ok_count = 0;
static uint32_t   s_read_err_count = 0;      /* total error sepanjang umur program (statistik) */
static uint32_t   s_consec_err_count = 0;    /* error berturut" sejak koneksi terakhir sehat */
static uint32_t   s_reconnect_count = 0;     /* berapa kali sudah reconnect (statistik) */

/* latest sample reading */
static int32_t s_last_ax = 0, s_last_ay = 0, s_last_az = 0;
static int32_t s_last_gx = 0, s_last_gy = 0, s_last_gz = 0;

static const char *activity_label(activity_t a);

static bool ensure_init(void)
{
    if (s_initialized) return true;

    i2c_shared_select(MPU6050_ADDR, 400000);

    printf("[activity] mpu6050_open()...\n\r");
    s_mpu_i2c = mpu6050_open();
    if (s_mpu_i2c == NULL) {
        printf("[activity][ERROR] mpu6050_open() gagal (return NULL)\n\r");
        return false;
    }

    int init_ret = mpu6050_init(s_mpu_i2c);
    if (init_ret != MPU6050_OK) {
        printf("[activity][ERROR] mpu6050_init() gagal, code=%d\n\r", init_ret);
        return false;
    }
    printf("[activity][OK] mpu6050_init() sukses\n\r");

    classifier_init();
    printf("[activity][OK] classifier_init() sukses, mulai deteksi aktivitas...\n\r");
    s_initialized = true;
    s_consec_err_count = 0;
    return true;
}

void layer_activity_reset(void)
{
    s_have_result = false;
    s_current = ACTIVITY_UNKNOWN;
    s_fall_edge_prev = ACTIVITY_UNKNOWN;
    if (s_initialized) {
        classifier_init();
    }
    printf("[activity] layer_activity_reset() - window classifier dikosongkan\n\r");
}

static void mg_dps_to_imu_sample(const accel_data_t *accel, const gyro_data_t *gyro, imu_sample_t *out)
{
    out->ax = (int16_t)(accel->x * ACCEL_SENS_2G / 1000);
    out->ay = (int16_t)(accel->y * ACCEL_SENS_2G / 1000);
    out->az = (int16_t)(accel->z * ACCEL_SENS_2G / 1000);

    out->gx = (int16_t)(gyro->x * GYRO_SENS_250DPS / 1000);
    out->gy = (int16_t)(gyro->y * GYRO_SENS_250DPS / 1000);
    out->gz = (int16_t)(gyro->z * GYRO_SENS_250DPS / 1000);
}

void layer_activity_poll(void)
{
    if (!ensure_init()) return;

    uint32_t now = g_tick_ms;
    if ((now - s_last_sample_ms) < SAMPLE_PERIOD_MS) return;
    s_last_sample_ms = now;

    i2c_shared_select(MPU6050_ADDR, 400000);

    accel_data_t accel;
    gyro_data_t  gyro;
    int ret = mpu6050_read_all(s_mpu_i2c, &accel, &gyro);
    if (ret != MPU6050_OK) {
        s_read_err_count++;
        s_consec_err_count++;
        printf("[activity][ERROR] mpu6050_read_all() gagal, code=%d (beruntun=%lu/%u)\n\r",
               ret, (unsigned long)s_consec_err_count, MPU_RECONNECT_ERR_THRESHOLD);

        if (s_consec_err_count >= MPU_RECONNECT_ERR_THRESHOLD) {
            s_reconnect_count++;
            printf("[activity][WARN] %u error beruntun, reconnect MPU6050 (percobaan ke-%lu)...\n\r",
                   MPU_RECONNECT_ERR_THRESHOLD, (unsigned long)s_reconnect_count);

            s_initialized = false;
            s_consec_err_count = 0;

            s_have_result = false;
            s_current = ACTIVITY_UNKNOWN;
            s_fall_edge_prev = ACTIVITY_UNKNOWN;
        }
        return;
    }
    s_read_ok_count++;
    s_consec_err_count = 0;
    s_last_ax = accel.x; s_last_ay = accel.y; s_last_az = accel.z;
    s_last_gx = gyro.x;  s_last_gy = gyro.y;  s_last_gz = gyro.z;

    imu_sample_t sample;
    mg_dps_to_imu_sample(&accel, &gyro, &sample);

    activity_t result = classifier_update(&sample);

    if ((now - s_last_log_ms) >= LOG_PERIOD_MS) {
        printf("[activity] ok=%lu err=%lu reconnect=%lu | accel x=%ld y=%ld z=%ld | gyro x=%ld y=%ld z=%ld | classifier=%s\n\r",
               (unsigned long)s_read_ok_count, (unsigned long)s_read_err_count,
               (unsigned long)s_reconnect_count,
               (long)s_last_ax, (long)s_last_ay, (long)s_last_az,
               (long)s_last_gx, (long)s_last_gy, (long)s_last_gz,
               activity_label(result));
        s_last_log_ms = now;
    }

    if (result == ACTIVITY_UNKNOWN) return;

    if (result != s_current || !s_have_result) {
        printf("[activity][RESULT] aktivitas berubah -> %s\n\r", activity_label(result));
    }

    s_current = result;
    s_have_result = true;

    if (result == ACTIVITY_FALL && s_fall_edge_prev != ACTIVITY_FALL) {
        printf("[activity][ALERT] FALL terdeteksi! notification_raise(\"Fall Detected!\", ...)\n\r");
        notification_raise("Fall Detected!", "Check on user");
    }
    s_fall_edge_prev = result;
}

static const char *activity_label(activity_t a)
{
    switch (a) {
        case ACTIVITY_SIT:   return "SITTING";
        case ACTIVITY_STAND: return "STANDING";
        case ACTIVITY_LIE:   return "LYING DOWN";
        case ACTIVITY_WALK:  return "WALKING";
        case ACTIVITY_FALL:  return "FALL DETECTED!";
        default:              return "...";
    }
}

static stickman_pose_t pose_for_activity(activity_t a)
{
    switch (a) {
        case ACTIVITY_SIT:   return STICKMAN_SIT;
        case ACTIVITY_LIE:   return STICKMAN_LIE;
        case ACTIVITY_WALK:  return STICKMAN_WALK;
        case ACTIVITY_FALL:  return STICKMAN_FALL;
        case ACTIVITY_STAND:
        default:              return STICKMAN_STAND;
    }
}

void layer_activity_draw(void)
{
    int16_t cx = CONTENT_AREA_WIDTH / 2;

    if (!s_have_result) {
        int16_t phase = (int16_t)((g_tick_ms / 150) % 8);
        draw_loading_spinner(cx, 18, 15, phase);
        ug_put_string_centered(42, FONT_6X8, CHAR_W_6X8, "Detecting");
        ug_put_string_centered(52, FONT_6X8, CHAR_W_6X8, "Activity...");
        return;
    }

    stickman_pose_t pose = pose_for_activity(s_current);

    if (pose == STICKMAN_LIE || pose == STICKMAN_FALL) {
        draw_stickman((int16_t)(cx - 24), 6, 30, pose);
    } else {
        draw_stickman(cx, 4, 30, pose);
    }

    ug_put_string_centered(52, FONT_8X8, CHAR_W_8X8, activity_label(s_current));
}

void layer_activity_interact(layer_event_t event)
{
    /* Nothing */
    (void)event;
}