/**
* @file oled.h
* @brief Simple API Driver for OLED Display (SSD1306 / SH1106).
*
*/

#ifndef OLED_H
#define OLED_H

#include <stdint.h>
#include <stdbool.h>

// OLED Configuration
#define OLED_WIDTH      128
#define OLED_HEIGHT      64

// Initialization 
/**
 * @brief Initialize OLED display.
 */
void OLED_Init(void);

/**
 * @brief Clear framebuffer.
 */
void OLED_Clear(void);

/**
 * @brief Send framebuffer to OLED.
 */
void OLED_Update(void);

//Display Control
/**
 * @brief Turn display ON.
 */
void OLED_DisplayOn(void);

/**
 * @brief Turn display OFF.
 */
void OLED_DisplayOff(void);

//Specific Graphics

/**
 * @brief Draw one pixel.
 *
 * @param x Horizontal position
 * @param y Vertical position
 * @param color true = ON, false = OFF
 */
void OLED_DrawPixel(uint8_t x,
                    uint8_t y,
                    bool color);

/**
 * @brief Draw a line.
 */
void OLED_DrawLine(uint8_t x0,
                   uint8_t y0,
                   uint8_t x1,
                   uint8_t y1,
                   bool color);

/**
 * @brief Draw rectangle outline.
 */
void OLED_DrawRect(uint8_t x,
                   uint8_t y,
                   uint8_t width,
                   uint8_t height,
                   bool color);

/**
 * @brief Draw filled rectangle.
 */
void OLED_FillRect(uint8_t x,
                   uint8_t y,
                   uint8_t width,
                   uint8_t height,
                   bool color);

/**
 * @brief Draw a circle.
 */
void OLED_DrawCircle(uint8_t x0,
                     uint8_t y0,
                     uint8_t r,
                     bool color);

//Font Rendering

/**
 * @brief Draw a single character using 5x7 bitmap font.
 *
 * @param x Starting X coordinate.
 * @param y Starting Y coordinate.
 * @param c Character to draw.
 * @param color true = pixel ON, false = pixel OFF.
 */
void OLED_DrawChar(uint8_t x,
                   uint8_t y,
                   char c,
                   bool color);

/**
 * @brief Draw a string using 5x7 bitmap font.
 *
 * @param x Starting X coordinate.
 * @param y Starting Y coordinate.
 * @param str Pointer to null-terminated string.
 * @param color true = pixel ON, false = pixel OFF.
 */
void OLED_DrawString(uint8_t x,
                     uint8_t y,
                     const char *str,
                     bool color);

#endif /* OLED_H */