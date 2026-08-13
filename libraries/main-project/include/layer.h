#ifndef LAYER_H
#define LAYER_H
#include <stdint.h>
#include <stdbool.h>
#include "oled.h"
#include "gui_port.h"

typedef enum {
    LAYER_WATCHFACE = 0,
    LAYER_NOTIFICATION,
    LAYER_STEPCOUNT,
    LAYER_ACTIVITY,
    LAYER_HEARTRATE,
    LAYER_SPO2,
    LAYER_COUNT
} layer_id_t;

typedef enum {
    LAYER_EVENT_SHORT = 0,
    LAYER_EVENT_HOLD
} layer_event_t;

typedef struct {
    const char *name;
    void (*draw)(void);
    void (*interact)(layer_event_t event);
    bool (*handle_left)(layer_event_t event);
} layer_ops_t;

extern const layer_ops_t g_layers[LAYER_COUNT];
void layer_manager_init(void);
void layer_manager_next(void);
void layer_manager_goto_notification(void);
void layer_manager_goto(layer_id_t id);

layer_id_t layer_manager_current(void);

/* for right click events */
void layer_manager_interact(layer_event_t event);
bool layer_manager_handle_left(layer_event_t event);
void layer_manager_draw_current(void);

#endif
