# LVGL GUI Framework

An optimized, deployment-ready LVGL v9 GUI framework for the **ICDeC RISC-V Wearable Device**. 

This system compiles into an interactive, high-performance SDL2 window on a host Linux environment for fluid application development and visual layout prototyping. It seamlessly cross-compiles to target the bare-metal **NusaCore RISC-V platform** to drive hardware peripherals over low-bandwidth display buses.

---

## Prerequisites

### Linux Host Development Environment (SDL2 Preview Window)
```bash
sudo apt update && sudo apt install -y \
  build-essential \
  cmake \
  pkg-config \
  python3-pip \
  python3-venv \
  libwayland-dev \
  libxkbcommon-dev \
  libwayland-bin \
  wayland-protocols \
  libsdl2-dev 

```

### RISC-V Deployment Toolchain

Target cross-compilation requires access to the following dependencies mapped into your execution profile:

* `chroot.tar.xz` — Dedicated RISC-V GCC cross-compiler (`riscv32-unknown-elf-gcc`)
* `pulp-runtime` — Low-level System-on-Chip (SoC) peripheral hardware layers

---

## First-Time Setup

Initialize the repository alongside your project layout dependencies by running the one-shot configuration tool script:

```bash
git clone <this-repo-url>
cd gui

./setup.sh

```

---

## Building and Execution

### 1. Linux Host Preview (SDL2 Application)

```bash
make PLATFORM=linux
./build/gui_app

```

A **512 × 256 px SDL window** will open, presenting a 4× anti-aliased viewport representation of the physical 128 × 64 monochrome hardware profile.

| Desktop Mouse Interaction | Classified Gesture Action |
| --- | --- |
| Left-click + immediate release | `GESTURE_CLICK` |
| Two rapid successive clicks | `GESTURE_DOUBLE_CLICK` |
| Maintain press point stationary ≥ 600 ms | `GESTURE_LONG_PRESS` |
| Drag click coordinates left/right ≥ 20 px | `GESTURE_SWIPE_LEFT` / `GESTURE_SWIPE_RIGHT` |
| Drag click coordinates up/down ≥ 20 px | `GESTURE_SWIPE_UP` / `GESTURE_SWIPE_DOWN` |

### 2. Isolated Automated Unit Testing Pipelines

Execute fast, non-graphical structural and algorithmic assertion suites natively on your host shell environment:

```bash
cd tests

make          # Build and run BOTH test blocks back-to-back
make gesture  # Compile and test ONLY the touch vector classification engine
make ui       # Compile and test ONLY layout data structures and safety bounds

```

### 3. Cross-Compiling for NusaCore RISC-V Hardware Target

Ensure that your development terminal session has loaded your cross-compiler toolchain binaries path parameters before generating your final production images:

```bash
# Verification check: confirm the cross-compiler toolchain is accessible
riscv32-unknown-elf-gcc --version

# Compile production bare-metal target
make PLATFORM=riscv
echo $?     # Verification check: must return 0

# Production Binary Location: build/gui_app

```

---

## Project Structure

```
gui/
├── setup.sh                 First-time environment setup and dependency wrapper
├── lv_conf.h                LVGL v9 structural compilation parameters (handled via #ifdefs)
├── main.c                   Application execution entry point + core sensor polling loop
├── Makefile                 Platform-aware unified build driver (linux | riscv)
├── libs/
│   └── lvgl/                LVGL v9 pure library source tree (tracked via Git Submodule)
├── src/
│   ├── ui/                  Auto-generated clean UI layouts exported from EEZ Studio
│   │   ├── ui.h / .c
│   │   └── screens.c
│   ├── display_driver.{h,c} SDL2 window surface ↔ OLED peripheral abstraction layers
│   ├── input_driver.{h,c}   SDL2 mouse event pointers ↔ hardware touch device registers
│   └── gesture.{h,c}        Isolated pure-C click, long-press, and vector swipe classifier
├── tests/
│   ├── test_gesture.c       Touch pattern duration and drift threshold evaluations
│   ├── test_ui_layers.c     Layout state modifications, string formatting, and buffer bounds checks
│   └── Makefile             Segmented target automation configurations
└── docs/
    ├── porting_guide.md     Platform driver integration protocols and hardware setups
    ├── display_layer.md     Data-binding bridge API formatting sensor metrics to text labels
    └── extending.md         Procedures for scaling out layout panels, sensors, and components

```

---

## LVGL Framework Dependency Management

To prevent cluttering project history tracks, core graphics engine library source trees are completely decoupled from primary uploads and instead pin down specific target code branches inside the official project repository lines (`release/v9.2`).

To pull all layout dependencies down during your initialization sequence, compile using the recurse parameter flag:

```bash
# Clone the repository along with all tracked submodules in a single step
git clone --recurse-submodules <this-repo-url>

# Alternatively, initialize components retroactively inside an existing working copy
git submodule update --init --recursive

```

To advance or lock down a newer stable patch modification version layer:

```bash
cd libs/lvgl
git fetch origin
git checkout release/v9.2
cd ../..
git add libs/lvgl
git commit -m "chore: advance framework submodules patch to newer v9.2 distribution release"

```

---

## Connecting Hardware Peripherals

| Functional Sub-Layer | Underlying File Target | Architectural Implementation Hook |
| --- | --- | --- |
| Physical OLED Display Panel | `src/display_driver.c` | Map the pixel buffer stream to your target peripheral page coordinates inside `oled_flush_cb()`. |
| Physical Touch Sensor Panel | `src/input_driver.c` | Extract coordinate vectors from hardware pins inside `touch_read_cb()`. |
| Pushing Live Sensor Telemetry | `main.c` (Core Event Loop) | Intercept and route telemetry fields to the formatting layer wrappers defined in `docs/display_layer.md`. |

*For complete implementation patterns, visual tool setups, and source configurations, read the developer instructions inside the `docs/` folder.*

---

## Technical Documentation Index

| Resource Location | Target Engineering Focus |
| --- | --- |
| `docs/porting_guide.md` | Build setup recipes, compiler configuration guides, and step-by-step physical hardware abstraction layer integration guides. |
| `docs/display_layer.md` | High-level controller API overview documenting how raw sensor records are translated into clean text formats. |
| `docs/extending.md` | Procedures for establishing custom layout panels inside EEZ Studio, assigning component variables, and wiring runtime updates. |
