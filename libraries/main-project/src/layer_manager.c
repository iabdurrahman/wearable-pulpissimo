#include "layer.h"

static layer_id_t s_current = LAYER_WATCHFACE;

void layer_manager_init(void)
{
    s_current = LAYER_WATCHFACE;
}

void layer_manager_next(void)
{
    s_current = (layer_id_t)((s_current + 1) % LAYER_COUNT); /* wraparound */
}

void layer_manager_goto_notification(void)
{
    s_current = LAYER_NOTIFICATION;
}

void layer_manager_goto(layer_id_t id)
{
    if (id < LAYER_COUNT) s_current = id;
}

layer_id_t layer_manager_current(void)
{
    return s_current;
}

void layer_manager_interact(layer_event_t event)
{
    if (g_layers[s_current].interact) {
        g_layers[s_current].interact(event);
    }
}

bool layer_manager_handle_left(layer_event_t event)
{
    if (g_layers[s_current].handle_left) {
        return g_layers[s_current].handle_left(event);
    }
    return false;
}

void layer_manager_draw_current(void)
{
    if (g_layers[s_current].draw) {
        g_layers[s_current].draw();
    }
}
