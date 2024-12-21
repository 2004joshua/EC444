#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/ledc.h"

// Motor control pins
#define EN1 32  // Left Motor PWM
#define EN2 14  // Right Motor PWM
#define IN1 15  // Left Motor Direction 1
#define IN2 33  // Left Motor Direction 2
#define IN3 27  // Right Motor Direction 1
#define IN4 4   // Right Motor Direction 2

// LEDC Configuration
#define LEDC_TIMER         LEDC_TIMER_0
#define LEDC_MODE          LEDC_HIGH_SPEED_MODE
#define LEDC_FREQ_HZ       50
#define LEDC_DUTY_RES      LEDC_TIMER_10_BIT
#define LEDC_MAX_DUTY      1023

void motor_init()
{
    // Configure GPIO for direction control
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << IN1) | (1ULL << IN2) | (1ULL << IN3) | (1ULL << IN4),
        .mode = GPIO_MODE_OUTPUT,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    // Set all direction pins to LOW initially
    gpio_set_level(IN1, 0);
    gpio_set_level(IN2, 0);
    gpio_set_level(IN3, 0);
    gpio_set_level(IN4, 0);

    // Configure LEDC for PWM
    ledc_timer_config_t ledc_timer = {
        .duty_resolution = LEDC_DUTY_RES,
        .freq_hz = LEDC_FREQ_HZ,
        .speed_mode = LEDC_MODE,
        .timer_num = LEDC_TIMER,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&ledc_timer);

    // Configure LEDC channels for each motor
    ledc_channel_config_t ledc_channel_left = {
        .gpio_num = EN1,
        .speed_mode = LEDC_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER,
        .duty = 0,  // Initially off
        .hpoint = 0,
    };
    ledc_channel_config(&ledc_channel_left);

    ledc_channel_config_t ledc_channel_right = {
        .gpio_num = EN2,
        .speed_mode = LEDC_MODE,
        .channel = LEDC_CHANNEL_1,
        .timer_sel = LEDC_TIMER,
        .duty = 0,  // Initially off
        .hpoint = 0,
    };
    ledc_channel_config(&ledc_channel_right);
}

void set_motor_state(int left_dir1, int left_dir2, int right_dir1, int right_dir2, int left_speed, int right_speed)
{
    // Set motor direction
    gpio_set_level(IN1, left_dir1);
    gpio_set_level(IN2, left_dir2);
    gpio_set_level(IN3, right_dir1);
    gpio_set_level(IN4, right_dir2);

    // Set motor speed (PWM duty cycle)
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_0, left_speed);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_0);
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_1, right_speed);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_1);
}

void test_motor_states()
{
    while (1)
    {
        printf("Forward\n");
        set_motor_state(1, 0, 1, 0, LEDC_MAX_DUTY, LEDC_MAX_DUTY);
        vTaskDelay(pdMS_TO_TICKS(3000));

        printf("Stop\n");
        set_motor_state(0, 0, 0, 0, 0, 0);
        vTaskDelay(pdMS_TO_TICKS(1000));

        printf("Backward\n");
        set_motor_state(0, 1, 0, 1, LEDC_MAX_DUTY, LEDC_MAX_DUTY);
        vTaskDelay(pdMS_TO_TICKS(3000));

        printf("Turn Left\n");
        set_motor_state(0, 0, 1, 0, 0, LEDC_MAX_DUTY);
        vTaskDelay(pdMS_TO_TICKS(3000));

        printf("Turn Right\n");
        set_motor_state(1, 0, 0, 0, LEDC_MAX_DUTY, 0);
        vTaskDelay(pdMS_TO_TICKS(3000));

        printf("Spin Left\n");
        set_motor_state(0, 1, 1, 0, LEDC_MAX_DUTY, LEDC_MAX_DUTY);
        vTaskDelay(pdMS_TO_TICKS(3000));

        printf("Spin Right\n");
        set_motor_state(1, 0, 0, 1, LEDC_MAX_DUTY, LEDC_MAX_DUTY);
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

void app_main()
{
    printf("Initializing motors...\n");
    motor_init();

    printf("Starting test sequence...\n");
    test_motor_states();
}