#include "oled.h"
#include "GUI/gui_port.h"

// Variables for GUI time
volatile uint32_t g_tick_ms = 0;
void Test_Pixel(void);

// Dummy micros() to fix linker error in pulp-runtime's i2c.c
uint32_t micros(void) {
    return 0;
}

// Dummy pe_start to fix linker error for cluster
void pe_start(void *arg) {}

// Dummy variables/functions to fix linker errors for timer/spi
int pos_freq_domains[3] = {10000000, 10000000, 10000000};
void spim_close(void) {}
void timer_set_clock_source(void) {}
void timer_set_prescaler(void) {}
void timer_set_continuity(void) {}
void timer_enable_irq(void) {}
void timer_disable_irq(void) {}
void adv_timer_get_timer_and_channel_from_io(void) {}
void adv_timer_update(void) {}
void adv_timer_enable_clock(void) {}
void adv_timer_start(void) {}
void adv_timer_disable_clock(void) {}
void adv_timer_stop(void) {}
void adv_timer_stop_and_reset(void) {}
void adv_timer_config_frequency(void) {}

#include "GUI/ui_layer/ui_manager.h"

int main(void)
{
    // 1. Initialize Hardware OLED
    OLED_Init();
    OLED_Clear();

    // 2. Initialize GUI System
    gui_port_init();

    // 3. Initialize the Smartwatch UI Manager
    demo_init();
    while(1)
    {
        /* 
         * TODO: Insert your touch sensor reading function here.
         * Example:
         * 
         * int16_t touch_x, touch_y;
         * bool is_pressed = CST816S_ReadTouch(&touch_x, &touch_y);
         * 
         * // Forward touch data to the GUI
         * port_touch(is_pressed, touch_x, touch_y);
         */

        /*
         * TODO SENSORS: 
         * Read your sensor modules here.
         * Example:
         * uint8_t spo2 = MAX30102_ReadSpO2();
         * UI_Oxygen_SetLevel(spo2);
         */

        // Run the Smartwatch UI logic (switches screens and animations)
        demo_run_loop();

        // Periodically call UG_Update to render the window and objects
        UG_Update();
        
        // Push the drawn frame to the OLED display
        OLED_Update();
        
        g_tick_ms++; // Increment time for gesture engine
    }

    return 0;
}