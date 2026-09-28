#ifndef I2C_SHARED_H
#define I2C_SHARED_H
#include "pulp.h"
#include <stdint.h>

/* Used address (7-bit for oled and rtc) */
#define I2C_SHARED_ADDR7_OLED   0x3C
#define I2C_SHARED_BAUD_OLED    400000

#define I2C_SHARED_ADDR7_RTC    0x68
#define I2C_SHARED_BAUD_RTC     100000

/* Timeout for reconfiguring I2C bus to prevent blocking */
#define I2C_SHARED_TIMEOUT_US   10000UL

/* Select I2C bus for a specific device */
void i2c_shared_select(uint8_t addr7, uint32_t baudrate);

void i2c_shared_select_oled(void);
void i2c_shared_select_rtc(void);

i2c_t *i2c_shared_rtc_handle(void);
void i2c_shared_rtc_delete_handle(void);

#endif