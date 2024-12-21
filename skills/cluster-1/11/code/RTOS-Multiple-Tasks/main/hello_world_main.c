/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "sdkconfig.h"
#include "driver/gpio.h"
#include "pins.h"

#define LED_DIR SDA
#define LED_1_GPIO A5
#define LED_2_GPIO SCK
#define LED_3_GPIO MOSI
#define LED_4_GPIO MISO
#define BUTTON_GPIO A0

// Hold for 1 second (1000ms)
#define HOLD_TIME 1000

// 0 = count up, 1 = count down
int direction = 0;

// Current count
int current_count = 0;

/**
 * @brief Configure all pins to output mode
 *
 * @return void
 */
void init_pins(void)
{
    // Set all pins to output
    gpio_set_direction(LED_DIR, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_1_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_2_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_3_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_4_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_direction(BUTTON_GPIO, GPIO_MODE_INPUT);
}

/**
 * @brief Set the pins to the given values
 *
 * @param one Value for LED 1
 * @param two Value for LED 2
 * @param three Value for LED 3
 * @param four Value for LED 4
 *
 * @return void
 *
 * @note This function will hold for HOLD_TIME milliseconds
 */
void set_count(uint32_t one, uint32_t two, uint32_t three, uint32_t four)
{
    gpio_set_level(LED_1_GPIO, one);
    gpio_set_level(LED_2_GPIO, two);
    gpio_set_level(LED_3_GPIO, three);
    gpio_set_level(LED_4_GPIO, four);
    vTaskDelay(HOLD_TIME / portTICK_PERIOD_MS);
}

/**
 * @brief Check if the button is pressed and change the direction if it is
 * 
 * @return void
 */
void check_button(void)
{
    while (1)
    {
        // Check if the button is pressed
        if (gpio_get_level(BUTTON_GPIO) == 1)
        {
            // Change the direction
            direction = !direction;
            printf("Button pressed, changing direction to %s\n", direction ? "down" : "up");
        }

        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

/**
 * @brief Update the direction LED
 * 
 * @return void
 */
void update_dir_led(void)
{
    while (1)
    {
        if (direction)
        {
            gpio_set_level(LED_DIR, 1);
        }
        else
        {
            gpio_set_level(LED_DIR, 0);
        }

        vTaskDelay(250 / portTICK_PERIOD_MS);
    }
}

/**
 * @brief Count up or down based on the direction
 * 
 * @return void
 */
void count(void){
    while(1){
        if (direction)
        {
            current_count -= 1;
            current_count = current_count < 0 ? 15 : current_count;
            uint32_t LED_ONE = (current_count & 0x8) >> 3;
            uint32_t LED_TWO = (current_count & 0x4) >> 2;
            uint32_t LED_THREE = (current_count & 0x2) >> 1;
            uint32_t LED_FOUR = current_count & 0x1;

            printf("[DOWN] Currently displaying: %d (%lu%lu%lu%lu)\n", current_count, LED_ONE, LED_TWO, LED_THREE, LED_FOUR);
            set_count(LED_ONE, LED_TWO, LED_THREE, LED_FOUR);
        }
        else
        {
            current_count += 1;
            current_count = current_count > 15 ? 0 : current_count;
            uint32_t LED_ONE = (current_count & 0x8) >> 3;
            uint32_t LED_TWO = (current_count & 0x4) >> 2;
            uint32_t LED_THREE = (current_count & 0x2) >> 1;
            uint32_t LED_FOUR = current_count & 0x1;

            printf("[UP] Currently displaying: %d (%lu%lu%lu%lu)\n", current_count, LED_ONE, LED_TWO, LED_THREE, LED_FOUR);
            set_count(LED_ONE, LED_TWO, LED_THREE, LED_FOUR);
        }
    }
}

/**
 * @brief Main function
 *
 * @return void
 */
void app_main(void)
{
    init_pins();

    xTaskCreate(check_button, "check_button", 2048, NULL, 10, NULL);
    xTaskCreate(update_dir_led, "update_dir_led", 2048, NULL, 10, NULL);
    xTaskCreate(count, "count", 2048, NULL, 10, NULL);
}
