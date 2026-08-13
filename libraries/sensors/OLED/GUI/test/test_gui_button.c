#include "../../oled.h"
#include "../gui_port.h"
#include "test_gui.h"
#include "pulp.h"
#define MAX_OBJECTS 2
static UG_WINDOW window;
static UG_OBJECT objects[MAX_OBJECTS];
static UG_BUTTON button;
static UG_TEXTBOX status;

extern volatile uint32_t g_tick_ms;

static void window_callback(UG_MESSAGE* msg)
{
    if (msg->type == MSG_TYPE_OBJECT && msg->id == OBJ_TYPE_BUTTON && msg->sub_id == BTN_ID_0)
    {
        if (msg->event == OBJ_EVENT_PRESSED) {
            UG_TextboxSetText(&window, TXB_ID_0, "Button Pressed!");
        } else if (msg->event == OBJ_EVENT_RELEASED) {
            UG_TextboxSetText(&window, TXB_ID_0, "Button Released");
        }
    }
}

void Test_GUI_Button_Run(void)
{
    UG_WindowCreate(&window, objects, MAX_OBJECTS, window_callback);
    UG_WindowResize(&window, 0, 0, 127, 63);
    UG_WindowSetStyle(&window, WND_STYLE_2D);
    window.fc = C_WHITE;
    window.bc = C_BLACK;

    UG_ButtonCreate(&window, &button, BTN_ID_0, 20, 10, 108, 35);
    UG_ButtonSetFont(&window, BTN_ID_0, &FONT_6X10);
    UG_ButtonSetText(&window, BTN_ID_0, "Click Me!");
    UG_ButtonSetStyle(&window, BTN_ID_0, BTN_STYLE_2D);

    UG_TextboxCreate(&window, &status, TXB_ID_0, 0, 45, 127, 60);
    UG_TextboxSetFont(&window, TXB_ID_0, &FONT_6X10);
    UG_TextboxSetText(&window, TXB_ID_0, "Waiting...");
    UG_TextboxSetAlignment(&window, TXB_ID_0, ALIGN_CENTER);

    UG_WindowShow(&window);
    UG_Update();
    OLED_Update();

    pos_delay_busy_ms(1500);
    g_tick_ms += 1500;

    port_touch(true, 64, 22);
    UG_Update();
    OLED_Update();

    pos_delay_busy_ms(1000);
    g_tick_ms += 1000;

    port_touch(false, 64, 22);
    UG_Update();
    OLED_Update();

    pos_delay_busy_ms(1500);
    g_tick_ms += 1500;
}
