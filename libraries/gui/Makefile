# =============================================================================
# NusaCore GUI Framework — Makefile
# ICDeC RISC-V Wearable Project
#
# Usage:
#   make PLATFORM=linux        Build for Linux/SDL2  (default)
#   make PLATFORM=riscv        Build for RISC-V / pulp-runtime
#   make clean
#
# Linux prerequisites:
#   sudo apt install libsdl2-dev   (Ubuntu/Debian)
#   sudo pacman -S sdl2            (CachyOS/Arch)
#
# RISC-V prerequisites:
#   . ../env.sh    (run once per terminal session)
# =============================================================================

PLATFORM ?= linux

BUILD_DIR = build

.PHONY: default_target
default_target: all

# =============================================================================
# LVGL dependency check
# LVGL is a git submodule and must be initialised before building.
# =============================================================================
LVGL_DIR  = libs/lvgl
LVGL_HEADER = $(LVGL_DIR)/lvgl.h

$(LVGL_HEADER):
	$(error LVGL not found. Run:  git submodule update --init  OR  ./setup.sh)

# =============================================================================
# Sources (shared between both targets)
# =============================================================================
APP_SRCS = \
    main.c \
    src/display_driver.c \
    src/input_driver.c \
    src/gesture.c

LVGL_SRCS := $(shell find $(LVGL_DIR)/src -type f -name '*.c')
ALL_SRCS := $(APP_SRCS) $(LVGL_SRCS)

COMMON_CFLAGS = \
    -I. \
    -Isrc \
    -I$(LVGL_DIR) \
    -DLV_CONF_INCLUDE_SIMPLE \
    -DLV_LVGL_H_INCLUDE_SIMPLE

# =============================================================================
# LINUX target
# =============================================================================
ifeq ($(PLATFORM), linux)

TARGET  = $(BUILD_DIR)/gui_app
CC      = gcc
CFLAGS  = -O0 -g -Wall -Wextra \
          $(COMMON_CFLAGS) \
          -DPLATFORM_LINUX \
          $(shell sdl2-config --cflags)

LDFLAGS = $(shell sdl2-config --libs) -lm -lpthread

.PHONY: all clean run

all: $(TARGET)

# Create build directory if it doesn't exist
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Update rule to depend on build directory and output to build/
$(TARGET): $(LVGL_HEADER) $(ALL_SRCS) | $(BUILD_DIR)
	@echo "[CC] Linking $(TARGET) (Linux/SDL2)..."
	$(CC) $(CFLAGS) $(ALL_SRCS) -o $@ $(LDFLAGS)
	@echo "[OK] ./$(TARGET)"

run: all
	./$(TARGET)

clean:
	rm -rf $(BUILD_DIR)

# =============================================================================
# RISC-V target (pulp-runtime)
# =============================================================================
else ifeq ($(PLATFORM), riscv)

PULP_APP        = gui_app
PULP_APP_C_SRCS = $(ALL_SRCS)
PULP_CFLAGS     = -O2 -g \
                  $(COMMON_CFLAGS) \
		  -DPLATFORM_RISCV

# Verify LVGL is present before handing off to pulp.mk
ifeq ($(wildcard $(LVGL_HEADER)),)
$(error LVGL not found at $(LVGL_DIR). Run: ./setup.sh)
endif

include $(PULPRT_HOME)/rules/pulp.mk

else
$(error Unknown PLATFORM="$(PLATFORM)". Use PLATFORM=linux or PLATFORM=riscv)
endif
