#ifndef HEART_RATE_H
#define HEART_RATE_H

#include <stdint.h>

/**
 * @brief Initialize the heart rate screen state.
 */
void heart_rate_init(void);

/**
 * @brief Set the current heart rate (BPM) to display.
 *
 * @param bpm Beats per minute.
 */
void heart_rate_set_bpm(uint8_t bpm);

/**
 * @brief Render the heart rate to the uGUI framebuffer.
 *
 * Draws the BPM value using FONT_12X16 and a label/icon using a smaller font.
 * Does NOT call OLED_Update(); the caller is responsible for flushing.
 */
void heart_rate_render(void);

/**
 * @brief Get the formatted string for unit testing.
 *
 * @param buf   Output buffer (must be at least 4 bytes)
 * @param size  Size of buffer
 */
void heart_rate_get_bpm_str(char *buf, uint8_t size);

#endif /* HEART_RATE_H */
