/*****************************************************************
 * File:      GPU.cpp
 * Category:  Main Application
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    GPU-side main application with boot sequence and display managers.
 *    - Boot animations for HUB75, OLED, and LEDs (minimum 1.5s)
 *    - Receives sensor data from CPU via UART
 *    - Displays sensor data on OLED with page navigation
 *    - Shows visualizations on HUB75 LED matrix
 *    - Sends LED animation data to CPU at 60Hz
 * 
 * Hardware:
 *    - ESP32-S3 (GPU)
 *    - OLED SH1107 128x128 display (I2C: SDA=GPIO2, SCL=GPIO1)
 *    - HUB75 Dual LED Matrix (128x32 total)
 *    - UART to CPU: RX=GPIO13, TX=GPIO12
 * 
 * Boot Sequence:
 *    1. Initialize HUB75 and OLED displays
 *    2. Show boot animations (1.5s minimum)
 *    3. Initialize UART communication
 *    4. Wait for valid sensor data from CPU
 *    5. Enter normal operation mode
 * 
 * Controls:
 *    - Button A: Previous page
 *    - Button B: Next page
 *****************************************************************/

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "rom/ets_sys.h"
#include <cmath>

// Display and LED managers
#include "Manager/HUB75DisplayManager.hpp"
#include "Manager/OLEDDisplayManager.hpp"
#include "Manager/LEDAnimationManager.hpp"

// Boot animations
#include "Animations/HUB75BootAnimations.hpp"
#include "Animations/OLEDBootAnimations.hpp"
#include "Animations/LEDBootAnimations.hpp"

// Test animations
#include "Animations/HUB75TestAnimations.hpp"
#include "Animations/LEDTestAnimations.hpp"

// UART communication
#include "Drivers/UART Comms/GpuUartBidirectional.hpp"

using namespace arcos::communication;
using namespace arcos::manager;
using namespace arcos::animations;

static const char* TAG = "GPU_MAIN";

// ============== Configuration ==============
constexpr int DISPLAY_WIDTH = 128;
constexpr int DISPLAY_HEIGHT = 128;
constexpr int TOTAL_PAGES = 5;
constexpr uint32_t LED_FPS = 60;
constexpr uint32_t LED_FRAME_INTERVAL_US = 1000000 / LED_FPS;
constexpr uint32_t BOOT_DURATION_MS = 1500;  // Minimum boot animation duration

// ============== Global Managers ==============
HUB75DisplayManager hub75_manager;
OLEDDisplayManager oled_manager;
LEDAnimationManager led_manager;
GpuUartBidirectional uart_comm;

// ============== Shared Data ==============
SemaphoreHandle_t sensor_data_mutex;
SensorDataPayload current_sensor_data;
bool data_received = false;
uint32_t last_data_time = 0;

// ============== Display State ==============
int current_page = 0;
bool button_a_prev = false;
bool button_b_prev = false;

// ============== Boot State ==============
enum class BootPhase{
  INIT_DISPLAYS,
  BOOT_ANIMATION,
  INIT_UART,
  WAIT_FOR_DATA,
  NORMAL_OPERATION
};

volatile BootPhase boot_phase = BootPhase::INIT_DISPLAYS;
uint32_t boot_start_time = 0;
bool uart_initialized = false;
bool displays_initialized = false;

// ============== Statistics ==============
struct Stats{
  uint32_t sensor_frames_received = 0;
  uint32_t led_frames_sent = 0;
  uint32_t display_updates = 0;
  uint32_t hub75_frames = 0;
  uint32_t last_report_time = 0;
  uint32_t sensor_fps = 0;
  uint32_t led_fps = 0;
  uint32_t hub75_fps = 0;
};
Stats stats;

/**
 * @brief Initialize HUB75 and OLED displays
 * @return true if both displays initialized successfully
 */
bool initializeDisplays(){
  ESP_LOGI(TAG, "Initializing displays...");
  
  // Initialize HUB75
  if(!hub75_manager.initialize(true)){
    ESP_LOGE(TAG, "Failed to initialize HUB75 display!");
    return false;
  }
  ESP_LOGI(TAG, "HUB75 initialized (%dx%d)", hub75_manager.getWidth(), hub75_manager.getHeight());
  
  // Initialize OLED
  if(!oled_manager.initialize(0, 2, 1, 400000, true, true, 0xCF)){
    ESP_LOGE(TAG, "Failed to initialize OLED display!");
    return false;
  }
  ESP_LOGI(TAG, "OLED initialized (%dx%d)", oled_manager.getWidth(), oled_manager.getHeight());
  
  return true;
}

/**
 * @brief Register all animations with managers
 */
void registerAllAnimations(){
  ESP_LOGI(TAG, "Registering animations...");
  
  // Register HUB75 animations
  hub75::registerBootAnimations(hub75_manager);
  hub75::registerTestAnimations(hub75_manager);
  
  // Register OLED boot animations
  oled::registerBootAnimations(oled_manager);
  
  // Register LED animations
  led::registerBootAnimations(led_manager);
  led::registerTestAnimations(led_manager);
  
  ESP_LOGI(TAG, "Registered %d HUB75 animations, %d OLED animations, %d LED animations",
           hub75_manager.getAnimationCount(),
           oled_manager.getAnimationCount(),
           led_manager.getAnimationCount());
}

/**
 * @brief Initialize UART communication
 * @return true if successful
 */
bool initializeUart(){
  ESP_LOGI(TAG, "Initializing UART communication...");
  
  if(!uart_comm.init()){
    ESP_LOGE(TAG, "Failed to initialize UART!");
    return false;
  }
  
  ESP_LOGI(TAG, "UART initialized (2 Mbps, RX=GPIO13, TX=GPIO12)");
  return true;
}

/**
 * @brief Display boot status on OLED
 */
void displayBootStatus(const char* status, bool success){
  oled_manager.clear();
  oled_manager.drawText(10, 30, "GPU BOOT", true);
  oled_manager.drawText(5, 50, status, true);
  
  if(success){
    oled_manager.drawText(100, 50, "[OK]", true);
  }else{
    oled_manager.drawText(95, 50, "[FAIL]", true);
  }
  
  oled_manager.show();
}

/**
 * @brief Run boot sequence
 * Shows boot animations while initializing subsystems
 * Note: HUB75 should only be called from ONE core after initialization
 */
void runBootSequence(){
  boot_start_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
  uint32_t current_time = boot_start_time;
  
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "========================================================");
  ESP_LOGI(TAG, "        SYNTH-HEAD GPU - Boot Sequence v1.0            ");
  ESP_LOGI(TAG, "========================================================");
  ESP_LOGI(TAG, "");
  
  // Phase 1: Initialize displays
  ESP_LOGI(TAG, "[1/4] Initializing displays...");
  displays_initialized = initializeDisplays();
  
  if(!displays_initialized){
    ESP_LOGE(TAG, "FATAL: Display initialization failed!");
    return;
  }
  
  // Phase 2: Register animations
  ESP_LOGI(TAG, "[2/4] Registering animations...");
  registerAllAnimations();
  
  // Phase 3: Initialize LED manager
  ESP_LOGI(TAG, "[3/4] Initializing LED system...");
  led_manager.initialize();
  led_manager.setFanSpeed(128);  // Start at 50%
  ESP_LOGI(TAG, "LED system initialized (%d LEDs)", LED_COUNT_TOTAL);
  
  // Phase 4: Show boot animations (minimum 1.5 seconds)
  ESP_LOGI(TAG, "[4/4] Running boot animations (%.1fs minimum)...", BOOT_DURATION_MS / 1000.0f);
  
  uint32_t animation_start = xTaskGetTickCount() * portTICK_PERIOD_MS;
  
  ESP_LOGI(TAG, "Boot animation loop starting...");
  uint32_t loop_count = 0;
  
  while((xTaskGetTickCount() * portTICK_PERIOD_MS - animation_start) < BOOT_DURATION_MS){
    current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    uint32_t anim_time = current_time - animation_start;
    
    // Run boot animations - HUB75 safe here since tasks aren't created yet
    hub75_manager.executeAnimation("boot_spinning_circles", anim_time);
    oled_manager.executeAnimation("boot_system_init", anim_time);
    led_manager.executeAnimation("boot_sequential_activation", anim_time);
    
    loop_count++;
    if(loop_count % 30 == 0){
      ESP_LOGI(TAG, "Boot animation running... (%.1fs / %.1fs)", 
        anim_time / 1000.0f, BOOT_DURATION_MS / 1000.0f);
    }
    
    vTaskDelay(pdMS_TO_TICKS(16));  // ~60fps
  }
  
  ESP_LOGI(TAG, "Boot animations complete! (%lu loops)", loop_count);
  
  // Clear HUB75 display and free any temporary animation state
  ESP_LOGI(TAG, "Clearing displays to free memory...");
  hub75_manager.clear();
  hub75_manager.show();
  oled_manager.clear();
  oled_manager.show();
  
  // Small delay to ensure display updates complete
  vTaskDelay(pdMS_TO_TICKS(50));
  
  ESP_LOGI(TAG, "===== TRANSITIONING TO WAIT_FOR_DATA =====");
  boot_phase = BootPhase::WAIT_FOR_DATA;
  ESP_LOGI(TAG, "");
  
  // Initialize UART
  ESP_LOGI(TAG, "Initializing UART communication...");
  displayBootStatus("Init UART...", false);
  vTaskDelay(pdMS_TO_TICKS(200));
  
  uart_initialized = initializeUart();
  
  if(!uart_initialized){
    ESP_LOGE(TAG, "FATAL: UART initialization failed!");
    displayBootStatus("UART Failed", false);
    vTaskDelay(pdMS_TO_TICKS(2000));
    return;
  }
  
  displayBootStatus("UART Ready", true);
  vTaskDelay(pdMS_TO_TICKS(500));
  
  // Wait for initial sensor data
  ESP_LOGI(TAG, "Waiting for sensor data from CPU...");
  displayBootStatus("Wait Sensor", false);
}

/**
 * @brief OLED page displays - same as original uart_gpu_sensor_display.cpp
 */
void displayImuPage(const SensorDataPayload& data){
  oled_manager.clear();
  oled_manager.drawText(0, 0, "===== IMU DATA =====", true);
  
  if(data.getImuValid()){
    char buf[32];
    oled_manager.drawText(0, 12, "Accel (g):", true);
    snprintf(buf, sizeof(buf), " X:%.2f", data.accel_x);
    oled_manager.drawText(0, 22, buf, true);
    snprintf(buf, sizeof(buf), " Y:%.2f", data.accel_y);
    oled_manager.drawText(0, 32, buf, true);
    snprintf(buf, sizeof(buf), " Z:%.2f", data.accel_z);
    oled_manager.drawText(0, 42, buf, true);
    
    oled_manager.drawText(0, 54, "Gyro (dps):", true);
    snprintf(buf, sizeof(buf), " X:%.1f", data.gyro_x);
    oled_manager.drawText(0, 64, buf, true);
    snprintf(buf, sizeof(buf), " Y:%.1f", data.gyro_y);
    oled_manager.drawText(0, 74, buf, true);
    snprintf(buf, sizeof(buf), " Z:%.1f", data.gyro_z);
    oled_manager.drawText(0, 84, buf, true);
    
    oled_manager.drawText(0, 96, "Mag (uT):", true);
    snprintf(buf, sizeof(buf), " X:%.1f", data.mag_x);
    oled_manager.drawText(0, 106, buf, true);
    snprintf(buf, sizeof(buf), " Y:%.1f Z:%.1f", data.mag_y, data.mag_z);
    oled_manager.drawText(0, 116, buf, true);
  }else{
    oled_manager.drawText(10, 60, "NO IMU DATA", true);
  }
  
  oled_manager.show();
}

void displayEnvironmentalPage(const SensorDataPayload& data){
  oled_manager.clear();
  oled_manager.drawText(0, 0, "=== ENVIRONMENT ===", true);
  
  if(data.getEnvValid()){
    char buf[32];
    oled_manager.drawText(0, 20, "Temperature:", true);
    snprintf(buf, sizeof(buf), "  %.2f C", data.temperature);
    oled_manager.drawText(0, 32, buf, true);
    
    oled_manager.drawText(0, 50, "Humidity:", true);
    snprintf(buf, sizeof(buf), "  %.1f %%", data.humidity);
    oled_manager.drawText(0, 62, buf, true);
    
    oled_manager.drawText(0, 80, "Pressure:", true);
    snprintf(buf, sizeof(buf), "  %.2f hPa", data.pressure / 100.0f);
    oled_manager.drawText(0, 92, buf, true);
  }else{
    oled_manager.drawText(10, 60, "NO ENV DATA", true);
  }
  
  oled_manager.show();
}

void displayGpsPage(const SensorDataPayload& data){
  oled_manager.clear();
  oled_manager.drawText(0, 0, "===== GPS DATA =====", true);
  
  if(data.getGpsValid()){
    char buf[32];
    oled_manager.drawText(0, 12, "Position:", true);
    snprintf(buf, sizeof(buf), " Lat:%.5f", data.latitude);
    oled_manager.drawText(0, 22, buf, true);
    snprintf(buf, sizeof(buf), " Lon:%.5f", data.longitude);
    oled_manager.drawText(0, 32, buf, true);
    snprintf(buf, sizeof(buf), " Alt:%.1fm", data.altitude);
    oled_manager.drawText(0, 42, buf, true);
    
    oled_manager.drawText(0, 54, "Navigation:", true);
    snprintf(buf, sizeof(buf), " Spd:%.1fkn", data.speed_knots);
    oled_manager.drawText(0, 64, buf, true);
    snprintf(buf, sizeof(buf), " Crs:%.1fdeg", data.course);
    oled_manager.drawText(0, 74, buf, true);
    
    oled_manager.drawText(0, 86, "Status:", true);
    snprintf(buf, sizeof(buf), " Sats:%u Fix:%u", data.gps_satellites, data.getGpsFixQuality());
    oled_manager.drawText(0, 96, buf, true);
    
    snprintf(buf, sizeof(buf), "Time: %02u:%02u:%02u", 
      data.gps_hour, data.gps_minute, data.gps_second);
    oled_manager.drawText(0, 108, buf, true);
  }else{
    oled_manager.drawText(10, 60, "NO GPS FIX", true);
  }
  
  oled_manager.show();
}

void displayMicrophonePage(const SensorDataPayload& data){
  oled_manager.clear();
  oled_manager.drawText(0, 0, "==== MIC DATA =====", true);
  
  if(data.getMicValid()){
    char buf[32];
    oled_manager.drawText(0, 12, "Level:", true);
    snprintf(buf, sizeof(buf), " %.1f dB", data.mic_db_level);
    oled_manager.drawText(42, 12, buf, true);
    
    if(data.getMicClipping()){
      oled_manager.drawText(90, 12, "[CLIP]", true);
    }
    
    oled_manager.drawText(0, 30, "Peak:", true);
    snprintf(buf, sizeof(buf), " %ld", data.mic_peak_amplitude);
    oled_manager.drawText(36, 30, buf, true);
    
    // Simple level bar
    int bar_width = static_cast<int>((data.mic_db_level + 60.0f) / 60.0f * 100.0f);
    if(bar_width < 0) bar_width = 0;
    if(bar_width > 100) bar_width = 100;
    
    oled_manager.drawRect(10, 50, 108, 20, false, true);
    oled_manager.fillRect(12, 52, bar_width, 16, true);
  }else{
    oled_manager.drawText(10, 60, "NO MIC DATA", true);
  }
  
  oled_manager.show();
}

void displaySystemPage(const SensorDataPayload& data){
  oled_manager.clear();
  oled_manager.drawText(0, 0, "==== SYSTEM INFO ====", true);
  
  char buf[32];
  oled_manager.drawText(0, 12, "Data Rate:", true);
  snprintf(buf, sizeof(buf), " RX:%lu TX:%lu FPS", stats.sensor_fps, stats.led_fps);
  oled_manager.drawText(0, 22, buf, true);
  
  oled_manager.drawText(0, 34, "Fan Speed:", true);
  snprintf(buf, sizeof(buf), " %u%%", (led_manager.getFanSpeed() * 100) / 255);
  oled_manager.drawText(0, 44, buf, true);
  
  oled_manager.drawText(0, 56, "Buttons:", true);
  snprintf(buf, sizeof(buf), " A:%u B:%u C:%u D:%u",
    data.getButtonA(), data.getButtonB(), data.getButtonC(), data.getButtonD());
  oled_manager.drawText(0, 66, buf, true);
  
  oled_manager.drawText(0, 78, "Sensors:", true);
  snprintf(buf, sizeof(buf), " IMU:%u ENV:%u",
    data.getImuValid(), data.getEnvValid());
  oled_manager.drawText(0, 88, buf, true);
  snprintf(buf, sizeof(buf), " GPS:%u MIC:%u",
    data.getGpsValidFlag(), data.getMicValid());
  oled_manager.drawText(0, 98, buf, true);
  
  snprintf(buf, sizeof(buf), "Pg %d/%d", current_page + 1, TOTAL_PAGES);
  oled_manager.drawText(90, 110, buf, true);
  
  oled_manager.show();
}

/**
 * @brief Handle page navigation
 */
void handlePageNavigation(const SensorDataPayload& data){
  bool button_a = data.getButtonA();
  bool button_b = data.getButtonB();
  
  if(button_a && !button_a_prev){
    current_page--;
    if(current_page < 0) current_page = TOTAL_PAGES - 1;
    ESP_LOGI(TAG, "Page: %d", current_page);
  }
  
  if(button_b && !button_b_prev){
    current_page++;
    if(current_page >= TOTAL_PAGES) current_page = 0;
    ESP_LOGI(TAG, "Page: %d", current_page);
  }
  
  button_a_prev = button_a;
  button_b_prev = button_b;
}

/**
 * @brief Display current page
 */
void displayCurrentPage(const SensorDataPayload& data){
  switch(current_page){
    case 0: displayImuPage(data); break;
    case 1: displayEnvironmentalPage(data); break;
    case 2: displayGpsPage(data); break;
    case 3: displayMicrophonePage(data); break;
    case 4: displaySystemPage(data); break;
    default: current_page = 0; displayImuPage(data); break;
  }
}

/**
 * @brief Core 0 Task: Receive UART data
 */
void uartReceiveTask(void* parameter){
  ESP_LOGI(TAG, "UART receive task started on Core 0");
  
  UartPacket packet;
  
  while(true){
    if(uart_comm.receivePacket(packet)){
      if(packet.message_type == MessageType::SENSOR_DATA){
        if(packet.payload_length == sizeof(SensorDataPayload)){
          if(xSemaphoreTake(sensor_data_mutex, pdMS_TO_TICKS(5)) == pdTRUE){
            memcpy(&current_sensor_data, packet.payload, sizeof(SensorDataPayload));
            data_received = true;
            last_data_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
            stats.sensor_frames_received++;
            
            // Transition to normal operation on first data
            if(boot_phase == BootPhase::WAIT_FOR_DATA){
              ESP_LOGI(TAG, "First sensor data received!");
              ESP_LOGI(TAG, "===== SETTING boot_phase TO NORMAL_OPERATION =====");
              boot_phase = BootPhase::NORMAL_OPERATION;
              ESP_LOGI(TAG, "boot_phase = %d (NORMAL_OPERATION)", (int)boot_phase);
              ESP_LOGI(TAG, "");
              ESP_LOGI(TAG, "========== BOOT COMPLETE ==========");
              ESP_LOGI(TAG, "");
            }
            
            xSemaphoreGive(sensor_data_mutex);
          }
        }
      }
    }
    
    vTaskDelay(1);
  }
}

/**
 * @brief LED Send Task - sends animations at 60 FPS
 */
void ledSendTask(void* parameter){
  ESP_LOGI(TAG, "LED send task started on Core 0");
  
  uint64_t next_frame_time = esp_timer_get_time();
  
  // Set initial animation
  led_manager.setCurrentAnimation("test_rainbow");
  
  while(true){
    uint64_t current_time = esp_timer_get_time();
    
    if(current_time >= next_frame_time){
      uint32_t time_ms = current_time / 1000;
      
      // Update current animation
      led_manager.updateCurrentAnimation(time_ms);
      
      // Smooth fan speed cycle (12-second cycle)
      uint32_t cycle_time_ms = time_ms % 12000;
      if(cycle_time_ms < 3000){
        led_manager.setFanSpeed((uint8_t)((cycle_time_ms * 255) / 3000));
      }else if(cycle_time_ms < 6000){
        led_manager.setFanSpeed(255);
      }else if(cycle_time_ms < 9000){
        uint32_t ramp_down = cycle_time_ms - 6000;
        led_manager.setFanSpeed((uint8_t)(255 - ((ramp_down * 255) / 3000)));
      }else{
        led_manager.setFanSpeed(0);
      }
      
      // Send via UART
      if(uart_comm.sendPacket(
        MessageType::LED_DATA,
        reinterpret_cast<uint8_t*>(&led_manager.getLedData()),
        sizeof(LedDataPayload)
      )){
        stats.led_frames_sent++;
      }
      
      next_frame_time += LED_FRAME_INTERVAL_US;
      
      if(current_time > next_frame_time + LED_FRAME_INTERVAL_US){
        next_frame_time = current_time;
      }
    }
    
    vTaskDelay(1);
  }
}

/**
 * @brief Core 1 Task: HUB75 display at 30Hz (DEDICATED - higher priority)
 * Note: 30Hz target is realistic given show() takes ~40ms. Higher priority (P3)
 * on dedicated Core 1 should improve performance without starving Core 0 tasks.
 */
void hub75UpdateTask(void* parameter){
  // Immediate log to verify task creation
  ets_printf("HUB75 task STARTING...\n");
  ESP_LOGI(TAG, "HUB75: Task function called");
  
  // Check which core we're running on
  int core_id = xPortGetCoreID();
  ESP_LOGI(TAG, "HUB75: Running on Core %d", core_id);
  
  vTaskDelay(pdMS_TO_TICKS(10));
  ESP_LOGI(TAG, "HUB75: Initial delay complete");
  
  ESP_LOGI(TAG, "HUB75 update task STARTED on Core 1");
  ESP_LOGI(TAG, "HUB75: Current boot_phase = %d", (int)boot_phase);
  
  // Wait for boot to complete
  ESP_LOGI(TAG, "HUB75 waiting for NORMAL_OPERATION phase...");
  uint32_t wait_count = 0;
  while(boot_phase != BootPhase::NORMAL_OPERATION){
    vTaskDelay(pdMS_TO_TICKS(100));
    wait_count++;
    if(wait_count % 10 == 0){
      ESP_LOGI(TAG, "HUB75: Still waiting... boot_phase=%d (waited %lu seconds)", 
        (int)boot_phase, wait_count / 10);
    }
  }
  
  ESP_LOGI(TAG, "HUB75: NORMAL_OPERATION detected! Waited %lu iterations", wait_count);
  ESP_LOGI(TAG, "HUB75 entering 30Hz rendering loop (balanced for dual-core)!");
  
  uint32_t hub75_anim_start = xTaskGetTickCount() * portTICK_PERIOD_MS;
  ESP_LOGI(TAG, "HUB75: Animation start time = %lu ms", hub75_anim_start);
  
  uint32_t last_frame_time = hub75_anim_start;
  constexpr uint32_t FRAME_INTERVAL_MS = 33;  // ~30Hz (33.33ms) - realistic target
  uint32_t frame_count = 0;
  
  ESP_LOGI(TAG, "HUB75: Entering main rendering loop NOW!");
  
  while(true){
    uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    uint32_t anim_time = current_time - hub75_anim_start;
    
    // Update HUB75 at 60Hz - RGB cycle (solid color fills)
    hub75_manager.executeAnimation("test_rgb_cycle", anim_time);
    hub75_manager.show();  // Call show() ONCE per frame in main loop
    
    stats.hub75_frames++;
    frame_count++;
    
    // Log every second
    if(frame_count >= 30){
      frame_count = 0;
    }
    
    // Maintain 30Hz frame rate
    last_frame_time += FRAME_INTERVAL_MS;
    uint32_t next_wake = last_frame_time;
    
    // Handle timing drift
    if(current_time > next_wake + FRAME_INTERVAL_MS){
      last_frame_time = current_time;
      next_wake = current_time;
    }
    
    int32_t delay_ms = static_cast<int32_t>(next_wake - current_time);
    if(delay_ms > 0){
      vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }else{
      vTaskDelay(1);  // Minimal delay to prevent watchdog
    }
  }
}

/**
 * @brief Core 0 Task: OLED display updates
 */
void oledUpdateTask(void* parameter){
  ESP_LOGI(TAG, "OLED update task started on Core 0");
  
  SensorDataPayload local_copy;
  bool have_data = false;
  
  while(true){
    uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    // Copy sensor data
    if(xSemaphoreTake(sensor_data_mutex, pdMS_TO_TICKS(5)) == pdTRUE){
      if(data_received){
        memcpy(&local_copy, &current_sensor_data, sizeof(SensorDataPayload));
        have_data = true;
      }
      xSemaphoreGive(sensor_data_mutex);
    }
    
    // Display based on boot phase
    if(boot_phase == BootPhase::NORMAL_OPERATION && have_data){
      // Normal operation
      handlePageNavigation(local_copy);
      displayCurrentPage(local_copy);
      
      stats.display_updates++;
    }else{
      // Boot/waiting phase
      oled_manager.clear();
      oled_manager.drawText(10, 50, "Waiting for", true);
      oled_manager.drawText(10, 62, "sensor data...", true);
      oled_manager.show();
    }
    
    // Statistics
    if(current_time - stats.last_report_time >= 1000){
      stats.sensor_fps = stats.sensor_frames_received;
      stats.led_fps = stats.led_frames_sent;
      stats.hub75_fps = stats.hub75_frames;
      
      if(boot_phase == BootPhase::NORMAL_OPERATION){
        ESP_LOGI(TAG, "Stats: RX:%lu | TX:%lu | HUB75:%lu | OLED:%lu fps | Page:%d",
          stats.sensor_fps, stats.led_fps, stats.hub75_fps, stats.display_updates, current_page);
      }
      
      stats.sensor_frames_received = 0;
      stats.led_frames_sent = 0;
      stats.display_updates = 0;
      stats.hub75_frames = 0;
      stats.last_report_time = current_time;
      
      // Cycle LED animation every 10 seconds
      static uint32_t last_anim_change = 0;
      if(current_time - last_anim_change >= 10000){
        led_manager.nextAnimation();
        last_anim_change = current_time;
        ESP_LOGI(TAG, "LED animation: %s", 
                 led_manager.getAnimationName(led_manager.getCurrentAnimationIndex()));
      }
    }
    
    // OLED updates as fast as possible (limited by I2C transfer time)
    vTaskDelay(pdMS_TO_TICKS(10));  // ~100Hz attempt, actual rate limited by I2C
  }
}

/**
 * @brief Application entry point
 */
extern "C" void app_main(void){
  // Watchdog disabled via sdkconfig.GPU_nowdt
  ESP_LOGI(TAG, "Starting GPU application (Watchdog disabled)");
  
  // Create mutex
  sensor_data_mutex = xSemaphoreCreateMutex();
  if(sensor_data_mutex == NULL){
    ESP_LOGE(TAG, "FATAL: Failed to create mutex!");
    return;
  }
  
  // Run boot sequence
  runBootSequence();
  
  if(!displays_initialized || !uart_initialized){
    ESP_LOGE(TAG, "FATAL: Boot sequence failed!");
    return;
  }
  
  ESP_LOGI(TAG, "Creating tasks...");
  
  TaskHandle_t uart_rx_handle = NULL;
  TaskHandle_t led_tx_handle = NULL;
  TaskHandle_t hub75_handle = NULL;
  TaskHandle_t oled_handle = NULL;
  
  // Check initial free heap
  size_t initial_heap = esp_get_free_heap_size();
  ESP_LOGI(TAG, "Initial free heap: %u bytes", initial_heap);
  
  // Core 0 tasks - UART, LED, and OLED (reduced stack sizes to conserve memory)
  xTaskCreatePinnedToCore(uartReceiveTask, "uart_rx", 4096, NULL, 2, &uart_rx_handle, 0);
  xTaskCreatePinnedToCore(ledSendTask, "led_tx", 4096, NULL, 2, &led_tx_handle, 0);
  xTaskCreatePinnedToCore(oledUpdateTask, "oled_disp", 4096, NULL, 2, &oled_handle, 0);
  
  // Check free heap before creating HUB75 task
  size_t free_heap = esp_get_free_heap_size();
  size_t min_free_heap = esp_get_minimum_free_heap_size();
  ESP_LOGI(TAG, "Free heap after Core 0 tasks: %u bytes (min was: %u bytes)", free_heap, min_free_heap);
  
  ESP_LOGI(TAG, "Creating HUB75 task on Core 1...");
  ESP_LOGI(TAG, "Current boot_phase before task creation: %d", (int)boot_phase);
  
  // Core 1 task - HUB75 dedicated (HIGHER priority P3 vs Core 0's P2) - Use 4KB stack
  BaseType_t result = xTaskCreatePinnedToCore(hub75UpdateTask, "hub75_60hz", 4096, NULL, 3, &hub75_handle, 1);
  
  if(result != pdPASS){
    ESP_LOGE(TAG, "FAILED TO CREATE HUB75 TASK! Error: %d", result);
    ESP_LOGE(TAG, "Free heap after failure: %u bytes", esp_get_free_heap_size());
  }else{
    ESP_LOGI(TAG, "HUB75 task creation returned success (Core 1), handle=%p", hub75_handle);
  }
  
  ESP_LOGI(TAG, "Waiting for tasks to start...");
  
  // Give tasks time to start
  vTaskDelay(pdMS_TO_TICKS(200));
  
  ESP_LOGI(TAG, "Task start delay complete");
  
  ESP_LOGI(TAG, "All tasks created!");
  ESP_LOGI(TAG, "Core 0: UART RX + LED TX @ 60Hz + OLED display (P2/P2/P2)");
  ESP_LOGI(TAG, "Core 1: HUB75 @ 30Hz target (P3 - HIGHER priority)");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "Controls: Button A = Prev Page | Button B = Next Page");
  ESP_LOGI(TAG, "");
  
  // Keep app_main alive to prevent task cleanup
  while(true){
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
