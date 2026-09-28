#include "layer.h"
#include "dedicated_layer.h"
#include "gui_widgets.h"
#include "watchface.h"
#include "notification_state.h"
#include "rtc.h"
#include "i2c_shared.h"
#include <stdio.h>

extern volatile uint32_t g_tick_ms;

typedef enum {
    WF_NORMAL = 0,
    WF_SET_ALARM,
    WF_MSG
} wf_mode_t;

static wf_mode_t s_mode = WF_NORMAL;

static int s_digit[4];         /* [0]=jam puluhan [1]=jam satuan [2]=menit puluhan [3]=menit satuan */
static int s_selected = 0;

static bool s_alarm_enabled = false;
static int  s_alarm_hour = -1;
static int  s_alarm_min  = -1;
static bool s_alarm_fired_this_minute = false;

static char s_msg_line1[20];
static char s_msg_line2[20];
static bool s_msg_has_line2 = false;
static uint32_t s_msg_until_ms = 0;
#define MSG_DURATION_MS   1500UL

static rtc_time_t s_time_cache;
static bool       s_time_cache_valid = false;
static uint32_t   s_last_rtc_read_ms = 0;
#define RTC_READ_PERIOD_MS   1000UL

static uint32_t s_rtc_ok_count = 0;
static uint32_t s_rtc_err_count = 0;
static uint32_t s_rtc_consecutive_err = 0;
static int      s_rtc_last_err_code = 0;
static uint32_t s_last_err_log_ms = 0;
#define RTC_ERR_LOG_PERIOD_MS  10000UL

static bool read_rtc_cached(rtc_time_t *out)
{
    if (!s_time_cache_valid || (g_tick_ms - s_last_rtc_read_ms) >= RTC_READ_PERIOD_MS) {
        i2c_shared_select_rtc();
        i2c_t *rtc_i2c = i2c_shared_rtc_handle();

        if (rtc_i2c != NULL) {
            rtc_time_t t;
            int ret = rtc_get_time(rtc_i2c, &t);

            if (ret == RTC_OK) {
                s_time_cache = t;
                s_time_cache_valid = true;
                s_rtc_ok_count++;
                s_rtc_consecutive_err = 0;
            } else {
                s_rtc_err_count++;
                s_rtc_consecutive_err++;
                s_rtc_last_err_code = ret;

                if (s_rtc_consecutive_err == 1) {
                    printf("[watchface][ERROR] rtc_get_time() gagal, code=%d\n\r", ret);
                }
            }

            /* close i2c for rtc */
            i2c_shared_rtc_delete_handle();
            rtc_i2c = NULL;
        } else {
            s_rtc_err_count++;
            s_rtc_consecutive_err++;
        }

        if (s_rtc_consecutive_err > 0 && (g_tick_ms - s_last_err_log_ms) >= RTC_ERR_LOG_PERIOD_MS) {
            printf("[watchface] rtc: ok=%lu err=%lu (gagal beruntun=%lu, code terakhir=%d)\n\r",
                   (unsigned long)s_rtc_ok_count, (unsigned long)s_rtc_err_count,
                   (unsigned long)s_rtc_consecutive_err, s_rtc_last_err_code);
            s_last_err_log_ms = g_tick_ms;
        }

        s_last_rtc_read_ms = g_tick_ms;
    }

    if (!s_time_cache_valid) return false;
    *out = s_time_cache;
    return true;
}

static void draw_normal(void)
{
    rtc_time_t t;
    bool ok = read_rtc_cached(&t);

    char time_str[12];
    char date_str[16];

    if (ok) {
        snprintf(time_str, sizeof(time_str), "%02u:%02u:%02u", t.hours, t.minutes, t.seconds);
        snprintf(date_str, sizeof(date_str), "%02u/%02u/20%02u", t.date, t.month, t.year);
    } else {
        snprintf(time_str, sizeof(time_str), "--:--:--");
        snprintf(date_str, sizeof(date_str), "--/--/----");
    }

    ug_put_string_centered(20, FONT_8X8, CHAR_W_8X8, time_str);
    ug_put_string_centered(38, FONT_6X8, CHAR_W_6X8, date_str);
}

static void draw_set_alarm(void)
{
    ug_put_string_centered(2, FONT_8X8, CHAR_W_8X8, "SET ALARM");

    bool blink_on = ((g_tick_ms / 400) % 2) == 0;

    char chars[5] = {
        (char)('0' + s_digit[0]),
        (char)('0' + s_digit[1]),
        ':',
        (char)('0' + s_digit[2]),
        (char)('0' + s_digit[3])
    };
    int digit_of_char[5] = { 0, 1, -1, 2, 3 };

    UG_FontSelect(&FONT_12X16);
    int16_t total_w = 5 * CHAR_W_12X16;
    int16_t x0 = (CONTENT_AREA_WIDTH - total_w) / 2;
    int16_t y  = 26;

    for (int i = 0; i < 5; i++) {
        int16_t x = (int16_t)(x0 + i * CHAR_W_12X16);
        if (digit_of_char[i] == s_selected && !blink_on) {
            continue;
        }
        char buf[2] = { chars[i], '\0' };
        UG_PutString(x, y, buf);
    }
}

static void draw_msg(void)
{
    if (s_msg_has_line2) {
        ug_put_string_centered(20, FONT_8X8, CHAR_W_8X8, s_msg_line1);
        ug_put_string_centered(34, FONT_8X8, CHAR_W_8X8, s_msg_line2);
    } else {
        ug_put_string_centered(26, FONT_8X8, CHAR_W_8X8, s_msg_line1);
    }

    if (g_tick_ms >= s_msg_until_ms) {
        s_mode = WF_NORMAL;
    }
}

void layer_watchface_draw(void)
{
    switch (s_mode) {
        case WF_SET_ALARM: draw_set_alarm(); break;
        case WF_MSG:        draw_msg();       break;
        case WF_NORMAL:
        default:            draw_normal();    break;
    }
}

static void enter_set_alarm_mode(void)
{
    int h = 0, m = 0;
    if (s_alarm_enabled) {
        h = s_alarm_hour;
        m = s_alarm_min;
    } else if (s_time_cache_valid) {
        h = s_time_cache.hours;
        m = s_time_cache.minutes;
    }
    s_digit[0] = h / 10;
    s_digit[1] = h % 10;
    s_digit[2] = m / 10;
    s_digit[3] = m % 10;
    s_selected = 0;
    s_mode = WF_SET_ALARM;
}

static void show_message(const char *line1, const char *line2)
{
    int i = 0;
    for (; i < (int)sizeof(s_msg_line1) - 1 && line1[i] != '\0'; i++) s_msg_line1[i] = line1[i];
    s_msg_line1[i] = '\0';

    if (line2 != NULL) {
        int j = 0;
        for (; j < (int)sizeof(s_msg_line2) - 1 && line2[j] != '\0'; j++) s_msg_line2[j] = line2[j];
        s_msg_line2[j] = '\0';
        s_msg_has_line2 = true;
    } else {
        s_msg_line2[0] = '\0';
        s_msg_has_line2 = false;
    }

    s_msg_until_ms = g_tick_ms + MSG_DURATION_MS;
    s_mode = WF_MSG;
}

static void increment_selected_digit(void)
{
    switch (s_selected) {
        case 0: /* jam puluhan: 0,1,2 */
            s_digit[0] = (s_digit[0] + 1) % 3;
            if (s_digit[0] == 2 && s_digit[1] > 3) s_digit[1] = 3;
            break;
        case 1: /* jam satuan: kalau puluhan=2 -> 0-3, selain itu 0-9 */
            if (s_digit[0] == 2) s_digit[1] = (s_digit[1] + 1) % 4;
            else                 s_digit[1] = (s_digit[1] + 1) % 10;
            break;
        case 2: /* menit puluhan: 0-5 */
            s_digit[2] = (s_digit[2] + 1) % 6;
            break;
        case 3: /* menit satuan: 0-9 */
            s_digit[3] = (s_digit[3] + 1) % 10;
            break;
        default:
            break;
    }
}

void layer_watchface_interact(layer_event_t event)
{
    if (s_mode == WF_NORMAL) {
        if (event == LAYER_EVENT_SHORT) {
            enter_set_alarm_mode();
        }
        return;
    }

    if (s_mode == WF_SET_ALARM) {
        if (event == LAYER_EVENT_SHORT) {
            /* pindah digit terpilih, wrap around 4 digit */
            s_selected = (s_selected + 1) % 4;
        } else if (event == LAYER_EVENT_HOLD) {
            /* simpan alarm */
            s_alarm_hour = s_digit[0] * 10 + s_digit[1];
            s_alarm_min  = s_digit[2] * 10 + s_digit[3];
            s_alarm_enabled = true;
            s_alarm_fired_this_minute = false;
            show_message("ALARM SET!", NULL);
        }
        return;
    }
}

bool layer_watchface_handle_left(layer_event_t event)
{
    if (s_mode == WF_SET_ALARM) {
        if (event == LAYER_EVENT_SHORT) {
            increment_selected_digit();
        } else if (event == LAYER_EVENT_HOLD) {
            s_alarm_enabled = false;
            s_alarm_hour = -1;
            s_alarm_min  = -1;
            show_message("ALARM", "CANCELED!");
        }
        return true;
    }

    if (s_mode == WF_MSG) {
        return true;
    }

    return false;
}

void layer_watchface_poll(void)
{
    rtc_time_t t;
    if (!read_rtc_cached(&t)) return;

    if (!s_alarm_enabled) return;

    if ((int)t.hours == s_alarm_hour && (int)t.minutes == s_alarm_min) {
        if (!s_alarm_fired_this_minute) {
            char detail[16];
            snprintf(detail, sizeof(detail), "%02d:%02d", s_alarm_hour, s_alarm_min);
            notification_raise("Alarm", detail);
            s_alarm_fired_this_minute = true;
        }
    } else {
        s_alarm_fired_this_minute = false;
    }
}