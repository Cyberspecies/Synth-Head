/**
 * @file wifi_led_gpu_sender.cpp
 * @brief GPU WiFi LED Sender - Sends LED data via WiFi at 60 FPS
 * 
 * Flow:
 * 1. GPU waits for WiFi config from CPU via UART
 * 2. GPU connects to same WiFi network
 * 3. GPU sends LED data to CPU via UDP at 60 FPS
 * 4. GPU receives button state from CPU via UDP
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"

extern "C" {
  #include "WiFiLedProtocol.h"
}

static const char* TAG = "WIFI_LED_GPU";

// UART Configuration (for receiving WiFi config from CPU)
#define UART_PORT_NUM UART_NUM_1
#define UART_TX_PIN GPIO_NUM_12
#define UART_RX_PIN GPIO_NUM_13
#define UART_BAUD_RATE 921600
#define UART_BUF_SIZE 1024

// WiFi configuration (received from CPU)
static char wifi_ssid[WIFI_SSID_MAX_LEN] = {0};
static char wifi_password[WIFI_PASSWORD_MAX_LEN] = {0};
static uint32_t cpu_ip_raw = 0;
static uint16_t led_port = DEFAULT_LED_PORT;
static uint16_t button_port = DEFAULT_BUTTON_PORT;
static bool wifi_config_received = false;
static bool wifi_connected = false;

// UDP socket
static int udp_socket = -1;
static struct sockaddr_in cpu_addr;

// LED data
static LedDataPacket led_packet;

// Button state
static bool button_a = false;
static bool button_b = false;
static bool button_c = false;
static bool button_d = false;

/**
 * @brief Calculate CRC8 checksum
 */
uint8_t calculateCRC8(const uint8_t* data, size_t length){
  uint8_t crc = 0x00;
  for(size_t i = 0; i < length; i++){
    crc ^= data[i];
    for(int j = 0; j < 8; j++){
      if(crc & 0x80){
        crc = (crc << 1) ^ 0x07;
      }else{
        crc <<= 1;
      }
    }
  }
  return crc;
}

/**
 * @brief WiFi event handler
 */
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data){
  if(event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START){
    esp_wifi_connect();
  }else if(event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED){
    ESP_LOGI(TAG, "WiFi disconnected, reconnecting...");
    wifi_connected = false;
    esp_wifi_connect();
  }else if(event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP){
    ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
    ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
    wifi_connected = true;
  }
}

/**
 * @brief Initialize UART for receiving WiFi config
 */
static void initUart(){
  uart_config_t uart_config = {
    .baud_rate = UART_BAUD_RATE,
    .data_bits = UART_DATA_8_BITS,
    .parity = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    .source_clk = UART_SCLK_APB,
  };
  
  ESP_ERROR_CHECK(uart_param_config(UART_PORT_NUM, &uart_config));
  ESP_ERROR_CHECK(uart_set_pin(UART_PORT_NUM, UART_TX_PIN, UART_RX_PIN, 
                                UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
  ESP_ERROR_CHECK(uart_driver_install(UART_PORT_NUM, UART_BUF_SIZE, UART_BUF_SIZE, 0, NULL, 0));
  
  ESP_LOGI(TAG, "UART initialized: RX=%d, TX=%d, Baud=%d", UART_RX_PIN, UART_TX_PIN, UART_BAUD_RATE);
}

/**
 * @brief Receive WiFi config from CPU via UART
 */
static bool receiveWiFiConfig(){
  ESP_LOGI(TAG, "Waiting for WiFi config from CPU...");
  
  WiFiConfig config;
  uint8_t* buffer = (uint8_t*)&config;
  int bytes_needed = sizeof(WiFiConfig);
  int bytes_received = 0;
  
  // Wait up to 30 seconds for config
  int64_t start_time = esp_timer_get_time();
  int64_t timeout = 30000000; // 30 seconds
  
  while(bytes_received < bytes_needed){
    int available = uart_read_bytes(UART_PORT_NUM, 
                                    &buffer[bytes_received], 
                                    bytes_needed - bytes_received, 
                                    100 / portTICK_PERIOD_MS);
    
    if(available > 0){
      bytes_received += available;
      ESP_LOGI(TAG, "Received %d/%d bytes", bytes_received, bytes_needed);
    }
    
    // Timeout check
    if((esp_timer_get_time() - start_time) > timeout){
      ESP_LOGE(TAG, "Timeout waiting for WiFi config");
      return false;
    }
  }
  
  // Validate sync markers
  if(config.sync1 != WIFI_CONFIG_SYNC_1 || config.sync2 != WIFI_CONFIG_SYNC_2){
    ESP_LOGE(TAG, "Invalid sync markers: 0x%02X 0x%02X", config.sync1, config.sync2);
    return false;
  }
  
  // Validate CRC
  uint8_t calculated_crc = calculateCRC8((uint8_t*)&config, sizeof(WiFiConfig) - 1);
  if(config.crc != calculated_crc){
    ESP_LOGE(TAG, "CRC mismatch: got 0x%02X, expected 0x%02X", config.crc, calculated_crc);
    return false;
  }
  
  // Store config
  strncpy(wifi_ssid, config.ssid, WIFI_SSID_MAX_LEN - 1);
  strncpy(wifi_password, config.password, WIFI_PASSWORD_MAX_LEN - 1);
  cpu_ip_raw = config.cpu_ip;
  led_port = config.led_port;
  button_port = config.button_port;
  
  ESP_LOGI(TAG, "WiFi config received:");
  ESP_LOGI(TAG, "  SSID: %s", wifi_ssid);
  ESP_LOGI(TAG, "  CPU IP: %d.%d.%d.%d", 
           (cpu_ip_raw & 0xFF), ((cpu_ip_raw >> 8) & 0xFF),
           ((cpu_ip_raw >> 16) & 0xFF), ((cpu_ip_raw >> 24) & 0xFF));
  ESP_LOGI(TAG, "  LED Port: %d", led_port);
  ESP_LOGI(TAG, "  Button Port: %d", button_port);
  
  wifi_config_received = true;
  return true;
}

/**
 * @brief Connect to WiFi
 */
static void connectWiFi(){
  ESP_LOGI(TAG, "Connecting to WiFi: %s", wifi_ssid);
  
  // Initialize TCP/IP
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_create_default_wifi_sta();
  
  // Initialize WiFi
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
  
  // Register event handlers
  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, 
                                              &wifi_event_handler, NULL));
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, 
                                              &wifi_event_handler, NULL));
  
  // Configure WiFi
  wifi_config_t wifi_config = {};
  strncpy((char*)wifi_config.sta.ssid, wifi_ssid, sizeof(wifi_config.sta.ssid) - 1);
  strncpy((char*)wifi_config.sta.password, wifi_password, sizeof(wifi_config.sta.password) - 1);
  
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());
  
  ESP_LOGI(TAG, "WiFi started, waiting for connection...");
}

/**
 * @brief Initialize UDP socket
 */
static void initUdpSocket(){
  // Create UDP socket
  udp_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if(udp_socket < 0){
    ESP_LOGE(TAG, "Failed to create socket");
    return;
  }
  
  // Setup CPU address
  memset(&cpu_addr, 0, sizeof(cpu_addr));
  cpu_addr.sin_family = AF_INET;
  cpu_addr.sin_port = htons(led_port);
  cpu_addr.sin_addr.s_addr = cpu_ip_raw;
  
  ESP_LOGI(TAG, "UDP socket created for %d.%d.%d.%d:%d",
           (cpu_ip_raw & 0xFF), ((cpu_ip_raw >> 8) & 0xFF),
           ((cpu_ip_raw >> 16) & 0xFF), ((cpu_ip_raw >> 24) & 0xFF),
           led_port);
}

/**
 * @brief Set LED RGBW value
 */
static void setLedRgbw(int led_index, uint8_t r, uint8_t g, uint8_t b, uint8_t w){
  if(led_index < 0 || led_index >= TOTAL_LEDS){
    return;
  }
  
  int offset = led_index * 4;
  led_packet.led_data[offset] = r;
  led_packet.led_data[offset + 1] = g;
  led_packet.led_data[offset + 2] = b;
  led_packet.led_data[offset + 3] = w;
}

/**
 * @brief Convert HSV to RGB
 */
static void hsvToRgb(float hue, uint8_t* r, uint8_t* g, uint8_t* b){
  float s = 1.0f;
  float v = 1.0f;
  
  float c = v * s;
  float x = c * (1.0f - fabs(fmod(hue / 60.0f, 2.0f) - 1.0f));
  float m = v - c;
  
  float r_prime, g_prime, b_prime;
  
  if(hue >= 0 && hue < 60){
    r_prime = c; g_prime = x; b_prime = 0;
  }else if(hue >= 60 && hue < 120){
    r_prime = x; g_prime = c; b_prime = 0;
  }else if(hue >= 120 && hue < 180){
    r_prime = 0; g_prime = c; b_prime = x;
  }else if(hue >= 180 && hue < 240){
    r_prime = 0; g_prime = x; b_prime = c;
  }else if(hue >= 240 && hue < 300){
    r_prime = x; g_prime = 0; b_prime = c;
  }else{
    r_prime = c; g_prime = 0; b_prime = x;
  }
  
  *r = (uint8_t)((r_prime + m) * 255.0f);
  *g = (uint8_t)((g_prime + m) * 255.0f);
  *b = (uint8_t)((b_prime + m) * 255.0f);
}

/**
 * @brief Create rainbow effect
 */
static void hueCycleEffect(float hue_offset){
  for(int i = 0; i < TOTAL_LEDS; i++){
    float led_hue = hue_offset + (i * 360.0f / TOTAL_LEDS);
    if(led_hue >= 360.0f){
      led_hue -= 360.0f;
    }
    
    uint8_t r, g, b;
    hsvToRgb(led_hue, &r, &g, &b);
    setLedRgbw(i, r, g, b, 0);
  }
}

/**
 * @brief Send LED packet via UDP
 */
static void sendLedPacket(uint8_t frame_counter){
  led_packet.magic = LED_PACKET_MAGIC;
  led_packet.frame_counter = frame_counter;
  led_packet.reserved = 0;
  led_packet.crc = calculateCRC8((uint8_t*)&led_packet, sizeof(LedDataPacket) - 1);
  
  int sent = sendto(udp_socket, &led_packet, sizeof(LedDataPacket), 0,
                    (struct sockaddr*)&cpu_addr, sizeof(cpu_addr));
  
  if(sent < 0){
    ESP_LOGW(TAG, "Failed to send UDP packet");
  }
}

/**
 * @brief Animation task - sends LED data at 60 FPS
 */
static void animationTask(void* pvParameters){
  float current_hue = 0.0f;
  int64_t last_frame_time_us = 0;
  uint8_t frame_counter = 1;
  uint32_t total_frames = 0;
  uint32_t frames_this_second = 0;
  int64_t last_fps_print_us = 0;
  
  ESP_LOGI(TAG, "Animation task started - 60 FPS WiFi transmission");
  
  last_frame_time_us = esp_timer_get_time();
  last_fps_print_us = last_frame_time_us;
  
  while(1){
    int64_t current_time_us = esp_timer_get_time();
    
    // Send at 60 FPS (16667 microseconds)
    if((current_time_us - last_frame_time_us) >= 16667 && wifi_connected){
      last_frame_time_us = current_time_us;
      
      // Update hue (0.6 degrees per frame = 360 degrees over 10 seconds)
      current_hue += 0.6f;
      if(current_hue >= 360.0f){
        current_hue -= 360.0f;
      }
      
      // Generate rainbow effect
      hueCycleEffect(current_hue);
      
      // Send packet
      sendLedPacket(frame_counter);
      
      total_frames++;
      frames_this_second++;
      
      // Increment frame counter (1-60)
      frame_counter++;
      if(frame_counter > 60){
        frame_counter = 1;
      }
      
      // Print FPS every second
      if((current_time_us - last_fps_print_us) >= 1000000){
        ESP_LOGI(TAG, ">>> GPU SEND FPS: %lu frames/sec | Total: %lu", 
                 frames_this_second, total_frames);
        frames_this_second = 0;
        last_fps_print_us = current_time_us;
      }
    }
    
    taskYIELD();
  }
}

extern "C" void app_main(){
  ESP_LOGI(TAG, "========================================");
  ESP_LOGI(TAG, "  WiFi LED Sender - GPU");
  ESP_LOGI(TAG, "========================================");
  
  // Initialize NVS (required for WiFi)
  esp_err_t ret = nvs_flash_init();
  if(ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND){
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);
  
  // Initialize UART
  initUart();
  
  // Receive WiFi config from CPU
  if(!receiveWiFiConfig()){
    ESP_LOGE(TAG, "Failed to receive WiFi config");
    while(1) vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
  
  // Connect to WiFi
  connectWiFi();
  
  // Wait for WiFi connection
  while(!wifi_connected){
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
  
  // Initialize UDP socket
  initUdpSocket();
  
  ESP_LOGI(TAG, "System ready - starting LED transmission");
  
  // Create animation task
  xTaskCreate(animationTask, "animation_task", 4096, NULL, 5, NULL);
}
