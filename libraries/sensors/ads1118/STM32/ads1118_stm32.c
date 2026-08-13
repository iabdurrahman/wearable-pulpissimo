/*
 * Copyright (C) 2026 ICDeC
 *
 * Driver library for the Texas Instruments ADS1118 SPI ADC - STM32 HAL port.
 *
 * Uses HAL_SPI_TransmitReceive() for the full-duplex 32-bit exchange, and
 * manual GPIO toggling for chip-select (unlike PULPissimo's spim_transfer(),
 * STM32 HAL does not manage CS automatically).
 *
 * IMPORTANT: SPI Mode 1 (CPOL=Low, CPHA=2Edge, per the ADS1118 datasheet)
 * must be configured in CubeMX when generating hspi's init code - this
 * driver has no way to change SPI mode at runtime the way the PULPissimo
 * version can (spim_conf.polarity/phase).
 */

#include <stdio.h>
#include "ads1118_stm32.h"

/* -------------------------------------------------------------------------
 * Internal driver state (singleton, following the mpu6050.c/ads1118.c
 * convention used elsewhere in this codebase)
 * ------------------------------------------------------------------------- */
static struct {
    SPI_HandleTypeDef *hspi;
    GPIO_TypeDef      *cs_port;
    uint16_t           cs_pin;
    uint16_t           mux;
    uint16_t           pga;
    uint16_t           data_rate;
    float              vdivider_ratio;
    uint16_t           mux_bminus;
    float              vdivider_ratio_bminus;
    uint16_t           mux_inplus;
    float              vdivider_ratio_inplus;
    float              low_batt_threshold_v;
    float              charge_detect_threshold_v;
    int                initialized;
} ads1118_state = { .initialized = 0 };

/* -------------------------------------------------------------------------
 * Internal helpers
 * ------------------------------------------------------------------------- */

static float ads1118_pga_to_fs_voltage(uint16_t pga)
{
    switch (pga) {
        case ADS1118_PGA_FS_6144: return ADS1118_FS_VOLTAGE_6144;
        case ADS1118_PGA_FS_4096: return ADS1118_FS_VOLTAGE_4096;
        case ADS1118_PGA_FS_2048: return ADS1118_FS_VOLTAGE_2048;
        case ADS1118_PGA_FS_1024: return ADS1118_FS_VOLTAGE_1024;
        case ADS1118_PGA_FS_0512: return ADS1118_FS_VOLTAGE_0512;
        case ADS1118_PGA_FS_0256: return ADS1118_FS_VOLTAGE_0256;
        default:                  return ADS1118_FS_VOLTAGE_2048;
    }
}

/**
 * @brief Koreksi kalibrasi linear (gain + offset) hasil regresi 13 titik
 * data kalibrasi (4.2V s/d 3.0V). Mengoreksi gain error yang kemungkinan
 * berasal dari toleransi resistor voltage divider, bukan dari ADC-nya.
 *
 * Formula: V_terkoreksi = (V_terukur + OFFSET) * GAIN
 */
static float ads1118_apply_calibration(float v_measured)
{
    const float CAL_OFFSET = 0.0048f;
    const float CAL_GAIN   = 1.0028f;

    return (v_measured + CAL_OFFSET) * CAL_GAIN;
}

/**
 * @brief Delay scaled to the selected data rate, using HAL_Delay (real
 * millisecond timer) instead of a busy-wait loop - a clear improvement
 * over the PULPissimo version's placeholder busy-wait, since HAL_Delay
 * is backed by SysTick and is actually accurate here.
 */
static void ads1118_delay_for_datarate(uint16_t data_rate)
{
    uint32_t delay_ms;

    switch (data_rate) {
        case ADS1118_DR_8SPS:   delay_ms = 130; break; /* 1/8 s + margin */
        case ADS1118_DR_16SPS:  delay_ms = 65;  break;
        case ADS1118_DR_32SPS:  delay_ms = 35;  break;
        case ADS1118_DR_64SPS:  delay_ms = 18;  break;
        case ADS1118_DR_128SPS: delay_ms = 10;  break;
        case ADS1118_DR_250SPS: delay_ms = 5;   break;
        case ADS1118_DR_475SPS: delay_ms = 3;   break;
        case ADS1118_DR_860SPS: delay_ms = 2;   break;
        default:                delay_ms = 10;  break;
    }

    HAL_Delay(delay_ms);
}

/**
 * @brief Perform one 32-bit SPI exchange with the ADS1118.
 *
 * Manually asserts/releases CS around HAL_SPI_TransmitReceive(), since
 * STM32 HAL (unlike PULPissimo's spim_transfer) does not manage CS
 * automatically. Sends the same 16-bit config word twice (per the
 * datasheet's 32-bit transfer cycle) and receives 4 bytes back: DATA_MSB,
 * DATA_LSB, CONFIG_MSB, CONFIG_LSB.
 */
static ads1118_status_t ads1118_spi_exchange(uint16_t config_word,
                                              int16_t *out_data,
                                              uint16_t *out_config_readback)
{
    uint8_t tx[4];
    uint8_t rx[4];
    HAL_StatusTypeDef hal_status;

    if (ads1118_state.hspi == NULL) {
        return ADS1118_ERR_SPI;
    }

    tx[0] = (uint8_t)(config_word >> 8);
    tx[1] = (uint8_t)(config_word & 0xFF);
    tx[2] = (uint8_t)(config_word >> 8);
    tx[3] = (uint8_t)(config_word & 0xFF);

    HAL_GPIO_WritePin(ads1118_state.cs_port, ads1118_state.cs_pin, GPIO_PIN_RESET); /* CS low */
    hal_status = HAL_SPI_TransmitReceive(ads1118_state.hspi, tx, rx, 4, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(ads1118_state.cs_port, ads1118_state.cs_pin, GPIO_PIN_SET);   /* CS high */

    if (hal_status != HAL_OK) {
        /* TEMPORARY diagnostic - remove once root cause is confirmed.
         * Dumps the specific SPI hardware error flag(s) so we can see
         * exactly what's happening during the real back-to-back read
         * sequence (MODF=mode fault, OVR=overrun, CRCERR, FRE=frame
         * error), not just in a manually-paced debug test. */
        printf("[ADS1118] HAL_SPI_TransmitReceive failed, hal_status=%d, "
               "SPI error flags=0x%08lX\r\n",
               (int)hal_status, (unsigned long)HAL_SPI_GetError(ads1118_state.hspi));
        return ADS1118_ERR_SPI;
    }

    /* Small settle time after releasing CS, before the bus is used again.
     * ads1118_read_raw_filtered() calls this back-to-back 12x with zero
     * gap otherwise; on a marginal breadboard connection that can be
     * tighter than the signal can reliably tolerate. Cheap insurance. */
    HAL_Delay(1);

    if (out_data != NULL) {
        *out_data = (int16_t)((rx[0] << 8) | rx[1]);
    }
    if (out_config_readback != NULL) {
        *out_config_readback = (uint16_t)((rx[2] << 8) | rx[3]);
    }

    return ADS1118_OK;
}

/**
 * @brief Wrapper around ads1118_spi_exchange() that retries up to
 * ADS1118_SPI_RETRY_COUNT times on ADS1118_ERR_SPI, with a short delay
 * between attempts.
 *
 * Breadboard/jumper-wire connections can glitch a single SPI transaction
 * occasionally without the wiring being genuinely broken - retrying a
 * couple of times is cheap and avoids failing an entire read (or an
 * entire 12-sample filter pass) over one transient bus error. This does
 * NOT retry other error types (NULL/CONFIG) since those aren't transient.
 */
static ads1118_status_t ads1118_spi_exchange_retry(uint16_t config_word,
                                                     int16_t *out_data,
                                                     uint16_t *out_config_readback)
{
    ads1118_status_t status;
    uint32_t attempt;

    for (attempt = 0; attempt < ADS1118_SPI_RETRY_COUNT; attempt++) {
        status = ads1118_spi_exchange(config_word, out_data, out_config_readback);
        if (status == ADS1118_OK) {
            return ADS1118_OK;
        }
        if (status != ADS1118_ERR_SPI) {
            return status; /* not transient, no point retrying */
        }
        if (attempt + 1 < ADS1118_SPI_RETRY_COUNT) {
            HAL_Delay(ADS1118_SPI_RETRY_DELAY_MS);
        }
    }

    return ADS1118_ERR_SPI; /* exhausted retries, still failing - real problem */
}

/**
 * @brief Simple insertion sort, ascending. Cukup untuk ukuran kecil
 * (ADS1118_FILTER_SAMPLE_COUNT = 12) - tidak perlu qsort/heap.
 */
static void ads1118_sort_int16_asc(int16_t *arr, uint16_t n)
{
    uint16_t i, j;
    int16_t key;

    for (i = 1; i < n; i++) {
        key = arr[i];
        j = i;
        while (j > 0 && arr[j - 1] > key) {
            arr[j] = arr[j - 1];
            j--;
        }
        arr[j] = key;
    }
}

/* -------------------------------------------------------------------------
 * Public API Implementation
 * ------------------------------------------------------------------------- */

uint16_t ads1118_encode_config(uint16_t mux, uint16_t pga, uint16_t data_rate)
{
    uint16_t cfg = 0;

    cfg |= ADS1118_OS_START;
    cfg |= mux;
    cfg |= pga;
    cfg |= ADS1118_MODE_SINGLESHOT;
    cfg |= data_rate;
    cfg |= ADS1118_TS_MODE_ADC;
    cfg |= ADS1118_PULLUP_DISABLE;
    cfg |= ADS1118_NOP_VALID;
    cfg |= ADS1118_RESERVED_BIT0;

    return cfg;
}

float ads1118_raw_to_voltage(int16_t raw, uint16_t pga)
{
    float fs = ads1118_pga_to_fs_voltage(pga);
    return ((float)raw / 32767.0f) * fs;
}

ads1118_status_t ads1118_default_config(ads1118_config_t *cfg)
{
    if (cfg == NULL) {
        return ADS1118_ERR_NULL;
    }

    cfg->hspi           = NULL;  /* caller MUST fill this in (e.g. &hspi1) */
    cfg->cs_port        = NULL;  /* caller MUST fill this in (e.g. GPIOA) */
    cfg->cs_pin         = 0;     /* caller MUST fill this in (e.g. GPIO_PIN_4) */
    cfg->mux            = ADS1118_MUX_AIN0_GND;
    cfg->pga            = ADS1118_PGA_FS_4096;
    cfg->data_rate      = ADS1118_DR_128SPS;
    cfg->vdivider_ratio = 0.5f;  /* ASSUMED 2:1 divider - confirm with hardware! */

    cfg->mux_bminus              = ADS1118_MUX_AIN1_GND;
    cfg->vdivider_ratio_bminus   = 0.5f;  /* ASSUMED - MUST match your actual B- divider! */
    cfg->mux_inplus              = ADS1118_MUX_AIN2_GND;
    cfg->vdivider_ratio_inplus   = 0.5f;  /* ASSUMED - MUST match your actual IN+ divider! */
    cfg->low_batt_threshold_v    = 3.3f;  /* typical single-cell Li-ion low cutoff, tune as needed */
    cfg->charge_detect_threshold_v = 3.0f; /* threshold on V(IN+) - tune to your board */

    return ADS1118_OK;
}

ads1118_status_t ads1118_init(const ads1118_config_t *cfg)
{
    if (cfg == NULL) {
        return ADS1118_ERR_NULL;
    }

    if (cfg->hspi == NULL || cfg->cs_port == NULL) {
        return ADS1118_ERR_CONFIG;
    }

    if (cfg->vdivider_ratio <= 0.0f || cfg->vdivider_ratio_bminus <= 0.0f ||
        cfg->vdivider_ratio_inplus <= 0.0f) {
        return ADS1118_ERR_CONFIG;
    }

    ads1118_state.hspi           = cfg->hspi;
    ads1118_state.cs_port        = cfg->cs_port;
    ads1118_state.cs_pin         = cfg->cs_pin;
    ads1118_state.mux            = cfg->mux;
    ads1118_state.pga            = cfg->pga;
    ads1118_state.data_rate      = cfg->data_rate;
    ads1118_state.vdivider_ratio = cfg->vdivider_ratio;
    ads1118_state.mux_bminus              = cfg->mux_bminus;
    ads1118_state.vdivider_ratio_bminus   = cfg->vdivider_ratio_bminus;
    ads1118_state.mux_inplus              = cfg->mux_inplus;
    ads1118_state.vdivider_ratio_inplus   = cfg->vdivider_ratio_inplus;
    ads1118_state.low_batt_threshold_v    = cfg->low_batt_threshold_v;
    ads1118_state.charge_detect_threshold_v = cfg->charge_detect_threshold_v;

    /* Idle CS high before first use */
    HAL_GPIO_WritePin(ads1118_state.cs_port, ads1118_state.cs_pin, GPIO_PIN_SET);

    ads1118_state.initialized = 1;

    printf("[ADS1118] Initialized (mux=0x%04X, pga=0x%04X, dr=0x%04X, divider=%d/1000, "
           "mux_bminus=0x%04X, divider_bminus=%d/1000, mux_inplus=0x%04X, divider_inplus=%d/1000, "
           "low_batt=%d mV, charge_thr=%d mV)\n",
           cfg->mux, cfg->pga, cfg->data_rate, (int)(cfg->vdivider_ratio * 1000),
           cfg->mux_bminus, (int)(cfg->vdivider_ratio_bminus * 1000),
           cfg->mux_inplus, (int)(cfg->vdivider_ratio_inplus * 1000),
           (int)(cfg->low_batt_threshold_v * 1000), (int)(cfg->charge_detect_threshold_v * 1000));

    return ADS1118_OK;
}

/**
 * @brief Internal: trigger + read a single-shot conversion on an
 * explicitly given MUX setting (does not touch ads1118_state.mux).
 * This is what lets B+ and B- share the same read/filter machinery
 * while targeting different ADS1118 input channels.
 */
static ads1118_status_t ads1118_read_raw_mux(uint16_t mux, int16_t *raw)
{
    uint16_t cfg;
    ads1118_status_t status;

    if (raw == NULL) {
        return ADS1118_ERR_NULL;
    }
    if (!ads1118_state.initialized) {
        return ADS1118_ERR_CONFIG;
    }

    cfg = ads1118_encode_config(mux, ads1118_state.pga, ads1118_state.data_rate);

    /* Step 1: trigger a new single-shot conversion (data returned by this
     * exchange is stale/leftover from before, so it is discarded). */
    status = ads1118_spi_exchange_retry(cfg, NULL, NULL);
    if (status != ADS1118_OK) {
        return status;
    }

    /* Step 2: wait for the conversion time estimated from the configured
     * data rate (real timer via HAL_Delay).
     *
     * NOTE: an earlier revision of this driver additionally polled the
     * config register's OS bit to confirm completion before trusting the
     * data (per the ADS1118 datasheet, OS should read 1 once a single-shot
     * conversion is done). That was removed after field testing showed
     * some ADS1118 modules (particularly low-cost/clone breakout boards)
     * do not report the OS bit correctly - it never flips to 1 even
     * though MUX/PGA/MODE/DR are latched correctly and the DATA register
     * genuinely updates every conversion cycle (confirmed by observing
     * real sample-to-sample noise). Relying on OS there caused false
     * ADS1118_ERR_TIMEOUT on otherwise-working hardware. The data-rate
     * delay below is the datasheet's own recommended minimum wait and is
     * reliable across ADS1118 variants. */
    ads1118_delay_for_datarate(ads1118_state.data_rate);

    /* Step 3: clock out the conversion result triggered in step 1. */
    status = ads1118_spi_exchange_retry(cfg, raw, NULL);
    if (status != ADS1118_OK) {
        return status;
    }

    return ADS1118_OK;
}

ads1118_status_t ads1118_read_raw(int16_t *raw)
{
    /* Backward-compatible thin wrapper: always reads the B+/main channel
     * (ads1118_state.mux), same behavior as before this file supported
     * B- as well. */
    return ads1118_read_raw_mux(ads1118_state.mux, raw);
}

ads1118_status_t ads1118_read_battery_voltage(float *voltage)
{
    int16_t raw;
    float measured;
    ads1118_status_t status;

    if (voltage == NULL) {
        return ADS1118_ERR_NULL;
    }
    if (!ads1118_state.initialized) {
        return ADS1118_ERR_CONFIG;
    }

    status = ads1118_read_raw(&raw);
    if (status != ADS1118_OK) {
        return status;
    }

    measured = ads1118_raw_to_voltage(raw, ads1118_state.pga);
    *voltage = measured / ads1118_state.vdivider_ratio;

    return ADS1118_OK;
}

/**
 * @brief Internal: 12-sample mean filter (see ads1118_read_raw_filtered()
 * docs) on an explicitly given MUX setting. Shared by both the B+ and B-
 * filtered reads.
 */
static ads1118_status_t ads1118_read_raw_filtered_mux(uint16_t mux, int16_t *raw_filtered)
{
    int16_t samples[ADS1118_FILTER_SAMPLE_COUNT];
    int32_t sum = 0;
    uint16_t i;
    ads1118_status_t status;

    if (raw_filtered == NULL) {
        return ADS1118_ERR_NULL;
    }
    if (!ads1118_state.initialized) {
        return ADS1118_ERR_CONFIG;
    }

    /* Ambil 12 sampel mentah, satu per satu. */
    for (i = 0; i < ADS1118_FILTER_SAMPLE_COUNT; i++) {
        status = ads1118_read_raw_mux(mux, &samples[i]);
        if (status != ADS1118_OK) {
            return status;
        }
    }

    /* Urutkan lalu buang 1 nilai terkecil (index 0) dan 1 nilai terbesar
     * (index terakhir), sisakan ADS1118_FILTER_AVG_COUNT (10) sampel di
     * tengah untuk dirata-ratakan. */
    ads1118_sort_int16_asc(samples, ADS1118_FILTER_SAMPLE_COUNT);

    for (i = 1; i < (ADS1118_FILTER_SAMPLE_COUNT - 1); i++) {
        sum += samples[i];
    }

    /* Pembulatan ke terdekat (bukan truncation) saat dibagi rata-rata */
    if (sum >= 0) {
        *raw_filtered = (int16_t)((sum + (ADS1118_FILTER_AVG_COUNT / 2)) / ADS1118_FILTER_AVG_COUNT);
    } else {
        *raw_filtered = (int16_t)((sum - (ADS1118_FILTER_AVG_COUNT / 2)) / ADS1118_FILTER_AVG_COUNT);
    }

    return ADS1118_OK;
}

ads1118_status_t ads1118_read_raw_filtered(int16_t *raw_filtered)
{
    /* Backward-compatible thin wrapper: always reads the B+/main channel. */
    return ads1118_read_raw_filtered_mux(ads1118_state.mux, raw_filtered);
}

ads1118_status_t ads1118_read_battery_voltage_filtered(float *voltage)
{
    int16_t raw_filtered;
    float measured;
    ads1118_status_t status;

    if (voltage == NULL) {
        return ADS1118_ERR_NULL;
    }
    if (!ads1118_state.initialized) {
        return ADS1118_ERR_CONFIG;
    }

    status = ads1118_read_raw_filtered(&raw_filtered);
    if (status != ADS1118_OK) {
        return status;
    }

    measured = ads1118_raw_to_voltage(raw_filtered, ads1118_state.pga);
    *voltage = measured / ads1118_state.vdivider_ratio;

    return ADS1118_OK;
}

ads1118_status_t ads1118_read_battery_status(float *battery_voltage,
                                              float *v_bminus,
                                              float *v_inplus,
                                              int *is_charging,
                                              int *low_battery_alert)
{
    int16_t raw_bplus, raw_bminus, raw_inplus;
    float v_plus, v_minus, v_in;
    ads1118_status_t status;
    int charging;

    if (!ads1118_state.initialized) {
        return ADS1118_ERR_CONFIG;
    }

    /* B+ (12-sample filtered) */
    status = ads1118_read_raw_filtered_mux(ads1118_state.mux, &raw_bplus);
    if (status != ADS1118_OK) {
        return status;
    }
    v_plus = ads1118_raw_to_voltage(raw_bplus, ads1118_state.pga) / ads1118_state.vdivider_ratio;

    /* B- (12-sample filtered), separate channel + divider ratio */
    status = ads1118_read_raw_filtered_mux(ads1118_state.mux_bminus, &raw_bminus);
    if (status != ADS1118_OK) {
        return status;
    }
    v_minus = ads1118_raw_to_voltage(raw_bminus, ads1118_state.pga) / ads1118_state.vdivider_ratio_bminus;

    /* IN+ (12-sample filtered), separate channel + divider ratio - used
     * PURELY for charge detection, independent of the battery reading
     * above (per user: IN+ reflects charger presence directly, e.g. USB
     * 5V, regardless of what state the battery itself is in). */
    status = ads1118_read_raw_filtered_mux(ads1118_state.mux_inplus, &raw_inplus);
    if (status != ADS1118_OK) {
        return status;
    }
    v_in = ads1118_raw_to_voltage(raw_inplus, ads1118_state.pga) / ads1118_state.vdivider_ratio_inplus;

    /* Simple threshold-based charge detection on V(IN+) - see
     * ads1118_config_t.charge_detect_threshold_v docs: tune this to sit
     * comfortably between your IN+ divider's "USB present" and "USB
     * absent" readings on your specific board. */
    charging = (v_in > ads1118_state.charge_detect_threshold_v) ? 1 : 0;

    if (battery_voltage != NULL) {
        *battery_voltage = ads1118_apply_calibration(v_plus - v_minus);
    }
    if (v_bminus != NULL) {
        *v_bminus = v_minus;
    }
    if (v_inplus != NULL) {
        *v_inplus = v_in;
    }
    if (is_charging != NULL) {
        *is_charging = charging;
    }
    if (low_battery_alert != NULL) {
        float v_batt_calibrated = ads1118_apply_calibration(v_plus - v_minus);
        *low_battery_alert = (!charging && (v_batt_calibrated < ads1118_state.low_batt_threshold_v)) ? 1 : 0;
    }

    return ADS1118_OK;
}

ads1118_status_t ads1118_deinit(void)
{
    ads1118_state.initialized = 0;
    ads1118_state.hspi = NULL;
    return ADS1118_OK;
}

void ads1118_debug_print_state(void)
{
    printf("[ADS1118] DEBUG state: initialized=%d, hspi=%p, cs_port=%p, cs_pin=%u\r\n",
           ads1118_state.initialized,
           (void *)ads1118_state.hspi,
           (void *)ads1118_state.cs_port,
           (unsigned)ads1118_state.cs_pin);
}
