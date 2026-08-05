#ifndef UI_OXYGEN_H
#define UI_OXYGEN_H

#include <stdint.h>

/**
 * @brief Initialize the Oxygen Display Layer (SpO2)
 */
void UI_Oxygen_Init(void);

/**
 * @brief Show the Oxygen Layer
 */
void UI_Oxygen_Show(void);

/**
 * @brief Hide the Oxygen Layer
 */
void UI_Oxygen_Hide(void);

/**
 * @brief Update the displayed SpO2 level
 * @param spo2_value The oxygen percentage (e.g., 98) from the PPG sensor
 */
void UI_Oxygen_SetLevel(uint8_t spo2_value);

#endif // UI_OXYGEN_H
