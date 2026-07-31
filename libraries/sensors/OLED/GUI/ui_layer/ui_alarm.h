#ifndef UI_ALARM_H
#define UI_ALARM_H

#include <stdbool.h>

/**
 * @brief Initialize the Alarm Popup Layer
 */
void UI_Alarm_Init(void);

/**
 * @brief Show an alarm with a specific message
 * @param msg The warning message
 */
void UI_Alarm_Show(const char* msg);

/**
 * @brief Hide the alarm popup
 */
void UI_Alarm_Hide(void);

/**
 * @brief Check if the alarm is currently active/visible
 * @return true if visible
 */
bool UI_Alarm_IsActive(void);

/**
 * @brief Must be called in the loop to handle the dismiss button
 */
void UI_Alarm_Loop(void);

#endif // UI_ALARM_H
