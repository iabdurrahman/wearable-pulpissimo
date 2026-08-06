/*
 * Copyright (C) 2026 ICDeC
 *
 * @file ads1118_regs.h
 * @brief ADS1118 Config Register Bit Definitions
 *
 * Bit-field definitions for the ADS1118 16-bit Config Register.
 * Reference: Texas Instruments ADS1118 datasheet (SBAS457A, Oct 2010 - Rev July 2011)
 */
#ifndef __ADS1118_REGS_H__
#define __ADS1118_REGS_H__

/* -------------------------------------------------------------------------
 * Config Register bit positions (16-bit word)
 * ------------------------------------------------------------------------- */
#define ADS1118_OS_SHIFT        15
#define ADS1118_MUX_SHIFT       12
#define ADS1118_PGA_SHIFT        9
#define ADS1118_MODE_SHIFT       8
#define ADS1118_DR_SHIFT         5
#define ADS1118_TS_MODE_SHIFT    4
#define ADS1118_PULLUP_SHIFT     3
#define ADS1118_NOP_SHIFT        1
/* Bit 0 = CNV_RDY_FL (read-only status flag) */

/* -------------------------------------------------------------------------
 * OS: Operational status / single-shot conversion start (bit 15)
 * ------------------------------------------------------------------------- */
#define ADS1118_OS_START        (0x1 << ADS1118_OS_SHIFT)  /* write: begin single conversion */

/* -------------------------------------------------------------------------
 * MUX[2:0]: Input multiplexer configuration (bits 14:12)
 * ------------------------------------------------------------------------- */
#define ADS1118_MUX_DIFF_0_1    (0x0 << ADS1118_MUX_SHIFT)  /* AINP=AIN0, AINN=AIN1 (chip default) */
#define ADS1118_MUX_DIFF_0_3    (0x1 << ADS1118_MUX_SHIFT)  /* AINP=AIN0, AINN=AIN3 */
#define ADS1118_MUX_DIFF_1_3    (0x2 << ADS1118_MUX_SHIFT)  /* AINP=AIN1, AINN=AIN3 */
#define ADS1118_MUX_DIFF_2_3    (0x3 << ADS1118_MUX_SHIFT)  /* AINP=AIN2, AINN=AIN3 */
#define ADS1118_MUX_AIN0_GND    (0x4 << ADS1118_MUX_SHIFT)  /* single-ended: AIN0 vs GND */
#define ADS1118_MUX_AIN1_GND    (0x5 << ADS1118_MUX_SHIFT)  /* single-ended: AIN1 vs GND */
#define ADS1118_MUX_AIN2_GND    (0x6 << ADS1118_MUX_SHIFT)  /* single-ended: AIN2 vs GND */
#define ADS1118_MUX_AIN3_GND    (0x7 << ADS1118_MUX_SHIFT)  /* single-ended: AIN3 vs GND */

/* -------------------------------------------------------------------------
 * PGA[2:0]: Programmable gain amplifier / full-scale range (bits 11:9)
 * ------------------------------------------------------------------------- */
#define ADS1118_PGA_FS_6144     (0x0 << ADS1118_PGA_SHIFT)  /* +-6.144V */
#define ADS1118_PGA_FS_4096     (0x1 << ADS1118_PGA_SHIFT)  /* +-4.096V */
#define ADS1118_PGA_FS_2048     (0x2 << ADS1118_PGA_SHIFT)  /* +-2.048V (chip default) */
#define ADS1118_PGA_FS_1024     (0x3 << ADS1118_PGA_SHIFT)  /* +-1.024V */
#define ADS1118_PGA_FS_0512     (0x4 << ADS1118_PGA_SHIFT)  /* +-0.512V */
#define ADS1118_PGA_FS_0256     (0x5 << ADS1118_PGA_SHIFT)  /* +-0.256V */

/* -------------------------------------------------------------------------
 * MODE: Device operating mode (bit 8)
 * ------------------------------------------------------------------------- */
#define ADS1118_MODE_CONTINUOUS  (0x0 << ADS1118_MODE_SHIFT)
#define ADS1118_MODE_SINGLESHOT  (0x1 << ADS1118_MODE_SHIFT)  /* chip default */

/* -------------------------------------------------------------------------
 * DR[2:0]: Data rate (bits 7:5)
 * ------------------------------------------------------------------------- */
#define ADS1118_DR_8SPS         (0x0 << ADS1118_DR_SHIFT)
#define ADS1118_DR_16SPS        (0x1 << ADS1118_DR_SHIFT)
#define ADS1118_DR_32SPS        (0x2 << ADS1118_DR_SHIFT)
#define ADS1118_DR_64SPS        (0x3 << ADS1118_DR_SHIFT)
#define ADS1118_DR_128SPS       (0x4 << ADS1118_DR_SHIFT)   /* chip default */
#define ADS1118_DR_250SPS       (0x5 << ADS1118_DR_SHIFT)
#define ADS1118_DR_475SPS       (0x6 << ADS1118_DR_SHIFT)
#define ADS1118_DR_860SPS       (0x7 << ADS1118_DR_SHIFT)

/* -------------------------------------------------------------------------
 * TS_MODE: Temperature sensor mode (bit 4)
 * ------------------------------------------------------------------------- */
#define ADS1118_TS_MODE_ADC     (0x0 << ADS1118_TS_MODE_SHIFT)  /* chip default */
#define ADS1118_TS_MODE_TEMP    (0x1 << ADS1118_TS_MODE_SHIFT)

/* -------------------------------------------------------------------------
 * PULL_UP_EN: DOUT pin pull-up enable (bit 3)
 * ------------------------------------------------------------------------- */
#define ADS1118_PULLUP_DISABLE  (0x0 << ADS1118_PULLUP_SHIFT)  /* chip default */
#define ADS1118_PULLUP_ENABLE   (0x1 << ADS1118_PULLUP_SHIFT)

/* -------------------------------------------------------------------------
 * NOP[1:0] (bits 2:1): must be written as '01' for the Config Register to
 * actually be updated. Any other value results in a NOP (no update).
 * ------------------------------------------------------------------------- */
#define ADS1118_NOP_VALID       (0x1 << ADS1118_NOP_SHIFT)  /* bits[2:1] = 01 */

/* Bit 0 (CNV_RDY_FL on read). Write matching the reset default (1). */
#define ADS1118_RESERVED_BIT0   (0x1 << 0)

/* -------------------------------------------------------------------------
 * Full-scale range values (Volts), one per PGA setting
 * ------------------------------------------------------------------------- */
#define ADS1118_FS_VOLTAGE_6144   6.144f
#define ADS1118_FS_VOLTAGE_4096   4.096f
#define ADS1118_FS_VOLTAGE_2048   2.048f
#define ADS1118_FS_VOLTAGE_1024   1.024f
#define ADS1118_FS_VOLTAGE_0512   0.512f
#define ADS1118_FS_VOLTAGE_0256   0.256f

/* Reset/power-up default Config Register value (per datasheet, Table 6) */
#define ADS1118_DEFAULT_CONFIG   0x8583

#endif /* __ADS1118_REGS_H__ */
