#include <string.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "driver/gpio.h"
#include "driver/ledc.h"  // Used for PWM signals
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "lwip/sockets.h"

// WiFi Configuration
#define WIFI_SSID       "TP-Link_EE49"
#define WIFI_PASSWORD   "34383940"

// UDP Configuration
#define PORT            3333
#define TAG             "UDP_SERVER"

// Motor control pins
#define EN_LEFT  32  // Left Motor PWM
#define EN_RIGHT 14  // Right Motor PWM
#define IN1      15  // Left Motor Direction 1
#define IN2      33  // Left Motor Direction 2
#define IN3      27  // Right Motor Direction 1
#define IN4      4   // Right Motor Direction 2

// PWM Configuration
#define PWM_TIMER         LEDC_TIMER_0
#define PWM_MODE          LEDC_HIGH_SPEED_MODE
#define PWM_FREQ_HZ       1000  // 1 kHz frequency for motor control
#define PWM_DUTY_RES      LEDC_TIMER_10_BIT
#define PWM_MAX_DUTY      ((1 << PWM_DUTY_RES) - 1)  // Max duty cycle based on resolution

#define TIMEOUT_MS        1000  // Timeout in milliseconds (1 second)

volatile char wasd = '0'; // Holds the latest WASD input
TimerHandle_t stop_timer; // Timer to stop the car on timeout

void set_motor_state(int left_dir1, int left_dir2, int right_dir1, int right_dir2, uint32_t left_speed, uint32_t right_speed); 

void timeout_callback(TimerHandle_t xTimer) {
    // Stop the car if the timer expires
    ESP_LOGW(TAG, "No commands received. Stopping the car.");
    wasd = '0'; // Force stop
    set_motor_state(0, 0, 0, 0, 0, 0);
}

void reset_timeout_timer() {
    // Reset the timeout timer
    if (xTimerReset(stop_timer, 0) != pdPASS) {
        ESP_LOGE(TAG, "Failed to reset timeout timer");
    }
}

void wifi_init_sta(void) {
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize TCP/IP
    ESP_ERROR_CHECK(esp_netif_init());

    // Create default event loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Create default WiFi station
    esp_netif_create_default_wifi_sta();

    // Initialize WiFi with default config
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Set WiFi configuration
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
        },
    };

    // Set WiFi mode and configuration
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    // Wait for connection
    ESP_LOGI(TAG, "Connecting to WiFi...");
    esp_wifi_connect();
}

void motor_init() {
    // Configure GPIO for motor direction control
    gpio_config_t io_conf = {
        .pin_bit_mask = ((1ULL << IN1) | (1ULL << IN2) | (1ULL << IN3) | (1ULL << IN4)),
        .mode = GPIO_MODE_OUTPUT,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    // Initialize all direction pins to LOW
    gpio_set_level(IN1, 0);
    gpio_set_level(IN2, 0);
    gpio_set_level(IN3, 0);
    gpio_set_level(IN4, 0);

    // Configure PWM timer for motor speed control
    ledc_timer_config_t pwm_timer = {
        .duty_resolution = PWM_DUTY_RES,
        .freq_hz = PWM_FREQ_HZ,
        .speed_mode = PWM_MODE,
        .timer_num = PWM_TIMER,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&pwm_timer);

    // Configure PWM channels for each motor
    ledc_channel_config_t pwm_channel_left = {
        .gpio_num = EN_LEFT,
        .speed_mode = PWM_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = PWM_TIMER,
        .duty = 0,  // Start with motors off
        .hpoint = 0,
    };
    ledc_channel_config(&pwm_channel_left);

    ledc_channel_config_t pwm_channel_right = {
        .gpio_num = EN_RIGHT,
        .speed_mode = PWM_MODE,
        .channel = LEDC_CHANNEL_1,
        .timer_sel = PWM_TIMER,
        .duty = 0,  // Start with motors off
        .hpoint = 0,
    };
    ledc_channel_config(&pwm_channel_right);
}

void set_motor_state(int left_dir1, int left_dir2, int right_dir1, int right_dir2, uint32_t left_speed, uint32_t right_speed) {
    // Set motor direction pins
    gpio_set_level(IN1, left_dir1);
    gpio_set_level(IN2, left_dir2);
    gpio_set_level(IN3, right_dir1);
    gpio_set_level(IN4, right_dir2);

    // Set motor speed (PWM duty cycle)
    ledc_set_duty(PWM_MODE, LEDC_CHANNEL_0, left_speed);
    ledc_update_duty(PWM_MODE, LEDC_CHANNEL_0);
    ledc_set_duty(PWM_MODE, LEDC_CHANNEL_1, right_speed);
    ledc_update_duty(PWM_MODE, LEDC_CHANNEL_1);
}

void udp_server_task(void *pvParameters) {
    char rx_buffer[128];
    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(PORT);

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "Socket created");

    if (bind(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) < 0) {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        close(sock);
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "Socket bound, port %d", PORT);

    while (1) {
        ESP_LOGI(TAG, "Waiting for data...");
        struct sockaddr_in source_addr;
        socklen_t socklen = sizeof(source_addr);

        int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0,
                           (struct sockaddr *)&source_addr, &socklen);

        if (len < 0) {
            ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
            break;
        } else {
            rx_buffer[len] = '\0'; // Null-terminate input
            wasd = rx_buffer[0];   // Update global WASD input
            ESP_LOGI(TAG, "Received %d bytes from %s: %s",
                     len, inet_ntoa(source_addr.sin_addr), rx_buffer);
            reset_timeout_timer(); // Reset the timeout timer
        }
    }

    if (sock != -1) {
        ESP_LOGE(TAG, "Shutting down socket and restarting...");
        shutdown(sock, 0);
        close(sock);
    }
    vTaskDelete(NULL);
}

void app_main() {
    ESP_LOGI(TAG, "Initializing motors...");
    motor_init();

    ESP_LOGI(TAG, "Setting up WiFi and UDP server...");
    wifi_init_sta();

    // Create the timeout timer
    stop_timer = xTimerCreate("StopTimer", pdMS_TO_TICKS(TIMEOUT_MS), pdFALSE, NULL, timeout_callback);
    if (stop_timer == NULL) {
        ESP_LOGE(TAG, "Failed to create timeout timer");
    } else {
        xTimerStart(stop_timer, 0); // Start the timer
    }

    xTaskCreate(udp_server_task, "udp_server", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "Starting WASD motor control...");
    while (1) {
        switch (wasd) {
            case 'w': // Forward
                ESP_LOGI(TAG, "Forward");
                set_motor_state(1, 0, 1, 0, PWM_MAX_DUTY, PWM_MAX_DUTY);
                break;

            case 'a': // Turn Left
                ESP_LOGI(TAG, "Turn Left");
                set_motor_state(0, 0, 1, 0, 0, PWM_MAX_DUTY); // Right motor moves forward, left motor stops
                break;

            case 'd': // Turn Right
                ESP_LOGI(TAG, "Turn Right");
                set_motor_state(1, 0, 0, 0, PWM_MAX_DUTY, 0);
                break;

            case 's': // Backward
                ESP_LOGI(TAG, "Backward");
                set_motor_state(0, 1, 0, 1, PWM_MAX_DUTY, PWM_MAX_DUTY);
                break;

            case '0': // Stop
            default:
                ESP_LOGI(TAG, "Stop");
                set_motor_state(0, 0, 0, 0, 0, 0);
                break;
        }
        vTaskDelay(pdMS_TO_TICKS(100)); // Small delay to prevent CPU overload
    }
}
