#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "pins.h"  // Include your custom pins.h file

#define ESP_INTR_FLAG_DEFAULT 0

// Pin definitions from pins.h
#define LED1_PIN T4   // GPIO 13
#define LED2_PIN T5   // GPIO 12
#define LED3_PIN T7   // GPIO 27
#define LED4_PIN T8   // GPIO 33
#define BUTTON_PIN T9 // GPIO 32

// Variables to keep track of the LED state
int current_led = 0;
int led_pins[] = {LED1_PIN, LED2_PIN, LED3_PIN, LED4_PIN};

// ISR handler for button press
void IRAM_ATTR button_isr_handler(void* arg) {
    // Toggle the LEDs in a round-robin manner
    gpio_set_level(led_pins[current_led], 0);  // Turn off current LED
    current_led = (current_led + 1) % 4;       // Move to the next LED
    gpio_set_level(led_pins[current_led], 1);  // Turn on the new LED
}

void app_main(void) {
    // Configure the LED pins
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;          // No interrupts for LEDs
    io_conf.mode = GPIO_MODE_OUTPUT;                // Set as output
    io_conf.pin_bit_mask = (1ULL << LED1_PIN) | (1ULL << LED2_PIN) | (1ULL << LED3_PIN) | (1ULL << LED4_PIN);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);

    // Turn off all LEDs initially
    for (int i = 0; i < 4; i++) {
        gpio_set_level(led_pins[i], 0);
    }

    // Configure the button pin
    io_conf.intr_type = GPIO_INTR_POSEDGE;          // Interrupt on rising edge (button press)
    io_conf.mode = GPIO_MODE_INPUT;                 // Set as input
    io_conf.pin_bit_mask = (1ULL << BUTTON_PIN);    // Button GPIO 32
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;        // Enable internal pull-up
    gpio_config(&io_conf);

    // Install ISR service and attach the button ISR handler
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    gpio_isr_handler_add(BUTTON_PIN, button_isr_handler, NULL);

    // Initial LED state
    gpio_set_level(LED1_PIN, 1);  // Start with the first LED on
}