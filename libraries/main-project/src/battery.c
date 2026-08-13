/* This is a dummy implementation of the ADC battery reading
 * for pulp-runtime usage */

#include "battery.h"

/* Min and max batt value example */
#define ADC_MIN   1500
#define ADC_MAX   2900

uint16_t battery_read_adc_dummy(void)
{
    /* DUMMY: (± 82%). */
    return 2650;
}

uint8_t battery_get_percent(void)
{
    uint16_t adc = battery_read_adc_dummy();

    if (adc <= ADC_MIN) return 0;
    if (adc >= ADC_MAX) return 100;

    uint32_t percent = (uint32_t)(adc - ADC_MIN) * 100 / (ADC_MAX - ADC_MIN);
    return (uint8_t)percent;
}
