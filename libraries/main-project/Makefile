PULP_APP        = test

PULP_APP_C_SRCS = \
    src/main.c \
    src/button.c \
    src/battery.c \
    src/dedicated_layer.c \
    src/gui_widgets.c \
    src/notification_state.c \
    src/i2c_shared.c \
    src/layer_manager.c \
    src/layers_table.c \
    src/layers/layer_watchface.c \
    src/layers/layer_notification.c \
    src/layers/layer_stepcount.c \
    src/layers/layer_activity.c \
    src/layers/layer_heartrate.c \
    src/layers/layer_spo2.c \
    ../sensors/OLED/oled.c \
    ../sensors/OLED/font/font5x7.c \
    ../sensors/OLED/GUI/gui_port.c \
    ../sensors/OLED/GUI/gesture.c \
    ../sensors/OLED/GUI/ugui/ugui.c \
    ../sensors/OLED/GUI/ugui/ui/Fonts/font_6x8.c \
    ../sensors/OLED/GUI/ugui/ui/Fonts/font_8x8.c \
    ../sensors/OLED/GUI/ugui/ui/Fonts/font_12x16.c \
    ../sensors/DS1307/rtc.c \
    ../sensors/lis3dhtr/lis3dhtr.c \
    ../sensors/mpu6050/mpu6050.c \
    ../sensors/max30102/max30102.c \
    ../sensors/max30102/filter/ppg_hr.c \
    ../app/step_counter/step_counter.c \
    ../app/Classification/classifier.c \
    sensors/hr_max30102/hr_max30102.c \
    sensors/spo2_max30102/spo2_max30102.c \
    sensors/afe4400/afe4400.c \
    sensors/afe4400/ppg_afe4400.c


PULP_CFLAGS = -Os -flto -g \
            -Iinclude \
            -I../sensors/OLED \
            -I../sensors/OLED/GUI \
            -I../sensors/OLED/GUI/ugui \
            -I../sensors/OLED/GUI/ugui/ui \
            -I../sensors/DS1307 \
            -I../sensors/lis3dhtr \
            -I../sensors/mpu6050 \
            -I../sensors/max30102 \
            -I../sensors/max30102/filter \
            -I../app/step_counter \
            -I../app/Classification \
            -Isensors/hr_max30102 \
            -Isensors/spo2_max30102 \
            -Isensors/afe4400

PULP_ARCH_CFLAGS ?=  -march=rv32imcxpulpv2
PULP_ARCH_C_LDFLAGS ?=  -march=rv32imcxpulpv2
PULP_ARCH_OBJDFLAGS ?= -Mmarch=rv32imcxpulpv2

include $(PULPRT_HOME)/rules/pulp.mk