/*
 * Unit Test: L3G4200D Library - Mock I2C
 *
 * Verifikasi logika library L3G4200D TANPA hardware.
 * Mock I2C mensimulasikan respons sensor untuk setiap register.
 *
 * Build:  gcc -o unit_test_l3g4200d unit_test_l3g4200d.c -lm -I.. -Imock
 * Run:    ./unit_test_l3g4200d
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "mock/pulp.h"

/* =========================================================================
 * MOCK I2C LAYER - Simulates pulp-runtime I2C driver
 * ========================================================================= */

/* Simulated register file (256 registers) */
static uint8_t mock_registers[256];

/* Tracking what the library sends */
static uint8_t  last_write_reg;
static uint8_t  last_write_val;
static int      last_write_stop;
static uint8_t  last_read_addr;   /* register addr sent before read */
static int      write_call_count;
static int      read_call_count;
static int      mock_i2c_fail;    /* Set to 1 to simulate I2C failure */

/* Track I2C device config */
static int      mock_dev_cs;
static int      mock_dev_id;
static unsigned int mock_dev_baudrate;

/* Types come from mock/pulp.h (compile with -Imock) */
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
bool i2c_managetimeoutflag(bool c) { (void)c; return false; }

int i2c_write(i2c_t *dev, unsigned char *data, int length, int send_stop) {
    (void)dev;
    write_call_count++;
    if (mock_i2c_fail) return 5; /* timeout */

    if (length == 1 && !send_stop) {
        /* Register address write (before read) */
        last_read_addr = data[0];
    } else if (length == 2 && send_stop) {
        /* Register write: data[0]=reg, data[1]=value */
        last_write_reg = data[0];
        last_write_val = data[1];
        last_write_stop = send_stop;
        mock_registers[data[0]] = data[1];
    }
    return 0;
}

int i2c_read(i2c_t *dev, unsigned char *rx_buff, int length, int pending) {
    (void)dev; (void)pending;
    read_call_count++;
    if (mock_i2c_fail) return 0;

    /* Strip auto-increment bit to get actual register address */
    uint8_t base_reg = last_read_addr & 0x7F;
    for (int i = 0; i < length; i++) {
        rx_buff[i] = mock_registers[base_reg + i];
    }
    return length;
}

/* Stub printf for pulp.h compatibility */
/* (we use real printf, no stub needed on host) */

/* =========================================================================
 * Include the ACTUAL library source (compiled together)
 * ========================================================================= */

/* Include library headers and source (mock/pulp.h is resolved via -Imock) */
#include "../gyro_common.h"
#include "../l3g4200d.h"
#include "../l3g4200d.c"

/* =========================================================================
 * TEST FRAMEWORK
 * ========================================================================= */

static int tests_passed = 0;
static int tests_failed = 0;

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
    last_write_reg = 0;
    last_write_val = 0;
    /* Set WHO_AM_I to correct value */
    mock_registers[0x0F] = 0xD3;
    /* Reset driver state */
    l3g4200d_state.initialized = 0;
    l3g4200d_state.i2c = NULL;
}

/* =========================================================================
 * TEST CASES
 * ========================================================================= */

void test_01_default_config(void) {
    printf("\n[TEST 01] default_config - Nilai default konfigurasi\n");
    l3g4200d_config_t cfg;
    gyro_status_t s = l3g4200d_default_config(&cfg);

    ASSERT_EQ("return GYRO_OK", s, GYRO_OK);
    ASSERT_EQ("i2c_addr = 0x68", cfg.i2c_addr, 0x68);
    ASSERT_EQ("i2c_id = 0", cfg.i2c_id, 0);
    ASSERT_EQ("i2c_freq = 100000", cfg.i2c_freq, 100000);
    ASSERT_EQ("range = 250DPS (0x00)", cfg.range, L3G4200D_RANGE_250DPS);
}

void test_02_default_config_null(void) {
    printf("\n[TEST 02] default_config(NULL) - Error handling\n");
    gyro_status_t s = l3g4200d_default_config(NULL);
    ASSERT_EQ("return GYRO_ERR_NULL", s, GYRO_ERR_NULL);
}

void test_03_init_i2c_address(void) {
    printf("\n[TEST 03] init - I2C address 7-bit → 8-bit conversion\n");
    reset_mock();
    l3g4200d_config_t cfg;
    l3g4200d_default_config(&cfg);
    l3g4200d_init(&cfg);

    /*
     * 7-bit addr 0x68 shifted left = 0xD0 (208).
     * But cs is signed char, so 0xD0 wraps to -48.
     * The bit pattern is the same — verify with unsigned cast.
     */
    ASSERT_EQ("cs = 0x68 << 1 = 0xD0 (as byte)", (uint8_t)mock_dev_cs, 0xD0);
    ASSERT_EQ("i2c_id = 0", mock_dev_id, 0);
}

void test_04_init_who_am_i(void) {
    printf("\n[TEST 04] init - WHO_AM_I verification (reg 0x0F = 0xD3)\n");
    reset_mock();
    l3g4200d_config_t cfg;
    l3g4200d_default_config(&cfg);

    /* Correct WHO_AM_I */
    mock_registers[0x0F] = 0xD3;
    gyro_status_t s = l3g4200d_init(&cfg);
    ASSERT_EQ("init returns GYRO_OK with correct WHO_AM_I", s, GYRO_OK);

    /* Wrong WHO_AM_I */
    reset_mock();
    mock_registers[0x0F] = 0xAA;
    s = l3g4200d_init(&cfg);
    ASSERT_EQ("init returns GYRO_ERR_ID with wrong WHO_AM_I", s, GYRO_ERR_ID);
}

void test_05_init_ctrl_reg1(void) {
    printf("\n[TEST 05] init - CTRL_REG1 (0x20) power on + axis enable\n");
    reset_mock();
    l3g4200d_config_t cfg;
    l3g4200d_default_config(&cfg);
    l3g4200d_init(&cfg);

    /*
     * Expected CTRL_REG1 = 0x0F:
     * Bit 3 (PD)  = 1 → Normal mode
     * Bit 2 (Zen) = 1 → Z enabled
     * Bit 1 (Yen) = 1 → Y enabled
     * Bit 0 (Xen) = 1 → X enabled
     */
    ASSERT_EQ("CTRL_REG1 (0x20) = 0x0F", mock_registers[0x20], 0x0F);
    printf("    Bit breakdown: PD=%d Zen=%d Yen=%d Xen=%d\n",
           (mock_registers[0x20] >> 3) & 1,
           (mock_registers[0x20] >> 2) & 1,
           (mock_registers[0x20] >> 1) & 1,
           (mock_registers[0x20] >> 0) & 1);
}

void test_06_init_ctrl_reg4(void) {
    printf("\n[TEST 06] init - CTRL_REG4 (0x23) BDU + full-scale range\n");
    reset_mock();
    l3g4200d_config_t cfg;
    l3g4200d_default_config(&cfg);

    /* Test range 250 DPS */
    cfg.range = L3G4200D_RANGE_250DPS;
    l3g4200d_init(&cfg);
    /* Expected: BDU=1 (0x80) | range(0x00) = 0x80 */
    ASSERT_EQ("CTRL_REG4 @250dps = 0x80", mock_registers[0x23], 0x80);

    /* Test range 500 DPS */
    reset_mock();
    cfg.range = L3G4200D_RANGE_500DPS;
    l3g4200d_init(&cfg);
    /* Expected: 0x80 | 0x10 = 0x90 */
    ASSERT_EQ("CTRL_REG4 @500dps = 0x90", mock_registers[0x23], 0x90);

    /* Test range 2000 DPS */
    reset_mock();
    cfg.range = L3G4200D_RANGE_2000DPS;
    l3g4200d_init(&cfg);
    /* Expected: 0x80 | 0x20 = 0xA0 */
    ASSERT_EQ("CTRL_REG4 @2000dps = 0xA0", mock_registers[0x23], 0xA0);
}

void test_07_read_raw_data(void) {
    printf("\n[TEST 07] read_raw - Little-endian data assembly\n");
    reset_mock();
    l3g4200d_config_t cfg;
    l3g4200d_default_config(&cfg);
    l3g4200d_init(&cfg);

    /*
     * Simulasi data sensor (little-endian: low byte first):
     * X = 0x0100 = 256,  Y = 0xFF00 = -256 (signed), Z = 0x0001 = 1
     */
    mock_registers[0x28] = 0x00; /* OUT_X_L */
    mock_registers[0x29] = 0x01; /* OUT_X_H → X = 0x0100 = 256 */
    mock_registers[0x2A] = 0x00; /* OUT_Y_L */
    mock_registers[0x2B] = 0xFF; /* OUT_Y_H → Y = 0xFF00 = -256 */
    mock_registers[0x2C] = 0x01; /* OUT_Z_L */
    mock_registers[0x2D] = 0x00; /* OUT_Z_H → Z = 0x0001 = 1 */

    gyro_raw_t raw;
    gyro_status_t s = l3g4200d_read_raw(&raw);

    ASSERT_EQ("return GYRO_OK", s, GYRO_OK);
    ASSERT_EQ("raw.x = 256", raw.x, 256);
    ASSERT_EQ("raw.y = -256", raw.y, -256);
    ASSERT_EQ("raw.z = 1", raw.z, 1);
}

void test_08_auto_increment_bit(void) {
    printf("\n[TEST 08] read_raw - Auto-increment bit (MSB=1 for multi-byte)\n");
    reset_mock();
    l3g4200d_config_t cfg;
    l3g4200d_default_config(&cfg);
    l3g4200d_init(&cfg);

    gyro_raw_t raw;
    l3g4200d_read_raw(&raw);

    /*
     * When reading 6 bytes from OUT_X_L (0x28),
     * the register address sent should be 0x28 | 0x80 = 0xA8
     */
    ASSERT_EQ("multi-byte read addr = 0x28|0x80 = 0xA8", last_read_addr, 0xA8);
}

void test_09_single_byte_no_autoincr(void) {
    printf("\n[TEST 09] who_am_i - Single byte read (no auto-increment)\n");
    reset_mock();
    l3g4200d_config_t cfg;
    l3g4200d_default_config(&cfg);
    l3g4200d_init(&cfg);

    uint8_t id;
    l3g4200d_who_am_i(&id);

    /* Single byte read should NOT set auto-increment bit */
    ASSERT_EQ("single-byte read addr = 0x0F (no MSB)", last_read_addr, 0x0F);
    ASSERT_EQ("WHO_AM_I value = 0xD3", id, 0xD3);
}

void test_10_sensitivity_250dps(void) {
    printf("\n[TEST 10] read_dps - Sensitivity @250dps (8.75 mdps/digit)\n");
    reset_mock();
    l3g4200d_config_t cfg;
    l3g4200d_default_config(&cfg);
    cfg.range = L3G4200D_RANGE_250DPS;
    l3g4200d_init(&cfg);

    /* Raw value 1000 → 1000 * 8.75 / 1000 = 8.75 °/s */
    mock_registers[0x28] = 0xE8; /* 1000 = 0x03E8, low byte */
    mock_registers[0x29] = 0x03; /* high byte */
    mock_registers[0x2A] = 0x00;
    mock_registers[0x2B] = 0x00;
    mock_registers[0x2C] = 0x00;
    mock_registers[0x2D] = 0x00;

    gyro_dps_t dps;
    l3g4200d_read_dps(&dps);

    ASSERT_FLOAT("X @raw=1000, sens=8.75mdps → 8.75°/s", dps.x, 8.75f, 0.01f);
    ASSERT_FLOAT("Y @raw=0 → 0.00°/s", dps.y, 0.0f, 0.01f);
}

void test_11_sensitivity_500dps(void) {
    printf("\n[TEST 11] read_dps - Sensitivity @500dps (17.50 mdps/digit)\n");
    reset_mock();
    l3g4200d_config_t cfg;
    l3g4200d_default_config(&cfg);
    cfg.range = L3G4200D_RANGE_500DPS;
    l3g4200d_init(&cfg);

    /* Raw value 1000 → 1000 * 17.50 / 1000 = 17.50 °/s */
    mock_registers[0x28] = 0xE8;
    mock_registers[0x29] = 0x03;
    mock_registers[0x2A] = 0x00;
    mock_registers[0x2B] = 0x00;
    mock_registers[0x2C] = 0x00;
    mock_registers[0x2D] = 0x00;

    gyro_dps_t dps;
    l3g4200d_read_dps(&dps);

    ASSERT_FLOAT("X @raw=1000, sens=17.50mdps → 17.50°/s", dps.x, 17.50f, 0.01f);
}

void test_12_sensitivity_2000dps(void) {
    printf("\n[TEST 12] read_dps - Sensitivity @2000dps (70.0 mdps/digit)\n");
    reset_mock();
    l3g4200d_config_t cfg;
    l3g4200d_default_config(&cfg);
    cfg.range = L3G4200D_RANGE_2000DPS;
    l3g4200d_init(&cfg);

    /* Raw value 1000 → 1000 * 70.0 / 1000 = 70.0 °/s */
    mock_registers[0x28] = 0xE8;
    mock_registers[0x29] = 0x03;
    mock_registers[0x2A] = 0x00;
    mock_registers[0x2B] = 0x00;
    mock_registers[0x2C] = 0x00;
    mock_registers[0x2D] = 0x00;

    gyro_dps_t dps;
    l3g4200d_read_dps(&dps);

    ASSERT_FLOAT("X @raw=1000, sens=70.0mdps → 70.0°/s", dps.x, 70.0f, 0.1f);
}

void test_13_set_range_changes_reg(void) {
    printf("\n[TEST 13] set_range - Dynamic range change updates CTRL_REG4\n");
    reset_mock();
    l3g4200d_config_t cfg;
    l3g4200d_default_config(&cfg);
    l3g4200d_init(&cfg);

    l3g4200d_set_range(L3G4200D_RANGE_2000DPS);
    ASSERT_EQ("CTRL_REG4 after set_range(2000) = 0xA0", mock_registers[0x23], 0xA0);

    l3g4200d_set_range(L3G4200D_RANGE_500DPS);
    ASSERT_EQ("CTRL_REG4 after set_range(500) = 0x90", mock_registers[0x23], 0x90);
}

void test_14_set_range_updates_sensitivity(void) {
    printf("\n[TEST 14] set_range - Sensitivity updates after range change\n");
    reset_mock();
    l3g4200d_config_t cfg;
    l3g4200d_default_config(&cfg);
    l3g4200d_init(&cfg);

    /* Start at 250dps, raw=1000 → 8.75 */
    mock_registers[0x28] = 0xE8;
    mock_registers[0x29] = 0x03;
    mock_registers[0x2A] = 0x00; mock_registers[0x2B] = 0x00;
    mock_registers[0x2C] = 0x00; mock_registers[0x2D] = 0x00;

    gyro_dps_t dps;
    l3g4200d_read_dps(&dps);
    ASSERT_FLOAT("Before: @250dps raw=1000 → 8.75", dps.x, 8.75f, 0.01f);

    /* Change to 2000dps, same raw → 70.0 */
    l3g4200d_set_range(L3G4200D_RANGE_2000DPS);
    l3g4200d_read_dps(&dps);
    ASSERT_FLOAT("After: @2000dps raw=1000 → 70.0", dps.x, 70.0f, 0.1f);
}

void test_15_deinit_power_down(void) {
    printf("\n[TEST 15] deinit - CTRL_REG1 set to 0x00 (power down)\n");
    reset_mock();
    l3g4200d_config_t cfg;
    l3g4200d_default_config(&cfg);
    l3g4200d_init(&cfg);

    ASSERT_EQ("CTRL_REG1 after init = 0x0F", mock_registers[0x20], 0x0F);

    l3g4200d_deinit();
    ASSERT_EQ("CTRL_REG1 after deinit = 0x00", mock_registers[0x20], 0x00);
}

void test_16_read_before_init(void) {
    printf("\n[TEST 16] Error handling - read before init\n");
    reset_mock();
    gyro_raw_t raw;
    gyro_dps_t dps;

    ASSERT_EQ("read_raw before init = ERR_CONFIG", l3g4200d_read_raw(&raw), GYRO_ERR_CONFIG);
    ASSERT_EQ("read_dps before init = ERR_CONFIG", l3g4200d_read_dps(&dps), GYRO_ERR_CONFIG);
}

void test_17_null_pointers(void) {
    printf("\n[TEST 17] Error handling - NULL pointers\n");
    reset_mock();
    l3g4200d_config_t cfg;
    l3g4200d_default_config(&cfg);
    l3g4200d_init(&cfg);

    ASSERT_EQ("read_raw(NULL) = ERR_NULL", l3g4200d_read_raw(NULL), GYRO_ERR_NULL);
    ASSERT_EQ("read_dps(NULL) = ERR_NULL", l3g4200d_read_dps(NULL), GYRO_ERR_NULL);
    ASSERT_EQ("who_am_i(NULL) = ERR_NULL", l3g4200d_who_am_i(NULL), GYRO_ERR_NULL);
    ASSERT_EQ("init(NULL) = ERR_NULL", l3g4200d_init(NULL), GYRO_ERR_NULL);
}

void test_18_i2c_failure(void) {
    printf("\n[TEST 18] Error handling - I2C failure\n");
    reset_mock();
    mock_i2c_fail = 1;

    l3g4200d_config_t cfg;
    l3g4200d_default_config(&cfg);
    ASSERT_EQ("init with I2C fail = ERR_I2C", l3g4200d_init(&cfg), GYRO_ERR_I2C);
}

void test_19_negative_raw_values(void) {
    printf("\n[TEST 19] read_raw - Negative values (two's complement)\n");
    reset_mock();
    l3g4200d_config_t cfg;
    l3g4200d_default_config(&cfg);
    l3g4200d_init(&cfg);

    /* -1 in 16-bit = 0xFFFF → low=0xFF, high=0xFF */
    mock_registers[0x28] = 0xFF; mock_registers[0x29] = 0xFF; /* X = -1 */
    mock_registers[0x2A] = 0x00; mock_registers[0x2B] = 0x80; /* Y = -32768 */
    mock_registers[0x2C] = 0xFF; mock_registers[0x2D] = 0x7F; /* Z = 32767 */

    gyro_raw_t raw;
    l3g4200d_read_raw(&raw);

    ASSERT_EQ("X = -1 (0xFFFF)", raw.x, -1);
    ASSERT_EQ("Y = -32768 (0x8000)", raw.y, -32768);
    ASSERT_EQ("Z = 32767 (0x7FFF)", raw.z, 32767);
}

/* =========================================================================
 * MAIN
 * ========================================================================= */

int main(void) {
    printf("╔══════════════════════════════════════════════╗\n");
    printf("║  L3G4200D Library Unit Test (Mock I2C)       ║\n");
    printf("║  Verifikasi register & logika tanpa hardware ║\n");
    printf("╚══════════════════════════════════════════════╝\n");

    test_01_default_config();
    test_02_default_config_null();
    test_03_init_i2c_address();
    test_04_init_who_am_i();
    test_05_init_ctrl_reg1();
    test_06_init_ctrl_reg4();
    test_07_read_raw_data();
    test_08_auto_increment_bit();
    test_09_single_byte_no_autoincr();
    test_10_sensitivity_250dps();
    test_11_sensitivity_500dps();
    test_12_sensitivity_2000dps();
    test_13_set_range_changes_reg();
    test_14_set_range_updates_sensitivity();
    test_15_deinit_power_down();
    test_16_read_before_init();
    test_17_null_pointers();
    test_18_i2c_failure();
    test_19_negative_raw_values();

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
