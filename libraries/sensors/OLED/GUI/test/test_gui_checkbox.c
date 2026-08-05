#include "../../oled.h"
#include "../gui_port.h"
#include "test_gui.h"
#include "pulp.h"
#define MAX_OBJECTS 1
static UG_WINDOW window;
static UG_OBJECT objects[MAX_OBJECTS];
static UG_CHECKBOX checkbox;

extern volatile uint32_t g_tick_ms;

void Test_GUI_Checkbox_Run(void)
{
    UG_WindowCreate(&window, objects, MAX_OBJECTS, NULL);
    UG_WindowResize(&window, 0, 0, 127, 63);
    UG_WindowSetStyle(&window, WND_STYLE_2D);
    window.fc = C_WHITE;
    window.bc = C_BLACK;

    UG_CheckboxCreate(&window, &checkbox, CHB_ID_0, 20, 25, 100, 40);
    UG_CheckboxSetFont(&window, CHB_ID_0, &FONT_6X10);
    UG_CheckboxSetText(&window, CHB_ID_0, "Check Me");
    UG_CheckboxSetStyle(&window, CHB_ID_0, CHB_STYLE_2D);

    UG_WindowShow(&window);
    UG_Update();
    OLED_Update();

    pos_delay_busy_ms(1500);
    g_tick_ms += 1500;

    port_touch(true, 25, 32); // press
    UG_Update();
    OLED_Update();

    pos_delay_busy_ms(100);
    g_tick_ms += 100;

    port_touch(false, 25, 32); // release
    UG_Update();
    OLED_Update();

    pos_delay_busy_ms(2400);
    g_tick_ms += 2400;
}
