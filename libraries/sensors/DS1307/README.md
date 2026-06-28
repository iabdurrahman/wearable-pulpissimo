# Bare-Metal RTC DS1307 Driver for RISC-V (PULPissimo)

Repositori ini berisi implementasi *driver bare-metal* bahasa C untuk membaca dan mengatur waktu pada sensor Real-Time Clock (RTC) DS1307. Driver ini dirancang khusus untuk berjalan di atas prosesor RISC-V NUSACORE pada *board* ARDUNESIA FPGA C10.

## Struktur File
- `rtc.h`: Deklarasi struktur data API dan *header* fungsi.
- `rtc.c`: Logika utama *driver*, implementasi I2C Read/Write, konversi BCD, dan *bit-masking*.
- `test.c`: Kode program utama (*entry point*) yang akan di-*flash* ke *board* FPGA untuk mencetak waktu via UART.
- `test_host.c`: Unit test lokal berbasis memori untuk mensimulasikan pembacaan I2C dan memvalidasi perhitungan matematika BCD tanpa perlu menyambungkan *hardware*.
- `build.sh` & `makefile`: *Script* otomatisasi *environment* dan kompilasi menggunakan PULP Runtime.

---

## Penjelasan Logika 

### 1. Konversi BCD (Binary-Coded Decimal)
Sensor DS1307 hanya menerima dan mengirim data dalam format BCD. Agar API lebih ramah digunakan (*user-friendly*), struktur `rtc_time_t` sengaja dirancang menggunakan nilai **Desimal murni (0-99)**. Driver ini secara otomatis melakukan konversi `dec_to_bcd` saat menulis ke sensor (*set time*), dan `bcd_to_dec` saat membaca dari sensor (*get time*).

### 2. Burst Read I2C (Auto-Increment Pointer)
Untuk efisiensi jalur komunikasi I2C, *driver* tidak memanggil alamat register satu per satu. Pembacaan dilakukan dengan teknik **Burst Read**:
- Menembak titik awal register `0x00` (Seconds).
- Menyedot 7 Byte data secara berurutan dalam satu kali tarikan (dari *Seconds* hingga *Year*) memanfaatkan fitur *Internal Auto-Increment Pointer* milik DS1307.

### 3. Bit-Masking & Konfigurasi Hardware
Data mentah di dalam register DS1307 bercampur dengan bit konfigurasi. Driver ini menggunakan operasi bitwise untuk membersihkan data tersebut:
- **`& 0x7F` pada Seconds:** Digunakan untuk membuang bit ke-7 (Clock Halt / CH) agar waktu murni terbaca, sekaligus memastikan bit CH selalu bernilai `0` saat *set time* agar osilator jam tetap menyala.
- **`& 0x3F` pada Hours:** Digunakan untuk mengabaikan bit ke-6 (Mode 12/24), sehingga pembacaan secara konstan terkunci pada format **24-Jam**.

### 4. Validasi Input
Semua fungsi melakukan pengecekan parameter (`NULL` pointer) dan `rtc_set_time` memvalidasi range waktu sesuai datasheet DS1307 sebelum mengirim ke sensor. Setiap operasi I2C dicek *return value*-nya untuk mendeteksi kegagalan komunikasi bus.

---

## Dokumentasi API

### Struktur Data
```c
typedef struct {
    uint8_t seconds; // Detik (0-59)
    uint8_t minutes; // Menit (0-59)
    uint8_t hours;   // Jam (0-23) - mode 24 jam
    uint8_t day;     // Hari (1-7, 1=Minggu)
    uint8_t date;    // Tanggal (1-31)
    uint8_t month;   // Bulan (1-12)
    uint8_t year;    // Tahun (0-99, representasi 2000-2099)
} rtc_time_t;
```

### Fungsi

#### `int rtc_init(i2c_dev_t *dev_conf, i2c_t **i2c_handle)`
Menginisialisasi hardware I2C Bus 0 untuk komunikasi dengan DS1307.

| Parameter | Deskripsi |
|---|---|
| `dev_conf` | Pointer ke struktur konfigurasi I2C (dialokasi pemanggil) |
| `i2c_handle` | Pointer-to-pointer yang akan diisi handle I2C hasil `i2c_open()` |

| Return | Keterangan |
|---|---|
| `0` | Inisialisasi berhasil |
| `-1` | Gagal (parameter `NULL` atau `i2c_open()` gagal) |

**Contoh:**
```c
i2c_dev_t rtc_dev_conf;
i2c_t *rtc_handle;

if (rtc_init(&rtc_dev_conf, &rtc_handle) != 0) {
    // Handle error
}
```

---

#### `int rtc_set_time(i2c_t *i2c_handle, rtc_time_t *time)`
Mengatur waktu pada DS1307 sekaligus mengaktifkan osilator (clear bit CH).

| Parameter | Deskripsi |
|---|---|
| `i2c_handle` | Handle I2C yang diperoleh dari `rtc_init()` |
| `time` | Pointer ke struktur `rtc_time_t` berisi waktu dalam format desimal |

| Return | Keterangan |
|---|---|
| `0` | Waktu berhasil diset |
| `-1` | Gagal (parameter `NULL`, range tidak valid, atau I2C write gagal) |

**Validasi range otomatis:** seconds 0-59, minutes 0-59, hours 0-23, day 1-7, date 1-31, month 1-12, year 0-99.

**Contoh:**
```c
rtc_time_t waktu = {
    .seconds = 0,
    .minutes = 30,
    .hours   = 14,
    .day     = 4,      // Kamis
    .date    = 26,
    .month   = 6,
    .year    = 26       // 2026
};

if (rtc_set_time(rtc_handle, &waktu) != 0) {
    // Handle error
}
```

---

#### `int rtc_get_time(i2c_t *i2c_handle, rtc_time_t *time)`
Membaca waktu terkini dari DS1307 menggunakan teknik *burst read* (7 byte sekaligus).

| Parameter | Deskripsi |
|---|---|
| `i2c_handle` | Handle I2C yang diperoleh dari `rtc_init()` |
| `time` | Pointer ke struktur `rtc_time_t` yang akan diisi hasil bacaan (dalam format desimal) |

| Return | Keterangan |
|---|---|
| `0` | Waktu berhasil dibaca |
| `-1` | Gagal (parameter `NULL` atau I2C read/write gagal) |

**Contoh:**
```c
rtc_time_t waktu_baca = {0};

if (rtc_get_time(rtc_handle, &waktu_baca) == 0) {
    printf("%02d:%02d:%02d\n", waktu_baca.hours, waktu_baca.minutes, waktu_baca.seconds);
}
```

---

#### `uint8_t dec_to_bcd(uint8_t val)` / `uint8_t bcd_to_dec(uint8_t val)`
Fungsi konversi internal yang diekspos untuk keperluan unit testing.

| Fungsi | Input | Output |
|---|---|---|
| `dec_to_bcd` | Desimal (0-99) | BCD (0x00-0x99) |
| `bcd_to_dec` | BCD (0x00-0x99) | Desimal (0-99) |

---

## Requirement Tree

```
rtc kalender
├── membaca kalender dan jam dari rtc
│   ├── bisa menggunakan i2c
│   └── bisa membaca register dari rtc
└── menyingkronkan data rtc ketika sistem menyala dan online
    ├── bisa menggunakan i2c
    └── bisa membaca register dari rtc
```

---

## Build & Flash

```bash
# Kompilasi untuk board FPGA
cd RTC/app
source ./build.sh

# Unit test lokal (tanpa hardware)
gcc -o test_host test_host.c && ./test_host
```

## Dependensi
- **PULP Runtime** (`pulp-runtime/`)
- **RISC-V GCC Toolchain** (`chroot/bin/`)
- **Target:** PULPissimo pada FPGA (10 MHz peripheral/SoC clock)