/*
 * lis3dhtr.h
 *
 * Driver I2C untuk fitur AUXILIARY ADC pada chip LIS3DHTR, dipakai
 * untuk monitor 2 titik tegangan sekaligus (baterai + input TP4056)
 * lewat channel ADC yang berbeda pada chip yang sama.
 *
 * PENTING: channel ADC adalah PARAMETER PER-PANGGILAN, bukan setting
 * global di init(). Chip secara hardware sudah sampling ADC1/ADC2/ADC3
 * secara bersamaan & terus-menerus selama ADC_PD=1 (lihat STATUS_AUX,
 * yang punya bit data-ready terpisah utk tiap channel) — jadi tidak
 * perlu "switch channel" di chip, cukup baca alamat register yang
 * berbeda per channel yang diinginkan.
 *
 * Driver ini TIDAK mengimplementasikan fitur accelerometer chip ini
 * sama sekali (bukan requirement task ini).
 */

#ifndef LIS3DHTR_H
#define LIS3DHTR_H

#include <stdint.h>
#include <stdbool.h>
#include "lis3dhtr_regs.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------- */
/* Enum konfigurasi                                                      */
/* -------------------------------------------------------------------- */

/* Output Data Rate — sekaligus jadi sampling rate SEMUA channel aux
 * ADC (chip-wide, bukan per-channel). TIDAK boleh power-down. */
typedef enum {
    LIS3DHTR_ODR_1HZ        = 0x1,
    LIS3DHTR_ODR_10HZ       = 0x2,
    LIS3DHTR_ODR_25HZ       = 0x3,
    LIS3DHTR_ODR_50HZ       = 0x4,
    LIS3DHTR_ODR_100HZ      = 0x5,
    LIS3DHTR_ODR_200HZ      = 0x6,
    LIS3DHTR_ODR_400HZ      = 0x7
} lis3dhtr_odr_t;

/* Resolusi aux ADC (chip-wide, berlaku utk semua channel sekaligus) —
 * HANYA bergantung ke LPen (CTRL_REG1 bit 3). */
typedef enum {
    LIS3DHTR_ADC_RES_10BIT = 0,  /* LPen=0 (Normal/High-res mode) */
    LIS3DHTR_ADC_RES_8BIT  = 1   /* LPen=1 (Low-power mode)       */
} lis3dhtr_adc_resolution_t;

/* Pin aux ADC — dipakai sbg PARAMETER tiap pemanggilan fungsi baca,
 * bukan disimpan sbg state global. */
typedef enum {
    LIS3DHTR_ADC_CH1 = 1,
    LIS3DHTR_ADC_CH2 = 2,
    LIS3DHTR_ADC_CH3 = 3
} lis3dhtr_adc_channel_t;

typedef enum {
    LIS3DHTR_OK = 0,
    LIS3DHTR_ERR_NULL_PTR,
    LIS3DHTR_ERR_NOT_INIT,
    LIS3DHTR_ERR_INVALID_CONFIG,
    LIS3DHTR_ERR_I2C_OPEN,
    LIS3DHTR_ERR_I2C_WRITE,
    LIS3DHTR_ERR_I2C_READ,
    LIS3DHTR_ERR_WHOAMI,
    LIS3DHTR_ERR_TIMEOUT       /* menunggu data-ready terlalu lama, lihat
                                 * lis3dhtr_read_raw_avg() */
} lis3dhtr_status_t;

/* -------------------------------------------------------------------- */
/* Struct konfigurasi — HANYA setting chip-wide (bukan per-channel)      */
/* -------------------------------------------------------------------- */

typedef struct {
    int      i2c_id;
    uint8_t  addr7;            /* 0x18 (SA0=GND) atau 0x19 (SA0=VCC) */
    unsigned int max_baudrate;

    lis3dhtr_odr_t             odr;         /* JANGAN power-down */
    lis3dhtr_adc_resolution_t  resolution;

    uint32_t i2c_timeout_us;
    bool     i2c_reset_on_timeout;
} lis3dhtr_config_t;

/* -------------------------------------------------------------------- */
/* API                                                                   */
/* -------------------------------------------------------------------- */

/* Default: i2c_id=0, addr7=0x19, max_baudrate=400000, odr=100Hz,
 * resolution=10-bit, i2c_timeout_us=10000, i2c_reset_on_timeout=true. */
lis3dhtr_status_t lis3dhtr_default_config(lis3dhtr_config_t *cfg);

lis3dhtr_status_t lis3dhtr_init(lis3dhtr_config_t *cfg);
lis3dhtr_status_t lis3dhtr_deinit(void);
lis3dhtr_status_t lis3dhtr_who_am_i(uint8_t *out);

/* true kalau data baru di 'channel' sudah siap dibaca (bit *DA di
 * STATUS_AUX). */
lis3dhtr_status_t lis3dhtr_data_ready(lis3dhtr_adc_channel_t channel, bool *ready);

/* Baca 1 sample kode ADC mentah dari 'channel', sudah di-shift sesuai
 * resolution, BELUM dikonversi ke Volt.
 *
 * CATATAN: fungsi ini TIDAK menunggu data-ready — kalau dipanggil
 * berkali-kali lebih cepat dari periode ODR, bisa membaca nilai chip
 * yang sama berulang. Karena CTRL_REG4 diset BDU=1 (Block Data
 * Update), baca L lalu H dalam SATU transaksi (yang sudah dilakukan
 * fungsi ini via auto-increment) itu atomik terhadap update chip DAN
 * otomatis clear bit *DA saat H selesai dibaca — properti ini yang
 * dipakai lis3dhtr_read_raw_avg() untuk menjamin independensi sample. */
lis3dhtr_status_t lis3dhtr_read_raw(lis3dhtr_adc_channel_t channel, int16_t *raw);

/* Baca tegangan di pin ADC (mV), BELUM dikompensasi divider.
 * CATATAN ARAH: raw MAKIN NEGATIF saat tegangan MAKIN TINGGI (sudah
 * dikonfirmasi di hardware asli, lihat lis3dhtr_raw_to_millivolt()). */
lis3dhtr_status_t lis3dhtr_read_millivolt(lis3dhtr_adc_channel_t channel, float *mv);

/* Baca tegangan di sumber SEBELUM divider (Volt), sudah dikompensasi
 * 'vdivider_ratio' (R2/(R1+R2)) yg diberikan sbg parameter — supaya
 * satu driver bisa dipakai utk beberapa titik ukur dgn rasio berbeda
 * sekaligus (mis. baterai di CH1, TP4056 input di CH2). */
lis3dhtr_status_t lis3dhtr_read_voltage(lis3dhtr_adc_channel_t channel,
                                        float vdivider_ratio,
                                        float *voltage);

/* ---- Varian oversampling: baca 'sample_count' kali (>=1), kembalikan
 * rata-ratanya, utk mengurangi noise.
 *
 * SETIAP sample menunggu bit data-ready (*DA di STATUS_AUX) BARU
 * lebih dulu lewat busy-poll I2C (tidak ada API timer/sleep portable
 * di scope driver ini, jadi menunggu dilakukan dengan polling
 * berulang, bukan delay berbasis waktu). Ini menjamin tiap sample
 * adalah pembacaan chip yang independen (bukan nilai lama yang
 * terbaca ulang), sesuai catatan pada lis3dhtr_read_raw().
 *
 * KONSEKUENSI: fungsi ini BLOCKING dan waktu tempuhnya kira-kira
 * sample_count / ODR detik (mis. 12 sample @ ODR 100Hz ≈ 120ms),
 * jauh lebih lambat dari versi tanpa menunggu data-ready. Ada guard
 * LIS3DHTR_DATA_READY_POLL_MAX_ITER per sample utk mencegah hang
 * permanen (mengembalikan LIS3DHTR_ERR_TIMEOUT) kalau ODR
 * dikonfigurasi sangat rendah relatif terhadap guard ini, channel
 * tidak pernah data-ready (misalnya ADC_PD tidak aktif), atau ada
 * masalah bus I2C — sesuaikan guard tsb di lis3dhtr.c kalau perlu
 * ODR yang sangat rendah (mis. 1Hz). ---- */
lis3dhtr_status_t lis3dhtr_read_raw_avg(lis3dhtr_adc_channel_t channel,
                                        int sample_count,
                                        int16_t *raw_avg);

lis3dhtr_status_t lis3dhtr_read_millivolt_avg(lis3dhtr_adc_channel_t channel,
                                              int sample_count,
                                              float *mv_avg);

lis3dhtr_status_t lis3dhtr_read_voltage_avg(lis3dhtr_adc_channel_t channel,
                                            float vdivider_ratio,
                                            int sample_count,
                                            float *voltage_avg);

/* ---------------------------------------------------------------------
 * Pembacaan tegangan baterai DIFERENSIAL (B+ minus B-)
 *
 * LATAR BELAKANG: pada modul charger TP4056 dengan IC proteksi
 * (mis. DW01+FS8205), jalur B- baterai TIDAK terhubung langsung ke
 * GND sistem — ada MOSFET proteksi di antaranya yang menimbulkan
 * drop tegangan kecil (puluhan-ratusan mV) saat arus mengalir
 * (charge/discharge). Akibatnya B- tidak boleh diasumsikan sama
 * dengan GND.
 *
 * SOLUSI: ukur B+ dan B- SEPARATE, keduanya terhadap GND yang SAMA
 * (GND sisi USB-/TP4056, bukan GND baterai), lalu selisihkan:
 *
 *     tegangan_baterai_asli = tegangan(B+) - tegangan(B-)
 *
 * Kedua titik boleh punya channel ADC & rasio voltage divider yang
 * berbeda (meski di board ini kebetulan sama, 47/141 utk keduanya),
 * makanya keduanya tetap jadi parameter terpisah supaya driver ini
 * tetap general-purpose. --------------------------------------------- */

/* Versi 1 sample (tanpa oversampling) per titik. */
lis3dhtr_status_t lis3dhtr_read_battery_voltage_diff(
    lis3dhtr_adc_channel_t ch_bplus,  float vdivider_ratio_bplus,
    lis3dhtr_adc_channel_t ch_bminus, float vdivider_ratio_bminus,
    float *voltage_diff);

/* Versi oversampling (sample_count per titik) — direkomendasikan,
 * konsisten dengan lis3dhtr_read_voltage_avg(). */
lis3dhtr_status_t lis3dhtr_read_battery_voltage_diff_avg(
    lis3dhtr_adc_channel_t ch_bplus,  float vdivider_ratio_bplus,
    lis3dhtr_adc_channel_t ch_bminus, float vdivider_ratio_bminus,
    int sample_count,
    float *voltage_diff_avg);

/* ---- Fungsi pure-logic dipakai unit test (bisa dipakai user juga) --- */

uint8_t lis3dhtr_encode_ctrl1(lis3dhtr_odr_t odr, lis3dhtr_adc_resolution_t res);
uint8_t lis3dhtr_encode_ctrl4(void);
uint8_t lis3dhtr_encode_temp_cfg(void);

int16_t lis3dhtr_adc_shift_raw(int16_t raw_left_justified, lis3dhtr_adc_resolution_t res);
float   lis3dhtr_adc_mv_per_digit(lis3dhtr_adc_resolution_t res);
float   lis3dhtr_raw_to_millivolt(int16_t raw, lis3dhtr_adc_resolution_t res);

#ifdef __cplusplus
}
#endif

#endif /* LIS3DHTR_H */