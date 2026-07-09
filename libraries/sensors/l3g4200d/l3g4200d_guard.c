/*
 * Copyright (C) 2026 ICDeC
 *
 * L3G4200D Guard Timer Workaround
 *
 * This file implements a hardware-timer-based watchdog that wraps
 * l3g4200d_init().  When the I2C bus hangs (no ACK, UDMA TX/RX stuck,
 * etc.) the timer ISR fires and forcibly clears the UDMA I2C channels
 * so that the blocking plp_udma_busy() loops in i2c_write()/i2c_read()
 * can exit.  After l3g4200d_init() returns, the guard checks whether
 * a timeout occurred and returns GYRO_ERR_TIMEOUT.
 *
 * Why not longjmp from ISR?
 *   - longjmp from an ISR leaves mstatus.MIE = 0 (interrupts disabled)
 *     because mret is never executed.
 *   - i2c_read() line 184 has a while(plp_udma_busy(TX)) with NO timeout,
 *     so the only way to break that loop is to clear the UDMA channel.
 *
 * Keep this in a separate file so the original l3g4200d.c stays clean.
 */

#include <stdio.h>
#include "pulp.h"
#include "hal/timer/timer_v2.h"
#include "hal/udma/i2c/udma_i2c_v2.h"
#include "l3g4200d.h"

/* ============================================================================
 * Configuration
 * ============================================================================ */

/** Which timer channel to use as the guard timer.
 *  POS_TICK uses TIMER_HI (POS_TICK_TIMER_ID=1), so we use TIMER_LO. */
#define L3G4200D_GUARD_TIMER_ID      TIMER_LO

/** How many milliseconds to wait before declaring a timeout */
#define L3G4200D_GUARD_TIMEOUT_MS    50

/** IRQ line for TIMER_LO on PULPissimo */
#define L3G4200D_GUARD_TIMER_IRQ     ARCHI_FC_EVT_TIMER0_LO   /* = 10 */

/** I2C peripheral ID used by the L3G4200D driver (always 0) */
#define L3G4200D_I2C_PERIPH_ID       0

/* ============================================================================
 * Private State
 * ============================================================================ */

static volatile int    l3g4200d_timeout_flag = 0;
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
 * busy and will return.  l3g4200d_init() will then propagate errors
 * back to l3g4200d_init_guarded(), which checks l3g4200d_timeout_flag.
 *
 * NOTE (diagnostik): printf ditambahkan di sini SEMENTARA untuk
 * memastikan ISR ini benar-benar dipanggil hardware. Kalau baris
 * "[ISR] guard timer fired!" tidak pernah muncul di layar meski hang
 * terjadi, itu bukti kuat ISR tidak pernah tereksekusi (masalah di
 * rantai IRQ/timer, bukan di I2C). Hapus print ini lagi setelah
 * diagnosis selesai supaya ISR tetap ringan.
 */
static void l3g4200d_timer_isr(void)
{
    /* Stop the guard timer so it doesn't fire again */
    timer_stop(guard_timer_addr, L3G4200D_GUARD_TIMER_ID);
    timer_disable_irq(guard_timer_addr, L3G4200D_GUARD_TIMER_ID);

    /* Signal timeout to the wrapper */
    l3g4200d_timeout_flag = 1;

    /* --- DIAGNOSTIK SEMENTARA --- */
    printf("[ISR] guard timer fired!\n");
    /* --- akhir diagnostik --- */

    /* Clear UDMA I2C channels to unblock the busy-wait loops */
    plp_i2c_tx_clear(L3G4200D_I2C_PERIPH_ID);
    plp_i2c_rx_clear(L3G4200D_I2C_PERIPH_ID);

    /* Reset I2C peripheral via UDMA clock gate cycle */
    {
        int actual_periph_id = ARCHI_UDMA_I2C_ID(L3G4200D_I2C_PERIPH_ID);

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
static void l3g4200d_guard_init(void)
{
    if (guard_timer_initialized) return;

    guard_timer_addr = timer_base_fc(0, 0);

    /* Register the ISR and unmask the timer IRQ */
    rt_irq_set_handler(L3G4200D_GUARD_TIMER_IRQ, l3g4200d_timer_isr);
    rt_irq_mask_set(1 << L3G4200D_GUARD_TIMER_IRQ);

    /* Use the reference clock directly (no prescaler) */
    timer_set_clock_source(guard_timer_addr, L3G4200D_GUARD_TIMER_ID, false);
    timer_set_prescaler(guard_timer_addr, L3G4200D_GUARD_TIMER_ID, false, 0);

    guard_timer_initialized = 1;
}

/**
 * @brief Arm (start) the guard timer.
 *
 * The compare value is computed assuming a 10 MHz reference clock.
 * Adjust if your SoC uses a different frequency.
 */
static void l3g4200d_arm_guard_timer(void)
{
    /* 10 MHz → 10 000 ticks/ms */
    uint32_t compare_value = (10000000UL / 1000UL) * L3G4200D_GUARD_TIMEOUT_MS;

    timer_reset(guard_timer_addr, L3G4200D_GUARD_TIMER_ID);
    timer_count_set(guard_timer_addr, L3G4200D_GUARD_TIMER_ID, 0);
    timer_cmp_set(guard_timer_addr, L3G4200D_GUARD_TIMER_ID, compare_value);
    timer_set_continuity(guard_timer_addr, L3G4200D_GUARD_TIMER_ID, true, true);
    timer_enable_irq(guard_timer_addr, L3G4200D_GUARD_TIMER_ID);
    timer_start(guard_timer_addr, L3G4200D_GUARD_TIMER_ID);
}

/**
 * @brief Disarm (stop) the guard timer after a successful return.
 */
static void l3g4200d_disarm_guard_timer(void)
{
    timer_stop(guard_timer_addr, L3G4200D_GUARD_TIMER_ID);
    timer_disable_irq(guard_timer_addr, L3G4200D_GUARD_TIMER_ID);
}

/* ============================================================================
 * Diagnostic Self-Test (TIDAK menyentuh I2C sama sekali)
 * ============================================================================ */

/**
 * @brief Uji apakah guard timer + ISR benar-benar menyala, TANPA
 * melibatkan I2C sama sekali. Tujuannya mengisolasi apakah masalah
 * hang ada di rantai timer/interrupt, atau memang murni di transaksi I2C.
 *
 * Cara kerja: arm timer dengan timeout ~50ms, lalu masuk ke busy-wait
 * MURNI (bukan I2C sama sekali) yang jauh lebih panjang dari itu.
 * Kalau l3g4200d_timeout_flag berubah jadi 1 di tengah busy-wait,
 * berarti ISR benar-benar tereksekusi oleh hardware. Kalau loop
 * selesai penuh tanpa flag pernah berubah, berarti rantai timer/IRQ
 * yang bermasalah -- BUKAN soal komunikasi I2C ke sensor.
 *
 * @return 1 jika ISR terbukti menyala, 0 jika tidak pernah menyala.
 */
int l3g4200d_guard_selftest(void)
{
    l3g4200d_guard_init();

    printf("[GUARD SELFTEST] Mulai, TANPA melibatkan I2C sama sekali...\n");

    l3g4200d_timeout_flag = 0;
    l3g4200d_arm_guard_timer(); /* seharusnya fire dalam ~50ms */

    uint32_t t_start = pos_tick_get_counter_us();
    int fired = 0;

    /* Busy-wait murni, cek flag tiap iterasi supaya begitu ISR mengubah
     * flag, kita langsung tahu dan bisa keluar sedini mungkin. */
    for (volatile long i = 0; i < 500000000L; i++) {
        if (l3g4200d_timeout_flag) {
            fired = 1;
            break;
        }
    }

    uint32_t t_end = pos_tick_get_counter_us();

    l3g4200d_disarm_guard_timer();

    printf("[GUARD SELFTEST] fired=%d, elapsed_us=%u\n", fired, t_end - t_start);

    if (fired) {
        printf("[GUARD SELFTEST] PASS: ISR guard timer bekerja normal.\n");
    } else {
        printf("[GUARD SELFTEST] FAIL: ISR TIDAK PERNAH MENYALA.\n");
        printf("[GUARD SELFTEST]       Masalah ada di rantai timer/IRQ,\n");
        printf("[GUARD SELFTEST]       BUKAN di transaksi I2C.\n");
    }

    return fired;
}

/* ============================================================================
 * Public API
 * ============================================================================ */

/**
 * @brief Guarded wrapper around l3g4200d_init().
 *
 * Arms a hardware guard timer, then calls l3g4200d_init().
 * If the I2C bus hangs, the guard timer ISR fires and clears the
 * UDMA channels, allowing the I2C driver's busy-wait loops to exit.
 * After l3g4200d_init() returns, we check whether the timeout flag
 * was set and return GYRO_ERR_TIMEOUT if so.
 */
gyro_status_t l3g4200d_init_guarded(const l3g4200d_config_t *cfg)
{
    gyro_status_t status;

    l3g4200d_guard_init();

    /* Reset timeout flag */
    l3g4200d_timeout_flag = 0;

    /* Arm the guard timer */
    l3g4200d_arm_guard_timer();

    /* Call the real init — may hang if I2C bus is stuck */
    status = l3g4200d_init(cfg);

    /* Disarm the guard timer (no-op if ISR already stopped it) */
    l3g4200d_disarm_guard_timer();

    /* Check if the guard timer fired during init */
    if (l3g4200d_timeout_flag) {
        printf("[L3G4200D][GUARD] l3g4200d_init timed out (guard timer forced UDMA clear)\n");
        return GYRO_ERR_TIMEOUT;
    }

    return status;
}