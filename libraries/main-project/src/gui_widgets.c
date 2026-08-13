#include "gui_widgets.h"
#include "dedicated_layer.h"

static int local_strlen(const char *s)
{
    int n = 0;
    while (s[n]) n++;
    return n;
}

void draw_bell_icon(int16_t cx, int16_t top_y, int16_t width, bool crossed)
{
    int16_t hanger_w = width / 4;
    if (hanger_w < 2) hanger_w = 2;
    int16_t hanger_h = width / 6;
    if (hanger_h < 1) hanger_h = 1;

    int16_t body_w = width;
    int16_t body_h = (width * 8) / 10;

    int16_t clapper_r = width / 8;
    if (clapper_r < 1) clapper_r = 1;

    int16_t hanger_x0 = cx - hanger_w / 2;
    int16_t hanger_y0 = top_y;
    UG_FillFrame(hanger_x0, hanger_y0, hanger_x0 + hanger_w - 1, hanger_y0 + hanger_h - 1, C_WHITE);

    int16_t body_top_y = hanger_y0 + hanger_h;
    int16_t tip_x = cx, tip_y = body_top_y;
    int16_t left_x = cx - body_w / 2, base_y = body_top_y + body_h;
    int16_t right_x = cx + body_w / 2;
    UG_FillTriangle(tip_x, tip_y, left_x, base_y, right_x, base_y, C_WHITE);

    int16_t clapper_cy = base_y + clapper_r;
    UG_FillCircle(cx, clapper_cy, clapper_r, C_WHITE);

    if (crossed) {
        int16_t pad = width / 6;
        UG_DrawLine(cx - body_w / 2 - pad, hanger_y0 - pad,
                    cx + body_w / 2 + pad, clapper_cy + clapper_r + pad, C_WHITE);
        UG_DrawLine(cx - body_w / 2 - pad + 1, hanger_y0 - pad,
                    cx + body_w / 2 + pad + 1, clapper_cy + clapper_r + pad, C_WHITE);
    }
}

void draw_warning_icon(int16_t cx, int16_t top_y, int16_t width)
{
    int16_t half = width / 2;
    int16_t height = (width * 9) / 10;
    int16_t tip_x = cx, tip_y = top_y;
    int16_t left_x = cx - half, base_y = top_y + height;
    int16_t right_x = cx + half;
    UG_FillTriangle(tip_x, tip_y, left_x, base_y, right_x, base_y, C_WHITE);

    int16_t bar_w = width / 10;
    if (bar_w < 2) bar_w = 2;
    int16_t bar_h = (height * 45) / 100;
    int16_t bar_x0 = cx - bar_w / 2;
    int16_t bar_y0 = top_y + height / 4;
    UG_FillFrame(bar_x0, bar_y0, bar_x0 + bar_w - 1, bar_y0 + bar_h - 1, C_BLACK);

    int16_t dot_r = bar_w / 2 + 1;
    int16_t dot_cy = bar_y0 + bar_h + dot_r + 2;
    UG_FillCircle(cx, dot_cy, dot_r, C_BLACK);
}

void draw_foot_icon(int16_t cx, int16_t top_y, int16_t width, int16_t body_height)
{
    int16_t toe_r = width / 7;
    if (toe_r < 2) toe_r = 2;
    int16_t toe_spacing = width / 3;
    int16_t toe_y = top_y + toe_r;

    UG_FillCircle((int16_t)(cx - toe_spacing), toe_y, toe_r, C_WHITE);
    UG_FillCircle(cx,                          toe_y, toe_r, C_WHITE);
    UG_FillCircle((int16_t)(cx + toe_spacing), toe_y, toe_r, C_WHITE);

    int16_t half_w = width / 2;
    int16_t body_x1 = (int16_t)(cx - half_w);
    int16_t body_x2 = (int16_t)(cx + half_w);
    int16_t body_y1 = (int16_t)(toe_y + toe_r + 2);
    int16_t body_y2 = (int16_t)(body_y1 + body_height);

    UG_FillRoundFrame(body_x1, body_y1, body_x2, body_y2, half_w, C_WHITE);
}

/* Stickman poses */
#define STICK_SCALE(v, scale) (int16_t)(((int32_t)(v) * (scale)) / 32)

static void thick_line(int16_t x1, int16_t y1, int16_t x2, int16_t y2)
{
    UG_DrawLine(x1, y1, x2, y2, C_WHITE);
    UG_DrawLine((int16_t)(x1 + 1), y1, (int16_t)(x2 + 1), y2, C_WHITE);
}

void draw_stickman(int16_t cx, int16_t top_y, int16_t scale, stickman_pose_t pose)
{
    int16_t head_r = STICK_SCALE(4, scale);
    if (head_r < 2) head_r = 2;

    if (pose == STICKMAN_LIE || pose == STICKMAN_FALL) {
        int16_t cy = (int16_t)(top_y + STICK_SCALE(16, scale));
        int16_t head_cx = (int16_t)(cx + head_r);
        int16_t neck_x  = (int16_t)(head_cx + head_r + 1);
        int16_t hip_x   = (int16_t)(neck_x + STICK_SCALE(14, scale));
        int16_t foot_x  = (int16_t)(hip_x + STICK_SCALE(12, scale));

        int16_t tilt = (pose == STICKMAN_FALL) ? STICK_SCALE(13, scale) : 0;

        UG_FillCircle(head_cx, cy, head_r, C_WHITE);
        thick_line(neck_x, cy, hip_x, (int16_t)(cy + tilt));
        UG_DrawLine(hip_x, (int16_t)(cy + tilt), foot_x, (int16_t)(cy + tilt - STICK_SCALE(3, scale)), C_WHITE); /* kaki depan */
        UG_DrawLine(hip_x, (int16_t)(cy + tilt), foot_x, (int16_t)(cy + tilt + STICK_SCALE(5, scale)), C_WHITE); /* kaki belakang, agak nekuk */
        UG_DrawLine(neck_x, cy, (int16_t)(neck_x + STICK_SCALE(6, scale)), (int16_t)(cy - STICK_SCALE(4, scale)), C_WHITE); /* tangan */
        UG_DrawLine(neck_x, cy, (int16_t)(neck_x + STICK_SCALE(5, scale)), (int16_t)(cy + STICK_SCALE(3, scale)), C_WHITE); /* tangan */

        if (pose == STICKMAN_FALL) {
            int16_t floor_y = (int16_t)(cy + tilt + STICK_SCALE(8, scale));
            UG_DrawLine((int16_t)(hip_x - 2), floor_y, (int16_t)(foot_x + 3), floor_y, C_WHITE);
        }
        return;
    }

    int16_t head_cy = (int16_t)(top_y + head_r);
    int16_t neck_y  = (int16_t)(head_cy + head_r + 1);
    int16_t hip_y   = (int16_t)(neck_y + STICK_SCALE(14, scale));

    UG_FillCircle(cx, head_cy, head_r, C_WHITE);
    thick_line(cx, neck_y, cx, hip_y);

    switch (pose) {
        case STICKMAN_SIT: {
            int16_t knee_x = (int16_t)(cx + STICK_SCALE(9, scale));
            int16_t foot_y = (int16_t)(hip_y + STICK_SCALE(10, scale));
            UG_DrawLine(cx, hip_y, knee_x, hip_y, C_WHITE);
            UG_DrawLine(knee_x, hip_y, knee_x, foot_y, C_WHITE);
            UG_DrawLine(cx, neck_y, (int16_t)(cx - STICK_SCALE(6, scale)), hip_y, C_WHITE);
            UG_DrawLine(cx, neck_y, (int16_t)(cx + STICK_SCALE(3, scale)), (int16_t)(neck_y + STICK_SCALE(8, scale)), C_WHITE);
            break;
        }
        case STICKMAN_WALK: {
            int16_t foot_y = (int16_t)(hip_y + STICK_SCALE(12, scale));
            UG_DrawLine(cx, hip_y, (int16_t)(cx + STICK_SCALE(8, scale)), foot_y, C_WHITE);
            UG_DrawLine(cx, hip_y, (int16_t)(cx - STICK_SCALE(8, scale)), foot_y, C_WHITE);
            UG_DrawLine(cx, neck_y, (int16_t)(cx - STICK_SCALE(8, scale)), (int16_t)(neck_y + STICK_SCALE(9, scale)), C_WHITE);
            UG_DrawLine(cx, neck_y, (int16_t)(cx + STICK_SCALE(8, scale)), (int16_t)(neck_y + STICK_SCALE(9, scale)), C_WHITE);
            break;
        }
        case STICKMAN_STAND:
        default: {
            int16_t foot_y = (int16_t)(hip_y + STICK_SCALE(12, scale));
            UG_DrawLine(cx, hip_y, (int16_t)(cx + STICK_SCALE(5, scale)), foot_y, C_WHITE);
            UG_DrawLine(cx, hip_y, (int16_t)(cx - STICK_SCALE(5, scale)), foot_y, C_WHITE);
            UG_DrawLine(cx, neck_y, (int16_t)(cx - STICK_SCALE(7, scale)), (int16_t)(neck_y + STICK_SCALE(10, scale)), C_WHITE);
            UG_DrawLine(cx, neck_y, (int16_t)(cx + STICK_SCALE(7, scale)), (int16_t)(neck_y + STICK_SCALE(10, scale)), C_WHITE);
            break;
        }
    }
}

void draw_loading_spinner(int16_t cx, int16_t cy, int16_t radius, int16_t phase)
{
    /* 8 posisi melingkar, 3 titik berturut-turut (mulai dari `phase`)
     * digambar terang/penuh membentuk "ekor" yang berputar; sisanya
     * digambar sebagai lingkaran kecil redup. */
    static const int8_t offs_x[8] = { 0,  7, 10,  7,  0, -7, -10, -7 };
    static const int8_t offs_y[8] = { -10, -7, 0, 7, 10, 7, 0, -7 };

    int16_t big_r = (int16_t)((radius * 3) / 10);
    if (big_r < 2) big_r = 2;
    int16_t small_r = (int16_t)((radius * 3) / 20);
    if (small_r < 1) small_r = 1;

    int p = ((int)phase % 8 + 8) % 8;

    for (int i = 0; i < 8; i++) {
        int16_t dx = (int16_t)((offs_x[i] * radius) / 10);
        int16_t dy = (int16_t)((offs_y[i] * radius) / 10);
        int16_t px = (int16_t)(cx + dx);
        int16_t py = (int16_t)(cy + dy);

        int back = (i - p + 8) % 8;

        if (back <= 2) {
            UG_FillCircle(px, py, big_r, C_WHITE);
        } else {
            UG_DrawCircle(px, py, small_r, C_WHITE);
        }
    }
}

void draw_heart_icon(int16_t cx, int16_t cy, int16_t size)
{
    int16_t r = (int16_t)((size * 3) / 10);
    if (r < 2) r = 2;
    int16_t lobe_dx = (int16_t)(r - r / 4);
    int16_t lobe_cy = (int16_t)(cy - r / 3);

    UG_FillCircle((int16_t)(cx - lobe_dx), lobe_cy, r, C_WHITE);
    UG_FillCircle((int16_t)(cx + lobe_dx), lobe_cy, r, C_WHITE);

    int16_t top_y   = (int16_t)(lobe_cy - r / 2);
    int16_t left_x  = (int16_t)(cx - lobe_dx - r);
    int16_t right_x = (int16_t)(cx + lobe_dx + r);
    int16_t tip_y   = (int16_t)(cy + size / 2);

    UG_FillTriangle(left_x, top_y, right_x, top_y, cx, tip_y, C_WHITE);
}

void draw_droplet_icon(int16_t cx, int16_t cy, int16_t size)
{
    int16_t half_w = (int16_t)((size * 3) / 10);
    if (half_w < 2) half_w = 2;

    int16_t tip_y = (int16_t)(cy - size / 2);
    int16_t tri_h = (int16_t)((size * 65) / 100);
    int16_t base_y = (int16_t)(tip_y + tri_h);

    UG_FillTriangle(cx, tip_y, (int16_t)(cx - half_w), base_y, (int16_t)(cx + half_w), base_y, C_WHITE);

    int16_t r = (int16_t)((size * 4) / 10);
    if (r < 2) r = 2;
    int16_t circle_cy = (int16_t)(base_y + r / 6);
    UG_FillCircle(cx, circle_cy, r, C_WHITE);
}

void draw_ecg_waveform(int16_t cx, int16_t top_y, int16_t width, int16_t height, int16_t reveal_x)
{
    static const int16_t px[9] = {   0,  20,  32,  42,  50,  62,  74,  84, 100 };
    static const int16_t py[9] = {  50,  50,   5,  85,  50,  50,   5,  85,  50 };

    int16_t left = (int16_t)(cx - width / 2);

    if (reveal_x > width) reveal_x = width;
    if (reveal_x < 0) reveal_x = 0;
    int16_t reveal_abs_x = (int16_t)(left + reveal_x);

    for (int i = 0; i < 8; i++) {
        int16_t x1 = (int16_t)(left + (px[i]     * width)  / 100);
        int16_t y1 = (int16_t)(top_y + (py[i]     * height) / 100);
        int16_t x2 = (int16_t)(left + (px[i + 1] * width)  / 100);
        int16_t y2 = (int16_t)(top_y + (py[i + 1] * height) / 100);

        if (x2 <= reveal_abs_x) {
            UG_DrawLine(x1, y1, x2, y2, C_WHITE);
        } else if (x1 < reveal_abs_x) {
            int32_t dx = x2 - x1;
            int32_t t_num = reveal_abs_x - x1;
            int16_t yi = (dx != 0) ? (int16_t)(y1 + (t_num * (y2 - y1)) / dx) : y1;
            UG_DrawLine(x1, y1, reveal_abs_x, yi, C_WHITE);
            break;
        } else {
            break;
        }
    }

    if (reveal_x > 0 && reveal_x < width) {
        UG_FillCircle(reveal_abs_x, (int16_t)(top_y + height / 2), 2, C_WHITE);
    }
}

void ug_put_string_centered(int16_t y, const UG_FONT *font, uint8_t char_w, const char *str)
{
    int16_t text_w = (int16_t)(local_strlen(str) * char_w);
    int16_t x = (CONTENT_AREA_WIDTH - text_w) / 2;
    if (x < 0) x = 0;
    UG_FontSelect((UG_FONT *)font);
    UG_PutString(x, y, (char *)str);
}