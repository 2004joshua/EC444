// This is based off of the hello-world example provided by ESP-IDF
// Besides the includes, everything else is original code

/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "./ADXL343.h"
#include "char_binary.h"
#include "driver/adc.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/uart.h"
#include "esp_adc_cal.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "math.h"
#include "pins.h"
#include "sdkconfig.h"
#include <esp_task_wdt.h>
#include <freertos/FreeRTOS.h>
#include <inttypes.h>
#include <stdio.h>

// ADC and Interrupt Constants
#define STACK_DEPTH 2048

// Components Pins
#define THERMO A4
#define BUTTON A2

// Thresholds and Conversion Constants
#define TEMP_CONVERSION 0.09 // V to C

// I2C Addresses
#define DISPLAY_ADDR 0x70 // Alphanumeric display address
#define ACCEL_ADDR 0x53   // Accelerometer address

// HT16K33 Display Commands
#define OSC 0x21                     // Oscillator command
#define HT16K33_BLINK_DISPLAYON 0x01 // Display on command
#define HT16K33_BLINK_OFF 0          // Blink off command
#define HT16K33_BLINK_CMD 0x80       // Blink command
#define HT16K33_CMD_BRIGHTNESS 0xE0  // Brightness command

// I2C Definitions
#define I2C_EXAMPLE_MASTER_SCL_IO 22 // GPIO number for I2C CLK
#define I2C_EXAMPLE_MASTER_SDA_IO 23 // GPIO number for I2C DATA
#define I2C_EXAMPLE_MASTER_NUM I2C_NUM_0
#define I2C_EXAMPLE_MASTER_FREQ_HZ 50000 // Reduce frequency to 50kHz
#define WRITE_BIT I2C_MASTER_WRITE
#define READ_BIT I2C_MASTER_READ
#define ACK_CHECK_EN true
#define ACK_VAL 0x00
#define NACK_VAL 0xFF

// UART Definitions
#define UART_PORT_NUM UART_NUM_0
#define UART_TX_PIN 1
#define UART_RX_PIN 3
#define BUF_SIZE 1024

// Display Constants
#define CAT_NAME "PEACHES"

typedef enum {
  resting,
  moving,
  upside_down,
} activity_t;

activity_t current_activity = resting;
int seconds_in_activity = 0;

// Setup global variables
float thermal_temp = 0;
int seconds = 0;

float xAccel, yAccel, zAccel;
float roll, pitch;

// Text buffer for scrolling text
uint8_t *text;
int text_length = 0;

void enum_to_string(activity_t activity, char *str) {
  char *resting_str = "resting";
  char *moving_str = "moving";
  char *upside_down_str = "upside_down";

  // Convert enum to string without using strcpy
  switch (activity) {
  case resting:
    for (int i = 0; i < 7; i++) {
      str[i] = resting_str[i];
    }
    str[7] = '\0';
    break;
  case moving:
    for (int i = 0; i < 6; i++) {
      str[i] = moving_str[i];
    }
    str[6] = '\0';
    break;
  case upside_down:
    for (int i = 0; i < 11; i++) {
      str[i] = upside_down_str[i];
    }
    str[11] = '\0';
    break;
  }
}

/**
 * @brief Initialize pins, ADC, and interrupts
 */
void init_pins() {
  gpio_set_direction(THERMO, GPIO_MODE_INPUT);
  gpio_set_direction(BUTTON, GPIO_MODE_INPUT);

  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten(ADC1_CHANNEL_5, ADC_ATTEN_DB_0);
}

// Function to initialize I2C master
static void i2c_master_init() {
  // printf("\n>> I2C Config\n");

  i2c_config_t conf;
  conf.mode = I2C_MODE_MASTER;
  conf.sda_io_num = I2C_EXAMPLE_MASTER_SDA_IO;
  conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
  conf.scl_io_num = I2C_EXAMPLE_MASTER_SCL_IO;
  conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
  conf.master.clk_speed = I2C_EXAMPLE_MASTER_FREQ_HZ;
  conf.clk_flags = 0;
  i2c_param_config(I2C_EXAMPLE_MASTER_NUM, &conf);
  i2c_driver_install(I2C_EXAMPLE_MASTER_NUM, conf.mode, 0, 0, 0);
}

/**
 * @brief Write a byte to a register
 */
void writeRegister(uint8_t deviceAddr, uint8_t regAddr, uint8_t data) {
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, (deviceAddr << 1) | I2C_MASTER_WRITE,
                        ACK_CHECK_EN);
  i2c_master_write_byte(cmd, regAddr, ACK_CHECK_EN);
  i2c_master_write_byte(cmd, data, ACK_CHECK_EN);
  i2c_master_stop(cmd);
  i2c_master_cmd_begin(I2C_EXAMPLE_MASTER_NUM, cmd, 1000 / portTICK_PERIOD_MS);
  i2c_cmd_link_delete(cmd);
}

/**
 * @brief Read a byte from a register
 */
uint8_t readRegister(uint8_t deviceAddr, uint8_t regAddr) {
  uint8_t data;
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, (deviceAddr << 1) | I2C_MASTER_WRITE,
                        ACK_CHECK_EN);
  i2c_master_write_byte(cmd, regAddr, ACK_CHECK_EN);
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, (deviceAddr << 1) | I2C_MASTER_READ, ACK_CHECK_EN);
  i2c_master_read_byte(cmd, &data, I2C_MASTER_NACK);
  i2c_master_stop(cmd);
  i2c_master_cmd_begin(I2C_EXAMPLE_MASTER_NUM, cmd, 1000 / portTICK_PERIOD_MS);
  i2c_cmd_link_delete(cmd);
  return data;
}

/**
 * @brief Read a 16-bit value from a register
 */
int16_t read16(uint8_t reg) {
  uint8_t lsb =
      readRegister(ACCEL_ADDR, reg); // Implement readRegister separately
  uint8_t msb =
      readRegister(ACCEL_ADDR, reg + 1); // Implement readRegister separately
  return (int16_t)((msb << 8) | lsb);
}

void setRange(range_t range) {
  uint8_t format = readRegister(ACCEL_ADDR, ADXL343_REG_DATA_FORMAT);
  format &= ~0x0F;
  format |= range;
  format |= 0x08;
  writeRegister(ACCEL_ADDR, ADXL343_REG_DATA_FORMAT, format);
}

int testConnection(uint8_t devAddr, int32_t timeout) {
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, (devAddr << 1) | I2C_MASTER_WRITE, ACK_CHECK_EN);
  i2c_master_stop(cmd);
  int err = i2c_master_cmd_begin(I2C_EXAMPLE_MASTER_NUM, cmd,
                                 1000 / portTICK_PERIOD_MS);
  i2c_cmd_link_delete(cmd);
  return err;
}

static void i2c_scanner() {
  int32_t scanTimeout = 1000;
  // printf("\n>> I2C scanning ..."
  //        "\n");
  uint8_t count = 0;
  for (uint8_t i = 1; i < 127; i++) {
    // printf("0x%X%s",i,"\n");
    if (testConnection(i, scanTimeout) == ESP_OK) {
      // printf("- Device found at address: 0x%X%s", i, "\n");
      count++;
    }
  }
  if (count == 0) {
    // printf("- No I2C devices found!"
    //        "\n");
  }
}

void getAccel(float *xp, float *yp, float *zp) {
  *xp = read16(ADXL343_REG_DATAX0) * ADXL343_MG2G_MULTIPLIER *
        SENSORS_GRAVITY_STANDARD;
  *yp = read16(ADXL343_REG_DATAY0) * ADXL343_MG2G_MULTIPLIER *
        SENSORS_GRAVITY_STANDARD;
  *zp = read16(ADXL343_REG_DATAZ0) * ADXL343_MG2G_MULTIPLIER *
        SENSORS_GRAVITY_STANDARD;
}

void calcRP(float x, float y, float z) {
  roll = atan2(y, z) * 57.3;
  pitch = atan2(-x, sqrt(y * y + z * z)) * 57.3;
  xAccel = x;
  yAccel = y;
  zAccel = z;
}

static void test_adxl343() {
  // printf("\n>> Polling ADXL343\n");
  while (1) {
    float xVal, yVal, zVal;
    getAccel(&xVal, &yVal, &zVal);
    calcRP(xVal, yVal, zVal);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

/**
 * @brief Read temperature
 */
void read_temp() {
  while (1) {
    thermal_temp = adc1_get_raw(ADC1_CHANNEL_0) * TEMP_CONVERSION;
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

void print_state() {
  while (1) {
    printf("Thermal Temp: %.2f\n", thermal_temp);
    printf("Roll: %.2f\n", roll);
    printf("Pitch: %.2f\n", pitch);
    printf("X Accel: %.2f\n", xAccel);
    printf("Y Accel: %.2f\n", yAccel);
    printf("Z Accel: %.2f\n", zAccel);
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
}

// Function to write to the alphanumeric display
void write_to_display(uint16_t *display_buffer) {
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, (DISPLAY_ADDR << 1) | WRITE_BIT, ACK_CHECK_EN);
  i2c_master_write_byte(cmd, 0x00, ACK_CHECK_EN);
  for (int i = 0; i < 4; i++) {
    i2c_master_write_byte(cmd, display_buffer[i] & 0xFF, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, display_buffer[i] >> 8, ACK_CHECK_EN);
  }
  i2c_master_stop(cmd);
  i2c_master_cmd_begin(I2C_EXAMPLE_MASTER_NUM, cmd, 1000 / portTICK_PERIOD_MS);
  i2c_cmd_link_delete(cmd);

  vTaskDelay(100 / portTICK_PERIOD_MS);
}

// Function to initialize the alphanumeric display
void setup_alpha_display() {
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, (DISPLAY_ADDR << 1) | WRITE_BIT, ACK_CHECK_EN);
  i2c_master_write_byte(cmd, OSC, ACK_CHECK_EN); // Turn on oscillator
  i2c_master_stop(cmd);
  i2c_master_cmd_begin(I2C_EXAMPLE_MASTER_NUM, cmd, 1000 / portTICK_PERIOD_MS);
  i2c_cmd_link_delete(cmd);

  // Blink display off
  cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, (DISPLAY_ADDR << 1) | WRITE_BIT, ACK_CHECK_EN);
  i2c_master_write_byte(
      cmd, HT16K33_BLINK_CMD | HT16K33_BLINK_OFF | HT16K33_BLINK_DISPLAYON,
      ACK_CHECK_EN); // Display on, no blink
  i2c_master_stop(cmd);
  i2c_master_cmd_begin(I2C_EXAMPLE_MASTER_NUM, cmd, 1000 / portTICK_PERIOD_MS);
  i2c_cmd_link_delete(cmd);

  // Set brightness
  cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, (DISPLAY_ADDR << 1) | WRITE_BIT, ACK_CHECK_EN);
  i2c_master_write_byte(cmd, HT16K33_CMD_BRIGHTNESS | 0xF,
                        ACK_CHECK_EN); // Max brightness
  i2c_master_stop(cmd);
  i2c_master_cmd_begin(I2C_EXAMPLE_MASTER_NUM, cmd, 1000 / portTICK_PERIOD_MS);
  i2c_cmd_link_delete(cmd);
}

// Function to initialize UART
void uart_init() {
  uart_config_t uart_config = {.baud_rate = 115200,
                               .data_bits = UART_DATA_8_BITS,
                               .parity = UART_PARITY_DISABLE,
                               .stop_bits = UART_STOP_BITS_1,
                               .flow_ctrl = UART_HW_FLOWCTRL_DISABLE};
  uart_param_config(UART_PORT_NUM, &uart_config);
  uart_set_pin(UART_PORT_NUM, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE,
               UART_PIN_NO_CHANGE);
  uart_driver_install(UART_PORT_NUM, BUF_SIZE * 2, 0, 0, NULL, 0);
}

// Function to display scrolling text on the alphanumeric display
void display_scrolling_text() {
  uint16_t display_buffer[8];
  // If we need to scroll the text
  if (text_length > 4) {
    for (int i = 0; i < text_length; i++) {
      for (int j = 0; j < 8; j++) {
        // Loop with a space at the end
        if (i + j < text_length) {
          display_buffer[j] = char_to_display(text[i + j]);
        } else {
          display_buffer[j] = char_to_display(' ');
        }
      }
      write_to_display(display_buffer);
      vTaskDelay(500 / portTICK_PERIOD_MS);
    }
  } else {
    for (int j = 0; j < 8; j++) {
      display_buffer[j] = char_to_display(text[j]);
    }
    write_to_display(display_buffer);
  }
}

void update_activity() {
  while (1) {
    if (abs(xAccel) > 5 || abs(yAccel) > 5) {
      if (current_activity == moving) {
        seconds_in_activity++;
      } else {
        current_activity = moving;
        seconds_in_activity = 0;
      }
    } else if ((abs(pitch) > 150 && abs(pitch) < 210) ||
               (abs(roll) > 150 && abs(roll) < 210)) {
      if (current_activity == upside_down) {
        seconds_in_activity++;
      } else {
        current_activity = upside_down;
        seconds_in_activity = 0;
      }
    } else {
      if (current_activity == resting) {
        seconds_in_activity++;
      } else {
        current_activity = resting;
        seconds_in_activity = 0;
      }
    }
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

void handle_button() {
  while (1) {
    if (gpio_get_level(BUTTON)) {
      // Set text to f"CAT_NAME - ACTIVITY (TIME)"
      text = (uint8_t *)malloc(BUF_SIZE);
      text_length = 0;
      for (int i = 0; i < 7; i++) {
        text[text_length++] = CAT_NAME[i];
      }

      text[text_length++] = ' ';
      text[text_length++] = '-';
      text[text_length++] = ' ';

      char activity[11];
      enum_to_string(current_activity, activity);
      for (int i = 0; i < 11; i++) {
        // Convert to uppercase
        if (activity[i] >= 97 && activity[i] <= 122) {
          activity[i] -= 32;
        }

        // If the char is _ then replace with space
        if (activity[i] == '_') {
          activity[i] = ' ';
        }

        text[text_length++] = activity[i];
      }

      text[text_length++] = ' ';
      text[text_length++] = '-';
      text[text_length++] = ' ';

      char time[3];
      sprintf(time, "%d", seconds_in_activity);
      for (int i = 0; i < 3; i++) {
        text[text_length++] = time[i];
      }

      display_scrolling_text();
    }

    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

void send_data() {
  while (1) {
    char activity[20];
    enum_to_string(current_activity, activity);

    printf(
        "{\"seconds\": %d, \"temp\": %.2f, \"roll\": %.2f, \"pitch\": %.2f, "
        "\"xAccel\": %.2f, \"yAccel\": %.2f, \"zAccel\": %.2f, "
        "\"activity\": \"%s\", \"time_in_activity\": %d, \"name\": \"%s\"}\n",
        ++seconds, thermal_temp, roll, pitch, xAccel, yAccel, zAccel, activity,
        seconds_in_activity, CAT_NAME);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

/**
 * @brief Main function
 */
void app_main(void) {
  init_pins();

  i2c_master_init();
  i2c_scanner();
  setup_alpha_display();
  uart_init();

  // Disable interrupts
  writeRegister(ACCEL_ADDR, ADXL343_REG_INT_ENABLE, 0);

  // Set range
  setRange(ADXL343_RANGE_16_G);

  // Enable measurements
  writeRegister(ACCEL_ADDR, ADXL343_REG_POWER_CTL, 0x08);

  // Create task to poll ADXL343
  xTaskCreate(test_adxl343, "test_adxl343", STACK_DEPTH, NULL, 1, NULL);
  xTaskCreate(read_temp, "read_temp", STACK_DEPTH, NULL, 1, NULL);
  xTaskCreate(update_activity, "update_activity", STACK_DEPTH, NULL, 1, NULL);

  xTaskCreate(handle_button, "handle_button", STACK_DEPTH, NULL, 1, NULL);

  xTaskCreate(send_data, "send_data", STACK_DEPTH, NULL, 1, NULL);
}