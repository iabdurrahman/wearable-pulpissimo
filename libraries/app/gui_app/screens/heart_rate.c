#include "heart_rate.h"
#include "gui_port.h"

static uint8_t s_bpm = 0;

void heart_rate_init(void) {
    s_bpm = 0;
}

void heart_rate_set_bpm(uint8_t bpm) {
    s_bpm = bpm;
}

static void format_number(char *buf, int val) {
    if (val >= 100) {
        buf[0] = '0' + (val / 100);
        buf[1] = '0' + ((val / 10) % 10);
        buf[2] = '0' + (val % 10);
        buf[3] = '\0';
    } else if (val >= 10) {
        buf[0] = '0' + (val / 10);
        buf[1] = '0' + (val % 10);
        buf[2] = '\0';
    } else {
        buf[0] = '0' + val;
        buf[1] = '\0';
    }
}

void heart_rate_render(void) {
    char bpm_str[4];
    format_number(bpm_str, s_bpm);

    // Title
    UG_FontSelect(FONT_8X8);
    UG_PutString(25, 10, "Heart Rate");

    // Value (BPM)
    UG_FontSelect(FONT_12X16);
    // Center it a bit better. If 2 digits, maybe shift right slightly? 
    // We'll just put it at a fixed X for simplicity.
    UG_PutString(40, 30, bpm_str);

    // Label
    UG_FontSelect(FONT_6X8);
    // If it's a 3-digit number (e.g. 100), the text spans 3 * 12 = 36 pixels.
    // 40 + 36 = 76. So put the label at 80.
    UG_PutString(80, 38, "BPM");
}

void heart_rate_get_bpm_str(char *buf, uint8_t size) {
    if (size >= 4) {
        format_number(buf, s_bpm);
    }
}
