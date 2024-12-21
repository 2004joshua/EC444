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
#include <math.h>
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_timer.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "lwip/sockets.h"

/*
    Quest 5: 

    Joshua Arrevillaga,
    Michael Barany,
    Samuel Kraft
*/

/*
           180
            | -----------------
            - +z               |
            |                  |
90 -|---- car ----|- 270      \ /
     -x  (front) +x         clockwise
            |
            - -z
            |
            0
*/

// UDP Configuration
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
static const char *TAG = "Robot13_Control";
static const char *payload = "ROBOTID 13";

#define WIFI_SSID       "TP-Link_EE49"
#define WIFI_PASSWORD   "34383940"
#define WASD_PORT            3333

#define TIMEOUT_MS        1000  // Timeout in milliseconds (1 second)

volatile char wasd = '0'; // Holds the latest WASD input
TimerHandle_t stop_timer; 

// Motor control pins
#define EN_LEFT  32
#define EN_RIGHT 14
#define IN1      15
#define IN2      33
#define IN3      27
#define IN4       4

// Motor PWM configuration
#define PWM_TIMER LEDC_TIMER_0
#define PWM_MODE LEDC_HIGH_SPEED_MODE
#define PWM_FREQ_HZ 1000
#define PWM_DUTY_RES LEDC_TIMER_10_BIT
#define PWM_MAX_DUTY ((1 << PWM_DUTY_RES) - 1)
#define MIN_SPEED_THRESHOLD (PWM_MAX_DUTY * 0.7) // Ensure speed is above this
#define TURN_SPEED (PWM_MAX_DUTY)                
#define DATA_UPDATED (1 << 0)

#define ADC_CHANNEL ADC_CHANNEL_3  // GPIO26 for ADC1 channel 3
#define ADC_UNIT ADC_UNIT_1        // Use ADC1
#define ADC_ATTEN ADC_ATTEN_DB_12  // Corrected to the new attenuatio
#define DEFAULT_VREF 1100          // Default reference voltage in mV

#define IR_DATA_UPDATED (1 << 1) // Event bit for IR data

static bool reverse = false; 

typedef enum {
    STATE_STOP,
    STATE_TURN,
    STATE_DRIVE_STRAIGHT
} RobotState;

RobotState current_state = STATE_TURN; 

#define TARGET_THRESHOLD 40

static float LEFT_CALIBRATION = 1.0f;
static float RIGHT_CALIBRATION = 0.9f;

// Robot Data Structure
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

typedef struct {
    float x; 
    float z; 
} coordinates; 

coordinates coords[] = {
    {.x = -728.0, .z =  730.0}, // 2
    {.x =  745.0, .z = -467.0}, // 1
    {.x = -725.0, .z = -455.0}, // 4
    {.x =  765.0, .z =  748.0}, // 3
};

volatile float sharp_ir_distance = -1.0; 

static volatile RobotData car;
static EventGroupHandle_t data_event_group;

// Function prototypes
void motor_init();
void set_motor_state(int left_dir1, int left_dir2, int right_dir1, int right_dir2, uint32_t left_speed, uint32_t right_speed);
int parseRobotData(const char *input);
void udp_client_task(void *pvParameters);
void compute_angle_error();
void compute_distance_to_target();

typedef enum {
    REMOTE_CONTROL,
    OPTITRACK_CONTROL
} car_control_mode; 

car_control_mode car_type_state = OPTITRACK_CONTROL;  

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

// Motor Initialization
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

// Parse Optitrack data
int parseRobotData(const char *input) {
    int result = sscanf(input, "%d,%f,%f,%f,%9s", &car.id, &car.x, &car.z, &car.current_theta, car.status);
    car.timestamp = esp_timer_get_time(); // Update timestamp when data is parsed
    return result == 5;
}

// UDP client task
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

void udp_wasd_server_task(void *pvParameters) {
    char rx_buffer[128];
    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(WASD_PORT);

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
    ESP_LOGI(TAG, "Socket bound, port %d", WASD_PORT);

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
            if(wasd == 'e') {
                switch(car_type_state) {
                    case OPTITRACK_CONTROL:
                        car_type_state = REMOTE_CONTROL; 
                        ESP_LOGI(TAG, "Entering Remote control mode..."); 
                        break;

                    case REMOTE_CONTROL:
                        car_type_state = OPTITRACK_CONTROL; 
                        ESP_LOGI(TAG, "Entering Optitrack mode..."); 
                        break;
                }
            }
            reset_timeout_timer(); // Reset the timeout timer
        }

        vTaskDelay(200 / portTICK_PERIOD_MS); 
    }

    if (sock != -1) {
        ESP_LOGE(TAG, "Shutting down socket and restarting...");
        shutdown(sock, 0);
        close(sock);
    }
    vTaskDelete(NULL);
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

// Perform a single timed turn increment
void perform_timed_turn(float interval_ms) {
    uint32_t turn_start_time = xTaskGetTickCount();
    while (fabsf(car.error_angle) > 5.0f) { // Continue turning until error is within threshold
        EventBits_t bits = xEventGroupWaitBits(data_event_group, DATA_UPDATED, pdTRUE, pdFALSE, pdMS_TO_TICKS(1000));
        
        if (!(bits & DATA_UPDATED)) {
            ESP_LOGE(TAG, "No fresh data within timeout. Exiting turn.");
            break; // Exit if no fresh data arrives
        }

        // Add timeout to prevent indefinite spinning
        if ((xTaskGetTickCount() - turn_start_time) > pdMS_TO_TICKS(5000)) {
            ESP_LOGE(TAG, "Turn timeout. Exiting...");
            break;
        }

        // Always turn right, moving clockwise
        if (car.error_angle > 0) {
            ESP_LOGI(TAG, "Turning Right (Clockwise) for %.0f ms, Angular Error: %.2f", interval_ms, car.error_angle);
            set_motor_state(1, 0, 0, 0, TURN_SPEED, 0); // Right turn
        } else {
            ESP_LOGI(TAG, "Turning Left (Counterclockwise) for %.0f ms, Angular Error: %.2f", interval_ms, car.error_angle);
            set_motor_state(0, 0, 1, 0, 0, TURN_SPEED); // Left turn
        }

        // Delay for the specified interval
        vTaskDelay(pdMS_TO_TICKS((uint32_t)interval_ms));
        set_motor_state(0, 0, 0, 0, 0, 0); // Stop motors briefly to allow re-evaluation

        // Update the latest angle error
        compute_angle_error(); 
        ESP_LOGI(TAG, "Reevaluating Angular Error: %.2f", car.error_angle);

        // Short delay to allow sensors or feedback to stabilize
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }

    ESP_LOGI(TAG, "Finished Turning. Final Angular Error: %.2f", car.error_angle);
}

#define DRIVE_STRAIGHT_MS 100

void drive_straight() {
    float base_speed = PWM_MAX_DUTY * 0.8f;
    float left_speed, right_speed;

    ESP_LOGI(TAG, "Driving straight");

    // Continuously drive while periodically checking the state
    while (car_type_state == OPTITRACK_CONTROL) {
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


void task1(void *pvParameters) {

    while (1) {
        if(car_type_state == OPTITRACK_CONTROL) {
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
                    set_motor_state(0, 0, 0, 0, 0, 0); // Stop motors
                
                    if(car.distance_from_target < TARGET_THRESHOLD) {
                       
car.position_idx = (car.position_idx + 1) % 4;  
                        car.target_x = coords[car.position_idx].x;
                        car.target_z = coords[car.position_idx].z;
                    } else {
                        ESP_LOGI(TAG, "Obstacle detected within 20 cm. Waiting...");
                        vTaskDelay(pdMS_TO_TICKS(500));
                    }
                
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
}

void wasd_task() {
    ESP_LOGI(TAG, "Starting WASD motor control...");
    
    static char last_command = '0'; // Track the last executed command
    while(1) {
        if (car_type_state == REMOTE_CONTROL) {
            if (wasd != last_command) { // Execute only if the command changes
                last_command = wasd; // Update the last command

                switch (wasd) {
                case 'w': // Forward
                    ESP_LOGI(TAG, "Forward");
                    set_motor_state(1, 0, 1, 0, PWM_MAX_DUTY, PWM_MAX_DUTY);
                    break;

                case 'a': // Turn Left
                    ESP_LOGI(TAG, "Turn Left");
                    set_motor_state(0, 0, 1, 0, 0, PWM_MAX_DUTY);
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
                    ESP_LOGI(TAG, "Stop");
                    set_motor_state(0, 0, 0, 0, 0, 0);
                    break;

                default: // Invalid Input
                    ESP_LOGW(TAG, "Invalid command: %c", wasd);
                    set_motor_state(0, 0, 0, 0, 0, 0); // Default to stop
                    break;
                }
            }
        }

        // Always yield for 200 ms
        vTaskDelay(pdMS_TO_TICKS(200));
    } 
}

// Main application entry point
void app_main(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(example_connect());

    data_event_group = xEventGroupCreate();
    
    if (data_event_group == NULL) {
        ESP_LOGE(TAG, "Failed to create event group");
        return;
    }  
    motor_init();

    // Initialize shared data
    memset(&car, 0, sizeof(RobotData));
    
    car.position_idx = 0;
    car.target_x = coords[0].x;
    car.target_z = coords[0].z;

    stop_timer = xTimerCreate("Stop Timer", pdMS_TO_TICKS(TIMEOUT_MS), pdFALSE, NULL, timeout_callback);
    if (stop_timer == NULL) {
        ESP_LOGE(TAG, "Failed to create timer");
    }

    ESP_LOGI(TAG, "Initial target index: %d", car.position_idx);
    ESP_LOGI(TAG, "Initial target position: X=%.2f, Z=%.2f", car.target_x, car.target_z);

    compute_angle_error(); 
    compute_distance_to_target(); 

    // Create tasks
    
    xTaskCreate(task1, "control_robot", 4096, NULL, 6, NULL);
    xTaskCreate(udp_client_task, "udp_client", 4096, NULL, 5, NULL);
    xTaskCreate(udp_wasd_server_task, "udp_wasd_server_task", 4096, NULL, 4, NULL); 
    xTaskCreate(wasd_task, "wasd_task", 4096, NULL, 3, NULL); 
    xTaskCreate(ir_sensor_task, "ir_sensor_task", 4096, NULL, 6, NULL);
    
}