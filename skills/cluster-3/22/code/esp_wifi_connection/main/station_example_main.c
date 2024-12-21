#include <string.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

#define PORT 4444  // UDP port for communication
#define HEARTBEAT_INTERVAL 2000  // 2 seconds
#define TIMEOUT_INTERVAL 5000    // 5 seconds
#define MY_ID 1                  // Unique ID for this node

static const char *TAG = "leader_election";

// WiFi credentials
#define WIFI_SSID "Group_7"
#define WIFI_PASS "smartsys"

// Peer IP addresses
const char *PEER_IPS[] = {
    "192.168.1.118",  // ESP32 Node 1
    "192.168.1.134"   // ESP32 Node 2
};
const int NUM_PEERS = 2;

int my_id = MY_ID;          // Node ID
int leader_id = -1;         // Current leader ID
int sock;                   // UDP socket

typedef enum {
    STATE_UNCERTAIN,
    STATE_LEADER,
    STATE_FOLLOWER
} NodeState;

NodeState current_state = STATE_UNCERTAIN;

// Function prototypes
void send_message(const char *message, int target_id);
void start_election();
void send_heartbeat();
void process_message(const char *message, const char *sender_ip);

// WiFi event handler
static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
    }
}

void wifi_init_sta() {
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &instance_any_id);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &instance_got_ip);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);
    esp_wifi_start();
}

// Function to send a message to a specific target
void send_message(const char *message, int target_id) {
    if (target_id < 0 || target_id >= NUM_PEERS) {
        ESP_LOGE(TAG, "Invalid target ID: %d", target_id);
        return;
    }

    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = inet_addr(PEER_IPS[target_id]);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(PORT);

    int err = sendto(sock, message, strlen(message), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err < 0) {
        ESP_LOGE(TAG, "Failed to send message to Node %d (%s): errno %d", target_id, PEER_IPS[target_id], errno);
    } else {
        ESP_LOGI(TAG, "Message sent to Node %d (%s): %s", target_id, PEER_IPS[target_id], message);
    }
}

// Start an election
void start_election() {
    current_state = STATE_UNCERTAIN;
    char message[32];
    snprintf(message, sizeof(message), "E:%d", my_id); // Election message format: "E:<my_id>"
    for (int i = 0; i < NUM_PEERS; i++) {
        if (i != my_id) {  // Don't send to itself
            send_message(message, i);
        }
    }
    ESP_LOGI(TAG, "Election started by Node %d", my_id);
}

// Send a heartbeat
void send_heartbeat() {
    if (current_state == STATE_LEADER) {
        char message[32];
        snprintf(message, sizeof(message), "H:%d", my_id); // Heartbeat message format: "H:<my_id>"
        for (int i = 0; i < NUM_PEERS; i++) {
            if (i != my_id) {  // Don't send to itself
                send_message(message, i);
            }
        }
        ESP_LOGI(TAG, "Heartbeat sent by Leader (%d)", my_id);
    }
}

// Process incoming messages
void process_message(const char *message, const char *sender_ip) {
    char type;
    int sender_id;

    sscanf(message, "%c:%d", &type, &sender_id);

    switch (type) {
        case 'E': // Election message
            if (sender_id > my_id) {
                leader_id = sender_id;
                current_state = STATE_FOLLOWER;
                ESP_LOGI(TAG, "Election: Node %d is the leader", leader_id);
            } else if (sender_id < my_id) {
                start_election(); // Start an election if this node has higher priority
            }
            break;
        case 'H': // Heartbeat message
            ESP_LOGI(TAG, "Heartbeat received from Leader (%d)", sender_id);
            leader_id = sender_id;
            current_state = STATE_FOLLOWER;
            break;
        case 'V': // Victory message
            leader_id = sender_id;
            current_state = STATE_FOLLOWER;
            ESP_LOGI(TAG, "Victory: Node %d is the leader", leader_id);
            break;
        default:
            ESP_LOGW(TAG, "Unknown message type: %s", message);
            break;
    }
}

// UDP server task
static void udp_server_task(void *pvParameters) {
    char rx_buffer[128];
    char addr_str[128];
    struct sockaddr_in source_addr;
    socklen_t socklen = sizeof(source_addr);

    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        vTaskDelete(NULL);
        return;
    }

    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(PORT);

    int err = bind(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err < 0) {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        close(sock);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "Socket bound, listening on port %d", PORT);

    while (1) {
        int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0, (struct sockaddr *)&source_addr, &socklen);
        if (len < 0) {
            ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
            break;
        } else {
            rx_buffer[len] = 0; // Null-terminate received data
            inet_ntoa_r(source_addr.sin_addr, addr_str, sizeof(addr_str) - 1);
            ESP_LOGI(TAG, "Received %d bytes from %s: %s", len, addr_str, rx_buffer);
            process_message(rx_buffer, addr_str);
        }
    }

    if (sock != -1) {
        ESP_LOGE(TAG, "Shutting down socket and restarting...");
        shutdown(sock, 0);
        close(sock);
    }
    vTaskDelete(NULL);
}

void app_main(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    wifi_init_sta();

    xTaskCreate(udp_server_task, "udp_server", 4096, NULL, 5, NULL);
    xTaskCreate((TaskFunction_t)send_heartbeat, "heartbeat_task", 2048, NULL, 5, NULL);
}
