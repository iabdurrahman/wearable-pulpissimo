# L3G4200D & MPU-6050 Gyroscope Sensor Library

Driver libraries for the L3G4200D and MPU-6050 gyroscope sensors on the ICDeC FPGA board (PULPissimo + Intel Cyclone 10), built on top of the [pulp-runtime](https://github.com/iabdurrahman/pulp-runtime.git) I2C driver.

## Features

- Simple and consistent API for both sensors
- Read raw gyroscope data (16-bit) and in °/s
- Flexible full-scale range configuration
- Error handling with clear status codes

## Struktur Proyek

<img width="348" height="678" alt="tree" src="https://github.com/user-attachments/assets/4295cb81-156b-4e95-bc7b-dc37f8ad1907" />


## Cara Build & Run

### Unit Test (tanpa hardware)

Verifikasi logika library di PC menggunakan Mock I2C:

```bash
cd gyro_lib/test

# L3G4200D
gcc -o unit_test_l3g4200d unit_test_l3g4200d.c -lm -I../include -Imock
./unit_test_l3g4200d

# MPU-6050
gcc -o unit_test_mpu6050 unit_test_mpu6050.c -lm -I../include -Imock
./unit_test_mpu6050
```

### Hardware Test (FPGA)

Build dan jalankan di board ICDeC:

```bash
# 1. Setup environment
export PATH="$PATH:/home/tetandclee/ICDeC/chroot/bin"
source recurse-submodules/configs/pulpissimo.sh

# 2. Build
cd gyro_lib/test
make all SENSOR=l3g4200d        # atau SENSOR=mpu6050

# 3. Run di FPGA
make run SENSOR=l3g4200d platform=fpga

# 4. Clean
rm -rf build
```

## Quick Start

### L3G4200D

```c
#include "l3g4200d.h"

int main() {
    l3g4200d_config_t cfg;
    gyro_dps_t dps;

    l3g4200d_default_config(&cfg);       // Load default config
    l3g4200d_init(&cfg);                 // Init sensor + verify WHO_AM_I
    l3g4200d_read_dps(&dps);             // Baca data dalam °/s
    printf("X=%.2f Y=%.2f Z=%.2f\n", dps.x, dps.y, dps.z);
    l3g4200d_deinit();                   // Power down + close I2C
}
```

### MPU-6050

```c
#include "mpu6050.h"

int main() {
    mpu6050_config_t cfg;
    gyro_dps_t dps;

    mpu6050_default_config(&cfg);        // Load default config
    mpu6050_init(&cfg);                  // Wake + verify WHO_AM_I
    mpu6050_read_gyro_dps(&dps);         // Baca data dalam °/s
    printf("X=%.2f Y=%.2f Z=%.2f\n", dps.x, dps.y, dps.z);
    mpu6050_deinit();                    // Sleep + close I2C
}
```

## API Documentation

### L3G4200D API

#### Configuration Structure
**`l3g4200d_config_t`**
Configuration structure for L3G4200D initialization.
Set these fields before calling `l3g4200d_init()`.
Use `l3g4200d_default_config()` to get a sane default configuration.

#### API Functions

**`gyro_status_t l3g4200d_default_config(l3g4200d_config_t *cfg);`**
Fill a config struct with default values.
Defaults:
- I2C address: 0x68 (SDO pin LOW)
- I2C ID: 0
- I2C frequency: 100kHz
- Range: ±250 °/s
- **Parameters**: `[out] cfg` - Pointer to configuration struct to fill.
- **Returns**: `GYRO_OK` on success, `GYRO_ERR_NULL` if cfg is NULL.

**`gyro_status_t l3g4200d_init(const l3g4200d_config_t *cfg);`**
Initialize the L3G4200D sensor.
Opens the I2C bus, verifies the WHO_AM_I register, and configures the sensor with the specified settings (power on, all axes enabled).
- **Parameters**: `[in] cfg` - Pointer to configuration struct.
- **Returns**: `GYRO_OK` on success, or an error code (`GYRO_ERR_NULL`, `GYRO_ERR_I2C`, `GYRO_ERR_ID`, `GYRO_ERR_CONFIG`).

**`gyro_status_t l3g4200d_who_am_i(uint8_t *id);`**
Read the WHO_AM_I register.
Useful for verifying the sensor is connected and responsive. Expected value: `0xD3`.
- **Parameters**: `[out] id` - Pointer to store the WHO_AM_I value.
- **Returns**: `GYRO_OK` on success, `GYRO_ERR_I2C` on communication error.

**`gyro_status_t l3g4200d_read_raw(gyro_raw_t *raw);`**
Read raw gyroscope data from all three axes.
Returns unscaled 16-bit signed values directly from the sensor registers.
- **Parameters**: `[out] raw` - Pointer to `gyro_raw_t` struct to store the data.
- **Returns**: `GYRO_OK` on success, `GYRO_ERR_I2C` on communication error.

**`gyro_status_t l3g4200d_read_dps(gyro_dps_t *dps);`**
Read gyroscope data in degrees per second (°/s).
Reads raw data and applies the sensitivity factor based on the currently configured full-scale range.
- **Parameters**: `[out] dps` - Pointer to `gyro_dps_t` struct to store the data.
- **Returns**: `GYRO_OK` on success, `GYRO_ERR_I2C` on communication error.

**`gyro_status_t l3g4200d_set_range(l3g4200d_range_t range);`**
Change the full-scale range.
Can be called after initialization to dynamically change the range.
- **Parameters**: `[in] range` - New full-scale range selection.
- **Returns**: `GYRO_OK` on success, `GYRO_ERR_I2C` on communication error.

**`gyro_status_t l3g4200d_deinit(void);`**
De-initialize the L3G4200D sensor.
Powers down the sensor and closes the I2C bus.
- **Returns**: `GYRO_OK` on success.

### MPU-6050 API

#### Configuration Structure
**`mpu6050_config_t`**
Configuration structure for MPU-6050 initialization.
Set these fields before calling `mpu6050_init()`.
Use `mpu6050_default_config()` to get a sane default configuration.

#### API Functions

**`gyro_status_t mpu6050_default_config(mpu6050_config_t *cfg);`**
Fill a config struct with default values.
Defaults:
- I2C address: 0x68 (AD0 pin LOW)
- I2C ID: 0
- I2C frequency: 100kHz
- Gyro range: ±250 °/s
- DLPF: 0 (disabled, gyro output rate = 8kHz)
- Sample rate divider: 79 (sample rate = 8000 / (1+79) = 100Hz)
- **Parameters**: `[out] cfg` - Pointer to configuration struct to fill.
- **Returns**: `GYRO_OK` on success, `GYRO_ERR_NULL` if cfg is NULL.

**`gyro_status_t mpu6050_init(const mpu6050_config_t *cfg);`**
Initialize the MPU-6050 sensor.
Opens the I2C bus, wakes the sensor from sleep mode, verifies the WHO_AM_I register, and configures the gyroscope with the specified settings.
- **Parameters**: `[in] cfg` - Pointer to configuration struct.
- **Returns**: `GYRO_OK` on success, or an error code (`GYRO_ERR_NULL`, `GYRO_ERR_I2C`, `GYRO_ERR_ID`, `GYRO_ERR_CONFIG`).

**`gyro_status_t mpu6050_who_am_i(uint8_t *id);`**
Read the WHO_AM_I register.
Useful for verifying the sensor is connected and responsive. Expected value: `0x68`.
- **Parameters**: `[out] id` - Pointer to store the WHO_AM_I value.
- **Returns**: `GYRO_OK` on success, `GYRO_ERR_I2C` on communication error.

**`gyro_status_t mpu6050_read_gyro_raw(gyro_raw_t *raw);`**
Read raw gyroscope data from all three axes.
Returns unscaled 16-bit signed values directly from the sensor registers.
- **Parameters**: `[out] raw` - Pointer to `gyro_raw_t` struct to store the data.
- **Returns**: `GYRO_OK` on success, `GYRO_ERR_I2C` on communication error.

**`gyro_status_t mpu6050_read_gyro_dps(gyro_dps_t *dps);`**
Read gyroscope data in degrees per second (°/s).
Reads raw data and applies the sensitivity factor based on the currently configured full-scale range.
- **Parameters**: `[out] dps` - Pointer to `gyro_dps_t` struct to store the data.
- **Returns**: `GYRO_OK` on success, `GYRO_ERR_I2C` on communication error.

**`gyro_status_t mpu6050_set_gyro_range(mpu6050_gyro_range_t range);`**
Change the gyroscope full-scale range.
Can be called after initialization to dynamically change the range.
- **Parameters**: `[in] range` - New full-scale range selection.
- **Returns**: `GYRO_OK` on success, `GYRO_ERR_I2C` on communication error.

**`gyro_status_t mpu6050_deinit(void);`**
De-initialize the MPU-6050 sensor.
Puts the sensor into sleep mode and closes the I2C bus.
- **Returns**: `GYRO_OK` on success.

## Perbedaan Kedua Sensor

| Aspek | L3G4200D | MPU-6050 |
|-------|----------|----------|
| WHO_AM_I | `0xD3` (reg `0x0F`) | `0x68` (reg `0x75`) |
| Data format | Little-endian | Big-endian |
| Multi-byte read | Perlu auto-increment bit (`\|0x80`) | Auto-increment otomatis |
| Sleep mode | Tidak ada | Perlu wake saat init |
| Sensitivity | mdps/digit (multiply) | LSB/°/s (divide) |
| Range | 250/500/2000 °/s | 250/500/1000/2000 °/s |
| Extra config | - | DLPF, sample rate divider |

## Unit Test Results

```
L3G4200D: 41 passed, 0 failed ✓
MPU-6050: 45 passed, 0 failed ✓
```

## Troubleshooting

| Masalah | Solusi |
|---------|--------|
| `GYRO_ERR_I2C` | Cek koneksi SDA/SCL |
| `GYRO_ERR_ID` | Cek modul sensor & alamat I2C |
| `GYRO_ERR_TIMEOUT` | Reset board, cek pull-up resistor |
| Data selalu 0 | Tambah delay setelah init |
| Build error | Jalankan `source configs/pulpissimo.sh` |

## Referensi

- L3G4200D: [datasheet](https://www.st.com/resource/en/datasheet/l3g4200d.pdf)
- MPU-6050: [datasheet](https://www.invensense.com/wp-content/uploads/2015/02/MPU-6000-Datasheet.pdf)
