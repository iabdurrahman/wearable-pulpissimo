#include "dedicated_layer.h"
#include "gui_widgets.h"
#include "battery.h"
#include <stdio.h>

extern volatile uint32_t g_tick_ms;

#define BATT_W        14   /* lebar badan baterai */
#define BATT_H        24   /* tinggi badan baterai */
#define BATT_CAP_W    6    /* lebar "kepala" positif di atas */
#define BATT_CAP_H    3
#define BATT_TOP_Y    3    /* jarak dari atas layar */

/* Teks persentase, di bawah ikon baterai */
#define PERCENT_Y     (BATT_TOP_Y + BATT_CAP_H + BATT_H + 4)

/* Lonceng notifikasi, di bawah teks persentase (hanya jika pending) */
#define BELL_WIDTH    14
#define BELL_TOP_Y    (PERCENT_Y + 12)

static int strlen_local(const char *s)
{
    int n = 0;
    while (s[n]) n++;
    return n;
}

static int16_t strip_center_x(void)
{
    return DEDICATED_LAYER_X_START + DEDICATED_LAYER_WIDTH / 2;
}

static void draw_battery(uint8_t percent)
{
    int16_t cx = strip_center_x();
    int16_t body_x0 = cx - BATT_W / 2;
    int16_t body_y0 = BATT_TOP_Y + BATT_CAP_H;

    int16_t cap_x0 = cx - BATT_CAP_W / 2;
    UG_FillFrame(cap_x0, BATT_TOP_Y, cap_x0 + BATT_CAP_W - 1, BATT_TOP_Y + BATT_CAP_H - 1, C_WHITE);

    UG_DrawFrame(body_x0, body_y0, body_x0 + BATT_W - 1, body_y0 + BATT_H - 1, C_WHITE);

    int16_t inner_x0 = body_x0 + 2;
    int16_t inner_x1 = body_x0 + BATT_W - 3;
    int16_t inner_h  = BATT_H - 4;
    int16_t gap      = 2;
    int16_t seg_h    = (inner_h - 2 * gap) / 3;

    int segments_on = 3;
    if (percent < 66) segments_on = 2;
    if (percent < 33) segments_on = 1;
    if (percent == 0) segments_on = 0;

    int16_t inner_y0 = body_y0 + 2;
    for (int i = 0; i < 3; i++) {
        int16_t seg_y1 = (int16_t)((inner_y0 + inner_h - 1) - i * (seg_h + gap));
        int16_t seg_y0 = (int16_t)(seg_y1 - seg_h + 1);

        if (i < segments_on) {
            UG_FillFrame(inner_x0, seg_y0, inner_x1, seg_y1, C_WHITE);
        }
        if (i < 2) {
            int16_t gap_y = (int16_t)(seg_y0 - 1);
            UG_DrawLine(inner_x0, gap_y, inner_x1, gap_y, C_BLACK);
        }
    }

    char buf[6];
    snprintf(buf, sizeof(buf), "%u%%", (unsigned)percent);
    UG_FontSelect(&FONT_6X8);
    int16_t text_w = (int16_t)(strlen_local(buf) * CHAR_W_6X8);
    int16_t text_x = (int16_t)(cx - text_w / 2);
    UG_PutString(text_x, PERCENT_Y, buf);
}

void dedicated_layer_init(void)
{
    g_notification_pending = false;
}

void dedicated_layer_draw(void)
{
    uint8_t percent = battery_get_percent();
    draw_battery(percent);

    /* Notification bell */
    if (g_notification_pending) {
        bool blink_on = ((g_tick_ms / 500) % 2) == 0;
        if (blink_on) {
            draw_bell_icon(strip_center_x(), BELL_TOP_Y, BELL_WIDTH, false);
        }
    }

    UG_DrawLine(DEDICATED_LAYER_X_START - 1, 0, DEDICATED_LAYER_X_START - 1, OLED_HEIGHT - 1, C_WHITE);
}