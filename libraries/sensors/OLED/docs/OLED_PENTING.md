# OLED Driver — Pulpissimo RISC-V

Dokumen ini menjelaskan cara menggunakan **OLED Driver** (`oled.h / oled.c`) untuk menampilkan grafis pada layar OLED monokrom berukuran 128×64 piksel. Driver ini berkomunikasi melalui **I2C** dan mendukung chip controller **SSD1306** maupun **SH1106**.

---

## 1. Cara Kerja 

Driver ini menggunakan **framebuffer** — sebuah array berukuran 128×64 bit (= 1024 byte) yang disimpan di RAM. Semua fungsi gambar (`DrawPixel`, `DrawLine`, dst.) hanya mengubah data di framebuffer ini. Perubahan **baru muncul di layar** setelah kamu memanggil `OLED_Update()`.

```
  DrawPixel(), DrawLine(), DrawRect(), ...
              │
              ▼
      ┌──────────────┐
      │  Framebuffer │  ← data di RAM, belum tampil
      │  (1024 byte) │
      └──────┬───────┘
             │  OLED_Update()
             ▼
      ┌──────────────┐
      │  Layar OLED  │  ← baru tampil di sini
      │  128 x 64 px │
      └──────────────┘
```

> **Penting:** Kalau menggambar sesuatu tapi tidak memanggil `OLED_Update()`, layar tidak akan berubah.

---

## 2. Setup Hardware

Hubungkan modul OLED ke pin I2C pada board Pulpissimo:

| Pin OLED | Pin Board  | Keterangan          |
|----------|------------|---------------------|
| VCC      | 3.3V       | Sumber tegangan     |
| GND      | GND        | Ground              |
| SDA      | I2C_SDA    | Data I2C            |
| SCL      | I2C_SCL    | Clock I2C           |

Alamat I2C default: **`0x3C`** (sudah di-set di dalam driver, tidak perlu diubah).

---

## 3. Quick Start — Menampilkan Teks

Ini adalah contoh paling sederhana untuk memastikan layar menyala dan bisa menampilkan sesuatu:

```c
#include "oled.h"

int main(void) {
    // 1. Inisialisasi layar
    OLED_Init();
    OLED_Clear();

    // 2. Gambar teks dan bentuk
    OLED_DrawString(10, 10, "Halo hihi!", true);
    OLED_DrawRect(5, 5, 118, 54, true);

    // 3. Kirim ke layar (WAJIB!) jan lupa yaa
    OLED_Update();

    while(1) {} // Loop selamanya
    return 0;
}
```

Build dan jalankan:
```bash
source /path/to/pulp-runtime/configs/pulpissimo.sh          //sesuaikan aja ya dengan cara build pulpissimo dan direktorinya
make clean all run platform=fpga
```

> **Tips:** Gunakan `platform=gvsoc` kalau mau menjalankan di simulator tanpa board fisik.

---

## 4. Daftar Fungsi Lengkap

### Inisialisasi & Kontrol

| Fungsi                | Kegunaan                                          |
|-----------------------|---------------------------------------------------|
| `OLED_Init()`         | Inisialisasi layar OLED (wajib dipanggil pertama) |
| `OLED_Clear()`        | Menghapus seluruh framebuffer (layar jadi hitam)  |
| `OLED_Update()`       | Mengirim framebuffer ke layar fisik               |
| `OLED_DisplayOn()`    | Menyalakan layar                                  |
| `OLED_DisplayOff()`   | Mematikan layar (hemat daya)                      |

### Menggambar Bentuk

| Fungsi                                    | Kegunaan                      |
|-------------------------------------------|-------------------------------|
| `OLED_DrawPixel(x, y, color)`             | Menggambar satu titik         |
| `OLED_DrawLine(x0, y0, x1, y1, color)`    | Menggambar garis lurus        |
| `OLED_DrawRect(x, y, w, h, color)`        | Menggambar kotak (outline)    |
| `OLED_FillRect(x, y, w, h, color)`        | Menggambar kotak isi penuh    |
| `OLED_DrawCircle(x0, y0, r, color)`       | Menggambar lingkaran          |

### Menampilkan Teks

| Fungsi                                    | Kegunaan                          |
|-------------------------------------------|-----------------------------------|
| `OLED_DrawChar(x, y, 'A', color)`         | Menggambar satu karakter          |
| `OLED_DrawString(x, y, "teks", color)`    | Menggambar string (banyak huruf)  |

> **Parameter `color`:** Gunakan `true` untuk menyalakan piksel (putih) dan `false` untuk mematikan (hitam).

---

## 5. Sistem Koordinat

```
  (0,0) ────────────────────── (127,0)
    │                              │
    │       Layar 128 x 64         │
    │       x = horizontal         │
    │       y = vertikal           │
    │                              │
  (0,63) ─────────────────── (127,63)
```

- **x** → horizontal, dari kiri (0) ke kanan (127)
- **y** → vertikal, dari atas (0) ke bawah (63)
- Koordinat di luar rentang ini akan diabaikan oleh driver.

> **Catatan:** Driver yang digunakan pada pulpissimo adalah SH1106, driver ini memiliki 132 kolom. Namun layar yang digunakan adalah 128x64, sehingga dibutuhkan penyesuaian dengan menggunakan `OFFSET_X = 2` (khusus untuk SH1106).

---

## 6. Contoh Lanjutan

### Menggambar Kotak dan Lingkaran

```c
OLED_Clear();

// Kotak di pojok kiri atas
OLED_FillRect(5, 5, 30, 20, true);

// Lingkaran di tengah layar
OLED_DrawCircle(64, 32, 15, true);

// Garis diagonal
OLED_DrawLine(0, 0, 127, 63, true);

OLED_Update();
```

### Menampilkan Nilai Sensor

```c
#include <stdio.h>

char buf[32];
int suhu = 36;

snprintf(buf, sizeof(buf), "Suhu: %d C", suhu);
OLED_DrawString(10, 25, buf, true);
OLED_Update();
```

---

## 7. Font Bawaan

Driver ini menyertakan font bitmap **5×7 piksel** (`font/font5x7.c`). Tapi cuma beberapa aja, gak lengkap. Font yang ada itu:
- Huruf kecil [lower case] (a-z)
- Angka (0-9)
- Beberap simbol  (`.`, `,`, `:`, `-`, `_`, `(`, `)`, `/`)

Jarak antar karakter otomatis 1 piksel, sehingga tiap karakter menempati **6 piksel horizontal**.

Font bawaan hanya menyediakan huruf kecil, jika tertulis huruf besar, logika otomatis yang tersedia akan mengubahnya menjadi huruf kecil. 

> **Catatan:** Jika ingin font yang lebih besar atau lebih beragam, gunakan sistem font dari framework **uGUI** (lihat dokumen `GUI.md`).

---

## 8. Struktur File

```
OLED/
├── oled.h              ← Header utama (include ini untuk menggunakan driver)
├── oled.c              ← Implementasi driver (I2C, framebuffer, gambar)
├── ssd1306_cmd.h       ← Definisi command register SSD1306/SH1106
├── font/
│   └── font5x7.c       ← Data bitmap font 5x7
├── tests/
│   ├── test_pixel.c     ← Test menggambar pixel
│   ├── test_line.c      ← Test menggambar garis
│   ├── test_kotak.c     ← Test menggambar kotak
│   ├── test_circle.c    ← Test menggambar lingkaran
│   ├── test_text.c      ← Test menampilkan teks
│   ├── test_tangga.c    ← Test pola tangga
│   └── test_papancatur.c ← Test pola papan catur
├── main.c              ← Entry point aplikasi
└── Makefile            ← Build system
```

---

## 9. Tips & Catatan Penting

1. **Selalu panggil `OLED_Init()` sebelum fungsi lain.** Fungsi ini mengirim sequence inisialisasi ke chip controller melalui I2C.

2. **Jangan lupa `OLED_Update()`.** Ini sering lupa. Semua fungsi gambar hanya menulis ke RAM, bukan langsung ke layar.

3. **`OLED_Clear()` hanya menghapus framebuffer.** Layar baru menjadi kosong setelah `OLED_Update()` dipanggil setelahnya.

4. **Driver ini menggunakan SH1106**, bukan SSD1306. Driver ini memiliki 132 kolom, namun layar yang digunakan adalah 128×64, sehingga dibutuhkan penyesuaian dengan menggunakan `OFFSET_X = 2`. 

5. **Ukuran layar tetap 128×64.** Definisi ini ada di `oled.h` sebagai `OLED_WIDTH` dan `OLED_HEIGHT`. Jangan diubah kecuali benar-benar mengganti modul layar.

6. **Untuk antarmuka yang lebih kompleks** (tombol, progress bar, textbox, navigasi layar), pake framework **uGUI** yang ada di `GUI`. Bisa liat  penjelasannya di `GUI.md`.
