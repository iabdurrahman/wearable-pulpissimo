# LVGL GUI Framework — Porting & Developer Guide

**Project:** ICDeC RISC-V Wearable  
**GUI Stack:** LVGL v9 · SDL2 (Linux dev) · RISC-V bare-metal (target) · EEZ Studio (Layout Engine)

---

## 1. Architecture Overview


```

┌────────────────────────────────────────────────────────────┐
│  Application Scheduling  (main.c)                          │
│  • Performs main bare-metal while(1) loop execution        │
│  • Polls RTC, PPG, I2C, and coordinates system states      │
└─────────────────────────────┬──────────────────────────────┘
│ Dispatches raw metrics data
▼
┌────────────────────────────────────────────────────────────┐
│  Decoupled UI Display Layer  (src/display_layer.c)         │
│  • Maps system variables directly onto named EEZ widgets   │
│  • Manages text formatting blocks (e.g., "%d bpm")         │
│  • **100% Unit-Testable on Host Linux via Mocks**          │
├──────────────────┬─────────────────────────────────────────┤
│  display_driver  │  input_driver                           │
│  (src/)          │  (src/)                                 │
│                  │                                         │
│  Linux:          │  Linux:                                 │
│  lv_sdl_window   │  lv_sdl_mouse  ─► gesture engine        │
│                  │                                         │
│  RISC-V:         │  RISC-V:                                │
│  OLED module ◄── │  touch module ─► gesture engine         │
│  flush_cb        │  read_cb                                │
├──────────────────┴─────────────────────────────────────────┤
│  gesture.c  (src/)  — pure C, zero LVGL dependency         │
│  click · double-click · long-press · swipe ×4              │
├────────────────────────────────────────────────────────────┤
│  LVGL v9  (libs/lvgl/)                                     │
├─────────────────────┬──────────────────────────────────────┤
│  Linux / SDL2       │  RISC-V / pulp-runtime               │
└─────────────────────┴──────────────────────────────────────┘

```

Everything above the `display_driver` / `input_driver` boundary line is completely platform-independent and is preserved identically across targets.

---

## 2. Project Structure


```

gui/
├── setup.sh                 One-shot system setup environment tool
├── lv_conf.h                LVGL v9 layout configurations (handled via #ifdefs)
├── main.c                   Main event loop execution + raw hardware polling loop
├── Makefile                 PLATFORM=linux | riscv configuration driver
├── docs/
│   ├── extending.md         Guide for expanding inputs, outputs, and UI layouts
│   ├── display_layer.md     API reference guide for the decoupled UI processor
│   └── porting_guide.md     This file
├── libs/
│   └── lvgl/                LVGL v9 native submodule repository
├── src/
│   ├── ui/                  Auto-generated clean layout output files from EEZ Studio
│   │   ├── ui.h / .c
│   │   └── screens.c
│   ├── display_layer.h / .c Decoupled text-formatting layer (Binds variables to UI)
│   ├── display_driver.h / .c Target platform visual hardware init
│   ├── input_driver.h   / .c Target platform hardware read + gesture routers
│   └── gesture.h        / .c Pure-C isolated gesture classification state machine
└── tests/
    ├── test_gesture.c       Unit tests tracking touch interaction mechanics
    ├── test_ui_layers.c     Unit tests checking display states and string mutations
    └── Makefile             Segmented targets (make, make gesture, make ui)

```

---

## 3. Quick Start

### 3.1 First-Time Setup

```bash
# Install host Linux development primitives
sudo apt install build-essential libsdl2-dev git     # Ubuntu/Debian

# Execute build layout configuration
./setup.sh

```

### 3.2 Build and Run on Linux (SDL2 Desktop Preview)

```bash
make PLATFORM=linux
./build/gui_app

```

You will see a **512 × 256 px SDL window** (4× scaled aspect view of your 128 × 64 monochrome OLED hardware configuration target).

| Desktop Mouse Action | Gesture Generated |
| --- | --- |
| Left-click and immediate release | `GESTURE_CLICK` |
| Two quick left-clicks sequentially | `GESTURE_DOUBLE_CLICK` |
| Click and remain stationary (≥ 600 ms) | `GESTURE_LONG_PRESS` |
| Click and drag left/right ≥ 20 px | `GESTURE_SWIPE_LEFT` / `GESTURE_SWIPE_RIGHT` |
| Click and drag up/down ≥ 20 px | `GESTURE_SWIPE_UP` / `GESTURE_SWIPE_DOWN` |

### 3.3 Execute Automated Unit Testing Suites

```bash
cd tests
make         # Executes both isolated testing pipelines back-to-back
make gesture # Runs only touch pattern recognition checks
make ui      # Runs only layout structural mutations and text checks

```

### 3.4 Cross-Compiling for NusaCore RISC-V Target Hardware

```bash
. ./env.sh                        # Load target toolchain profile paths (Once per terminal session)
make PLATFORM=riscv
echo $?                           # Verification check: Must return 0
# Target Output Binary Location: build/gui_app

```

---

## 4. Connecting the ICDeC Hardware Peripherals

To switch out development stubs for physical microcontrollers, implement communication calls inside **`src/display_driver.c`** and **`src/input_driver.c`**.

### 4.1 Monochrome Display — `src/display_driver.c`

Open **`lv_conf.h`** and confirm that the execution mode is set to 1-bit monochrome formatting bounds:

```c
#define LV_COLOR_DEPTH 1

```

Inside `oled_flush_cb`, map the incoming memory stream indices into the page-by-page serialization logic layout demanded by the SSD1306/SH1106 visual memory map:

```c
static void oled_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    int32_t w = lv_area_get_width(area);
    int32_t h = lv_area_get_height(area);

    // Reconstruct monochrome bits into structural layout pages (8-pixel vertical blocks)
    for (int32_t row = area->y1; row <= area->y2; row++) {
        int page = row / 8;
        int bit  = row % 8;

        // Implement low-level target hardware bus address configurations (I2C/SPI)
        bsp_i2c_set_page_address(page);
        bsp_i2c_set_column_address(area->x1);

        for (int32_t col = area->x1; col <= area->x2; col++) {
            // Read corresponding bit array values out of the processed LVGL map
            uint8_t target_byte = calculate_mono_byte(px_map, col, row);
            bsp_i2c_send_data_byte(target_byte);
        }
    }

    lv_display_flush_ready(disp); // CRITICAL: Relinquish layout canvas resource locks
}

```

### 4.2 Touch Peripheral Input — `src/input_driver.c`

Incorporate your specific screen driver initialization sequence inside `touch_read_cb`. Ensure that coordinate vectors are updated during **both** touch states (`PRESSED` and `RELEASED`) so that the drag acceleration deltas are computed cleanly by the underlying tracking matrix:

```c
static void touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    int16_t x = 0, y = 0;
    bool pressed = bsp_touch_hardware_poll(&x, &y);

    // Route coordinates to the custom gesture classification engine
    gesture_feed(pressed, x, y);

    gesture_event_t ev;
    if (gesture_poll(&ev) && g_gesture_cb != NULL) {
        g_gesture_cb(&ev); // Notify root event layout interceptors inside main.c
    }

    // Forward telemetry captures back to LVGL v9 tracking registers
    data->point.x = x;
    data->point.y = y;
    data->state   = pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

```

---

## 5. Gesture Engine Reference (`src/gesture.h`)

### Gesture Types

| Identifier Token | Verification Threshold Properties |
| --- | --- |
| `GESTURE_CLICK` | Press + release action within `< 300 ms`, spatial movement `< 10 px`. |
| `GESTURE_DOUBLE_CLICK` | Two successive clicks recorded within a `< 400 ms` window interval. |
| `GESTURE_LONG_PRESS` | Position hold extended `≥ 600 ms` with spatial drift `< 10 px`. |
| `GESTURE_SWIPE_LEFT` | Rapid directional swipe drag to the left matching `> 20 px`. |
| `GESTURE_SWIPE_RIGHT` | Rapid directional swipe drag to the right matching `> 20 px`. |
| `GESTURE_SWIPE_UP` | Rapid directional swipe drag upwards matching `> 20 px`. |
| `GESTURE_SWIPE_DOWN` | Rapid directional swipe drag downwards matching `> 20 px`. |

### Tuning Configuration Parameters

Modify these macro definitions inside **`src/gesture.h`** to tune screen touch behaviors:

```c
#define GESTURE_CLICK_MAX_MS      300   // Increase if soft clicks are drop-failing
#define GESTURE_LONG_PRESS_MS     600   // Lower if contextual popups feel laggy
#define GESTURE_DCLICK_WINDOW_MS  400   // Calibrate double click detection windows
#define GESTURE_CLICK_MAX_PX       10   // Increase if physical hand shaking triggers unintended swipes
#define GESTURE_SWIPE_MIN_PX       20   // Lower if physical swipe bounds are too stiff

```

---

## 6. Injecting Live Sensor Data to UI Display Layers

Because layouts are constructed in **EEZ Studio**, do not write manual string formatting code or directly mutate widget properties inside `main.c`. Instead, pass raw data directly to the functions inside `src/display_layer.c`. This preserves decoupling and lets the testing framework evaluate your visual boundaries.

```c
// Inside your main bare-metal while(1) polling loop framework inside main.c:

// 1. Dispatch Real-Time Clock data to updating layers
struct tm rtc_time;
if (bsp_rtc_read_time(&rtc_time)) {
    ui_update_clock_and_date(&rtc_time);
    ui_update_notifications(&rtc_time);
}

// 2. Poll hardware sensors and dispatch clean metrics directly
if (bsp_i2c_sensor_data_ready()) {
    int active_bpm  = bsp_max30102_read_bpm();
    int active_spo2 = bsp_max30102_read_spo2();
    int total_steps = bsp_pedometer_get_steps();

    // The display layer automatically addresses layout positioning, centering, and parsing string text fields
    ui_update_health_metrics(active_bpm, active_spo2, total_steps);
    ui_update_activity_state(active_bpm);
}

```

---

## 7. Platform Compilation Reference Flags

| Target Preprocessor Macro | Invocation Instruction | System Architectural Consequence |
| --- | --- | --- |
| `PLATFORM_LINUX` | `make PLATFORM=linux` | Links SDL2 display window and emulated mouse pointer parameters. |
| `PLATFORM_RISCV` | `make PLATFORM=riscv` | Compiles targeting NusaCore SoC with custom hardware configuration initialization files. |
| `UNIT_TESTING` | Handled internally in `tests/` | Circumvents heavy dependencies to allow mock-driven compilation on Linux hosts. |

---

## 8. Known Limitations / Engineering Backlog

| Operational Target Component | Current Project Status | Resolution & Engineering Requirement Notes |
| --- | --- | --- |
| Tick Source (RISC-V) | **PENDING** | Replace software busy loop delay steps inside `main.c` with a true 1 ms hardware Timer ISR calling `lv_tick_inc(1)`. |
| Screen Refresh Animations | **DISABLED** | Animations should remain disabled (`LV_ANIM_OFF`) on monochrome I2C configurations to prevent bus bandwidth bottlenecks. |
| Multi-Touch Engine | **UNSUPPORTED** | Gesture processing engine architecture is restricted to single-finger vector changes. |
| Screen Brightness Control | **PENDING** | Connect display hardware power rails to an available PWM line on the NusaCore SoC interface to handle brightness scaling. |
