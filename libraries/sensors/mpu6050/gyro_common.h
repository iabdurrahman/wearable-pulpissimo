/*
 * Copyright (C) 2026 ICDeC
 *
 * Gyroscope Sensor Library - Common Definitions
 *
 * Shared data types and structures used by both L3G4200D and MPU-6050
 * gyroscope sensor drivers on the ICDeC PULPissimo FPGA board.
 */

#ifndef __GYRO_COMMON_H__
#define __GYRO_COMMON_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Status Codes
 * ============================================================================ */

/**
 * @brief Return status codes for all gyroscope library functions.
 */
typedef enum {
    GYRO_OK          = 0,   /**< Operation completed successfully */
    GYRO_ERR_I2C     = -1,  /**< I2C communication error */
    GYRO_ERR_ID      = -2,  /**< WHO_AM_I verification failed (wrong device) */
    GYRO_ERR_CONFIG  = -3,  /**< Configuration error (invalid parameter) */
    GYRO_ERR_TIMEOUT = -4,  /**< I2C transaction timed out */
    GYRO_ERR_NULL    = -5   /**< NULL pointer passed as argument */
} gyro_status_t;

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief Raw gyroscope data (16-bit signed integer per axis).
 *
 * Contains the unprocessed sensor register values.
 * To convert to degrees per second (°/s), use the corresponding
 * sensor's read_dps() function or apply the sensitivity factor manually.
 */
typedef struct {
    int16_t x;  /**< Raw angular rate data, X-axis */
    int16_t y;  /**< Raw angular rate data, Y-axis */
    int16_t z;  /**< Raw angular rate data, Z-axis */
} gyro_raw_t;

/**
 * @brief Gyroscope data in degrees per second (°/s).
 *
 * Contains the processed sensor data with sensitivity scaling applied.
 * Values represent angular velocity around each axis.
 */
typedef struct {
    float x;    /**< Angular rate X-axis in °/s */
    float y;    /**< Angular rate Y-axis in °/s */
    float z;    /**< Angular rate Z-axis in °/s */
} gyro_dps_t;

/* ============================================================================
 * Full-Scale Range Enumerations
 * ============================================================================ */

/**
 * @brief Full-scale range options for L3G4200D.
 *
 * Higher range = less sensitivity but wider measurement capability.
 */
typedef enum {
    L3G4200D_RANGE_250DPS  = 0x00,  /**< ±250 °/s  (sensitivity: 8.75 mdps/digit) */
    L3G4200D_RANGE_500DPS  = 0x10,  /**< ±500 °/s  (sensitivity: 17.50 mdps/digit) */
    L3G4200D_RANGE_2000DPS = 0x20   /**< ±2000 °/s (sensitivity: 70.00 mdps/digit) */
} l3g4200d_range_t;

/**
 * @brief Full-scale range options for MPU-6050 gyroscope.
 *
 * Higher range = less sensitivity but wider measurement capability.
 */
typedef enum {
    MPU6050_GYRO_RANGE_250DPS  = 0x00,  /**< ±250 °/s  (sensitivity: 131 LSB/°/s) */
    MPU6050_GYRO_RANGE_500DPS  = 0x08,  /**< ±500 °/s  (sensitivity: 65.5 LSB/°/s) */
    MPU6050_GYRO_RANGE_1000DPS = 0x10,  /**< ±1000 °/s (sensitivity: 32.8 LSB/°/s) */
    MPU6050_GYRO_RANGE_2000DPS = 0x18   /**< ±2000 °/s (sensitivity: 16.4 LSB/°/s) */
} mpu6050_gyro_range_t;

/* ============================================================================
 * Default I2C Configuration
 * ============================================================================ */

#define GYRO_DEFAULT_I2C_ID       0          /**< Default I2C peripheral ID */
#define GYRO_DEFAULT_I2C_BAUDRATE 100000     /**< Default I2C baudrate (100kHz standard mode) */

/** L3G4200D default I2C slave address (SDO pin HIGH) — 7-bit address */
#define L3G4200D_I2C_ADDR_DEFAULT 0x69
/** L3G4200D alternative I2C slave address (SDO pin LOW) — 7-bit address */
#define L3G4200D_I2C_ADDR_ALT     0x68

/** MPU-6050 default I2C slave address (AD0 pin HIGH) — 7-bit address */
#define MPU6050_I2C_ADDR_DEFAULT  0x69
/** MPU-6050 alternative I2C slave address (AD0 pin LOW) — 7-bit address */
#define MPU6050_I2C_ADDR_ALT      0x68

/* ============================================================================
 * Timeout / Retry Configuration
 * ============================================================================ */

#define GYRO_I2C_TIMEOUT_US       5000    /**< I2C hardware timeout (microseconds) */
#define GYRO_I2C_MAX_RETRIES      3       /**< Max retries per I2C operation */
#define GYRO_BOOT_DELAY_CYCLES    50000   /**< Software delay for sensor boot (~5-10ms) */
#define GYRO_RESET_DELAY_CYCLES   100000  /**< Software delay for device reset (~100ms) */
#define GYRO_WAKEUP_DELAY_CYCLES  50000   /**< Software delay for wake-up stabilization */

#ifdef __cplusplus
}
#endif

#endif /* __GYRO_COMMON_H__ */
