#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

// ADC Configuration
#define ADC_CHANNEL ADC_CHANNEL_0  // GPIO36 for ADC1 channel 0
#define ADC_UNIT ADC_UNIT_1        // Use ADC1
#define ADC_ATTEN ADC_ATTEN_DB_12  // Corrected to the new attenuation
#define DEFAULT_VREF 1100          // Default reference voltage in mV

static const char *TAG = "IR_Range_Sensor";

void app_main() {
    // Initialize ADC
    adc_oneshot_unit_handle_t adc1_handle;
    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = ADC_UNIT,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &adc1_handle));

    adc_oneshot_chan_cfg_t channel_cfg = {
        .atten = ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL, &channel_cfg));

    // Initialize ADC calibration
    adc_cali_handle_t cali_handle = NULL;
    bool do_calibration = adc_cali_create_scheme_line_fitting(&(adc_cali_line_fitting_config_t){
                                                                  .atten = ADC_ATTEN,
                                                                  .bitwidth = ADC_BITWIDTH_12,
                                                              },
                                                              &cali_handle) == ESP_OK;

    if (!do_calibration) {
        ESP_LOGW(TAG, "ADC calibration could not be created. Proceeding without calibration.");
    }

    while (1) {
        int raw_adc;
        int voltage;

        // Read ADC value
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, ADC_CHANNEL, &raw_adc));

        // Convert raw ADC to voltage
        if (do_calibration) {
            ESP_ERROR_CHECK(adc_cali_raw_to_voltage(cali_handle, raw_adc, &voltage));
        } else {
            // Estimate voltage without calibration
            voltage = (raw_adc * 3300) / 4095;
        }

        // Convert voltage to distance (example formula)
        float distance = (voltage < 400) ? -1 : 27.86 / (voltage / 1000.0) - 0.42;

        // Display results
        if (distance > 0) {
            ESP_LOGI(TAG, "ADC Value: %d, Voltage: %d mV, Distance: %.2f cm", raw_adc, voltage, distance);
        } else {
            ESP_LOGI(TAG, "ADC Value: %d, Voltage: %d mV, Distance: Out of range", raw_adc, voltage);
        }

        // Wait for 2 seconds
        vTaskDelay(pdMS_TO_TICKS(2000));
    }

    // Cleanup
    if (do_calibration) {
        adc_cali_delete_scheme_line_fitting(cali_handle);
    }
    adc_oneshot_del_unit(adc1_handle);
}
