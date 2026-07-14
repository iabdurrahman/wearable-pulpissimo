/**
 * @file font5x7.h
 * @brief 5x7 bitmap font definitions
 *
 * Supported characters:
 *  - Space
 *  - Digits (0-9)
 *  - Lowercase letters (a-z)
 *  - '.', ',', ':', '-', '_', '(', ')', '/'
 */

#ifndef FONT5X7_H
#define FONT5X7_H

#include <stdint.h>

#define FONT_WIDTH     5
#define FONT_HEIGHT    7

/* Character table */
extern const char Font5x7_CharMap[];

/* Bitmap table */
extern const uint8_t Font5x7[][5];

/* Number of supported characters */
extern const uint8_t Font5x7_Count;

#endif