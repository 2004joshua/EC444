/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include "pins.h"
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"

#define pin A4

void init() {
    gpio_set_direction(pin, GPIO_MODE_INPUT);
}

void read_adc() {
    while (true) {

        adc1_config_channel_atten(ADC1_CHANNEL_4, ADC_ATTEN_DB_0);
        adc1_config_width(ADC_WIDTH_BIT_12);

        double raw = 0.001 * adc1_get_raw(ADC1_CHANNEL_4);
        double voltage = (raw * 3300 / 4095); 
        printf("Raw Voltage: %f | Voltage: %f \n\n",raw, voltage);

        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void app_main(void) {
   init();
   read_adc(); 
}
