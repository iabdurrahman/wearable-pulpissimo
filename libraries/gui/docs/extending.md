# NusaCore GUI Framework — Extension Guide

How to add new **input devices**, **output devices**, and **EEZ Studio UI screens** to the framework without changing the rest of the codebase.

---

## Table of contents

1. [Framework architecture recap](https://www.google.com/search?q=%231-framework-architecture-recap)
2. [Adding a new output device (display)](https://www.google.com/search?q=%232-adding-a-new-output-device-display)
* 2a. Different OLED controller
* 2b. TFT / colour display over SPI


3. [Adding a new input device](https://www.google.com/search?q=%233-adding-a-new-input-device)
* 3a. Different touchscreen controller (same gesture engine)
* 3b. Physical push button (new LVGL indev type)


4. [Adding a new UI screen layout via EEZ Studio](https://www.google.com/search?q=%234-adding-a-new-ui-screen-layer-via-eez-studio)
* 4a. Creating and exporting screens
* 4b. UI Controller Patterns (`src/display_layer.c`)


5. [How sensor tasks push data to the UI](https://www.google.com/search?q=%235-how-sensor-tasks-push-data-to-the-ui)

---

## 1. Framework architecture recap

```
main.c
  ├── display_driver_init()   ← ONE display, registered once
  ├── input_driver_init()     ← ONE indev, registered once
  ├── input_driver_set_gesture_cb(on_gesture)
  └── ui_init()               ← Load generated layout from EEZ Studio
        ├── Tile 0: Clock
        ├── Tile 1: Health
        └── Tile 2: Fitness   ← Bound to backend data via src/display_layer.c

```

**Rule of thumb:**

* New *hardware output* → edit `src/display_driver.c` only
* New *hardware input* → edit `src/input_driver.c` only (gesture engine stays)
* New *UI visual design changes* → edit in **EEZ Studio**, click **Export**, and add any custom widget text-formatting functions into `src/display_layer.c`
* New *Unit Testing Validation* → run `make ui` inside `tests/` directory to instantly verify data parsing bounds locally.

---

## 2. Adding a new output device (display)

All display code lives in `src/display_driver.c`. The rest of the codebase only calls `display_driver_init()` and is completely abstract to the hardware layer.

### How `display_driver.c` works

```
display_driver_init()
 │
 ├─ [Linux]   lv_sdl_window_create(W, H)          ← no flush_cb needed
 │
 └─ [RISC-V]  lv_display_create(W, H)
                lv_display_set_flush_cb(disp, oled_flush_cb)
                lv_display_set_buffers(...)
                       │
                       ▼
               oled_flush_cb(disp, area, px_map)
                 │  Convert pixel format if needed
                 │  Send pixels to hardware
                 └  lv_display_flush_ready(disp)   ← MUST call this

```

### 2a. Swapping to a different OLED controller (e.g. SH1106 instead of SSD1306)

SH1106 uses a *page-based* write (no auto-increment across pages). Only `oled_flush_cb` needs to change.

**In `src/display_driver.c`, RISC-V section:**

```c
static void oled_flush_cb(lv_display_t *disp,
                          const lv_area_t *area,
                          uint8_t *px_map)
{
    int32_t w = lv_area_get_width(area);
    int32_t h = lv_area_get_height(area);

    // Convert LVGL pixel buffer → 1-bit (0 = off, 1 = on)
    // px_map is 16-bit RGB565 (see lv_conf.h LV_COLOR_DEPTH 16)
    uint16_t *src = (uint16_t *)px_map;

    for (int32_t row = area->y1; row <= area->y2; row++) {
        int page = row / 8;
        int bit  = row % 8;

        // SH1106: set page + column address before each row
        bsp_i2c_cmd(0xB0 | page);               // set page
        bsp_i2c_cmd(0x00 | (area->x1 & 0x0F));  // col low nibble
        bsp_i2c_cmd(0x10 | (area->x1 >> 4));    // col high nibble

        uint8_t byte = 0;
        for (int32_t col = area->x1; col <= area->x2; col++) {
            int idx = (row - area->y1) * w + (col - area->x1);
            if (src[idx]) byte |= (1 << bit);
        }
        bsp_i2c_data(&byte, 1);
    }

    lv_display_flush_ready(disp);   // ← always required
}

```

### 2b. Adding a TFT colour display (e.g. ST7789, ILI9341) over SPI

TFT displays use 16-bit RGB565 natively — which is exactly what LVGL uses at `LV_COLOR_DEPTH 16`. No pixel conversion needed.

**Step 1 — Update `lv_conf.h**` if switching the RISC-V build to 16-bit:

```c
#define LV_COLOR_DEPTH  16

```

**Step 2 — Edit `src/display_driver.h**`:

```c
// Change the resolution constants to match the TFT
#define DISPLAY_HOR_RES  240    // e.g. 240×240 round TFT
#define DISPLAY_VER_RES  240

```

**Step 3 — Edit `src/display_driver.c**` (RISC-V section):

```c
/* Include TFT module driver header */
#include "tft.h"

static uint16_t g_draw_buf[DISPLAY_HOR_RES * 20];  // 20-line partial band buffer

static void tft_flush_cb(lv_display_t *disp,
                         const lv_area_t *area,
                         uint8_t *px_map)
{
    // px_map is already RGB565 — send directly to TFT via SPI
    tft_set_window(area->x1, area->y1, area->x2, area->y2);
    tft_write_pixels((uint16_t *)px_map,
                     lv_area_get_width(area) * lv_area_get_height(area));

    lv_display_flush_ready(disp);
}

lv_display_t *display_driver_init(void)
{
    tft_init();   

    lv_display_t *disp = lv_display_create(DISPLAY_HOR_RES, DISPLAY_VER_RES);
    lv_display_set_flush_cb(disp, tft_flush_cb);
    lv_display_set_buffers(disp, g_draw_buf, NULL,
                            sizeof(g_draw_buf),
                            LV_DISPLAY_RENDER_MODE_PARTIAL);
    return disp;
}

```

---

## 3. Adding a new input device

All input code lives in `src/input_driver.c`. The gesture processing engine (`src/gesture.c`) is untouched regardless of the input hardware.

### 3a. Swapping to a different touchscreen controller

Only the inside of `touch_read_cb` changes.

**Find this block in `src/input_driver.c**` (RISC-V section):

```c
static void touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    bool    pressed = false;
    int16_t x = 0, y = 0;

    /* Replace with your hardware read call */
    feed_and_report(pressed, x, y, data);
}

```

**Replace the stubs** with your controller's read call:

```c
// Example: CST816S via ICDeC touch module
#include "touch.h"

static void touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    touch_point_t pt;
    bool pressed = touch_read(&pt);   // returns true if a finger touch is registered
    feed_and_report(pressed, pt.x, pt.y, data);
}

```

The `feed_and_report()` helper automatically coordinates gesture states and signals your UI callbacks cleanly.

### 3b. Adding a physical push button

Buttons use `LV_INDEV_TYPE_KEYPAD` instead of `LV_INDEV_TYPE_POINTER`. They map to LVGL's focus/navigation group system rather than to x/y coordinates.

**Step 1 — Add a second indev configuration in `input_driver_init()`:**

```c
static void button_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    bool pressed = (gpio_read(BUTTON_PIN) == 0);  // Active-low push button

    // Map hardware button edge status → LVGL virtual key code
    data->key   = LV_KEY_ENTER;   
    data->state = pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

// Register inside input_driver_init:
lv_indev_t *btn_indev = lv_indev_create();
lv_indev_set_type(btn_indev, LV_INDEV_TYPE_KEYPAD);
lv_indev_set_read_cb(btn_indev, button_read_cb);
lv_indev_set_display(btn_indev, display);

```

**Step 2 — Make UI objects respond to key events** by creating an asset navigation group inside `main.c` following `ui_init()`:

```c
lv_group_t *grp = lv_group_create();
lv_group_set_default(grp);
lv_indev_set_group(btn_indev, grp);

// Add your interactive screens or text objects to enable selection highlight
lv_group_add_obj(grp, my_button_widget);

```

---

## 4. Adding a new UI screen layout via EEZ Studio

Adding a screen block under the current template pipeline means establishing visually managed child panels or containers inside EEZ Studio's canvas, giving the necessary text elements explicit names to generate standard C variables, and handling tile snapping or state processing loops via the decoupled **Display Layer** controller.

### 4a. Step-by-Step Design Workflow

1. **Visual Generation inside EEZ Studio**: Launch your project and drop layouts natively onto your master root `Tileview` parent object component window.
2. **Assign Unique Names**: Select each target sensor element variable node. On the properties panel aligned along the right monitor boundary edge, locate the **Name** input field box and supply a unique descriptive programmatic token flag (e.g., `steps_label`, `activity_label`, or `notification_label`).
3. **Export Code**: Click **Project -> Export Code**. This operation updates the tracking variables list structure located inside `src/ui/ui.h` and `src/ui/screens.c`.
4. **Update Main Navigation Bounds**: Adjust your limits inside `main.c`:
```c
#define TILE_COUNT 6   // Increment total tracking layers appropriately

```



---

### 4b. UI Controller Patterns (`src/display_layer.c`)

When you assign custom names to widgets inside the visual designer, they are populated into a shared global application architecture structure called `objects`. To keep the codebase clean and completely unit-testable, all widget updates must live within `src/display_layer.c`.

#### 1. Clock & Calendar Display

```c
void ui_update_clock_and_date(const struct tm *time_info)
{
    if (!time_info) return;

    if (objects.clock_label) {
        lv_label_set_text_fmt(objects.clock_label, "%02d:%02d:%02d",
                              time_info->tm_hour, time_info->tm_min, time_info->tm_sec);
    }
    if (objects.date_label) { 
        lv_label_set_text_fmt(objects.date_label, "%02d/%02d/%04d",
                              time_info->tm_mday, time_info->tm_mon + 1, time_info->tm_year + 1900);
    }
}

```

#### 2. Vital Health Metrics (Heart Rate, SpO2 & Steps)

```c
void ui_update_health_metrics(int bpm, int spo2, int steps)
{
    if (objects.heartbeat_label) {
        lv_label_set_text_fmt(objects.heartbeat_label, "%d bpm", bpm);
    }
    if (objects.o2_label) {
        lv_label_set_text_fmt(objects.o2_label, "SpO2: %d%%", spo2);
    }
    if (objects.steps_label) {
        lv_obj_set_style_text_align(objects.steps_label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(objects.steps_label, LV_ALIGN_CENTER, 0, 0);
        lv_label_set_text_fmt(objects.steps_label, "%d steps", steps);
    }
}

```

#### 3. Real-Time Activity Recognition Classifier State

```c
void ui_update_activity_state(int bpm)
{
    if (!objects.activity_label) return;

    if (bpm > 80) {
        lv_label_set_text(objects.activity_label, "State: RUNNING");
    } else if (bpm > 72) {
        lv_label_set_text(objects.activity_label, "State: WALKING");
    } else {
        lv_label_set_text(objects.activity_label, "State:  STILL ");
    }
}

```

---

## 5. How Sensor Tasks Push Data to the UI

LVGL is **not thread-safe** by default on bare-metal single-core processing environments (the structural baseline target configuration for the NusaCore SoC). Because all hardware drivers, tick loops, and graphics engines execute synchronously inside a single continuous main context `while(1)` pipeline loop, modifying element values via decoupled display layer functions is safe and predictable.

The core application event loop should focus exclusively on timing, sensor acquisition, and data dispatching:

```c
#include "display_layer.h"

int main(void)
{
    // ... System Initializations (LVGL, Input drivers, UI layouts) ...
    ui_init();

    while(1) {
        lv_tick_inc(5);
        ui_tick(); 

        #ifdef PLATFORM_LINUX
        // =====================================================================
        // LINUX HOST ENVIRONMENT - SIMULATION EMULATOR LOOP
        // =====================================================================
        time_t raw_time = time(NULL);
        struct tm *time_info = localtime(&raw_time);

        // Dispatched system time update
        ui_update_clock_and_date(time_info);

        // Simulation variance updates roughly every 1.5 seconds
        static uint32_t sim_ticks = 0;
        static int current_steps = 1420;
        sim_ticks++;
        
        if (sim_ticks % 300 == 0) {
            int simulated_bpm = 65 + (rand() % 21);
            int simulated_spo2 = 97 + (rand() % 3);
            current_steps += (rand() % 5);

            // Pass values straight to decoupled processing layers
            ui_update_health_metrics(simulated_bpm, simulated_spo2, current_steps);
            ui_update_activity_state(simulated_bpm);
            ui_update_notifications(time_info);
        }
        usleep(5000); // 5 ms sleep preserves host CPU cycles
        
        #else
        // =====================================================================
        // BARE-METAL HARDWARE INTERFACE LOOP (NusaCore RISC-V Target)
        // =====================================================================
        struct tm rtc_time;
        if (bsp_rtc_read_time(&rtc_time)) {
             ui_update_clock_and_date(&rtc_time); 
             ui_update_notifications(&rtc_time);
        }
        
        if (bsp_i2c_sensor_data_ready()) {
            int real_bpm  = bsp_max30102_read_bpm();
            int real_spo2 = bsp_max30102_read_spo2();
            int current_steps = bsp_pedometer_get_steps();

            ui_update_health_metrics(real_bpm, real_spo2, current_steps);
            ui_update_activity_state(real_bpm);
        }
        #endif

        // Instruct the rendering core to refresh mutated visual positions
        lv_timer_handler();   
    }
    return 0;
}

```

If the NusaCore RISC-V system configurations migrate towards a multi-core symmetric multi-processing platform structure or utilize an RTOS environment where independent task blocks alter variables asynchronously from separate processing threads, wrap your UI state updates inside a critical protection block:

```c
// Example multi-threaded protection pattern
lv_lock();     // Assert memory protection lock flag over the graphics driver engine
ui_update_health_metrics(async_bpm, async_spo2, async_steps);
lv_unlock();   // Release execution permission boundaries back to scheduling task loops

```

---

## Quick Reference — Which File to Edit

| Goal | Target Location | Implementation Action Required |
| --- | --- | --- |
| New OLED / TFT display | `src/display_driver.c` (RISC-V section) | Integrate specific hardware initialization code and implement target blit loops inside `oled_flush_cb`. |
| Display size change | `src/display_driver.h` + `lv_conf.h` | Modify the resolution layout sizing definitions macros (`DISPLAY_HOR_RES` / `DISPLAY_VER_RES`). |
| New touch controller | `src/input_driver.c` | Update peripheral communications channels directly inside the `touch_read_cb` function block. |
| New button / encoder | `src/input_driver.c` | Instantiate a secondary device object interface mapped directly to the `LV_INDEV_TYPE_KEYPAD` protocol. |
| Adjust Screen Space Res | **EEZ Studio UI Layouts** | Modify layouts visually, select object element names, and execute project export commands into the source folder. |
| Modify Display Data Updates | `src/display_layer.c` | Update core string-formatting definitions or logical thresholds. |
| Pass Sensors to UI Screens | `main.c` (Event Loop) | Read hardware/simulated metrics and route them directly via `ui_update_*` functions. |
