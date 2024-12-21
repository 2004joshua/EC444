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

#define THERMO A3

/**
 * @brief Configure all pins to output mode
 * 
 * @return void
 */
void init_pins(void)
{
    // Set all pins to output
    gpio_set_direction(THERMO, GPIO_MODE_INPUT);
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
        adc1_config_channel_atten(ADC1_CHANNEL_3, ADC_ATTEN_DB_0);
        float thermo = adc1_get_raw(ADC1_CHANNEL_3) * 0.02;
        printf("Thermistor: %f Celsius\n", thermo);

        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
}