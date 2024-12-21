//Michael Barany, Josh Arrevillaga, Sam Kraft

#include <stdio.h>
#include <string.h>
#include <stdlib.h>             
#include <inttypes.h>           
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"     
#include "esp_log.h"
#include "esp_system.h"         
#include "driver/rmt_tx.h"      
#include "soc/rmt_reg.h"        
#include "driver/uart.h"
#include "driver/gptimer.h"     
#include "soc/clk_tree_defs.h"           
#include "driver/gpio.h"        
#include "driver/mcpwm_prelude.h"
#include "driver/ledc.h"   
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_websocket_client.h"
#include "esp_event.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

#define MCPWM_TIMER_RESOLUTION_HZ 10000000 
#define MCPWM_FREQ_HZ             38000    
#define MCPWM_FREQ_PERIOD         263      
#define MCPWM_GPIO_NUM            25

#define LEDC_TIMER              LEDC_TIMER_0
#define LEDC_MODE               LEDC_LOW_SPEED_MODE
#define LEDC_OUTPUT_IO          25
#define LEDC_CHANNEL            LEDC_CHANNEL_0
#define LEDC_DUTY_RES           6     
#define LEDC_DUTY               32    
#define LEDC_FREQUENCY          38000 

#define UART_TX_GPIO_NUM 26 // A0
#define UART_RX_GPIO_NUM 34 // A2
#define BUF_SIZE (1024)
#define BUF_SIZE2 (32)

#define GPIO_INPUT_IO_1       4
#define ESP_INTR_FLAG_DEFAULT 0
#define GPIO_INPUT_PIN_SEL    1ULL<<GPIO_INPUT_IO_1

#define WIFI_SSID "Group_7"
#define WIFI_PASS "smartsys"
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1
#define EXAMPLE_ESP_MAXIMUM_RETRY 5

#define BLUEPIN   14 // Leader LED
#define GREENPIN  32 // Follower LED
#define REDPIN    15 // Error/Timeout LED
#define BUTTONPIN 4  
#define MAX_NODES 3 

// Timing
#define HEARTBEAT_INTERVAL 2000 
#define HEARTBEAT_TIMEOUT  10000 

// Array mapping ID to IP addresses
const char *node_ips[MAX_NODES] = {
    "192.168.1.134", 
    "192.168.1.106", 
    "192.168.1.119"
};

const int node_ports[MAX_NODES] = {
    3000, 
    3001, 
    3002
};

// ====================== STATE DEFINITIONS ======================
typedef enum {
    STATE_UNCERTAIN = 'U', // No leader elected
    STATE_LEADER    = 'L', // Current node is the leader
    STATE_FOLLOWER  = 'F', // Current node is a follower
} State;

// ======================= GLOBAL VARIABLES ======================
SemaphoreHandle_t mutex = NULL;

char start = 0x1B;
int myID = 2;           // Device's unique ID change for each esp
int leaderID = 0;       
State current_state = STATE_UNCERTAIN;
TickType_t last_heartbeat = 0;

static const char *TAG = "wifi station";
static const char *TAG_UART = "UART station";
static int s_retry_num = 0;
static EventGroupHandle_t s_wifi_event_group;
static QueueHandle_t gpio_evt_queue = NULL;
static bool buttonPress = false; 


// ==================== FUNCTION PROTOTYPES =====================
void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
void wifi_init();
void send_message_to_all(const char *message);
void send_confirmation(int senderID);
void recv_task(void *arg);
static void button_init(); 
void button_task();
void heartbeat_task(void *arg);
void start_election();
void led_task(void *arg);
void print_current_state();
static void IRAM_ATTR gpio_isr_handler(void* arg);
char genCheckSum(char *p, int len); 
bool checkCheckSum(uint8_t *p, int len); 

// ==================== (init functions)=========================
static void pwm_init() {

  // Create timer
  mcpwm_timer_handle_t pwm_timer = NULL;
  mcpwm_timer_config_t pwm_timer_config = {
      .group_id = 0,
      .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT,
      .resolution_hz = MCPWM_TIMER_RESOLUTION_HZ,
      .period_ticks = MCPWM_FREQ_PERIOD,
      .count_mode = MCPWM_TIMER_COUNT_MODE_UP,
  };
  ESP_ERROR_CHECK(mcpwm_new_timer(&pwm_timer_config, &pwm_timer));

  // Create operator
  mcpwm_oper_handle_t oper = NULL;
  mcpwm_operator_config_t operator_config = {
      .group_id = 0, // operator must be in the same group to the timer
  };
  ESP_ERROR_CHECK(mcpwm_new_operator(&operator_config, &oper));

  // Connect timer and operator
  ESP_ERROR_CHECK(mcpwm_operator_connect_timer(oper, pwm_timer));

  // Create comparator from the operator
  mcpwm_cmpr_handle_t comparator = NULL;
  mcpwm_comparator_config_t comparator_config = {
      .flags.update_cmp_on_tez = true,
  };
  ESP_ERROR_CHECK(mcpwm_new_comparator(oper, &comparator_config, &comparator));

  // Create generator from the operator
  mcpwm_gen_handle_t generator = NULL;
  mcpwm_generator_config_t generator_config = {
      .gen_gpio_num = MCPWM_GPIO_NUM,
  };
  ESP_ERROR_CHECK(mcpwm_new_generator(oper, &generator_config, &generator));

  // set the initial compare value, so that the duty cycle is 50%
  ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(comparator,132));
  // CANNOT FIGURE OUT HOW MANY TICKS TO COMPARE TO TO GET 50%

  // Set generator action on timer and compare event
  // go high on counter empty
  ESP_ERROR_CHECK(mcpwm_generator_set_action_on_timer_event(generator,
                  MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH)));
  // go low on compare threshold
  ESP_ERROR_CHECK(mcpwm_generator_set_action_on_compare_event(generator,
                  MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, comparator, MCPWM_GEN_ACTION_LOW)));
  ESP_ERROR_CHECK(mcpwm_timer_enable(pwm_timer));
  ESP_ERROR_CHECK(mcpwm_timer_start_stop(pwm_timer, MCPWM_TIMER_START_NO_STOP));

}

static void uart_init() {
  const uart_config_t uart_config = {
      .baud_rate = 1200, 
      .data_bits = UART_DATA_8_BITS,
      .parity    = UART_PARITY_DISABLE,
      .stop_bits = UART_STOP_BITS_1,
      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
      .source_clk = UART_SCLK_DEFAULT
  };
  uart_param_config(UART_NUM_1, &uart_config);
  uart_set_pin(UART_NUM_1, UART_TX_GPIO_NUM, UART_RX_GPIO_NUM, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
  uart_set_line_inverse(UART_NUM_1,UART_SIGNAL_RXD_INV);
  uart_driver_install(UART_NUM_1, BUF_SIZE * 2, 0, 0, NULL, 0);
}


// ==================== Wi-Fi INIT & EVENT HANDLER ================
static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
  {
      esp_wifi_connect();
  }
  else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
  {
      if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY)
      {
          esp_wifi_connect();
          s_retry_num++;
          ESP_LOGI(TAG, "retry to connect to the AP");
      }
      else
      {
          xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
      }
      ESP_LOGI(TAG, "connect to the AP fail");
  }
  else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
  {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    ESP_LOGI(TAG, "Got IP address: " IPSTR, IP2STR(&event->ip_info.ip));
    s_retry_num = 0;
    xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    start_election();
  }
}

void wifi_init(void)
{
  s_wifi_event_group = xEventGroupCreate();

  ESP_ERROR_CHECK(esp_netif_init());

  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_create_default_wifi_sta();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  esp_event_handler_instance_t instance_any_id;
  esp_event_handler_instance_t instance_got_ip;
  ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,ESP_EVENT_ANY_ID,&event_handler,NULL, &instance_any_id));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,&event_handler,NULL,&instance_got_ip));

  wifi_config_t wifi_config = {
      .sta = {
          .ssid = WIFI_SSID,
          .password = WIFI_PASS,
      },
  };
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());

  ESP_LOGI(TAG, "wifi_init_sta finished.");


  EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,pdFALSE,pdFALSE,portMAX_DELAY);

  if (bits & WIFI_CONNECTED_BIT)
  {
      ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",WIFI_SSID, WIFI_PASS);
  }
  else if (bits & WIFI_FAIL_BIT)
  {
      ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",WIFI_SSID, WIFI_PASS);
  }
  else
  {
      ESP_LOGE(TAG, "UNEXPECTED EVENT");
  }
}

static void button_init() {
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_POSEDGE;
    io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);

    gpio_intr_enable(GPIO_INPUT_IO_1 );
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    gpio_isr_handler_add(GPIO_INPUT_IO_1, gpio_isr_handler, (void*) GPIO_INPUT_IO_1);

    //create a queue to handle gpio event from isr
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
}

// ========================= SEND FUNCTIONS ========================

void send_message_to_all(const char *message) {
    for (int i = 0; i < MAX_NODES; i++) {
        if (i == myID || strcmp(node_ips[i], "0.0.0.0") == 0) {
            continue;
        }

        struct sockaddr_in target_addr;
        target_addr.sin_family = AF_INET;
        target_addr.sin_port = htons(node_ports[i]);
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
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void send_message_to_server(uint8_t *message, int message_len) {
    int udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_socket < 0) {
        ESP_LOGE(TAG, "Socket creation failed, errno: %d", errno);
        return;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8080); 

   
    const char *server_ip = "192.168.1.111"; 

    int ret = inet_pton(AF_INET, server_ip, &server_addr.sin_addr);
    if (ret <= 0) {
        ESP_LOGE(TAG, "Invalid server IP address: %s", server_ip);
        close(udp_socket);
        return;
    }

    ESP_LOGI(TAG, "Sending message to server: %s", message);

    ssize_t sent_bytes = sendto(udp_socket, message, 32, 0,(struct sockaddr *)&server_addr, sizeof(server_addr));
    if (sent_bytes < 0) {
        ESP_LOGE(TAG, "Failed to send to server, errno: %d (%s)", errno, strerror(errno));
    } else {
        ESP_LOGI(TAG, "Message sent to server");
    }

    close(udp_socket);
}

void send_confirmation(int senderID) {
    if (senderID < 0 || senderID >= MAX_NODES) {
        ESP_LOGE(TAG, "Invalid sender ID: %d", senderID);
        return;
    }

    struct sockaddr_in target_addr;
    target_addr.sin_family = AF_INET;
    target_addr.sin_port = htons(node_ports[senderID]);
    inet_aton(node_ips[senderID], &target_addr.sin_addr);

    int udp_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udp_socket < 0) {
        ESP_LOGE(TAG, "Failed to create socket for confirmation: %d", errno);
        return;
    }

    char message[64];
    snprintf(message, sizeof(message), "C:%d:ACK", myID); // Example confirmation format: "C:<myID>:ACK"

    int sent = sendto(udp_socket, message, strlen(message), 0, (struct sockaddr *)&target_addr, sizeof(target_addr));
    if (sent > 0) {
        ESP_LOGI(TAG, "Confirmation sent to Node %d (IP: %s): %s", senderID, node_ips[senderID], message);
    } else {
        ESP_LOGE(TAG, "Failed to send confirmation to Node %d (IP: %s). Error: %d", senderID, node_ips[senderID], errno);
    }

    close(udp_socket);
}


void send_ir_task() {
    char *data_out = (char *)malloc(5);  // Allocate memory outside the loop
    if (data_out == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for IR data");
        vTaskDelete(NULL);  // Exit task if allocation fails
    }

    while (1) {
        // Check button state
        if (buttonPress == true) {  // Button is pressed (active-low)
            ESP_LOGI(TAG, "Button pressed, sending IR message...");

            xSemaphoreTake(mutex, portMAX_DELAY);
            data_out[0] = start;               // Start byte
            data_out[1] = 'S';                // Message type (Election,Heartbeat, Victory)
            data_out[2] = myID;         // voted ID
            data_out[3] = 'G';     // Vote
            data_out[4] = genCheckSum(data_out, 5 - 1);  // Checksum

            uart_write_bytes(UART_NUM_1, data_out, 5);  // Send data via UART
            xSemaphoreGive(mutex);

            ESP_LOGI(TAG, "IR message sent: Start: %c, MsgType: %c, MyID: %d, LeaderID: %c",
                     data_out[0], data_out[1], data_out[2], data_out[3]);

            buttonPress = false; 
            // Debounce delay
            vTaskDelay(200 / portTICK_PERIOD_MS);  // Wait for 200ms to debounce button
        }

        // Small delay to prevent continuous polling
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }

    free(data_out);  // Free memory if exiting (though this should never happen in a loop)
}

// ======================= RECV FUNCTIONS =========================

void recv_ir_task(){
  uint8_t *data_in = (uint8_t *) malloc(BUF_SIZE2);
  if(data_in == NULL) {
    ESP_LOGE(TAG, "Failure to allocate memory to data_in..."); 
    vTaskDelete(NULL); 
  }
  
  while (1) {
    int len_in = uart_read_bytes(UART_NUM_1, data_in, BUF_SIZE2, 100 / portTICK_PERIOD_MS);
    ESP_LOGE(TAG_UART, "Length: %d", len_in);
    if (len_in > 0) {
      int nn = 0;
      while (data_in[nn] != start) {
        nn++;
      }
      uint8_t copied[5];
      memcpy(copied, data_in + nn, 5 * sizeof(uint8_t));
      
      if (checkCheckSum(copied,5)) {
        char message[64]; // Buffer to hold the formatted string
        snprintf(message, sizeof(message), "%c:%d:%c", copied[1], copied[2], copied[3]); // Format the IR message
        int coordinator_socket;
        struct sockaddr_in server_addr;
        if ((coordinator_socket = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
            perror("Socket creation failed");
            exit(EXIT_FAILURE);
        } 
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(node_ports[leaderID]);
        if (inet_pton(AF_INET, node_ips[leaderID], &server_addr.sin_addr) <= 0) {
            perror("Invalid IP address");
            close(coordinator_socket);
            exit(EXIT_FAILURE);
        }
        if (sendto(coordinator_socket, message, 5, 0, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
            perror("Message send failed");
        } else {
            printf("Message sent to %s:%d\n", node_ips[leaderID], node_ports[leaderID]);
        }

        close(coordinator_socket);        

        ESP_LOG_BUFFER_HEXDUMP(TAG_UART, copied,len_in, ESP_LOG_INFO);
        uart_flush(UART_NUM_1);
      }
    }
    else{
      // printf("Nothing received.\n");
    }
    vTaskDelay(100);
  }
  free(data_in);
}

void recv_task(void *arg) {
    struct sockaddr_in listen_addr;
    memset(&listen_addr, 0, sizeof(listen_addr));
    listen_addr.sin_family = AF_INET;
    listen_addr.sin_port = htons(node_ports[myID]);
    listen_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    uint8_t *data_in = (uint8_t *)malloc(BUF_SIZE2);
    if (data_in == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for buffer");
        vTaskDelete(NULL);
    }

    int udp_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udp_socket < 0) {
        ESP_LOGE(TAG, "Failed to create socket: %d", errno);
        free(data_in);
        vTaskDelete(NULL);
    }

    if (bind(udp_socket, (struct sockaddr *)&listen_addr, sizeof(listen_addr)) < 0) {
        ESP_LOGE(TAG, "Failed to bind socket: %d", errno);
        close(udp_socket);
        free(data_in);
        vTaskDelete(NULL);
    }

    ESP_LOGI(TAG, "Listening for messages on port %d", node_ports[myID]);

    while (1) {
        struct sockaddr_in source_addr;
        socklen_t addr_len = sizeof(source_addr);
        memset(data_in, 0, BUF_SIZE2); // Clear the buffer

        int len = recvfrom(udp_socket, data_in, BUF_SIZE2 - 1, 0, (struct sockaddr *)&source_addr, &addr_len);
        if (len > 0) {
            data_in[len] = '\0'; // Null-terminate the string

            ESP_LOGI(TAG, "Raw message from %s:%d", inet_ntoa(source_addr.sin_addr), ntohs(source_addr.sin_port));
            ESP_LOG_BUFFER_HEXDUMP(TAG, data_in, len, ESP_LOG_INFO);

            // Validate message format
            if (data_in[0] == 'E' || data_in[0] == 'H' || data_in[0] == 'V' || data_in[0]== 'S' || data_in[0]== 'C') {
                char type;
                int senderID, senderLeaderID;
                sscanf((char *)data_in, "%c:%d:%d", &type, &senderID, &senderLeaderID);

                xSemaphoreTake(mutex, portMAX_DELAY);
                switch (type) {
                    case 'E': // Election
                        ESP_LOGI(TAG, "Election message received from Node %d", senderID);
                        if (senderID > myID) {
                            leaderID = senderID;
                            current_state = STATE_FOLLOWER;
                            print_current_state();
                        }
                        break;

                    case 'H': // Heartbeat
                        ESP_LOGI(TAG, "Heartbeat from Leader %d", senderLeaderID);
                        leaderID = senderLeaderID;
                        last_heartbeat = xTaskGetTickCount();
                        current_state = STATE_FOLLOWER;
                        break;

                    case 'V': // Victory
                        ESP_LOGI(TAG, "Victory message: Leader is Node %d", senderID);
                        leaderID = senderID;
                        current_state = (leaderID == myID) ? STATE_LEADER : STATE_FOLLOWER;
                        break;
                    case 'S':
                    
                        if(leaderID == myID){
                                send_message_to_server(data_in, sizeof(data_in));
                                send_confirmation(senderID);

                        }

                    break;

                    default:
                        ESP_LOGW(TAG, "Unknown message type: %c", type);
                        break;
                }
                xSemaphoreGive(mutex);
            } else {
                ESP_LOGW(TAG, "Invalid message format: %s", (char *)data_in);
            }
        } else {
            ESP_LOGE(TAG, "recvfrom failed: %d", errno);
        }

        vTaskDelay(100 / portTICK_PERIOD_MS); 
    }

    free(data_in);
    close(udp_socket);
}

// ======================= ELECTION FUNCTIONS =========================

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
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}

// ======================= UTILITY FUNCTIONS =========================

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
                gpio_set_level(BLUEPIN,1);
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

void button_task() {
    uint32_t io_num;
    while (1) {
        if (xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
            xSemaphoreTake(mutex, portMAX_DELAY);
            buttonPress = true;  // Set the flag
            xSemaphoreGive(mutex);

            printf("Button pressed.\n");
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);  // Small delay to avoid rapid queue processing
    }
}

char genCheckSum(char *p, int len) {
  char temp = 0;
  for (int i = 0; i < len; i++){
    temp = temp^p[i];
  }
  // printf("%X\n",temp);  // Diagnostic

  return temp;
}

bool checkCheckSum(uint8_t *p, int len) {
  char temp = (char) 0;
  bool isValid;
  for (int i = 0; i < len-1; i++){
    temp = temp^p[i];
  }
  // printf("Check: %02X ", temp); // Diagnostic
  if (temp == p[len-1]) {
    isValid = true; }
  else {
    isValid = false; }
  return isValid;
}

static void IRAM_ATTR gpio_isr_handler(void* arg){
  uint32_t gpio_num = (uint32_t) arg;
  xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

// ========================= MAIN FUNCTION ========================
void app_main() {
    esp_reset_reason_t reason = esp_reset_reason();
    ESP_LOGI(TAG, "Reset reason: %d", reason);
    mutex = xSemaphoreCreateMutex();
    if (mutex == NULL) {
    ESP_LOGE(TAG, "Failed to create mutex!");
    return;
    }
    
    pwm_init();
    uart_init();
    nvs_flash_init();
    wifi_init();
    button_init(); 

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
    xTaskCreate(send_ir_task, "send_ir_task", 4096, NULL, 10, NULL);
    xTaskCreate(heartbeat_task, "heartbeat_task", 4096, NULL, 10, NULL);
    xTaskCreate(recv_task, "recv_task", 4096, NULL, 12, NULL);
    xTaskCreate(recv_ir_task, "recv_ir_task", 4096, NULL, 12, NULL);
    xTaskCreate(led_task, "led_task", 2048, NULL, 10, NULL);
    xTaskCreate(button_task, "button_task", 1024*2, NULL, 5, NULL);
}
