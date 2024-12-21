#include <stdio.h>
#include "pins.h"
#include "driver/i2c.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "char_binary.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#define ESP_INTR_FLAG_DEFAULT 0

#define BUTTON_PIN A0

// 14-Segment Display
#define SLAVE_ADDR 0x70              // alphanumeric address
#define OSC 0x21                     // oscillator cmd
#define HT16K33_BLINK_DISPLAYON 0x01 // Display on cmd
#define HT16K33_BLINK_OFF 0          // Blink off cmd
#define HT16K33_BLINK_CMD 0x80       // Blink cmd
#define HT16K33_CMD_BRIGHTNESS 0xE0  // Brightness cmd

// Master I2C
#define I2C_EXAMPLE_MASTER_SCL_IO 22        // gpio number for i2c clk
#define I2C_EXAMPLE_MASTER_SDA_IO 23        // gpio number for i2c data
#define I2C_EXAMPLE_MASTER_NUM I2C_NUM_0    // i2c port
#define I2C_EXAMPLE_MASTER_TX_BUF_DISABLE 0 // i2c master no buffer needed
#define I2C_EXAMPLE_MASTER_RX_BUF_DISABLE 0 // i2c master no buffer needed
#define I2C_EXAMPLE_MASTER_FREQ_HZ 100000   // i2c master clock freq
#define WRITE_BIT I2C_MASTER_WRITE          // i2c master write
#define READ_BIT I2C_MASTER_READ            // i2c master read
#define ACK_CHECK_EN true                   // i2c master will check ack
#define ACK_CHECK_DIS false                 // i2c master will not check ack
#define ACK_VAL 0x00                        // i2c ack value
#define NACK_VAL 0xFF                       // i2c ttttnack valuet

#define UART_PORT_NUM UART_NUM_0 // Use UART1
#define UART_TX_PIN 1
#define UART_RX_PIN 3
#define BUF_SIZE 1024

// Function to initiate
static void i2c_example_master_init()
{
    // Debug
    printf("\n>> i2c Config\n");
    int err;

    // Port configuration
    int i2c_master_port = I2C_EXAMPLE_MASTER_NUM;

    /// Define I2C configurations
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;                        // Master mode
    conf.sda_io_num = I2C_EXAMPLE_MASTER_SDA_IO;        // Default SDA pin
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;            // Internal pullup
    conf.scl_io_num = I2C_EXAMPLE_MASTER_SCL_IO;        // Default SCL pin
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;            // Internal pullup
    conf.master.clk_speed = I2C_EXAMPLE_MASTER_FREQ_HZ; // CLK frequency
    conf.clk_flags = 0;                                 // <-- UNCOMMENT IF YOU GET ERRORS (see readme.md)
    err = i2c_param_config(i2c_master_port, &conf);     // Configure
    if (err == ESP_OK)
    {
        printf("- parameters: ok\n");
    }

    // Install I2C driver
    err = i2c_driver_install(i2c_master_port, conf.mode,
                             I2C_EXAMPLE_MASTER_RX_BUF_DISABLE,
                             I2C_EXAMPLE_MASTER_TX_BUF_DISABLE, 0);
    // i2c_set_data_mode(i2c_master_port,I2C_DATA_MODE_LSB_FIRST,I2C_DATA_MODE_LSB_FIRST);
    if (err == ESP_OK)
    {
        printf("- initialized: yes\n\n");
    }

    // Dat in MSB mode
    i2c_set_data_mode(i2c_master_port, I2C_DATA_MODE_MSB_FIRST, I2C_DATA_MODE_MSB_FIRST);
}

static void uart_init()
{
    const uart_config_t uart_config = {
        .baud_rate = 115200,                   // Set baud rate
        .data_bits = UART_DATA_8_BITS,         // Data bits (8 bits per frame)
        .parity = UART_PARITY_DISABLE,         // Disable parity check
        .stop_bits = UART_STOP_BITS_1,         // Use 1 stop bit
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE, // Disable hardware flow control
        .source_clk = UART_SCLK_APB,           // Set clock source
    };

    uart_param_config(UART_PORT_NUM, &uart_config);
    uart_set_pin(UART_PORT_NUM, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(UART_PORT_NUM, 1024, 0, 0, NULL, 0);
}

// Utility  Functions //////////////////////////////////////////////////////////

// Utility function to test for I2C device address -- not used in deploy
int testConnection(uint8_t devAddr, int32_t timeout)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (devAddr << 1) | I2C_MASTER_WRITE, ACK_CHECK_EN);
    i2c_master_stop(cmd);
    int err = i2c_master_cmd_begin(I2C_EXAMPLE_MASTER_NUM, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    return err;
}

// Utility function to scan for i2c device
static void i2c_scanner()
{
    int32_t scanTimeout = 1000;
    printf("\n>> I2C scanning ..."
           "\n");
    uint8_t count = 0;
    for (uint8_t i = 1; i < 127; i++)
    {
        // printf("0x%X%s",i,"\n");
        if (testConnection(i, scanTimeout) == ESP_OK)
        {
            printf("- Device found at address: 0x%X%s", i, "\n");
            count++;
        }
    }
    if (count == 0)
        printf("- No I2C devices found!"
               "\n");
    printf("\n");
}

////////////////////////////////////////////////////////////////////////////////

// Alphanumeric Functions //////////////////////////////////////////////////////

// Turn on oscillator for alpha display
int alpha_oscillator()
{
    int ret;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (SLAVE_ADDR << 1) | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, OSC, ACK_CHECK_EN);
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(I2C_EXAMPLE_MASTER_NUM, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    vTaskDelay(200 / portTICK_PERIOD_MS);
    return ret;
}

// Set blink rate to off
int no_blink()
{
    int ret;
    i2c_cmd_handle_t cmd2 = i2c_cmd_link_create();
    i2c_master_start(cmd2);
    i2c_master_write_byte(cmd2, (SLAVE_ADDR << 1) | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd2, HT16K33_BLINK_CMD | HT16K33_BLINK_DISPLAYON | (HT16K33_BLINK_OFF << 1), ACK_CHECK_EN);
    i2c_master_stop(cmd2);
    ret = i2c_master_cmd_begin(I2C_EXAMPLE_MASTER_NUM, cmd2, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd2);
    vTaskDelay(200 / portTICK_PERIOD_MS);
    return ret;
}

// Set Brightness
int set_brightness_max(uint8_t val)
{
    int ret;
    i2c_cmd_handle_t cmd3 = i2c_cmd_link_create();
    i2c_master_start(cmd3);
    i2c_master_write_byte(cmd3, (SLAVE_ADDR << 1) | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd3, HT16K33_CMD_BRIGHTNESS | val, ACK_CHECK_EN);
    i2c_master_stop(cmd3);
    ret = i2c_master_cmd_begin(I2C_EXAMPLE_MASTER_NUM, cmd3, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd3);
    vTaskDelay(200 / portTICK_PERIOD_MS);
    return ret;
}

////////////////////////////////////////////////////////////////////////////////

static void setup_alpha_display()
{
    // Debug
    int ret;
    printf(">> Test Alphanumeric Display: \n");

    // Set up routines
    // Turn on alpha oscillator
    ret = alpha_oscillator();

    if (ret == ESP_OK)
    {
        printf("- oscillator: ok \n");
    }

    // Set display blink off

    ret = no_blink();

    if (ret == ESP_OK)
    {
        printf("- blink: off \n");
    }

    ret = set_brightness_max(0xF);

    if (ret == ESP_OK)
    {
        printf("- brightness: max \n");
    }
}

// Create variable to store the string to display - can be longer than 4 characters
uint8_t *text;
int text_length = 0;

void append_from_uart()
{
    uint8_t *data = (uint8_t *)malloc(BUF_SIZE);
    while (1)
    {
        int len = uart_read_bytes(UART_PORT_NUM, data, (BUF_SIZE - 1), 20 / portTICK_PERIOD_MS);
        if (len > 0)
        {
            printf("Read %d bytes: '%c'\n", len, *data);
            for (int i = 0; i < len; i++)
            {
                text[text_length] = data[i];
                text_length++;
                text[text_length + 1] = '\0';
            }
        }
    }
}

void write_to_display(uint16_t *display_buffer)
{
    i2c_cmd_handle_t cmd4 = i2c_cmd_link_create();
    i2c_master_start(cmd4);
    i2c_master_write_byte(cmd4, (SLAVE_ADDR << 1) | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd4, (uint8_t)0x00, ACK_CHECK_EN);
    for (int j = 0; j < 8; j++)
    {
        i2c_master_write_byte(cmd4, display_buffer[j] & 0xFF, ACK_CHECK_EN);
        i2c_master_write_byte(cmd4, display_buffer[j] >> 8, ACK_CHECK_EN);
    }
    i2c_master_stop(cmd4);
    i2c_master_cmd_begin(I2C_EXAMPLE_MASTER_NUM, cmd4, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd4);
}



volatile bool timer_running = false; // Flag to track if the counter is running
int counter = 0;                     // Counter variable

// Function to update the display with the current counter value
void update_display_with_counter() {
    uint16_t display_buffer[8];
    int tens = (counter / 10) % 10;
    int units = counter % 10;

    display_buffer[0] = char_to_display('0' + tens);
    display_buffer[1] = char_to_display('0' + units);
    for (int j = 2; j < 8; j++) {
        display_buffer[j] = char_to_display(' ');
    }

    write_to_display(display_buffer);
}

// ISR for button press to toggle the counter
void IRAM_ATTR button_isr_handler(void *arg) {
    // Toggle the timer_running flag
    timer_running = !timer_running;

    // Reset counter if timer is restarted
    if (timer_running) {
        counter = 0;
    }
}


void display_scrolling_text() {
    uint16_t display_buffer[8];

    while (1) {
        // If the counter is running and hasn't reached 15

        
        if (timer_running) {
            update_display_with_counter(); // Display the current counter value
            counter++;                     // Increment the counter
            vTaskDelay(1000 / portTICK_PERIOD_MS); // Delay 1 second
        } 
        else if (!timer_running) {
            // Display "00" when timer is stopped
            display_buffer[0] = char_to_display('0');
            display_buffer[1] = char_to_display('0');
            for (int j = 2; j < 8; j++) {
                display_buffer[j] = char_to_display(' ');
            }
            write_to_display(display_buffer);
        }
        
        vTaskDelay(500 / portTICK_PERIOD_MS); // Check the button status every 500 ms
    }
}




void app_main()
{
    // Setup the text buffer
    text = (uint8_t *)malloc(BUF_SIZE);
    gpio_config_t io_conf;
 // Configure the button pin
    io_conf.intr_type = GPIO_INTR_POSEDGE;          // Interrupt on rising edge (button press)
    io_conf.mode = GPIO_MODE_INPUT;                 // Set as input
    io_conf.pin_bit_mask = (1ULL << BUTTON_PIN);    // Button GPIO 32
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;        // Enable internal pull-up
    gpio_config(&io_conf);

 // Install ISR service and attach the button ISR handler
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    gpio_isr_handler_add(BUTTON_PIN, button_isr_handler, NULL);


    i2c_example_master_init();
    i2c_scanner();
    uart_init();
    setup_alpha_display();
    xTaskCreate(append_from_uart, "append_from_uart", 1024 * 2, NULL, 1, NULL);
    xTaskCreate(display_scrolling_text, "display_scrolling_text", 1024 * 2, NULL, 1, NULL);
}