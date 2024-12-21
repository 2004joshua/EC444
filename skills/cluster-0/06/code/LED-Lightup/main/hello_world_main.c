/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "driver/gpio.h"
#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "pins.h"


#define LED_1 MOSI
#define LED_2 MISO
#define LED_3 SCL
#define LED_4 SDA

// 1000ms == 1s
#define DELAY 1000

void init_pins(void) {
    // Set pins to output
    gpio_set_direction(LED_1, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_2, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_3, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_4, GPIO_MODE_OUTPUT);
}

void set_pins(uint32_t LED1_in, uint32_t LED2_in, uint32_t LED3_in, uint32_t LED4_in) {
    gpio_set_level(LED_1, LED1_in);
    gpio_set_level(LED_2, LED2_in);
    gpio_set_level(LED_3, LED3_in);
    gpio_set_level(LED_4, LED4_in);
    
    vTaskDelay(DELAY / portTICK_PERIOD_MS); // Wait for 1 second
}

void app_main(void) {
    init_pins();
    while(1) {
        // LEDs turn on progressively
        set_pins(1, 0, 0, 0);  // LED 1 on
        set_pins(1, 1, 0, 0);  // LED 1 and 2 on
        set_pins(1, 1, 1, 0);  // LED 1, 2, and 3 on
        set_pins(1, 1, 1, 1);  // LED 1, 2, 3, and 4 on
        
        // LEDs turn off in reverse order (binary countdown)
        set_pins(1, 1, 1, 0);  // LED 1, 2, and 3 on
        set_pins(1, 1, 0, 0);  // LED 1 and 2 on
        set_pins(1, 0, 0, 0);  // LED 1 on
        set_pins(0, 0, 0, 0);  // All LEDs off
        
        // Repeat after a full cycle
    }
}

