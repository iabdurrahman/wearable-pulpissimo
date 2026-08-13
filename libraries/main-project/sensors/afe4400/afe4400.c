/* This is a simple porting dummy implementation of the AFE4400 sensor driver
 * for pulp-runtime usage */

#include "afe4400.h"
#include <stddef.h>

afe4400_config_t afe4400_get_default_config(void)
{
    afe4400_config_t config;
    config.sample_rate = 100; /* placeholder */
    return config;
}

afe4400_status_t afe4400_init(afe4400_t *dev, int spi_port)
{
    if (dev == NULL) return AFE4400_ERROR_INVALID_PARAM;

    /* TODO: bring-up SPI API. */
    dev->spi_port = spi_port;
    dev->ready = true;
    return AFE4400_OK;
}

afe4400_status_t afe4400_configure(afe4400_t *dev, const afe4400_config_t *config)
{
    if (dev == NULL || config == NULL) return AFE4400_ERROR_INVALID_PARAM;
    if (!dev->ready) return AFE4400_ERROR_SPI_OPEN;

    /* TODO: write configurations */
    return AFE4400_OK;
}

afe4400_status_t afe4400_read_sample(afe4400_t *dev, afe4400_sample_t *sample)
{
    if (dev == NULL || sample == NULL) return AFE4400_ERROR_INVALID_PARAM;
    if (!dev->ready) return AFE4400_ERROR_SPI_OPEN;

    /* DUMMY: ppg samples for loading ui simulation */
    static uint32_t counter = 0;
    counter++;

    uint32_t wobble = (counter * 37) % 2000;
    sample->ir  = 60000u + wobble;
    sample->red = 45000u + (wobble / 2);

    return AFE4400_OK;
}