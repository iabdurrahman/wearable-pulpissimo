/*
 * Copyright (C) 2026 ICDeC
 *
 * Test Application: ADS1118 SPI ADC - Battery Voltage Reading
 *
 * Initializes the ADS1118, reads the battery voltage through a
 * single-ended channel (AIN0) behind an external voltage divider, and
 * continuously prints raw ADC codes alongside the computed battery
 * voltage.
 *
 * Usage:
 *   make all SENSOR=ads1118
 *   make run SENSOR=ads1118 platform=fpga
 */

#include <stdio.h>
#include "pulp.h"
#include "ads1118.h"

#define NUM_READINGS 20

int main()
{
    ads1118_status_t status;
    ads1118_config_t cfg;
    int16_t          raw;
    float            voltage;
    int              pass_count = 0;
    int              fail_count = 0;

    printf("========================================\n");
    printf(" ADS1118 Battery Voltage Test\n");
    printf(" ICDeC PULPissimo FPGA Board\n");
    printf("========================================\n\n");

    printf("[TEST-MINIMAL] UART sanity check - if you see this, UART works!\n");


    /* ---- Test 1: Default Configuration ---- */
    printf("[TEST 1] Loading default configuration...\n");
    status = ads1118_default_config(&cfg);
    if (status == ADS1118_OK) {
        printf("  PASS: Default config loaded\n");
        printf("  mux=0x%04X, pga=0x%04X, dr=0x%04X, divider=%d/1000\n",
               cfg.mux, cfg.pga, cfg.data_rate, (int)(cfg.vdivider_ratio * 1000));
        pass_count++;
    } else {
        printf("  FAIL: Could not load default config (err=%d)\n", status);
        fail_count++;
    }
    printf("\n");

    /*
     * NOTE: adjust cfg.vdivider_ratio here to match the actual resistor
     * divider on your board (ratio = R2 / (R1 + R2)) before running on
     * real hardware - otherwise the reported battery voltage will be
     * wrong. The default (0.5) is only a placeholder assumption.
     */

    /* ---- Test 2: Initialization ---- */
    printf("[TEST 2] Initializing ADS1118...\n");
    status = ads1118_init(&cfg);
    if (status == ADS1118_OK) {
        printf("  PASS: Sensor initialized successfully\n");
        pass_count++;
    } else {
        printf("  FAIL: Initialization failed (err=%d)\n", status);
        fail_count++;
        printf("\n========================================\n");
        printf(" RESULTS: %d PASSED, %d FAILED\n", pass_count, fail_count);
        printf("========================================\n");
        return -1;
    }
    printf("\n");

    /* ---- Test 3: Raw + Voltage Reading ---- */
    printf("[TEST 3] Reading battery voltage (%d samples)...\n", NUM_READINGS);
    printf("  %-6s  %10s  %12s\n", "Sample", "Raw", "Voltage (V)");
    printf("  ------  ----------  ------------\n");

    int read_pass = 1;
    for (int i = 0; i < NUM_READINGS; i++) {
        status = ads1118_read_raw(&raw);
        if (status != ADS1118_OK) {
            printf("  %-6d  ERROR (err=%d)\n", i + 1, status);
            read_pass = 0;
            continue;
        }
        status = ads1118_read_battery_voltage(&voltage);
        if (status == ADS1118_OK) {
            printf("  %-6d  %10d  %6d.%03d\n", i + 1, raw,
                   (int)voltage, (int)(voltage * 1000) % 1000);
        } else {
            printf("  %-6d  %10d  ERROR (err=%d)\n", i + 1, raw, status);
            read_pass = 0;
        }
        for (volatile int d = 0; d < 50000; d++);
    }
    if (read_pass) {
        printf("  PASS: All readings completed\n");
        pass_count++;
    } else {
        printf("  FAIL: Some readings failed\n");
        fail_count++;
    }
    printf("\n");

    /* ---- Test 4: De-initialization ---- */
    printf("[TEST 4] De-initializing sensor...\n");
    status = ads1118_deinit();
    if (status == ADS1118_OK) {
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


    printf("[TEST-MINIMAL] Reached end of main without touching SPI.\n");

    return 0;
}

void pe_start(void)
{
}