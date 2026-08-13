#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include "pulp.h"
#include "button.h"
#include "layer.h"
#include "dedicated_layer.h"
#include "watchface.h"
#include "stepcount.h"
#include "activity.h"
#include "heartrate.h"
#include "spo2.h"
#include "i2c_shared.h"
#include "oled.h"
#include "gui_port.h"

void pe_start(void) {}
uint32_t micros(void) { return 0; }

volatile uint32_t g_tick_ms = 0;

/* BTN1 = left click, BTN2 = right click */
static ButtonState s_btn1;
static ButtonState s_btn2;

/* Render period for dedicated layer */
#define RENDER_PERIOD_MS         500UL

/* Render period for fast updates */
#define RENDER_PERIOD_FAST_MS    120UL

/* Handle left button click events (BTN1) */
static void handle_left_click(int event)
{
    layer_event_t lev = (event == BUTTON_HOLD) ? LAYER_EVENT_HOLD : LAYER_EVENT_SHORT;
    if (layer_manager_handle_left(lev)) {
        return;
    }

    if (event == BUTTON_SHORT) {
        layer_manager_next();
    } else if (event == BUTTON_HOLD) {
        layer_manager_goto_notification();
    }
}

// Handle right button click events (BTN2)
static void handle_right_click(int event)
{
    if (event == BUTTON_SHORT) {
        layer_manager_interact(LAYER_EVENT_SHORT);
    } else if (event == BUTTON_HOLD) {
        layer_manager_interact(LAYER_EVENT_HOLD);
    }
}

static void render_frame(void)
{
    i2c_shared_select_oled();
    OLED_Clear();
    dedicated_layer_draw();
    layer_manager_draw_current();
    OLED_Update();
}

ButtonState btn1;
ButtonState btn2;

int main(void)
{
    pos_tick_init();

    printf("==========================================\n\r");
    printf("   ICDEC Smartwatch - Layer Integration    \n\r");
    printf("==========================================\n\r");

    OLED_Init();
    gui_port_init();
    OLED_Clear();

    dedicated_layer_init();
    layer_manager_init();

    buttonHardwareInit();
    buttonInit(&s_btn1);
    buttonInit(&s_btn2);

    render_frame();
    long last_render_ms = pos_tick_get_counter_ms();
    layer_id_t prev_layer = layer_manager_current();

    while (1) {
        long now_ms = pos_tick_get_counter_ms();
        g_tick_ms = (uint32_t)now_ms;

        int ev1 = buttonUpdate(&s_btn1, BTN1);
        int ev2 = buttonUpdate(&s_btn2, BTN2);

        if (ev1 != BUTTON_NONE) handle_left_click(ev1);
        if (ev2 != BUTTON_NONE) handle_right_click(ev2);

        /* alarm checking */
        layer_watchface_poll();

        layer_id_t cur_layer = layer_manager_current();
        bool just_entered = (cur_layer != prev_layer);

        if (cur_layer == LAYER_STEPCOUNT) {
            layer_stepcount_poll();
        } else if (cur_layer == LAYER_ACTIVITY) {
            if (just_entered) {
                layer_activity_reset();
            }
            layer_activity_poll();
        } else if (cur_layer == LAYER_HEARTRATE) {
            if (just_entered) layer_heartrate_reset();
            layer_heartrate_poll();
        } else if (cur_layer == LAYER_SPO2) {
            if (just_entered) layer_spo2_reset();
            layer_spo2_poll();
        }
        prev_layer = cur_layer;

        bool need_render = (ev1 != BUTTON_NONE) || (ev2 != BUTTON_NONE);

        bool fast_layer = (cur_layer == LAYER_ACTIVITY) || (cur_layer == LAYER_HEARTRATE) || (cur_layer == LAYER_SPO2);
        uint32_t render_period = fast_layer ? RENDER_PERIOD_FAST_MS : RENDER_PERIOD_MS;
        if ((now_ms - last_render_ms) >= (long)render_period) {
            need_render = true;
            last_render_ms = now_ms;
        }

        if (need_render) {
            render_frame();
        }

        for (volatile int d = 0; d < 10000; d++);
    }

    return 0;
}