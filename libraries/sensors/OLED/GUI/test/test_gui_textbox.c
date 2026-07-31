#include "../../oled.h"
#include "../gui_port.h"
#include "test_gui.h"
#include "pulp.h"

extern volatile uint32_t g_tick_ms;
#define MAX_OBJECTS 2
static UG_WINDOW window;
static UG_OBJECT objects[MAX_OBJECTS];
static UG_TEXTBOX textbox1;
static UG_TEXTBOX textbox2;

void Test_GUI_Textbox_Run(void)
{
    UG_WindowCreate(&window, objects, MAX_OBJECTS, NULL);
    UG_WindowResize(&window, 0, 0, 127, 63);
    UG_WindowSetStyle(&window, WND_STYLE_2D);
    window.fc = C_WHITE;
    window.bc = C_BLACK;

    UG_TextboxCreate(&window, &textbox1, TXB_ID_0, 5, 5, 122, 25);
    UG_TextboxSetFont(&window, TXB_ID_0, &FONT_6X10);
    UG_TextboxSetText(&window, TXB_ID_0, "GUI Textbox");
    UG_TextboxSetAlignment(&window, TXB_ID_0, ALIGN_CENTER);

    UG_TextboxCreate(&window, &textbox2, TXB_ID_1, 5, 30, 122, 55);
    UG_TextboxSetFont(&window, TXB_ID_1, &FONT_6X10);
    UG_TextboxSetText(&window, TXB_ID_1, "Halo ini video\nfitur demo GUI uGUI");
    UG_TextboxSetAlignment(&window, TXB_ID_1, ALIGN_CENTER);

    UG_WindowShow(&window);
    UG_Update();
    OLED_Update();
    
    pos_delay_busy_ms(4000);
    g_tick_ms += 4000;
}
