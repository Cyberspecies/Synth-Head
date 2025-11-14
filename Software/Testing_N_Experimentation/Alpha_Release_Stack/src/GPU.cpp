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
 *    - Button 1 (A): Set/Enter (not used in debug mode)
 *    - Button 2 (B): Navigate Up / Previous
 *    - Button 3 (C): Navigate Down / Next
 *    - Button 4 (D): Mode selector (hold to access menu)
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
#include "Manager/ImageSpriteLoader.hpp"

// Boot animations
#include "Animations/Boot/HUB75BootAnimations.hpp"
#include "Animations/Boot/OLEDBootAnimations.hpp"
#include "Animations/Boot/LEDBootAnimations.hpp"

// Test animations
#include "Animations/Test/HUB75TestAnimations.hpp"
#include "Animations/Test/LEDTestAnimations.hpp"

// UI System
#include "UI/ButtonManager.hpp"
#include "UI/Menu/MenuSystem.hpp"
#include "UI/Menu/MenuRenderer.hpp"

// UART communication
#include "Drivers/UART Comms/GpuUartBidirectional.hpp"
#include "Drivers/UART Comms/FileTransferManager.hpp"

using namespace arcos::communication;
using namespace arcos::manager;
using namespace arcos::animations;
using namespace arcos::ui;

static const char* TAG = "GPU_MAIN";

// Global stats for menu system (required by MenuRenderer)
namespace arcos::ui::menu {
  uint32_t g_sensor_fps = 0;
  uint32_t g_led_fps = 0;
  uint8_t g_fan_speed = 0;
}

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
FileTransferReceiver file_receiver;
ImageSpriteLoader sprite_loader;

// ============== Shared Data ==============
SemaphoreHandle_t sensor_data_mutex;
SensorDataPayload current_sensor_data;
bool data_received = false;
uint32_t last_data_time = 0;

// ============== Shader Parameters ==============
uint8_t shader_color1_r = 255, shader_color1_g = 0, shader_color1_b = 0;
uint8_t shader_color2_r = 0, shader_color2_g = 0, shader_color2_b = 255;
uint8_t shader_speed = 128;

// ============== UI System ==============
ButtonManager button_manager;
menu::MenuSystem menu_system;

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
  arcos::animations::oled::registerBootAnimations(oled_manager);
  
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
 * @brief Core 0 Task: Receive UART data (including file transfers)
 */
void uartReceiveTask(void* parameter){
  ESP_LOGI(TAG, "UART receive task started on Core 0");
  
  UartPacket packet;
  
  while(true){
    if(uart_comm.receivePacket(packet)){
      // Handle different message types
      switch(packet.message_type){
        case MessageType::SENSOR_DATA:
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
          break;
          
        case MessageType::FILE_TRANSFER_START:
          if(packet.payload_length == sizeof(FileTransferMetadata)){
            FileTransferMetadata metadata;
            memcpy(&metadata, packet.payload, sizeof(FileTransferMetadata));
            
            ESP_LOGI(TAG, "File transfer started:");
            ESP_LOGI(TAG, "  Filename: %s", metadata.filename);
            ESP_LOGI(TAG, "  Size: %lu bytes", metadata.total_size);
            ESP_LOGI(TAG, "  Fragments: %u", metadata.total_fragments);
            
            if(file_receiver.handleMetadata(metadata)){
              ESP_LOGI(TAG, "  Ready to receive file data");
            }else{
              ESP_LOGE(TAG, "  ERROR: Failed to initialize file receiver!");
            }
          }
          break;
          
        case MessageType::FILE_TRANSFER_DATA:
          if(packet.payload_length == sizeof(FileTransferFragment)){
            FileTransferFragment fragment;
            memcpy(&fragment, packet.payload, sizeof(FileTransferFragment));
            
            if(file_receiver.handleFragment(fragment)){
              // Fragment received successfully
              if((fragment.fragment_index + 1) % 10 == 0){  // Log every 10 fragments
                ESP_LOGI(TAG, "File RX: Fragment %u received (%.1f%%)", 
                        fragment.fragment_index + 1,
                        file_receiver.getProgress() * 100.0f);
              }
            }
          }
          break;
        
        case MessageType::FILE_TRANSFER_ACK:
          // ACKs are sent by GPU, received by CPU - not handled here
          break;
          
        case MessageType::DISPLAY_SETTINGS:
          if(packet.payload_length == sizeof(DisplaySettings)){
            DisplaySettings settings;
            memcpy(&settings, packet.payload, sizeof(DisplaySettings));
            
            ESP_LOGI(TAG, "Display settings received from CPU:");
            ESP_LOGI(TAG, "  Face: %u, Effect: %u, Shader: %u", 
                    settings.display_face, settings.display_effect, 
                    settings.display_shader);
            ESP_LOGI(TAG, "  Color1 RGB: (%u,%u,%u), Color2 RGB: (%u,%u,%u), Speed: %u",
                    settings.color1_r, settings.color1_g, settings.color1_b,
                    settings.color2_r, settings.color2_g, settings.color2_b,
                    settings.shader_speed);
            
            // Apply settings to menu system
            menu_system.setDisplayFace((menu::DisplayFace)settings.display_face);
            menu_system.setDisplayEffect((menu::DisplayEffect)settings.display_effect);
            menu_system.setDisplayShader((menu::DisplayShader)settings.display_shader);
            
            // Store shader parameters globally for use in rendering
            shader_color1_r = settings.color1_r;
            shader_color1_g = settings.color1_g;
            shader_color1_b = settings.color1_b;
            shader_color2_r = settings.color2_r;
            shader_color2_g = settings.color2_g;
            shader_color2_b = settings.color2_b;
            shader_speed = settings.shader_speed;
            
            ESP_LOGI(TAG, "Display settings applied successfully!");
          }
          break;
          
        case MessageType::LED_SETTINGS:
          if(packet.payload_length == sizeof(LedSettings)){
            LedSettings settings;
            memcpy(&settings, packet.payload, sizeof(LedSettings));
            
            ESP_LOGI(TAG, "LED settings received from CPU:");
            ESP_LOGI(TAG, "  Mode: %u, Speed: %u, Brightness: %u", 
                    settings.led_strip_mode, settings.speed, settings.brightness);
            ESP_LOGI(TAG, "  Color1 RGB: (%u,%u,%u), Color2 RGB: (%u,%u,%u)",
                    settings.color1_r, settings.color1_g, settings.color1_b,
                    settings.color2_r, settings.color2_g, settings.color2_b);
            
            // Apply LED mode to menu system
            menu_system.setLedStripMode((menu::LedStripMode)settings.led_strip_mode);
            
            // TODO: Store LED parameters globally for LED rendering
            // (brightness, colors, speed should be used by LED manager)
            
            ESP_LOGI(TAG, "LED settings applied successfully!");
          }
          break;
          
        default:
          // Ignore unknown message types
          break;
      }
    }
    
    vTaskDelay(1);
  }
}

/**
 * @brief LED Send Task - sends animations at 60 FPS (controlled by menu)
 */
void ledSendTask(void* parameter){
  ESP_LOGI(TAG, "LED send task started on Core 0");
  
  uint64_t next_frame_time = esp_timer_get_time();
  
  // Animation names for each LED mode
  const char* led_animation_names[] = {
    "test_rainbow",       // DYNAMIC_DISPLAY (placeholder, will be implemented)
    "test_rainbow",       // RAINBOW
    "test_breathing",     // BREATHING
    "test_wave",          // WAVE
    "test_fire",          // FIRE
    "test_theater_chase"  // THEATER_CHASE
  };
  
  menu::LedStripMode last_mode = menu::LedStripMode::RAINBOW;
  led_manager.setCurrentAnimation(led_animation_names[static_cast<uint8_t>(last_mode)]);
  
  while(true){
    uint64_t current_time = esp_timer_get_time();
    
    if(current_time >= next_frame_time){
      uint32_t time_ms = current_time / 1000;
      
      // Get current LED mode from menu system
      menu::LedStripMode current_mode = menu_system.getLedStripMode();
      
      // Switch animation if mode changed
      if(current_mode != last_mode){
        led_manager.setCurrentAnimation(led_animation_names[static_cast<uint8_t>(current_mode)]);
        last_mode = current_mode;
        ESP_LOGI(TAG, "LED mode changed to: %s", led_animation_names[static_cast<uint8_t>(current_mode)]);
      }
      
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
  ESP_LOGI(TAG, "HUB75 entering 30Hz rendering loop (menu-controlled)!");
  
  uint32_t hub75_anim_start = xTaskGetTickCount() * portTICK_PERIOD_MS;
  ESP_LOGI(TAG, "HUB75: Animation start time = %lu ms", hub75_anim_start);
  
  uint32_t last_frame_time = hub75_anim_start;
  constexpr uint32_t FRAME_INTERVAL_MS = 33;  // ~30Hz (33.33ms) - realistic target
  uint32_t frame_count = 0;
  
  ESP_LOGI(TAG, "HUB75: Entering main rendering loop NOW!");
  
  while(true){
    uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    uint32_t anim_time = current_time - hub75_anim_start;
    
    // Get current display settings from menu
    menu::DisplayFace face = menu_system.getDisplayFace();
    menu::DisplayEffect effect = menu_system.getDisplayEffect();
    menu::DisplayShader shader = menu_system.getDisplayShader();
    
    // Clear display
    hub75_manager.clear();
    
    // Draw selected face on BOTH panels independently
    // Panel 0 (left display): x = 0-63, center at (32, 16)
    // Panel 1 (right display): x = 64-127, center at (96, 16)
    constexpr int cy = 16;    // Both panels center Y
    
    switch(face){
      case menu::DisplayFace::CUSTOM_IMAGE:
        // Display custom image loaded from file transfer
        if(sprite_loader.isLoaded()){
          // Render sprite on both panels (centered at x=32, y=16 and x=96, y=16)
          sprite_loader.renderOnBothPanels(hub75_manager);
        }else{
          // No image loaded - show "NO IMAGE" message
          // Draw text indicator on left panel
          hub75_manager.drawLine(15, 14, 48, 14, RGB(255, 0, 0));
          hub75_manager.drawLine(15, 16, 48, 16, RGB(255, 0, 0));
          hub75_manager.drawLine(15, 18, 48, 18, RGB(255, 0, 0));
          
          // Draw text indicator on right panel
          hub75_manager.drawLine(79, 14, 112, 14, RGB(255, 0, 0));
          hub75_manager.drawLine(79, 16, 112, 16, RGB(255, 0, 0));
          hub75_manager.drawLine(79, 18, 112, 18, RGB(255, 0, 0));
        }
        break;
      case menu::DisplayFace::PANEL_NUMBER:
        // Show panel numbers for dual panel setup (left=0, right=1)
        // Draw large "0" on left panel (pixels 0-63)
        // Vertical lines
        for(int y = 8; y < 24; y++){
          hub75_manager.setPixel(20, y, RGB(255, 255, 255));
          hub75_manager.setPixel(21, y, RGB(255, 255, 255));
          hub75_manager.setPixel(40, y, RGB(255, 255, 255));
          hub75_manager.setPixel(41, y, RGB(255, 255, 255));
        }
        // Horizontal lines top
        for(int x = 20; x <= 41; x++){
          hub75_manager.setPixel(x, 8, RGB(255, 255, 255));
          hub75_manager.setPixel(x, 9, RGB(255, 255, 255));
        }
        // Horizontal lines bottom
        for(int x = 20; x <= 41; x++){
          hub75_manager.setPixel(x, 22, RGB(255, 255, 255));
          hub75_manager.setPixel(x, 23, RGB(255, 255, 255));
        }
        
        // Draw large "1" on right panel (pixels 64-127)
        // Vertical line
        for(int y = 8; y < 24; y++){
          hub75_manager.setPixel(94, y, RGB(255, 255, 255));
          hub75_manager.setPixel(95, y, RGB(255, 255, 255));
        }
        // Top diagonal
        hub75_manager.setPixel(90, 10, RGB(255, 255, 255));
        hub75_manager.setPixel(91, 10, RGB(255, 255, 255));
        hub75_manager.setPixel(91, 9, RGB(255, 255, 255));
        hub75_manager.setPixel(92, 9, RGB(255, 255, 255));
        hub75_manager.setPixel(92, 8, RGB(255, 255, 255));
        hub75_manager.setPixel(93, 8, RGB(255, 255, 255));
        break;
      case menu::DisplayFace::ORIENTATION:
        // Show orientation arrows: UP and FORWARD on BOTH panels
        // PANEL 0 (left display)
        // UP arrow - Yellow on left side
        hub75_manager.drawLine(20, 24, 20, 8, RGB(255, 255, 0)); // Vertical shaft
        hub75_manager.drawLine(20, 8, 16, 12, RGB(255, 255, 0)); // Left arrowhead
        hub75_manager.drawLine(20, 8, 24, 12, RGB(255, 255, 0)); // Right arrowhead
        // FORWARD arrow - Cyan on right side
        hub75_manager.drawLine(35, cy, 50, cy, RGB(0, 255, 255)); // Horizontal shaft
        hub75_manager.drawLine(50, cy, 46, cy - 3, RGB(0, 255, 255)); // Top arrowhead
        hub75_manager.drawLine(50, cy, 46, cy + 3, RGB(0, 255, 255)); // Bottom arrowhead
        
        // PANEL 1 (right display)
        // UP arrow - Yellow on left side
        hub75_manager.drawLine(84, 24, 84, 8, RGB(255, 255, 0)); // Vertical shaft
        hub75_manager.drawLine(84, 8, 80, 12, RGB(255, 255, 0)); // Left arrowhead
        hub75_manager.drawLine(84, 8, 88, 12, RGB(255, 255, 0)); // Right arrowhead
        // FORWARD arrow - Cyan on right side
        hub75_manager.drawLine(99, cy, 114, cy, RGB(0, 255, 255)); // Horizontal shaft
        hub75_manager.drawLine(114, cy, 110, cy - 3, RGB(0, 255, 255)); // Top arrowhead
        hub75_manager.drawLine(114, cy, 110, cy + 3, RGB(0, 255, 255)); // Bottom arrowhead
        break;
      default:
        break;
    }
    
    // Apply effects overlay
    switch(effect){
      case menu::DisplayEffect::WAVE:
        // Animated wave lines across display
        for(int x = 0; x < 128; x += 4){
          int wave_y = cy + static_cast<int>(6 * sinf((anim_time / 200.0f) + (x / 10.0f)));
          if(wave_y >= 0 && wave_y < 32){
            hub75_manager.setPixel(x, wave_y, RGB(100, 100, 255));
            if(wave_y + 1 < 32) hub75_manager.setPixel(x, wave_y + 1, RGB(80, 80, 200));
          }
        }
        break;
      case menu::DisplayEffect::GRID:
        // Draw grid overlay
        for(int x = 0; x < 128; x += 16){
          hub75_manager.drawLine(x, 0, x, 31, RGB(50, 50, 50));
        }
        for(int y = 0; y < 32; y += 8){
          hub75_manager.drawLine(0, y, 127, y, RGB(50, 50, 50));
        }
        break;
      case menu::DisplayEffect::PARTICLES:
        // Simple particle effect - random dots
        for(int i = 0; i < 20; i++){
          int px = (anim_time * 3 + i * 17) % 128;
          int py = (anim_time * 2 + i * 13) % 32;
          hub75_manager.setPixel(px, py, RGB(255, 200, 100));
        }
        break;
      case menu::DisplayEffect::TRAILS:
        // Trailing dots moving across screen
        for(int i = 0; i < 5; i++){
          int tx = (anim_time / 10 + i * 25) % 128;
          int ty = 4 + i * 6;
          for(int trail = 0; trail < 5; trail++){
            int trail_x = tx - trail * 3;
            if(trail_x >= 0 && trail_x < 128){
              uint8_t brightness = 255 - trail * 50;
              hub75_manager.setPixel(trail_x, ty, RGB(brightness, brightness / 2, 0));
            }
          }
        }
        break;
      case menu::DisplayEffect::NONE:
      default:
        break;
    }
    
    // Apply shader effects (fragment shaders - applied ONLY to sprite pixels)
    // Note: All shaders process the sprite image, not the entire display
    // Non-sprite display faces (PANEL_NUMBER, ORIENTATION) are not affected by shaders
    switch(shader){
      case menu::DisplayShader::NONE:
        // No shader applied
        break;
        
      case menu::DisplayShader::HUE_CYCLE_SPRITE:
        // Hue cycling on sprite with row offset (black = transparent)
        // Only works with custom sprite loaded
        if(face == menu::DisplayFace::CUSTOM_IMAGE && sprite_loader.isLoaded()){
          // Clear and redraw sprite with hue transformation
          hub75_manager.clear();
          
          // Get sprite info for reprocessing
          const uint8_t* sprite_data = sprite_loader.getData();
          uint16_t sprite_width = sprite_loader.getWidth();
          uint16_t sprite_height = sprite_loader.getHeight();
          
          // Calculate hue offset based on time and speed
          float hue_base = (anim_time * shader_speed / 5000.0f);
          
          // Render on both panels with hue cycling
          for(int panel = 0; panel < 2; panel++){
            int panel_cx = panel == 0 ? 32 : 96;
            int panel_cy = 16;
            int start_x = panel_cx - sprite_width / 2;
            int start_y = panel_cy - sprite_height / 2;
            
            for(int y = 0; y < sprite_height; y++){
              // Hue offset per row
              float row_hue = hue_base + (y * 15.0f); // 15 degree offset per row
              row_hue = fmodf(row_hue, 360.0f);
              
              for(int x = 0; x < sprite_width; x++){
                int pixel_idx = (y * sprite_width + x) * 3;
                uint8_t r = sprite_data[pixel_idx];
                uint8_t g = sprite_data[pixel_idx + 1];
                uint8_t b = sprite_data[pixel_idx + 2];
                
                // Skip black pixels (transparent)
                if(r < 10 && g < 10 && b < 10) continue;
                
                // Convert to HSV, modify hue, convert back
                float hue, sat, val;
                // Simple RGB to HSV
                float rf = r / 255.0f, gf = g / 255.0f, bf = b / 255.0f;
                float max_val = fmaxf(fmaxf(rf, gf), bf);
                float min_val = fminf(fminf(rf, gf), bf);
                float delta = max_val - min_val;
                
                val = max_val;
                sat = (max_val > 0.0f) ? (delta / max_val) : 0.0f;
                
                if(delta > 0.0f){
                  if(max_val == rf) hue = 60.0f * fmodf((gf - bf) / delta, 6.0f);
                  else if(max_val == gf) hue = 60.0f * ((bf - rf) / delta + 2.0f);
                  else hue = 60.0f * ((rf - gf) / delta + 4.0f);
                  if(hue < 0.0f) hue += 360.0f;
                }else{
                  hue = 0.0f;
                }
                
                // Apply row hue offset
                hue = fmodf(hue + row_hue, 360.0f);
                
                // HSV back to RGB
                float c = val * sat;
                float x_val = c * (1.0f - fabsf(fmodf(hue / 60.0f, 2.0f) - 1.0f));
                float m = val - c;
                float r1, g1, b1;
                
                if(hue < 60.0f){ r1 = c; g1 = x_val; b1 = 0; }
                else if(hue < 120.0f){ r1 = x_val; g1 = c; b1 = 0; }
                else if(hue < 180.0f){ r1 = 0; g1 = c; b1 = x_val; }
                else if(hue < 240.0f){ r1 = 0; g1 = x_val; b1 = c; }
                else if(hue < 300.0f){ r1 = x_val; g1 = 0; b1 = c; }
                else{ r1 = c; g1 = 0; b1 = x_val; }
                
                uint8_t new_r = (uint8_t)((r1 + m) * 255.0f);
                uint8_t new_g = (uint8_t)((g1 + m) * 255.0f);
                uint8_t new_b = (uint8_t)((b1 + m) * 255.0f);
                
                int px = start_x + x;
                int py = start_y + y;
                if(px >= 0 && px < 128 && py >= 0 && py < 32){
                  hub75_manager.setPixel(px, py, RGB(new_r, new_g, new_b));
                }
              }
            }
          }
        }
        break;
        
      case menu::DisplayShader::HUE_CYCLE_OVERRIDE:
        // Override sprite colors with cycling hue (rainbow wave)
        // Only affects sprite pixels
        if(face == menu::DisplayFace::CUSTOM_IMAGE && sprite_loader.isLoaded()){
          hub75_manager.clear();
          
          const uint8_t* sprite_data = sprite_loader.getData();
          uint16_t sprite_width = sprite_loader.getWidth();
          uint16_t sprite_height = sprite_loader.getHeight();
          
          float hue_base = (anim_time * shader_speed / 5000.0f);
          
          for(int panel = 0; panel < 2; panel++){
            int panel_cx = panel == 0 ? 32 : 96;
            int panel_cy = 16;
            int start_x = panel_cx - sprite_width / 2;
            int start_y = panel_cy - sprite_height / 2;
            
            for(int y = 0; y < sprite_height; y++){
              for(int x = 0; x < sprite_width; x++){
                int pixel_idx = (y * sprite_width + x) * 3;
                uint8_t r = sprite_data[pixel_idx];
                uint8_t g = sprite_data[pixel_idx + 1];
                uint8_t b = sprite_data[pixel_idx + 2];
                
                // Skip black pixels (transparent)
                if(r < 10 && g < 10 && b < 10) continue;
                
                // Hue varies across X axis
                float hue = fmodf(hue_base + (x * 2.8f), 360.0f);
                
                // HSV to RGB with full saturation and value
                float c = 1.0f;
                float x_val = c * (1.0f - fabsf(fmodf(hue / 60.0f, 2.0f) - 1.0f));
                float rf, gf, bf;
                
                if(hue < 60.0f){ rf = c; gf = x_val; bf = 0; }
                else if(hue < 120.0f){ rf = x_val; gf = c; bf = 0; }
                else if(hue < 180.0f){ rf = 0; gf = c; bf = x_val; }
                else if(hue < 240.0f){ rf = 0; gf = x_val; bf = c; }
                else if(hue < 300.0f){ rf = x_val; gf = 0; bf = c; }
                else{ rf = c; gf = 0; bf = x_val; }
                
                int px = start_x + x;
                int py = start_y + y;
                if(px >= 0 && px < 128 && py >= 0 && py < 32){
                  hub75_manager.setPixel(px, py, RGB(
                    (uint8_t)(rf * 255.0f),
                    (uint8_t)(gf * 255.0f),
                    (uint8_t)(bf * 255.0f)
                  ));
                }
              }
            }
          }
        }
        break;
        
      case menu::DisplayShader::COLOR_OVERRIDE_STATIC:
        // Fill sprite with static color
        if(face == menu::DisplayFace::CUSTOM_IMAGE && sprite_loader.isLoaded()){
          hub75_manager.clear();
          
          const uint8_t* sprite_data = sprite_loader.getData();
          uint16_t sprite_width = sprite_loader.getWidth();
          uint16_t sprite_height = sprite_loader.getHeight();
          
          for(int panel = 0; panel < 2; panel++){
            int panel_cx = panel == 0 ? 32 : 96;
            int panel_cy = 16;
            int start_x = panel_cx - sprite_width / 2;
            int start_y = panel_cy - sprite_height / 2;
            
            for(int y = 0; y < sprite_height; y++){
              for(int x = 0; x < sprite_width; x++){
                int pixel_idx = (y * sprite_width + x) * 3;
                uint8_t r = sprite_data[pixel_idx];
                uint8_t g = sprite_data[pixel_idx + 1];
                uint8_t b = sprite_data[pixel_idx + 2];
                
                // Skip black pixels (transparent)
                if(r < 10 && g < 10 && b < 10) continue;
                
                int px = start_x + x;
                int py = start_y + y;
                if(px >= 0 && px < 128 && py >= 0 && py < 32){
                  hub75_manager.setPixel(px, py, RGB(shader_color1_r, shader_color1_g, shader_color1_b));
                }
              }
            }
          }
        }
        break;
        
      case menu::DisplayShader::COLOR_OVERRIDE_BREATHE:
        // Breathing animation between two colors on sprite
        if(face == menu::DisplayFace::CUSTOM_IMAGE && sprite_loader.isLoaded()){
          hub75_manager.clear();
          
          const uint8_t* sprite_data = sprite_loader.getData();
          uint16_t sprite_width = sprite_loader.getWidth();
          uint16_t sprite_height = sprite_loader.getHeight();
          
          float breath = (sinf(anim_time * shader_speed / 10000.0f) + 1.0f) / 2.0f; // 0.0 to 1.0
          
          uint8_t r = shader_color1_r + (shader_color2_r - shader_color1_r) * breath;
          uint8_t g = shader_color1_g + (shader_color2_g - shader_color1_g) * breath;
          uint8_t b = shader_color1_b + (shader_color2_b - shader_color1_b) * breath;
          
          for(int panel = 0; panel < 2; panel++){
            int panel_cx = panel == 0 ? 32 : 96;
            int panel_cy = 16;
            int start_x = panel_cx - sprite_width / 2;
            int start_y = panel_cy - sprite_height / 2;
            
            for(int y = 0; y < sprite_height; y++){
              for(int x = 0; x < sprite_width; x++){
                int pixel_idx = (y * sprite_width + x) * 3;
                uint8_t sr = sprite_data[pixel_idx];
                uint8_t sg = sprite_data[pixel_idx + 1];
                uint8_t sb = sprite_data[pixel_idx + 2];
                
                // Skip black pixels (transparent)
                if(sr < 10 && sg < 10 && sb < 10) continue;
                
                int px = start_x + x;
                int py = start_y + y;
                if(px >= 0 && px < 128 && py >= 0 && py < 32){
                  hub75_manager.setPixel(px, py, RGB(r, g, b));
                }
              }
            }
          }
        }
        break;
        
      case menu::DisplayShader::RGB_SPLIT:
        // Chromatic aberration effect - offset RGB channels on sprite
        if(face == menu::DisplayFace::CUSTOM_IMAGE && sprite_loader.isLoaded()){
          hub75_manager.clear();
          
          const uint8_t* sprite_data = sprite_loader.getData();
          uint16_t sprite_width = sprite_loader.getWidth();
          uint16_t sprite_height = sprite_loader.getHeight();
          
          for(int panel = 0; panel < 2; panel++){
            int panel_cx = panel == 0 ? 32 : 96;
            int panel_cy = 16;
            int start_x = panel_cx - sprite_width / 2;
            int start_y = panel_cy - sprite_height / 2;
            
            // Draw red channel shifted left
            for(int y = 0; y < sprite_height; y++){
              for(int x = 0; x < sprite_width; x++){
                int pixel_idx = (y * sprite_width + x) * 3;
                uint8_t r = sprite_data[pixel_idx];
                if(r < 10) continue;
                
                int px = start_x + x - 1; // Shift left
                int py = start_y + y;
                if(px >= 0 && px < 128 && py >= 0 && py < 32){
                  hub75_manager.setPixel(px, py, RGB(r, 0, 0));
                }
              }
            }
            
            // Draw green channel (no shift)
            for(int y = 0; y < sprite_height; y++){
              for(int x = 0; x < sprite_width; x++){
                int pixel_idx = (y * sprite_width + x) * 3;
                uint8_t g = sprite_data[pixel_idx + 1];
                if(g < 10) continue;
                
                int px = start_x + x;
                int py = start_y + y;
                if(px >= 0 && px < 128 && py >= 0 && py < 32){
                  hub75_manager.setPixel(px, py, RGB(0, g, 0));
                }
              }
            }
            
            // Draw blue channel shifted right
            for(int y = 0; y < sprite_height; y++){
              for(int x = 0; x < sprite_width; x++){
                int pixel_idx = (y * sprite_width + x) * 3;
                uint8_t b = sprite_data[pixel_idx + 2];
                if(b < 10) continue;
                
                int px = start_x + x + 1; // Shift right
                int py = start_y + y;
                if(px >= 0 && px < 128 && py >= 0 && py < 32){
                  hub75_manager.setPixel(px, py, RGB(0, 0, b));
                }
              }
            }
          }
        }
        break;
        
      case menu::DisplayShader::SCANLINES:
        // Darken every other line for CRT effect on sprite
        if(face == menu::DisplayFace::CUSTOM_IMAGE && sprite_loader.isLoaded()){
          uint16_t sprite_width = sprite_loader.getWidth();
          uint16_t sprite_height = sprite_loader.getHeight();
          
          for(int panel = 0; panel < 2; panel++){
            int panel_cx = panel == 0 ? 32 : 96;
            int panel_cy = 16;
            int start_x = panel_cx - sprite_width / 2;
            int start_y = panel_cy - sprite_height / 2;
            
            // Darken every other row
            for(int y = 0; y < sprite_height; y += 2){
              for(int x = 0; x < sprite_width; x++){
                int px = start_x + x;
                int py = start_y + y;
                if(px >= 0 && px < 128 && py >= 0 && py < 32){
                  hub75_manager.setPixel(px, py, RGB(0, 0, 0));
                }
              }
            }
          }
        }
        break;
        
      case menu::DisplayShader::PIXELATE:
        // Pixelation effect - blocky sprite rendering
        if(face == menu::DisplayFace::CUSTOM_IMAGE && sprite_loader.isLoaded()){
          hub75_manager.clear();
          
          const uint8_t* sprite_data = sprite_loader.getData();
          uint16_t sprite_width = sprite_loader.getWidth();
          uint16_t sprite_height = sprite_loader.getHeight();
          
          constexpr int block_size = 3;
          
          for(int panel = 0; panel < 2; panel++){
            int panel_cx = panel == 0 ? 32 : 96;
            int panel_cy = 16;
            int start_x = panel_cx - sprite_width / 2;
            int start_y = panel_cy - sprite_height / 2;
            
            for(int by = 0; by < sprite_height; by += block_size){
              for(int bx = 0; bx < sprite_width; bx += block_size){
                // Sample center pixel of block
                int sample_x = bx + block_size / 2;
                int sample_y = by + block_size / 2;
                if(sample_x >= sprite_width) sample_x = sprite_width - 1;
                if(sample_y >= sprite_height) sample_y = sprite_height - 1;
                
                int pixel_idx = (sample_y * sprite_width + sample_x) * 3;
                uint8_t r = sprite_data[pixel_idx];
                uint8_t g = sprite_data[pixel_idx + 1];
                uint8_t b = sprite_data[pixel_idx + 2];
                
                if(r < 10 && g < 10 && b < 10) continue;
                
                // Fill block with sampled color
                for(int dy = 0; dy < block_size; dy++){
                  for(int dx = 0; dx < block_size; dx++){
                    int px = start_x + bx + dx;
                    int py = start_y + by + dy;
                    if(px >= 0 && px < 128 && py >= 0 && py < 32 && 
                       (bx + dx) < sprite_width && (by + dy) < sprite_height){
                      hub75_manager.setPixel(px, py, RGB(r, g, b));
                    }
                  }
                }
              }
            }
          }
        }
        break;
        
      case menu::DisplayShader::INVERT:
        // Color inversion on sprite
        if(face == menu::DisplayFace::CUSTOM_IMAGE && sprite_loader.isLoaded()){
          hub75_manager.clear();
          
          const uint8_t* sprite_data = sprite_loader.getData();
          uint16_t sprite_width = sprite_loader.getWidth();
          uint16_t sprite_height = sprite_loader.getHeight();
          
          for(int panel = 0; panel < 2; panel++){
            int panel_cx = panel == 0 ? 32 : 96;
            int panel_cy = 16;
            int start_x = panel_cx - sprite_width / 2;
            int start_y = panel_cy - sprite_height / 2;
            
            for(int y = 0; y < sprite_height; y++){
              for(int x = 0; x < sprite_width; x++){
                int pixel_idx = (y * sprite_width + x) * 3;
                uint8_t r = sprite_data[pixel_idx];
                uint8_t g = sprite_data[pixel_idx + 1];
                uint8_t b = sprite_data[pixel_idx + 2];
                
                if(r < 10 && g < 10 && b < 10) continue;
                
                int px = start_x + x;
                int py = start_y + y;
                if(px >= 0 && px < 128 && py >= 0 && py < 32){
                  hub75_manager.setPixel(px, py, RGB(255 - r, 255 - g, 255 - b));
                }
              }
            }
          }
        }
        break;
        
      case menu::DisplayShader::DITHER:
        // Dither pattern on sprite
        if(face == menu::DisplayFace::CUSTOM_IMAGE && sprite_loader.isLoaded()){
          uint16_t sprite_width = sprite_loader.getWidth();
          uint16_t sprite_height = sprite_loader.getHeight();
          
          for(int panel = 0; panel < 2; panel++){
            int panel_cx = panel == 0 ? 32 : 96;
            int panel_cy = 16;
            int start_x = panel_cx - sprite_width / 2;
            int start_y = panel_cy - sprite_height / 2;
            
            for(int y = 0; y < sprite_height; y++){
              for(int x = 0; x < sprite_width; x++){
                int px = start_x + x;
                int py = start_y + y;
                
                // Dither every other pixel
                if((x + y) % 2 == 0 && px >= 0 && px < 128 && py >= 0 && py < 32){
                  hub75_manager.setPixel(px, py, RGB(0, 0, 0));
                }
              }
            }
          }
        }
        break;
        
      default:
        break;
    }
    
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
 * @brief Core 0 Task: OLED display updates with menu system
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
      // Update button manager
      button_manager.update(local_copy, current_time);
      
      // Update menu system
      menu_system.update(button_manager, local_copy, current_time);
      
      // Update global stats for menu system
      menu::g_sensor_fps = stats.sensor_fps;
      menu::g_led_fps = stats.led_fps;
      menu::g_fan_speed = led_manager.getFanSpeed();
      
      // Render current menu state
      menu_system.render(oled_manager);
      
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
        const char* mode_names[] = {"SCREENSAVER", "IDLE_GPS", "DEBUG", "FACES", "EFFECTS", "SHADERS", "LED_CFG"};
        ESP_LOGI(TAG, "Stats: RX:%lu | TX:%lu | HUB75:%lu | OLED:%lu fps | Mode:%s",
          stats.sensor_fps, stats.led_fps, stats.hub75_fps, stats.display_updates, 
          mode_names[static_cast<uint8_t>(menu_system.getCurrentMode())]);
      }
      
      stats.sensor_frames_received = 0;
      stats.led_frames_sent = 0;
      stats.display_updates = 0;
      stats.hub75_frames = 0;
      stats.last_report_time = current_time;
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
  
  // Initialize file receiver
  ESP_LOGI(TAG, "Initializing file transfer receiver...");
  file_receiver.init(&uart_comm);
  
  // Setup file receive callback
  file_receiver.setReceiveCallback([](uint32_t file_id, const uint8_t* data, uint32_t size){
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "========================================================");
    ESP_LOGI(TAG, "  File Transfer Completed!");
    ESP_LOGI(TAG, "========================================================");
    ESP_LOGI(TAG, "  File ID: 0x%08lX", file_id);
    ESP_LOGI(TAG, "  Size: %lu bytes", size);
    
    // Clear previous sprite before loading new one
    sprite_loader.clearImage();
    
    // Try to load as custom sprite image
    if(sprite_loader.loadImage(data, size)){
      ESP_LOGI(TAG, "  Custom sprite loaded successfully!");
      ESP_LOGI(TAG, "  Sprite dimensions: %dx%d", sprite_loader.getWidth(), sprite_loader.getHeight());
      ESP_LOGI(TAG, "  Select 'CUSTOM_IMAGE' in Display Faces menu to view");
    }else{
      ESP_LOGW(TAG, "  Failed to load as sprite image");
      ESP_LOGW(TAG, "  Expected format: 2-byte width, 2-byte height, RGB pixel data");
      
      // Show sample of received data for debugging
      ESP_LOGI(TAG, "  First 16 bytes:");
      for(int i = 0; i < 16 && i < size; i++){
        ets_printf("    [%d]: 0x%02X\n", i, data[i]);
      }
    }
    
    ESP_LOGI(TAG, "========================================================");
    ESP_LOGI(TAG, "");
  });
  
  ESP_LOGI(TAG, "File transfer receiver initialized");
  
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
