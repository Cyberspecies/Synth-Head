/*****************************************************************
 * File:      uart_gpu_sensor_display.cpp
 * Category:  communication/applications
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    GPU-side bidirectional application:
 *    - Receives sensor data from CPU and displays on OLED with pages
 *    - Displays HUB75 LED matrix visualizations
 *    - Generates LED animations and sends RGBW data to CPU at 60Hz
 * 
 * Hardware:
 *    - ESP32-S3 (GPU)
 *    - OLED SH1107 128x128 display (I2C: SDA=GPIO2, SCL=GPIO1)
 *    - HUB75 Dual LED Matrix: TWO separate 64x32 panels (dual OE pins)
 *      * Left Panel: X=0-63, Y=0-31
 *      * Right Panel: X=64-127, Y=0-31
 *    - UART to CPU: RX=GPIO13, TX=GPIO12
 * 
 * Display Layout:
 *    - Page 0: IMU Data (Accelerometer, Gyroscope, Magnetometer)
 *    - Page 1: Environmental Data (Temperature, Humidity, Pressure)
 *    - Page 2: GPS Data (Position, Satellites, Time)
 *    - Page 3: Microphone Data with waveform graph
 *    - Page 4: System Info (FPS, Button states, LED animation)
 * 
 * Controls:
 *    - Button A: Previous page
 *    - Button B: Next page
 *****************************************************************/

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <cmath>
#include "Drivers/UART Comms/GpuUartBidirectional.hpp"
#include "abstraction/drivers/components/OLED/driver_oled_sh1107.hpp"
#include "Manager/DisplayManagerHUB75.hpp"
#include "Manager/DisplayManagerHUB75.impl.hpp"

using namespace arcos::communication;
using namespace arcos::abstraction;
using namespace arcos::abstraction::drivers;

static const char* TAG = "GPU_BIDIRECTIONAL";

// ============== Display Configuration ==============
constexpr int DISPLAY_WIDTH = 128;
constexpr int DISPLAY_HEIGHT = 128;
constexpr int LINE_HEIGHT = 10;
constexpr int LINES_PER_PAGE = 12;
constexpr int TOTAL_PAGES = 5;

// ============== HUB75 Configuration ==============
constexpr int HUB75_WIDTH = 128;
constexpr int HUB75_HEIGHT = 32;

// ============== Microphone Graph Configuration ==============
constexpr float MIC_GRAPH_DURATION_SEC = 1.5f;  // Graph width in seconds (adjustable)
constexpr int MIC_GRAPH_WIDTH = 120;            // Graph width in pixels
constexpr int MIC_GRAPH_HEIGHT = 40;            // Graph height in pixels
constexpr int MIC_GRAPH_SAMPLES = MIC_GRAPH_WIDTH;  // One sample per pixel
constexpr float MIC_DB_MIN = -60.0f;            // Minimum dB for graph
constexpr float MIC_DB_MAX = 0.0f;              // Maximum dB for graph

// ============== LED Configuration ==============
constexpr uint32_t LED_FPS = 60;
constexpr uint32_t LED_FRAME_INTERVAL_US = 1000000 / LED_FPS;

// ============== Global Instances ==============
GpuUartBidirectional uart_comm;
DRIVER_OLED_SH1107 oled_display;
DisplayManagerHUB75 hub75_display;

// ============== Shared Data (Protected by Mutexes) ==============
SemaphoreHandle_t sensor_data_mutex;
SensorDataPayload current_sensor_data;
bool data_received = false;
uint32_t last_data_time = 0;

LedDataPayload led_data;
uint8_t current_animation = 0;
uint32_t animation_time_ms = 0;

// ============== Display State ==============
int current_page = 0;
bool button_a_prev = false;
bool button_b_prev = false;

// ============== Microphone Graph State ==============
float mic_history[MIC_GRAPH_SAMPLES];          // Circular buffer for dB levels
int mic_history_index = 0;                     // Current write position
uint32_t last_mic_sample_time = 0;             // Last time we added a sample
uint32_t mic_sample_interval_ms = 0;           // Calculated from graph duration

// ============== Statistics ==============
struct Stats{
  uint32_t sensor_frames_received = 0;
  uint32_t led_frames_sent = 0;
  uint32_t display_updates = 0;
  uint32_t last_report_time = 0;
  uint32_t sensor_fps = 0;
  uint32_t led_fps = 0;
};
Stats stats;

/**
 * @brief Initialize OLED display
 */
bool initializeOLED(){
  ESP_LOGI(TAG, "Initializing OLED SH1107 display...");
  
  // Initialize I2C bus: bus_id=0, SDA=GPIO2, SCL=GPIO1, 400kHz
  HalResult i2c_result = ESP32S3_I2C::Initialize(0, 2, 1, 400000);
  if(i2c_result != HalResult::Success){
    ESP_LOGE(TAG, "Failed to initialize I2C bus!");
    return false;
  }
  
  // Initialize OLED with custom configuration
  OLEDConfig config;
  config.contrast = 0xCF;
  config.flip_horizontal = true;
  config.flip_vertical = true;
  
  if(!oled_display.initialize(config)){
    ESP_LOGE(TAG, "Failed to initialize OLED display!");
    return false;
  }
  
  // Flip display upside down
  if(!oled_display.setUpsideDown(true)){
    ESP_LOGW(TAG, "Warning: Failed to set display upside down");
  }
  
  ESP_LOGI(TAG, "OLED display initialized successfully");
  return true;
}

/**
 * @brief Initialize HUB75 LED matrix display
 */
bool initializeHUB75(){
  ESP_LOGI(TAG, "Initializing HUB75 dual LED matrix (128x32)...");
  
  // Initialize with dual OE pins mode
  if(!hub75_display.initialize(true)){
    ESP_LOGE(TAG, "Failed to initialize HUB75 display!");
    return false;
  }
  
  ESP_LOGI(TAG, "HUB75 display initialized successfully");
  ESP_LOGI(TAG, "Display size: %dx%d pixels", hub75_display.getWidth(), hub75_display.getHeight());
  
  // Show boot screen
  hub75_display.showScreen(ScreenType::BOOT);
  vTaskDelay(pdMS_TO_TICKS(1000));
  
  return true;
}

/**
 * @brief Draw text at specified position
 */
void drawText(int x, int y, const char* text){
  oled_display.drawString(x, y, text, true);
}

/**
 * @brief Clear display buffer
 */
void clearDisplay(){
  oled_display.clearBuffer();
}

/**
 * @brief Update display (flush buffer)
 */
void updateDisplay(){
  oled_display.updateDisplay();
}

// ============== HUB75 Visualization Functions ==============

/**
 * @brief Visualize IMU data on HUB75 as colored bars
 * Left Panel (64x32): Accelerometer (X, Y, Z)
 * Right Panel (64x32): Gyroscope (X, Y, Z)
 */
void hub75VisualizeIMU(const SensorDataPayload& data){
  hub75_display.clear();
  
  // LEFT PANEL: Accelerometer (-2g to +2g) mapped to 0-31 pixels
  int acc_x = static_cast<int>((data.accel_x / 2.0f + 1.0f) * 15.5f);
  int acc_y = static_cast<int>((data.accel_y / 2.0f + 1.0f) * 15.5f);
  int acc_z = static_cast<int>((data.accel_z / 2.0f + 1.0f) * 15.5f);
  
  // Clamp values
  acc_x = (acc_x < 0) ? 0 : (acc_x > 31 ? 31 : acc_x);
  acc_y = (acc_y < 0) ? 0 : (acc_y > 31 ? 31 : acc_y);
  acc_z = (acc_z < 0) ? 0 : (acc_z > 31 ? 31 : acc_z);
  
  // Draw accelerometer bars on left panel (0-63)
  hub75_display.drawRect(2, 31 - acc_x, 18, acc_x, RGB(255, 0, 0), true, false);    // Red = X
  hub75_display.drawRect(23, 31 - acc_y, 18, acc_y, RGB(0, 255, 0), true, false);   // Green = Y
  hub75_display.drawRect(44, 31 - acc_z, 18, acc_z, RGB(0, 0, 255), true, false);   // Blue = Z
  
  // Labels for left panel
  hub75_display.drawText(7, 2, "X", 1, RGB(255, 100, 100));
  hub75_display.drawText(28, 2, "Y", 1, RGB(100, 255, 100));
  hub75_display.drawText(49, 2, "Z", 1, RGB(100, 100, 255));
  hub75_display.drawText(18, 25, "ACCEL", 1, RGB(200, 200, 200));
  
  // RIGHT PANEL: Gyroscope (-250 to +250 dps) mapped to 0-31 pixels
  int gyro_x = static_cast<int>((data.gyro_x / 250.0f + 1.0f) * 15.5f);
  int gyro_y = static_cast<int>((data.gyro_y / 250.0f + 1.0f) * 15.5f);
  int gyro_z = static_cast<int>((data.gyro_z / 250.0f + 1.0f) * 15.5f);
  
  // Clamp values
  gyro_x = (gyro_x < 0) ? 0 : (gyro_x > 31 ? 31 : gyro_x);
  gyro_y = (gyro_y < 0) ? 0 : (gyro_y > 31 ? 31 : gyro_y);
  gyro_z = (gyro_z < 0) ? 0 : (gyro_z > 31 ? 31 : gyro_z);
  
  // Draw gyroscope bars on right panel (64-127)
  hub75_display.drawRect(66, 31 - gyro_x, 18, gyro_x, RGB(255, 128, 0), true, false);   // Orange = X
  hub75_display.drawRect(87, 31 - gyro_y, 18, gyro_y, RGB(128, 255, 0), true, false);   // Yellow-Green = Y
  hub75_display.drawRect(108, 31 - gyro_z, 18, gyro_z, RGB(0, 128, 255), true, false);  // Light Blue = Z
  
  // Labels for right panel
  hub75_display.drawText(71, 2, "X", 1, RGB(255, 128, 100));
  hub75_display.drawText(92, 2, "Y", 1, RGB(128, 255, 100));
  hub75_display.drawText(113, 2, "Z", 1, RGB(100, 128, 255));
  hub75_display.drawText(82, 25, "GYRO", 1, RGB(200, 200, 200));
  
  hub75_display.show();
}

/**
 * @brief Visualize environmental data as horizontal bars
 * Left Panel (64x32): Temperature & Humidity bars
 * Right Panel (64x32): Pressure bar with numeric values
 */
void hub75VisualizeEnvironmental(const SensorDataPayload& data){
  hub75_display.clear();
  
  // LEFT PANEL: Temperature & Humidity
  // Temperature: 0-40°C mapped to panel width (64)
  int temp_width = static_cast<int>((data.temperature / 40.0f) * 64.0f);
  temp_width = (temp_width < 0) ? 0 : (temp_width > 64 ? 64 : temp_width);
  
  // Humidity: 0-100% mapped to panel width (64)
  int humid_width = static_cast<int>((data.humidity / 100.0f) * 64.0f);
  humid_width = (humid_width < 0) ? 0 : (humid_width > 64 ? 64 : humid_width);
  
  // Draw bars on left panel
  hub75_display.drawRect(0, 2, temp_width, 10, RGB(255, 0, 0), true, false);      // Red = Temp
  hub75_display.drawRect(0, 20, humid_width, 10, RGB(0, 255, 255), true, false);  // Cyan = Humidity
  
  // Labels
  hub75_display.drawText(2, 14, "TEMP", 1, RGB(200, 200, 200));
  
  // Temperature value
  char buf[16];
  snprintf(buf, sizeof(buf), "%.1fC", data.temperature);
  hub75_display.drawText(28, 14, buf, 1, RGB(255, 100, 100));
  
  // RIGHT PANEL: Pressure
  // Pressure: 900-1100 hPa mapped to panel width (64)
  int pressure_width = static_cast<int>(((data.pressure / 100.0f - 900.0f) / 200.0f) * 64.0f);
  pressure_width = (pressure_width < 0) ? 0 : (pressure_width > 64 ? 64 : pressure_width);
  
  // Draw pressure bar on right panel
  hub75_display.drawRect(64, 2, pressure_width, 10, RGB(255, 255, 0), true, false);  // Yellow = Pressure
  
  // Humidity value on right panel
  snprintf(buf, sizeof(buf), "%.0f%%", data.humidity);
  hub75_display.drawText(66, 14, buf, 1, RGB(100, 255, 255));
  
  // Pressure value
  hub75_display.drawRect(64, 20, pressure_width, 10, RGB(255, 128, 0), true, false);  // Orange bar
  snprintf(buf, sizeof(buf), "%.0fhPa", data.pressure / 100.0f);
  hub75_display.drawText(66, 24, buf, 1, RGB(255, 200, 100));
  
  hub75_display.show();
}

/**
 * @brief Visualize microphone data as waveform
 * Left Panel (64x32): Real-time waveform
 * Right Panel (64x32): VU meter style bars
 */
void hub75VisualizeMicrophone(const SensorDataPayload& data){
  hub75_display.clear();
  
  // Map decibel value (-60 to 0 dB) to height
  float normalized = (data.mic_db_level - MIC_DB_MIN) / (MIC_DB_MAX - MIC_DB_MIN);
  
  // LEFT PANEL: Waveform
  float wave_height = normalized * 12.0f;
  
  // Draw center line
  hub75_display.drawLine(0, 16, 64, 16, RGB(50, 50, 50), false);
  
  // Draw waveform using antialiased lines
  hub75_display.setAntialiasing(true);
  
  for(int x = 0; x < 63; x++){
    float phase1 = (x / 64.0f) * 6.28318f;  // One full wave per panel
    float phase2 = ((x + 1) / 64.0f) * 6.28318f;
    
    float y1 = 16 + sinf(phase1) * wave_height;
    float y2 = 16 + sinf(phase2) * wave_height;
    
    // Gradient from blue to magenta based on amplitude
    uint8_t blue = 255;
    uint8_t red = static_cast<uint8_t>(normalized * 255.0f);
    
    hub75_display.drawLine(x, y1, x + 1, y2, RGB(red, 0, blue), true);
  }
  
  // RIGHT PANEL: VU Meter bars
  int bar_height = static_cast<int>(normalized * 30.0f);
  
  // Draw multiple bars for VU meter effect
  for(int i = 0; i < 5; i++){
    int bar_x = 66 + i * 11;
    int this_height = bar_height - (i * 2);
    if(this_height < 0) this_height = 0;
    
    // Color gradient from green to red
    uint8_t red_level = static_cast<uint8_t>((i / 5.0f) * 255);
    uint8_t green_level = static_cast<uint8_t>((1.0f - i / 5.0f) * 255);
    
    hub75_display.drawRect(bar_x, 31 - this_height, 9, this_height, 
                          RGB(red_level, green_level, 0), true, false);
  }
  
  // dB level text
  char buf[16];
  snprintf(buf, sizeof(buf), "%.0fdB", data.mic_db_level);
  hub75_display.drawText(78, 2, buf, 1, RGB(200, 200, 200));
  
  hub75_display.show();
}

/**
 * @brief Windows-style spinning loading animation
 * Shows circles rotating around a center pivot on each display
 */
void hub75SpinningLoadingAnimation(){
  static uint32_t animation_start = 0;
  static bool initialized = false;
  
  if(!initialized){
    animation_start = xTaskGetTickCount() * portTICK_PERIOD_MS;
    initialized = true;
  }
  
  hub75_display.clear();
  
  // Animation parameters
  uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
  float angle = ((current_time - animation_start) % 2000) / 2000.0f * 6.28318f;  // Full rotation in 2 seconds
  
  // Number of circles and their properties
  const int num_circles = 5;
  const float orbit_radius = 10.0f;  // Distance from center
  const float circle_radius = 2.0f;  // Fixed size for all circles
  
  // Enable antialiasing for smooth circles
  hub75_display.setAntialiasing(true);
  
  // Draw spinning circles on left display (64x32)
  const float center_x1 = 32.0f;
  const float center_y1 = 16.0f;
  
  for(int i = 0; i < num_circles; i++){
    // Calculate angle for this circle
    float circle_angle = angle + (i * 6.28318f / num_circles);
    
    // Calculate position
    float x = center_x1 + cosf(circle_angle) * orbit_radius;
    float y = center_y1 + sinf(circle_angle) * orbit_radius * 0.5f;  // Compressed vertically
    
    // Color based on position in circle
    uint8_t hue = static_cast<uint8_t>((i * 255) / num_circles);
    RGB color = {static_cast<uint8_t>(255 - hue), hue, 255};
    
    // Draw filled circle with antialiasing
    hub75_display.drawCircle(x, y, circle_radius, color, true, true);
  }
  
  // Draw center pivot point for left display
  hub75_display.drawCircle(center_x1, center_y1, 1.5f, RGB(255, 255, 255), true, true);
  
  // Draw spinning circles on right display (64x32) - opposite rotation
  const float center_x2 = 96.0f;
  const float center_y2 = 16.0f;
  
  for(int i = 0; i < num_circles; i++){
    // Calculate angle for this circle (opposite direction)
    float circle_angle = -angle + (i * 6.28318f / num_circles);
    
    // Calculate position
    float x = center_x2 + cosf(circle_angle) * orbit_radius;
    float y = center_y2 + sinf(circle_angle) * orbit_radius * 0.5f;
    
    // Different color scheme for right display
    uint8_t hue = static_cast<uint8_t>((i * 255) / num_circles);
    RGB color = {hue, static_cast<uint8_t>(255 - hue), 255};
    
    // Draw filled circle with antialiasing
    hub75_display.drawCircle(x, y, circle_radius, color, true, true);
  }
  
  // Draw center pivot point for right display
  hub75_display.drawCircle(center_x2, center_y2, 1.5f, RGB(255, 255, 255), true, true);
  
  hub75_display.show();
}

/**
 * @brief Show system info visualization
 * Left Panel (64x32): FPS counters
 * Right Panel (64x32): Status indicators
 */
void hub75VisualizeSystemInfo(){
  hub75_display.clear();
  hub75_display.setAntialiasing(false);
  
  // LEFT PANEL: FPS Information
  // Draw frame around left panel
  hub75_display.drawRect(0, 0, 63, 31, RGB(100, 100, 255), false, false);
  
  char buf[16];
  
  // Sensor RX FPS
  hub75_display.drawText(4, 4, "RX", 1, RGB(0, 255, 0));
  snprintf(buf, sizeof(buf), "%lu", stats.sensor_fps);
  hub75_display.drawText(20, 4, buf, 1, RGB(0, 255, 255));
  
  // LED TX FPS
  hub75_display.drawText(4, 14, "TX", 1, RGB(255, 128, 0));
  snprintf(buf, sizeof(buf), "%lu", stats.led_fps);
  hub75_display.drawText(20, 14, buf, 1, RGB(255, 255, 0));
  
  // Display updates
  hub75_display.drawText(4, 24, "DISP", 1, RGB(255, 0, 255));
  
  // RIGHT PANEL: Status indicators
  // Draw frame around right panel
  hub75_display.drawRect(64, 0, 63, 31, RGB(255, 100, 100), false, false);
  
  // Status dots for sensor validity
  hub75_display.drawText(68, 4, "IMU", 1, RGB(200, 200, 200));
  hub75_display.drawCircle(100, 7, 2, 
    current_sensor_data.getImuValid() ? RGB(0, 255, 0) : RGB(255, 0, 0), 
    true, false);
  
  hub75_display.drawText(68, 14, "ENV", 1, RGB(200, 200, 200));
  hub75_display.drawCircle(100, 17, 2,
    current_sensor_data.getEnvValid() ? RGB(0, 255, 0) : RGB(255, 0, 0),
    true, false);
  
  hub75_display.drawText(68, 24, "GPS", 1, RGB(200, 200, 200));
  hub75_display.drawCircle(100, 27, 2,
    current_sensor_data.getGpsValid() ? RGB(0, 255, 0) : RGB(255, 0, 0),
    true, false);
  
  // Animation indicator on right
  hub75_display.drawText(108, 4, "A", 1, RGB(255, 255, 0));
  snprintf(buf, sizeof(buf), "%d", current_animation);
  hub75_display.drawText(114, 4, buf, 1, RGB(255, 200, 0));
  
  hub75_display.show();
}

/**
 * @brief Display Page 0: IMU Data
 */
void displayImuPage(const SensorDataPayload& data){
  clearDisplay();
  
  drawText(0, 0, "===== IMU DATA =====");
  
  if(data.getImuValid()){
    char buf[32];
    
    drawText(0, 12, "Accel (g):");
    snprintf(buf, sizeof(buf), " X:%.2f", data.accel_x);
    drawText(0, 22, buf);
    snprintf(buf, sizeof(buf), " Y:%.2f", data.accel_y);
    drawText(0, 32, buf);
    snprintf(buf, sizeof(buf), " Z:%.2f", data.accel_z);
    drawText(0, 42, buf);
    
    drawText(0, 54, "Gyro (dps):");
    snprintf(buf, sizeof(buf), " X:%.1f", data.gyro_x);
    drawText(0, 64, buf);
    snprintf(buf, sizeof(buf), " Y:%.1f", data.gyro_y);
    drawText(0, 74, buf);
    snprintf(buf, sizeof(buf), " Z:%.1f", data.gyro_z);
    drawText(0, 84, buf);
    
    drawText(0, 96, "Mag (uT):");
    snprintf(buf, sizeof(buf), " X:%.1f", data.mag_x);
    drawText(0, 106, buf);
    snprintf(buf, sizeof(buf), " Y:%.1f Z:%.1f", data.mag_y, data.mag_z);
    drawText(0, 116, buf);
  }else{
    drawText(10, 60, "NO IMU DATA");
  }
  
  updateDisplay();
}

/**
 * @brief Display Page 1: Environmental Data
 */
void displayEnvironmentalPage(const SensorDataPayload& data){
  clearDisplay();
  
  drawText(0, 0, "=== ENVIRONMENT ===");
  
  if(data.getEnvValid()){
    char buf[32];
    
    drawText(0, 20, "Temperature:");
    snprintf(buf, sizeof(buf), "  %.2f C", data.temperature);
    drawText(0, 32, buf);
    
    drawText(0, 50, "Humidity:");
    snprintf(buf, sizeof(buf), "  %.1f %%", data.humidity);
    drawText(0, 62, buf);
    
    drawText(0, 80, "Pressure:");
    snprintf(buf, sizeof(buf), "  %.0f Pa", data.pressure);
    drawText(0, 92, buf);
    snprintf(buf, sizeof(buf), "  %.2f hPa", data.pressure / 100.0f);
    drawText(0, 104, buf);
  }else{
    drawText(10, 60, "NO ENV DATA");
  }
  
  updateDisplay();
}

/**
 * @brief Display Page 2: GPS Data
 */
void displayGpsPage(const SensorDataPayload& data){
  clearDisplay();
  
  drawText(0, 0, "===== GPS DATA =====");
  
  if(data.getGpsValid()){
    char buf[32];
    
    drawText(0, 12, "Position:");
    snprintf(buf, sizeof(buf), " Lat:%.5f", data.latitude);
    drawText(0, 22, buf);
    snprintf(buf, sizeof(buf), " Lon:%.5f", data.longitude);
    drawText(0, 32, buf);
    snprintf(buf, sizeof(buf), " Alt:%.1fm", data.altitude);
    drawText(0, 42, buf);
    
    drawText(0, 54, "Navigation:");
    snprintf(buf, sizeof(buf), " Spd:%.1fkn", data.speed_knots);
    drawText(0, 64, buf);
    snprintf(buf, sizeof(buf), " Crs:%.1fdeg", data.course);
    drawText(0, 74, buf);
    
    drawText(0, 86, "Status:");
    snprintf(buf, sizeof(buf), " Sats:%u Fix:%u", data.gps_satellites, data.getGpsFixQuality());
    drawText(0, 96, buf);
    
    snprintf(buf, sizeof(buf), "Time: %02u:%02u:%02u", 
      data.gps_hour, data.gps_minute, data.gps_second);
    drawText(0, 108, buf);
  }else{
    drawText(10, 60, "NO GPS FIX");
  }
  
  updateDisplay();
}

/**
 * @brief Add microphone sample to history buffer
 */
void addMicSample(float db_level){
  mic_history[mic_history_index] = db_level;
  mic_history_index = (mic_history_index + 1) % MIC_GRAPH_SAMPLES;
}

// ============== LED Animation Functions ==============

/**
 * @brief Rainbow wave animation
 */
void animationRainbow(){
  float time_sec = animation_time_ms / 1000.0f;
  
  for(uint16_t i = 0; i < LED_COUNT_TOTAL; i++){
    float hue = fmodf((i / (float)LED_COUNT_TOTAL) + (time_sec * 0.2f), 1.0f);
    float h = hue * 6.0f;
    int region = (int)h;
    float f = h - region;
    
    uint8_t q = (uint8_t)(255 * (1.0f - f));
    uint8_t t = (uint8_t)(255 * f);
    
    switch(region % 6){
      case 0: led_data.leds[i] = RgbwColor(255, t, 0, 0); break;
      case 1: led_data.leds[i] = RgbwColor(q, 255, 0, 0); break;
      case 2: led_data.leds[i] = RgbwColor(0, 255, t, 0); break;
      case 3: led_data.leds[i] = RgbwColor(0, q, 255, 0); break;
      case 4: led_data.leds[i] = RgbwColor(t, 0, 255, 0); break;
      case 5: led_data.leds[i] = RgbwColor(255, 0, q, 0); break;
    }
  }
}

/**
 * @brief Breathing animation with different colors per strip
 */
void animationBreathing(){
  float time_sec = animation_time_ms / 1000.0f;
  uint8_t brightness = (uint8_t)(127.5f + 127.5f * sinf(time_sec * 2.0f));
  
  led_data.setLeftFinColor(RgbwColor(brightness, 0, 0, 0));
  led_data.setTongueColor(RgbwColor(0, brightness, 0, 0));
  led_data.setRightFinColor(RgbwColor(0, 0, brightness, 0));
  led_data.setScaleColor(RgbwColor(0, 0, 0, brightness));
}

/**
 * @brief Wave animation across all strips
 */
void animationWave(){
  float time_sec = animation_time_ms / 1000.0f;
  
  for(uint16_t i = 0; i < LED_COUNT_TOTAL; i++){
    float wave = sinf((i * 0.3f) + (time_sec * 3.0f));
    uint8_t brightness = (uint8_t)(127.5f + 127.5f * wave);
    led_data.leds[i] = RgbwColor(brightness, brightness / 2, 0, 0);
  }
}

/**
 * @brief Update current animation
 */
void updateAnimation(){
  switch(current_animation){
    case 0: animationRainbow(); break;
    case 1: animationBreathing(); break;
    case 2: animationWave(); break;
    default: animationRainbow(); break;
  }
}

/**
 * @brief Draw microphone waveform graph
 */
void drawMicGraph(){
  const int graph_x = 4;
  const int graph_y = 70;
  
  // Draw graph border
  oled_display.drawRect(graph_x - 1, graph_y - 1, MIC_GRAPH_WIDTH + 2, MIC_GRAPH_HEIGHT + 2, false, true);
  
  // Draw center line (for reference)
  int mid_y = graph_y + MIC_GRAPH_HEIGHT / 2;
  for(int x = 0; x < MIC_GRAPH_WIDTH; x += 4){
    oled_display.setPixel(graph_x + x, mid_y, true);
  }
  
  // Draw waveform
  for(int i = 0; i < MIC_GRAPH_SAMPLES - 1; i++){
    // Get samples (oldest to newest, scrolling left)
    int idx1 = (mic_history_index + i) % MIC_GRAPH_SAMPLES;
    int idx2 = (mic_history_index + i + 1) % MIC_GRAPH_SAMPLES;
    
    float db1 = mic_history[idx1];
    float db2 = mic_history[idx2];
    
    // Clamp to range
    if(db1 < MIC_DB_MIN) db1 = MIC_DB_MIN;
    if(db1 > MIC_DB_MAX) db1 = MIC_DB_MAX;
    if(db2 < MIC_DB_MIN) db2 = MIC_DB_MIN;
    if(db2 > MIC_DB_MAX) db2 = MIC_DB_MAX;
    
    // Map dB to pixel Y coordinate (inverted - higher dB = lower Y)
    int y1 = graph_y + MIC_GRAPH_HEIGHT - 1 - (int)((db1 - MIC_DB_MIN) / (MIC_DB_MAX - MIC_DB_MIN) * (MIC_GRAPH_HEIGHT - 1));
    int y2 = graph_y + MIC_GRAPH_HEIGHT - 1 - (int)((db2 - MIC_DB_MIN) / (MIC_DB_MAX - MIC_DB_MIN) * (MIC_GRAPH_HEIGHT - 1));
    
    // Draw line between consecutive points
    oled_display.drawLine(graph_x + i, y1, graph_x + i + 1, y2, true);
  }
}

/**
 * @brief Display Page 3: Microphone Data with Waveform Graph
 */
void displayMicrophonePage(const SensorDataPayload& data){
  clearDisplay();
  
  drawText(0, 0, "==== MIC DATA =====");
  
  if(data.getMicValid()){
    char buf[32];
    
    // Current dB level
    drawText(0, 12, "Level:");
    snprintf(buf, sizeof(buf), " %.1f dB", data.mic_db_level);
    drawText(42, 12, buf);
    
    // Clipping indicator
    if(data.getMicClipping()){
      drawText(90, 12, "[CLIP]");
    }
    
    // Peak amplitude
    drawText(0, 24, "Peak:");
    snprintf(buf, sizeof(buf), " %ld", data.mic_peak_amplitude);
    drawText(36, 24, buf);
    
    // Graph title and range
    drawText(0, 38, "Waveform:");
    snprintf(buf, sizeof(buf), "%.1fs", MIC_GRAPH_DURATION_SEC);
    drawText(60, 38, buf);
    
    // dB range labels
    drawText(0, 52, "-60dB");
    drawText(100, 52, "0dB");
    
    // Draw the waveform graph
    drawMicGraph();
    
    // Footer info
    drawText(0, 118, "Graph scrolls left");
  }else{
    drawText(10, 60, "NO MIC DATA");
  }
  
  updateDisplay();
}

/**
 * @brief Display Page 4: System Info
 */
void displaySystemPage(const SensorDataPayload& data){
  clearDisplay();
  
  drawText(0, 0, "==== SYSTEM INFO ====");
  
  char buf[32];
  
  // Data rates
  drawText(0, 12, "Data Rate:");
  snprintf(buf, sizeof(buf), " RX:%lu TX:%lu FPS", stats.sensor_fps, stats.led_fps);
  drawText(0, 22, buf);
  
  // Fan speed
  drawText(0, 34, "Fan Speed:");
  snprintf(buf, sizeof(buf), " %u%% (%u/255)", (led_data.fan_speed * 100) / 255, led_data.fan_speed);
  drawText(0, 44, buf);
  
  // Buttons
  drawText(0, 56, "Buttons:");
  snprintf(buf, sizeof(buf), " A:%u B:%u C:%u D:%u",
    data.getButtonA(), data.getButtonB(), data.getButtonC(), data.getButtonD());
  drawText(0, 66, buf);
  
  // Sensor validity
  drawText(0, 78, "Sensors:");
  snprintf(buf, sizeof(buf), " IMU:%u ENV:%u",
    data.getImuValid(), data.getEnvValid());
  drawText(0, 88, buf);
  snprintf(buf, sizeof(buf), " GPS:%u MIC:%u",
    data.getGpsValidFlag(), data.getMicValid());
  drawText(0, 98, buf);
  
  // Current animation
  drawText(0, 110, "Anim:");
  const char* anim_names[] = {"Rainbow", "Breath", "Wave"};
  snprintf(buf, sizeof(buf), " %s", anim_names[current_animation]);
  drawText(35, 110, buf);
  
  // Page indicator
  snprintf(buf, sizeof(buf), "Pg %d/%d", current_page + 1, TOTAL_PAGES);
  drawText(95, 110, buf);
  
  updateDisplay();
}

/**
 * @brief Handle page navigation based on button states
 */
void handlePageNavigation(const SensorDataPayload& data){
  bool button_a = data.getButtonA();
  bool button_b = data.getButtonB();
  
  // Button A: Previous page (rising edge detection)
  if(button_a && !button_a_prev){
    current_page--;
    if(current_page < 0){
      current_page = TOTAL_PAGES - 1;
    }
    ESP_LOGI(TAG, "Page changed to %d", current_page);
  }
  
  // Button B: Next page (rising edge detection)
  if(button_b && !button_b_prev){
    current_page++;
    if(current_page >= TOTAL_PAGES){
      current_page = 0;
    }
    ESP_LOGI(TAG, "Page changed to %d", current_page);
  }
  
  button_a_prev = button_a;
  button_b_prev = button_b;
}

/**
 * @brief Display current page based on page number
 */
void displayCurrentPage(const SensorDataPayload& data){
  // Update OLED display based on page
  // Show spinning loading animation on all HUB75 pages for visual interest
  switch(current_page){
    case 0:
      displayImuPage(data);
      hub75SpinningLoadingAnimation();
      break;
    case 1:
      displayEnvironmentalPage(data);
      hub75SpinningLoadingAnimation();
      break;
    case 2:
      displayGpsPage(data);
      hub75SpinningLoadingAnimation();
      break;
    case 3:
      displayMicrophonePage(data);
      hub75SpinningLoadingAnimation();
      break;
    case 4:
      displaySystemPage(data);
      hub75SpinningLoadingAnimation();
      break;
    default:
      current_page = 0;
      displayImuPage(data);
      hub75SpinningLoadingAnimation();
      break;
  }
}

/**
 * @brief Core 0 Task: Receive UART data and update shared buffer
 */
void uartReceiveTask(void* parameter){
  ESP_LOGI(TAG, "UART receive task started on Core 0");
  
  UartPacket packet;
  
  while(true){
    // Check for received packets (non-blocking)
    if(uart_comm.receivePacket(packet)){
      if(packet.message_type == MessageType::SENSOR_DATA){
        // Parse sensor data payload
        if(packet.payload_length == sizeof(SensorDataPayload)){
          if(xSemaphoreTake(sensor_data_mutex, pdMS_TO_TICKS(5)) == pdTRUE){
            memcpy(&current_sensor_data, packet.payload, sizeof(SensorDataPayload));
            data_received = true;
            last_data_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
            stats.sensor_frames_received++;
            xSemaphoreGive(sensor_data_mutex);
          }
        }
      }
    }
    
    // Small delay to prevent task starvation
    vTaskDelay(1);
  }
}

/**
 * @brief LED Send Task - generates animations and sends to CPU at 60 FPS
 */
void ledSendTask(void* parameter){
  ESP_LOGI(TAG, "LED send task started on Core 0");
  
  uint64_t next_frame_time = esp_timer_get_time();
  
  while(true){
    uint64_t current_time = esp_timer_get_time();
    
    if(current_time >= next_frame_time){
      // Update animation
      animation_time_ms = current_time / 1000;
      updateAnimation();
      
      // Smooth fan speed animation: 12-second cycle
      // 0-3s: Ramp up from 0% to 100%
      // 3-6s: Hold at 100%
      // 6-9s: Ramp down from 100% to 0%
      // 9-12s: Hold at 0%
      uint32_t cycle_time_ms = animation_time_ms % 12000;  // 12-second cycle
      
      if(cycle_time_ms < 3000){
        // Ramp up: 0-3 seconds
        led_data.fan_speed = (uint8_t)((cycle_time_ms * 255) / 3000);
      }else if(cycle_time_ms < 6000){
        // Hold at 100%: 3-6 seconds
        led_data.fan_speed = 255;
      }else if(cycle_time_ms < 9000){
        // Ramp down: 6-9 seconds
        uint32_t ramp_down_time = cycle_time_ms - 6000;
        led_data.fan_speed = (uint8_t)(255 - ((ramp_down_time * 255) / 3000));
      }else{
        // Hold at 0%: 9-12 seconds
        led_data.fan_speed = 0;
      }
      
      // Send LED data via UART
      if(uart_comm.sendPacket(
        MessageType::LED_DATA,
        reinterpret_cast<uint8_t*>(&led_data),
        sizeof(LedDataPayload)
      )){
        stats.led_frames_sent++;
      }
      
      // Calculate next frame time
      next_frame_time += LED_FRAME_INTERVAL_US;
      
      // Resync if fallen behind
      if(current_time > next_frame_time + LED_FRAME_INTERVAL_US){
        next_frame_time = current_time;
      }
    }
    
    vTaskDelay(1);
  }
}

/**
 * @brief Core 1 Task: Update display based on received data
 */
void displayUpdateTask(void* parameter){
  ESP_LOGI(TAG, "Display update task started on Core 1");
  
  // Initialize microphone graph
  mic_sample_interval_ms = (uint32_t)((MIC_GRAPH_DURATION_SEC * 1000.0f) / (float)MIC_GRAPH_SAMPLES);
  last_mic_sample_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
  for(int i = 0; i < MIC_GRAPH_SAMPLES; i++){
    mic_history[i] = MIC_DB_MIN;
  }
  ESP_LOGI(TAG, "Microphone graph: %.1fs window, %d samples, %ums interval", 
           MIC_GRAPH_DURATION_SEC, MIC_GRAPH_SAMPLES, mic_sample_interval_ms);
  
  SensorDataPayload local_copy;
  bool have_data = false;
  
  while(true){
    uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    // Copy shared data to local buffer
    if(xSemaphoreTake(sensor_data_mutex, pdMS_TO_TICKS(5)) == pdTRUE){
      if(data_received){
        memcpy(&local_copy, &current_sensor_data, sizeof(SensorDataPayload));
        have_data = true;
      }
      xSemaphoreGive(sensor_data_mutex);
    }
    
    // Update microphone history buffer at calculated interval
    if(have_data && local_copy.getMicValid()){
      if(current_time - last_mic_sample_time >= mic_sample_interval_ms){
        addMicSample(local_copy.mic_db_level);
        last_mic_sample_time = current_time;
      }
    }
    
    // Update display if we have data
    if(have_data){
      // Handle button navigation
      handlePageNavigation(local_copy);
      
      // Display current page
      displayCurrentPage(local_copy);
      stats.display_updates++;
    }else{
      // No data received yet - show waiting message and spinning loading animation
      clearDisplay();
      drawText(10, 50, "Waiting for");
      drawText(10, 62, "sensor data...");
      updateDisplay();
      
      // Show spinning loading animation on HUB75 while waiting
      hub75SpinningLoadingAnimation();
    }
    
    // Print statistics every second
    if(current_time - stats.last_report_time >= 1000){
      stats.sensor_fps = stats.sensor_frames_received;
      stats.led_fps = stats.led_frames_sent;
      
      ESP_LOGI(TAG, "Stats: Sensor RX: %lu fps | LED TX: %lu fps | Display: %lu | Page: %d | Anim: %d",
        stats.sensor_fps, stats.led_fps, stats.display_updates, current_page, current_animation);
      
      stats.sensor_frames_received = 0;
      stats.led_frames_sent = 0;
      stats.display_updates = 0;
      stats.last_report_time = current_time;
      
      // Cycle animation every 10 seconds
      if((current_time / 10000) % 3 != current_animation){
        current_animation = (current_time / 10000) % 3;
        ESP_LOGI(TAG, "Switching to animation %d", current_animation);
      }
    }
    
    // Update at 60fps for smooth animation
    vTaskDelay(pdMS_TO_TICKS(16));  // ~60Hz (16.67ms)
  }
}

/**
 * @brief Application entry point
 */
extern "C" void app_main(void){
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "========================================================");
  ESP_LOGI(TAG, "  GPU Bidirectional: Sensor Display + LED Animations   ");
  ESP_LOGI(TAG, "========================================================");
  ESP_LOGI(TAG, "");
  
  // Initialize OLED display
  if(!initializeOLED()){
    ESP_LOGE(TAG, "FATAL: OLED initialization failed!");
    ESP_LOGE(TAG, "System halted.");
    return;
  }
  
  // Initialize HUB75 LED matrix display
  if(!initializeHUB75()){
    ESP_LOGE(TAG, "FATAL: HUB75 initialization failed!");
    ESP_LOGE(TAG, "System halted.");
    return;
  }
  
  // Show startup message on OLED
  clearDisplay();
  drawText(10, 20, "GPU System");
  drawText(10, 32, "Initializing...");
  drawText(10, 44, "OLED: OK");
  drawText(10, 56, "HUB75: OK");
  drawText(10, 68, "Sensor RX");
  drawText(10, 80, "LED TX @ 60fps");
  updateDisplay();
  
  // Initialize UART communication
  ESP_LOGI(TAG, "Initializing UART communication...");
  if(!uart_comm.init()){
    ESP_LOGE(TAG, "FATAL: UART initialization failed!");
    ESP_LOGE(TAG, "System halted.");
    
    clearDisplay();
    drawText(10, 50, "UART INIT");
    drawText(10, 62, "FAILED!");
    updateDisplay();
    return;
  }
  ESP_LOGI(TAG, "UART initialized (2 Mbps, RX=GPIO13, TX=GPIO12)");
  
  // Initialize LED data and fan control
  led_data.setAllColor(RgbwColor(0, 0, 0, 0));
  led_data.fan_speed = 128;  // Start at 50% speed
  ESP_LOGI(TAG, "LED animation system initialized (%d LEDs, %d bytes)", 
           LED_COUNT_TOTAL, sizeof(LedDataPayload));
  ESP_LOGI(TAG, "Fan control initialized (default: 50%%)");
  
  // Create mutex for shared data protection
  sensor_data_mutex = xSemaphoreCreateMutex();
  if(sensor_data_mutex == NULL){
    ESP_LOGE(TAG, "FATAL: Failed to create mutex!");
    return;
  }
  
  // Initialize shared sensor data
  memset(&current_sensor_data, 0, sizeof(SensorDataPayload));
  
  ESP_LOGI(TAG, "Creating tasks on both cores...");
  
  // Core 0 tasks
  xTaskCreatePinnedToCore(
    uartReceiveTask,
    "uart_receive",
    8192,
    NULL,
    3,
    NULL,
    0  // Core 0
  );
  
  xTaskCreatePinnedToCore(
    ledSendTask,
    "led_send",
    8192,
    NULL,
    3,
    NULL,
    0  // Core 0
  );
  
  // Core 1 tasks
  xTaskCreatePinnedToCore(
    displayUpdateTask,
    "display_update",
    8192,
    NULL,
    2,
    NULL,
    1  // Core 1
  );
  
  ESP_LOGI(TAG, "All tasks created!");
  ESP_LOGI(TAG, "Core 0 - UART RX (Sensors @ 60Hz) + LED TX @ 60Hz");
  ESP_LOGI(TAG, "Core 1 - Display updates @ 60Hz (OLED + HUB75)");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "Controls:");
  ESP_LOGI(TAG, "  Button A - Previous page");
  ESP_LOGI(TAG, "  Button B - Next page");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "Pages:");
  ESP_LOGI(TAG, "  0 - IMU (Accel/Gyro/Mag)");
  ESP_LOGI(TAG, "  1 - Environment (Temp/Humidity/Pressure)");
  ESP_LOGI(TAG, "  2 - GPS (Position/Navigation)");
  ESP_LOGI(TAG, "  3 - Microphone (Audio levels)");
  ESP_LOGI(TAG, "  4 - System Info (FPS/Buttons/Status)");
  ESP_LOGI(TAG, "================================================");
  ESP_LOGI(TAG, "");
}
