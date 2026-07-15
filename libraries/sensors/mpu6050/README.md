# MPU-6050 Gyroscope Sensor Library

Driver library minimal untuk InvenSense **MPU-6050** 6-Axis Gyroscope/Accelerometer pada board ICDeC FPGA (PULPissimo + Intel Cyclone 10), dibangun di atas [pulp-runtime](https://github.com/iabdurrahman/pulp-runtime.git) I2C driver.

Hanya berisi register yang benar-benar dipakai. Bukan port lengkap dari seluruh peta register MPU-6050.

## Fitur

- API minimal dan langsung pakai — hanya fungsi yang sudah terverifikasi di hardware
- Baca data gyroscope mentah (16-bit signed) dan dalam °/s (fixed-point integer)
- Konfigurasi full-scale range: ±250, ±500, ±1000, ±2000 °/s
- Digital Low-Pass Filter (DLPF) dan Sample Rate Divider yang bisa dikonfigurasi
- Auto-detection alamat I2C (mencoba default, lalu alternatif)
- Error handling dengan status code yang jelas
- **Tanpa floating-point** — semua konversi pakai integer fixed-point (×100)

## Catatan Penting

> [!IMPORTANT]
> Pin ADO set HIGH untuk alamat I2C 0x69

### 1. PULPissimo Tidak Punya FPU Hardware

Semua konversi ke deg/s dilakukan dengan **integer fixed-point** (dps × 100), **bukan float**. Float terbukti menghasilkan nilai yang salah di PULPissimo.

Contoh: nilai `x_x100 = 359` artinya **3.59 °/s**.

### 2. Delay Wajib antara Write dan Read (Repeated-Start)

Perlu delay kecil (~200 loop iterasi) antara fase "tulis alamat register" dan fase "baca data" (repeated-start). Tanpa ini, hasil baca **selalu 0xFF** meski tidak ada error/timeout dari `i2c_write()`/`i2c_read()`. Delay ini sudah dibangun ke dalam fungsi internal `mpu_read_reg()`.

### 3. Output Data Big-Endian

MPU-6050 mengeluarkan data dalam format **big-endian** — byte High duluan, baru byte Low. Ini **kebalikan** dari L3G4200D yang little-endian.

```
Register 0x43 = GYRO_XOUT_H (high byte)
Register 0x44 = GYRO_XOUT_L (low byte)
→ X = (GYRO_XOUT_H << 8) | GYRO_XOUT_L
```

### 4. Auto-Increment Otomatis (Tanpa Bit 0x80)

MPU-6050 **otomatis** melakukan auto-increment saat membaca register berurutan. **Tidak perlu** set bit 0x80 pada alamat register awal seperti L3G4200D.

```c
// Cukup kirim alamat register awal, lalu baca banyak byte
uint8_t reg = 0x43;  // GYRO_XOUT_H (tanpa bit 0x80)
i2c_write(i2c, &reg, 1, 0);
i2c_read(i2c, buf, 6, 0);   // baca 6 byte: XH, XL, YH, YL, ZH, ZL
```

### 5. Sleep Mode Default Saat Power-On

Setelah power-on, MPU-6050 berada dalam **SLEEP MODE** (bit SLEEP di PWR_MGMT_1 = 1). **Wajib** tulis `0x00` ke PWR_MGMT_1 untuk membangunkan device sebelum bisa membaca data.

## Struktur File

```
mpu6050/
├── mpu6050.h             # Public API header
├── mpu6050.c             # Implementasi driver
└── README.md             # Dokumentasi ini
└── test/
    ├── test_mpu6050.c        # Hardware test untuk FPGA
    └── Makefile              # Build system untuk hardware test
```

## Cara Build & Run

### Hardware Test (FPGA)

Build dan jalankan di board ICDeC:

```bash
# 1. Setup environment

export PATH="$PATH:/home/tetandclee/ICDeC/chroot/bin"
source recurse-submodules/configs/pulpissimo.sh

# 2. Build
cd libraries/sensors/mpu6050/test
make all

# 3. Run di FPGA
make run platform=fpga

# 4. Clean
rm -rf build
```

## Quick Start

```c
#include "mpu6050.h"

int main() {
    mpu6050_config_t cfg;
    mpu6050_dps_x100_t dps;

    mpu6050_default_config(&cfg);        // Load default config
    mpu6050_init(&cfg);                  // Wake + verify WHO_AM_I + konfigurasi
    mpu6050_read_dps_x100(&dps);         // Baca data dalam °/s (×100)

    // Contoh: dps.x_x100 = 359 artinya 3.59 °/s
    printf("X=%d.%02d Y=%d.%02d Z=%d.%02d dps\n",
           dps.x_x100 / 100, abs(dps.x_x100 % 100),
           dps.y_x100 / 100, abs(dps.y_x100 % 100),
           dps.z_x100 / 100, abs(dps.z_x100 % 100));

    mpu6050_deinit();                    // Sleep + close I2C
}
```

---

## API Documentation

### Status Codes

```c
typedef enum {
    MPU6050_OK          =  0,  // Operasi berhasil
    MPU6050_ERR_I2C     = -1,  // I2C gagal buka / komunikasi umum
    MPU6050_ERR_ID      = -2,  // WHO_AM_I tidak sesuai (bukan MPU-6050)
    MPU6050_ERR_CONFIG  = -3,  // Gagal konfigurasi register, atau fungsi dipanggil sebelum init
    MPU6050_ERR_TIMEOUT = -4,  // Transaksi I2C timeout
    MPU6050_ERR_NULL    = -5   // Pointer argumen NULL
} mpu6050_status_t;
```

### Data Structures

#### `mpu6050_raw_t` — Data Mentah Gyroscope

```c
typedef struct {
    int16_t x;  // Raw angular rate data, sumbu X
    int16_t y;  // Raw angular rate data, sumbu Y
    int16_t z;  // Raw angular rate data, sumbu Z
} mpu6050_raw_t;
```

Berisi nilai 16-bit signed langsung dari register sensor, tanpa konversi. Untuk mendapatkan °/s, gunakan `mpu6050_read_dps_x100()` atau hitung manual dengan sensitivity factor.

#### `mpu6050_dps_x100_t` — Data Gyroscope dalam °/s (Fixed-Point ×100)

```c
typedef struct {
    int32_t x_x100;  // Angular rate sumbu X, dikali 100
    int32_t y_x100;  // Angular rate sumbu Y, dikali 100
    int32_t z_x100;  // Angular rate sumbu Z, dikali 100
} mpu6050_dps_x100_t;
```

Format **fixed-point integer** dipakai karena PULPissimo tidak punya FPU. Nilai sudah dikalikan 100 sehingga presisi desimal tetap tersimpan dalam integer.

| Contoh `x_x100` | Artinya |
|--|--|
| `100` | 1.00 °/s |
| `-1000` | -10.00 °/s |
| `359` | 3.59 °/s |

#### `mpu6050_range_t` — Full-Scale Range

```c
typedef enum {
    MPU6050_RANGE_250DPS  = 0x00,  // ±250 °/s  (sensitivitas: 131   LSB/°/s)
    MPU6050_RANGE_500DPS  = 0x08,  // ±500 °/s  (sensitivitas: 65.5  LSB/°/s)
    MPU6050_RANGE_1000DPS = 0x10,  // ±1000 °/s (sensitivitas: 32.8  LSB/°/s)
    MPU6050_RANGE_2000DPS = 0x18   // ±2000 °/s (sensitivitas: 16.4  LSB/°/s)
} mpu6050_range_t;
```

Nilai-nilai ini langsung merupakan bit-field untuk register GYRO_CONFIG (bits FS_SEL[1:0], posisi bit [4:3]). Range yang lebih tinggi = sensitivitas lebih rendah tetapi rentang pengukuran lebih lebar.

> [!NOTE]
> MPU-6050 punya 4 level range (termasuk ±1000 °/s), sedangkan L3G4200D hanya punya 3 level.

#### `mpu6050_config_t` — Konfigurasi Inisialisasi

```c
typedef struct {
    uint8_t         i2c_addr;   // 7-bit I2C address (0x69 atau 0x68)
    int             i2c_id;     // I2C peripheral ID (biasanya 0)
    unsigned int    i2c_freq;   // Baudrate I2C (disarankan 100000 / 100kHz)
    mpu6050_range_t range;      // Full-scale range awal
    uint8_t         dlpf_cfg;   // Digital Low Pass Filter config (0-6)
    uint8_t         smplrt_div; // Sample rate divider
} mpu6050_config_t;
```

Set field-field ini sebelum memanggil `mpu6050_init()`. Gunakan `mpu6050_default_config()` untuk mendapatkan konfigurasi default yang sudah terbukti bekerja.

**DLPF (Digital Low-Pass Filter):**

| `dlpf_cfg` | Bandwidth Gyro | Delay | Gyro Output Rate |
|------------|---------------|-------|-----------------|
| 0 | 256 Hz | 0.98 ms | 8 kHz |
| 1 | 188 Hz | 1.9 ms | 1 kHz |
| 2 | 98 Hz | 2.8 ms | 1 kHz |
| 3 | 42 Hz | 4.8 ms | 1 kHz |
| 4 | 20 Hz | 8.3 ms | 1 kHz |
| 5 | 10 Hz | 13.4 ms | 1 kHz |
| 6 | 5 Hz | 18.6 ms | 1 kHz |

**Sample Rate:**

```
Sample Rate = Gyro Output Rate / (1 + smplrt_div)
```

Contoh: `dlpf_cfg=0` → Gyro Output Rate = 8 kHz, `smplrt_div=0` → Sample Rate = 8000/(1+0) = 8 kHz.

### API Functions

---

#### `mpu6050_default_config`

```c
mpu6050_status_t mpu6050_default_config(mpu6050_config_t *cfg);
```

Mengisi struct konfigurasi dengan nilai default yang sudah **terbukti bekerja** di hardware.

**Nilai default:**

| Field | Nilai | Keterangan |
|-------|-------|------------|
| `i2c_addr` | `0x69` | AD0 pin = HIGH (Vdd) |
| `i2c_id` | `0` | I2C peripheral pertama |
| `i2c_freq` | `100000` | 100 kHz (I2C standard mode) |
| `range` | `MPU6050_RANGE_250DPS` | ±250 °/s |
| `dlpf_cfg` | `0` | DLPF disabled, Gyro BW 256Hz |
| `smplrt_div` | `0` | Sample Rate = Gyro Output Rate / (1+0) |

**Parameter:**
- `[out] cfg` — Pointer ke struct konfigurasi yang akan diisi.

**Return:**
- `MPU6050_OK` — Berhasil.
- `MPU6050_ERR_NULL` — `cfg` adalah NULL.

---

#### `mpu6050_init`

```c
mpu6050_status_t mpu6050_init(mpu6050_config_t *cfg);
```

Inisialisasi sensor MPU-6050. Langkah-langkah yang dilakukan:

1. Membuka bus I2C dengan alamat dari `cfg->i2c_addr`
2. Membaca register **WHO_AM_I** (0x75) — harus bernilai `0x68`
3. Jika gagal di alamat default, otomatis mencoba alamat alternatif (0x69 ↔ 0x68) — **auto-detection**
4. Menulis **PWR_MGMT_1** (0x6B) = `0x00` — **wake from sleep** (WAJIB, karena default power-on = sleep)
5. Readback & verifikasi PWR_MGMT_1 (bit SLEEP dan DEVICE_RESET harus 0)
6. Menulis **SMPLRT_DIV** (0x19) jika `smplrt_div != 0`
7. Menulis **CONFIG** (0x1A) jika `dlpf_cfg != 0` — hanya bits [2:0] yang dipakai (di-mask dengan 0x07)
8. Menulis **GYRO_CONFIG** (0x1B) = nilai range dari config
9. Readback & verifikasi GYRO_CONFIG

> [!NOTE]
> Setelah init berhasil, field `cfg->i2c_addr` akan diupdate jika auto-detection mengubah alamat.
> Setiap konfigurasi register diverifikasi dengan readback — jika tidak cocok, return `MPU6050_ERR_CONFIG`.

**Parameter:**
- `[in,out] cfg` — Pointer ke struct konfigurasi. Diupdate jika alamat berubah via auto-detection.

**Return:**
- `MPU6050_OK` — Inisialisasi berhasil.
- `MPU6050_ERR_NULL` — `cfg` adalah NULL.
- `MPU6050_ERR_I2C` — Gagal membuka bus I2C.
- `MPU6050_ERR_TIMEOUT` — Tidak ada respons dari sensor (I2C timeout).
- `MPU6050_ERR_ID` — WHO_AM_I tidak cocok (bukan MPU-6050, atau sensor salah).
- `MPU6050_ERR_CONFIG` — Readback verifikasi register gagal, atau wake from sleep gagal.

---

#### `mpu6050_who_am_i`

```c
mpu6050_status_t mpu6050_who_am_i(uint8_t *id);
```

Membaca register **WHO_AM_I** (0x75). Berguna untuk verifikasi manual apakah sensor masih terhubung dan responsif. Nilai yang diharapkan: **`0x68`**.

> [!NOTE]
> Harus dipanggil **setelah** `mpu6050_init()`. Jika dipanggil sebelum init, return `MPU6050_ERR_CONFIG`.

**Parameter:**
- `[out] id` — Pointer untuk menyimpan nilai WHO_AM_I.

**Return:**
- `MPU6050_OK` — Berhasil.
- `MPU6050_ERR_NULL` — `id` adalah NULL.
- `MPU6050_ERR_CONFIG` — Sensor belum diinisialisasi.
- `MPU6050_ERR_TIMEOUT` — Komunikasi I2C timeout.

---

#### `mpu6050_read_raw`

```c
mpu6050_status_t mpu6050_read_raw(mpu6050_raw_t *raw);
```

Membaca data gyroscope mentah dari ketiga sumbu sekaligus. Mengembalikan nilai 16-bit signed langsung dari register sensor, tanpa scaling apapun.

Secara internal, membaca 6 byte berturut-turut dari register GYRO_XOUT_H (0x43). MPU-6050 otomatis auto-increment (tanpa bit 0x80). Data sensor di-reassemble dari big-endian:

```
raw->x = (GYRO_XOUT_H << 8) | GYRO_XOUT_L
raw->y = (GYRO_YOUT_H << 8) | GYRO_YOUT_L
raw->z = (GYRO_ZOUT_H << 8) | GYRO_ZOUT_L
```

**Parameter:**
- `[out] raw` — Pointer ke `mpu6050_raw_t` untuk menyimpan data.

**Return:**
- `MPU6050_OK` — Berhasil.
- `MPU6050_ERR_NULL` — `raw` adalah NULL.
- `MPU6050_ERR_CONFIG` — Sensor belum diinisialisasi.
- `MPU6050_ERR_TIMEOUT` — Komunikasi I2C timeout / byte tidak lengkap.

---

#### `mpu6050_read_dps_x100`

```c
mpu6050_status_t mpu6050_read_dps_x100(mpu6050_dps_x100_t *dps);
```

Membaca data gyroscope dan mengkonversi ke **derajat per detik** (°/s) dalam format fixed-point integer (dikali 100).

Secara internal: membaca raw data, lalu menghitung:

```
dps_x100 = (raw_value × num) / denom
```

Dimana sensitivity berdasarkan range aktif:

| Range | num | denom | Rumus asal | Sensitivitas |
|-------|-----|-------|-----------|-------------|
| ±250 °/s | 100 | 131 | 100 / 131 | 131 LSB/°/s |
| ±500 °/s | 200 | 131 | 100 / 65.5 | 65.5 LSB/°/s |
| ±1000 °/s | 125 | 41 | 100 / 32.8 | 32.8 LSB/°/s |
| ±2000 °/s | 250 | 41 | 100 / 16.4 | 16.4 LSB/°/s |

> [!TIP]
> Overflow safe: max raw 32767 × max num 250 = 8,191,750 — aman dalam `int32_t`.
> Untuk menampilkan dengan desimal: `printf("%d.%02d", val / 100, abs(val % 100));`

**Parameter:**
- `[out] dps` — Pointer ke `mpu6050_dps_x100_t` untuk menyimpan data.

**Return:**
- `MPU6050_OK` — Berhasil.
- `MPU6050_ERR_NULL` — `dps` adalah NULL.
- `MPU6050_ERR_CONFIG` — Sensor belum diinisialisasi.
- `MPU6050_ERR_TIMEOUT` — Komunikasi I2C timeout.

---

#### `mpu6050_set_range`

```c
mpu6050_status_t mpu6050_set_range(mpu6050_range_t range);
```

Mengubah full-scale range saat runtime. Bisa dipanggil setelah inisialisasi untuk mengubah range secara dinamis. Menulis nilai baru ke GYRO_CONFIG (0x1B) dan mengupdate sensitivity internal.

**Parameter:**
- `[in] range` — Pilihan full-scale range baru (`MPU6050_RANGE_250DPS`, `MPU6050_RANGE_500DPS`, `MPU6050_RANGE_1000DPS`, atau `MPU6050_RANGE_2000DPS`).

**Return:**
- `MPU6050_OK` — Berhasil.
- `MPU6050_ERR_CONFIG` — Sensor belum diinisialisasi.
- `MPU6050_ERR_I2C` — Gagal menulis ke register.

---

#### `mpu6050_deinit`

```c
mpu6050_status_t mpu6050_deinit(void);
```

De-inisialisasi sensor MPU-6050:

1. Menulis PWR_MGMT_1 = `0x40` (set bit SLEEP — sensor masuk sleep mode untuk hemat daya)
2. Menutup bus I2C
3. Mereset internal driver state

Aman dipanggil meskipun sensor belum diinisialisasi (langsung return `MPU6050_OK`).

**Return:**
- `MPU6050_OK` — Selalu berhasil.

---

## Register Map

Hanya register yang dipakai oleh library ini:

| Register | Alamat | Macro | Deskripsi |
|----------|--------|-------|-----------|
| SMPLRT_DIV | `0x19` | `MPU6050_REG_SMPLRT_DIV` | Sample Rate Divider |
| CONFIG | `0x1A` | `MPU6050_REG_CONFIG` | DLPF & EXT_SYNC configuration |
| GYRO_CONFIG | `0x1B` | `MPU6050_REG_GYRO_CONFIG` | Gyro full-scale range (FS_SEL) |
| GYRO_XOUT_H | `0x43` | `MPU6050_REG_GYRO_XOUT_H` | Awal data gyro (6 byte berurutan: XH,XL,YH,YL,ZH,ZL) |
| PWR_MGMT_1 | `0x6B` | `MPU6050_REG_PWR_MGMT_1` | Power Management 1 |
| WHO_AM_I | `0x75` | `MPU6050_REG_WHO_AM_I` | Device ID, read-only, default `0x68` |

### Konstanta Penting

| Macro | Nilai | Keterangan |
|-------|-------|------------|
| `MPU6050_WHO_AM_I_VALUE` | `0x68` | Nilai WHO_AM_I yang benar |
| `MPU6050_PWR1_DEVICE_RESET` | `0x80` | Device reset (bit 7 PWR_MGMT_1) |
| `MPU6050_PWR1_SLEEP` | `0x40` | Sleep mode (bit 6) — default ON setelah power-on |
| `MPU6050_PWR1_WAKEUP` | `0x00` | Clear sleep + reset → wake up |

### Konfigurasi Internal (Tidak Bisa Diubah User)

| Macro | Nilai | Keterangan |
|-------|-------|------------|
| `MPU6050_I2C_TIMEOUT_US` | `5000` | Software timeout per transaksi I2C (µs) |
| `MPU6050_READ_DELAY_LOOPS` | `200` | Delay antara write-register dan read-data |
| `MPU6050_CONFIG_SETTLE_LOOPS` | `50000` | Delay setelah menulis register konfigurasi |
| `MPU6050_WAKEUP_DELAY_LOOPS` | `100000` | Delay setelah wake from sleep (stabilisasi) |

## Alamat I2C

| Macro | Alamat | Kondisi |
|-------|--------|---------|
| `MPU6050_I2C_ADDR_DEFAULT` | `0x69` | AD0 pin = HIGH (Vdd) |
| `MPU6050_I2C_ADDR_ALT` | `0x68` | AD0 pin = LOW (GND) |

Saat inisialisasi, driver akan mencoba alamat default terlebih dahulu. Jika gagal (WHO_AM_I tidak cocok atau I2C timeout), otomatis mencoba alamat alternatif.

## Perbedaan dengan L3G4200D

| Aspek | MPU-6050 | L3G4200D |
|-------|----------|----------|
| WHO_AM_I | `0x68` (reg `0x75`) | `0xD3` (reg `0x0F`) |
| Data format | **Big-endian** (High duluan) | Little-endian (Low duluan) |
| Multi-byte read | Auto-increment **otomatis** | Perlu bit 0x80 pada alamat register |
| Sleep mode | Default ON setelah power-on — perlu wake | Tidak ada sleep mode |
| Sensitivity | LSB/°/s (pembagian: `raw × num / denom`) | mdps/digit (perkalian: `raw × num / denom`) |
| Range | 250/500/**1000**/2000 °/s | 250/500/2000 °/s |
| Extra config | DLPF, Sample Rate Divider | — |
| Deinit | Set SLEEP bit (hemat daya) | Power-down mode (CTRL_REG1 = 0x00) |

## Unit Test

Library ini dilengkapi 24 unit test dengan Mock I2C yang memverifikasi:

| Test | Deskripsi |
|------|-----------|
| 01 | Default config values (addr, freq, range, dlpf, smplrt) |
| 02 | `default_config(NULL)` → `ERR_NULL` |
| 03 | I2C address: 7-bit ke 8-bit conversion (0x69 → 0xD2) |
| 04 | Wake from sleep: PWR_MGMT_1 = 0x00 |
| 05 | WHO_AM_I verification (0x68 = pass, lainnya = fail) |
| 06 | SMPLRT_DIV register |
| 07 | DLPF configuration + bit masking |
| 08 | GYRO_CONFIG per range (250/500/1000/2000 dps) |
| 09 | Raw data assembly (big-endian, signed 16-bit) |
| 10 | Tidak ada auto-increment bit (berbeda dari L3G4200D) |
| 11-14 | Sensitivity per range (250/500/1000/2000 dps) |
| 15 | `set_range()` mengubah GYRO_CONFIG |
| 16 | `set_range()` mengupdate sensitivity |
| 17 | `deinit()` → set SLEEP bit (PWR_MGMT_1 = 0x40) |
| 18 | Read sebelum init → `ERR_CONFIG` |
| 19 | NULL pointer handling |
| 20 | I2C failure handling |
| 21 | Negative raw values (big-endian two's complement) |
| 22 | Timeout retry recovery |
| 23 | Timeout exhausted → `ERR_CONFIG` |
| 24 | Device reset sequence verification |

```
MPU-6050: 24 tests, all passed ✓
```

## Troubleshooting

| Masalah | Solusi |
|---------|--------|
| `MPU6050_ERR_I2C` | Cek koneksi SDA/SCL, pastikan pull-up resistor terpasang |
| `MPU6050_ERR_ID` | Cek modul sensor & alamat I2C (AD0 pin HIGH/LOW) |
| `MPU6050_ERR_TIMEOUT` | Reset board, cek pull-up resistor, cek apakah sensor terpasang dengan benar |
| `MPU6050_ERR_CONFIG` | Pastikan `mpu6050_init()` sudah dipanggil sebelum read/write |
| Data selalu 0xFF | Delay antara write dan read mungkin terlalu kecil — cek `MPU6050_READ_DELAY_LOOPS` |
| Data selalu 0 | Sensor mungkin masih dalam sleep mode — pastikan init berhasil (wake from sleep) |
| Build error | Jalankan `source configs/pulpissimo.sh` terlebih dahulu |

## Referensi

- **Datasheet**: [MPU-6000/MPU-6050 (InvenSense)](https://invensense.tdk.com/wp-content/uploads/2015/02/MPU-6000-Datasheet1.pdf)
- **Register Map**: [MPU-6000/MPU-6050 Register Map](https://invensense.tdk.com/wp-content/uploads/2015/02/MPU-6000-Register-Map1.pdf)
- **Platform**: [pulp-runtime I2C driver](https://github.com/iabdurrahman/pulp-runtime.git)
