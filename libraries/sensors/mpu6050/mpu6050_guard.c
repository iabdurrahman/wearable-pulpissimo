/*
 * Copyright (C) 2026 ICDeC
 *
 * MPU-6050 Guard Timer Workaround
 *
 * This file implements a hardware-timer-based watchdog that wraps
 * mpu6050_init().  When the I2C bus hangs (no ACK, UDMA TX/RX stuck,
 * etc.) the timer ISR fires and forcibly clears the UDMA I2C channels
 * so that the blocking plp_udma_busy() loops in i2c_write()/i2c_read()
 * can exit.  After mpu6050_init() returns, the guard checks whether
 * a timeout occurred and returns GYRO_ERR_TIMEOUT.
 *
 * Why not longjmp from ISR?
 *   - longjmp from an ISR leaves mstatus.MIE = 0 (interrupts disabled)
 *     because mret is never executed.
 *   - i2c_read() line 184 has a while(plp_udma_busy(TX)) with NO timeout,
 *     so the only way to break that loop is to clear the UDMA channel.
 *
 * Keep this in a separate file so the original mpu6050.c stays clean.
 */

#include <stdio.h>
#include "pulp.h"
#include "hal/timer/timer_v2.h"
#include "hal/udma/i2c/udma_i2c_v2.h"
#include "mpu6050.h"

/* ============================================================================
 * Configuration
 * ============================================================================ */

/** Which timer channel to use as the guard timer.
 *  POS_TICK uses TIMER_HI (POS_TICK_TIMER_ID=1), so we use TIMER_LO. */
#define MPU6050_GUARD_TIMER_ID      TIMER_LO

/** How many milliseconds to wait before declaring a timeout */
#define MPU6050_GUARD_TIMEOUT_MS    50

/** IRQ line for TIMER_LO on PULPissimo */
#define MPU6050_GUARD_TIMER_IRQ     ARCHI_FC_EVT_TIMER0_LO   /* = 10 */

/** I2C peripheral ID used by the MPU-6050 driver (always 0) */
#define MPU6050_I2C_PERIPH_ID       0

/* ============================================================================
 * Private State
 * ============================================================================ */

static volatile int    mpu6050_timeout_flag = 0;
static unsigned int    guard_timer_addr;
static int             guard_timer_initialized = 0;

/* ============================================================================
 * Timer ISR
 * ============================================================================ */

/**
 * @brief ISR invoked when the guard timer expires.
 *
 * Instead of using longjmp (which would leave interrupts disabled since
 * mret is skipped), this ISR:
 *  1. Stops the guard timer
 *  2. Sets the timeout flag
 *  3. Clears UDMA I2C TX and RX channels to unblock plp_udma_busy() loops
 *  4. Resets the I2C peripheral via clock gate cycle
 *
 * After the ISR returns (via the normal mret path), the i2c_write()/
 * i2c_read() polling loop will see that the UDMA channel is no longer
 * busy and will return.  mpu6050_init() will then propagate errors
 * back to mpu6050_init_guarded(), which checks mpu6050_timeout_flag.
 */
static void mpu6050_timer_isr(void)
{
    /* Stop the guard timer so it doesn't fire again */
    timer_stop(guard_timer_addr, MPU6050_GUARD_TIMER_ID);
    timer_disable_irq(guard_timer_addr, MPU6050_GUARD_TIMER_ID);

    /* Signal timeout to the wrapper */
    mpu6050_timeout_flag = 1;

    /* Clear UDMA I2C channels to unblock the busy-wait loops */
    plp_i2c_tx_clear(MPU6050_I2C_PERIPH_ID);
    plp_i2c_rx_clear(MPU6050_I2C_PERIPH_ID);

    /* Reset I2C peripheral via UDMA clock gate cycle */
    {
        int actual_periph_id = ARCHI_UDMA_I2C_ID(MPU6050_I2C_PERIPH_ID);

        /* Turn off UDMA peripheral clock gate */
        plp_udma_cg_set(plp_udma_cg_get() & ~(1 << actual_periph_id));

        /* Short delay for reset to take effect */
        for (volatile int j = 0; j < 10; j++);

        /* Turn on UDMA peripheral clock gate */
        plp_udma_cg_set(plp_udma_cg_get() | (1 << actual_periph_id));
    }
}

/* ============================================================================
 * Guard Timer Helpers
 * ============================================================================ */

/**
 * @brief One-time initialisation of the guard timer hardware & IRQ.
 */
static void mpu6050_guard_init(void)
{
    if (guard_timer_initialized) return;

    guard_timer_addr = timer_base_fc(0, 0);

    /* Register the ISR and unmask the timer IRQ */
    rt_irq_set_handler(MPU6050_GUARD_TIMER_IRQ, mpu6050_timer_isr);
    rt_irq_mask_set(1 << MPU6050_GUARD_TIMER_IRQ);

    /* Use the reference clock directly (no prescaler) */
    timer_set_clock_source(guard_timer_addr, MPU6050_GUARD_TIMER_ID, false);
    timer_set_prescaler(guard_timer_addr, MPU6050_GUARD_TIMER_ID, false, 0);

    guard_timer_initialized = 1;
}

/**
 * @brief Arm (start) the guard timer.
 *
 * The compare value is computed assuming a 10 MHz reference clock.
 * Adjust if your SoC uses a different frequency.
 */
static void mpu6050_arm_guard_timer(void)
{
    /* 10 MHz → 10 000 ticks/ms */
    uint32_t compare_value = (10000000UL / 1000UL) * MPU6050_GUARD_TIMEOUT_MS;

    timer_reset(guard_timer_addr, MPU6050_GUARD_TIMER_ID);
    timer_count_set(guard_timer_addr, MPU6050_GUARD_TIMER_ID, 0);
    timer_cmp_set(guard_timer_addr, MPU6050_GUARD_TIMER_ID, compare_value);
    timer_set_continuity(guard_timer_addr, MPU6050_GUARD_TIMER_ID, true, true);
    timer_enable_irq(guard_timer_addr, MPU6050_GUARD_TIMER_ID);
    timer_start(guard_timer_addr, MPU6050_GUARD_TIMER_ID);
}

/**
 * @brief Disarm (stop) the guard timer after a successful return.
 */
static void mpu6050_disarm_guard_timer(void)
{
    timer_stop(guard_timer_addr, MPU6050_GUARD_TIMER_ID);
    timer_disable_irq(guard_timer_addr, MPU6050_GUARD_TIMER_ID);
}

/* ============================================================================
 * Public API
 * ============================================================================ */

/**
 * @brief Guarded wrapper around mpu6050_init().
 *
 * Arms a hardware guard timer, then calls mpu6050_init().
 * If the I2C bus hangs, the guard timer ISR fires and clears the
 * UDMA channels, allowing the I2C driver's busy-wait loops to exit.
 * After mpu6050_init() returns, we check whether the timeout flag
 * was set and return GYRO_ERR_TIMEOUT if so.
 */
gyro_status_t mpu6050_init_guarded(const mpu6050_config_t *cfg)
{
    gyro_status_t status;

    mpu6050_guard_init();

    /* Reset timeout flag */
    mpu6050_timeout_flag = 0;

    /* Arm the guard timer */
    mpu6050_arm_guard_timer();

    /* Call the real init — may hang if I2C bus is stuck */
    status = mpu6050_init(cfg);

    /* Disarm the guard timer (no-op if ISR already stopped it) */
    mpu6050_disarm_guard_timer();

    /* Check if the guard timer fired during init */
    if (mpu6050_timeout_flag) {
        printf("[MPU6050][GUARD] mpu6050_init timed out (guard timer forced UDMA clear)\n");
        return GYRO_ERR_TIMEOUT;
    }

    return status;
}
