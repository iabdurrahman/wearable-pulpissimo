# Smartwatch OLED & GUI Framework — RISC-V Pulpissimo

Proyek ini berisi implementasi **Driver Layar OLED monokrom (128x64)** beserta **Framework GUI (Graphical User Interface)** ringan berbasis uGUI yang ditujukan untuk platform RISC-V (Pulpissimo). Proyek ini dirancang khusus agar dapat menangani rendering antarmuka pengguna (UI) ala *smartwatch* beserta interaksi sentuhan (gesture) menggunakan *hardware* dengan sumber daya memori yang terbatas.

## 📖 Dokumentasi Lengkap

Untuk menjaga agar repositori tetap rapi, seluruh panduan teknis dan tutorial penggunaan dipisahkan ke dalam folder `docs/`. Silakan baca dokumen di bawah ini sesuai dengan bagian yang ingin kamu pelajari:

### 1. [Panduan Driver OLED (OLED_PENTING.md)](docs/OLED_PENTING.md)
Dokumen ini membahas segala hal di level *hardware/low-level*, di antaranya:
- Cara kerja *framebuffer* dan komunikasi I2C ke OLED (SSD1306/SH1106).
- Setup pin dan inisialisasi dasar.
- Fungsi-fungsi primitif untuk menggambar piksel, garis, kotak, dan teks dasar.
- Limitasi *font* bawaan *driver* dasar.

### 2. [Panduan Framework GUI & Gesture (GUI_PENTING.md)](docs/GUI_PENTING.md)
Dokumen ini membahas pengembangan tampilan di level *high-level* (UI Layer), di antaranya:
- Arsitektur uGUI dan *Gesture Engine* untuk mendeteksi *swipe*, *click*, dll.
- Cara menggunakan komponen UI (*Widget*): *Button*, *Textbox*, *Checkbox*, dan *Progress Bar*.
- Sistem *UI Manager* untuk mengatur perpindahan antar layar (seperti layer *Blood Oxygen*, *Step Count*, *Activity*, dan *Alarm*).
- Panduan *porting* GUI ke *hardware* mikrokontroler lain.
- Cara menjalankan *Unit Test* GUI di Linux (tanpa butuh fisik *board*).

---

## 🚀 Quick Start (Build & Run)

Pastikan *environment* Pulpissimo sudah terkonfigurasi. 

```bash
# Sesuaikan path script env dengan direktori instalasi pulp-runtime kamu
source /path/to/pulp-runtime/configs/pulpissimo.sh

# Build dan jalankan di FPGA
make clean all run platform=fpga

# (Atau) Build dan jalankan di simulator
make clean all run platform=gvsoc
```

Untuk menambahkan tampilan/layar baru, disarankan untuk membaca panduan di `docs/GUI_PENTING.md`!
