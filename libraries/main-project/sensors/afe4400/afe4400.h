#ifndef AFE4400_H
#define AFE4400_H

#include <stdint.h>
#include <stdbool.h>
#include "pulp.h"

typedef enum {
    AFE4400_OK                  = 0,
    AFE4400_ERROR_INVALID_PARAM = -1,
    AFE4400_ERROR_SPI_OPEN      = -2,
    AFE4400_ERROR_NO_DATA       = -3
} afe4400_status_t;

typedef struct {
    uint32_t red;
    uint32_t ir;
} afe4400_sample_t;

typedef struct {
    uint32_t sample_rate;
} afe4400_config_t;

typedef struct {
    int  spi_port;
    bool ready;
#ifndef SOFTWARE_TEST
    spim_t *spim;
#endif
    int  last_rdy_level;
} afe4400_t;

afe4400_config_t afe4400_get_default_config(void);
afe4400_status_t afe4400_init(afe4400_t *dev, int spi_port);
afe4400_status_t afe4400_configure(afe4400_t *dev, const afe4400_config_t *config);
afe4400_status_t afe4400_read_sample(afe4400_t *dev, afe4400_sample_t *sample);

/* GPIO helpers (not yet implemented), look at button.c for implementation examples*/
void board_gpio_init_out(int pin, int initial_level);
void board_gpio_init_in(int pin);
void board_gpio_write(int pin, int level);
int  board_gpio_read(int pin);

#endif