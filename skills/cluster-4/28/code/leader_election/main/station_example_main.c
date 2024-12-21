#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "driver/gpio.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_log.h"

// ========================= CONSTANTS ============================
#define WIFI_SSID "Group_7"
#define WIFI_PASS "smartsys"
#define UDP_PORT 3000

#define MAX_NODES 2 // Maximum number of ESP32 nodes in the network

// Array mapping ID to IP addresses
const char *node_ips[MAX_NODES] = {
    "192.168.1.134", // Node ID 0
    "192.168.1.106", // Node ID 1
};

// GPIO Pins
#define BLUEPIN   14 // Leader LED
#define GREENPIN  32 // Follower LED
#define REDPIN    15 // Error/Timeout LED
#define BUTTONPIN 4  // Button for initiating election

// Timing
#define HEARTBEAT_INTERVAL 2000 // Leader sends heartbeat every 2 seconds
#define HEARTBEAT_TIMEOUT  5000 // Follower timeout if no heartbeat within 5 seconds

// ====================== STATE DEFINITIONS ======================
typedef enum {
    STATE_UNCERTAIN = 'U', // No leader elected
    STATE_LEADER    = 'L', // Current node is the leader
    STATE_FOLLOWER  = 'F', // Current node is a follower
} State;

// ======================= GLOBAL VARIABLES ======================
static const char *TAG = "BullyAlgorithm";
SemaphoreHandle_t mutex = NULL;

char myID = 0;           // Device's unique ID
char leaderID = 0;       // Current leader ID
State current_state = STATE_UNCERTAIN;
TickType_t last_heartbeat = 0;

// ==================== FUNCTION PROTOTYPES =====================
void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
void wifi_init();
void send_message_to_all(const char *message);
void button_task(void *arg);
void heartbeat_task(void *arg);
void recv_task(void *arg);
void start_election();
void led_task(void *arg);
void print_current_state();

// ==================== Wi-Fi INIT & EVENT HANDLER ================
void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
        ESP_LOGI(TAG, "Reconnecting to Wi-Fi...");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ESP_LOGI(TAG, "Connected to Wi-Fi network.");
        ESP_LOGI(TAG, "Node joined the network, initiating election...");
        start_election();
    }
}

void wifi_init() {
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);
    esp_wifi_start();

    ESP_LOGI(TAG, "Wi-Fi initialization complete.");
}

void button_task(void *arg) {
    while (1) {
        if (gpio_get_level(BUTTONPIN) == 0) {
            ESP_LOGI(TAG, "Button pressed, initiating election...");
            start_election();
        }
        vTaskDelay(200 / portTICK_PERIOD_MS);
    }
}

// ========================= UDP FUNCTIONS ========================
void send_message_to_all(const char *message) {
    for (int i = 0; i < MAX_NODES; i++) {
        if (i == myID || strcmp(node_ips[i], "0.0.0.0") == 0) {
            continue;
        }

        struct sockaddr_in target_addr;
        target_addr.sin_family = AF_INET;
        target_addr.sin_port = htons(UDP_PORT);
        inet_aton(node_ips[i], &target_addr.sin_addr);

        int udp_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (udp_socket < 0) {
            ESP_LOGE(TAG, "Failed to create socket for sending: %d", errno);
            continue;
        }

        int sent = sendto(udp_socket, message, strlen(message), 0, (struct sockaddr *)&target_addr, sizeof(target_addr));
        if (sent > 0) {
            ESP_LOGI(TAG, "Message sent to Node %d (IP: %s): %s", i, node_ips[i], message);
        } else {
            ESP_LOGE(TAG, "Failed to send message to Node %d (IP: %s). Error: %d", i, node_ips[i], errno);
        }

        close(udp_socket);
    }
}

// ======================= TASK FUNCTIONS =========================

void start_election() {
    xSemaphoreTake(mutex, portMAX_DELAY);
    current_state = STATE_UNCERTAIN; // Reset to uncertain
    leaderID = myID;                 // Assume self as leader initially
    xSemaphoreGive(mutex);

    char message[64];
    snprintf(message, sizeof(message), "E:%d:%d", myID, leaderID);
    send_message_to_all(message);

    ESP_LOGI(TAG, "Election initiated by Node %d", myID);

    // Wait for a response or declare victory
    vTaskDelay(pdMS_TO_TICKS(HEARTBEAT_TIMEOUT));

    xSemaphoreTake(mutex, portMAX_DELAY);
    if (current_state == STATE_UNCERTAIN) {
        // No response means this node becomes the leader
        current_state = STATE_LEADER;
        ESP_LOGI(TAG, "Leader elected: Node %d", myID);

        // Send victory message
        snprintf(message, sizeof(message), "V:%d", myID);
        send_message_to_all(message);
    }
    xSemaphoreGive(mutex);

    print_current_state();
}

void heartbeat_task(void *arg) {
    while (1) {
        if (current_state == STATE_LEADER) {
            char message[64];
            snprintf(message, sizeof(message), "H:%d:%d", myID, leaderID);
            send_message_to_all(message);
            ESP_LOGI(TAG, "Heartbeat sent by Leader (%d)", myID);
        } else if (current_state == STATE_FOLLOWER) {
            // Check for heartbeat timeout
            if ((xTaskGetTickCount() - last_heartbeat) > pdMS_TO_TICKS(HEARTBEAT_TIMEOUT)) {
                ESP_LOGW(TAG, "Heartbeat timeout! Starting a new election...");
                start_election();
            }
        }
        vTaskDelay(HEARTBEAT_INTERVAL / portTICK_PERIOD_MS);
    }
}

void recv_task(void *arg) {
    struct sockaddr_in listen_addr;
    memset(&listen_addr, 0, sizeof(listen_addr));
    listen_addr.sin_family = AF_INET;
    listen_addr.sin_port = htons(UDP_PORT);
    listen_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    int udp_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udp_socket < 0) {
        ESP_LOGE(TAG, "Failed to create socket for receiving: %d", errno);
        vTaskDelete(NULL);
    }

    if (bind(udp_socket, (struct sockaddr *)&listen_addr, sizeof(listen_addr)) < 0) {
        ESP_LOGE(TAG, "Failed to bind socket for receiving: %d", errno);
        close(udp_socket);
        vTaskDelete(NULL);
    }

    char buffer[256]; // Increased buffer size
    struct sockaddr_in source_addr;
    socklen_t addr_len = sizeof(source_addr);

    while (1) {
        int len = recvfrom(udp_socket, buffer, sizeof(buffer) - 1, 0, (struct sockaddr *)&source_addr, &addr_len);
        if (len > 0) {
            buffer[len] = '\0';
            ESP_LOGI(TAG, "Message received from %s:%d -> %s",
                     inet_ntoa(source_addr.sin_addr), ntohs(source_addr.sin_port), buffer);

            char type;
            int senderID, senderLeaderID;
            sscanf(buffer, "%c:%d:%d", &type, &senderID, &senderLeaderID);

            xSemaphoreTake(mutex, portMAX_DELAY);

            if (type == 'E') { // Election message
                if (senderID > myID) {
                    leaderID = senderID;
                    current_state = STATE_FOLLOWER;
                    ESP_LOGI(TAG, "Election: Node %d is the new leader.", leaderID);
                    print_current_state();
                }
            } else if (type == 'H') { // Heartbeat message
                leaderID = senderLeaderID;
                last_heartbeat = xTaskGetTickCount(); // Update last heartbeat
                current_state = STATE_FOLLOWER;
                ESP_LOGI(TAG, "Heartbeat received from Leader (%d)", leaderID);
            } else if (type == 'V') { // Victory message
                leaderID = senderID;
                current_state = (leaderID == myID) ? STATE_LEADER : STATE_FOLLOWER;
                ESP_LOGI(TAG, "Victory: Leader is Node %d", leaderID);
            }

            xSemaphoreGive(mutex);
        }

        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

void led_task(void *arg) {
    while (1) {
        switch (current_state) {
            case STATE_UNCERTAIN:
                gpio_set_level(REDPIN, 1);
                gpio_set_level(BLUEPIN, 0);
                gpio_set_level(GREENPIN, 0);
                break;
            case STATE_LEADER:
                gpio_set_level(REDPIN, 0);
                gpio_set_level(BLUEPIN, 1);
                gpio_set_level(GREENPIN, 0);
                break;
            case STATE_FOLLOWER:
                gpio_set_level(REDPIN, 0);
                gpio_set_level(BLUEPIN, 0);
                gpio_set_level(GREENPIN, 1);
                break;
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

void print_current_state() {
    switch (current_state) {
        case STATE_UNCERTAIN:
            ESP_LOGI(TAG, "Current state: UNCERTAIN");
            break;
        case STATE_LEADER:
            ESP_LOGI(TAG, "Current state: LEADER");
            break;
        case STATE_FOLLOWER:
            ESP_LOGI(TAG, "Current state: FOLLOWER");
            break;
    }
}

// ========================= MAIN FUNCTION ========================
void app_main() {
    esp_reset_reason_t reason = esp_reset_reason();
    ESP_LOGI(TAG, "Reset reason: %d", reason);

    nvs_flash_init();
    wifi_init();

    mutex = xSemaphoreCreateMutex();

    // Initialize GPIO
    esp_rom_gpio_pad_select_gpio(BLUEPIN);
    gpio_set_direction(BLUEPIN, GPIO_MODE_OUTPUT);

    esp_rom_gpio_pad_select_gpio(GREENPIN);
    gpio_set_direction(GREENPIN, GPIO_MODE_OUTPUT);

    esp_rom_gpio_pad_select_gpio(REDPIN);
    gpio_set_direction(REDPIN, GPIO_MODE_OUTPUT);

    esp_rom_gpio_pad_select_gpio(BUTTONPIN);
    gpio_set_direction(BUTTONPIN, GPIO_MODE_INPUT);

    // Create tasks
    xTaskCreate(button_task, "button_task", 4096, NULL, 10, NULL);
    xTaskCreate(heartbeat_task, "heartbeat_task", 4096, NULL, 10, NULL);
    xTaskCreate(recv_task, "recv_task", 4096, NULL, 10, NULL);
    xTaskCreate(led_task, "led_task", 2048, NULL, 10, NULL);
}