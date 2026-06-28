
#include <stdio.h>
#include "pulp.h"
#include "rtc.h"

// UNIT TEST RTC DS1307 - UNTUK RISC-V BOARD (FPGA)

// PULP Runtime entry point - required by crt0
void pe_start(void);
int main(void);

void pe_start(void) {
    main();
}

int main(void) {
    printf("\n");
    printf("RTC DS1307 Unit Test - RISC-V BOARD\n");
    printf("\nInisialisasi I2C Bus\n");
    
    i2c_dev_t rtc_dev_conf;
    i2c_t *rtc_handle;

    printf("Menginisialisasi I2C_0...\n");
    if (rtc_init(&rtc_dev_conf, &rtc_handle) != 0) {
        printf("[ERROR] I2C initialization failed!\n");
        return -1;
    }
    printf("[OK] I2C Initialized!\n");
    printf("\n Set Waktu Awal \n");
    
    rtc_time_t waktu_awal = {
        .seconds = 0,
        .minutes = 30,
        .hours = 14,
        .day = 4,      // Kamis
        .date = 24,
        .month = 6,
        .year = 26
    };

    printf("Setting initial time: %02d:%02d:%02d | %02d/%02d/20%02d\n",
           waktu_awal.hours, waktu_awal.minutes, waktu_awal.seconds,
           waktu_awal.date, waktu_awal.month, waktu_awal.year);
    
    if (rtc_set_time(rtc_handle, &waktu_awal) != 0) {
        printf("[ERROR] Failed to set RTC time!\n");
    } else {
        printf("[OK] RTC time set successfully!\n");
    }

    printf("\n Reading RTC Time\n");
    
    rtc_time_t waktu_baca = {0};
    
    for (int i = 0; i < 10; i++) {
        if (rtc_get_time(rtc_handle, &waktu_baca) == 0) {
            printf("[%d] %02d:%02d:%02d | %02d/%02d/20%02d\n",
                   i+1,
                   waktu_baca.hours, waktu_baca.minutes, waktu_baca.seconds,
                   waktu_baca.date, waktu_baca.month, waktu_baca.year);
        } else {
            printf("[%d] ERROR reading RTC!\n", i+1);
        }

        // Delay ~1 detik agar DS1307 sempat update registernya
        for (volatile int d = 0; d < 1000000; d++);
    }

    printf("\nTest completed successfully!\n");
    
    return 0;
}

