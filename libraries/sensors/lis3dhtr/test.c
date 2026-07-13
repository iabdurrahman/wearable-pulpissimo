#include <stdint.h>
#include <stdio.h>
#include "pulp.h"
#include "lis3dhtr.h"

void pe_start(void) {}

int main(void)
{
    i2c_t *i2c = lis3dhtr_open();
    if (i2c == NULL) return -1;
    printf("[OK] i2c_open succeeded\n\r");

    int ret = lis3dhtr_init(i2c);
    if (ret != 0) {
        printf("[ERROR] lis3dhtr_init failed, code=%d\n\r", ret);
        i2c_close(i2c);
        return ret;
    }
    printf("[OK] lis3dhtr_init succeeded\n\r");

    accel_data_t accel;
    while (1) {
        ret = lis3dhtr_read_accel(i2c, &accel);
        if (ret == LIS3DHTR_OK) {
            printf("X=%dmg Y=%dmg Z=%dmg\n\r",
                   accel.x, accel.y, accel.z);
        } else {
            printf("[ERROR] read_accel failed, code=%d\n\r", ret);
        }
    }

    i2c_close(i2c);
    return 0;
}