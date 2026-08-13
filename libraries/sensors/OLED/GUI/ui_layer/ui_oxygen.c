#include "ui_oxygen.h"
#include "../oled.h"
#include "../GUI/gui_port.h"
#include <stdio.h>

#define MAX_OBJECTS 5
static UG_WINDOW window;
static UG_OBJECT objects[MAX_OBJECTS];
static UG_TEXTBOX title;
static UG_TEXTBOX value_text;
static UG_PROGRESS progress_bar;
static UG_TEXTBOX arrow_left;
static UG_TEXTBOX arrow_right;

void UI_Oxygen_Init(void)
{
    UG_WindowCreate(&window, objects, MAX_OBJECTS, NULL);
    UG_WindowResize(&window, 0, 0, 127, 63);
    UG_WindowSetStyle(&window, WND_STYLE_2D);
    window.fc = C_WHITE;
    window.bc = C_BLACK;

    // Title
    UG_TextboxCreate(&window, &title, TXB_ID_0, 0, 2, 127, 15);
    UG_TextboxSetFont(&window, TXB_ID_0, &FONT_6X10);
    UG_TextboxSetText(&window, TXB_ID_0, "Blood Oxygen (SpO2)");
    UG_TextboxSetAlignment(&window, TXB_ID_0, ALIGN_CENTER);

    // Value Text
    UG_TextboxCreate(&window, &value_text, TXB_ID_1, 0, 20, 127, 40);
    // Note: If you have a larger font (e.g. 10x16 or bigger), use it here.
    // We default to FONT_6X10 for safety.
    UG_TextboxSetFont(&window, TXB_ID_1, &FONT_6X10);
    UG_TextboxSetText(&window, TXB_ID_1, "-- %");
    UG_TextboxSetAlignment(&window, TXB_ID_1, ALIGN_CENTER);

    // Progress bar (as a visual indicator)
    UG_ProgressCreate(&window, &progress_bar, PGB_ID_0, 10, 45, 118, 55);
    UG_ProgressSetStyle(&window, PGB_ID_0, PGB_STYLE_2D);
    UG_ProgressSetProgress(&window, PGB_ID_0, 0); 

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

void UI_Oxygen_Show(void)
{
    UG_WindowShow(&window);
}

void UI_Oxygen_Hide(void)
{
    UG_WindowHide(&window);
}

void UI_Oxygen_SetLevel(uint8_t spo2_value)
{
    // Convert number to string
    static char str_buf[16];
    snprintf(str_buf, sizeof(str_buf), "%d %%", spo2_value);
    
    // Update Text
    UG_TextboxSetText(&window, TXB_ID_1, str_buf);
    
    // Update Progress bar (mapped 0-100)
    UG_ProgressSetProgress(&window, PGB_ID_0, spo2_value);
}
