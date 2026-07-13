/*
 * Copyright (C) 2026 ICDeC
 *
 * Test Application: L3G4200D Gyroscope Sensor (RAW I2C, PULPissimo)
 *
 * Berbeda dari test_l3g4200d.c sebelumnya: file ini TIDAK memanggil
 * l3g4200d_init()/l3g4200d.c sama sekali. Semua transaksi I2C dilakukan
 * langsung lewat i2c_write()/i2c_read() dari pulp.h, dipecah jadi test
 * granular satu-per-satu -- mengikuti struktur test STM32 Nucleo yang
 * sudah terbukti berhasil (test_i2c_device_found, test_who_am_i,
 * test_ctrl_reg1_power_up, test_ctrl_reg4_set_range,
 * test_read_gyro_raw_data).
 *
 * Tujuannya: mengisolasi persis di titik mana transaksi gagal/hang,
 * tanpa tertutup oleh logika retry/multi-step di dalam l3g4200d_init().
 *
 * Wiring (sama seperti versi STM32):
 *   L3G4200D VCC  -> 3.3V
 *   L3G4200D GND  -> GND
 *   L3G4200D SCL  -> I2C1 SCL Nusacore
 *   L3G4200D SDA  -> I2C1 SDA Nusacore
 *   L3G4200D SDO  -> 3.3V  (SDO=1 -> alamat I2C 0x69)
 *   L3G4200D CS   -> 3.3V  (wajib HIGH supaya mode I2C aktif, bukan SPI)
 *
 * PENTING soal L3G4200D:
 *   1. Output data LITTLE-ENDIAN (byte Low duluan, baru High).
 *   2. Untuk multi-byte read (auto-increment), bit MSB (bit7) dari
 *      alamat register HARUS di-set manual (OR dengan 0x80).
 *
 * Usage:
 *   make all SENSOR=l3g4200d
 *   make run SENSOR=l3g4200d platform=fpga
 */

#include <stdio.h>
#include "pulp.h"

/* ---------------------------------------------------------------------
 * Alamat I2C dan definisi register
 * --------------------------------------------------------------------- */
#define L3G4200D_ADDR_7BIT     0x69   /* SDO = 1 */
#define L3G4200D_ADDR_8BIT     (L3G4200D_ADDR_7BIT << 1)  /* i2c_dev_t.cs pakai format 8-bit: 0xD2 */

#define REG_WHO_AM_I           0x0F   /* Reset value 0xD3 */
#define REG_CTRL_REG1          0x20   /* Power mode, ODR, axes enable */
#define REG_CTRL_REG4          0x23   /* FS_SEL (full scale) */
#define REG_OUT_X_L            0x28   /* OUT_X_L..OUT_Z_H berurutan (6 byte) */

#define AUTO_INCREMENT_BIT     0x80   /* wajib di-OR-kan ke alamat register saat baca >1 byte */

/* FS_SEL = 00 -> range +/-250 dps, sensitivitas 8.75 mdps/digit.
 * Dinyatakan sebagai pecahan integer 875/100 karena RISC-V PULPissimo
 * TIDAK memiliki FPU -- float menghasilkan hasil SALAH.
 * Rumus: dps_x100 = raw * 875 / 1000  (setara raw * 8.75 / 10) */
#define GYRO_SENS_NUM   875   /* numerator   (8.75 * 100) */
#define GYRO_SENS_DENOM 1000  /* denominator (100 * 10)   */

/* Software timeout untuk tiap transaksi I2C (microseconds).
 * Ini memakai mekanisme timeout BAWAAN i2c.c (bukan guard timer terpisah),
 * supaya kalau device tidak ACK, program tidak hang selamanya. */
#define I2C_XFER_TIMEOUT_US    5000

/* ---------------------------------------------------------------------
 * State I2C global untuk test ini
 * --------------------------------------------------------------------- */
static i2c_t     *g_i2c;
static i2c_dev_t  g_i2c_dev;

/* ---------------------------------------------------------------------
 * Helper I2C low-level (raw, tanpa lewat l3g4200d.c)
 * --------------------------------------------------------------------- */

/**
 * @brief Tulis 1 byte value ke 1 register.
 * @return 0 sukses, selain itu kode error dari i2c_write() (5 = timeout).
 */
static int l3g_write_reg(uint8_t reg, uint8_t value)
{
    unsigned char data[2];
    data[0] = reg;
    data[1] = value;
    return i2c_write(g_i2c, data, 2, 1); /* send_stop = 1 */
}

/**
 * @brief Baca 1 byte dari 1 register (single-byte read, tanpa auto-increment).
 * @return 0 sukses (nilai ada di *out), -1 gagal.
 */
static int l3g_read_reg(uint8_t reg, uint8_t *out)
{
    unsigned char r = reg;

    /* Kirim alamat register dulu, TANPA stop (repeated start) */
    int ret = i2c_write(g_i2c, &r, 1, 0);
    if (ret != 0) {
        return -1; /* gagal di fase write alamat register */
    }

    /* EKSPERIMEN: beri jeda kecil sebelum fase read, siapa tahu
     * L3G4200D butuh waktu antara write-alamat dan repeated-start-read.
     * Kalau ini yang jadi solusi, delay ini bisa dipertahankan atau
     * dituning lebih presisi. */
    for (volatile int d = 0; d < 200; d++);

    int n = i2c_read(g_i2c, out, 1, 0);
    if (n != 1) {
        return -2; /* gagal di fase baca data */
    }

    return 0;
}

/**
 * @brief Varian ALTERNATIF: pakai STOP + START terpisah (bukan repeated
 * start murni), untuk uji apakah L3G4200D lebih suka pola ini -- mirip
 * temuan kita di STM32 soal masalah repeated-start di clock tinggi.
 */
static int l3g_read_reg_stopstart(uint8_t reg, uint8_t *out)
{
    unsigned char r = reg;

    /* Kirim alamat register, KALI INI dengan STOP di akhir */
    int ret = i2c_write(g_i2c, &r, 1, 1); /* send_stop = 1 */
    if (ret != 0) {
        return -1;
    }

    for (volatile int d = 0; d < 200; d++);

    int n = i2c_read(g_i2c, out, 1, 0);
    if (n != 1) {
        return -2;
    }

    return 0;
}

/**
 * @brief Baca banyak byte sekaligus dengan auto-increment (WAJIB set bit 0x80).
 * @return jumlah byte yang berhasil dibaca, -1 kalau fase write alamat gagal.
 */
static int l3g_read_bytes_auto_increment(uint8_t startReg, uint8_t *buf, int len)
{
    unsigned char r = startReg | AUTO_INCREMENT_BIT;

    int ret = i2c_write(g_i2c, &r, 1, 0);
    if (ret != 0) {
        return -1;
    }

    return i2c_read(g_i2c, buf, len, 0); /* return = jumlah byte terbaca */
}

/* ---------------------------------------------------------------------
 * Test 0: Buka I2C bus (setup, bukan transaksi ke sensor)
 * --------------------------------------------------------------------- */
static int test_i2c_open(void)
{
    printf("[TEST 0] Membuka I2C bus...\n");

    i2c_dev_init(&g_i2c_dev);
    g_i2c_dev.id           = 0; /* I2C peripheral ID 0 (sesuai GYRO_DEFAULT_I2C_ID di gyro_common.h) */
    g_i2c_dev.cs           = L3G4200D_ADDR_8BIT;
    g_i2c_dev.max_baudrate = 100000;

    printf("  I2C Port : %d\n", g_i2c_dev.id);
    printf("  Address  : 0x%02X (7-bit: 0x%02X)\n", (uint8_t)g_i2c_dev.cs, L3G4200D_ADDR_7BIT);
    printf("  Baudrate : %d\n", g_i2c_dev.max_baudrate);

    g_i2c = i2c_open(&g_i2c_dev);
    if (g_i2c == NULL) {
        printf("  FAIL: i2c_open() mengembalikan NULL\n");
        return 0;
    }

    /* Aktifkan software timeout bawaan i2c.c supaya transaksi gagal
     * tidak hang selamanya. reset_on_timeout = 1 supaya UDMA channel
     * di-reset otomatis kalau timeout kejadian. */
    i2c_settimeout(I2C_XFER_TIMEOUT_US, 1);

    printf("  PASS: I2C bus terbuka, timeout diset %d us\n", I2C_XFER_TIMEOUT_US);
    return 1;
}

/* ---------------------------------------------------------------------
 * Test 1: Device merespon di alamat 0x69 (ACK check sederhana)
 * --------------------------------------------------------------------- */
static int test_i2c_device_found(void)
{
    printf("[TEST 1] Cek device merespon di alamat 0x%02X...\n", L3G4200D_ADDR_7BIT);

    unsigned char probe = REG_WHO_AM_I;
    int ret = i2c_write(g_i2c, &probe, 1, 1); /* write 1 byte, langsung stop */

    if (ret == 0) {
        printf("  PASS: Device ACK di alamat 0x%02X\n", L3G4200D_ADDR_7BIT);
        return 1;
    } else {
        printf("  FAIL: Tidak ada ACK (ret=%d, timeout_flag=%d)\n",
               ret, i2c_managetimeoutflag(true));
        printf("  CHECK: wiring SDA/SCL, pull-up, CS pin (harus HIGH), SDO pin\n");
        return 0;
    }
}

/* ---------------------------------------------------------------------
 * Test 2: WHO_AM_I harus bernilai 0xD3
 * --------------------------------------------------------------------- */
static int test_who_am_i(void)
{
    printf("[TEST 2] Membaca register WHO_AM_I (0x%02X)...\n", REG_WHO_AM_I);

    /* Percobaan A: repeated-start + delay kecil */
    uint8_t val_a = 0;
    int ret_a = l3g_read_reg(REG_WHO_AM_I, &val_a);
    printf("  [A] repeated-start+delay: ret=%d, val=0x%02X\n", ret_a, val_a);

    /* Percobaan B: STOP lalu START terpisah */
    uint8_t val_b = 0;
    int ret_b = l3g_read_reg_stopstart(REG_WHO_AM_I, &val_b);
    printf("  [B] stop-then-start     : ret=%d, val=0x%02X\n", ret_b, val_b);

    if (ret_a == 0 && val_a == 0xD3) {
        printf("  PASS (via metode A - repeated-start+delay)\n");
        return 1;
    } else if (ret_b == 0 && val_b == 0xD3) {
        printf("  PASS (via metode B - stop-then-start)\n");
        return 1;
    } else {
        printf("  FAIL: kedua metode gagal (expected 0xD3), timeout_flag=%d\n",
               i2c_managetimeoutflag(true));
        return 0;
    }
}

/* ---------------------------------------------------------------------
 * Test 3: Power up + enable axis X,Y,Z lewat CTRL_REG1
 *   0x0F -> ODR=100Hz, PD=1 (normal mode), Zen=Yen=Xen=1
 * --------------------------------------------------------------------- */
static int test_ctrl_reg1_power_up(void)
{
    printf("[TEST 3] Menulis CTRL_REG1 = 0x0F (power up + enable XYZ)...\n");

    int wret = l3g_write_reg(REG_CTRL_REG1, 0x0F);
    if (wret != 0) {
        printf("  FAIL: Gagal menulis CTRL_REG1 (ret=%d, timeout_flag=%d)\n",
               wret, i2c_managetimeoutflag(true));
        return 0;
    }

    /* delay singkat untuk sensor stabil setelah power up */
    for (volatile int i = 0; i < 50000; i++);

    uint8_t val = 0;
    int rret = l3g_read_reg(REG_CTRL_REG1, &val);
    if (rret == 0 && val == 0x0F) {
        printf("  PASS: CTRL_REG1 readback = 0x%02X\n", val);
        return 1;
    } else {
        printf("  FAIL: readback ret=%d, val=0x%02X (expected 0x0F)\n", rret, val);
        return 0;
    }
}

/* ---------------------------------------------------------------------
 * Test 4: Konfigurasi CTRL_REG4 (FS_SEL = 00 -> +/-250 dps)
 * --------------------------------------------------------------------- */
static int test_ctrl_reg4_set_range(void)
{
    printf("[TEST 4] Menulis CTRL_REG4 = 0x00 (FS_SEL = +/-250 dps)...\n");

    int wret = l3g_write_reg(REG_CTRL_REG4, 0x00);
    if (wret != 0) {
        printf("  FAIL: Gagal menulis CTRL_REG4 (ret=%d, timeout_flag=%d)\n",
               wret, i2c_managetimeoutflag(true));
        return 0;
    }

    for (volatile int i = 0; i < 50000; i++);

    uint8_t val = 0xFF;
    int rret = l3g_read_reg(REG_CTRL_REG4, &val);
    if (rret == 0 && val == 0x00) {
        printf("  PASS: CTRL_REG4 readback = 0x%02X\n", val);
        return 1;
    } else {
        printf("  FAIL: readback ret=%d, val=0x%02X (expected 0x00)\n", rret, val);
        return 0;
    }
}

/* ---------------------------------------------------------------------
 * Test 5: Baca data gyro mentah (X, Y, Z) - LITTLE-ENDIAN
 * --------------------------------------------------------------------- */
static int test_read_gyro_raw_data(void)
{
    printf("[TEST 5] Membaca 6 byte data gyro (OUT_X_L, auto-increment)...\n");

    uint8_t buf[6] = {0};
    int received = l3g_read_bytes_auto_increment(REG_OUT_X_L, buf, 6);

    if (received != 6) {
        printf("  FAIL: Byte diterima = %d (harus 6), timeout_flag=%d\n",
               received, i2c_managetimeoutflag(true));
        return 0;
    }

    /* Little-endian: byte Low duluan, byte High kedua */
    int16_t gx_raw = (int16_t)((buf[1] << 8) | buf[0]);
    int16_t gy_raw = (int16_t)((buf[3] << 8) | buf[2]);
    int16_t gz_raw = (int16_t)((buf[5] << 8) | buf[4]);

    /* Konversi ke dps (dikali 100 supaya tetap integer, tanpa float).
     * dps_x100 = raw * 875 / 1000.
     * Contoh: raw=1000 -> 1000*875/1000 = 875 -> artinya 8.75 dps.
     * Print: dps_x100/100 . abs(dps_x100%100) -> "8.75" */
    int gx_dps_x100 = (int)((int32_t)gx_raw * GYRO_SENS_NUM / GYRO_SENS_DENOM);
    int gy_dps_x100 = (int)((int32_t)gy_raw * GYRO_SENS_NUM / GYRO_SENS_DENOM);
    int gz_dps_x100 = (int)((int32_t)gz_raw * GYRO_SENS_NUM / GYRO_SENS_DENOM);

    printf("  Raw: X=%d Y=%d Z=%d\n", gx_raw, gy_raw, gz_raw);
    printf("  dps (x100, integer): X=%d Y=%d Z=%d\n",
           gx_dps_x100, gy_dps_x100, gz_dps_x100);

    /* Cetak juga dalam format desimal "X.YY dps" supaya lebih mudah dibaca.
     * Trick: pakai abs() manual karena stdlib abs() mungkin tidak tersedia. */
    {
        int vals[3]       = { gx_dps_x100, gy_dps_x100, gz_dps_x100 };
        const char *lbl[] = { "X", "Y", "Z" };
        int i;
        printf("  dps (desimal):");
        for (i = 0; i < 3; i++) {
            int v    = vals[i];
            int sign = (v < 0) ? -1 : 1;
            int av   = v * sign;              /* abs value */
            int whole = av / 100;
            int frac  = av % 100;
            printf(" %s=%s%d.%02d", lbl[i], (sign < 0) ? "-" : "", whole, frac);
        }
        printf(" dps\n");
    }

    int all_zero = (gx_raw == 0 && gy_raw == 0 && gz_raw == 0);
    int all_ff   = (gx_raw == -1 && gy_raw == -1 && gz_raw == -1);

    if (all_zero || all_ff) {
        printf("  FAIL: Data stuck di 0x0000/0xFFFF - kemungkinan I2C gagal\n");
        return 0;
    }

    printf("  PASS: Data gyro terbaca dengan wajar\n");
    return 1;
}

/* ---------------------------------------------------------------------
 * Pembacaan Kontinyu: baca gyro berulang-ulang dalam loop
 * --------------------------------------------------------------------- */

/* Jumlah sampel yang dibaca sebelum berhenti.
 * Ganti ke 0 untuk loop tanpa henti (infinite). */
#define CONTINUOUS_NUM_SAMPLES  100

/* Delay antar pembacaan (loop counter, bukan microseconds).
 * Tuning: 200000 ~ 200ms @50MHz PULPissimo. */
#define CONTINUOUS_DELAY_LOOPS   200000

static void continuous_gyro_read(void)
{
    printf("\n========================================\n");
    printf(" CONTINUOUS GYRO READ\n");
    if (CONTINUOUS_NUM_SAMPLES > 0)
        printf(" Jumlah sampel: %d\n", CONTINUOUS_NUM_SAMPLES);
    else
        printf(" Mode: infinite (reset board untuk stop)\n");
    printf("========================================\n\n");

    printf("  #    |  Raw X   Raw Y   Raw Z  |  dps X    dps Y    dps Z\n");
    printf("-------+------------------------+---------------------------\n");

    int sample = 0;

    while (1) {
        uint8_t buf[6] = {0};
        int received = l3g_read_bytes_auto_increment(REG_OUT_X_L, buf, 6);

        if (received != 6) {
            printf("  [!] Gagal baca data (received=%d, timeout_flag=%d)\n",
                   received, i2c_managetimeoutflag(true));

            /* Coba lanjut setelah delay, jangan langsung berhenti */
            for (volatile int d = 0; d < CONTINUOUS_DELAY_LOOPS; d++);
            continue;
        }

        /* Little-endian: Low byte duluan */
        int16_t gx_raw = (int16_t)((buf[1] << 8) | buf[0]);
        int16_t gy_raw = (int16_t)((buf[3] << 8) | buf[2]);
        int16_t gz_raw = (int16_t)((buf[5] << 8) | buf[4]);

        /* Konversi ke dps*100 (integer only) */
        int gx_dps = (int)((int32_t)gx_raw * GYRO_SENS_NUM / GYRO_SENS_DENOM);
        int gy_dps = (int)((int32_t)gy_raw * GYRO_SENS_NUM / GYRO_SENS_DENOM);
        int gz_dps = (int)((int32_t)gz_raw * GYRO_SENS_NUM / GYRO_SENS_DENOM);

        /* Helper: cetak nilai desimal X.YY dari dps_x100 */
        #define PRINT_DPS(v) do { \
            int _s = ((v) < 0) ? -1 : 1; \
            int _a = (v) * _s; \
            printf("%s%d.%02d", (_s < 0) ? "-" : " ", _a / 100, _a % 100); \
        } while(0)

        printf(" %4d  | %6d  %6d  %6d  | ",
               sample, gx_raw, gy_raw, gz_raw);
        PRINT_DPS(gx_dps); printf("   ");
        PRINT_DPS(gy_dps); printf("   ");
        PRINT_DPS(gz_dps); printf(" dps\n");

        #undef PRINT_DPS

        sample++;

        /* Berhenti jika sudah mencapai jumlah sampel (0 = infinite) */
        if (CONTINUOUS_NUM_SAMPLES > 0 && sample >= CONTINUOUS_NUM_SAMPLES)
            break;

        /* Delay antar pembacaan */
        for (volatile int d = 0; d < CONTINUOUS_DELAY_LOOPS; d++);
    }

    printf("\n  Selesai: %d sampel terbaca.\n", sample);
}

/* ---------------------------------------------------------------------
 * Runner
 * --------------------------------------------------------------------- */
int main()
{
    int pass_count = 0;
    int fail_count = 0;

    printf("========================================\n");
    printf(" L3G4200D Raw I2C Test (tanpa l3g4200d.c)\n");
    printf(" ICDeC PULPissimo FPGA Board\n");
    printf("========================================\n\n");

    if (!test_i2c_open()) {
        printf("\nSTOP: I2C bus gagal dibuka, test dihentikan.\n");
        return -1;
    }
    pass_count++;
    printf("\n");

    /* Setiap test dijalankan berurutan, TAPI tetap lanjut walau ada yang
     * gagal (kecuali test_i2c_open di atas), supaya kita bisa lihat
     * gambaran lengkap: mana yang lolos, mana yang tidak. */

    if (test_i2c_device_found()) pass_count++; else fail_count++;
    printf("\n");

    if (test_who_am_i()) pass_count++; else fail_count++;
    printf("\n");

    if (test_ctrl_reg1_power_up()) pass_count++; else fail_count++;
    printf("\n");

    if (test_ctrl_reg4_set_range()) pass_count++; else fail_count++;
    printf("\n");

    if (test_read_gyro_raw_data()) pass_count++; else fail_count++;
    printf("\n");

    printf("========================================\n");
    printf(" RESULTS: %d PASSED, %d FAILED\n", pass_count, fail_count);
    printf("========================================\n");

    /* Kalau semua test lulus, masuk ke mode pembacaan kontinyu */
    if (fail_count == 0) {
        continuous_gyro_read();
    } else {
        printf("\n  [!] Ada test yang gagal, pembacaan kontinyu dilewati.\n");
    }

    i2c_close(g_i2c);

    return (fail_count == 0) ? 0 : -1;
}

void pe_start(void)
{
}