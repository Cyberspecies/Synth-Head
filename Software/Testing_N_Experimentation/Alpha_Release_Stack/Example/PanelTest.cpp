/**
 * File:      main.cpp
 * Project:   ARCOS Alpha Release Stack
 * Category:  Application
 * 
 * Description:
 *    HUB75 dual display initialization with dual OE pins mode
 *    and color test patterns using SimpleHUB75Display
 * 
 * Hardware:
 *    - ESP32-S3 (esp32s3usbotg)
 *    - Dual HUB75 LED Matrix Panels (64x32 each)
 *    - Dual OE mode for independent panel control
 * 
 * Framework: ESP-IDF
 * Date:      2025-11-04
 */

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include "abstraction/drivers/components/HUB75/driver_hub75_simple.hpp"

static const char* TAG = "HUB75_TEST";

using namespace arcos::abstraction::drivers;

// Global display instance
SimpleHUB75Display display;

// Display configuration
constexpr int PANEL_WIDTH = 64;
constexpr int PANEL_HEIGHT = 32;
constexpr int NUM_PANELS = 2;  // Dual display setup

// Test colors (using ARCOS RGB structure)
const RGB COLOR_RED = {255, 0, 0};
const RGB COLOR_GREEN = {0, 255, 0};
const RGB COLOR_BLUE = {0, 0, 255};
const RGB COLOR_CYAN = {0, 255, 255};
const RGB COLOR_MAGENTA = {255, 0, 255};
const RGB COLOR_YELLOW = {255, 255, 0};
const RGB COLOR_WHITE = {255, 255, 255};
const RGB COLOR_BLACK = {0, 0, 0};

// Test mode tracking
int current_test = 0;
unsigned long last_test_switch = 0;
const unsigned long TEST_DURATION_MS = 3000;  // 3 seconds per test

/**
 * @brief Initialize HUB75 dual display with dual OE pins mode
 */
bool initializeDisplay(){
  ESP_LOGI(TAG, "=== ARCOS HUB75 Dual Display Initialization ===");
  
  // Initialize with dual OE mode (true = dual display, 128x32 total)
  // Uses sensible defaults with automatic configuration
  if(!display.begin(true)){
    ESP_LOGE(TAG, "Failed to initialize HUB75 display!");
    return false;
  }
  
  ESP_LOGI(TAG, "SUCCESS: HUB75 dual display initialized!");
  ESP_LOGI(TAG, "Display size: %dx%d pixels", display.getWidth(), display.getHeight());
  ESP_LOGI(TAG, "Dual OE mode: ENABLED");
  ESP_LOGI(TAG, "Gamma correction: ENABLED (2.2)");
  
  return true;
}

/**
 * @brief Fill entire display with a solid color
 */
void fillDisplay(const RGB& color){
  display.fill(color);
  display.show();
}

/**
 * @brief Draw vertical gradient test pattern
 */
void drawVerticalGradient(){
  int width = display.getWidth();
  int height = display.getHeight();
  
  for(int y = 0; y < height; y++){
    uint8_t intensity = (y * 255) / height;
    RGB color(intensity, intensity, intensity);
    
    for(int x = 0; x < width; x++){
      display.setPixel(x, y, color);
    }
  }
  display.show();
}

/**
 * @brief Draw horizontal gradient test pattern
 */
void drawHorizontalGradient(){
  int width = display.getWidth();
  int height = display.getHeight();
  
  for(int x = 0; x < width; x++){
    uint8_t intensity = (x * 255) / width;
    RGB color(intensity, intensity, intensity);
    
    for(int y = 0; y < height; y++){
      display.setPixel(x, y, color);
    }
  }
  display.show();
}

/**
 * @brief Draw RGB color bars test pattern
 */
void drawColorBars(){
  int width = display.getWidth();
  int height = display.getHeight();
  int bar_width = width / 7;
  
  const RGB colors[7] = {
    COLOR_WHITE,
    COLOR_YELLOW,
    COLOR_CYAN,
    COLOR_GREEN,
    COLOR_MAGENTA,
    COLOR_RED,
    COLOR_BLUE
  };
  
  for(int i = 0; i < 7; i++){
    int start_x = i * bar_width;
    int end_x = (i == 6) ? width : (i + 1) * bar_width;
    
    for(int x = start_x; x < end_x; x++){
      for(int y = 0; y < height; y++){
        display.setPixel(x, y, colors[i]);
      }
    }
  }
  display.show();
}

/**
 * @brief Draw split screen test (left panel vs right panel)
 */
void drawSplitScreen(){
  int width = display.getWidth();
  int height = display.getHeight();
  int mid = width / 2;
  
  // Left half - Red
  for(int x = 0; x < mid; x++){
    for(int y = 0; y < height; y++){
      display.setPixel(x, y, COLOR_RED);
    }
  }
  
  // Right half - Blue
  for(int x = mid; x < width; x++){
    for(int y = 0; y < height; y++){
      display.setPixel(x, y, COLOR_BLUE);
    }
  }
  display.show();
}

/**
 * @brief Draw checkerboard pattern
 */
void drawCheckerboard(){
  int width = display.getWidth();
  int height = display.getHeight();
  int checker_size = 8;
  
  for(int y = 0; y < height; y++){
    for(int x = 0; x < width; x++){
      bool is_white = ((x / checker_size) + (y / checker_size)) % 2 == 0;
      RGB color = is_white ? COLOR_WHITE : COLOR_BLACK;
      display.setPixel(x, y, color);
    }
  }
  display.show();
}

/**
 * @brief Run color test suite
 */
void runColorTests(){
  uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
  
  if(current_time - last_test_switch >= TEST_DURATION_MS){
    current_test = (current_test + 1) % 9;
    last_test_switch = current_time;
    
    switch(current_test){
      case 0:
        ESP_LOGI(TAG, "Test 1/9: Solid RED");
        fillDisplay(COLOR_RED);
        break;
        
      case 1:
        ESP_LOGI(TAG, "Test 2/9: Solid GREEN");
        fillDisplay(COLOR_GREEN);
        break;
        
      case 2:
        ESP_LOGI(TAG, "Test 3/9: Solid BLUE");
        fillDisplay(COLOR_BLUE);
        break;
        
      case 3:
        ESP_LOGI(TAG, "Test 4/9: Solid WHITE");
        fillDisplay(COLOR_WHITE);
        break;
        
      case 4:
        ESP_LOGI(TAG, "Test 5/9: Vertical Gradient");
        drawVerticalGradient();
        break;
        
      case 5:
        ESP_LOGI(TAG, "Test 6/9: Horizontal Gradient");
        drawHorizontalGradient();
        break;
        
      case 6:
        ESP_LOGI(TAG, "Test 7/9: RGB Color Bars");
        drawColorBars();
        break;
        
      case 7:
        ESP_LOGI(TAG, "Test 8/9: Split Screen (Red/Blue)");
        drawSplitScreen();
        break;
        
      case 8:
        ESP_LOGI(TAG, "Test 9/9: Checkerboard Pattern");
        drawCheckerboard();
        break;
        
      default:
        current_test = 0;
        break;
    }
  }
}

/**
 * @brief Main display test task
 */
void displayTestTask(void* pvParameters){
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "=== ARCOS HUB75 Dual Display Test ===");
  ESP_LOGI(TAG, "Initializing dual HUB75 displays with dual OE pins mode...");
  ESP_LOGI(TAG, "");
  
  // Initialize display
  if(!initializeDisplay()){
    ESP_LOGE(TAG, "FATAL: Display initialization failed!");
    ESP_LOGE(TAG, "System halted.");
    vTaskDelete(NULL);
    return;
  }
  
  ESP_LOGI(TAG, "Starting color test sequence...");
  ESP_LOGI(TAG, "Tests will cycle automatically every 3 seconds.");
  ESP_LOGI(TAG, "");
  
  last_test_switch = xTaskGetTickCount() * portTICK_PERIOD_MS;
  
  // Main test loop
  while(1){
    runColorTests();
    vTaskDelay(pdMS_TO_TICKS(10));  // 10ms delay
  }
}

extern "C" void app_main(void){
  // Create display test task
  xTaskCreate(
    displayTestTask,
    "display_test",
    8192,  // Stack size
    NULL,
    5,     // Priority
    NULL
  );
}
