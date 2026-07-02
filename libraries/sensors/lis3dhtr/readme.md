# LIS3DHTR Accelerometer Driver

I2C driver for the LIS3DHTR 3-axis accelerometer, written for PULPissimo using pulp-runtime.

---

## Hardware Setup

The LIS3DHTR is already mounted on the board, no external wiring needed. SDO is connected to VCC on-board, giving a 7-bit address of **0x19**.

### I2C Address

| SDO pin   | 7-bit | 8-bit |
|-----------|-------|-------|
| SDO → VCC | 0x19  | 0x32  |
| SDO → GND | 0x18  | 0x30  |

> **Note:** The I2C driver library (`i2c.c`) automatically appends the R/W bit, which effectively left-shifts the address by 1 internally. For this reason, `dev.cs` must be set to the **8-bit address** (`0x32`), not the 7-bit one (`0x19`).
>
> ```c
> dev.cs = LIS3DHTR_ADDR;   // 0x32 — 8-bit format required by the driver
> ```

---

## Default Configuration

| Parameter   | Value      | Note                     |
|-------------|------------|--------------------------|
| ODR         | 100 Hz     | Output data rate         |
| Full-scale  | ±2g        |                          |
| Resolution  | 10-bit     | Right-justified          |
| Sensitivity | 4 mg/digit |                          |
| BDU         | Enabled    | Block data update        |
| Interface   | I2C        | 400 kHz fast mode        |

---

## Data Structure

```c
typedef struct {
    int16_t x_mg;   // X-axis acceleration in milli-g
    int16_t y_mg;   // Y-axis acceleration in milli-g
    int16_t z_mg;   // Z-axis acceleration in milli-g
} accel_data_t;
```

---

## API

### `lis3dhtr_open`

```c
i2c_t *lis3dhtr_open(void);
```

Sets up the I2C peripheral with the correct address, baudrate, and timeout for the LIS3DHTR. Call this before `lis3dhtr_init`.

**Return values:**
- valid `i2c_t *`: success
- `NULL`: I2C peripheral failed to open

---

### `lis3dhtr_init`

```c
int lis3dhtr_init(i2c_t *i2c);
```

Initializes the sensor: checks WHO_AM_I, configures ODR, full-scale, and BDU.

**Parameters:**
- `i2c`: I2C handle from `lis3dhtr_open()`

**Return values:**

| Value | Constant                | Meaning                            |
|-------|-------------------------|------------------------------------|
| 0     | `LIS3DHTR_OK`           | Success                            |
| -2    | `LIS3DHTR_ERR_COMM`     | I2C error reading WHO_AM_I         |
| -4    | `LIS3DHTR_ERR_WHO_AM_I` | Wrong WHO_AM_I (wrong sensor?)     |
| -5    | `LIS3DHTR_ERR_CFG_REG1` | Failed to write CTRL_REG1          |
| -6    | `LIS3DHTR_ERR_CFG_REG4` | Failed to write CTRL_REG4          |

---

### `lis3dhtr_read_accel`

```c
int lis3dhtr_read_accel(i2c_t *i2c, accel_data_t *out);
```

Reads acceleration data from all three axes. Polls the STATUS register until new data is ready (max 1000 iterations) before reading.

**Parameters:**
- `i2c`: same I2C handle from `lis3dhtr_open()`
- `out`: pointer to `accel_data_t` to store the result

**Return values:**

| Value | Constant              | Meaning                               |
|-------|-----------------------|---------------------------------------|
| 0     | `LIS3DHTR_OK`         | Success, `out` contains valid data    |
| -7    | `LIS3DHTR_ERR_STATUS` | STATUS read failed or ZYXDA timeout   |
| -8    | `LIS3DHTR_ERR_READ`   | Burst read of output registers failed |

---

## Usage Example

```c
#include "pulp.h"
#include "lis3dhtr.h"

int main(void)
{
    // 1. Open I2C
    i2c_t *i2c = lis3dhtr_open();
    if (i2c == NULL) return -1;

    // 2. Init sensor
    if (lis3dhtr_init(i2c) != LIS3DHTR_OK) {
        i2c_close(i2c);
        return -1;
    }

    // 3. Read data
    accel_data_t accel;
    while (1) {
        if (lis3dhtr_read_accel(i2c, &accel) == LIS3DHTR_OK) {
            printf("X=%dmg Y=%dmg Z=%dmg\n\r",
                   accel.x_mg, accel.y_mg, accel.z_mg);
        }
    }

    i2c_close(i2c);
    return 0;
}
```
---

## Notes

**Output unit is milli-g (mg):**
Acceleration values are returned as `int16_t` in milli-g, where 1000mg = 1g = 9.81 m/s². This means a sensor lying flat and still should read Z ≈ 1000mg (gravity), with X and Y near 0mg. 

**Burst read with `| 0x80`:**
LIS3DHTR uses bit 7 of the register address as an auto-increment flag for multi-byte I2C reads. Without it, all 6 bytes would be read from the same register.

**Output resolution:**
Configured in normal mode (10-bit, right-justified). Raw data is right-shifted 6 bits before applying sensitivity. High-resolution mode (12-bit) is available but not enabled by default.

> **Note:** Sample-to-sample variation of ±50mg is normal sensor noise in normal mode.
