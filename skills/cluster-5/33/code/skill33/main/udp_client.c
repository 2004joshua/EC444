/* BSD Socket API Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

#ifdef CONFIG_EXAMPLE_SOCKET_IP_INPUT_STDIN
#include "addr_from_stdin.h"
#endif

#if defined(CONFIG_EXAMPLE_IPV4)
#define HOST_IP_ADDR CONFIG_EXAMPLE_IPV4_ADDR
#elif defined(CONFIG_EXAMPLE_IPV6)
#define HOST_IP_ADDR CONFIG_EXAMPLE_IPV6_ADDR
#else
#define HOST_IP_ADDR ""
#endif

#define PORT CONFIG_EXAMPLE_PORT

static const char *TAG = "example";
static const char *payload = "Message from ESP32 ";


typedef struct {
    int id;
    float x;
    float z;
    float distance_from_target;
    float current_theta;
    float error_angle; 
    float target_theta; 
    char status[10];
    int64_t timestamp;
    int position_idx; 
    float target_x; 
    float target_z; 
} RobotData;

static volatile RobotData car;

coordinates coords[] = {
    {.x = -728.0, .z =  730.0} 
}

void motor_init() {
    gpio_config_t io_conf = {
        .pin_bit_mask = ((1ULL << IN1) | (1ULL << IN2) | (1ULL << IN3) | (1ULL << IN4)),
        .mode = GPIO_MODE_OUTPUT,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    ledc_timer_config_t pwm_timer = {
        .duty_resolution = PWM_DUTY_RES,
        .freq_hz = PWM_FREQ_HZ,
        .speed_mode = PWM_MODE,
        .timer_num = PWM_TIMER,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&pwm_timer);

    ledc_channel_config_t pwm_channel_left = {
        .gpio_num = EN_LEFT,
        .speed_mode = PWM_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = PWM_TIMER,
        .duty = 0,
        .hpoint = 0,
    };
    ledc_channel_config(&pwm_channel_left);

    ledc_channel_config_t pwm_channel_right = {
        .gpio_num = EN_RIGHT,
        .speed_mode = PWM_MODE,
        .channel = LEDC_CHANNEL_1,
        .timer_sel = PWM_TIMER,
        .duty = 0,
        .hpoint = 0,
    };
    ledc_channel_config(&pwm_channel_right);
}

void udp_client_task(void *pvParameters) {
    char rx_buffer[128];
    struct sockaddr_in dest_addr = {
        .sin_addr.s_addr = inet_addr(HOST_IP_ADDR),
        .sin_family = AF_INET,
        .sin_port = htons(PORT),
    };

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        vTaskDelete(NULL);
        return;
    }

    while (1) {
        sendto(sock, payload, strlen(payload), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));

        struct sockaddr_storage source_addr;
        socklen_t socklen = sizeof(source_addr);
        int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0, (struct sockaddr *)&source_addr, &socklen);
        //printf("LEN: %i\n", len);
        if (len > 0) {
            rx_buffer[len] = '\0';
            if (parseRobotData(rx_buffer)) {
                ESP_LOGI(TAG, "Updated car: X=%.2f, Z=%.2f, Theta=%.2f",
                         car.x, car.z, car.current_theta);

                compute_angle_error(); 
                compute_distance_to_target();

                xEventGroupSetBits(data_event_group, DATA_UPDATED);
            }
            
        }
         else if (len < 0)
        {
            ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
            memset(rx_buffer, 0, sizeof(rx_buffer)); // Clear the buffer
            ESP_LOGI(TAG, "Buffer cleared.");
        }
        
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }

    shutdown(sock, 0);
    close(sock);
}

void drive_straight() {
    float base_speed = PWM_MAX_DUTY * 0.8f;
    float left_speed, right_speed;

    ESP_LOGI(TAG, "Driving straight");

    // Continuously drive while periodically checking the state
    while (1) {
        // Recompute position and errors
        compute_angle_error();
        compute_distance_to_target();

        // Adjust speeds based on small angular error
        if (fabsf(car.error_angle) <= 5.0f) {
            left_speed = fmaxf(MIN_SPEED_THRESHOLD, base_speed * LEFT_CALIBRATION);
            right_speed = fmaxf(MIN_SPEED_THRESHOLD, base_speed * RIGHT_CALIBRATION);
        } else if (car.error_angle > 0 && car.error_angle <= 180.0f) {
            // Small drift to the left; slow down left wheel
            left_speed = fmaxf(MIN_SPEED_THRESHOLD, base_speed * 0.9f * LEFT_CALIBRATION);
            right_speed = fmaxf(MIN_SPEED_THRESHOLD, base_speed * RIGHT_CALIBRATION);
        } else {
            // Small drift to the right; slow down right wheel
            left_speed = fmaxf(MIN_SPEED_THRESHOLD, base_speed * LEFT_CALIBRATION);
            right_speed = fmaxf(MIN_SPEED_THRESHOLD, base_speed * 0.9f * RIGHT_CALIBRATION);
        }

        // Log driving state
        ESP_LOGI(TAG, "Driving Straight: Left Speed = %.2f, Right Speed = %.2f, Error Angle = %.2f",
                 left_speed, right_speed, car.error_angle);

        // Set motor speeds to adjust course
        set_motor_state(1, 0, 1, 0, (uint32_t)left_speed, (uint32_t)right_speed);

        // Short delay for movement and updates
        vTaskDelay(pdMS_TO_TICKS(100)); // 100ms delay

        // Transition conditions
        if ((fabsf(car.error_angle) > 5.0f) ||
            car.distance_from_target < TARGET_THRESHOLD) {
            ESP_LOGI(TAG, "Condition changed: Angular Error: %.2f, Distance: %.2f",
                     car.error_angle, car.distance_from_target);

            // Stop the motors and exit the driving loop
            set_motor_state(0, 0, 0, 0, 0, 0);
            break;
        }
    }
}

// Calculates error angle and the target based off position
void compute_angle_error() {
    // Compute differences in X and Z positions
    float dx = car.target_x - car.x;
    float dz = car.target_z - car.z; 

    // Compute target angle in degrees relative to car's coordinate system
    car.target_theta = atan2f(-dx, -dz) * (180.0f / (float)M_PI);

    // Normalize theta_target to [0, 360)
    if (car.target_theta < 0) {
        car.target_theta += 360.0f;
    }

    car.error_angle = car.target_theta - car.current_theta;

    // Normalize error angle to [-180, 180]
    if (car.error_angle > 180.0f) {
        car.error_angle -= 360.0f;
    } else if (car.error_angle < -180.0f) {
        car.error_angle += 360.0f;
    }
    // Debug logs (useful during testing; consider removing for production)
    ESP_LOGI(TAG, "Target Position: X=%.2f, Z=%.2f", car.target_x, car.target_z);
    ESP_LOGI(TAG, "Current Position: X=%.2f, Z=%.2f, Theta=%.2f", car.x, car.z, car.current_theta);
    ESP_LOGI(TAG, "dx: %.2f, dz: %.2f, Theta Target: %.2f, Angular Error: %.2f",
             dx, dz, car.target_theta, car.error_angle);
}

// Compute distance to target
void compute_distance_to_target() {
    float dx = car.target_x - car.x;
    float dz = car.target_z - car.z;
    car.distance_from_target = sqrtf(dx * dx + dz * dz);
}

void set_motor_state(int left_dir1, int left_dir2, int right_dir1, int right_dir2, uint32_t left_speed, uint32_t right_speed) {
    gpio_set_level(IN1, left_dir1);
    gpio_set_level(IN2, left_dir2);
    gpio_set_level(IN3, right_dir1);
    gpio_set_level(IN4, right_dir2);

    uint32_t calibrated_left = (uint32_t)(left_speed * LEFT_CALIBRATION);
    uint32_t calibrated_right = (uint32_t)(right_speed * RIGHT_CALIBRATION);


     ledc_set_duty(PWM_MODE, LEDC_CHANNEL_0, calibrated_left);
    ledc_update_duty(PWM_MODE, LEDC_CHANNEL_0);
    ledc_set_duty(PWM_MODE, LEDC_CHANNEL_1, calibrated_right);
    ledc_update_duty(PWM_MODE, LEDC_CHANNEL_1);
}

void task1(void *pvParameters) {

    while (1) {
            ESP_LOGI(TAG, "Read car: X=%.2f, Z=%.2f, Theta=%.2f",
                car.x, car.z, car.current_theta);

            // Determine the state
            RobotState state;
            bool keep_going = (fabsf(car.error_angle) > 5.0f && fabsf(car.error_angle) < 350.0f); 

            if( (sharp_ir_distance > -1.0f && sharp_ir_distance < 20.0f)) {
                ESP_LOGI(TAG, "Obstacle in the way: %.2f", sharp_ir_distance); 
                state = STATE_STOP; 
            } else if (car.distance_from_target < TARGET_THRESHOLD) {
                ESP_LOGI(TAG, "Arrived...");
                state = STATE_STOP;
            } else if (fabsf(car.error_angle) > 5.0f && keep_going) {
                state = STATE_TURN;
            } else {
                state = STATE_DRIVE_STRAIGHT;
            }

            // Execute the state
            switch (state) {
                case STATE_STOP:
                    ESP_LOGI(TAG, "Target reached. Stopping...");
                    set_motor_state(0, 0, 0, 0, 0, 0); // Stop motor
                
                    break;

                case STATE_TURN:
                    ESP_LOGI(TAG, "Turning to correct angle...");
                    perform_timed_turn(50.0f); // Turn in 75ms increments
                    break;

                case STATE_DRIVE_STRAIGHT:
                    ESP_LOGI(TAG, "Driving straight...");
                    drive_straight(); // Move forward
                    break;
            }

            // Delay to control loop frequency
            vTaskDelay(100 / portTICK_PERIOD_MS);
        } else {
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }
}

void ir_sensor_task(void *pvParameters) {
    adc_oneshot_unit_handle_t adc1_handle;
    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = ADC_UNIT,  // Use ADC_UNIT_1 if Wi-Fi is active
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &adc1_handle));

    adc_oneshot_chan_cfg_t channel_cfg = {
        .atten = ADC_ATTEN_DB_11,    // Set attenuation for 0-3.3V range
        .bitwidth = ADC_BITWIDTH_12, // 12-bit resolution
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL, &channel_cfg));

    while (1) {
        int raw_adc;
        int voltage;

        // Read ADC value
        if (adc_oneshot_read(adc1_handle, ADC_CHANNEL, &raw_adc) != ESP_OK) {
            ESP_LOGW(TAG, "Failed to read ADC");
            continue;
        }

        // Convert raw ADC to voltage (3.3V range)
        voltage = (raw_adc * 3300) / 4095; // Voltage in millivolts

        // Convert voltage to distance using the sensor's datasheet formula
        float distance = -1;
        if (voltage < 300) {
            ESP_LOGW(TAG, "Voltage too low: %d mV. Object may be out of range.", voltage);
        } else {
            distance = 27.86 / (voltage / 1000.0) - 0.42; // Adjust this based on your specific sensor version
        }

        // Update global variable
        sharp_ir_distance = distance;

        // Log the data
        ESP_LOGI(TAG, "IR Sensor: ADC Value: %d, Voltage: %d mV, Distance: %.2f cm", raw_adc, voltage, distance);

        // Delay between readings
        vTaskDelay(200 / portTICK_PERIOD_MS); // 200 ms delay
    }

    adc_oneshot_del_unit(adc1_handle);
    vTaskDelete(NULL);
}



void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());

    xTaskCreate(task1, "control_robot", 4096, NULL, 6, NULL);
    xTaskCreate(udp_client_task, "udp_client", 4096, NULL, 5, NULL);
    xTaskCreate(ir_sensor_task, "ir_sensor_task", 4096, NULL, 6, NULL);
}
