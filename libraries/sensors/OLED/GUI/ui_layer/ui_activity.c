#include "ui_activity.h"
#include "../oled.h"
#include "../GUI/gui_port.h"

#define MAX_OBJECTS 4
static UG_WINDOW window;
static UG_OBJECT objects[MAX_OBJECTS];
static UG_TEXTBOX title;
static UG_TEXTBOX status_text;
static UG_TEXTBOX arrow_left;
static UG_TEXTBOX arrow_right;

void UI_Activity_Init(void)
{
    UG_WindowCreate(&window, objects, MAX_OBJECTS, NULL);
    UG_WindowResize(&window, 0, 0, 127, 63);
    UG_WindowSetStyle(&window, WND_STYLE_2D);
    window.fc = C_WHITE;
    window.bc = C_BLACK;

    // Title
    UG_TextboxCreate(&window, &title, TXB_ID_0, 0, 5, 127, 20);
    UG_TextboxSetFont(&window, TXB_ID_0, &FONT_6X10);
    UG_TextboxSetText(&window, TXB_ID_0, "Activity Status");
    UG_TextboxSetAlignment(&window, TXB_ID_0, ALIGN_CENTER);

    // Status Text
    UG_TextboxCreate(&window, &status_text, TXB_ID_1, 0, 30, 127, 50);
    UG_TextboxSetFont(&window, TXB_ID_1, &FONT_6X10);
    UG_TextboxSetText(&window, TXB_ID_1, "IDLE");
    UG_TextboxSetAlignment(&window, TXB_ID_1, ALIGN_CENTER);

    // Left Arrow
    UG_TextboxCreate(&window, &arrow_left, TXB_ID_2, 4, 25, 14, 35);
    UG_TextboxSetFont(&window, TXB_ID_2, &FONT_6X10);
    UG_TextboxSetText(&window, TXB_ID_2, "<");
    UG_TextboxSetAlignment(&window, TXB_ID_2, ALIGN_CENTER_LEFT);

    // Right Arrow
    UG_TextboxCreate(&window, &arrow_right, TXB_ID_3, 113, 25, 123, 35);
    UG_TextboxSetFont(&window, TXB_ID_3, &FONT_6X10);
    UG_TextboxSetText(&window, TXB_ID_3, ">");
    UG_TextboxSetAlignment(&window, TXB_ID_3, ALIGN_CENTER_RIGHT);
}

void UI_Activity_Show(void)
{
    UG_TextboxSetText(&window, TXB_ID_2, "<");
    UG_TextboxSetText(&window, TXB_ID_3, ">");
    UG_WindowShow(&window);
}

void UI_Activity_Hide(void)
{
    UG_WindowHide(&window);
}

void UI_Activity_SetStatus(const char* status)
{
    UG_TextboxSetText(&window, TXB_ID_1, (char*)status);
    UG_TextboxSetText(&window, TXB_ID_2, "");
    UG_TextboxSetText(&window, TXB_ID_3, "");
}
