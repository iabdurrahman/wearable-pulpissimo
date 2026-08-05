#include "../../oled.h"
#include "../gui_port.h"
#include "test_gui.h"
#include "pulp.h"
#define MAX_OBJECTS 2
static UG_WINDOW window;
static UG_OBJECT objects[MAX_OBJECTS];
static UG_PROGRESS progress;
static UG_TEXTBOX textbox1;

extern volatile uint32_t g_tick_ms;

void Test_GUI_Progress_Run(void)
{
    UG_WindowCreate(&window, objects, MAX_OBJECTS, NULL);
    UG_WindowResize(&window, 0, 0, 127, 63);
    UG_WindowSetStyle(&window, WND_STYLE_2D);
    window.fc = C_WHITE;
    window.bc = C_BLACK;

    UG_ProgressCreate(&window, &progress, PGB_ID_0, 10, 20, 118, 35);
    UG_ProgressSetStyle(&window, PGB_ID_0, PGB_STYLE_2D);
    UG_ProgressSetProgress(&window, PGB_ID_0, 0); 
    
    UG_TextboxCreate(&window, &textbox1, TXB_ID_1, 5, 40, 122, 55);
    UG_TextboxSetFont(&window, TXB_ID_1, &FONT_6X10);
    UG_TextboxSetText(&window, TXB_ID_1, "Processing...");
    UG_TextboxSetAlignment(&window, TXB_ID_1, ALIGN_CENTER);

    UG_WindowShow(&window);
    UG_Update();
    OLED_Update();

    for (int i = 0; i <= 100; i++) {
        UG_ProgressSetProgress(&window, PGB_ID_0, i);
        UG_Update();
        OLED_Update();
        pos_delay_busy_ms(40);
        g_tick_ms += 40;
    }
}
