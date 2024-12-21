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
#include "driver/adc.h"
#include "esp_adc_cal.h"

#define PHOTO A9

/**
 * @brief Configure all pins to output mode
 * 
 * @return void
 */
void init_pins(void)
{
    // Set all pins to output
    gpio_set_direction(PHOTO, GPIO_MODE_INPUT);
}



/**
 * @brief Main function
 * 
 * @return void
 */
void app_main(void)
{
    init_pins();

    while (1)
    {
        // Read analog voltage from thermo sensor
        adc1_config_width(ADC_WIDTH_BIT_12);
        adc1_config_channel_atten(ADC1_CHANNEL_5, ADC_ATTEN_DB_0);
        float photo = adc1_get_raw(ADC1_CHANNEL_5) * 3300 / 4095.0 / 1000.0;
        printf("Photoresistor: %fv\n", photo);


        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
}
