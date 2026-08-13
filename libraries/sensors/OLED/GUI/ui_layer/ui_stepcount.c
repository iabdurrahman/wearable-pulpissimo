#include "ui_stepcount.h"
#include "../oled.h"
#include "../GUI/gui_port.h"
#include <stdio.h>

#define MAX_OBJECTS 5
static UG_WINDOW window;
static UG_OBJECT objects[MAX_OBJECTS];
static UG_TEXTBOX title;
static UG_TEXTBOX step_text;
static UG_TEXTBOX target_text;
static UG_TEXTBOX arrow_left;
static UG_TEXTBOX arrow_right;

void UI_StepCount_Init(void)
{
    UG_WindowCreate(&window, objects, MAX_OBJECTS, NULL);
    UG_WindowResize(&window, 0, 0, 127, 63);
    UG_WindowSetStyle(&window, WND_STYLE_2D);
    window.fc = C_WHITE;
    window.bc = C_BLACK;

    // Title
    UG_TextboxCreate(&window, &title, TXB_ID_0, 0, 2, 127, 15);
    UG_TextboxSetFont(&window, TXB_ID_0, &FONT_6X10);
    UG_TextboxSetText(&window, TXB_ID_0, "Step Count");
    UG_TextboxSetAlignment(&window, TXB_ID_0, ALIGN_CENTER);

    // Step Value Text
    UG_TextboxCreate(&window, &step_text, TXB_ID_1, 0, 20, 127, 40);
    UG_TextboxSetFont(&window, TXB_ID_1, &FONT_6X10);
    UG_TextboxSetText(&window, TXB_ID_1, "0");
    UG_TextboxSetAlignment(&window, TXB_ID_1, ALIGN_CENTER);

    // Target Text
    UG_TextboxCreate(&window, &target_text, TXB_ID_2, 0, 45, 127, 58);
    UG_TextboxSetFont(&window, TXB_ID_2, &FONT_6X10);
    UG_TextboxSetText(&window, TXB_ID_2, "Target: 10000");
    UG_TextboxSetAlignment(&window, TXB_ID_2, ALIGN_CENTER);

    // Left Arrow
    UG_TextboxCreate(&window, &arrow_left, TXB_ID_3, 4, 25, 14, 35);
    UG_TextboxSetFont(&window, TXB_ID_3, &FONT_6X10);
    UG_TextboxSetText(&window, TXB_ID_3, "<");
    UG_TextboxSetAlignment(&window, TXB_ID_3, ALIGN_CENTER_LEFT);

    // Right Arrow
    UG_TextboxCreate(&window, &arrow_right, TXB_ID_4, 113, 25, 123, 35);
    UG_TextboxSetFont(&window, TXB_ID_4, &FONT_6X10);
    UG_TextboxSetText(&window, TXB_ID_4, ">");
    UG_TextboxSetAlignment(&window, TXB_ID_4, ALIGN_CENTER_RIGHT);
}

void UI_StepCount_Show(void)
{
    UG_WindowShow(&window);
}

void UI_StepCount_Hide(void)
{
    UG_WindowHide(&window);
}

void UI_StepCount_SetSteps(uint32_t steps)
{
    static char str_buf[16];
    snprintf(str_buf, sizeof(str_buf), "%lu", (unsigned long)steps);
    
    UG_TextboxSetText(&window, TXB_ID_1, str_buf);
}
