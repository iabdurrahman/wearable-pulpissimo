/*
 * test_lis3dhtr.c
 *
 * Integration test untuk FPGA/GVSoC (pulp-rt asli, BUKAN mock).
 * Monitor tegangan baterai secara DIFERENSIAL (B+ minus B-, terhadap
 * GND USB-/TP4056 yang sama) + input TP4056 lewat channel aux ADC
 * yang berbeda pada chip LIS3DHTR yang sama, di board ICDeC PULPissimo:
 *
 *   - ADC1: tegangan B+ baterai Li-ion 1S (3.0V - 4.2V), divider
 *           R1=94k (2x47k seri) + R2=47k -> rasio 47/141
 *   - ADC2: tegangan B- baterai (terhadap GND USB-/TP4056, BUKAN
 *           GND baterai) — muncul karena drop tegangan di MOSFET
 *           proteksi TP4056 (mis. DW01+FS8205) saat charge/discharge.
 *           Divider sama persis (47/141).
 *   - ADC3: tegangan input modul TP4056 (5.2V saat charging),
 *           divider sama persis (47/141) -> di pin ADC akan CLIP
 *           di ~1.6V karena melebihi rentang ADC. Ini SENGAJA
 *           dibiarkan clip karena tujuannya cuma deteksi
 *           charging/tidak (binary), bukan baca nilai presisi.
 *
 * Tegangan baterai ASLI = tegangan(B+) - tegangan(B-), lihat
 * lis3dhtr_read_battery_voltage_diff_avg() di lis3dhtr.h/.c.
 *
 * DEBUG: setiap siklus juga mencetak raw + tegangan MENTAH DI PIN
 * (sebelum divider) dari ADC2 (B-), utk verifikasi apakah pin sedang
 * clip di ujung rentang aux ADC (~884-1684mV) — lihat diskusi soal
 * jendela input ADC yang sempit di lis3dhtr.h.
 */

#include <stdio.h>
#include "pulp.h"
#include "lis3dhtr.h"

/* WAJIB ada — crt0.o pulp-runtime mereferensikan simbol pe_start()
 * sebagai entry point core cluster di vector table. */
void pe_start(void) {}

/* -------------------------------------------------------------------- */
/* Konstanta konfigurasi board                                          */
/* -------------------------------------------------------------------- */

#define BATTERY_BPLUS_ADC_CHANNEL   LIS3DHTR_ADC_CH1
#define BATTERY_BMINUS_ADC_CHANNEL  LIS3DHTR_ADC_CH2
#define TP4056_ADC_CHANNEL          LIS3DHTR_ADC_CH3

/* R1=94k (2x47k seri), R2=47k -> rasio = R2/(R1+R2) = 47/141.
 * Dipakai utk KETIGA titik ukur (B+, B-, TP4056), karena resistornya
 * sama persis di semua jalur. */
#define VDIVIDER_RATIO_BPLUS    (47.0f / 141.0f)
#define VDIVIDER_RATIO_BMINUS   (47.0f / 141.0f)
#define VDIVIDER_RATIO_TP4056   (47.0f / 141.0f)

/* Jumlah sample utk oversampling (kurangi noise), per titik ukur.
 * Lihat catatan di lis3dhtr.h soal keterbatasan tanpa API timer
 * portable. */
#define OVERSAMPLE_COUNT      12

/* Kurva discharge Li-ion 1S: mapping LINEAR sederhana (bukan kurva
 * asli yg non-linear) sesuai keputusan awal. */
#define BATTERY_VOLTAGE_EMPTY 3.0f
#define BATTERY_VOLTAGE_FULL  4.2f

/* Threshold raw ADC (mode 10-bit, range -512..+511) utk deteksi
 * TP4056 charging. Channel TP4056 sekarang ADC3, tapi logic & nilai
 * threshold TIDAK berubah — cuma pindah pin.
 *
 * PENTING — arah raw SUDAH DIKONFIRMASI DI HARDWARE ASLI, terbalik
 * dari asumsi awal: raw MAKIN NEGATIF saat tegangan MAKIN TINGGI.
 *   - TP4056 charging   (~5.2V, clip di batas atas) -> raw terukur ~-508
 *   - TP4056 tdk charging (~0V, clip di batas bawah) -> raw terukur ~+508
 *   - Baterai penuh (4.2V @ rasio 47/141)            -> raw ~-256 (estimasi)
 * -400 dipilih NEGATIF, di tengah antara -256 (baterai penuh) dan
 * -508 (TP4056 clip), dgn margin aman ke dua arah. */
#define TP4056_CHARGING_RAW_THRESHOLD  (-400)

/* Jeda antar siklus baca, dalam microsecond. */
#define LOOP_DELAY_US  15000000u  /* 15 detik */

/* -------------------------------------------------------------------- */
/* Helper delay — busy-wait berbasis pos_tick_get_counter_us(), fungsi   */
/* yang sudah TERKONFIRMASI ADA (dipakai langsung di i2c.c mentor utk    */
/* hitung timeout I2C), jadi bukan tebakan nama API timer.               */
/* -------------------------------------------------------------------- */
static void delay_us(uint32_t us)
{
    uint32_t start = pos_tick_get_counter_us();
    while ((pos_tick_get_counter_us() - start) < us) {
        /* busy-wait */
    }
}

/* -------------------------------------------------------------------- */
/* Fungsi baca baterai — membungkus pembacaan diferensial (B+ - B-)     */
/* oversampled, supaya pemanggil di main() tidak perlu tahu detail       */
/* channel/divider tiap titik ukur.                                     */
/* -------------------------------------------------------------------- */

/* Baca tegangan B+ (sudah dikompensasi divider), oversampled. */
static lis3dhtr_status_t read_bplus_voltage(float *voltage)
{
    return lis3dhtr_read_voltage_avg(BATTERY_BPLUS_ADC_CHANNEL,
                                     VDIVIDER_RATIO_BPLUS,
                                     OVERSAMPLE_COUNT, voltage);
}

/* Baca tegangan B- (sudah dikompensasi divider), oversampled. Nilai
 * ini TIDAK BOLEH dipakai apa adanya selama pin ADC2 masih clip (lihat
 * catatan di header file) — di sini hanya utk pembuktian/debug,
 * bukan representasi drop tegangan B- yang sebenarnya. */
static lis3dhtr_status_t read_bminus_voltage(float *voltage)
{
    return lis3dhtr_read_voltage_avg(BATTERY_BMINUS_ADC_CHANNEL,
                                     VDIVIDER_RATIO_BMINUS,
                                     OVERSAMPLE_COUNT, voltage);
}

/* Baca kode ADC mentah (oversampled) dari channel input TP4056 (ADC3). */
static lis3dhtr_status_t read_tp4056_raw(int16_t *raw)
{
    return lis3dhtr_read_raw_avg(TP4056_ADC_CHANNEL, OVERSAMPLE_COUNT, raw);
}

/* ---- DEBUG: baca raw + tegangan MENTAH DI PIN (SEBELUM divider) dari
 * B- (ADC2). Dipakai utk verifikasi apakah pin ADC2 sedang clip di
 * ujung rentang (~884-1684mV) — lihat catatan di header file soal
 * masalah jendela input aux ADC. Nilai mV di sini BUKAN tegangan B-
 * yang sudah dikompensasi divider, tapi tegangan mentah yang benar-
 * benar dilihat chip di pin-nya. */
static lis3dhtr_status_t read_bminus_debug(int16_t *raw_avg, float *pin_mv_avg)
{
    lis3dhtr_status_t st = lis3dhtr_read_raw_avg(BATTERY_BMINUS_ADC_CHANNEL,
                                                  OVERSAMPLE_COUNT, raw_avg);
    if (st != LIS3DHTR_OK) {
        return st;
    }

    return lis3dhtr_read_millivolt_avg(BATTERY_BMINUS_ADC_CHANNEL,
                                       OVERSAMPLE_COUNT, pin_mv_avg);
}

/* -------------------------------------------------------------------- */
/* Helper aplikasi (BUKAN bagian driver — logic bisnis di layer ini)     */
/* -------------------------------------------------------------------- */

/* Mapping linear tegangan baterai -> persentase (0-100), di-clamp. */
static int battery_voltage_to_percent(float voltage)
{
    float pct = (voltage - BATTERY_VOLTAGE_EMPTY) /
                (BATTERY_VOLTAGE_FULL - BATTERY_VOLTAGE_EMPTY) * 100.0f;

    if (pct < 0.0f)   { pct = 0.0f; }
    if (pct > 100.0f) { pct = 100.0f; }

    return (int)pct;
}

static void print_float_3dp(const char *label, float value)
{
    /* Hindari %f (sering tidak didukung penuh di printf bare-metal
     * pulp-rt); pecah manual jadi integer + 3 desimal. */
    int whole = (int)value;
    int frac  = (int)((value - (float)whole) * 1000.0f);
    if (frac < 0) { frac = -frac; }
    printf("%s%d.%03d\n\r", label, whole, frac);
}

int main()
{
    printf("=== LIS3DHTR Aux ADC Integration Test (FPGA/GVSoC) ===\n\r");
    printf("Monitor: baterai diferensial B+/B- (ADC1/ADC2) + input TP4056 (ADC3)\n\r");

    lis3dhtr_config_t cfg;
    lis3dhtr_default_config(&cfg);

    cfg.odr = LIS3DHTR_ODR_400HZ;

    lis3dhtr_status_t st = lis3dhtr_init(&cfg);
    if (st != LIS3DHTR_OK) {
        printf("[FAIL] lis3dhtr_init() gagal, status=%d\n\r", (int)st);
        return 1;
    }
    printf("[OK] lis3dhtr_init() sukses, WHO_AM_I terverifikasi 0x33\n\r");
    printf("Mulai monitoring terus-menerus, jeda %u detik per siklus...\n\r",
           (unsigned int)(LOOP_DELAY_US / 1000000u));

    while (1) {
        bool battery_ready = false;
        lis3dhtr_data_ready(BATTERY_BPLUS_ADC_CHANNEL, &battery_ready);

        if (!battery_ready) {
            printf("[WARN] data ADC belum siap, skip siklus ini\n\r");
            delay_us(LOOP_DELAY_US);
            continue;
        }

        /* --- Baterai: baca B+ dan B- TERPISAH (sudah divider) dulu,
         * supaya bisa dibuktikan/dibandingkan, baru selisihkan --- */
        float v_bplus, v_bminus;
        lis3dhtr_status_t st_bplus  = read_bplus_voltage(&v_bplus);
        lis3dhtr_status_t st_bminus = read_bminus_voltage(&v_bminus);

        if (st_bplus != LIS3DHTR_OK) {
            printf("================================================\n\r");
            printf("[FAIL] baca tegangan B+ gagal, status=%d\n\r", (int)st_bplus);
        } else {
            print_float_3dp("Tegangan B+ : ", v_bplus);
        }

        if (st_bminus != LIS3DHTR_OK) {
            printf("[FAIL] baca tegangan B- gagal, status=%d\n\r", (int)st_bminus);
        } else {
            print_float_3dp("Tegangan B- : ", v_bminus);
        }

        if (st_bplus == LIS3DHTR_OK && st_bminus == LIS3DHTR_OK) {
            float v_battery = v_bplus - v_bminus;
            int percent = battery_voltage_to_percent(v_battery);

            print_float_3dp("Tegangan baterai (B+ - B-): ", v_battery);
            printf("Persentase baterai (linear): %d%%\n\r", percent);
        }
#if 0
        /* --- DEBUG: B- (ADC2) raw + tegangan mentah di pin, utk cek
         * apakah sedang clip di ujung rentang aux ADC (~884-1684mV) --- */
        int16_t raw_bminus;
        float pin_mv_bminus;
        st = read_bminus_debug(&raw_bminus, &pin_mv_bminus);
        if (st != LIS3DHTR_OK) {
            printf("[FAIL] baca debug B- gagal, status=%d\n\r", (int)st);
        } else {
            printf("Raw B- (ADC2): %d\n\r", (int)raw_bminus);
            print_float_3dp("Tegangan pin B- (mentah, blm divider) mV: ", pin_mv_bminus);
        }
#endif
        /* --- TP4056: baca RAW + threshold binary --- */
        int16_t raw_tp4056;
        st = read_tp4056_raw(&raw_tp4056);
        if (st != LIS3DHTR_OK) {
            printf("[FAIL] baca raw TP4056 gagal, status=%d\n\r", (int)st);
        } else {
            /* raw MAKIN NEGATIF = tegangan MAKIN TINGGI (arah sudah
             * dikonfirmasi di hardware), jadi "charging" = raw <=
             * threshold (bukan >=). */
            bool charging = (raw_tp4056 <= TP4056_CHARGING_RAW_THRESHOLD);
            printf("Raw TP4056: %d\n\r", (int)raw_tp4056);
            printf("Status charging: %s\n\r", charging ? "YA" : "TIDAK");
            printf("================================================\n\r");
        }

        printf("---\n\r");
        delay_us(LOOP_DELAY_US);
    }

    /* Tidak pernah sampai sini — monitoring terus-menerus by design,
     * sesuai firmware alat yang menyala terus. */
}