# Library Driver MAX30102 & Algoritma Heart Rate

Dokumentasi ini menjelaskan implementasi driver hardware **MAX30102** (Sensor Heart Rate & SpO2) dan algoritma pendeteksi detak jantung (**PPG Heart Rate**) untuk board FPGA ICDeC (PULPissimo runtime). 

Semua file pendukung berada di folder `max30102/` dan dirancang untuk berjalan tanpa menggunakan Floating-Point Unit (FPU) hardware.

---

## 1. Struktur Folder

```text
max30102/
├── max30102.h              # API Publik Driver Hardware
├── max30102.c              # Implementasi Driver Hardware (I2C)
├── max30102_regs.h         # Definisi Register Internal MAX30102
├── test_max30102.c         # Unit Test & Runner Streaming Data RAW
├── Makefile                # Build System untuk Runner Driver
├── setup_env.sh            # Script Setup Environment GCC Toolchain
├── README.md               # Dokumentasi ini (Menimpa file lama)
└── filter/
    ├── ppg_hr.h            # API Publik Algoritma Heart Rate
    ├── ppg_hr.c            # Porting Algoritma SparkFun PBA (Integer-Only)
    └── test_ppg_hr.c       # Runner Pengujian Algoritma & Detak Jantung
```

---

## 2. Detail Library Driver (`max30102.c` / `max30102.h`)

Driver ini mengadopsi prinsip **Single Responsibility**. Peran driver ini **hanya terbatas pada antarmuka hardware**, meliputi:
* Inisialisasi bus I2C dan verifikasi Part ID sensor (harus bernilai `0x15`).
* Melakukan konfigurasi parameter fisik sensor (Sample Rate, ADC Range, Pulse Width, FIFO Averaging, dan LED Drive Current).
* Mengambil data mentah (raw samples) berupa ADC counts untuk kanal LED RED dan IR dari FIFO internal sensor.

Driver ini **tidak bertanggung jawab** atas pengolahan sinyal seperti filtering, deteksi puncak (peak detection), estimasi HR (Heart Rate), maupun SpO2.

### Konfigurasi Default Driver
Secara default, fungsi `max30102_get_default_config()` menyediakan parameter sebagai berikut:
* **Sample Rate**: 400 sampel/detik (`MAX30102_SR_400`)
* **ADC Range**: 4096 full-scale (`MAX30102_ADC_RANGE_4096`)
* **Pulse Width**: 18-bit (`MAX30102_PULSE_WIDTH_18BIT`) -> resolusi ADC tertinggi
* **FIFO Averaging**: Rata-rata 4 sampel per entri FIFO (`MAX30102_FIFO_AVG_4`)
* **LED Current**: ~7 mA (`0x24`) untuk LED Red dan IR

### API Publik Driver
```c
max30102_status_t max30102_init(max30102_t *dev, int i2c_port);
max30102_status_t max30102_configure(max30102_t *dev, const max30102_config_t *config);
max30102_config_t max30102_get_default_config(void);
max30102_status_t max30102_reset(max30102_t *dev);
max30102_status_t max30102_get_part_id(max30102_t *dev, uint8_t *part_id);
max30102_status_t max30102_clear_fifo(max30102_t *dev);
max30102_status_t max30102_read_sample(max30102_t *dev, max30102_sample_t *sample);
```

---

## 3. Detail Algoritma Heart Rate (`filter/ppg_hr.c` / `filter/ppg_hr.h`)

Algoritma ini merupakan porting langsung dari algoritma **PBA (Peripheral Beat Amplitude) dari SparkFun / Maxim Integrated** (Nathan Seidle) dengan beberapa penyesuaian untuk platform PULPissimo.

### Karakteristik Algoritma
1. **Reentrant & Multi-Instance**: 
   Kode asli SparkFun menggunakan variabel global `static` untuk menyimpan status filter (sehingga membatasi hanya bisa menggunakan 1 sensor). Porting ini mengelompokkan semua state filter ke dalam struct context `ppg_hr_t` sehingga mendukung reentrancy (bisa menjalankan beberapa instance filter secara independen).
2. **Integer-Only (Tanpa Floating Point)**:
   Meskipun hardware FPU (Floating Point Unit) sebenarnya tersedia pada platform PULPissimo (dengan mengubah konfigurasi build flags), library ini dirancang untuk berjalan sepenuhnya dengan matematika integer (tanpa operasi floating-point). Seluruh estimasi DC, filter FIR 12-tap, dan perhitungan BPM dikonversi ke tipe data integer 16-bit / 32-bit untuk menjaga kompatibilitas bawaan dan performa optimal.
   * Rumus asli BPM: `60 / (delta_ms / 1000.0)`
   * Rumus integer: `60000 / delta_ms` (dengan proteksi pembagian dengan nol).
3. **Penyaringan Sinyal**:
   Menggunakan DC Estimator rata-rata untuk menghilangkan komponen DC statis, diikuti oleh low-pass FIR filter untuk meminimalkan high frequency noise sebelum mendeteksi crossing titik nol (zero crossing) dengan threshold amplitudo dinamis.

### API Publik Algoritma
```c
void ppg_hr_init(ppg_hr_t *hr);
bool ppg_hr_check_for_beat(ppg_hr_t *hr, int32_t sample);
ppg_hr_result_t ppg_hr_process(ppg_hr_t *hr, int32_t ir_sample, uint32_t now_ms);
```

---

## 4. ⚠️ CRITICAL: Penggunaan Timer Bawaan PULP

Algoritma perhitungan BPM sangat bergantung pada keakuratan waktu antar detak jantung (`delta_ms`). Platform PULPissimo memiliki batasan hardware tertentu terkait manajemen waktu standar.

> [!IMPORTANT]
> **Limitasi Hardware & Solusi Timer Bawaan**
> * Fungsi timer standar dari API seperti `micros()` dan `millis()` pada platform ini **belum di-bringup sepenuhnya** (selalu mengembalikan nilai `0` dalam pengujian unit).
> * Solusinya, sangat disarankan untuk menggunakan fungsi penunjuk waktu (kernel timer) bawaan **PULP runtime**, yaitu:
>   `long pos_tick_get_counter_ms(void);`

### Cara Kerja Timer Bawaan PULP
PULP runtime mengonfigurasi peripheral APB Timer untuk menghasilkan interupsi berkala setiap **1 ms**. Interrupt Service Routine (ISR) bertugas menaikkan variabel counter global di memori. Fungsi `pos_tick_get_counter_ms()` membaca counter tersebut dan menggabungkannya dengan pembacaan sub-milidetik dari hardware timer untuk menghasilkan timestamp milidetik yang presisi dan meningkat secara monoton. Ini jauh lebih andal dibandingkan mengandalkan *software timer* (delay loop buatan) atau perhitungan berbasis *sample rate*.

> [!WARNING]
> **Persyaratan Wajib Saat Integrasi Hardware (FPGA)**
> 1. **Inisialisasi Tick**: Pastikan sistem interupsi tick telah diinisialisasi. Di dalam PULP runtime, inisialisasi dilakukan secara otomatis oleh fungsi boot `pos_init_start()`. Namun, pada aplikasi custom yang tidak melewati siklus boot standar, modul tick harus diinisialisasi manual via `pos_tick_init()`.
> 2. **Interupsi Harus Aktif**: Karena penambahan nilai tick digerakkan oleh interupsi hardware timer, **interupsi global CPU wajib diaktifkan** (`hal_irq_enable()`). Jika interupsi dinonaktifkan dalam waktu lama (misal karena busy wait CPU di tempat lain), counter timer akan macet, menyebabkan `delta_ms` bernilai `0` dan merusak kalkulasi BPM.
> 3. **Masalah Jitter**: Jika sistem mengalami beban komputasi tinggi yang memblokir penanganan interupsi (irq latency), timestamp yang didapatkan dari `pos_tick_get_counter_ms()` akan mengalami jitter. Jitter sebesar 10 ms saja dapat menyebabkan fluktuasi pembacaan BPM yang signifikan terutama pada detak jantung tinggi.

### Penanganan Waktu Saat Pengujian Simulasi (`SOFTWARE_TEST`)
Saat dicompile untuk simulasi lokal PC (tanpa board FPGA) menggunakan flag `-DSOFTWARE_TEST`, driver interupsi runtime tidak berjalan.
* Pada file runner `filter/test_ppg_hr.c`, kalkulasi waktu diwakili oleh software clock simulator (`sw_now_ms()`) yang mensimulasikan berjalannya waktu berdasarkan jumlah pembacaan sampel dan parameter konfigurasi sampel rate sensor:
  $$\text{Interval FIFO (ms)} = \frac{1000 \times \text{FIFO Averaging}}{\text{Sample Rate (Hz)}}$$
* Dengan cara ini, pengujian algoritma dapat berjalan secara deterministik dan akurat di PC tanpa bergantung pada hardware timer fisik.

---

## 5. Limitasi Sistem & Catatan Desain

1. **Jenuh ADC (Saturation)**:
   MAX30102 memiliki resolusi ADC 18-bit. Arus LED (Red & IR) yang terlalu besar dapat menyebabkan cahaya memantul berlebihan pada kulit tipis, sehingga ADC jenuh pada nilai maksimal `0x03FFFF`. Jika jenuh, tidak akan ada fluktuasi AC dan detak jantung tidak akan terdeteksi. Gunakan arus LED rendah (~7 mA hingga ~10 mA) sebagai titik awal.
2. **Deteksi Jari Terlepas (No Finger Detection)**:
   Algoritma tidak memiliki pendeteksi fisiologis yang rumit. Untuk membedakan apakah sensor sedang dipakai atau mengambang di udara, periksa nilai mutlak dari raw IR ADC count. Jika nilai **IR < 50000**, diasumsikan tidak ada jari yang menyentuh sensor, dan hasil perhitungan BPM harus diabaikan atau di-reset.
3. **Akurasi & Motion Artifact**:
   Algoritma filter PBA ini sangat sensitif terhadap gerakan (motion artifact). Pergeseran sensor pada kulit akan terbaca sebagai detak jantung palsu karena perubahan amplitudo sinyal IR yang drastis. Pastikan sensor terpasang kokoh pada kulit selama pengukuran.
4. **⚠️ Inkosistensi BPM (Tiba-tiba *Drop*)**:
   Saat ini, hasil pembacaan BPM masih belum sepenuhnya konsisten. Terkadang BPM terbaca akurat namun bisa tiba-tiba anjlok (*sudden drop*). Analisis penyebab utama:
   * **Detak Terlewat (*Missed Beats*)**: Algoritma mendeteksi detak berbasis *threshold* amplitudo sinyal AC. Jika sedikit gerakan membuat amplitudo AC turun sesaat, algoritma akan gagal melihat detak jantung. Ini membuat rentang waktu (`delta_ms`) ke detak berikutnya menjadi ganda, yang secara matematis langsung memangkas nilai pembacaan BPM menjadi setengahnya secara tiba-tiba.
   * **Distorsi Timestamp (*Time Warping*) akibat FIFO**: Parameter `now_ms` diberikan ketika sampel **dibaca dari FIFO**, bukan ketika sampel tersebut fisik terjadi. Jika program tertahan (misal: I2C lambat, update OLED), sampel akan antre di FIFO. Saat terbaca secara beruntun (*burst*), jarak waktunya menjadi berantakan, merusak kalkulasi `delta_ms` yang sangat peka terhadap akurasi milidetik.
   * **Saran Pengembangan & Optimisasi**: Untuk perbaikan ke depan, optimisasi wajib dimulai dengan **mengganti semua sisa *software timer* buatan sendiri atau *loop counting delay*** menggunakan timer bawaan PULP yang lebih presisi, yaitu `pos_tick_get_counter_ms()` dan `pos_delay_busy_ms()`. Selain itu, ke depannya sebaiknya beralih dari sistem *polling FIFO pasif* menjadi metode berbasis *hardware interrupt* menggunakan pin `INT` dari sensor agar setiap sampel mendapatkan timestamp yang nyata dan tidak tertunda.

---

## 6. Cara Build & Run

### Menjalankan Hardware Test Driver Raw (`test_max30102.c`)
Fungsi ini akan melakukan uji inisialisasi dasar driver lalu melakukan live streaming data RAW RED dan IR lewat serial console secara terus menerus.

1. Hubungkan board FPGA ke PC dan jalankan terminal.
2. Load environment compiler RISC-V:
   ```bash
   source setup_env.sh
   ```
3. Bersihkan sisa build lama dan compile:
   ```bash
   make clean && make all
   ```
4. Jalankan program pada target board FPGA:
   ```bash
   make run platform=fpga
   ```

### Menjalankan Hardware Test Algoritma Detak Jantung (`filter/test_ppg_hr.c`)
Program ini berada di folder `filter` dan menggabungkan driver pembacaan dengan filter PPG. Output berupa informasi detak jantung: `IR=<value>, BPM=<value>, Avg BPM=<value>`.

Untuk mengompilasi program test algoritma ini, Anda dapat menggunakan Makefile utama yang telah disesuaikan (atau menggunakan salinan dari `makefile.txt` bagian `test_ppg_hr`).
```bash
# Ganti isi Makefile utama dengan konfigurasi target test_ppg_hr di makefile.txt, lalu jalankan:
source setup_env.sh
make clean && make all
make run platform=fpga
```

### Simulasi Lokal (PC)
Jika Anda hanya ingin melakukan pengujian fungsionalitas sintaks dan logika kompilasi di PC lokal:
```bash
gcc test_max30102.c max30102.c -I. -DSOFTWARE_TEST -o test_local
./test_local
```

---

## 7. Contoh Integrasi Driver dengan Algoritma Heart Rate

Berikut adalah pola standar implementasi untuk membaca data sensor dan memprosesnya secara real-time di aplikasi Anda menggunakan timer bawaan PULP:

```c
#include "max30102.h"
#include "ppg_hr.h"
#include "pulp.h"
#include <stdio.h>

max30102_t sensor;
ppg_hr_t hr_filter;

int main(void) {
    // 1. Inisialisasi Driver MAX30102
    if (max30102_init(&sensor, 0) != MAX30102_OK) {
        printf("Sensor MAX30102 tidak ditemukan pada port I2C_0!\n");
        return -1;
    }
    
    // Gunakan konfigurasi standar SparkFun (100 Hz, 4x avg, ~6.4mA IR)
    max30102_config_t config = max30102_get_default_config();
    config.sample_rate = MAX30102_SR_100;
    config.fifo_avg = MAX30102_FIFO_AVG_4;
    config.ir_led_current = 0x1F;   // Arus LED optimal untuk HR
    config.red_led_current = 0x0A;  // Red sebagai indikator visual saja
    max30102_configure(&sensor, &config);

    // 2. Inisialisasi Filter detak jantung
    ppg_hr_init(&hr_filter);
    
    printf("Mulai monitoring detak jantung...\n");
    
    while(1) {
        max30102_sample_t sample;
        // Baca data dari FIFO sensor jika tersedia
        max30102_status_t status = max30102_read_sample(&sensor, &sample);
        
        if (status == MAX30102_OK) {
            // Ambil waktu dari timer bawaan PULP
            uint32_t t_ms = pos_tick_get_counter_ms();
            
            // Proses sampel IR ke algoritma heart rate
            ppg_hr_result_t result = ppg_hr_process(&hr_filter, (int32_t)sample.ir, t_ms);
            
            // Cek apakah terdeteksi ada jari
            if (sample.ir < 50000) {
                printf("Letakkan jari Anda pada sensor...\n");
                ppg_hr_init(&hr_filter); // Reset filter state agar tidak berantakan
            } else if (result.beat_detected) {
                printf("Detak Terdeteksi! BPM=%d | Rata-rata BPM=%d\n", 
                       (int)result.bpm, (int)result.avg_bpm);
            }
        }
        
        // Pacing delay kecil untuk menghindari pembacaan I2C berlebihan
        pos_delay_busy_ms(5);
    }
    
    return 0;
}
```
