# GUI — Framework uGUI untuk OLED

Dokumen ini menjelaskan cara menggunakan sistem **GUI (Graphical User Interface)** yang sudah terintegrasi di proyek ini. Sistem GUI ini dibangun di dari framework **uGUI** by deividAlfa dan dilengkapi dengan **Gesture Engine** penerapan dari GUI LVGL sebelumnya.

> **Catatan:** Bisa baca dokumen **OLED Driver** dulu untuk lebih memahami cara kerja OLED dan GUI ini kedepannya.

---

## 1. Gambaran Umum Sistem

Sistem GUI ini terdiri dari **empat lapisan** yang saling bekerja sama:

```
┌─────────────────────────────────────────────┐
│           UI Manager (ui_manager.c)         │  ← Mengatur layar mana yang aktif
├─────────────────────────────────────────────┤
│  UI Layers (ui_oxygen, ui_activity, ...)    │  ← Tampilan per fitur wearable
├─────────────────────────────────────────────┤
│  uGUI Framework (ugui.c + fitur/*.c)        │  ← Widget: Button, Textbox, dll.
├──────────────┬──────────────────────────────┤
│ Porting Layer│   Gesture Engine             │  ← Jembatan ke hardware +
│ (gui_port.c) │   (gesture.c)                │    deteksi sentuhan kompleks
├──────────────┴──────────────────────────────┤
│           OLED Driver (oled.c)              │  ← Kirim piksel ke layar fisik
└─────────────────────────────────────────────┘
```

**Mengapa pakai uGUI?** Karena framework ini sangat ringan — tidak butuh framebuffer besar dan bisa berjalan di mikrokontroler dengan RAM terbatas. Cocok untuk layar monokrom 128×64.

Selain itu, GUI ini juga bisa digunakan di RISC-V dan support untuk gesture/sensor touch screen.

µGUI - deividAlfa : https://github.com/deividAlfa/UGUI

---

## 2. Struktur Folder GUI

```
GUI/
├── gui_port.c          ← Porting Layer (jembatan ke hardware)
├── gui_port.h
├── gesture.c           ← Gesture Engine (deteksi swipe, klik, dll.)
├── gesture.h
├── demo.c              ← Demo showcase fitur yang disediakan uGUI
├── demo.h
├── ugui/               ← Framework uGUI (JANGAN DIUBAH)
│   ├── ugui.c          ← Core rendering engine
│   ├── ugui.h          ← Header utama uGUI
│   └── fitur/          ← Modul widget yang diaktifkan
│       ├── ugui_button.c
│       ├── ugui_checkbox.c
│       ├── ugui_progress.c
│       ├── ugui_textbox.c
│       ├── ugui_image.c
│       └── Fonts/
│           └── font_6x10.c   ← Font default untuk GUI
├── ui_layer/           ← Layer tampilan wearable
│   ├── ui_manager.c    ← Pengatur pergantian layar
│   ├── ui_oxygen.c     ← Layar Blood Oxygen (SpO2)
│   ├── ui_activity.c   ← Layar Activity Recognition
│   ├── ui_stepcount.c  ← Layar Step Counter
│   └── ui_alarm.c      ← Pop-up Alarm / Notifikasi
└── test/               ← Contoh dan unit test widget
    ├── test_gui_textbox.c
    ├── test_gui_button.c
    ├── test_gui_checkbox.c
    ├── test_gui_progress.c
    └── test_gui_input.c   ← Unit test Gesture Engine (bisa jalan di Linux)
```

---

## 3. Quick Start — Menampilkan GUI

### Langkah 1: Inisialisasi

Di `main.c`, kamu perlu menyiapkan tiga hal:
1. OLED hardware
2. GUI system
3. UI Manager (kalau mau pakai sistem layar wearable)

```c
#include "oled.h"
#include "GUI/gui_port.h"
#include "GUI/ui_layer/ui_manager.h"

// Variabel waktu global — WAJIB ada, digunakan oleh Gesture Engine
volatile uint32_t g_tick_ms = 0;

int main(void) {
    // 1. Inisialisasi hardware OLED
    OLED_Init();
    OLED_Clear();

    // 2. Inisialisasi sistem GUI (uGUI + Gesture Engine)
    gui_port_init();

    // 3. Inisialisasi UI Manager (semua layer wearable)
    ui_manager_init();

    // 4. Main loop
    while(1) {
        ui_manager_run_loop();  // Jalankan logika perpindahan layar
        UG_Update();            // Render semua widget ke framebuffer
        OLED_Update();          // Kirim framebuffer ke layar fisik
        g_tick_ms++;            // Tambah waktu (untuk gesture)
    }

    return 0;
}
```

### Langkah 2: Build

```bash
source /path/to/pulp-runtime/configs/pulpissimo.sh      //sesuaikan aja ya dengan cara build pulpissimo dan direktorinya
make clean all run 
```

---

## 4. Cara Menggunakan Widget uGUI

Semua widget uGUI bekerja dengan pola yang sama. Ini pola dasarnya:

```
  1. Buat Window         →  UG_WindowCreate()
  2. Buat Widget di      →  UG_XxxCreate()       (Textbox, Button, dll.)
     dalam Window
  3. Atur Properties     →  UG_XxxSetFont(), UG_XxxSetText(), ...
  4. Tampilkan Window    →  UG_WindowShow()
  5. Render              →  UG_Update() + OLED_Update()
```

> **Aturan Penting:** Setiap widget harus berada di dalam sebuah **Window**. Kamu tidak bisa membuat widget tanpa Window.

### 4.1 Textbox (Kotak Teks)

Textbox digunakan untuk menampilkan teks statis (judul, label, nilai sensor, dll).

```c
#define MAX_OBJECTS 1
static UG_WINDOW window;
static UG_OBJECT objects[MAX_OBJECTS];
static UG_TEXTBOX textbox;

void tampilkan_textbox(void)
{
    // 1. Buat window (full screen)
    UG_WindowCreate(&window, objects, MAX_OBJECTS, NULL);
    UG_WindowResize(&window, 0, 0, 127, 63);
    UG_WindowSetStyle(&window, WND_STYLE_2D);
    window.fc = C_WHITE;  // Foreground color
    window.bc = C_BLACK;  // Background color

    // 2. Buat textbox di dalam window
    //    Parameter: window, pointer, ID, x1, y1, x2, y2
    UG_TextboxCreate(&window, &textbox, TXB_ID_0, 5, 5, 122, 25);
    UG_TextboxSetFont(&window, TXB_ID_0, &FONT_6X10);
    UG_TextboxSetText(&window, TXB_ID_0, "Halo ketemu lagi hihi!");
    UG_TextboxSetAlignment(&window, TXB_ID_0, ALIGN_CENTER);

    // 3. Tampilkan
    UG_WindowShow(&window);
    UG_Update();
    OLED_Update();
}
```

**Catatan tentang newline:** Kamu bisa menggunakan `\n` di dalam teks untuk membuat baris baru:
```c
UG_TextboxSetText(&window, TXB_ID_0, "Baris pertama\nBaris kedua");
```

### 4.2 Button (Tombol)

Button bisa merespon sentuhan melalui **callback function**.

```c
#define MAX_OBJECTS 2
static UG_WINDOW window;
static UG_OBJECT objects[MAX_OBJECTS];
static UG_BUTTON button;
static UG_TEXTBOX status_text;

// Callback — dipanggil otomatis saat tombol ditekan/dilepas
static void window_callback(UG_MESSAGE* msg)
{
    if (msg->type == MSG_TYPE_OBJECT &&
        msg->id   == OBJ_TYPE_BUTTON &&
        msg->sub_id == BTN_ID_0)
    {
        if (msg->event == OBJ_EVENT_PRESSED) {
            UG_TextboxSetText(&window, TXB_ID_0, "Ditekan!");
        }
        else if (msg->event == OBJ_EVENT_RELEASED) {
            UG_TextboxSetText(&window, TXB_ID_0, "Dilepas!");
        }
    }
}

void tampilkan_button(void)
{
    // Perhatikan: callback diisi di parameter ke-4
    UG_WindowCreate(&window, objects, MAX_OBJECTS, window_callback);
    UG_WindowResize(&window, 0, 0, 127, 63);
    UG_WindowSetStyle(&window, WND_STYLE_2D);
    window.fc = C_WHITE;
    window.bc = C_BLACK;

    // Buat tombol
    UG_ButtonCreate(&window, &button, BTN_ID_0, 20, 10, 108, 35);
    UG_ButtonSetFont(&window, BTN_ID_0, &FONT_6X10);
    UG_ButtonSetText(&window, BTN_ID_0, "Klik Disini!");
    UG_ButtonSetStyle(&window, BTN_ID_0, BTN_STYLE_2D);

    // Buat textbox untuk status
    UG_TextboxCreate(&window, &status_text, TXB_ID_0, 0, 45, 127, 60);
    UG_TextboxSetFont(&window, TXB_ID_0, &FONT_6X10);
    UG_TextboxSetText(&window, TXB_ID_0, "Menunggu...");
    UG_TextboxSetAlignment(&window, TXB_ID_0, ALIGN_CENTER);

    UG_WindowShow(&window);
    UG_Update();
    OLED_Update();
}
```

**Cara men-trigger tombol** (karena belum ada sensor sentuh fisik):
```c
port_touch(true,  64, 22);   // Simulasi tekan di koordinat (64, 22)
UG_Update();
OLED_Update();
// ...tunggu sebentar...
port_touch(false, 64, 22);   // Simulasi lepas
UG_Update();
OLED_Update();
```

### 4.3 Checkbox (Kotak Centang)

```c
static UG_CHECKBOX checkbox;

UG_CheckboxCreate(&window, &checkbox, CHB_ID_0, 20, 25, 100, 40);
UG_CheckboxSetFont(&window, CHB_ID_0, &FONT_6X10);
UG_CheckboxSetText(&window, CHB_ID_0, "Opsi 1");
UG_CheckboxSetStyle(&window, CHB_ID_0, CHB_STYLE_2D);
```

Checkbox bisa di-toggle dengan `port_touch()` yang sama seperti Button.

### 4.4 Progress Bar

```c
static UG_PROGRESS progress;

// Buat progress bar
UG_ProgressCreate(&window, &progress, PGB_ID_0, 10, 20, 118, 35);
UG_ProgressSetStyle(&window, PGB_ID_0, PGB_STYLE_2D);
UG_ProgressSetProgress(&window, PGB_ID_0, 0);  // Mulai dari 0%

// Animasi: naikkan dari 0 ke 100
for (int i = 0; i <= 100; i++) {
    UG_ProgressSetProgress(&window, PGB_ID_0, i);
    UG_Update();
    OLED_Update();
    pos_delay_busy_ms(40);  // Delay 40ms per frame  [ini pake delay pulpissimo bisa di cek buat lebih jelasnya]
    g_tick_ms += 40;
}
```

---

## 5. Pola Window — Hal yang Perlu Dipahami

### 5.1 MAX_OBJECTS

Setiap window punya batas jumlah widget. Angka `MAX_OBJECTS` harus **≥ jumlah widget** di dalam window itu:

```c
#define MAX_OBJECTS 3  // Kalau ada 3 widget (misal: 2 textbox + 1 button)
static UG_OBJECT objects[MAX_OBJECTS];
```

Kalau terlalu kecil, widget terakhir tidak akan muncul (tanpa error).

### 5.2 Window Style

Selalu gunakan `WND_STYLE_2D` untuk layar monokrom:
```c
UG_WindowSetStyle(&window, WND_STYLE_2D);
```

Kalau ingin window dengan **title bar** (seperti pop-up alarm):
```c
UG_WindowSetStyle(&window, WND_STYLE_2D | WND_STYLE_SHOW_TITLE);
UG_WindowSetTitleHeight(&window, 14);
UG_WindowSetTitleText(&window, "JUDUL");
UG_WindowSetTitleTextFont(&window, &FONT_6X10);
```

### 5.3 ID Widget

Setiap tipe widget punya set ID yang terpisah:

| Tipe      | ID Tersedia                              |
|-----------|------------------------------------------|
| Textbox   | `TXB_ID_0`, `TXB_ID_1`, ... `TXB_ID_9`   |
| Button    | `BTN_ID_0`, `BTN_ID_1`, ... `BTN_ID_9`   |
| Checkbox  | `CHB_ID_0`, `CHB_ID_1`, ... `CHB_ID_9`   |
| Progress  | `PGB_ID_0`, `PGB_ID_1`, ... `PGB_ID_9`   |

> ID ini **hanya perlu unik di dalam satu window**. Dua window berbeda boleh sama-sama menggunakan `TXB_ID_0`.

### 5.4 Warna

Untuk layar monokrom, hanya ada dua warna:
- `C_WHITE` = piksel menyala (putih)
- `C_BLACK` = piksel mati (hitam)

Atur warna default window:
```c
window.fc = C_WHITE;  // Foreground (teks, garis)
window.bc = C_BLACK;  // Background
```

---

## 6. Gesture Engine — Deteksi Sentuhan

Gesture Engine dapat mengenali **8 jenis sentuhan**:

| Gesture                | Penjelasan                                        |
|------------------------|---------------------------------------------------|
| `GESTURE_CLICK`        | Ketuk pendek (< 300ms, geser < 10px)              |
| `GESTURE_DOUBLE_CLICK` | Dua ketukan cepat (jeda < 400ms)                  |
| `GESTURE_LONG_PRESS`   | Tahan lama (> 600ms)                              |
| `GESTURE_SWIPE_LEFT`   | Usap dari kanan ke kiri (> 20px)                  |
| `GESTURE_SWIPE_RIGHT`  | Usap dari kiri ke kanan (> 20px)                  |
| `GESTURE_SWIPE_UP`     | Usap dari bawah ke atas (> 20px)                  |
| `GESTURE_SWIPE_DOWN`   | Usap dari atas ke bawah (> 20px)                  |
| `GESTURE_NONE`         | Tidak ada gesture terdeteksi                      |

### Cara Menggunakan Gesture

```c
#include "GUI/gui_port.h"

while(1) {
    // 1. Baca sensor sentuh I2C (contoh pseudocode)
    int16_t touch_x, touch_y;
    bool is_pressed = CST816S_ReadTouch(&touch_x, &touch_y);

    // 2. Kirim data mentah ke sistem GUI
    port_touch(is_pressed, touch_x, touch_y);

    // 3. Cek apakah ada gesture yang terdeteksi
    gesture_event_t event;
    if (gui_port_poll_gesture(&event)) {
        switch (event.type) {
            case GESTURE_SWIPE_LEFT:
                // Pindah ke layar berikutnya
                break;
            case GESTURE_SWIPE_RIGHT:
                // Kembali ke layar sebelumnya
                break;
            case GESTURE_CLICK:
                // Aksi klik (misalnya dismiss notifikasi)
                break;
            case GESTURE_DOUBLE_CLICK:
                // Aksi double klik
                break;
        }
    }

    // 4. Jangan lupa increment waktu!
    g_tick_ms++;
}
```

### Threshold yang Bisa Disesuaikan

Semua threshold ada di `gesture.h` dan bisa diubah sesuai kebutuhan:

```c
#define GESTURE_CLICK_MAX_MS       300   // Maks durasi untuk dianggap klik
#define GESTURE_LONG_PRESS_MS      600   // Min durasi untuk long press
#define GESTURE_DCLICK_WINDOW_MS   400   // Maks jeda antar klik untuk double click
#define GESTURE_CLICK_MAX_PX        10   // Maks geser yang masih dianggap klik
#define GESTURE_SWIPE_MIN_PX        20   // Min geser untuk dianggap swipe
```

---

## 7. UI Layer — Sistem Layar Wearable

UI Layer adalah sistem yang mengorganisir layar-layar wearable. Setiap layar adalah modul independen dengan pola API yang sama.

### 7.1 Layer yang Tersedia

| Layer          | File                | Fungsi                                          |
|----------------|---------------------|-------------------------------------------------|
| Blood Oxygen   | `ui_oxygen.c`       | Menampilkan level SpO2 dari sensor PPG          |
| Activity       | `ui_activity.c`     | Menampilkan status aktivitas (WALKING, RUNNING) |
| Step Count     | `ui_stepcount.c`    | Menampilkan jumlah langkah                      |
| Alarm          | `ui_alarm.c`        | Pop-up notifikasi darurat dengan tombol Dismiss |

### 7.2 Pola API Setiap Layer

Setiap layer punya 4 fungsi standar:

```c
void UI_Xxx_Init(void);              // Inisialisasi (sekali di awal)
void UI_Xxx_Show(void);              // Tampilkan layar ini
void UI_Xxx_Hide(void);              // Sembunyikan layar ini
void UI_Xxx_SetYyy(value);           // Update data (nilai sensor, dll.)
```

### 7.3 Contoh: Menampilkan Blood Oxygen

```c
#include "GUI/ui_layer/ui_oxygen.h"

// Di inisialisasi:
UI_Oxygen_Init();

// Di main loop — tampilkan dan update:
UI_Oxygen_Show();
UI_Oxygen_SetLevel(97);   // Set SpO2 ke 97%
UG_Update();
OLED_Update();
```

### 7.4 Contoh: Menampilkan Alarm Pop-up

```c
#include "GUI/ui_layer/ui_alarm.h"

// Di inisialisasi:
UI_Alarm_Init();

// Untuk menampilkan alarm:
UI_Alarm_Show("Heart Rate\nTerlalu Tinggi!");
UG_Update();
OLED_Update();

// Untuk meng-cek apakah alarm masih aktif:
if (UI_Alarm_IsActive()) {
    // Alarm masih ditampilkan
}

// Untuk menutup alarm secara manual:
UI_Alarm_Hide();
```

### 7.5 UI Manager — Mengatur Pergantian Layar

UI Manager (`ui_manager.c`) mengelola urutan tampilan layar. Saat ini, UI Manager menjalankan **demo loop** yang berganti layar secara otomatis:

```
  Oxygen (4 detik)  →  Activity (4 detik)  →  Step Count (4 detik)  →  Alarm Pop-up (3 detik)
      ↑                                                                          │
      └────────────────────────────── loop ──────────────────────────────────────┘
```

Untuk menggunakannya di `main.c`:
```c
#include "GUI/ui_layer/ui_manager.h"

ui_manager_init();    // Sekali di awal

while(1) {
    ui_manager_run_loop();  // Otomatis berganti layar
}
```

---

## 8. Cara Membuat Layer Baru

Kalau kamu ingin menambahkan layar baru (misal: **Heart Rate**), ikuti langkah ini:

### Langkah 1: Buat file header `ui_heartrate.h`

```c
#ifndef UI_HEARTRATE_H
#define UI_HEARTRATE_H

#include <stdint.h>

void UI_HeartRate_Init(void);
void UI_HeartRate_Show(void);
void UI_HeartRate_Hide(void);
void UI_HeartRate_SetBPM(uint8_t bpm);

#endif
```

### Langkah 2: Buat file implementasi `ui_heartrate.c`

```c
#include "ui_heartrate.h"
#include "../oled.h"
#include "../GUI/gui_port.h"
#include <stdio.h>

#define MAX_OBJECTS 3
static UG_WINDOW window;
static UG_OBJECT objects[MAX_OBJECTS];
static UG_TEXTBOX title;
static UG_TEXTBOX value_text;
static UG_TEXTBOX arrow_left;

void UI_HeartRate_Init(void)
{
    UG_WindowCreate(&window, objects, MAX_OBJECTS, NULL);
    UG_WindowResize(&window, 0, 0, 127, 63);
    UG_WindowSetStyle(&window, WND_STYLE_2D);
    window.fc = C_WHITE;
    window.bc = C_BLACK;

    // Judul
    UG_TextboxCreate(&window, &title, TXB_ID_0, 0, 2, 127, 15);
    UG_TextboxSetFont(&window, TXB_ID_0, &FONT_6X10);
    UG_TextboxSetText(&window, TXB_ID_0, "Heart Rate");
    UG_TextboxSetAlignment(&window, TXB_ID_0, ALIGN_CENTER);

    // Nilai BPM
    UG_TextboxCreate(&window, &value_text, TXB_ID_1, 0, 20, 127, 45);
    UG_TextboxSetFont(&window, TXB_ID_1, &FONT_6X10);
    UG_TextboxSetText(&window, TXB_ID_1, "-- bpm");
    UG_TextboxSetAlignment(&window, TXB_ID_1, ALIGN_CENTER);
}

void UI_HeartRate_Show(void)  { UG_WindowShow(&window);  }
void UI_HeartRate_Hide(void)  { UG_WindowHide(&window);  }

void UI_HeartRate_SetBPM(uint8_t bpm)
{
    static char buf[16];
    snprintf(buf, sizeof(buf), "%d bpm", bpm);
    UG_TextboxSetText(&window, TXB_ID_1, buf);
}
```

### Langkah 3: Daftarkan di `ui_manager.c`

```c
#include "ui_heartrate.h"

void ui_manager_init(void) {
    // ... layer lain ...
    UI_HeartRate_Init();  // Tambahkan ini
}
```

### Langkah 4: Tambahkan di `Makefile`

```makefile
PULP_APP_C_SRCS = \
    ...
    GUI/ui_layer/ui_heartrate.c \
    ...
```

---

## 9. Porting ke Hardware Berbeda

Kalau kamu mengganti modul layar atau mikrokontroler, kamu **tidak perlu menulis ulang** kode GUI. Cukup ubah dua fungsi di `gui_port.c`:

### 1. Fungsi Pixel

```c
// Ubah isi fungsi ini agar memanggil DrawPixel dari display baru
static void framework_pixel(UG_S16 x, UG_S16 y, UG_COLOR c) {
    OLED_DrawPixel((uint8_t)x, (uint8_t)y, (c == C_WHITE));
}
```

### 2. Fungsi Waktu

```c
// Ubah agar mengembalikan waktu dalam milidetik dari timer hardware baru
uint32_t gesture_tick_ms(void) {
    return g_tick_ms;
}
```

Itu saja. Semua widget, gesture, dan UI layer akan otomatis bekerja dengan hardware baru.

---

## 10. Unit Test (Bisa Jalan di Linux)

Kamu bisa memverifikasi logika gesture **tanpa board fisik** menggunakan unit test yang sudah disediakan. Test ini berjalan di PC biasa dan tidak butuh hardware apapun.

```bash
cd GUI/test
make test
```

Test ini akan menjalankan **32 skenario** termasuk:
- Klik biasa vs klik dengan sedikit geser
- Double click dan batas waktu antar-klik
- Long press
- Swipe ke 4 arah
- Verifikasi rendering uGUI tidak crash

> **catatan:** ini maksudnya untuk yang gesture ya, khususnya `test_gui_input.c`

---

## 11. Referensi Cepat — Fungsi yang Paling Sering Dipakai

| Kebutuhan                    | Fungsi                                                  |
|------------------------------|---------------------------------------------------------|
| Inisialisasi GUI             | `gui_port_init()`                                       |
| Buat window                  | `UG_WindowCreate(&wnd, objs, n, callback)`              |
| Atur ukuran window           | `UG_WindowResize(&wnd, x1, y1, x2, y2)`                 |
| Tampilkan window             | `UG_WindowShow(&wnd)`                                   |
| Sembunyikan window           | `UG_WindowHide(&wnd)`                                   |
| Buat textbox                 | `UG_TextboxCreate(&wnd, &tb, ID, x1, y1, x2, y2)`       |
| Set teks                     | `UG_TextboxSetText(&wnd, ID, "teks")`                   |
| Buat tombol                  | `UG_ButtonCreate(&wnd, &btn, ID, x1, y1, x2, y2)`       |
| Buat progress bar            | `UG_ProgressCreate(&wnd, &pg, ID, x1, y1, x2, y2)`      |
| Update progress              | `UG_ProgressSetProgress(&wnd, ID, persen)`              |
| Buat checkbox                | `UG_CheckboxCreate(&wnd, &cb, ID, x1, y1, x2, y2)`      |
| Render ke framebuffer        | `UG_Update()`                                           |
| Kirim ke layar fisik         | `OLED_Update()`                                         |
| Simulasi sentuhan            | `port_touch(pressed, x, y)`                             |
| Cek gesture                  | `gui_port_poll_gesture(&event)`                         |

---

## 12. FAQ & Troubleshooting 

**Q: Widgetnya gabisa muncul di layar, kenapa?**
A: Pastikan tiga hal:
1. `MAX_OBJECTS` cukup besar untuk menampung semua widget di window.
2. Kamu sudah memanggil `UG_WindowShow()`.
3. Kamu sudah memanggil `UG_Update()` diikuti `OLED_Update()`.

**Q: Tombol gak merespon sentuhan, kenapa?**
A: Pastikan:
1. Window dibuat dengan **callback function** (parameter ke-4 di `UG_WindowCreate`).
2. Kamu memanggil `port_touch()` dengan koordinat yang benar (harus berada di dalam area tombol).
3. Kamu memanggil `UG_Update()` setelah `port_touch()`.

**Q: Tampilan terlihat kacau / ada artefak aneh?**
A: Gunakan `WND_STYLE_2D` dan pastikan warna window sudah diatur:
```c
window.fc = C_WHITE;
window.bc = C_BLACK;
```

**Q: Bisakah menampilkan lebih dari satu window sekaligus?**
A: Bisa. Misalnya alarm pop-up ditampilkan di atas layar yang sedang aktif. Tapi untuk layar utama (Oxygen, Activity, dll.), sebaiknya **sembunyikan dulu yang lain** sebelum menampilkan yang baru agar tidak tumpang tindih.
