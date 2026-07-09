/*
 * Copyright (C) 2026 ICDeC
 *
 * Test Application: L3G4200D Gyroscope Sensor
 *
 * CATATAN DIAGNOSTIK: setvbuf/_IONBF DAN fflush() tidak didukung
 * minimal libc pulp-runtime (keduanya gagal link). Ini artinya printf
 * di sistem ini sudah unbuffered secara inheren -- setiap karakter
 * langsung dikirim ke UART, tidak perlu di-flush manual.
 */

#include <stdio.h>
#include "pulp.h"
#include "l3g4200d.h"
#include "l3g4200d_guard.h"

#define NUM_READINGS 20   /* Number of gyro readings to take */

int main()
{
    gyro_status_t     status;
    l3g4200d_config_t cfg;
    gyro_raw_t        raw;
    gyro_dps_t        dps;
    uint8_t           who_am_i;
    int               pass_count = 0;
    int               fail_count = 0;

    printf("========================================\n");
    printf(" L3G4200D Gyroscope Test Application\n");
    printf(" ICDeC PULPissimo FPGA Board\n");
    printf("========================================\n\n");

    /* --- TAMBAHAN: selftest guard timer dulu, TANPA I2C sama sekali --- */
    printf("[PRE-TEST] Menguji guard timer secara terisolasi...\n");
    int guard_ok = l3g4200d_guard_selftest();
    if (!guard_ok) {
        printf("[PRE-TEST] Guard timer TIDAK berfungsi. Perbaiki ini dulu\n");
        printf("[PRE-TEST] sebelum lanjut ke test I2C sensor.\n");
        return -1;
    }
    printf("[PRE-TEST] Guard timer OK, lanjut ke test sensor.\n\n");
    /* --- akhir tambahan --- */

    /* ---- Test 1: Default Configuration ---- */
    printf("[TEST 1] Loading default configuration...\n");
    status = l3g4200d_default_config(&cfg);
    if (status == GYRO_OK) {
        printf("  PASS: Default config loaded\n");
        printf("  I2C addr=0x%02X, id=%d, freq=%u, range=%d\n",
               cfg.i2c_addr, cfg.i2c_id, cfg.i2c_freq, cfg.range);
        pass_count++;
    } else {
        printf("  FAIL: Could not load default config (err=%d)\n", status);
        fail_count++;
    }
    printf("\n");

    /* ---- Test 2: Initialization (auto-detect address) ---- */
    printf("[TEST 2] Initializing L3G4200D...\n");

    /* Try default address first (0x69) */
    printf("  Trying addr=0x%02X...\n", cfg.i2c_addr);
    status = l3g4200d_init_guarded(&cfg);

    /* If default fails, try alternate address (0x68) */
    if (status != GYRO_OK) {
        printf("  addr=0x%02X failed (err=%d), trying 0x%02X...\n",
               cfg.i2c_addr, status, L3G4200D_I2C_ADDR_ALT);
        cfg.i2c_addr = L3G4200D_I2C_ADDR_ALT;
        status = l3g4200d_init_guarded(&cfg);
    }

    printf("  l3g4200d_init returned: %d\n", status);
    if (status == GYRO_OK) {
        printf("  PASS: Sensor initialized on addr=0x%02X\n", cfg.i2c_addr);
        pass_count++;
    } else {
        printf("  FAIL: Initialization failed on both addresses (err=%d)\n", status);
        printf("  CHECK: Verify wiring, pull-ups on SDA/SCL, and SDO pin state\n");
        fail_count++;
        printf("\n========================================\n");
        printf(" RESULTS: %d PASSED, %d FAILED\n", pass_count, fail_count);
        printf("========================================\n");
        return -1;
    }
    printf("\n");

    /* ---- Test 3: WHO_AM_I Verification ---- */
    printf("[TEST 3] Reading WHO_AM_I register...\n");
    status = l3g4200d_who_am_i(&who_am_i);
    if (status == GYRO_OK && who_am_i == L3G4200D_WHO_AM_I_VALUE) {
        printf("  PASS: WHO_AM_I = 0x%02X (expected 0x%02X)\n",
               who_am_i, L3G4200D_WHO_AM_I_VALUE);
        pass_count++;
    } else {
        printf("  FAIL: WHO_AM_I = 0x%02X (expected 0x%02X, err=%d)\n",
               who_am_i, L3G4200D_WHO_AM_I_VALUE, status);
        fail_count++;
    }
    printf("\n");

    /* ---- Test 4: Raw Data Reading ---- */
    printf("[TEST 4] Reading raw gyroscope data (%d samples)...\n", NUM_READINGS);
    printf("  %-6s  %8s  %8s  %8s\n", "Sample", "X_raw", "Y_raw", "Z_raw");
    printf("  ------  --------  --------  --------\n");

    int raw_pass = 1;
    for (int i = 0; i < NUM_READINGS; i++) {
        status = l3g4200d_read_raw(&raw);
        if (status == GYRO_OK) {
            printf("  %-6d  %8d  %8d  %8d\n", i + 1, raw.x, raw.y, raw.z);
        } else {
            printf("  %-6d  ERROR (err=%d)\n", i + 1, status);
            raw_pass = 0;
        }
        for (volatile int d = 0; d < 50000; d++);
    }
    if (raw_pass) {
        printf("  PASS: All raw readings completed\n");
        pass_count++;
    } else {
        printf("  FAIL: Some raw readings failed\n");
        fail_count++;
    }
    printf("\n");

    /* ---- Test 5: DPS Data Reading ---- */
    printf("[TEST 5] Reading gyroscope data in deg/s (%d samples)...\n", NUM_READINGS);
    printf("  %-6s  %10s  %10s  %10s\n", "Sample", "X (°/s)", "Y (°/s)", "Z (°/s)");
    printf("  ------  ----------  ----------  ----------\n");

    int dps_pass = 1;
    for (int i = 0; i < NUM_READINGS; i++) {
        status = l3g4200d_read_dps(&dps);
        if (status == GYRO_OK) {
            printf("  %-6d  %6d.%02d  %6d.%02d  %6d.%02d\n",
                   i + 1,
                   (int)dps.x, (int)(dps.x * 100) % 100,
                   (int)dps.y, (int)(dps.y * 100) % 100,
                   (int)dps.z, (int)(dps.z * 100) % 100);
        } else {
            printf("  %-6d  ERROR (err=%d)\n", i + 1, status);
            dps_pass = 0;
        }
        for (volatile int d = 0; d < 50000; d++);
    }
    if (dps_pass) {
        printf("  PASS: All DPS readings completed\n");
        pass_count++;
    } else {
        printf("  FAIL: Some DPS readings failed\n");
        fail_count++;
    }
    printf("\n");

    /* ---- Test 6: Range Change ---- */
    printf("[TEST 6] Changing range to ±500 dps...\n");
    status = l3g4200d_set_range(L3G4200D_RANGE_500DPS);
    if (status == GYRO_OK) {
        printf("  PASS: Range changed to ±500 dps\n");
        pass_count++;

        status = l3g4200d_read_dps(&dps);
        if (status == GYRO_OK) {
            printf("  Verification read: X=%d.%02d, Y=%d.%02d, Z=%d.%02d °/s\n",
                   (int)dps.x, (int)(dps.x * 100) % 100,
                   (int)dps.y, (int)(dps.y * 100) % 100,
                   (int)dps.z, (int)(dps.z * 100) % 100);
        }
    } else {
        printf("  FAIL: Range change failed (err=%d)\n", status);
        fail_count++;
    }
    printf("\n");

    /* ---- Test 7: De-initialization ---- */
    printf("[TEST 7] De-initializing sensor...\n");
    status = l3g4200d_deinit();
    if (status == GYRO_OK) {
        printf("  PASS: Sensor de-initialized\n");
        pass_count++;
    } else {
        printf("  FAIL: De-initialization failed (err=%d)\n", status);
        fail_count++;
    }
    printf("\n");

    /* ---- Results ---- */
    printf("========================================\n");
    printf(" RESULTS: %d PASSED, %d FAILED\n", pass_count, fail_count);
    if (fail_count == 0) {
        printf(" STATUS: ALL TESTS PASSED\n");
    } else {
        printf(" STATUS: SOME TESTS FAILED\n");
    }
    printf("========================================\n");

    return (fail_count == 0) ? 0 : -1;
}

void pe_start(void)
{
}