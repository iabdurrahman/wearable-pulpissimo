#ifndef UI_STEPCOUNT_H
#define UI_STEPCOUNT_H

#include <stdint.h>

/**
 * @brief Initialize the Step Count Layer
 */
void UI_StepCount_Init(void);

/**
 * @brief Show the Step Count Layer
 */
void UI_StepCount_Show(void);

/**
 * @brief Hide the Step Count Layer
 */
void UI_StepCount_Hide(void);

/**
 * @brief Update the displayed Step Count
 * @param steps Number of steps
 */
void UI_StepCount_SetSteps(uint32_t steps);

#endif // UI_STEPCOUNT_H
