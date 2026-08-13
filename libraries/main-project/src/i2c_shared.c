#include "i2c_shared.h"
#include "rtc.h"
#include "lis3dhtr.h"
#include <stdbool.h>
#include <stdio.h>

/* Cache device yang sedang aktif di bus, supaya i2c_shared_select() tidak
 * perlu panggil i2c_open() lagi kalau device yang diminta sama dengan yang
 * terakhir dipilih */
static uint8_t  s_active_addr7   = 0x00;
static uint32_t s_active_baud    = 0;
static bool     s_have_active    = false;

void i2c_shared_select(uint8_t addr7, uint32_t baudrate)
{
    if (s_have_active && s_active_addr7 == addr7 && s_active_baud == baudrate) {
        return; /* device yang diminta sudah aktif, tidak perlu reconfigure */
    }

    /* Pasang timeout sebelum reconfigure */
    i2c_settimeout(I2C_SHARED_TIMEOUT_US, true);

    i2c_dev_t conf;
    i2c_dev_init(&conf);
    conf.id = 0;
    conf.cs = (uint32_t)addr7 << 1;
    conf.max_baudrate = baudrate;
    i2c_open(&conf);

    s_active_addr7 = addr7;
    s_active_baud  = baudrate;
    s_have_active  = true;
}

void i2c_shared_select_oled(void)
{
    i2c_shared_select(I2C_SHARED_ADDR7_OLED, I2C_SHARED_BAUD_OLED);
}

void i2c_shared_select_rtc(void)
{
    i2c_shared_select(I2C_SHARED_ADDR7_RTC, I2C_SHARED_BAUD_RTC);
}

static i2c_dev_t s_rtc_conf;
static i2c_t    *s_rtc_i2c = NULL;
static bool      s_rtc_ready = false;

i2c_t *i2c_shared_rtc_handle(void)
{
    if (!s_rtc_ready) {
        int ret = rtc_init(&s_rtc_conf, &s_rtc_i2c);
        if (ret == RTC_OK) {
            s_rtc_ready = true;
            printf("[i2c_shared][OK] rtc_init() sukses\n\r");
        } else {
            printf("[i2c_shared][ERROR] rtc_init() gagal, code=%d\n\r", ret);
        }
    }
    return s_rtc_ready ? s_rtc_i2c : NULL;
}