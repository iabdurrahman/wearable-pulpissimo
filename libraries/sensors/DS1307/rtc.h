#ifndef RTC_H
#define RTC_H

#include <stdint.h>
#include "pulp.h" 

typedef struct {
    uint8_t seconds; // 0-59
    uint8_t minutes; // 0-59
    uint8_t hours;   // 0-23 (Mode 24 jam)
    uint8_t day;     // 1-7 (Hari dalam seminggu)
    uint8_t date;    // 1-31 (Tanggal)
    uint8_t month;   // 1-12 (Bulan)
    uint8_t year;    // 0-99 (Tahun)
} rtc_time_t;

// Fungsi untuk inisialisasi hardware I2C_0 ke DS1307
int rtc_init(i2c_dev_t *dev_conf, i2c_t **i2c_handle);

// Fungsi untuk membaca waktu desimal dari RTC DS1307
int rtc_get_time(i2c_t *i2c_handle, rtc_time_t *time);

// Fungsi untuk mengatur waktu awal sekaligus mengaktifkan jam (clear bit CH)
int rtc_set_time(i2c_t *i2c_handle, rtc_time_t *time);

// Fungsi konversi BCD untuk unit testing
uint8_t dec_to_bcd(uint8_t val);
uint8_t bcd_to_dec(uint8_t val);

#endif 