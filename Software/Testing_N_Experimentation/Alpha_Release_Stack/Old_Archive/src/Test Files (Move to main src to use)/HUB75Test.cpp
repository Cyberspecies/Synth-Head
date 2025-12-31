/**
 * File:      HUB75Test.cpp
 * Project:   ARCOS Alpha Release Stack
 * Category:  Application
 * 
 * Description:
 *    Comprehensive HUB75 dual display test with extensive color patterns,
 *    gradients, animations, and pixel accuracy tests using ARCOS library
 * 
 * Hardware:
 *    - ESP32-S3 (esp32s3usbotg)
 *    - Dual HUB75 LED Matrix Panels (64x32 each)
 *    - Dual OE mode for independent panel control
 * 
 * Framework: ESP-IDF
 * Date:      2025-11-10
 */

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <math.h>
#include "abstraction/drivers/components/HUB75/driver_hub75_simple.hpp"

static const char* TAG = "HUB75_TEST";

using namespace arcos::abstraction::drivers;

// Global display instance
SimpleHUB75Display display;

// Display configuration
constexpr int PANEL_WIDTH = 64;
constexpr int PANEL_HEIGHT = 32;
constexpr int TOTAL_WIDTH = 128;
constexpr int TOTAL_HEIGHT = 32;

// Test colors (using ARCOS RGB structure)
const RGB COLOR_RED = {255, 0, 0};
const RGB COLOR_GREEN = {0, 255, 0};
const RGB COLOR_BLUE = {0, 0, 255};
const RGB COLOR_CYAN = {0, 255, 255};
const RGB COLOR_MAGENTA = {255, 0, 255};
const RGB COLOR_YELLOW = {255, 255, 0};
const RGB COLOR_WHITE = {255, 255, 255};
const RGB COLOR_BLACK = {0, 0, 0};
const RGB COLOR_ORANGE = {255, 165, 0};
const RGB COLOR_PURPLE = {128, 0, 128};
const RGB COLOR_PINK = {255, 192, 203};

// Test mode tracking
int current_test = 0;
int animation_frame = 0;
unsigned long last_test_switch = 0;
const unsigned long TEST_DURATION_MS = 3000;  // 3 seconds per test

/**
 * @brief Initialize HUB75 dual display with dual OE pins mode
 */
bool initializeDisplay(){
  ESP_LOGI(TAG, "=== ARCOS HUB75 Dual Display Initialization ===");
  
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
 * @brief Draw RGB color gradient
 */
void drawRGBGradient(){
  int width = display.getWidth();
  int height = display.getHeight();
  
  for(int x = 0; x < width; x++){
    uint8_t r = (x * 255) / width;
    uint8_t g = ((width - x) * 255) / width;
    uint8_t b = 128;
    RGB color(r, g, b);
    
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
 * @brief Draw quad split (4 quadrants with different colors)
 */
void drawQuadSplit(){
  int width = display.getWidth();
  int height = display.getHeight();
  int mid_x = width / 2;
  int mid_y = height / 2;
  
  const RGB colors[4] = {COLOR_RED, COLOR_GREEN, COLOR_BLUE, COLOR_YELLOW};
  
  // Top-left quadrant
  for(int x = 0; x < mid_x; x++){
    for(int y = 0; y < mid_y; y++){
      display.setPixel(x, y, colors[0]);
    }
  }
  
  // Top-right quadrant
  for(int x = mid_x; x < width; x++){
    for(int y = 0; y < mid_y; y++){
      display.setPixel(x, y, colors[1]);
    }
  }
  
  // Bottom-left quadrant
  for(int x = 0; x < mid_x; x++){
    for(int y = mid_y; y < height; y++){
      display.setPixel(x, y, colors[2]);
    }
  }
  
  // Bottom-right quadrant
  for(int x = mid_x; x < width; x++){
    for(int y = mid_y; y < height; y++){
      display.setPixel(x, y, colors[3]);
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
 * @brief Draw diagonal stripes pattern
 */
void drawDiagonalStripes(){
  int width = display.getWidth();
  int height = display.getHeight();
  int stripe_width = 8;
  
  for(int y = 0; y < height; y++){
    for(int x = 0; x < width; x++){
      bool is_colored = ((x + y) / stripe_width) % 2 == 0;
      RGB color = is_colored ? COLOR_CYAN : COLOR_MAGENTA;
      display.setPixel(x, y, color);
    }
  }
  display.show();
}

/**
 * @brief Draw pixel test pattern (every nth pixel)
 */
void drawPixelTest(){
  int width = display.getWidth();
  int height = display.getHeight();
  
  display.fill(COLOR_BLACK);
  
  // Draw a grid of pixels
  for(int y = 0; y < height; y += 4){
    for(int x = 0; x < width; x += 4){
      display.setPixel(x, y, COLOR_WHITE);
    }
  }
  display.show();
}

/**
 * @brief Draw border test
 */
void drawBorder(){
  int width = display.getWidth();
  int height = display.getHeight();
  
  display.fill(COLOR_BLACK);
  
  // Top and bottom borders
  for(int x = 0; x < width; x++){
    display.setPixel(x, 0, COLOR_RED);
    display.setPixel(x, height - 1, COLOR_RED);
  }
  
  // Left and right borders
  for(int y = 0; y < height; y++){
    display.setPixel(0, y, COLOR_GREEN);
    display.setPixel(width - 1, y, COLOR_BLUE);
  }
  
  display.show();
}

/**
 * @brief Draw animated rainbow wave
 */
void drawRainbowWave(int frame){
  int width = display.getWidth();
  int height = display.getHeight();
  
  for(int x = 0; x < width; x++){
    for(int y = 0; y < height; y++){
      // Create rainbow effect with animation
      float hue = (float)(x + frame) / (float)width * 360.0f;
      
      // Convert HSV to RGB (simplified)
      float h_prime = hue / 60.0f;
      int h_int = (int)h_prime % 6;
      float f = h_prime - (int)h_prime;
      
      uint8_t r = 0, g = 0, b = 0;
      
      switch(h_int){
        case 0: r = 255; g = (uint8_t)(255 * f); b = 0; break;
        case 1: r = (uint8_t)(255 * (1.0f - f)); g = 255; b = 0; break;
        case 2: r = 0; g = 255; b = (uint8_t)(255 * f); break;
        case 3: r = 0; g = (uint8_t)(255 * (1.0f - f)); b = 255; break;
        case 4: r = (uint8_t)(255 * f); g = 0; b = 255; break;
        case 5: r = 255; g = 0; b = (uint8_t)(255 * (1.0f - f)); break;
      }
      
      RGB color(r, g, b);
      display.setPixel(x, y, color);
    }
  }
  display.show();
}

/**
 * @brief Draw moving bouncing box animation
 */
void drawBouncingBox(int frame){
  int width = display.getWidth();
  int height = display.getHeight();
  
  display.fill(COLOR_BLACK);
  
  // Calculate box position with bounce
  int box_size = 16;
  int x = (frame % (width - box_size));
  int y = ((frame / 2) % (height - box_size));
  
  // Draw box
  for(int i = 0; i < box_size; i++){
    for(int j = 0; j < box_size; j++){
      if(x + i < width && y + j < height){
        display.setPixel(x + i, y + j, COLOR_YELLOW);
      }
    }
  }
  
  display.show();
}

/**
 * @brief Run comprehensive HUB75 test suite
 */
void runColorTests(){
  uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
  
  if(current_time - last_test_switch >= TEST_DURATION_MS){
    current_test = (current_test + 1) % 15;
    last_test_switch = current_time;
    animation_frame = 0;
    
    switch(current_test){
      case 0:
        ESP_LOGI(TAG, "Test 1/15: Solid RED");
        fillDisplay(COLOR_RED);
        break;
        
      case 1:
        ESP_LOGI(TAG, "Test 2/15: Solid GREEN");
        fillDisplay(COLOR_GREEN);
        break;
        
      case 2:
        ESP_LOGI(TAG, "Test 3/15: Solid BLUE");
        fillDisplay(COLOR_BLUE);
        break;
        
      case 3:
        ESP_LOGI(TAG, "Test 4/15: Solid WHITE");
        fillDisplay(COLOR_WHITE);
        break;
        
      case 4:
        ESP_LOGI(TAG, "Test 5/15: Vertical Gradient");
        drawVerticalGradient();
        break;
        
      case 5:
        ESP_LOGI(TAG, "Test 6/15: Horizontal Gradient");
        drawHorizontalGradient();
        break;
        
      case 6:
        ESP_LOGI(TAG, "Test 7/15: RGB Gradient");
        drawRGBGradient();
        break;
        
      case 7:
        ESP_LOGI(TAG, "Test 8/15: RGB Color Bars");
        drawColorBars();
        break;
        
      case 8:
        ESP_LOGI(TAG, "Test 9/15: Split Screen (Red/Blue)");
        drawSplitScreen();
        break;
        
      case 9:
        ESP_LOGI(TAG, "Test 10/15: Quad Split");
        drawQuadSplit();
        break;
        
      case 10:
        ESP_LOGI(TAG, "Test 11/15: Checkerboard Pattern");
        drawCheckerboard();
        break;
        
      case 11:
        ESP_LOGI(TAG, "Test 12/15: Diagonal Stripes");
        drawDiagonalStripes();
        break;
        
      case 12:
        ESP_LOGI(TAG, "Test 13/15: Pixel Test Grid");
        drawPixelTest();
        break;
        
      case 13:
        ESP_LOGI(TAG, "Test 14/15: Border Test");
        drawBorder();
        break;
        
      case 14:
        ESP_LOGI(TAG, "Test 15/15: Rainbow Wave (Animated)");
        // Animation handled in loop below
        break;
        
      default:
        current_test = 0;
        break;
    }
  }
  
  // Handle animated tests
  if(current_test == 14){
    drawRainbowWave(animation_frame);
    animation_frame = (animation_frame + 2) % 128;
  }
}

/**
 * @brief Main display test task
 */
void displayTestTask(void* pvParameters){
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "=== ARCOS HUB75 Comprehensive Test ===");
  ESP_LOGI(TAG, "Initializing dual HUB75 displays...");
  ESP_LOGI(TAG, "");
  
  // Initialize display
  if(!initializeDisplay()){
    ESP_LOGE(TAG, "FATAL: Display initialization failed!");
    ESP_LOGE(TAG, "System halted.");
    vTaskDelete(NULL);
    return;
  }
  
  ESP_LOGI(TAG, "Starting comprehensive test sequence...");
  ESP_LOGI(TAG, "15 tests will cycle automatically every 3 seconds.");
  ESP_LOGI(TAG, "");
  
  last_test_switch = xTaskGetTickCount() * portTICK_PERIOD_MS;
  
  // Main test loop
  while(1){
    runColorTests();
    vTaskDelay(pdMS_TO_TICKS(20));  // 20ms delay for smooth animation
  }
}

extern "C" void app_main(void){
  // Create display test task
  xTaskCreate(
    displayTestTask,
    "hub75_test",
    8192,  // Stack size
    NULL,
    5,     // Priority
    NULL
  );
}
