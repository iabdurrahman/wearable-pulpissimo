#include "demo.h"
#include "oled.h"
#include "GUI/gui_port.h"
#include "GUI/test/test_gui.h"
#include "pulp.h"

extern volatile uint32_t g_tick_ms;

void demo_init(void)
{
    // No longer needed for sequential style
}

void demo_run_loop(void)
{
    // Scene 1: Textbox Demo (4 seconds)
    Test_GUI_Textbox_Run();

    // Scene 2: Button Demo with Auto-Click (4 seconds)
    Test_GUI_Button_Run();

    // Scene 3: Checkbox Demo with Auto-Toggle (4 seconds)
    Test_GUI_Checkbox_Run();

    // Scene 4: Progress Bar Demo (4 seconds)
    Test_GUI_Progress_Run();
}
