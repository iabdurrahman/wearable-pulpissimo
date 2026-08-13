#ifndef UI_MANAGER_H
#define UI_MANAGER_H

/**
 * @brief Initialize all UI components and start the UI Manager
 */
void ui_manager_init(void);

/**
 * @brief Run the UI Manager loop (handles screen switching and animations)
 * Call this inside your main while(1) loop.
 */
void ui_manager_run_loop(void);

#endif // UI_MANAGER_H
