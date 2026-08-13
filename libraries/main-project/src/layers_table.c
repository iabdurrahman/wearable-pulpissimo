#include "layer.h"

void layer_watchface_draw(void);
void layer_watchface_interact(layer_event_t event);
bool layer_watchface_handle_left(layer_event_t event);

void layer_notification_draw(void);
void layer_notification_interact(layer_event_t event);

void layer_stepcount_draw(void);
void layer_stepcount_interact(layer_event_t event);

void layer_activity_draw(void);
void layer_activity_interact(layer_event_t event);

void layer_heartrate_draw(void);
void layer_heartrate_interact(layer_event_t event);

void layer_spo2_draw(void);
void layer_spo2_interact(layer_event_t event);

const layer_ops_t g_layers[LAYER_COUNT] = {
    [LAYER_WATCHFACE]   = { .name = "Watch Face",   .draw = layer_watchface_draw,   .interact = layer_watchface_interact,   .handle_left = layer_watchface_handle_left },
    [LAYER_NOTIFICATION]= { .name = "Notification", .draw = layer_notification_draw,.interact = layer_notification_interact,.handle_left = NULL },
    [LAYER_STEPCOUNT]   = { .name = "Step Count",   .draw = layer_stepcount_draw,   .interact = layer_stepcount_interact,   .handle_left = NULL },
    [LAYER_ACTIVITY]    = { .name = "Activity",     .draw = layer_activity_draw,    .interact = layer_activity_interact,    .handle_left = NULL },
    [LAYER_HEARTRATE]   = { .name = "Heart Rate",   .draw = layer_heartrate_draw,   .interact = layer_heartrate_interact,   .handle_left = NULL },
    [LAYER_SPO2]        = { .name = "SpO2",         .draw = layer_spo2_draw,        .interact = layer_spo2_interact,        .handle_left = NULL },
};
