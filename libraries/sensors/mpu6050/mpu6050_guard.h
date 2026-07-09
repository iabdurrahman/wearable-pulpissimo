/*
 * Copyright (C) 2026 ICDeC
 *
 * MPU-6050 Guard Timer Workaround
 *
 * Provides a guarded wrapper around mpu6050_init() that uses a hardware
 * timer interrupt to forcibly clear UDMA I2C channels if the I2C bus
 * hangs (e.g. due to missing ACK, clock stretching, or stuck bus).
 *
 * Usage:
 *   Replace mpu6050_init(&cfg) with mpu6050_init_guarded(&cfg)
 *   in your test application.  If the init hangs for longer than
 *   MPU6050_GUARD_TIMEOUT_MS, the guard timer fires and the call
 *   returns GYRO_ERR_TIMEOUT instead of blocking forever.
 */

#ifndef __MPU6050_GUARD_H__
#define __MPU6050_GUARD_H__

#include "mpu6050.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Guarded version of mpu6050_init().
 *
 * Calls mpu6050_init(cfg) with a hardware timer watchdog.
 * If the init function hangs (I2C bus stuck), the timer ISR fires
 * after ~50 ms and clears the UDMA I2C channels, allowing the
 * driver's busy-wait loops to exit naturally.
 *
 * @param cfg  Pointer to sensor configuration (same as mpu6050_init).
 * @return GYRO_OK on success, GYRO_ERR_TIMEOUT if the guard timer fired,
 *         or any other gyro_status_t returned by mpu6050_init().
 */
gyro_status_t mpu6050_init_guarded(const mpu6050_config_t *cfg);

#ifdef __cplusplus
}
#endif

#endif /* __MPU6050_GUARD_H__ */
