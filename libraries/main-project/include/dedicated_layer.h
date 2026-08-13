#ifndef DEDICATED_LAYER_H
#define DEDICATED_LAYER_H
#include <stdint.h>
#include <stdbool.h>
#include "oled.h"
#include "gui_port.h"
#include "notification_state.h"

#define DEDICATED_LAYER_WIDTH    24
#define DEDICATED_LAYER_X_START  (OLED_WIDTH - DEDICATED_LAYER_WIDTH)

#define CONTENT_AREA_WIDTH       (OLED_WIDTH - DEDICATED_LAYER_WIDTH)

void dedicated_layer_init(void);
void dedicated_layer_draw(void);

#endif
