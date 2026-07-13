/**
 * @file test_max30102.c
 * @brief Hardware Test / Bring-up Runner for the MAX30102 Driver
 *
 * Runs against real PULP/Nusacore FPGA hardware only (no host-side mock).
 * Exercises init, configuration, and raw ADC acquisition, then drops into
 * an infinite loop continuously streaming raw RED/IR samples for live
 * monitoring on real hardware.
 *
 * No PPG signal processing (filtering, HR, SpO2, etc.) is tested or
 * referenced here - that belongs to a separate algorithm library built
 * on top of this driver.
 */

#include "pulp.h"
#include "max30102.h"
#include "max30102_regs.h"
#include <stdio.h>

int tests_passed = 0;
int tests_failed = 0;

#define ASSERT(condition, test_name) \
    do { \
        if (!(condition)) { \
            printf("[FAIL] %s\n", test_name); \
            tests_failed++; \
        } else { \
            printf("[PASS] %s\n", test_name); \
            tests_passed++; \
        } \
    } while (0)

max30102_t sensor;

void test_initialization(void) {
    printf("\n--- Test: Initialization ---\n");
    max30102_status_t status = max30102_init(&sensor, 0); // Use I2C port 0
    ASSERT(status == MAX30102_OK, "max30102_init returns OK");
}

void test_reset(void) {
    printf("\n--- Test: Reset ---\n");
    max30102_status_t status = max30102_reset(&sensor);
    ASSERT(status == MAX30102_OK, "max30102_reset returns OK");
}

void test_part_id(void) {
    printf("\n--- Test: Part ID ---\n");
    uint8_t part_id = 0;
    max30102_status_t status = max30102_get_part_id(&sensor, &part_id);
    ASSERT(status == MAX30102_OK, "max30102_get_part_id returns OK");
    ASSERT(part_id == MAX30102_EXPECTED_PART_ID, "Part ID is 0x15");
}

void test_configure_default(void) {
    printf("\n--- Test: Configure with Default Settings ---\n");
    max30102_config_t config = max30102_get_default_config();
    max30102_status_t status = max30102_configure(&sensor, &config);
    ASSERT(status == MAX30102_OK, "max30102_configure (default) returns OK");
}

void test_configure_custom(void) {
    printf("\n--- Test: Configure with Custom Settings ---\n");
    max30102_config_t config;
    config.sample_rate     = MAX30102_SR_100;
    config.adc_range       = MAX30102_ADC_RANGE_8192;
    config.pulse_width     = MAX30102_PULSE_WIDTH_16BIT;
    config.fifo_avg        = MAX30102_FIFO_AVG_8;
    config.red_led_current = 0x1F;
    config.ir_led_current  = 0x1F;

    max30102_status_t status = max30102_configure(&sensor, &config);
    ASSERT(status == MAX30102_OK, "max30102_configure (custom) returns OK");
}

void test_clear_fifo(void) {
    printf("\n--- Test: FIFO Clear ---\n");
    max30102_status_t status = max30102_clear_fifo(&sensor);
    ASSERT(status == MAX30102_OK, "max30102_clear_fifo returns OK");
}

void test_read_sample(void) {
    printf("\n--- Test: Read RAW Sample ---\n");
    max30102_sample_t sample = {0};

    max30102_status_t status = max30102_clear_fifo(&sensor);
    ASSERT(status == MAX30102_OK, "FIFO clear returns OK");

    // Poll for the first sample: right after a clear, the sensor needs
    // roughly one sample period before new data lands in the FIFO, so
    // MAX30102_ERROR_NO_DATA immediately afterward is expected, not a fault.
    status = MAX30102_ERROR_NO_DATA;
    int attempts = 0;
    const int max_attempts = 200; // generous timeout across sample rates
    while (status == MAX30102_ERROR_NO_DATA && attempts < max_attempts) {
        status = max30102_read_sample(&sensor, &sample);
        if (status == MAX30102_ERROR_NO_DATA) {
            for (volatile int i = 0; i < 10000; i++); // small settle delay
            attempts++;
        }
    }
    ASSERT(status == MAX30102_OK, "max30102_read_sample returns OK");

    printf("RED : %u\n", sample.red);
    printf("IR  : %u\n", sample.ir);
}

void test_fifo_empty(void) {
    printf("\n--- Test: FIFO Empty ---\n");

    max30102_clear_fifo(&sensor);

    max30102_sample_t sample;
    max30102_status_t status = max30102_read_sample(&sensor, &sample);
    ASSERT(status == MAX30102_ERROR_NO_DATA, "read_sample reports NO_DATA on empty FIFO");
}

void test_invalid_param(void) {
    printf("\n--- Test: Invalid Parameter Handling ---\n");

    max30102_status_t status;

    status = max30102_read_sample(NULL, NULL);
    ASSERT(status == MAX30102_ERROR_INVALID_PARAM, "read_sample rejects NULL device");

    status = max30102_read_sample(&sensor, NULL);
    ASSERT(status == MAX30102_ERROR_INVALID_PARAM, "read_sample rejects NULL sample buffer");

    status = max30102_configure(&sensor, NULL);
    ASSERT(status == MAX30102_ERROR_INVALID_PARAM, "configure rejects NULL config");
}

/**
 * @brief Continuously stream raw RED/IR samples. Never returns.
 *
 * Intended for live bring-up/monitoring on real hardware. Explicitly
 * re-applies the default configuration first - it must not simply
 * inherit whatever configuration a prior test left the sensor in
 * (e.g. test_configure_custom() intentionally uses low-sensitivity
 * settings to prove max30102_configure() works with custom values).
 */
void run_continuous_acquisition(void) {
    printf("\n--- Continuous RAW Sample Acquisition (runs forever) ---\n");

    max30102_config_t config = max30102_get_default_config();
    max30102_status_t cfg_status = max30102_configure(&sensor, &config);
    if (cfg_status != MAX30102_OK) {
        printf("Failed to (re)configure sensor before acquisition (%d)\n", cfg_status);
        return;
    }

    printf("RED,IR\n");

    while (1) {
        max30102_sample_t sample;
        max30102_status_t status = max30102_read_sample(&sensor, &sample);

        switch (status) {
            case MAX30102_OK:
                printf("%u,%u\n", sample.red, sample.ir);
                break;

            case MAX30102_ERROR_NO_DATA:
                break;

            default:
                printf("Read Error (%d)\n", status);
                break;
        }

        for (volatile int i = 0; i < 100000; i++); // pacing delay
    }
}

int main(void) {
    printf("==========================================\n");
    printf("      MAX30102 DRIVER HARDWARE TEST       \n");
    printf("==========================================\n");

    test_initialization();
    test_reset();
    test_part_id();
    test_configure_default();
    test_configure_custom();
    test_clear_fifo();
    test_read_sample();
    test_fifo_empty();
    test_invalid_param();

    printf("\n==========================================\n");
    printf("TEST RESULTS: %d PASS, %d FAIL\n", tests_passed, tests_failed);
    printf("==========================================\n");

    // Falls through into an infinite acquisition loop for live monitoring.
    run_continuous_acquisition();

    return 0; // unreachable
}

void pe_start(void) {}

// Dummy micros() to satisfy linker error in I2C driver
uint32_t micros(void) {
    return 0;
}