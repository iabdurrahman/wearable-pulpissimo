#include "layer.h"
#include "dedicated_layer.h"
#include "gui_widgets.h"
#include "notification_state.h"

static void draw_pending(void)
{
    int16_t cx = CONTENT_AREA_WIDTH / 2;

    draw_warning_icon(cx, 0, 26);
    ug_put_string_centered(25, FONT_8X8, CHAR_W_8X8, g_notification_title);
    ug_put_string_centered(36, FONT_6X8, CHAR_W_6X8, g_notification_detail);
    ug_put_string_centered(46, FONT_6X8, CHAR_W_6X8, "right click");
    ug_put_string_centered(55, FONT_6X8, CHAR_W_6X8, "to dismiss");
}

static void draw_empty(void)
{
    int16_t cx = CONTENT_AREA_WIDTH / 2;

    draw_bell_icon(cx, 2, 30, true);

    ug_put_string_centered(44, FONT_6X8, CHAR_W_6X8, "No");
    ug_put_string_centered(54, FONT_6X8, CHAR_W_6X8, "notifications");
}

void layer_notification_draw(void)
{
    if (g_notification_pending) {
        draw_pending();
    } else {
        draw_empty();
    }
}

void layer_notification_interact(layer_event_t event)
{
    if (event == LAYER_EVENT_SHORT) {
        notification_clear();
    }
}