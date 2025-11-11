/**
 * File:      DualDisplayTest.cpp
 * Project:   ARCOS Alpha Release Stack
 * Category:  Application
 * 
 * Description:
 *    Comprehensive test demonstrating both HUB75 dual display and
 *    OLED SH1107 display working together using ARCOS library
 * 
 * Hardware:
 *    - ESP32-S3 (esp32s3usbotg)
 *    - Dual HUB75 LED Matrix Panels (64x32 each)
 *    - OLED SH1107 128x128 monochrome display (I2C)
 * 
 * Framework: ESP-IDF
 * Date:      2025-11-10
 */

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include "abstraction/drivers/components/HUB75/driver_hub75_simple.hpp"
#include "abstraction/drivers/components/OLED/driver_oled_sh1107.hpp"

static const char* TAG = "DUAL_DISPLAY_TEST";

using namespace arcos::abstraction::drivers;
using namespace arcos::abstraction;

// Global display instances
SimpleHUB75Display hub75_display;
DRIVER_OLED_SH1107 oled_display;

// Display configuration
constexpr int HUB75_WIDTH = 128;
constexpr int HUB75_HEIGHT = 32;
constexpr int OLED_WIDTH = 128;
constexpr int OLED_HEIGHT = 128;

// Test colors for HUB75 (using ARCOS RGB structure)
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
const unsigned long TEST_DURATION_MS = 4000;  // 4 seconds per test

/**
 * @brief Initialize HUB75 dual display with dual OE pins mode
 */
bool initializeHUB75(){
  ESP_LOGI(TAG, "=== Initializing HUB75 Dual Display ===");
  
  if(!hub75_display.begin(true)){
    ESP_LOGE(TAG, "Failed to initialize HUB75 display!");
    return false;
  }
  
  ESP_LOGI(TAG, "SUCCESS: HUB75 display initialized!");
  ESP_LOGI(TAG, "Display size: %dx%d pixels", hub75_display.getWidth(), hub75_display.getHeight());
  
  return true;
}

/**
 * @brief Initialize OLED SH1107 display with custom configuration
 */
bool initializeOLED(){
  ESP_LOGI(TAG, "=== Initializing OLED SH1107 Display ===");
  
  // Initialize I2C bus: bus_id=0, SDA=GPIO2, SCL=GPIO1, 400kHz
  ESP_LOGI(TAG, "Initializing I2C bus (SDA=GPIO2, SCL=GPIO1)...");
  HalResult i2c_result = ESP32S3_I2C::Initialize(0, 2, 1, 400000);
  if(i2c_result != HalResult::Success){
    ESP_LOGE(TAG, "FATAL: I2C initialization failed!");
    ESP_LOGE(TAG, "Check OLED wiring: SDA->GPIO2, SCL->GPIO1");
    return false;
  }
  ESP_LOGI(TAG, "I2C bus initialized successfully (SDA=GPIO2, SCL=GPIO1)");
  
  // Custom configuration with horizontal and vertical flip
  OLEDConfig config;
  config.contrast = 0xCF;  // High contrast
  config.flip_horizontal = true;
  config.flip_vertical = true;
  
  if(!oled_display.initialize(config)){
    ESP_LOGE(TAG, "Failed to initialize OLED display!");
    return false;
  }
  
  // Flip display upside down (180 degree rotation)
  if(!oled_display.setUpsideDown(true)){
    ESP_LOGW(TAG, "Warning: Failed to set display upside down");
  } else {
    ESP_LOGI(TAG, "Display flipped upside down (180 degrees)");
  }
  
  ESP_LOGI(TAG, "SUCCESS: OLED display initialized!");
  ESP_LOGI(TAG, "Display size: %dx%d pixels", OLED_WIDTH, OLED_HEIGHT);
  
  return true;
}

// ============== HUB75 TEST PATTERNS ==============

/**
 * @brief Fill HUB75 display with solid color
 */
void hub75FillColor(const RGB& color){
  hub75_display.fill(color);
  hub75_display.show();
}

/**
 * @brief Draw RGB color bars on HUB75
 */
void hub75DrawColorBars(){
  int width = hub75_display.getWidth();
  int height = hub75_display.getHeight();
  int bar_width = width / 7;
  
  const RGB colors[7] = {
    COLOR_WHITE, COLOR_YELLOW, COLOR_CYAN, COLOR_GREEN,
    COLOR_MAGENTA, COLOR_RED, COLOR_BLUE
  };
  
  for(int i = 0; i < 7; i++){
    int start_x = i * bar_width;
    int end_x = (i == 6) ? width : (i + 1) * bar_width;
    
    for(int x = start_x; x < end_x; x++){
      for(int y = 0; y < height; y++){
        hub75_display.setPixel(x, y, colors[i]);
      }
    }
  }
  hub75_display.show();
}

/**
 * @brief Draw horizontal gradient on HUB75
 */
void hub75DrawHorizontalGradient(){
  int width = hub75_display.getWidth();
  int height = hub75_display.getHeight();
  
  for(int x = 0; x < width; x++){
    uint8_t intensity = (x * 255) / width;
    RGB color(intensity, intensity, intensity);
    
    for(int y = 0; y < height; y++){
      hub75_display.setPixel(x, y, color);
    }
  }
  hub75_display.show();
}

/**
 * @brief Draw split screen test on HUB75
 */
void hub75DrawSplitScreen(){
  int width = hub75_display.getWidth();
  int height = hub75_display.getHeight();
  int mid = width / 2;
  
  // Left half - Red
  for(int x = 0; x < mid; x++){
    for(int y = 0; y < height; y++){
      hub75_display.setPixel(x, y, COLOR_RED);
    }
  }
  
  // Right half - Blue
  for(int x = mid; x < width; x++){
    for(int y = 0; y < height; y++){
      hub75_display.setPixel(x, y, COLOR_BLUE);
    }
  }
  hub75_display.show();
}

// ============== OLED TEST PATTERNS ==============

/**
 * @brief Clear OLED display
 */
void oledClear(){
  oled_display.clearBuffer();
  oled_display.updateDisplay();
}

/**
 * @brief Draw text on OLED
 */
void oledDrawText(const char* text, int x, int y){
  oled_display.drawString(x, y, text, true);
  oled_display.updateDisplay();
}

/**
 * @brief Draw graphics test pattern on OLED
 */
void oledDrawGraphicsTest(){
  oled_display.clearBuffer();
  
  // Draw rectangles
  oled_display.drawRect(10, 10, 40, 30, false, true);  // Outline
  oled_display.drawRect(60, 10, 40, 30, true, true);   // Filled
  
  // Draw circles
  oled_display.drawCircle(30, 70, 15, false, true);    // Outline
  oled_display.drawCircle(80, 70, 15, true, true);     // Filled
  
  // Draw lines
  oled_display.drawLine(5, 100, 50, 120, true);
  oled_display.drawLine(50, 100, 5, 120, true);
  
  oled_display.updateDisplay();
}

/**
 * @brief Draw checkerboard pattern on OLED
 */
void oledDrawCheckerboard(){
  oled_display.clearBuffer();
  
  int checker_size = 8;
  for(int y = 0; y < OLED_HEIGHT; y++){
    for(int x = 0; x < OLED_WIDTH; x++){
      bool is_on = ((x / checker_size) + (y / checker_size)) % 2 == 0;
      oled_display.setPixel(x, y, is_on);
    }
  }
  
  oled_display.updateDisplay();
}

/**
 * @brief Draw border and crosshair on OLED
 */
void oledDrawBorderCrosshair(){
  oled_display.clearBuffer();
  
  // Draw border
  oled_display.drawRect(0, 0, OLED_WIDTH, OLED_HEIGHT, false, true);
  
  // Draw crosshair
  int mid_x = OLED_WIDTH / 2;
  int mid_y = OLED_HEIGHT / 2;
  oled_display.drawLine(mid_x, 0, mid_x, OLED_HEIGHT - 1, true);
  oled_display.drawLine(0, mid_y, OLED_WIDTH - 1, mid_y, true);
  
  // Draw center circle
  oled_display.drawCircle(mid_x, mid_y, 10, false, true);
  
  oled_display.updateDisplay();
}

/**
 * @brief Display system info on OLED
 */
void oledDrawSystemInfo(){
  oled_display.clearBuffer();
  
  oled_display.drawString(0, 0, "ARCOS System", true);
  oled_display.drawString(0, 10, "HUB75: Active", true);
  oled_display.drawString(0, 20, "OLED: Active", true);
  oled_display.drawString(0, 30, "Framework: IDF", true);
  
  // Draw FreeRTOS logo-like pattern
  oled_display.drawRect(10, 50, 108, 70, false, true);
  oled_display.drawString(20, 60, "FreeRTOS", true);
  oled_display.drawCircle(64, 95, 20, true, true);
  
  oled_display.updateDisplay();
}

/**
 * @brief Draw animated bar graph on OLED
 */
void oledDrawBarGraph(int frame){
  oled_display.clearBuffer();
  
  oled_display.drawString(5, 0, "Bar Graph Test", true);
  
  // Draw 4 animated bars
  for(int i = 0; i < 4; i++){
    int x = 10 + i * 30;
    int height = 10 + ((frame + i * 15) % 90);
    oled_display.drawRect(x, 118 - height, 20, height, true, true);
  }
  
  oled_display.updateDisplay();
}

/**
 * @brief Run synchronized display tests
 */
void runDisplayTests(){
  static int animation_frame = 0;
  uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
  
  if(current_time - last_test_switch >= TEST_DURATION_MS){
    current_test = (current_test + 1) % 8;
    last_test_switch = current_time;
    animation_frame = 0;
    
    switch(current_test){
      case 0:
        ESP_LOGI(TAG, "Test 1/8: Solid Colors + System Info");
        hub75FillColor(COLOR_RED);
        oledDrawSystemInfo();
        break;
        
      case 1:
        ESP_LOGI(TAG, "Test 2/8: Color Bars + Text");
        hub75DrawColorBars();
        oledClear();
        oledDrawText("Color Bars", 20, 30);
        oledDrawText("Test Active", 15, 60);
        break;
        
      case 2:
        ESP_LOGI(TAG, "Test 3/8: Gradient + Graphics");
        hub75DrawHorizontalGradient();
        oledDrawGraphicsTest();
        break;
        
      case 3:
        ESP_LOGI(TAG, "Test 4/8: Split Screen + Checkerboard");
        hub75DrawSplitScreen();
        oledDrawCheckerboard();
        break;
        
      case 4:
        ESP_LOGI(TAG, "Test 5/8: Green + Border Crosshair");
        hub75FillColor(COLOR_GREEN);
        oledDrawBorderCrosshair();
        break;
        
      case 5:
        ESP_LOGI(TAG, "Test 6/8: Cyan + Bar Graph (Animated)");
        hub75FillColor(COLOR_CYAN);
        // Bar graph will animate in next case
        break;
        
      case 6:
        ESP_LOGI(TAG, "Test 7/8: Magenta + Bar Graph (Animated)");
        hub75FillColor(COLOR_MAGENTA);
        // Bar graph will animate in next case
        break;
        
      case 7:
        ESP_LOGI(TAG, "Test 8/8: White + System Info");
        hub75FillColor(COLOR_WHITE);
        oledDrawSystemInfo();
        break;
    }
  }
  
  // Animate bar graph during test 5 and 6
  if(current_test == 5 || current_test == 6){
    oledDrawBarGraph(animation_frame);
    animation_frame = (animation_frame + 2) % 90;
  }
}

/**
 * @brief Main display test task
 */
void displayTestTask(void* pvParameters){
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "=== ARCOS Dual Display Test ===");
  ESP_LOGI(TAG, "HUB75 + OLED SH1107 Demonstration");
  ESP_LOGI(TAG, "");
  
  // Initialize HUB75 display
  if(!initializeHUB75()){
    ESP_LOGE(TAG, "FATAL: HUB75 initialization failed!");
    ESP_LOGE(TAG, "System halted.");
    vTaskDelete(NULL);
    return;
  }
  
  // Initialize OLED display
  if(!initializeOLED()){
    ESP_LOGE(TAG, "FATAL: OLED initialization failed!");
    ESP_LOGE(TAG, "System halted.");
    vTaskDelete(NULL);
    return;
  }
  
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "Starting synchronized test sequence...");
  ESP_LOGI(TAG, "Tests will cycle automatically every 4 seconds.");
  ESP_LOGI(TAG, "");
  
  last_test_switch = xTaskGetTickCount() * portTICK_PERIOD_MS;
  
  // Main test loop
  while(1){
    runDisplayTests();
    vTaskDelay(pdMS_TO_TICKS(50));  // 50ms delay for smooth animation
  }
}

extern "C" void app_main(void){
  // Create display test task
  xTaskCreate(
    displayTestTask,
    "dual_display_test",
    8192,  // Stack size
    NULL,
    5,     // Priority
    NULL
  );
}
