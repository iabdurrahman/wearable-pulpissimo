# LIS3DHTR Driver — Multi-Point Voltage Monitor (Auxiliary ADC)

Driver I2C untuk fitur **auxiliary ADC** pada chip LIS3DHTR di board
ICDeC PULPissimo, dipakai untuk monitor **2 titik tegangan sekaligus**:
- **Tegangan baterai** Li-ion 1S (3.0V–4.2V) → channel ADC1
- **Tegangan input modul TP4056** (5.2V saat charging) → channel ADC2

**PENTING:** driver ini TIDAK mengimplementasikan fitur accelerometer
chip ini sama sekali. LIS3DHTR punya dua fitur independen:
accelerometer 3-axis, dan auxiliary ADC 10-bit dengan 3 pin input
terpisah (ADC1/ADC2/ADC3). Task ini hanya butuh yang kedua.

## Arsitektur: channel sebagai parameter, bukan setting global

Chip secara hardware **sudah otomatis sampling ADC1/ADC2/ADC3 secara
bersamaan & terus-menerus** selama `ADC_PD=1` (masing-masing punya
register `OUT_ADCx` dan bit data-ready sendiri di `STATUS_AUX`). Jadi
driver ini **tidak perlu "switch channel"** di chip — channel cukup
jadi parameter tiap pemanggilan fungsi baca:

```c
lis3dhtr_read_raw(LIS3DHTR_ADC_CH1, &raw_battery);
lis3dhtr_read_raw(LIS3DHTR_ADC_CH2, &raw_tp4056);
```

`lis3dhtr_config_t` cuma berisi setting **chip-wide** (ODR, resolusi,
I2C, timeout) — bukan channel/divider-ratio, karena itu bisa beda-beda
per titik ukur dan diberikan sbg parameter saat baca, bukan disimpan
sebagai state global.

## Struktur File

```
lis3dhtr/
├── lis3dhtr_regs.h         # Bit definitions & register map (khusus aux ADC)
├── lis3dhtr.h              # Public API
├── lis3dhtr.c              # Implementasi driver
├── test_lis3dhtr.c         # Integration test FPGA/GVSoC — demo monitor 2 titik
├── Makefile
└── README.md
```

## Cara Pakai API

### Fungsi yang dipakai user

| Fungsi                                                            | Kegunaan                                                            |
|-------------------------------------------------------------------|---------------------------------------------------------------------|
| ma        |
| `lis3dhtr_init(&cfg)`                                             | Buka I2C, aktifkan aux ADC (semua channel), validasi WHO_AM_I       |
| `lis3dhtr_data_ready(channel, &ready)`                            | Cek data baru siap dibaca di channel tsb                            |
| `lis3dhtr_read_raw(channel, &raw)`                                | Baca 1 sample kode ADC mentah dari channel tsb                      |
| `lis3dhtr_read_millivolt(channel, &mv)`                           | Baca tegangan di pin ADC (mV), BELUM dikompensasi divider           |
| `lis3dhtr_read_voltage(channel, ratio, &voltage)`                 | Baca tegangan di sumber (Volt), SUDAH dikompensasi `ratio`          |
| `lis3dhtr_read_raw_avg(channel, N, &raw_avg)`                     | Sama `read_raw`, tapi rata-rata dari `N` sample (oversampling)      |
| `lis3dhtr_read_millivolt_avg(channel, N, &mv_avg)`                | Sama `read_millivolt`, dgn oversampling                             |
| `lis3dhtr_read_voltage_avg(channel, ratio, N, &voltage_avg)`      | Sama `read_voltage`, dgn oversampling — **direkomendasikan dipakai**|
| `lis3dhtr_who_am_i(&out)`                                         | Baca register WHO_AM_I mentah (harus 0x33)                          |
| `lis3dhtr_deinit()`                                               | Tutup koneksi I2C                                                   |

Fungsi `lis3dhtr_encode_ctrl1()`, `lis3dhtr_encode_ctrl4()`,
`lis3dhtr_encode_temp_cfg()`, `lis3dhtr_adc_shift_raw()`,
`lis3dhtr_adc_mv_per_digit()`, `lis3dhtr_raw_to_millivolt()` juga ada
di header, tapi itu terutama untuk keperluan pengujian logic secara
terpisah (lihat repo pengembangan penulis untuk unit test lengkapnya).

### Contoh pemakaian: monitor 2 titik sekaligus (baterai + TP4056)

```c
#include "lis3dhtr.h"

#define BATTERY_CH      LIS3DHTR_ADC_CH1
#define TP4056_CH       LIS3DHTR_ADC_CH2
#define VDIVIDER_RATIO  (47.0f / 141.0f)   /* R1=94k, R2=47k */
#define OVERSAMPLE_N    12

int main() {
    lis3dhtr_config_t cfg;
    lis3dhtr_default_config(&cfg);
    lis3dhtr_init(&cfg);   /* satu init utk SEMUA channel sekaligus */

    /* Baterai — presisi penting, pakai read_voltage_avg */
    float v_battery;
    lis3dhtr_read_voltage_avg(BATTERY_CH, VDIVIDER_RATIO, OVERSAMPLE_N, &v_battery);
    int percent = (int)((v_battery - 3.0f) / (4.2f - 3.0f) * 100.0f); /* mapping linear, di app */

    /* TP4056 — cuma perlu deteksi charging/tidak, baca raw + threshold */
    int16_t raw_tp4056;
    lis3dhtr_read_raw_avg(TP4056_CH, OVERSAMPLE_N, &raw_tp4056);
    bool charging = (raw_tp4056 <= -400);  /* raw NEGATIF = tegangan TINGGI */

    lis3dhtr_deinit();
}
```

**Catatan penting soal desain ini:**
- **Persentase baterai dihitung di kode APLIKASI, bukan di driver** —
  driver cuma memberi Volt yang akurat; mapping ke persentase (linear
  atau kurva Li-ion non-linear) adalah keputusan logic bisnis yang
  sengaja dipisah dari driver.
- **TP4056 sengaja dibaca sbg raw + threshold, bukan Volt** — karena
  tegangan input TP4056 (5.2V) lewat divider yang sama (47/141) akan
  menghasilkan ~1.73V di pin ADC, **melebihi rentang valid aux ADC
  (800mV–1600mV)**. Chip akan *clip* di kode maksimum. Ini SENGAJA
  dibiarkan karena tujuan monitor TP4056 cuma deteksi charging/tidak
  (binary), bukan baca nilai presisi — kalau butuh nilai presisi,
  perlu divider terpisah khusus TP4056 dgn rasio lebih kecil.

### Referensi Konstanta

**Channel ADC:**

| Konstanta             | Register OUT_x           |
|-----------------------|--------------------------|
| `LIS3DHTR_ADC_CH1`    | OUT_ADC1 (0x08/0x09)     |
| `LIS3DHTR_ADC_CH2`    | OUT_ADC2 (0x0A/0x0B)     |
| `LIS3DHTR_ADC_CH3`    | OUT_ADC3 (0x0C/0x0D)     |

**Resolusi ADC** (field `cfg.resolution`, chip-wide, berlaku semua channel):

| Konstanta                   | Bit valid | LPen |
|-----------------------------|-----------|------|
| `LIS3DHTR_ADC_RES_10BIT`    | 10-bit    | 0    | (default)
| `LIS3DHTR_ADC_RES_8BIT`     | 8-bit     | 1    | (low-power)

**ODR** (field `cfg.odr`, chip-wide, sekaligus sampling rate SEMUA channel):

| Konstanta                | Sample rate |
|--------------------------|-------------|
| `LIS3DHTR_ODR_1HZ`       | 1 Hz        |
| `LIS3DHTR_ODR_10HZ`      | 10 Hz       |
| `LIS3DHTR_ODR_25HZ`      | 25 Hz       |
| `LIS3DHTR_ODR_50HZ`      | 50 Hz       |
| `LIS3DHTR_ODR_100HZ`     | 100 Hz      | (default)
| `LIS3DHTR_ODR_200HZ`     | 200 Hz      |
| `LIS3DHTR_ODR_400HZ`     | 400 Hz      |

**vdivider_ratio** — parameter di tiap pemanggilan `read_voltage()`/
`read_voltage_avg()`, didefinisikan `R2/(R1+R2)`:

| Rangkaian                    | Rasio                     |
|------------------------------|---------------------------|
| Tanpa divider                | `1.0f`                    |
| R1=94k (2x47k seri), R2=47k  | `47.0f/141.0f` ≈ `0.3333` | (dipakai project ini)

## Oversampling

`*_avg()` membaca `sample_count` kali lalu merata-ratakan, untuk
mengurangi noise. **Keterbatasan yang perlu diketahui:** fungsi ini
TIDAK menunggu data-ready baru di antara tiap sample (tidak ada API
timer portable di scope driver ini) — kalau ODR jauh lebih lambat dari
kecepatan I2C, sample yang terkumpul bisa jadi nilai chip yang sama
berulang (belum tentu independen secara statistik). Untuk hasil
terbaik, panggil di aplikasi dengan jeda antar panggilan, bukan
mengandalkan `sample_count` besar dalam satu burst cepat.

## Build & Jalankan (FPGA)

```bash
make all                  # build test_lis3dhtr untuk target RISC-V
make run platform=fpga    # jalankan di FPGA
make clean                # bersihkan build artifacts
```

Butuh `PULPRT_HOME` & toolchain RISC-V PULPissimo SDK ter-setup.
`test_lis3dhtr.c` **wajib** punya stub `void pe_start(void) {}` —
`crt0.o` pulp-runtime mereferensikan simbol ini di vector table.

**Flag compiler penting:** kalau muncul crash `__rt_handle_illegal_instr`
(illegal instruction trap) di GDB, cek `PULP_ARCH_CFLAGS`/
`PULP_ARCH_C_LDFLAGS` di Makefile — board ICDeC PULPissimo butuh
`-march=rv32imcxpulpv2` (custom ISA extension PULP), bukan cuma
`-march=rv32imc` polos.

## Verifikasi terhadap datasheet resmi

Register map, bit layout (CTRL_REG1, CTRL_REG4, STATUS_AUX), alamat
I2C, dan auto-increment bit sudah di-cross-check terhadap datasheet
resmi **STMicroelectronics LIS3DH, Doc ID 17530 Rev 1 (Mei 2010)** dan
cocok 100%. Datasheet ini berlaku untuk LIS3DHTR juga (Table 1: varian
tape-and-reel, silicon identik).

**Catatan penting soal fitur aux ADC secara khusus:** detail teknis
input range (1200mV ± 400mV) dan bit `ADC_PD` di TEMP_CFG_REG **tidak
dijabarkan rinci** di datasheet Rev 1 ini. Nilai ini dikonfirmasi dari
**forum resmi ST Community**, serta silang-cek dengan beberapa driver
open-source pihak ketiga.

## Koreksi #1 — arah raw TERBALIK (berdasarkan pengujian TP4056)

Saat diuji langsung di board, ditemukan arah formula
`lis3dhtr_raw_to_millivolt()` yang **awalnya salah** — diasumsikan raw
makin POSITIF saat tegangan makin tinggi, padahal kenyataannya
**terbalik**: raw makin NEGATIF saat tegangan makin tinggi. Sudah
diperbaiki jadi `mV = bias - raw × step` (bukan `+`).

Bukti dari pengujian TP4056 (referensi tegangan yang jelas — 5.2V vs
~0V — jadi arahnya gampang dikonfirmasi):

| Kondisi TP4056 | Tegangan real | Raw terukur (SEBELUM fix) |
|---|---|---|
| Charging | ~5.2V (clip di batas atas ADC) | **-508** (bukan +511 spt dugaan awal) |
| Tidak charging | ~0V (clip di batas bawah ADC) | **+508** (bukan -512 spt dugaan awal) |

## Koreksi #2 — bias mV terkalibrasi (1283.5mV, bukan 1200mV Diambil data kalibrasi lengkap: **13 level tegangan
(3.0V–4.2V, step 0.1V) × 10 replikasi = 130 titik data**, dicatat
pasangan (Vin asli, raw ADC).

Hasil linear regression: `Vin = -0.00236 × raw + 3.8525` dengan
**R² = 0.9999** (hampir sempurna linear). Errornya ternyata **offset
konstan** (~0.25V di semua titik, bukan berubah-ubah), yang berarti
cuma satu angka yang salah: bias 1200mV di formula. Menyelesaikan
untuk bias yang tepat (dengan step tetap di nilai teoretis 0.78125
mV/digit, karena selisihnya cuma ~0.8% — dalam toleransi normal
resistor):

```
bias_terbaik = rata-rata dari (Vin × 1000 × ratio + raw × step)
             = 1283.5 mV   (bukan 1200 mV)
```

Hasil setelah kalibrasi: error maksimum turun dari **~260mV jadi
~17mV** di seluruh rentang 3.0V–4.2V — sudah diterapkan sebagai
konstanta baru di `lis3dhtr_raw_to_millivolt()`.

**Validasi lapangan (setelah kalibrasi):** 65 sampel diambil ulang
di seluruh rentang 3.0V–4.2V, hasilnya error rata-rata **0.13%**,
median **0.12%**, maksimum **0.77%** (satu outlier, kemungkinan noise
pengukuran sesaat) — jauh melampaui cukup akurat untuk estimasi
persentase baterai.

**Batasan kalibrasi ini:** data kalibrasi HANYA mencakup rentang mV di
pin ~1000mV–1400mV (Vin 3.0V–4.2V @ rasio 47/141) — jauh dari titik
ekstrem/clip (~800mV & ~1600mV). Jadi **bias (1283.5mV) sudah
tervalidasi kuat**, tapi **lebar rentang ±400mV di kedua sisi bias
masih ASUMSI** dari ST Community, belum diverifikasi langsung.

