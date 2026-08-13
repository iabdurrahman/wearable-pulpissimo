# ADS1118 Driver — Battery Voltage Reader

Driver SPI untuk chip ADC ADS1118 pada board ICDeC
PULPissimo, dipakai untuk membaca tegangan baterai melalui channel
single-ended AIN0 di belakang voltage divider eksternal.

## Struktur File

```
ads1118/
├── ads1118_regs.h        # Bit definitions Config Register (dari datasheet SBAS457A)
├── ads1118.h              # Public API
├── ads1118.c              # Implementasi driver
├── README.md
└── test/
    ├── test_ads1118.c          # Integration test untuk FPGA/GVSoC (pulp-rt asli)
    ├── unit_test_ads1118.c     # Unit test host PC (mock SPI, tanpa hardware)
    ├── Makefile
    └── mock/
        └── pulp.h              # Stub SPI untuk unit testing
```

## Cara Pakai API

Cukup `#include "ads1118.h"` — file ini sudah otomatis membawa
`ads1118_regs.h`, jadi tidak perlu include terpisah untuk memakai
konstanta MUX/PGA/DR di bawah.

### Fungsi yang dipakai user 

| Fungsi                                     | Kegunaan                                                                                   |      
|--------------------------------------------|--------------------------------------------------------------------------------------------|
| `ads1118_default_config(&cfg)`             | Isi `cfg` dengan nilai default yang aman                                                   |
| `ads1118_init(&cfg)`                       | Buka koneksi SPI ke chip, kirim config                                                     |
| `ads1118_read_raw(&raw)`                   | Baca kode ADC mentah (int16_t), belum dikonversi ke Volt                                   |
| `ads1118_read_battery_voltage(&voltage)`   | Baca tegangan baterai (float), sudah dikonversi ke Volt dan dikompensasi voltage divider   |
| `ads1118_deinit()`                         | Tutup koneksi SPI 

Fungsi `ads1118_encode_config()` dan `ads1118_raw_to_voltage()` juga ada
di header, tapi itu untuk keperluan unit test — user tidak perlu
memanggilnya langsung.

### Contoh pemakaian dasar (pakai semua nilai default)

```c
#include "ads1118.h"

int main() {
    ads1118_config_t cfg;
    ads1118_default_config(&cfg);

    /* WAJIB disesuaikan dengan hardware asli sebelum dipakai di board nyata: */
    cfg.vdivider_ratio = 0.5f;      /* ganti sesuai rasio R2/(R1+R2) divider kamu */

    ads1118_init(&cfg);

    float voltage;
    ads1118_read_battery_voltage(&voltage);
    printf("Tegangan baterai: %.3f V\n", voltage);

    ads1118_deinit();
}
```

### Contoh mengubah konfigurasi (channel, gain, data rate, divider)

Pola pemakaiannya: panggil `ads1118_default_config()` dulu untuk isi
semua field dengan nilai aman, lalu **timpa** field spesifik yang mau
diubah. Tidak perlu isi semua field dari nol.

```c
#include "ads1118.h"

int main() {
    ads1118_config_t cfg;
    ads1118_default_config(&cfg);   /* isi default dulu, WAJIB dipanggil */

    cfg.mux             = ADS1118_MUX_AIN2_GND;  /* pakai channel AIN2, bukan AIN0 */
    cfg.pga             = ADS1118_PGA_FS_1024;   /* gain, full-scale +-1.024V */
    cfg.data_rate       = ADS1118_DR_475SPS;     /* kecepatan sampling 475SPS */
    cfg.vdivider_ratio  = 1.0f / 4.0f;            /* rasio voltage divider 4:1 */

    ads1118_init(&cfg);   /* semua config di atas baru "dikirim" sekaligus di sini */

    float voltage;
    ads1118_read_battery_voltage(&voltage);

    ads1118_deinit();
}
```

Catatan penting soal urutan pemanggilan:
- `ads1118_default_config()` **wajib** dipanggil sebelum menimpa field
  manapun, supaya field yang tidak kamu ubah (mis. `spi_id`, `cs_pin`)
  tetap terisi nilai valid, bukan nilai sembarangan dari memori.
- Perubahan field pada `cfg` baru benar-benar dikirim ke chip saat
  `ads1118_init(&cfg)` dipanggil — sebelum itu, `cfg` cuma variabel
  biasa di memori, belum berpengaruh ke hardware.

### Referensi Konstanta

**MUX — pilih channel input** (field `cfg.mux`)

| Konstanta              | Maksud                              |
|------------------------|-------------------------------------|
| `ADS1118_MUX_DIFF_0_1` | Differential: AIN0 (+) vs AIN1 (-)  |
| `ADS1118_MUX_DIFF_0_3` | Differential: AIN0 (+) vs AIN3 (-)  |
| `ADS1118_MUX_DIFF_1_3` | Differential: AIN1 (+) vs AIN3 (-)  |
| `ADS1118_MUX_DIFF_2_3` | Differential: AIN2 (+) vs AIN3 (-)  |
| `ADS1118_MUX_AIN0_GND` | Single-ended: AIN0 dibanding GND    |
| `ADS1118_MUX_AIN1_GND` | Single-ended: AIN1 dibanding GND    |
| `ADS1118_MUX_AIN2_GND` | Single-ended: AIN2 dibanding GND    |
| `ADS1118_MUX_AIN3_GND` | Single-ended: AIN3 dibanding GND    |

**PGA — gain / full-scale range** (field `cfg.pga`). Full-scale (FS)
menentukan rentang tegangan maksimum yang bisa terbaca; makin kecil FS,
makin tinggi resolusinya, tapi sinyal akan clipping kalau melebihi FS.

| Konstanta             | Full-scale range |
|-----------------------|------------------|
| `ADS1118_PGA_FS_6144` | ±6.144 V         |
| `ADS1118_PGA_FS_4096` | ±4.096 V         | (dipakai di default config) 
| `ADS1118_PGA_FS_2048` | ±2.048 V         | (default reset chip) 
| `ADS1118_PGA_FS_1024` | ±1.024 V         |
| `ADS1118_PGA_FS_0512` | ±0.512 V         |
| `ADS1118_PGA_FS_0256` | ±0.256 V         |

**DR — data rate / kecepatan sampling** (field `cfg.data_rate`). Makin
tinggi SPS, makin cepat pembacaan tapi noise sedikit lebih besar; makin
rendah SPS, makin lambat tapi lebih presisi/stabil.

| Konstanta           | Sample rate      |
|---------------------|------------------|
| `ADS1118_DR_8SPS`   | 8 sample/detik   |(paling presisi, paling lambat) 
| `ADS1118_DR_16SPS`  | 16 sample/detik  |
| `ADS1118_DR_32SPS`  | 32 sample/detik  |
| `ADS1118_DR_64SPS`  | 64 sample/detik  |
| `ADS1118_DR_128SPS` | 128 sample/detik |(dipakai di default config) 
| `ADS1118_DR_250SPS` | 250 sample/detik |
| `ADS1118_DR_475SPS` | 475 sample/detik |
| `ADS1118_DR_860SPS` | 860 sample/detik |(paling cepat, paling banyak noise) 

**Rasio Voltage Divider** (field `cfg.vdivider_ratio`) — bukan
konstanta, tapi angka `float` biasa. Didefinisikan sebagai
`R2/(R1+R2)`, yaitu seberapa besar tegangan asli "dikecilkan" sebelum
sampai ke pin AIN.

| Situasi                                   | Nilai yang diisi |
|-------------------------------------------|------------------|
| Tanpa divider (baterai langsung ke AIN)   | `1.0f`           |
| Divider 2:1 (tegangan jadi setengahnya)   | `0.5f`           |
| Divider 3:1 (tegangan jadi sepertiganya)  | `1.0f / 3.0f`    |
| Divider 4:1 (tegangan jadi seperempatnya) | `1.0f / 4.0f`    |

## Menjalankan Unit Test

```bash
cd test
make unit
```

Semua 16 test case (32 assertion) memverifikasi logic murni tanpa hardware:
bit-packing Config Register, konversi raw code → voltage, kompensasi voltage
divider, dan error handling (NULL pointer, SPI gagal, baca sebelum init).

## Hal yang WAJIB diverifikasi/disesuaikan sebelum build ke hardware/GVSoC

1. **`spim_cs_e` value `SPIM_CS_AUTO`** dipakai di setiap transfer (asumsi:
   artinya "release CS setelah transfer ini"), berdasarkan analogi langsung
   dengan `rt_spim_cs_e` (`RT_SPIM_CS_AUTO`/`RT_SPIM_CS_KEEP`) milik pulp-rt
   asli, karena `spi.h` tidak tersedia untuk konfirmasi langsung. Cek
   `spi.h`/`pulp.h` asli project untuk memastikan nama dan urutan enum ini.
2. **`spim_transfer()` tidak melaporkan status berhasil/gagal** (return
   `void`) — driver ini hanya bisa mendeteksi kegagalan saat `spim_open()`
   (`init()`), bukan saat transfer data berlangsung. Ini keterbatasan dari
   API yang disediakan, bukan bug driver.
3. **`vdivider_ratio` default (0.5)** adalah asumsi rasio divider 2:1,
   BUKAN nilai terukur dari rangkaian asli. Wajib diganti sesuai resistor
   pembagi tegangan yang sebenarnya dipasang di board.
4. **PGA default (±4.096V)** dipilih dengan asumsi baterai 4.2V melalui
   divider 2:1 (~2.1V di AIN0). Hitung ulang kalau rasio divider berbeda.
5. **`ads1118_delay_for_datarate()`** memakai busy-wait loop kasar (jumlah
   iterasi bukan berbasis timer nyata) — ganti dengan API timer SDK yang
   sesungguhnya (mis. `rt_time_wait_us`) supaya waktu tunggu konversi
   akurat di hardware/GVSoC.

## Sudah dikonfirmasi dari library SPI asli mentor (spi.c)

- Fungsi: `spim_conf_init()`, `spim_open()`, `spim_close()`, `spim_transfer()`
- Tipe: `spim_conf_t` (field: `wordsize`, `big_endian`, `max_baudrate`,
  `cs_gpio`, `cs`, `id`, `polarity`, `phase`, `bitOrder`), `spim_t`
- Panjang data di `spim_transfer()` dalam **bit**, bukan byte (driver ini
  selalu mengirim 4 byte = 32 bit per transaksi)
- SPI mode 1 (CPOL=0, CPHA=1) di-set eksplisit di `ads1118_init()` sesuai
  requirement datasheet ADS1118 (default `spim_conf_init()` kemungkinan
  mode 0, jadi ini **wajib** di-override, sudah dilakukan di kode ini)