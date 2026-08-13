#ifndef UI_ACTIVITY_H
#define UI_ACTIVITY_H

/**
 * @brief Initialize the Activity Layer
 */
void UI_Activity_Init(void);

/**
 * @brief Show the Activity Layer
 */
void UI_Activity_Show(void);

/**
 * @brief Hide the Activity Layer
 */
void UI_Activity_Hide(void);

/**
 * @brief Update the displayed Activity status
 * @param status String representing activity (e.g., "WALKING", "RUNNING")
 */
void UI_Activity_SetStatus(const char* status);

#endif // UI_ACTIVITY_H
