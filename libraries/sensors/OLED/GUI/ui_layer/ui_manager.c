#include "ui_manager.h"
#include "ui_oxygen.h"
#include "ui_activity.h"
#include "ui_stepcount.h"
#include "ui_alarm.h"
#include "../oled.h"
#include "../GUI/gui_port.h"
#include "pulp.h"

extern volatile uint32_t g_tick_ms;

// Demo variables
static uint8_t demo_spo2 = 98;
static uint32_t demo_steps = 2540;

void ui_manager_init(void)
{
    UI_Oxygen_Init();
    UI_Activity_Init();
    UI_StepCount_Init();
    UI_Alarm_Init();
}

void ui_manager_run_loop(void)
{
    // Screen 1: Oxygen (4 seconds)
    UI_Activity_Hide();
    UI_StepCount_Hide();
    UI_Oxygen_Show();
    demo_spo2 = 96;
    for (int i = 0; i < 4; i++) {
        UI_Oxygen_SetLevel(demo_spo2);
        if (demo_spo2 < 98) demo_spo2++;
        else demo_spo2 = 96;
        UG_Update();
        OLED_Update();
        pos_delay_busy_ms(1000);
        g_tick_ms += 1000;
    }

    // Screen 2: Activity (4 seconds)
    UI_Oxygen_Hide();
    UI_StepCount_Hide();
    UI_Activity_Show();
    UG_Update();
    OLED_Update();
    
    pos_delay_busy_ms(2000);
    g_tick_ms += 2000;
    
    UI_Activity_SetStatus("RUNNING");
    UG_Update();
    OLED_Update();
    pos_delay_busy_ms(1000);
    g_tick_ms += 1000;
    
    UI_Activity_SetStatus("WALKING");
    UG_Update();
    OLED_Update();
    pos_delay_busy_ms(1000);
    g_tick_ms += 1000;

    // Screen 3: Step Count (4 seconds)
    UI_Oxygen_Hide();
    UI_Activity_Hide();
    UI_StepCount_Show();
    for (int i = 0; i < 8; i++) {
        demo_steps += 2;
        UI_StepCount_SetSteps(demo_steps);
        UG_Update();
        OLED_Update();
        pos_delay_busy_ms(500);
        g_tick_ms += 500;
    }

    // Alarm Pop-up (approx 3 seconds)
    UI_Alarm_Show("Heart Rate\nHigh!");
    UG_Update();
    OLED_Update();

    // Auto-dismiss after 3s
    pos_delay_busy_ms(3000);
    g_tick_ms += 3000;
    
    port_touch(true, 62, 49); // press
    UG_Update();
    OLED_Update();
    
    pos_delay_busy_ms(100);
    g_tick_ms += 100;
    
    port_touch(false, 62, 49); // release
    UG_Update();
    OLED_Update();

    // Final short delay so we can see it disappeared before looping
    pos_delay_busy_ms(500);
    g_tick_ms += 500;
}
