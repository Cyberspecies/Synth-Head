/**
 * File:      OLEDTest.cpp
 * Project:   ARCOS Alpha Release Stack
 * Category:  Application
 * 
 * Description:
 *    Comprehensive OLED SH1107 display test with graphics primitives,
 *    text rendering, animations, and monochrome patterns using ARCOS library
 * 
 * Hardware:
 *    - ESP32-S3 (esp32s3usbotg)
 *    - OLED SH1107 128x128 monochrome display (I2C)
 * 
 * Framework: ESP-IDF
 * Date:      2025-11-10
 */

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <math.h>
#include "abstraction/hal.hpp"
#include "abstraction/drivers/components/OLED/driver_oled_sh1107.hpp"

static const char* TAG = "OLED_TEST";

using namespace arcos::abstraction;

// Global OLED display instance
DRIVER_OLED_SH1107 oled;

// Display configuration
constexpr int OLED_WIDTH = 128;
constexpr int OLED_HEIGHT = 128;

// Test mode tracking
int current_test = 0;
int animation_frame = 0;
unsigned long last_test_switch = 0;
const unsigned long TEST_DURATION_MS = 3000;  // 3 seconds per test

/**
 * @brief Initialize OLED SH1107 display with custom configuration
 */
bool initializeDisplay(){
  ESP_LOGI(TAG, "=== ARCOS OLED SH1107 Display Initialization ===");
  
  // Custom configuration
  OLEDConfig config;
  config.contrast = 0xCF;  // High contrast
  config.flip_horizontal = true;
  config.flip_vertical = true;
  
  if(!oled.initialize(config)){
    ESP_LOGE(TAG, "Failed to initialize OLED display!");
    return false;
  }
  
  // Flip display upside down (180 degree rotation)
  if(!oled.setUpsideDown(true)){
    ESP_LOGW(TAG, "Warning: Failed to set display upside down");
  } else {
    ESP_LOGI(TAG, "Display flipped upside down (180 degrees)");
  }
  
  ESP_LOGI(TAG, "SUCCESS: OLED display initialized!");
  ESP_LOGI(TAG, "Display size: %dx%d pixels", OLED_WIDTH, OLED_HEIGHT);
  ESP_LOGI(TAG, "Contrast: 0x%02X", config.contrast);
  
  return true;
}

// ============== BASIC PATTERNS ==============

/**
 * @brief Fill display with pattern
 */
void fillPattern(uint8_t pattern){
  oled.fillBuffer(pattern);
  oled.updateDisplay();
}

/**
 * @brief Clear display
 */
void clearDisplay(){
  oled.clearBuffer();
  oled.updateDisplay();
}

/**
 * @brief Draw border
 */
void drawBorder(){
  oled.clearBuffer();
  oled.drawRect(0, 0, OLED_WIDTH, OLED_HEIGHT, false, true);
  oled.updateDisplay();
}

/**
 * @brief Draw checkerboard pattern
 */
void drawCheckerboard(){
  oled.clearBuffer();
  
  int checker_size = 8;
  for(int y = 0; y < OLED_HEIGHT; y++){
    for(int x = 0; x < OLED_WIDTH; x++){
      bool is_on = ((x / checker_size) + (y / checker_size)) % 2 == 0;
      oled.setPixel(x, y, is_on);
    }
  }
  
  oled.updateDisplay();
}

/**
 * @brief Draw diagonal stripes
 */
void drawDiagonalStripes(){
  oled.clearBuffer();
  
  int stripe_width = 8;
  for(int y = 0; y < OLED_HEIGHT; y++){
    for(int x = 0; x < OLED_WIDTH; x++){
      bool is_on = ((x + y) / stripe_width) % 2 == 0;
      oled.setPixel(x, y, is_on);
    }
  }
  
  oled.updateDisplay();
}

/**
 * @brief Draw concentric circles
 */
void drawConcentricCircles(){
  oled.clearBuffer();
  
  int center_x = OLED_WIDTH / 2;
  int center_y = OLED_HEIGHT / 2;
  
  for(int r = 10; r < 64; r += 10){
    oled.drawCircle(center_x, center_y, r, false, true);
  }
  
  oled.updateDisplay();
}

// ============== GRAPHICS PRIMITIVES ==============

/**
 * @brief Test rectangles (filled and outline)
 */
void testRectangles(){
  oled.clearBuffer();
  
  // Outline rectangles
  oled.drawRect(10, 10, 40, 30, false, true);
  oled.drawRect(60, 10, 40, 30, false, true);
  
  // Filled rectangles
  oled.drawRect(10, 50, 40, 30, true, true);
  oled.drawRect(60, 50, 40, 30, true, true);
  
  // Mixed sizes
  oled.drawRect(35, 90, 58, 25, false, true);
  
  oled.updateDisplay();
}

/**
 * @brief Test circles (filled and outline)
 */
void testCircles(){
  oled.clearBuffer();
  
  // Small circles
  oled.drawCircle(20, 20, 15, false, true);
  oled.drawCircle(60, 20, 15, true, true);
  oled.drawCircle(100, 20, 15, false, true);
  
  // Medium circles
  oled.drawCircle(30, 65, 20, false, true);
  oled.drawCircle(90, 65, 20, true, true);
  
  // Large circle
  oled.drawCircle(64, 105, 15, true, true);
  
  oled.updateDisplay();
}

/**
 * @brief Test lines (various angles)
 */
void testLines(){
  oled.clearBuffer();
  
  int center_x = OLED_WIDTH / 2;
  int center_y = OLED_HEIGHT / 2;
  
  // Draw lines at different angles
  for(int angle = 0; angle < 360; angle += 30){
    float rad = angle * 3.14159f / 180.0f;
    int x = center_x + (int)(50 * cos(rad));
    int y = center_y + (int)(50 * sin(rad));
    oled.drawLine(center_x, center_y, x, y, true);
  }
  
  oled.updateDisplay();
}

/**
 * @brief Test crosshair and grid
 */
void testCrosshair(){
  oled.clearBuffer();
  
  // Draw border
  oled.drawRect(0, 0, OLED_WIDTH, OLED_HEIGHT, false, true);
  
  // Draw crosshair
  int mid_x = OLED_WIDTH / 2;
  int mid_y = OLED_HEIGHT / 2;
  oled.drawLine(mid_x, 0, mid_x, OLED_HEIGHT - 1, true);
  oled.drawLine(0, mid_y, OLED_WIDTH - 1, mid_y, true);
  
  // Draw center circle
  oled.drawCircle(mid_x, mid_y, 10, false, true);
  oled.drawCircle(mid_x, mid_y, 20, false, true);
  
  oled.updateDisplay();
}

// ============== TEXT RENDERING ==============

/**
 * @brief Test text rendering
 */
void testTextRendering(){
  oled.clearBuffer();
  
  oled.drawString(5, 0, "ARCOS Framework", true);
  oled.drawString(5, 10, "OLED SH1107", true);
  oled.drawString(5, 20, "128x128 Display", true);
  oled.drawString(5, 35, "ESP32-S3 Board", true);
  oled.drawString(5, 50, "I2C Interface", true);
  oled.drawString(5, 65, "Monochrome Test", true);
  oled.drawString(5, 85, "0123456789", true);
  oled.drawString(5, 100, "!@#$%^&*()_+-=", true);
  oled.drawString(5, 115, "Test Complete!", true);
  
  oled.updateDisplay();
}

/**
 * @brief Test large text with borders
 */
void testTextWithBorders(){
  oled.clearBuffer();
  
  // Draw border
  oled.drawRect(5, 5, 118, 118, false, true);
  
  // Draw text
  oled.drawString(15, 30, "ARCOS", true);
  oled.drawString(10, 50, "SYSTEM", true);
  oled.drawString(15, 70, "READY", true);
  
  // Draw decorative elements
  oled.drawCircle(64, 100, 8, true, true);
  
  oled.updateDisplay();
}

/**
 * @brief Test scrolling text effect
 */
void testScrollingText(int frame){
  oled.clearBuffer();
  
  const char* text = "Scrolling Text Demo - ARCOS Framework";
  int text_width, text_height;
  oled.getTextSize(text, &text_width, &text_height);
  
  int x_pos = OLED_WIDTH - (frame % (OLED_WIDTH + text_width));
  oled.drawString(x_pos, 50, text, true);
  
  // Draw reference lines
  oled.drawLine(0, 40, OLED_WIDTH - 1, 40, true);
  oled.drawLine(0, 65, OLED_WIDTH - 1, 65, true);
  
  oled.updateDisplay();
}

// ============== ANIMATIONS ==============

/**
 * @brief Animated bouncing ball
 */
void animateBouncingBall(int frame){
  oled.clearBuffer();
  
  // Calculate ball position
  int ball_radius = 8;
  int x = (frame % (OLED_WIDTH - 2 * ball_radius)) + ball_radius;
  int y_range = OLED_HEIGHT - 2 * ball_radius;
  int y_pos = (frame / 2) % y_range;
  
  // Create bounce effect
  if((frame / y_range) % 2 == 1){
    y_pos = y_range - y_pos;
  }
  int y = y_pos + ball_radius;
  
  // Draw border
  oled.drawRect(0, 0, OLED_WIDTH, OLED_HEIGHT, false, true);
  
  // Draw ball
  oled.drawCircle(x, y, ball_radius, true, true);
  
  // Draw trajectory line
  oled.drawLine(x, 0, x, OLED_HEIGHT - 1, true);
  
  oled.updateDisplay();
}

/**
 * @brief Animated rotating line
 */
void animateRotatingLine(int frame){
  oled.clearBuffer();
  
  int center_x = OLED_WIDTH / 2;
  int center_y = OLED_HEIGHT / 2;
  
  // Draw center circle
  oled.drawCircle(center_x, center_y, 5, true, true);
  
  // Draw rotating line
  float angle = (frame * 6.0f) * 3.14159f / 180.0f;
  int line_length = 50;
  int x = center_x + (int)(line_length * cos(angle));
  int y = center_y + (int)(line_length * sin(angle));
  oled.drawLine(center_x, center_y, x, y, true);
  
  // Draw outer circle
  oled.drawCircle(center_x, center_y, line_length, false, true);
  
  oled.updateDisplay();
}

/**
 * @brief Animated expanding circles
 */
void animateExpandingCircles(int frame){
  oled.clearBuffer();
  
  int center_x = OLED_WIDTH / 2;
  int center_y = OLED_HEIGHT / 2;
  int max_radius = 64;
  
  // Draw 3 expanding circles with phase offset
  for(int i = 0; i < 3; i++){
    int radius = ((frame + i * 20) % max_radius);
    if(radius > 0 && radius < max_radius){
      oled.drawCircle(center_x, center_y, radius, false, true);
    }
  }
  
  oled.updateDisplay();
}

/**
 * @brief Animated bar graph
 */
void animateBarGraph(int frame){
  oled.clearBuffer();
  
  oled.drawString(10, 5, "Bar Graph Demo", true);
  
  // Draw 6 animated bars
  for(int i = 0; i < 6; i++){
    int x = 10 + i * 20;
    int height = 10 + ((frame + i * 15) % 90);
    oled.drawRect(x, 118 - height, 15, height, true, true);
  }
  
  // Draw baseline
  oled.drawLine(5, 119, OLED_WIDTH - 5, 119, true);
  
  oled.updateDisplay();
}

/**
 * @brief Animated sine wave
 */
void animateSineWave(int frame){
  oled.clearBuffer();
  
  oled.drawString(10, 5, "Sine Wave", true);
  
  // Draw axes
  int mid_y = OLED_HEIGHT / 2;
  oled.drawLine(0, mid_y, OLED_WIDTH - 1, mid_y, true);
  
  // Draw sine wave
  for(int x = 0; x < OLED_WIDTH - 1; x++){
    float angle = (x + frame) * 3.14159f / 16.0f;
    int y1 = mid_y + (int)(20 * sin(angle));
    
    angle = (x + 1 + frame) * 3.14159f / 16.0f;
    int y2 = mid_y + (int)(20 * sin(angle));
    
    oled.drawLine(x, y1, x + 1, y2, true);
  }
  
  oled.updateDisplay();
}

// ============== SYSTEM INFO ==============

/**
 * @brief Display system information
 */
void displaySystemInfo(){
  oled.clearBuffer();
  
  oled.drawRect(0, 0, OLED_WIDTH, OLED_HEIGHT, false, true);
  
  oled.drawString(10, 10, "ARCOS System", true);
  oled.drawLine(10, 20, 118, 20, true);
  
  oled.drawString(10, 30, "Display: SH1107", true);
  oled.drawString(10, 45, "Size: 128x128", true);
  oled.drawString(10, 60, "Bus: I2C", true);
  oled.drawString(10, 75, "MCU: ESP32-S3", true);
  oled.drawString(10, 90, "RTOS: FreeRTOS", true);
  
  // Draw logo circle
  oled.drawCircle(64, 112, 8, true, true);
  
  oled.updateDisplay();
}

/**
 * @brief Run comprehensive OLED test suite
 */
void runOLEDTests(){
  uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
  
  if(current_time - last_test_switch >= TEST_DURATION_MS){
    current_test = (current_test + 1) % 17;
    last_test_switch = current_time;
    animation_frame = 0;
    
    switch(current_test){
      case 0:
        ESP_LOGI(TAG, "Test 1/17: System Information");
        displaySystemInfo();
        break;
        
      case 1:
        ESP_LOGI(TAG, "Test 2/17: Border");
        drawBorder();
        break;
        
      case 2:
        ESP_LOGI(TAG, "Test 3/17: Checkerboard");
        drawCheckerboard();
        break;
        
      case 3:
        ESP_LOGI(TAG, "Test 4/17: Diagonal Stripes");
        drawDiagonalStripes();
        break;
        
      case 4:
        ESP_LOGI(TAG, "Test 5/17: Concentric Circles");
        drawConcentricCircles();
        break;
        
      case 5:
        ESP_LOGI(TAG, "Test 6/17: Rectangles");
        testRectangles();
        break;
        
      case 6:
        ESP_LOGI(TAG, "Test 7/17: Circles");
        testCircles();
        break;
        
      case 7:
        ESP_LOGI(TAG, "Test 8/17: Lines");
        testLines();
        break;
        
      case 8:
        ESP_LOGI(TAG, "Test 9/17: Crosshair");
        testCrosshair();
        break;
        
      case 9:
        ESP_LOGI(TAG, "Test 10/17: Text Rendering");
        testTextRendering();
        break;
        
      case 10:
        ESP_LOGI(TAG, "Test 11/17: Text with Borders");
        testTextWithBorders();
        break;
        
      case 11:
        ESP_LOGI(TAG, "Test 12/17: Scrolling Text (Animated)");
        // Animation handled below
        break;
        
      case 12:
        ESP_LOGI(TAG, "Test 13/17: Bouncing Ball (Animated)");
        // Animation handled below
        break;
        
      case 13:
        ESP_LOGI(TAG, "Test 14/17: Rotating Line (Animated)");
        // Animation handled below
        break;
        
      case 14:
        ESP_LOGI(TAG, "Test 15/17: Expanding Circles (Animated)");
        // Animation handled below
        break;
        
      case 15:
        ESP_LOGI(TAG, "Test 16/17: Bar Graph (Animated)");
        // Animation handled below
        break;
        
      case 16:
        ESP_LOGI(TAG, "Test 17/17: Sine Wave (Animated)");
        // Animation handled below
        break;
        
      default:
        current_test = 0;
        break;
    }
  }
  
  // Handle animated tests
  switch(current_test){
    case 11:
      testScrollingText(animation_frame);
      animation_frame = (animation_frame + 2) % 512;
      break;
    case 12:
      animateBouncingBall(animation_frame);
      animation_frame = (animation_frame + 2) % 200;
      break;
    case 13:
      animateRotatingLine(animation_frame);
      animation_frame = (animation_frame + 1) % 360;
      break;
    case 14:
      animateExpandingCircles(animation_frame);
      animation_frame = (animation_frame + 2) % 128;
      break;
    case 15:
      animateBarGraph(animation_frame);
      animation_frame = (animation_frame + 2) % 90;
      break;
    case 16:
      animateSineWave(animation_frame);
      animation_frame = (animation_frame + 2) % 128;
      break;
  }
}

/**
 * @brief Main OLED test task
 */
void oledTestTask(void* pvParameters){
  last_test_switch = xTaskGetTickCount() * portTICK_PERIOD_MS;
  
  ESP_LOGI(TAG, "OLED test task running");
  
  // Main test loop
  while(1){
    runOLEDTests();
    vTaskDelay(pdMS_TO_TICKS(50));  // 50ms delay for smooth animation
  }
}

extern "C" void app_main(void){
  // 3 second delay for serial monitor to connect
  vTaskDelay(pdMS_TO_TICKS(3000));
  
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "=== ARCOS OLED SH1107 Comprehensive Test ===");
  ESP_LOGI(TAG, "");
  
  // Initialize I2C bus: bus_id=0, SDA=GPIO2, SCL=GPIO1, 400kHz
  ESP_LOGI(TAG, "Initializing I2C bus (SDA=GPIO1, SCL=GPIO2)...");
  HalResult i2c_result = ESP32S3_I2C::Initialize(0, 2, 1, 400000);
  if(i2c_result != HalResult::Success){
    ESP_LOGE(TAG, "FATAL: I2C initialization failed!");
    ESP_LOGE(TAG, "Check OLED wiring: SDA->GPIO2, SCL->GPIO1");
    return;
  }
  ESP_LOGI(TAG, "I2C bus initialized successfully (SDA=GPIO2, SCL=GPIO1)");
  
  // Scan I2C bus for devices
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "Scanning I2C bus...");
  bool device_found = false;
  uint8_t detected_address = 0;
  
  for(uint8_t addr = 0x08; addr < 0x78; addr++){
    HalResult result = ESP32S3_I2C::ProbeDevice(0, addr);
    
    if(result == HalResult::Success){
      ESP_LOGI(TAG, "  Device found at address 0x%02X", addr);
      device_found = true;
      detected_address = addr;
    }
  }
  
  if(!device_found){
    ESP_LOGE(TAG, "No I2C devices found!");
    ESP_LOGE(TAG, "Check wiring: SDA->GPIO1, SCL->GPIO2");
    return;
  }
  
  ESP_LOGI(TAG, "I2C scan complete. Using address 0x%02X", detected_address);
  ESP_LOGI(TAG, "");
  
  // Update OLED instance with detected address
  oled = DRIVER_OLED_SH1107(detected_address, 0);
  
  // Initialize OLED display
  ESP_LOGI(TAG, "Initializing OLED display at 0x%02X...", detected_address);
  if(!initializeDisplay()){
    ESP_LOGE(TAG, "FATAL: Display initialization failed!");
    ESP_LOGE(TAG, "System halted.");
    return;
  }
  
  ESP_LOGI(TAG, "Starting comprehensive test sequence...");
  ESP_LOGI(TAG, "17 tests will cycle automatically every 3 seconds.");
  ESP_LOGI(TAG, "");
  
  // Create OLED test task on core 1
  xTaskCreatePinnedToCore(
    oledTestTask,
    "oled_test",
    8192,  // Stack size
    NULL,
    5,     // Priority
    NULL,
    1      // Core 1
  );
  
  ESP_LOGI(TAG, "OLED test running. Tests will cycle automatically.");
}
