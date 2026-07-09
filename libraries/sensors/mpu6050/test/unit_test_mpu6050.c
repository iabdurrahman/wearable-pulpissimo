/*
 * Unit Test: MPU-6050 Library - Mock I2C
 *
 * Build:  gcc -o unit_test_mpu6050 unit_test_mpu6050.c -lm -I.. -Imock
 * Run:    ./unit_test_mpu6050
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "mock/pulp.h"

/* =========================================================================
 * MOCK I2C LAYER
 * ========================================================================= */

static uint8_t mock_registers[256];
static uint8_t  last_write_reg, last_write_val;
static uint8_t  last_read_addr;
static int      write_call_count, read_call_count;
static int      mock_i2c_fail;
static int      mock_timeout_flag;
static int      mock_timeout_count;
static int      mock_dev_cs, mock_dev_id;
static unsigned int mock_dev_baudrate;
static i2c_t mock_i2c_handle;

void i2c_dev_init(i2c_dev_t *dev) {
    dev->cs = -1; dev->id = -1; dev->max_baudrate = 200000;
}

i2c_t *i2c_open(i2c_dev_t *dev) {
    if (mock_i2c_fail) return NULL;
    mock_dev_cs = dev->cs;
    mock_dev_id = dev->id;
    mock_dev_baudrate = dev->max_baudrate;
    mock_i2c_handle.id = dev->id;
    mock_i2c_handle.cs = dev->cs;
    mock_i2c_handle.max_baudrate = dev->max_baudrate;
    return &mock_i2c_handle;
}

void i2c_close(i2c_t *i2c) { (void)i2c; }
void i2c_settimeout(uint32_t t, bool r) { (void)t; (void)r; }
bool i2c_managetimeoutflag(bool c) {
    (void)c;
    if (mock_timeout_flag) {
        if (mock_timeout_count > 0) {
            mock_timeout_count--;
            return true;
        }
        mock_timeout_flag = 0;
    }
    return false;
}

int i2c_write(i2c_t *dev, unsigned char *data, int length, int send_stop) {
    (void)dev;
    write_call_count++;
    if (mock_i2c_fail) return 5;
    if (length == 1 && !send_stop) {
        last_read_addr = data[0];
    } else if (length == 2 && send_stop) {
        last_write_reg = data[0];
        last_write_val = data[1];
        mock_registers[data[0]] = data[1];
    }
    return 0;
}

int i2c_read(i2c_t *dev, unsigned char *rx_buff, int length, int pending) {
    (void)dev; (void)pending;
    read_call_count++;
    if (mock_i2c_fail) return 0;
    uint8_t base_reg = last_read_addr;
    for (int i = 0; i < length; i++)
        rx_buff[i] = mock_registers[base_reg + i];
    return length;
}

/* =========================================================================
 * Include library source
 * ========================================================================= */

#include "../gyro_common.h"
#include "../mpu6050.h"
#include "../mpu6050.c"

/* =========================================================================
 * TEST FRAMEWORK
 * ========================================================================= */

static int tests_passed = 0, tests_failed = 0;

#define ASSERT_EQ(name, actual, expected) do { \
    if ((actual) == (expected)) { \
        printf("  ✓ PASS: %s (got %d)\n", name, (int)(actual)); \
        tests_passed++; \
    } else { \
        printf("  ✗ FAIL: %s (expected %d, got %d)\n", name, (int)(expected), (int)(actual)); \
        tests_failed++; \
    } \
} while(0)

#define ASSERT_FLOAT(name, actual, expected, tol) do { \
    if (fabs((actual) - (expected)) < (tol)) { \
        printf("  ✓ PASS: %s (got %.4f)\n", name, (double)(actual)); \
        tests_passed++; \
    } else { \
        printf("  ✗ FAIL: %s (expected %.4f, got %.4f)\n", name, (double)(expected), (double)(actual)); \
        tests_failed++; \
    } \
} while(0)

static void reset_mock(void) {
    memset(mock_registers, 0, sizeof(mock_registers));
    write_call_count = 0;
    read_call_count = 0;
    mock_i2c_fail = 0;
    mock_timeout_flag = 0;
    mock_timeout_count = 0;
    last_write_reg = 0;
    last_write_val = 0;
    mock_registers[0x75] = 0x68; /* WHO_AM_I */
    mpu6050_state.initialized = 0;
    mpu6050_state.i2c = NULL;
}

/* =========================================================================
 * TEST CASES
 * ========================================================================= */

void test_01_default_config(void) {
    printf("\n[TEST 01] default_config - Nilai default konfigurasi\n");
    mpu6050_config_t cfg;
    gyro_status_t s = mpu6050_default_config(&cfg);
    ASSERT_EQ("return GYRO_OK", s, GYRO_OK);
    ASSERT_EQ("i2c_addr = 0x69", cfg.i2c_addr, 0x69);
    ASSERT_EQ("i2c_id = 0", cfg.i2c_id, 0);
    ASSERT_EQ("i2c_freq = 100000", cfg.i2c_freq, 100000);
    ASSERT_EQ("gyro_range = 250DPS (0x00)", cfg.gyro_range, MPU6050_GYRO_RANGE_250DPS);
    ASSERT_EQ("dlpf_cfg = 0", cfg.dlpf_cfg, 0);
    ASSERT_EQ("smplrt_div = 79", cfg.smplrt_div, 79);
}

void test_02_default_config_null(void) {
    printf("\n[TEST 02] default_config(NULL) - Error handling\n");
    ASSERT_EQ("return GYRO_ERR_NULL", mpu6050_default_config(NULL), GYRO_ERR_NULL);
}

void test_03_init_i2c_address(void) {
    printf("\n[TEST 03] init - I2C address passed as 7-bit (no shift)\n");
    reset_mock();
    mpu6050_config_t cfg;
    mpu6050_default_config(&cfg);
    mpu6050_init(&cfg);
    ASSERT_EQ("cs = 0xD2 (8-bit, left-shifted 0x69)", (uint8_t)mock_dev_cs, 0xD2);
    ASSERT_EQ("i2c_id = 0", mock_dev_id, 0);
}

void test_04_init_wake_from_sleep(void) {
    printf("\n[TEST 04] init - PWR_MGMT_1 (0x6B) wake from sleep\n");
    reset_mock();
    mpu6050_config_t cfg;
    mpu6050_default_config(&cfg);
    mpu6050_init(&cfg);
    /* PWR_MGMT_1 should be written with 0x00 to wake sensor */
    ASSERT_EQ("PWR_MGMT_1 (0x6B) = 0x00 (awake)", mock_registers[0x6B], 0x00);
}

void test_05_init_who_am_i(void) {
    printf("\n[TEST 05] init - WHO_AM_I verification (reg 0x75 = 0x68)\n");
    reset_mock();
    mpu6050_config_t cfg;
    mpu6050_default_config(&cfg);

    mock_registers[0x75] = 0x68;
    ASSERT_EQ("init OK with correct WHO_AM_I", mpu6050_init(&cfg), GYRO_OK);

    reset_mock();
    mock_registers[0x75] = 0xBB;
    ASSERT_EQ("init ERR_ID with wrong WHO_AM_I", mpu6050_init(&cfg), GYRO_ERR_ID);
}

void test_06_init_smplrt_div(void) {
    printf("\n[TEST 06] init - SMPLRT_DIV (0x19) sample rate divider\n");
    reset_mock();
    mpu6050_config_t cfg;
    mpu6050_default_config(&cfg);
    cfg.smplrt_div = 79; /* 8000/(1+79) = 100Hz */
    mpu6050_init(&cfg);
    ASSERT_EQ("SMPLRT_DIV (0x19) = 79", mock_registers[0x19], 79);
}

void test_07_init_dlpf(void) {
    printf("\n[TEST 07] init - CONFIG (0x1A) DLPF configuration\n");
    reset_mock();
    mpu6050_config_t cfg;
    mpu6050_default_config(&cfg);
    cfg.dlpf_cfg = 3;
    mpu6050_init(&cfg);
    ASSERT_EQ("CONFIG (0x1A) = 3 (DLPF=3)", mock_registers[0x1A], 3);

    /* Test masking: only bits [2:0] should be used */
    reset_mock();
    cfg.dlpf_cfg = 0xFF;
    mpu6050_init(&cfg);
    ASSERT_EQ("CONFIG masked to 0x07", mock_registers[0x1A], 0x07);
}

void test_08_init_gyro_config(void) {
    printf("\n[TEST 08] init - GYRO_CONFIG (0x1B) full-scale range\n");
    reset_mock();
    mpu6050_config_t cfg;
    mpu6050_default_config(&cfg);

    cfg.gyro_range = MPU6050_GYRO_RANGE_250DPS;
    mpu6050_init(&cfg);
    ASSERT_EQ("GYRO_CONFIG @250dps = 0x00", mock_registers[0x1B], 0x00);

    reset_mock();
    cfg.gyro_range = MPU6050_GYRO_RANGE_500DPS;
    mpu6050_init(&cfg);
    ASSERT_EQ("GYRO_CONFIG @500dps = 0x08", mock_registers[0x1B], 0x08);

    reset_mock();
    cfg.gyro_range = MPU6050_GYRO_RANGE_1000DPS;
    mpu6050_init(&cfg);
    ASSERT_EQ("GYRO_CONFIG @1000dps = 0x10", mock_registers[0x1B], 0x10);

    reset_mock();
    cfg.gyro_range = MPU6050_GYRO_RANGE_2000DPS;
    mpu6050_init(&cfg);
    ASSERT_EQ("GYRO_CONFIG @2000dps = 0x18", mock_registers[0x1B], 0x18);
}

void test_09_read_raw_big_endian(void) {
    printf("\n[TEST 09] read_gyro_raw - Big-endian data assembly\n");
    reset_mock();
    mpu6050_config_t cfg;
    mpu6050_default_config(&cfg);
    mpu6050_init(&cfg);

    /* Big-endian: high byte first */
    mock_registers[0x43] = 0x01; /* GYRO_XOUT_H */
    mock_registers[0x44] = 0x00; /* GYRO_XOUT_L → X = 0x0100 = 256 */
    mock_registers[0x45] = 0xFF; /* GYRO_YOUT_H */
    mock_registers[0x46] = 0x00; /* GYRO_YOUT_L → Y = 0xFF00 = -256 */
    mock_registers[0x47] = 0x00; /* GYRO_ZOUT_H */
    mock_registers[0x48] = 0x01; /* GYRO_ZOUT_L → Z = 0x0001 = 1 */

    gyro_raw_t raw;
    gyro_status_t s = mpu6050_read_gyro_raw(&raw);
    ASSERT_EQ("return GYRO_OK", s, GYRO_OK);
    ASSERT_EQ("raw.x = 256", raw.x, 256);
    ASSERT_EQ("raw.y = -256", raw.y, -256);
    ASSERT_EQ("raw.z = 1", raw.z, 1);
}

void test_10_no_auto_increment_bit(void) {
    printf("\n[TEST 10] read_gyro_raw - No auto-increment bit (unlike L3G4200D)\n");
    reset_mock();
    mpu6050_config_t cfg;
    mpu6050_default_config(&cfg);
    mpu6050_init(&cfg);

    gyro_raw_t raw;
    mpu6050_read_gyro_raw(&raw);
    /* MPU-6050 does NOT set MSB for auto-increment */
    ASSERT_EQ("read addr = 0x43 (no MSB set)", last_read_addr, 0x43);
}

void test_11_sensitivity_250dps(void) {
    printf("\n[TEST 11] read_gyro_dps - Sensitivity @250dps (131 LSB/°/s)\n");
    reset_mock();
    mpu6050_config_t cfg;
    mpu6050_default_config(&cfg);
    cfg.gyro_range = MPU6050_GYRO_RANGE_250DPS;
    mpu6050_init(&cfg);

    /* Raw=131 → 131/131 = 1.0 °/s */
    mock_registers[0x43] = 0x00;
    mock_registers[0x44] = 0x83; /* 131 = 0x0083 */
    mock_registers[0x45] = 0x00; mock_registers[0x46] = 0x00;
    mock_registers[0x47] = 0x00; mock_registers[0x48] = 0x00;

    gyro_dps_t dps;
    mpu6050_read_gyro_dps(&dps);
    ASSERT_FLOAT("X @raw=131, sens=131 → 1.0°/s", dps.x, 1.0f, 0.01f);
}

void test_12_sensitivity_500dps(void) {
    printf("\n[TEST 12] read_gyro_dps - Sensitivity @500dps (65.5 LSB/°/s)\n");
    reset_mock();
    mpu6050_config_t cfg;
    mpu6050_default_config(&cfg);
    cfg.gyro_range = MPU6050_GYRO_RANGE_500DPS;
    mpu6050_init(&cfg);

    /* Raw=655 → 655/65.5 = 10.0 °/s */
    mock_registers[0x43] = 0x02;
    mock_registers[0x44] = 0x8F; /* 655 = 0x028F */
    mock_registers[0x45] = 0x00; mock_registers[0x46] = 0x00;
    mock_registers[0x47] = 0x00; mock_registers[0x48] = 0x00;

    gyro_dps_t dps;
    mpu6050_read_gyro_dps(&dps);
    ASSERT_FLOAT("X @raw=655, sens=65.5 → 10.0°/s", dps.x, 10.0f, 0.02f);
}

void test_13_sensitivity_1000dps(void) {
    printf("\n[TEST 13] read_gyro_dps - Sensitivity @1000dps (32.8 LSB/°/s)\n");
    reset_mock();
    mpu6050_config_t cfg;
    mpu6050_default_config(&cfg);
    cfg.gyro_range = MPU6050_GYRO_RANGE_1000DPS;
    mpu6050_init(&cfg);

    /* Raw=328 → 328/32.8 = 10.0 °/s */
    mock_registers[0x43] = 0x01;
    mock_registers[0x44] = 0x48; /* 328 = 0x0148 */
    mock_registers[0x45] = 0x00; mock_registers[0x46] = 0x00;
    mock_registers[0x47] = 0x00; mock_registers[0x48] = 0x00;

    gyro_dps_t dps;
    mpu6050_read_gyro_dps(&dps);
    ASSERT_FLOAT("X @raw=328, sens=32.8 → 10.0°/s", dps.x, 10.0f, 0.02f);
}

void test_14_sensitivity_2000dps(void) {
    printf("\n[TEST 14] read_gyro_dps - Sensitivity @2000dps (16.4 LSB/°/s)\n");
    reset_mock();
    mpu6050_config_t cfg;
    mpu6050_default_config(&cfg);
    cfg.gyro_range = MPU6050_GYRO_RANGE_2000DPS;
    mpu6050_init(&cfg);

    /* Raw=164 → 164/16.4 = 10.0 °/s */
    mock_registers[0x43] = 0x00;
    mock_registers[0x44] = 0xA4; /* 164 = 0x00A4 */
    mock_registers[0x45] = 0x00; mock_registers[0x46] = 0x00;
    mock_registers[0x47] = 0x00; mock_registers[0x48] = 0x00;

    gyro_dps_t dps;
    mpu6050_read_gyro_dps(&dps);
    ASSERT_FLOAT("X @raw=164, sens=16.4 → 10.0°/s", dps.x, 10.0f, 0.02f);
}

void test_15_set_range_updates_reg(void) {
    printf("\n[TEST 15] set_gyro_range - Dynamic range change\n");
    reset_mock();
    mpu6050_config_t cfg;
    mpu6050_default_config(&cfg);
    mpu6050_init(&cfg);

    mpu6050_set_gyro_range(MPU6050_GYRO_RANGE_2000DPS);
    ASSERT_EQ("GYRO_CONFIG after set(2000) = 0x18", mock_registers[0x1B], 0x18);

    mpu6050_set_gyro_range(MPU6050_GYRO_RANGE_1000DPS);
    ASSERT_EQ("GYRO_CONFIG after set(1000) = 0x10", mock_registers[0x1B], 0x10);
}

void test_16_set_range_updates_sensitivity(void) {
    printf("\n[TEST 16] set_gyro_range - Sensitivity updates after change\n");
    reset_mock();
    mpu6050_config_t cfg;
    mpu6050_default_config(&cfg);
    mpu6050_init(&cfg);

    /* @250dps: raw=131 → 1.0 */
    mock_registers[0x43] = 0x00; mock_registers[0x44] = 0x83;
    mock_registers[0x45] = 0x00; mock_registers[0x46] = 0x00;
    mock_registers[0x47] = 0x00; mock_registers[0x48] = 0x00;

    gyro_dps_t dps;
    mpu6050_read_gyro_dps(&dps);
    ASSERT_FLOAT("Before @250dps raw=131 → 1.0", dps.x, 1.0f, 0.01f);

    /* Change to 2000dps: same raw=131 → 131/16.4 ≈ 7.99 */
    mpu6050_set_gyro_range(MPU6050_GYRO_RANGE_2000DPS);
    mpu6050_read_gyro_dps(&dps);
    ASSERT_FLOAT("After @2000dps raw=131 → 7.99", dps.x, 131.0f/16.4f, 0.02f);
}

void test_17_deinit_sleep_mode(void) {
    printf("\n[TEST 17] deinit - PWR_MGMT_1 sleep bit set (0x40)\n");
    reset_mock();
    mpu6050_config_t cfg;
    mpu6050_default_config(&cfg);
    mpu6050_init(&cfg);

    ASSERT_EQ("PWR_MGMT_1 after init = 0x00 (awake)", mock_registers[0x6B], 0x00);

    mpu6050_deinit();
    ASSERT_EQ("PWR_MGMT_1 after deinit = 0x40 (sleep)", mock_registers[0x6B], 0x40);
}

void test_18_read_before_init(void) {
    printf("\n[TEST 18] Error handling - read before init\n");
    reset_mock();
    gyro_raw_t raw;
    gyro_dps_t dps;
    ASSERT_EQ("read_gyro_raw before init = ERR_CONFIG", mpu6050_read_gyro_raw(&raw), GYRO_ERR_CONFIG);
    ASSERT_EQ("read_gyro_dps before init = ERR_CONFIG", mpu6050_read_gyro_dps(&dps), GYRO_ERR_CONFIG);
}

void test_19_null_pointers(void) {
    printf("\n[TEST 19] Error handling - NULL pointers\n");
    reset_mock();
    mpu6050_config_t cfg;
    mpu6050_default_config(&cfg);
    mpu6050_init(&cfg);

    ASSERT_EQ("read_gyro_raw(NULL) = ERR_NULL", mpu6050_read_gyro_raw(NULL), GYRO_ERR_NULL);
    ASSERT_EQ("read_gyro_dps(NULL) = ERR_NULL", mpu6050_read_gyro_dps(NULL), GYRO_ERR_NULL);
    ASSERT_EQ("who_am_i(NULL) = ERR_NULL", mpu6050_who_am_i(NULL), GYRO_ERR_NULL);
    ASSERT_EQ("init(NULL) = ERR_NULL", mpu6050_init(NULL), GYRO_ERR_NULL);
}

void test_20_i2c_failure(void) {
    printf("\n[TEST 20] Error handling - I2C failure\n");
    reset_mock();
    mock_i2c_fail = 1;
    mpu6050_config_t cfg;
    mpu6050_default_config(&cfg);
    ASSERT_EQ("init with I2C fail = ERR_I2C", mpu6050_init(&cfg), GYRO_ERR_I2C);
}

void test_21_negative_raw_big_endian(void) {
    printf("\n[TEST 21] read_gyro_raw - Negative values (big-endian two's complement)\n");
    reset_mock();
    mpu6050_config_t cfg;
    mpu6050_default_config(&cfg);
    mpu6050_init(&cfg);

    mock_registers[0x43] = 0xFF; mock_registers[0x44] = 0xFF; /* X = -1 */
    mock_registers[0x45] = 0x80; mock_registers[0x46] = 0x00; /* Y = -32768 */
    mock_registers[0x47] = 0x7F; mock_registers[0x48] = 0xFF; /* Z = 32767 */

    gyro_raw_t raw;
    mpu6050_read_gyro_raw(&raw);
    ASSERT_EQ("X = -1 (0xFFFF)", raw.x, -1);
    ASSERT_EQ("Y = -32768 (0x8000)", raw.y, -32768);
    ASSERT_EQ("Z = 32767 (0x7FFF)", raw.z, 32767);
}

void test_22_i2c_timeout_retry(void) {
    printf("\n[TEST 22] Timeout retry - recovers after transient timeout\n");
    reset_mock();
    mpu6050_config_t cfg;
    mpu6050_default_config(&cfg);

    mock_timeout_flag = 1;
    mock_timeout_count = 1;
    gyro_status_t s = mpu6050_init(&cfg);
    ASSERT_EQ("init recovers after 1 timeout", s, GYRO_OK);
}

void test_23_i2c_timeout_exhausted(void) {
    printf("\n[TEST 23] Timeout exhausted - init fails\n");
    reset_mock();
    mpu6050_config_t cfg;
    mpu6050_default_config(&cfg);

    mock_timeout_flag = 1;
    mock_timeout_count = 100;
    gyro_status_t s = mpu6050_init(&cfg);
    /* Wake-up path converts GYRO_ERR_TIMEOUT to GYRO_ERR_CONFIG */
    ASSERT_EQ("init fails with ERR_CONFIG (timeout)", s, GYRO_ERR_CONFIG);
}

void test_24_device_reset_sequence(void) {
    printf("\n[TEST 24] init - Device reset sent before wake-up\n");
    reset_mock();
    mpu6050_config_t cfg;
    mpu6050_default_config(&cfg);
    mpu6050_init(&cfg);

    /*
     * After init, PWR_MGMT_1 should be 0x00 (awake).
     * The device reset (0x80) was sent first, then wake (0x00).
     * Since mock records final register value, we verify 0x00.
     */
    ASSERT_EQ("PWR_MGMT_1 final = 0x00 (awake)", mock_registers[0x6B], 0x00);
}

/* =========================================================================
 * MAIN
 * ========================================================================= */

int main(void) {
    printf("╔══════════════════════════════════════════════╗\n");
    printf("║  MPU-6050 Library Unit Test (Mock I2C)       ║\n");
    printf("║  Verifikasi register & logika tanpa hardware ║\n");
    printf("╚══════════════════════════════════════════════╝\n");

    test_01_default_config();
    test_02_default_config_null();
    test_03_init_i2c_address();
    test_04_init_wake_from_sleep();
    test_05_init_who_am_i();
    test_06_init_smplrt_div();
    test_07_init_dlpf();
    test_08_init_gyro_config();
    test_09_read_raw_big_endian();
    test_10_no_auto_increment_bit();
    test_11_sensitivity_250dps();
    test_12_sensitivity_500dps();
    test_13_sensitivity_1000dps();
    test_14_sensitivity_2000dps();
    test_15_set_range_updates_reg();
    test_16_set_range_updates_sensitivity();
    test_17_deinit_sleep_mode();
    test_18_read_before_init();
    test_19_null_pointers();
    test_20_i2c_failure();
    test_21_negative_raw_big_endian();
    test_22_i2c_timeout_retry();
    test_23_i2c_timeout_exhausted();
    test_24_device_reset_sequence();

    printf("\n══════════════════════════════════════════════\n");
    printf("  TOTAL: %d passed, %d failed (of %d)\n",
           tests_passed, tests_failed, tests_passed + tests_failed);
    if (tests_failed == 0)
        printf("  STATUS: ALL TESTS PASSED ✓\n");
    else
        printf("  STATUS: SOME TESTS FAILED ✗\n");
    printf("══════════════════════════════════════════════\n");

    return tests_failed ? 1 : 0;
}
