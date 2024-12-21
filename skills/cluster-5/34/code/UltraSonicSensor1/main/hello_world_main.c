#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

#define ADC_CHANNEL ADC_CHANNEL_0  // GPIO36 (A4)
#define ADC_ATTEN ADC_ATTEN_DB_12  // Updated to the new attenuation
#define ADC_BITWIDTH ADC_BITWIDTH_12  // 12-bit resolution
#define DEFAULT_VREF 1100          // Default reference voltage in mV

static const char *TAG = "Ultrasonic_ADC";

void app_main() {
    adc_oneshot_unit_handle_t adc1_handle;
    adc_cali_handle_t cali_handle;
    bool calibration_enabled = false;

    // Initialize ADC unit
    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &adc1_handle));

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL, &chan_cfg));

    // Initialize ADC calibration
    if (adc_cali_create_scheme_line_fitting(&(adc_cali_line_fitting_config_t){
                                                 .atten = ADC_ATTEN,
                                                 .bitwidth = ADC_BITWIDTH,
                                             },
                                             &cali_handle) == ESP_OK) {
        calibration_enabled = true;
        ESP_LOGI(TAG, "Calibration enabled.");
    } else {
        ESP_LOGW(TAG, "Calibration not available, proceeding without calibration.");
    }

    while (1) {
        int raw_adc;
        int voltage;

        // Read ADC raw value
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, ADC_CHANNEL, &raw_adc));

        // Convert raw value to voltage
        if (calibration_enabled) {
            ESP_ERROR_CHECK(adc_cali_raw_to_voltage(cali_handle, raw_adc, &voltage));
        } else {
            voltage = (raw_adc * 3300) / 4095;  // Approximation if calibration is not available
        }

        // Convert voltage to distance
        float distance_mm = ((float)voltage / 1000.0) * (1024.0 / 5.0);

        // Log results
        ESP_LOGI(TAG, "ADC Value: %d, Voltage: %d mV, Distance: %.2f mm", raw_adc, voltage, distance_mm);

        // Delay for 2 seconds
        vTaskDelay(pdMS_TO_TICKS(2000));
    }

    // Cleanup
    if (calibration_enabled) {
        adc_cali_delete_scheme_line_fitting(cali_handle);
    }
    adc_oneshot_del_unit(adc1_handle);
}
