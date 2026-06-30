/*
 * Stub pulp.h for unit testing on host PC.
 * Provides only the types and function declarations needed by the gyro drivers.
 * The actual implementations are mocked in the test file.
 */
#ifndef __PULP_H__
#define __PULP_H__

#include <stdint.h>
#include <stdio.h>

#ifndef bool
typedef int bool;
#define true 1
#define false 0
#endif

#define L2_DATA

typedef struct {
    int periph_id;
    int id;
    char cs;
    unsigned int max_baudrate;
    unsigned int div;
} i2c_t;

#define I2C_CMD_BUFFER_SIZE 48

typedef struct {
    signed char id;
    signed char cs;
    unsigned int max_baudrate;
} i2c_dev_t;

/* These are implemented as mocks in the test file */
i2c_t *i2c_open(i2c_dev_t *dev);
void i2c_close(i2c_t *i2c);
int i2c_write(i2c_t *dev, unsigned char *data, int length, int send_stop);
int i2c_read(i2c_t *dev_i2c, unsigned char *rx_buff, int length, int pending);
void i2c_dev_init(i2c_dev_t *dev);
int i2c_get_div(int i2c_freq);
void i2c_settimeout(uint32_t timeout, bool reset_on_timeout);
bool i2c_managetimeoutflag(bool clearflag);

#endif /* __PULP_H__ */
