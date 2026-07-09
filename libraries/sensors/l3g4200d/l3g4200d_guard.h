/*
 * Copyright (C) 2026 ICDeC
 *
 * L3G4200D Guard Timer Workaround
 *
 * Provides a guarded wrapper around l3g4200d_init() that uses a hardware
 * timer interrupt to forcibly clear UDMA I2C channels if the I2C bus
 * hangs (e.g. due to missing ACK, clock stretching, or stuck bus).
 *
 * Usage:
 *   Replace l3g4200d_init(&cfg) with l3g4200d_init_guarded(&cfg)
 *   in your test application.  If the init hangs for longer than
 *   L3G4200D_GUARD_TIMEOUT_MS, the guard timer fires and the call
 *   returns GYRO_ERR_TIMEOUT instead of blocking forever.
 */

#ifndef __L3G4200D_GUARD_H__
#define __L3G4200D_GUARD_H__

#include "l3g4200d.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Guarded version of l3g4200d_init().
 *
 * Calls l3g4200d_init(cfg) with a hardware timer watchdog.
 * If the init function hangs (I2C bus stuck), the timer ISR fires
 * after ~50 ms and clears the UDMA I2C channels, allowing the
 * driver's busy-wait loops to exit naturally.
 *
 * @param cfg  Pointer to sensor configuration (same as l3g4200d_init).
 * @return GYRO_OK on success, GYRO_ERR_TIMEOUT if the guard timer fired,
 *         or any other gyro_status_t returned by l3g4200d_init().
 */
gyro_status_t l3g4200d_init_guarded(const l3g4200d_config_t *cfg);

/**
 * @brief Uji ISR guard timer secara terisolasi, TANPA melibatkan I2C
 * sama sekali. Panggil ini SEBELUM l3g4200d_init_guarded() untuk
 * memastikan rantai hardware timer + IRQ bekerja normal, sehingga
 * kalau nanti l3g4200d_init_guarded() masih hang, kita tahu pasti
 * masalahnya di transaksi I2C, bukan di mekanisme guard-nya sendiri.
 *
 * @return 1 jika ISR terbukti menyala, 0 jika tidak pernah menyala.
 */
int l3g4200d_guard_selftest(void);

#ifdef __cplusplus
}
#endif

#endif /* __L3G4200D_GUARD_H__ */