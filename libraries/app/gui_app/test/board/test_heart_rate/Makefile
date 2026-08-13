# ============================================================================
# Makefile for Board Test: Heart Rate on OLED with MAX30102 (PULPissimo RISC-V)
# Location: gui_app/test/board/test_heart_rate/
#
# Usage:
#   source build.sh  (or manual: source pulpissimo.sh && make clean all)
#   make run platform=fpga
# ============================================================================

PULP_APP = heart_rate_test

# Paths relative to this Makefile
SENSORS_DIR = ../../../../../sensors
OLED_DIR    = $(SENSORS_DIR)/OLED
MAX_DIR     = $(SENSORS_DIR)/max30102
APP_DIR     = ../../..

PULP_APP_C_SRCS = \
    main.c \
    $(MAX_DIR)/max30102.c \
    $(MAX_DIR)/filter/ppg_hr.c \
    $(OLED_DIR)/oled.c \
    $(APP_DIR)/gui_manager.c \
    $(APP_DIR)/screens/heart_rate.c \
    $(APP_DIR)/screens/watch_face.c \
    $(OLED_DIR)/GUI/ugui/ugui.c \
    $(OLED_DIR)/GUI/ugui/ui/Fonts/font_8x8.c \
    $(OLED_DIR)/GUI/ugui/ui/Fonts/font_12x16.c \
    $(OLED_DIR)/GUI/ugui/ui/Fonts/font_6x8.c \
    $(OLED_DIR)/GUI/ugui/ui/Fonts/font_6x10.c \
    $(OLED_DIR)/GUI/gesture.c \
    $(OLED_DIR)/GUI/gui_port.c

PULP_CFLAGS = -O3 -g \
    -I. \
    -I$(MAX_DIR) \
    -I$(MAX_DIR)/filter \
    -I$(OLED_DIR) \
    -I$(OLED_DIR)/GUI \
    -I$(OLED_DIR)/GUI/ugui \
    -I$(OLED_DIR)/GUI/ugui/ui \
    -I$(APP_DIR)

include $(PULPRT_HOME)/rules/pulp.mk
