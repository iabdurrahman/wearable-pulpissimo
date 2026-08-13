#ifndef GUI_WIDGETS_H
#define GUI_WIDGETS_H
#include <stdint.h>
#include <stdbool.h>
#include "oled.h"
#include "gui_port.h"

#define CHAR_W_6X8    6
#define CHAR_W_8X8    8
#define CHAR_W_12X16  12

void draw_bell_icon(int16_t cx, int16_t top_y, int16_t width, bool crossed);
void draw_warning_icon(int16_t cx, int16_t top_y, int16_t width);
void draw_foot_icon(int16_t cx, int16_t top_y, int16_t width, int16_t body_height);

typedef enum {
    STICKMAN_STAND = 0,
    STICKMAN_SIT,
    STICKMAN_LIE,
    STICKMAN_WALK,
    STICKMAN_FALL
} stickman_pose_t;

void draw_stickman(int16_t cx, int16_t top_y, int16_t scale, stickman_pose_t pose);
void draw_loading_spinner(int16_t cx, int16_t cy, int16_t radius, int16_t phase);
void draw_heart_icon(int16_t cx, int16_t cy, int16_t size);
void draw_droplet_icon(int16_t cx, int16_t cy, int16_t size);
void draw_ecg_waveform(int16_t cx, int16_t top_y, int16_t width, int16_t height, int16_t reveal_x);
void ug_put_string_centered(int16_t y, const UG_FONT *font, uint8_t char_w, const char *str);

#endif
