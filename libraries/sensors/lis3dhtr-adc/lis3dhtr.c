/*
 * lis3dhtr.c
 *
 * Implementasi driver aux ADC LIS3DHTR — channel sbg parameter per
 * pemanggilan (bukan state global), utk mendukung monitor multi-titik
 * tegangan (mis. baterai + input TP4056) sekaligus.
 */

#include <stddef.h>
#include "lis3dhtr.h"
#include "pulp.h"

/* -------------------------------------------------------------------- */
/* State internal — HANYA hal chip-wide (bukan per-channel)             */
/* -------------------------------------------------------------------- */

static i2c_dev_t                  s_i2c_dev;
static i2c_t                     *s_i2c = NULL;
static lis3dhtr_adc_resolution_t  s_resolution = LIS3DHTR_ADC_RES_10BIT;

/* Batas iterasi polling data-ready PER SAMPLE di lis3dhtr_read_raw_avg().
 * Tujuannya cuma jaga-jaga (guard) supaya tidak hang selamanya kalau
 * data-ready tidak pernah nyala (mis. ADC_PD nonaktif atau channel
 * salah), BUKAN pengganti perhitungan waktu berbasis ODR yang presisi
 * (tidak ada API timer portable di scope driver ini). Nilai ini cukup
 * besar utk ODR serendah 1Hz dgn overhead I2C polling wajar; kalau
 * ODR dikonfigurasi lebih rendah dari itu atau bus I2C sangat lambat,
 * naikkan nilai ini. */
#define LIS3DHTR_DATA_READY_POLL_MAX_ITER   200000

/* -------------------------------------------------------------------- */
/* Helper internal I2C register access                                  */
/* -------------------------------------------------------------------- */

static lis3dhtr_status_t write_reg(uint8_t reg, uint8_t value)
{
    unsigned char buf[2];
    buf[0] = reg;
    buf[1] = value;

    int rc = i2c_write(s_i2c, buf, 2, 1 /* send_stop */);
    if (rc != 0) {
        return LIS3DHTR_ERR_I2C_WRITE;
    }
    return LIS3DHTR_OK;
}

static lis3dhtr_status_t read_regs(uint8_t reg, uint8_t *buf, int len)
{
    uint8_t addr = reg;
    if (len > 1) {
        addr |= LIS3DHTR_AUTO_INCREMENT_BIT;
    }

    int wrc = i2c_write(s_i2c, &addr, 1, 0 /* send_stop=0 -> repeated start */);
    if (wrc != 0) {
        return LIS3DHTR_ERR_I2C_WRITE;
    }

    int rrc = i2c_read(s_i2c, buf, len, 0 /* pending=0 -> STOP setelah baca */);
    if (rrc != len) {
        return LIS3DHTR_ERR_I2C_READ;
    }

    return LIS3DHTR_OK;
}

static uint8_t adc_out_l_reg(lis3dhtr_adc_channel_t ch)
{
    switch (ch) {
        case LIS3DHTR_ADC_CH1: return LIS3DHTR_REG_OUT_ADC1_L;
        case LIS3DHTR_ADC_CH2: return LIS3DHTR_REG_OUT_ADC2_L;
        case LIS3DHTR_ADC_CH3:
        default:               return LIS3DHTR_REG_OUT_ADC3_L;
    }
}

static uint8_t status_aux_da_bit(lis3dhtr_adc_channel_t ch)
{
    switch (ch) {
        case LIS3DHTR_ADC_CH1: return LIS3DHTR_STATUSAUX_1DA;
        case LIS3DHTR_ADC_CH2: return LIS3DHTR_STATUSAUX_2DA;
        case LIS3DHTR_ADC_CH3:
        default:               return LIS3DHTR_STATUSAUX_3DA;
    }
}

/* Busy-poll STATUS_AUX sampai bit data-ready 'channel' menyala, atau
 * sampai LIS3DHTR_DATA_READY_POLL_MAX_ITER tercapai (-> timeout).
 * Dipakai lis3dhtr_read_raw_avg() supaya tiap sample dijamin data
 * baru dari chip, bukan nilai lama yang terbaca ulang. */
static lis3dhtr_status_t wait_data_ready(lis3dhtr_adc_channel_t channel)
{
    for (int iter = 0; iter < LIS3DHTR_DATA_READY_POLL_MAX_ITER; iter++) {
        bool ready = false;
        lis3dhtr_status_t st = lis3dhtr_data_ready(channel, &ready);
        if (st != LIS3DHTR_OK) {
            return st;
        }
        if (ready) {
            return LIS3DHTR_OK;
        }
    }
    return LIS3DHTR_ERR_TIMEOUT;
}

/* -------------------------------------------------------------------- */
/* API publik                                                            */
/* -------------------------------------------------------------------- */

lis3dhtr_status_t lis3dhtr_default_config(lis3dhtr_config_t *cfg)
{
    if (cfg == NULL) {
        return LIS3DHTR_ERR_NULL_PTR;
    }

    cfg->i2c_id       = 0;
    cfg->addr7        = LIS3DHTR_I2C_ADDR_SA0_HIGH; /* 0x19 */
    cfg->max_baudrate = 400000u;

    cfg->odr        = LIS3DHTR_ODR_100HZ;
    cfg->resolution = LIS3DHTR_ADC_RES_10BIT;

    cfg->i2c_timeout_us       = 10000u;
    cfg->i2c_reset_on_timeout = true;

    return LIS3DHTR_OK;
}

lis3dhtr_status_t lis3dhtr_init(lis3dhtr_config_t *cfg)
{
    if (cfg == NULL) {
        return LIS3DHTR_ERR_NULL_PTR;
    }

    if ((int)cfg->odr == 0) {
        return LIS3DHTR_ERR_INVALID_CONFIG;
    }

    i2c_dev_init(&s_i2c_dev);
    s_i2c_dev.id           = (signed char)cfg->i2c_id;
    s_i2c_dev.cs           = (signed char)(cfg->addr7 << 1);
    s_i2c_dev.max_baudrate = cfg->max_baudrate;

    s_i2c = i2c_open(&s_i2c_dev);
    if (s_i2c == NULL) {
        return LIS3DHTR_ERR_I2C_OPEN;
    }

    if (cfg->i2c_timeout_us > 0u) {
        i2c_settimeout(cfg->i2c_timeout_us, cfg->i2c_reset_on_timeout);
    }

    s_resolution = cfg->resolution;

    uint8_t ctrl1 = lis3dhtr_encode_ctrl1(cfg->odr, cfg->resolution);
    lis3dhtr_status_t st = write_reg(LIS3DHTR_REG_CTRL_REG1, ctrl1);
    if (st != LIS3DHTR_OK) {
        s_i2c = NULL;
        return st;
    }

    uint8_t ctrl4 = lis3dhtr_encode_ctrl4();
    st = write_reg(LIS3DHTR_REG_CTRL_REG4, ctrl4);
    if (st != LIS3DHTR_OK) {
        s_i2c = NULL;
        return st;
    }

    uint8_t temp_cfg = lis3dhtr_encode_temp_cfg();
    st = write_reg(LIS3DHTR_REG_TEMP_CFG, temp_cfg);
    if (st != LIS3DHTR_OK) {
        s_i2c = NULL;
        return st;
    }

    uint8_t who = 0;
    st = lis3dhtr_who_am_i(&who);
    if (st != LIS3DHTR_OK) {
        s_i2c = NULL;
        return st;
    }
    if (who != LIS3DHTR_WHO_AM_I_VALUE) {
        s_i2c = NULL;
        return LIS3DHTR_ERR_WHOAMI;
    }

    return LIS3DHTR_OK;
}

lis3dhtr_status_t lis3dhtr_deinit(void)
{
    if (s_i2c != NULL) {
        i2c_close(s_i2c);
        s_i2c = NULL;
    }
    return LIS3DHTR_OK;
}

lis3dhtr_status_t lis3dhtr_who_am_i(uint8_t *out)
{
    if (out == NULL) {
        return LIS3DHTR_ERR_NULL_PTR;
    }
    if (s_i2c == NULL) {
        return LIS3DHTR_ERR_NOT_INIT;
    }

    uint8_t val = 0;
    lis3dhtr_status_t st = read_regs(LIS3DHTR_REG_WHO_AM_I, &val, 1);
    if (st != LIS3DHTR_OK) {
        return st;
    }

    *out = val;
    return LIS3DHTR_OK;
}

lis3dhtr_status_t lis3dhtr_data_ready(lis3dhtr_adc_channel_t channel, bool *ready)
{
    if (ready == NULL) {
        return LIS3DHTR_ERR_NULL_PTR;
    }
    if (s_i2c == NULL) {
        return LIS3DHTR_ERR_NOT_INIT;
    }

    uint8_t status_aux = 0;
    lis3dhtr_status_t st = read_regs(LIS3DHTR_REG_STATUS_AUX, &status_aux, 1);
    if (st != LIS3DHTR_OK) {
        return st;
    }

    *ready = (status_aux & status_aux_da_bit(channel)) != 0;
    return LIS3DHTR_OK;
}

lis3dhtr_status_t lis3dhtr_read_raw(lis3dhtr_adc_channel_t channel, int16_t *raw)
{
    if (raw == NULL) {
        return LIS3DHTR_ERR_NULL_PTR;
    }
    if (s_i2c == NULL) {
        return LIS3DHTR_ERR_NOT_INIT;
    }

    uint8_t buf[2];
    uint8_t reg = adc_out_l_reg(channel);
    lis3dhtr_status_t st = read_regs(reg, buf, 2);
    if (st != LIS3DHTR_OK) {
        return st;
    }

    int16_t left_justified = (int16_t)((uint16_t)((uint16_t)buf[1] << 8) | buf[0]);
    *raw = lis3dhtr_adc_shift_raw(left_justified, s_resolution);

    return LIS3DHTR_OK;
}

lis3dhtr_status_t lis3dhtr_read_millivolt(lis3dhtr_adc_channel_t channel, float *mv)
{
    if (mv == NULL) {
        return LIS3DHTR_ERR_NULL_PTR;
    }

    int16_t raw;
    lis3dhtr_status_t st = lis3dhtr_read_raw(channel, &raw);
    if (st != LIS3DHTR_OK) {
        return st;
    }

    *mv = lis3dhtr_raw_to_millivolt(raw, s_resolution);
    return LIS3DHTR_OK;
}

lis3dhtr_status_t lis3dhtr_read_voltage(lis3dhtr_adc_channel_t channel,
                                        float vdivider_ratio,
                                        float *voltage)
{
    if (voltage == NULL) {
        return LIS3DHTR_ERR_NULL_PTR;
    }

    float mv;
    lis3dhtr_status_t st = lis3dhtr_read_millivolt(channel, &mv);
    if (st != LIS3DHTR_OK) {
        return st;
    }

    *voltage = (mv / 1000.0f) / vdivider_ratio;
    return LIS3DHTR_OK;
}

lis3dhtr_status_t lis3dhtr_read_raw_avg(lis3dhtr_adc_channel_t channel,
                                        int sample_count,
                                        int16_t *raw_avg)
{
    if (raw_avg == NULL) {
        return LIS3DHTR_ERR_NULL_PTR;
    }
    if (sample_count <= 0) {
        return LIS3DHTR_ERR_INVALID_CONFIG;
    }
    if (s_i2c == NULL) {
        return LIS3DHTR_ERR_NOT_INIT;
    }

    int32_t sum = 0;
    for (int i = 0; i < sample_count; i++) {
        /* Tunggu data-ready BARU dulu sebelum tiap sample, supaya
         * tiap sample adalah pembacaan chip yang independen (bukan
         * nilai lama yang kebetulan terbaca ulang karena I2C lebih
         * cepat dari ODR). Lihat wait_data_ready() dan catatan di
         * lis3dhtr_read_raw() soal BDU + auto-clear bit *DA. */
        lis3dhtr_status_t st = wait_data_ready(channel);
        if (st != LIS3DHTR_OK) {
            return st;
        }

        int16_t raw;
        st = lis3dhtr_read_raw(channel, &raw);
        if (st != LIS3DHTR_OK) {
            return st;
        }
        sum += raw;
    }

    *raw_avg = (int16_t)(sum / sample_count);
    return LIS3DHTR_OK;
}

lis3dhtr_status_t lis3dhtr_read_millivolt_avg(lis3dhtr_adc_channel_t channel,
                                              int sample_count,
                                              float *mv_avg)
{
    if (mv_avg == NULL) {
        return LIS3DHTR_ERR_NULL_PTR;
    }

    int16_t raw_avg;
    lis3dhtr_status_t st = lis3dhtr_read_raw_avg(channel, sample_count, &raw_avg);
    if (st != LIS3DHTR_OK) {
        return st;
    }

    *mv_avg = lis3dhtr_raw_to_millivolt(raw_avg, s_resolution);
    return LIS3DHTR_OK;
}

lis3dhtr_status_t lis3dhtr_read_voltage_avg(lis3dhtr_adc_channel_t channel,
                                            float vdivider_ratio,
                                            int sample_count,
                                            float *voltage_avg)
{
    if (voltage_avg == NULL) {
        return LIS3DHTR_ERR_NULL_PTR;
    }

    float mv_avg;
    lis3dhtr_status_t st = lis3dhtr_read_millivolt_avg(channel, sample_count, &mv_avg);
    if (st != LIS3DHTR_OK) {
        return st;
    }

    *voltage_avg = (mv_avg / 1000.0f) / vdivider_ratio;
    return LIS3DHTR_OK;
}

/* -------------------------------------------------------------------- */
/* Pembacaan tegangan baterai diferensial (B+ minus B-)                 */
/* Lihat penjelasan lengkap di lis3dhtr.h.                               */
/* -------------------------------------------------------------------- */

lis3dhtr_status_t lis3dhtr_read_battery_voltage_diff(
    lis3dhtr_adc_channel_t ch_bplus,  float vdivider_ratio_bplus,
    lis3dhtr_adc_channel_t ch_bminus, float vdivider_ratio_bminus,
    float *voltage_diff)
{
    if (voltage_diff == NULL) {
        return LIS3DHTR_ERR_NULL_PTR;
    }

    float v_bplus;
    lis3dhtr_status_t st = lis3dhtr_read_voltage(ch_bplus, vdivider_ratio_bplus, &v_bplus);
    if (st != LIS3DHTR_OK) {
        return st;
    }

    float v_bminus;
    st = lis3dhtr_read_voltage(ch_bminus, vdivider_ratio_bminus, &v_bminus);
    if (st != LIS3DHTR_OK) {
        return st;
    }

    *voltage_diff = v_bplus - v_bminus;
    return LIS3DHTR_OK;
}

lis3dhtr_status_t lis3dhtr_read_battery_voltage_diff_avg(
    lis3dhtr_adc_channel_t ch_bplus,  float vdivider_ratio_bplus,
    lis3dhtr_adc_channel_t ch_bminus, float vdivider_ratio_bminus,
    int sample_count,
    float *voltage_diff_avg)
{
    if (voltage_diff_avg == NULL) {
        return LIS3DHTR_ERR_NULL_PTR;
    }

    /* Catatan urutan: B+ dan B- dibaca berurutan (bukan simultan),
     * masing-masing sudah oversampled 'sample_count' kali sendiri.
     * Selama tegangan baterai relatif stabil dibanding waktu total
     * pembacaan (2x sample_count/ODR detik), ini cukup akurat; kalau
     * beban baterai berubah sangat cepat, pertimbangkan sample_count
     * kecil supaya jeda antar B+ dan B- minim. */
    float v_bplus_avg;
    lis3dhtr_status_t st = lis3dhtr_read_voltage_avg(ch_bplus, vdivider_ratio_bplus,
                                                      sample_count, &v_bplus_avg);
    if (st != LIS3DHTR_OK) {
        return st;
    }

    float v_bminus_avg;
    st = lis3dhtr_read_voltage_avg(ch_bminus, vdivider_ratio_bminus,
                                   sample_count, &v_bminus_avg);
    if (st != LIS3DHTR_OK) {
        return st;
    }

    *voltage_diff_avg = v_bplus_avg - v_bminus_avg;
    return LIS3DHTR_OK;
}

/* -------------------------------------------------------------------- */
/* Fungsi pure-logic (dipakai unit test + internal)                     */
/* -------------------------------------------------------------------- */

uint8_t lis3dhtr_encode_ctrl1(lis3dhtr_odr_t odr, lis3dhtr_adc_resolution_t res)
{
    uint8_t reg = (uint8_t)((uint8_t)odr << LIS3DHTR_CTRL1_ODR_SHIFT);

    if (res == LIS3DHTR_ADC_RES_8BIT) {
        reg |= LIS3DHTR_CTRL1_LPEN;
    }

    return reg;
}

uint8_t lis3dhtr_encode_ctrl4(void)
{
    return LIS3DHTR_CTRL4_BDU;
}

uint8_t lis3dhtr_encode_temp_cfg(void)
{
    return LIS3DHTR_TEMPCFG_ADC_PD;
}

int16_t lis3dhtr_adc_shift_raw(int16_t raw_left_justified, lis3dhtr_adc_resolution_t res)
{
    return (res == LIS3DHTR_ADC_RES_8BIT) ? (int16_t)(raw_left_justified >> 8)
                                           : (int16_t)(raw_left_justified >> 6);
}

float lis3dhtr_adc_mv_per_digit(lis3dhtr_adc_resolution_t res)
{
    return (res == LIS3DHTR_ADC_RES_8BIT) ? (800.0f / 256.0f)
                                           : (800.0f / 1024.0f);
}

float lis3dhtr_raw_to_millivolt(int16_t raw, lis3dhtr_adc_resolution_t res)
{
    /* PENTING: berdasarkan pengujian di hardware asli (bukan asumsi
     * dari datasheet/community), arah aux ADC chip ini TERBALIK dari
     * yang awalnya diasumsikan — raw MAKIN NEGATIF saat tegangan input
     * MAKIN TINGGI (bukan makin positif). Dikonfirmasi dari uji TP4056:
     * input ~5.2V (clip di batas atas) -> raw ~-508 (sangat negatif);
     * input ~0V (clip di batas bawah)  -> raw ~+508 (sangat positif).
     *
     * KALIBRASI BIAS (1283.5 mV, bukan 1200 mV): dihitung dari data
     * pengukuran nyata (13 level tegangan 3.0V-4.2V, replikasi per
     * level dgn jeda 15 detik). Lihat CALIBRATION.md utk data mentah,
     * langkah regresi, dan verifikasi independen (R^2~0.9998, error
     * <0.3% di level rata-rata). Data kalibrasi mencakup rentang mV di
     * pin ~1000-1400mV (Vin 3.0-4.2V @ rasio divider 47/141) — TIDAK
     * mencakup titik ekstrem/clip (~800mV & ~1600mV), jadi lebar
     * rentang ±400mV di kedua sisi bias MASIH ASUMSI dari ST Community,
     * belum tervalidasi langsung seperti nilai bias 1283.5 ini. */
    return 1284.2f - ((float)raw * lis3dhtr_adc_mv_per_digit(res)); //hasil perhitungan 1283.5 /1284.2
}