#include "ui_alarm.h"
#include "../oled.h"
#include "../GUI/gui_port.h"

#define MAX_OBJECTS 2
static UG_WINDOW window;
static UG_OBJECT objects[MAX_OBJECTS];
static UG_TEXTBOX msg_text;
static UG_BUTTON dismiss_btn;

static bool alarm_active = false;
extern volatile uint32_t g_tick_ms;
static uint32_t alarm_start_time = 0;

static void window_callback(UG_MESSAGE* msg)
{
    if (msg->type == MSG_TYPE_OBJECT && msg->id == OBJ_TYPE_BUTTON && msg->sub_id == BTN_ID_0)
    {
        if (msg->event == OBJ_EVENT_PRESSED) {
            UI_Alarm_Hide();
        }
    }
}

void UI_Alarm_Init(void)
{
    UG_WindowCreate(&window, objects, MAX_OBJECTS, window_callback);
    // Pop-up window in the center
    UG_WindowResize(&window, 4, 4, 123, 59);
    UG_WindowSetStyle(&window, WND_STYLE_2D | WND_STYLE_SHOW_TITLE);
    UG_WindowSetTitleHeight(&window, 14);
    UG_WindowSetTitleText(&window, "ALARM");
    UG_WindowSetTitleTextFont(&window, &FONT_6X10);
    window.fc = C_WHITE; // Inverse colors for alarm to grab attention
    window.bc = C_BLACK;

    // Message
    UG_TextboxCreate(&window, &msg_text, TXB_ID_0, 0, 2, 117, 20);
    UG_TextboxSetFont(&window, TXB_ID_0, &FONT_6X10);
    UG_TextboxSetText(&window, TXB_ID_0, "WARNING!");
    UG_TextboxSetAlignment(&window, TXB_ID_0, ALIGN_CENTER);

    // Dismiss Button
    UG_ButtonCreate(&window, &dismiss_btn, BTN_ID_0, 20, 22, 97, 36);
    UG_ButtonSetFont(&window, BTN_ID_0, &FONT_6X10);
    UG_ButtonSetText(&window, BTN_ID_0, "DISMISS");
    UG_ButtonSetStyle(&window, BTN_ID_0, BTN_STYLE_2D);
}

void UI_Alarm_Show(const char* msg)
{
    UG_TextboxSetText(&window, TXB_ID_0, (char*)msg);
    UG_WindowShow(&window);
    alarm_active = true;
    alarm_start_time = g_tick_ms;
}

void UI_Alarm_Hide(void)
{
    UG_WindowHide(&window);
    alarm_active = false;
}

bool UI_Alarm_IsActive(void)
{
    return alarm_active;
}

void UI_Alarm_Loop(void)
{
    if (!alarm_active) return;
    
    // Auto-dismiss simulation for demo purposes
    // since we don't have a real touch sensor yet.
    // The button is roughly at x:62, y:49 on the global screen
    uint32_t elapsed = g_tick_ms - alarm_start_time;
    if (elapsed > 3000 && elapsed < 3100) {
        port_touch(true, 62, 49); // press
    } else if (elapsed >= 3100 && elapsed < 3200) {
        port_touch(false, 62, 49); // release
    }
}
