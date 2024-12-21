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

volatile RobotData car; 

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

    xTaskCreate(udp_client_task, "udp_client", 4096, NULL, 5, NULL);
    
}