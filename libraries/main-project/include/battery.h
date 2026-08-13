#ifndef BATTERY_H
#define BATTERY_H
#include <stdint.h>

uint16_t battery_read_adc_dummy(void);
uint8_t battery_get_percent(void);

#endif
