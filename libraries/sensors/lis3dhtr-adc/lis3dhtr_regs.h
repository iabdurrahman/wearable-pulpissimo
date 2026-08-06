/*
 * lis3dhtr_regs.h
 *
 * Bit & register definitions untuk fitur AUXILIARY ADC LIS3DHTR
 * (STMicroelectronics). Diambil dari datasheet resmi ST (LIS3DH Doc ID
 * 17530 Rev 1 — register map identik dengan LIS3DHTR) dan dikonfirmasi
 * silang dengan forum resmi ST Community perihal bit ADC_PD & input
 * range aux ADC (tidak terdokumentasi rinci di datasheet Rev 1).
 *
 * File ini HANYA berisi register yang dipakai untuk fitur aux ADC
 * (baca tegangan baterai). Register accelerometer (CTRL_REG1 bit
 * Xen/Yen/Zen, OUT_X/Y/Z, dsb.) sengaja tidak dipakai/di-set karena
 * requirement task ini murni baca ADC, bukan accelerometer.
 */

#ifndef LIS3DHTR_REGS_H
#define LIS3DHTR_REGS_H

/* -------------------------------------------------------------------- */
/* I2C slave address (7-bit)                                            */
/* -------------------------------------------------------------------- */
#define LIS3DHTR_I2C_ADDR_SA0_LOW   0x18u  /* SA0 = GND */
#define LIS3DHTR_I2C_ADDR_SA0_HIGH  0x19u  /* SA0 = VCC */

#define LIS3DHTR_WHO_AM_I_VALUE     0x33u

/* -------------------------------------------------------------------- */
/* Register addresses yang dipakai                                      */
/* -------------------------------------------------------------------- */
#define LIS3DHTR_REG_STATUS_AUX     0x07u  /* status data-ready aux ADC  */
#define LIS3DHTR_REG_OUT_ADC1_L     0x08u
#define LIS3DHTR_REG_OUT_ADC1_H     0x09u
#define LIS3DHTR_REG_OUT_ADC2_L     0x0Au
#define LIS3DHTR_REG_OUT_ADC2_H     0x0Bu
#define LIS3DHTR_REG_OUT_ADC3_L     0x0Cu
#define LIS3DHTR_REG_OUT_ADC3_H     0x0Du
#define LIS3DHTR_REG_WHO_AM_I       0x0Fu
/* NOTE: alamat 0x1E adalah "Reserved (do not modify)" menurut datasheet
 * resmi LIS3DH Doc ID 17530 Rev 1 (Table 17) — bukan CTRL_REG0, jadi
 * sengaja tidak didefinisikan di sini. */
#define LIS3DHTR_REG_TEMP_CFG       0x1Fu  /* ADC_PD & TEMP_EN di sini   */
#define LIS3DHTR_REG_CTRL_REG1      0x20u  /* ODR & LPen (resolusi ADC)  */
#define LIS3DHTR_REG_CTRL_REG4      0x23u  /* BDU                        */

/* Bit mask: di-OR-kan ke alamat register saat multi-byte read/write
 * supaya chip auto-increment address setiap byte. */
#define LIS3DHTR_AUTO_INCREMENT_BIT 0x80u

/* -------------------------------------------------------------------- */
/* TEMP_CFG_REG (0x1F) bit fields                                        */
/*   [7] ADC_PD   [6] TEMP_EN   [5:0] reserved (harus 0)                 */
/* Nama "ADC_PD" dari datasheet (Table 22/23); di forum ST Community     */
/* bit yang sama kadang disebut "ADC_EN" — posisi bit (7) & fungsinya    */
/* (enable aux ADC) sama persis, cuma beda penamaan.                     */
/* -------------------------------------------------------------------- */
#define LIS3DHTR_TEMPCFG_TEMP_EN    (1u << 6)
#define LIS3DHTR_TEMPCFG_ADC_PD     (1u << 7)

/* -------------------------------------------------------------------- */
/* CTRL_REG1 (0x20) bit fields                                          */
/*   [7:4] ODR3-ODR0   [3] LPen   [2:0] Zen/Yen/Xen (tidak dipakai)     */
/* -------------------------------------------------------------------- */
#define LIS3DHTR_CTRL1_LPEN         (1u << 3)
#define LIS3DHTR_CTRL1_ODR_SHIFT    4u

/* -------------------------------------------------------------------- */
/* CTRL_REG4 (0x23) bit fields — hanya BDU yang relevan untuk ADC        */
/* -------------------------------------------------------------------- */
#define LIS3DHTR_CTRL4_BDU          (1u << 7)

/* -------------------------------------------------------------------- */
/* STATUS_AUX (0x07) bit fields                                          */
/*   [7] 321OR [6] 3OR [5] 2OR [4] 1OR [3] 321DA [2] 3DA [1] 2DA [0] 1DA */
/* -------------------------------------------------------------------- */
#define LIS3DHTR_STATUSAUX_1DA      (1u << 0)  /* data ADC1 baru siap    */
#define LIS3DHTR_STATUSAUX_2DA      (1u << 1)  /* data ADC2 baru siap    */
#define LIS3DHTR_STATUSAUX_3DA      (1u << 2)  /* data ADC3 baru siap    */
#define LIS3DHTR_STATUSAUX_321DA    (1u << 3)  /* semua channel baru siap*/

#endif /* LIS3DHTR_REGS_H */
