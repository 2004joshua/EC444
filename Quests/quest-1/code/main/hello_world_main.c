/**
 * Authors: Joshua Arrevillaga, Michael Barany, Sam Kraft, and Alicja Mahr
 * Date: 2024-09-20
 */

// This is based off of the hello-world example provided by ESP-IDF
// Besides the includes, everything else is original code

/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include <esp_task_wdt.h>
#include "pins.h"
#include <freertos/FreeRTOS.h>
#include "driver/adc.h"
#include "esp_adc_cal.h"

// ADC and Interrupt Constants
#define STACK_DEPTH 2048
#define ESP_INTR_FLAG_DEFAULT 0

// LED Pins
#define GREEN_LED A5
#define YELLOW_LED SCK
#define RED_LED MOSI

// Components Pins
#define PHOTOCELL A9
#define THERMO A4
#define BATTERY A3
#define RESET_BUTTON A6
#define DIRECTION_BUTTON A8

// Thresholds and Conversion Constants
#define DARK_THRESHOLD 100 // Lux
#define LUX_CONVERSION 227.87 // V to lux
#define TEMP_CONVERSION 0.09 // V to C

// Setup LED states
typedef enum state
{
    READY,
    ACTIVE,
    DONE
} state_t;
state_t state = READY;
gpio_config_t io_conf;

// Setup global variables
float light_voltage = 0;
float thermal_temp = 0;
float battery_voltage = 0;
int yellow_led_on = 0;
int seconds = 0;
int direction = 0; // 0 = horizontal, 1 = vertical

/**
 * @brief Interrupt handler for button press
 */
void handle_button_interrupt()
{
    // Interrupt handler for button press
    state = READY;
}

/**
 * @brief Initialize pins, ADC, and interrupts
 */
void init_pins()
{
    gpio_set_direction(GREEN_LED, GPIO_MODE_OUTPUT);
    gpio_set_direction(YELLOW_LED, GPIO_MODE_OUTPUT);
    gpio_set_direction(RED_LED, GPIO_MODE_OUTPUT);

    gpio_set_direction(PHOTOCELL, GPIO_MODE_INPUT);
    gpio_set_direction(THERMO, GPIO_MODE_INPUT);
    gpio_set_direction(BATTERY, GPIO_MODE_INPUT);
    gpio_set_direction(RESET_BUTTON, GPIO_MODE_INPUT);
    gpio_set_direction(DIRECTION_BUTTON, GPIO_MODE_INPUT);

    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_5, ADC_ATTEN_DB_0);

    io_conf.intr_type = GPIO_INTR_POSEDGE; // No interrupts for LEDs
    io_conf.mode = GPIO_MODE_INPUT;       // Set as output
    io_conf.pin_bit_mask = (1ULL << RESET_BUTTON);;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);

    // Install ISR service and attach the button ISR handler
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    gpio_isr_handler_add(RESET_BUTTON, handle_button_interrupt, NULL);
}

/**
 * @brief Handle direction button press
 */
void handle_direction_button(){
    while(1) {
        if (gpio_get_level(DIRECTION_BUTTON) == 1) {
            direction = !direction;
        }

        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

/**
 * @brief Read battery voltage
 */
void read_battery()
{
    while (1)
    {
        battery_voltage = adc1_get_raw(ADC1_CHANNEL_4) * 3300 / 4095.0 / 1000.0;
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

/**
 * @brief Read temperature
 */
void read_temp()
{
    while (1)
    {
        thermal_temp = adc1_get_raw(ADC1_CHANNEL_0) * TEMP_CONVERSION;
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

/**
 * @brief Read light
 */
void read_light()
{
    while (1)
    {
        light_voltage = adc1_get_raw(ADC1_CHANNEL_5) * 3300 / 4095.0 / 1000.0 * LUX_CONVERSION;

        if (state == READY && light_voltage < DARK_THRESHOLD)
        {
            state = ACTIVE;
        }
        else if (state == ACTIVE && light_voltage > DARK_THRESHOLD)
        {
            state = DONE;
        }

        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

/**
 * @brief Set LEDs based on state
 *
 * @return void
 */
void set_leds()
{
    while (1)
    {
        switch (state)
        {
        case READY:
            gpio_set_level(GREEN_LED, 1);
            gpio_set_level(YELLOW_LED, 0);
            gpio_set_level(RED_LED, 0);
            break;
        case ACTIVE:
            gpio_set_level(GREEN_LED, 0);
            gpio_set_level(YELLOW_LED, yellow_led_on);
            gpio_set_level(RED_LED, 0);

            // Hold for 2 seconds (1900 + 100) before changing LED state
            yellow_led_on = !yellow_led_on;
            vTaskDelay(1900 / portTICK_PERIOD_MS);

            break;
        case DONE:
            gpio_set_level(GREEN_LED, 0);
            gpio_set_level(YELLOW_LED, 0);
            gpio_set_level(RED_LED, 1);
            break;
        default:
            break;
        }

        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

/**
 * @brief Print state
 */
void print_state()
{
    while (1)
    {
        printf("Time: %d s, Temp: %.2f C, Light: %.4f Lux, Battery: %.1f V, Direction: %s\n", seconds, thermal_temp, light_voltage, battery_voltage, direction ? "Vertical" : "Horizontal");

        seconds += 2;
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
}

/**
 * @brief Main function
 */
void app_main(void)
{
    init_pins();

    // We have to read data first to ensure the state is correct
    xTaskCreate(read_light, "read_light", STACK_DEPTH, NULL, 1, NULL);
    xTaskCreate(read_temp, "read_temp", STACK_DEPTH, NULL, 1, NULL);
    xTaskCreate(read_battery, "read_battery", STACK_DEPTH, NULL, 1, NULL);
    xTaskCreate(handle_direction_button, "handle_direction_button", STACK_DEPTH, NULL, 1, NULL);

    xTaskCreate(print_state, "print_state", STACK_DEPTH, NULL, 1, NULL);
    xTaskCreate(set_leds, "set_leds", STACK_DEPTH, NULL, 1, NULL);
}