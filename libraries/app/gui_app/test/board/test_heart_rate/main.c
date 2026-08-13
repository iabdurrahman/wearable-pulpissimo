/**
 * @file    main.c
 * @brief   Board Test: Heart Rate on OLED with MAX30102 (PULPissimo RISC-V)
 *
 * Menggabungkan Sensor Detak Jantung dan UI uGUI.
 * Tidak menggunakan printf / libc sama sekali demi menghemat RAM!
 */

#include "gui_port.h"
#include "gui_manager.h"
#include "screens/heart_rate.h"
#include "oled.h"
#include "max30102.h"
#include "ppg_hr.h"
#include <stdbool.h>
#include <pmsis.h>

static max30102_t sensor;
static ppg_hr_t hr_filter;
volatile uint32_t g_tick_ms = 0;

static max30102_config_t build_sparkfun_example_config(void) {
  max30102_config_t config = max30102_get_default_config();
  config.sample_rate = MAX30102_SR_100;
  config.adc_range = MAX30102_ADC_RANGE_4096;
  config.pulse_width = MAX30102_PULSE_WIDTH_18BIT;
  config.fifo_avg = MAX30102_FIFO_AVG_4;
  config.ir_led_current = 0x1F;
  config.red_led_current = 0x0A;
  return config;
}

int main(void) {
  // 1. Initialize OLED First so we can show errors!
  i2c_dev_t oled_conf;
  i2c_dev_init(&oled_conf);
  oled_conf.id = 0;
  oled_conf.cs = 0x3C << 1;
  oled_conf.max_baudrate = 400000;
  i2c_open(&oled_conf);

  OLED_Init();
  OLED_Clear();
  gui_manager_init();
  gui_manager_set_screen(SCREEN_HEART_RATE); 
  
  // Render initial loading state
  gui_manager_render();
  OLED_Update();

  // 2. Initialize MAX30102
  max30102_status_t init_status = max30102_init(&sensor, 0);
  if (init_status != MAX30102_OK) {
    // Show error on OLED!
    i2c_open(&oled_conf);
    OLED_Clear();
    UG_FontSelect(FONT_8X8);
    UG_PutString(10, 20, "Sensor Error!");
    UG_PutString(10, 40, "Check Wiring!");
    OLED_Update();
    while (1) {
      pos_delay_busy_ms(1000);
    } 
  }

  max30102_config_t config = build_sparkfun_example_config();
  max30102_configure(&sensor, &config);
  ppg_hr_init(&hr_filter);

  int print_counter = 0;
  ppg_hr_result_t hr_result = {0}; 
  
  uint32_t last_ui_update = pos_tick_get_counter_ms();
  bool finger_present = true;
  uint32_t no_finger_start_time = 0;

  while (1) {
    if (sensor.i2c_dev == NULL) {
        i2c_dev_t max_conf;
        i2c_dev_init(&max_conf);
        max_conf.id = 0;
        max_conf.cs = MAX30102_I2C_ADDR << 1;
        max_conf.max_baudrate = 100000;
        sensor.i2c_dev = i2c_open(&max_conf);
    }

    while (1) {
        max30102_sample_t sample = {0};
        max30102_status_t status = max30102_read_sample(&sensor, &sample);
        
        if (status == MAX30102_OK) {
            if (sample.ir < 50000) {
                if (finger_present) {
                    finger_present = false;
                    no_finger_start_time = pos_tick_get_counter_ms();
                } else if (pos_tick_get_counter_ms() - no_finger_start_time >= 3000) {
                    ppg_hr_init(&hr_filter);
                    hr_result.avg_bpm = 0;
                    hr_result.bpm = 0;
                    no_finger_start_time = pos_tick_get_counter_ms();
                }
            } else {
                finger_present = true;
            }

            hr_result = ppg_hr_process(&hr_filter, (int32_t)sample.ir, pos_tick_get_counter_ms());
        } else {
            if (sensor.i2c_dev != NULL) {
                i2c_close((i2c_t *)sensor.i2c_dev);
                sensor.i2c_dev = NULL;
            }
            break; 
        }
    }

    uint32_t now = pos_tick_get_counter_ms();
    if (now - last_ui_update >= 1000) {
      last_ui_update = now;
      print_counter++;

      // Update BPM text every 5 seconds (just like Demo Medical)
      if (print_counter % 3 == 0) {
        if (hr_result.avg_bpm > 0) {
            heart_rate_set_bpm((uint8_t)hr_result.avg_bpm);
        } else {
            heart_rate_set_bpm(0); // 0 means '---' or measuring
        }
      }

      // SWITCH I2C TO OLED (400 kHz, Addr 0x3C)
      i2c_dev_t oled_conf;
      i2c_dev_init(&oled_conf);
      oled_conf.id = 0;
      oled_conf.cs = 0x3C << 1;
      oled_conf.max_baudrate = 400000;
      i2c_t *oled_i2c = i2c_open(&oled_conf);

      OLED_Clear();
      gui_manager_render();
      OLED_Update();

      if (oled_i2c != NULL) {
          i2c_close(oled_i2c);
      }
    }

    pos_delay_busy_ms(5); 
  }

  return 0;
}

void pe_start(void) {}
uint32_t micros(void) { return 0; }
