// Written by Michael Barany, Josh Arreivillaga, Sam Kraft with the assistance for ChatGPT as an aid
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
#include "lwip/sockets.h"
#include "lwip/netdb.h"

#include "esp_event.h"
#include "esp_wifi.h"
#include "nvs_flash.h"



#define DEVICE_NAME "ESP_Device_4"

#define WIFI_SSID "Group_7"
#define WIFI_PASS "smartsys"
#define UDP_SERVER_IP "192.168.1.111" // Replace with your target server's IP
#define UDP_SERVER_PORT 33333         // Replace with your target server's port
#define LED_GPIO GPIO_NUM_25
static int blink_time = 500; // Default blink time in milliseconds

static const char *TAG = "WIFI_APP";
static int udp_socket;
static struct sockaddr_in server_addr;

/**
 * @brief Event handler for Wi-Fi events
 */
static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "Disconnected from Wi-Fi, attempting to reconnect...");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
    }
}

/**
 * @brief Initialize Wi-Fi in station mode
 */
static void wifi_init() {
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    esp_netif_t *netif = esp_netif_create_default_wifi_sta();
    assert(netif);
    
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));
    
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    ESP_LOGI(TAG, "Wi-Fi initialization complete. Attempting to connect to SSID: %s", WIFI_SSID);
}


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
    thermal_temp = ((adc1_get_raw(ADC1_CHANNEL_0) * (3.3/4096)) - 0.5)*100;
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
 * @brief Initialize UDP socket
 */
static void udp_init() {
    udp_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (udp_socket < 0) {
        ESP_LOGE(TAG, "Failed to create UDP socket");
        return;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(UDP_SERVER_PORT);
    inet_pton(AF_INET, UDP_SERVER_IP, &server_addr.sin_addr.s_addr);

    ESP_LOGI(TAG, "UDP socket initialized. Sending to %s:%d", UDP_SERVER_IP, UDP_SERVER_PORT);
}

/**
 * @brief Send data over UDP
 */
static void udp_send_task(void *pvParameters) {
    char buffer[512];
    int resting_time = 0, active_time = 0, highly_active_time = 0;
    const int check_interval = 1000; // 1 second

    while (1) {
        // Calculate net acceleration
        float net_accel = sqrt(xAccel * xAccel + yAccel * yAccel + zAccel * zAccel);
        char *state;

        if (net_accel > 15) {
            state = "Highly Active";
            highly_active_time += check_interval / 1000;
        } else if (net_accel > 11) {
            state = "Active";
            active_time += check_interval / 1000;
        } else {
            state = "Resting";
            resting_time += check_interval / 1000;
        }

        snprintf(buffer, sizeof(buffer),
                 "{\"device_name\": \"%s\", \"net_accel\": %.2f, \"state\": \"%s\", "
                 "\"resting_time\": %d, \"active_time\": %d, \"highly_active_time\": %d}",
                 DEVICE_NAME, net_accel, state, resting_time, active_time, highly_active_time);

        int err = sendto(udp_socket, buffer, strlen(buffer), 0,
                         (struct sockaddr *)&server_addr, sizeof(server_addr));
        if (err < 0) {
            ESP_LOGE(TAG, "Failed to send data via UDP");
        } else {
            ESP_LOGI(TAG, "Data sent via UDP: %s", buffer);
        }

        vTaskDelay(check_interval / portTICK_PERIOD_MS); // Send data every second
    }
}


// UDP Server Task
static void udp_server_task(void *pvParameters) {
    char rx_buffer[128];
    char addr_str[128];
    struct sockaddr_in dest_addr;

    dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(UDP_SERVER_PORT);

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "Socket created");

    if (bind(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) < 0) {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        close(sock);
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "Socket bound, port %d", UDP_SERVER_PORT);

    struct sockaddr_storage source_addr;
    socklen_t socklen = sizeof(source_addr);

    while (1) {
        ESP_LOGI(TAG, "Waiting for data");

        int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0, (struct sockaddr *)&source_addr, &socklen);
        if (len < 0) {
            ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
            break;
        } else {
            rx_buffer[len] = 0; // Null-terminate received data
            inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr, addr_str, sizeof(addr_str) - 1);
            ESP_LOGI(TAG, "Received %d bytes from %s:", len, addr_str);
            ESP_LOGI(TAG, "%s", rx_buffer);

            if (strcmp(rx_buffer, "GET_BLINK_TIME") == 0) {
                char response[64];
                snprintf(response, sizeof(response), "BLINK_TIME:%d", blink_time);
                sendto(sock, response, strlen(response), 0, (struct sockaddr *)&source_addr, socklen);
            } else if (strncmp(rx_buffer, "SET_BLINK_TIME:", 15) == 0) {
                int new_blink_time = atoi(rx_buffer + 15);
                blink_time = new_blink_time;
                const char *ack = "Blink time updated";
                sendto(sock, ack, strlen(ack), 0, (struct sockaddr *)&source_addr, socklen);
                ESP_LOGI(TAG, "Updated blink time to %d ms", blink_time);
            } else {
                const char *error = "Unrecognized command";
                sendto(sock, error, strlen(error), 0, (struct sockaddr *)&source_addr, socklen);
            }
        }
    }

    if (sock != -1) {
        shutdown(sock, 0);
        close(sock);
    }
    vTaskDelete(NULL);
}

// Blink Task
static void blink_task(void *pvParameters) {
    gpio_reset_pin(LED_GPIO); // Reset GPIO pin to default state
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT); // Set GPIO as output

    while (1) {
        gpio_set_level(LED_GPIO, 1); // Turn the LED on
        vTaskDelay(blink_time / portTICK_PERIOD_MS); // Delay for ON time
        gpio_set_level(LED_GPIO, 0); // Turn the LED off
        vTaskDelay(blink_time / portTICK_PERIOD_MS); // Delay for OFF time
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

  // Initialize Wi-Fi
  wifi_init();

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

   // Initialize UDP
    udp_init();

  xTaskCreate(handle_button, "handle_button", STACK_DEPTH, NULL, 1, NULL);

  // Create UDP sending task
    xTaskCreate(udp_send_task, "udp_send_task", 4096, NULL, 1, NULL);

    xTaskCreate(udp_server_task, "udp_server", 4096, NULL, 5, NULL);
    xTaskCreate(blink_task, "blink_task", 2048, NULL, 5, NULL);

  //xTaskCreate(send_data, "send_data", STACK_DEPTH, NULL, 1, NULL);
}
