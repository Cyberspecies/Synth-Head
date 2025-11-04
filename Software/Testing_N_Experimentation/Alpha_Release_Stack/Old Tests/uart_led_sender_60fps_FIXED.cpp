/**
 * @file uart_led_sender_60fps_FIXED.cpp
 * @brief UART LED Sender Test - FIXED VERSION with High-Resolution Timing
 * 
 * This version uses esp_timer_get_time() for microsecond-precision timing
 * to achieve true 60 FPS instead of being limited by FreeRTOS tick rate.
 * 
 * Hardware Configuration:
 * - TX: GPIO 12 (sends LED data to receiver's RX GPIO 11)
 * - RX: GPIO 13 (receives button data from receiver's TX GPIO 12)
 * - Baud: 1,000,000 (1 Mbps)
 * 
 * LED Data Structure (197 bytes total):
 * - Left Fin:   13 LEDs × 4 bytes = bytes 0-51    (GPIO 18 on receiver)
 * - Right Fin:  13 LEDs × 4 bytes = bytes 52-103  (GPIO 38 on receiver)
 * - Tongue:     9 LEDs × 4 bytes  = bytes 104-139 (GPIO 8 on receiver)
 * - Scale:      14 LEDs × 4 bytes = bytes 140-195 (GPIO 37 on receiver)
 * - Frame Counter: 1 byte = byte 196 (cycles 1-60 for skip detection)
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"  // For high-resolution microsecond timer

static const char* TAG = "UART_LED_SENDER";

// UART Configuration
#define UART_PORT_NUM UART_NUM_1
#define UART_TX_PIN GPIO_NUM_12
#define UART_RX_PIN GPIO_NUM_13
#define UART_BAUD_RATE 921600  // 921.6 kbps - standard high-speed rate with better reliability
#define UART_BUF_SIZE 4096  // 4KB buffers for high throughput

// LED Configuration
#define LEFT_FIN_COUNT 13
#define RIGHT_FIN_COUNT 13
#define TONGUE_COUNT 9
#define SCALE_COUNT 14
#define TOTAL_LEDS (LEFT_FIN_COUNT + RIGHT_FIN_COUNT + TONGUE_COUNT + SCALE_COUNT)
#define BYTES_PER_LED 4
#define LED_DATA_BYTES (TOTAL_LEDS * BYTES_PER_LED) // 196 bytes

// Frame protocol with sync markers and CRC
#define SYNC_BYTE_1 0xAA
#define SYNC_BYTE_2 0x55
#define FRAME_COUNTER_BYTES 1
#define CRC_BYTES 1
#define SYNC_BYTES 2
#define TOTAL_FRAME_SIZE (SYNC_BYTES + LED_DATA_BYTES + FRAME_COUNTER_BYTES + CRC_BYTES) // 200 bytes

// Frame rate control - using microseconds for precision
#define TARGET_FPS 60
#define FRAME_INTERVAL_US (1000000 / TARGET_FPS)  // 16667 microseconds = 16.667ms

// LED frame buffer (full packet)
static uint8_t frame_packet[TOTAL_FRAME_SIZE];

// Button state
typedef struct{
  bool a;
  bool b;
  bool c;
  bool d;
} ButtonState;

static ButtonState button_state = {false, false, false, false};

/**
 * @brief Initialize UART peripheral
 */
static void initUart(){
  const uart_config_t uart_config = {
    .baud_rate = UART_BAUD_RATE,
    .data_bits = UART_DATA_8_BITS,
    .parity = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    .rx_flow_ctrl_thresh = 122,
    .source_clk = UART_SCLK_DEFAULT,
  };
  
  ESP_ERROR_CHECK(uart_param_config(UART_PORT_NUM, &uart_config));
  ESP_ERROR_CHECK(uart_set_pin(UART_PORT_NUM, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
  ESP_ERROR_CHECK(uart_driver_install(UART_PORT_NUM, UART_BUF_SIZE, UART_BUF_SIZE, 0, NULL, 0));
  
  ESP_LOGI(TAG, "UART initialized: TX=%d, RX=%d, Baud=%d", UART_TX_PIN, UART_RX_PIN, UART_BAUD_RATE);
}

/**
 * @brief Calculate CRC8 checksum
 */
static uint8_t calculateCRC8(const uint8_t* data, size_t length){
  uint8_t crc = 0x00;
  for(size_t i = 0; i < length; i++){
    crc ^= data[i];
    for(int j = 0; j < 8; j++){
      if(crc & 0x80){
        crc = (crc << 1) ^ 0x07; // CRC-8 polynomial
      }else{
        crc <<= 1;
      }
    }
  }
  return crc;
}

/**
 * @brief Set a single LED's RGBW value in the frame buffer
 */
static void setLedRgbw(int led_index, uint8_t r, uint8_t g, uint8_t b, uint8_t w){
  if(led_index < 0 || led_index >= TOTAL_LEDS){
    return;
  }
  
  // LED data starts at offset 2 (after sync bytes)
  int offset = SYNC_BYTES + (led_index * BYTES_PER_LED);
  frame_packet[offset] = r;
  frame_packet[offset + 1] = g;
  frame_packet[offset + 2] = b;
  frame_packet[offset + 3] = w;
}

/**
 * @brief Fill a section of LEDs with the same color
 */
static void fillLeds(int start_index, int count, uint8_t r, uint8_t g, uint8_t b, uint8_t w){
  for(int i = 0; i < count; i++){
    setLedRgbw(start_index + i, r, g, b, w);
  }
}

/**
 * @brief Clear all LEDs (set to black/off)
 */
static void clearAllLeds(){
  // Clear only LED data section (skip sync bytes)
  memset(&frame_packet[SYNC_BYTES], 0, LED_DATA_BYTES);
}

/**
 * @brief Send the LED frame with sync markers, frame counter, and CRC
 * Frame format (200 bytes):
 *   [0-1]     Sync bytes (0xAA, 0x55)
 *   [2-197]   LED data (196 bytes)
 *   [198]     Frame counter (1-60)
 *   [199]     CRC8 checksum
 */
static void sendFrame(uint8_t frame_counter){
  // Set sync markers
  frame_packet[0] = SYNC_BYTE_1;
  frame_packet[1] = SYNC_BYTE_2;
  
  // Set frame counter (at byte 198)
  frame_packet[SYNC_BYTES + LED_DATA_BYTES] = frame_counter;
  
  // Calculate CRC over sync + LED data + counter (199 bytes)
  uint8_t crc = calculateCRC8(frame_packet, TOTAL_FRAME_SIZE - CRC_BYTES);
  frame_packet[TOTAL_FRAME_SIZE - 1] = crc;
  
  // Send complete frame (200 bytes)
  int written = uart_write_bytes(UART_PORT_NUM, (const char*)frame_packet, TOTAL_FRAME_SIZE);
  
  if(written != TOTAL_FRAME_SIZE){
    ESP_LOGW(TAG, "Warning: Only wrote %d/%d bytes", written, TOTAL_FRAME_SIZE);
  }
}

/**
 * @brief Read button state from UART (4 bytes expected)
 */
static bool readButtons(){
  uint8_t button_data[4];
  int len = uart_read_bytes(UART_PORT_NUM, button_data, 4, 0); // Non-blocking
  
  if(len == 4){
    button_state.a = (button_data[0] == 0x01);
    button_state.b = (button_data[1] == 0x01);
    button_state.c = (button_data[2] == 0x01);
    button_state.d = (button_data[3] == 0x01);
    return true;
  }
  return false;
}

/**
 * @brief Print button state to console
 */
static void printButtonState(){
  ESP_LOGI(TAG, "Buttons: A=%d B=%d C=%d D=%d",
           button_state.a ? 1 : 0,
           button_state.b ? 1 : 0,
           button_state.c ? 1 : 0,
           button_state.d ? 1 : 0);
}

/**
 * @brief Convert HSV to RGB
 */
static void hsvToRgb(float hue, uint8_t* r, uint8_t* g, uint8_t* b){
  float s = 1.0f; // Full saturation
  float v = 1.0f; // Full value/brightness
  
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
 * @brief Create a rainbow effect across all LEDs
 */
static void hueCycleEffect(float hue_offset){
  for(int i = 0; i < TOTAL_LEDS; i++){
    // Each LED gets a different hue
    // Spread 360 degrees across all 49 LEDs
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
 * @brief Main animation task - FIXED with high-resolution timing
 */
static void animationTask(void* pvParameters){
  float current_hue = 0.0f;
  int64_t last_frame_time_us = 0;
  int64_t last_button_print_us = 0;
  uint8_t frame_counter = 1; // Frame counter cycles 1-60
  
  // FPS tracking
  uint32_t total_frames = 0;
  uint32_t frames_this_second = 0;
  int64_t last_fps_print_us = 0;
  
  ESP_LOGI(TAG, "Animation task started with HIGH-RESOLUTION TIMING");
  ESP_LOGI(TAG, "Frame Protocol: Sync markers + CRC8 validation");
  ESP_LOGI(TAG, "Sending %d bytes per frame:", TOTAL_FRAME_SIZE);
  ESP_LOGI(TAG, "  - Sync: 2 bytes (0xAA 0x55)");
  ESP_LOGI(TAG, "  - LED data: %d bytes", LED_DATA_BYTES);
  ESP_LOGI(TAG, "  - Frame counter: 1 byte (1-60)");
  ESP_LOGI(TAG, "  - CRC8: 1 byte");
  ESP_LOGI(TAG, "Target: 60 FPS (frame every %d microseconds)", FRAME_INTERVAL_US);
  ESP_LOGI(TAG, "Hue cycle: 0-360 degrees over 10 seconds");
  ESP_LOGI(TAG, "");
  
  // Clear all LEDs initially
  clearAllLeds();
  
  // Get initial time
  last_frame_time_us = esp_timer_get_time();
  last_fps_print_us = last_frame_time_us;
  
  while(1){
    // Use HIGH-RESOLUTION timer (microseconds)
    int64_t current_time_us = esp_timer_get_time();
    
    // Update animation at precise 60 FPS
    if((current_time_us - last_frame_time_us) >= FRAME_INTERVAL_US){
      last_frame_time_us = current_time_us;
      
      // Cycle hue from 0 to 360 degrees over 10 seconds
      // At 60 FPS, 10 seconds = 600 frames
      // Increment per frame = 360 / 600 = 0.6 degrees
      current_hue += 0.6f;
      if(current_hue >= 360.0f){
        current_hue -= 360.0f;
      }
      
      // Apply hue cycle effect to all LEDs
      hueCycleEffect(current_hue);
      
      // Send frame with frame counter (197 bytes total)
      sendFrame(frame_counter);
      
      // Track frames
      total_frames++;
      frames_this_second++;
      
      // Increment frame counter (1-60)
      frame_counter++;
      if(frame_counter > 60){
        frame_counter = 1;
      }
      
      // Print FPS every second
      if((current_time_us - last_fps_print_us) >= 1000000){ // 1 second in microseconds
        ESP_LOGI(TAG, ">>> GPU SEND FPS: %lu frames/sec | Total sent: %lu", 
                 frames_this_second, total_frames);
        frames_this_second = 0;
        last_fps_print_us = current_time_us;
      }
    }
    
    // Read button state
    if(readButtons()){
      // Print button state every 200ms when buttons are pressed
      if((current_time_us - last_button_print_us) >= 200000){ // 200ms in microseconds
        last_button_print_us = current_time_us;
        printButtonState();
      }
    }
    
    // Minimal delay to prevent watchdog timeout
    // Using taskYIELD() instead of vTaskDelay() for better responsiveness
    taskYIELD();
  }
}

extern "C" void app_main(){
  ESP_LOGI(TAG, "========================================");
  ESP_LOGI(TAG, "  UART LED Sender - 60 FPS FIXED");
  ESP_LOGI(TAG, "========================================");
  ESP_LOGI(TAG, "ARCOS Alpha Release Stack");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "Using esp_timer_get_time() for microsecond precision");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "TX Pin: GPIO %d", UART_TX_PIN);
  ESP_LOGI(TAG, "RX Pin: GPIO %d", UART_RX_PIN);
  ESP_LOGI(TAG, "Baud Rate: %d", UART_BAUD_RATE);
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "Total LEDs: %d", TOTAL_LEDS);
  ESP_LOGI(TAG, "Bytes per frame: %d (Sync:2 + LED:%d + Counter:1 + CRC:1)", 
           TOTAL_FRAME_SIZE, LED_DATA_BYTES);
  ESP_LOGI(TAG, "Target FPS: %d", TARGET_FPS);
  ESP_LOGI(TAG, "Frame interval: %d microseconds", FRAME_INTERVAL_US);
  ESP_LOGI(TAG, "");
  
  // Initialize UART
  initUart();
  
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "LED Sections:");
  ESP_LOGI(TAG, "  Left Fin:  %2d LEDs (bytes %3d-%3d)", 
           LEFT_FIN_COUNT, 0, LEFT_FIN_COUNT * BYTES_PER_LED - 1);
  ESP_LOGI(TAG, "  Right Fin: %2d LEDs (bytes %3d-%3d)", 
           RIGHT_FIN_COUNT, LEFT_FIN_COUNT * BYTES_PER_LED, 
           (LEFT_FIN_COUNT + RIGHT_FIN_COUNT) * BYTES_PER_LED - 1);
  ESP_LOGI(TAG, "  Tongue:    %2d LEDs (bytes %3d-%3d)", 
           TONGUE_COUNT, (LEFT_FIN_COUNT + RIGHT_FIN_COUNT) * BYTES_PER_LED, 
           (LEFT_FIN_COUNT + RIGHT_FIN_COUNT + TONGUE_COUNT) * BYTES_PER_LED - 1);
  ESP_LOGI(TAG, "  Scale:     %2d LEDs (bytes %3d-%3d)", 
           SCALE_COUNT, (LEFT_FIN_COUNT + RIGHT_FIN_COUNT + TONGUE_COUNT) * BYTES_PER_LED, 
           LED_DATA_BYTES - 1);
  ESP_LOGI(TAG, "  Frame Counter: byte %d (cycles 1-60)", LED_DATA_BYTES);
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "Starting LED animation - hue cycle (0-360° over 10 seconds)...");
  ESP_LOGI(TAG, "Press buttons on receiver to see button states");
  ESP_LOGI(TAG, "========================================");
  ESP_LOGI(TAG, "");
  
  // Create animation task with high priority
  xTaskCreate(animationTask, "animation_task", 4096, NULL, 5, NULL);
}
