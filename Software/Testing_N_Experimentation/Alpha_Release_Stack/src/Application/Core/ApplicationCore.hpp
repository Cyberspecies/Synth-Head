/*****************************************************************
 * File:      ApplicationCore.hpp
 * Category:  Application/Core
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Main dual-core application orchestrator for Synth-Head.
 *    Manages core allocation:
 *    - Core 0: General tasks (sensors, network, input, web server)
 *    - Core 1: GPU pipeline (animation compositing, GPU commands)
 * 
 *    Provides clean interfaces between cores using thread-safe
 *    synchronization primitives.
 *****************************************************************/

#ifndef ARCOS_APPLICATION_CORE_HPP_
#define ARCOS_APPLICATION_CORE_HPP_

#include <stdint.h>
#include <cstring>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_timer.h"

namespace Application {

// ============================================================
// Core Assignment Constants
// ============================================================

constexpr int CORE_GENERAL = 0;      // Sensors, network, web, input
constexpr int CORE_GPU = 1;          // Animation compositing, GPU commands

constexpr int PRIORITY_GENERAL_HIGH = 5;
constexpr int PRIORITY_GENERAL_MED = 3;
constexpr int PRIORITY_GPU_PIPELINE = 6;  // Higher priority for smooth rendering
constexpr int PRIORITY_LOW = 1;

// Stack sizes (words, not bytes)
constexpr int STACK_SIZE_GPU = 8192;
constexpr int STACK_SIZE_SENSOR = 4096;
constexpr int STACK_SIZE_NETWORK = 8192;
constexpr int STACK_SIZE_INPUT = 2048;

// ============================================================
// Application State (shared between cores)
// ============================================================

struct ApplicationState {
  // Animation parameters (written by Core 0, read by Core 1)
  float lookX = 0.0f;           // Eye X position (-1.0 to 1.0)
  float lookY = 0.0f;           // Eye Y position (-1.0 to 1.0)
  float blinkProgress = 0.0f;   // 0 = open, 1 = closed
  uint8_t expressionId = 0;     // Current expression/eye set
  float emotionBlend = 0.0f;    // Blend factor for expression transition
  
  // Shader parameters
  uint8_t shaderType = 0;       // 0=solid, 1=rainbow, 2=gradient, 3=pulse
  float shaderSpeed = 1.0f;
  uint8_t brightness = 80;
  
  // Colors (RGB888)
  uint8_t primaryR = 255;
  uint8_t primaryG = 255;
  uint8_t primaryB = 255;
  uint8_t secondaryR = 0;
  uint8_t secondaryG = 0;
  uint8_t secondaryB = 255;
  
  // System state
  bool autoBlinkEnabled = true;
  uint32_t autoBlinkIntervalMs = 3000;
  bool mirrorMode = true;
  
  // Frame timing
  uint32_t targetFps = 60;
  
  // Flags
  bool requestUpdate = false;   // Core 0 sets this to request render update
  bool gpuReady = false;        // Core 1 sets when GPU is initialized
};

// ============================================================
// Core Statistics
// ============================================================

struct CoreStats {
  uint32_t framesRendered = 0;
  float currentFps = 0.0f;
  uint32_t maxFrameTimeUs = 0;
  uint32_t avgFrameTimeUs = 0;
  uint32_t droppedFrames = 0;
  uint32_t freeStackWords = 0;
};

// ============================================================
// Application Core Controller
// ============================================================

class ApplicationCore {
public:
  static constexpr const char* TAG = "AppCore";
  
  ApplicationCore()
    : initialized_(false)
    , running_(false)
    , stateMutex_(nullptr)
    , gpuTaskHandle_(nullptr)
    , sensorTaskHandle_(nullptr)
  {}
  
  ~ApplicationCore() {
    shutdown();
  }
  
  /** Initialize the dual-core application system
   * @return true if successful
   */
  bool init() {
    if (initialized_) return true;
    
    ESP_LOGI(TAG, "Initializing dual-core application system");
    
    // Create mutex for state synchronization
    stateMutex_ = xSemaphoreCreateMutex();
    if (!stateMutex_) {
      ESP_LOGE(TAG, "Failed to create state mutex");
      return false;
    }
    
    // Initialize state to defaults
    memset(&state_, 0, sizeof(ApplicationState));
    state_.brightness = 80;
    state_.shaderType = 1;  // Rainbow by default
    state_.shaderSpeed = 1.0f;
    state_.autoBlinkEnabled = true;
    state_.autoBlinkIntervalMs = 3000;
    state_.mirrorMode = true;
    state_.targetFps = 60;
    state_.primaryR = 255;
    state_.primaryG = 255;
    state_.primaryB = 255;
    
    initialized_ = true;
    ESP_LOGI(TAG, "Application core initialized");
    return true;
  }
  
  /** Start the dual-core tasks
   * @return true if tasks started successfully
   */
  bool start() {
    if (!initialized_ || running_) return false;
    
    ESP_LOGI(TAG, "Starting dual-core tasks");
    
    // Create GPU pipeline task on Core 1
    BaseType_t result = xTaskCreatePinnedToCore(
      gpuTaskEntry,
      "GpuPipeline",
      STACK_SIZE_GPU,
      this,
      PRIORITY_GPU_PIPELINE,
      &gpuTaskHandle_,
      CORE_GPU
    );
    
    if (result != pdPASS) {
      ESP_LOGE(TAG, "Failed to create GPU task");
      return false;
    }
    
    running_ = true;
    ESP_LOGI(TAG, "GPU pipeline started on Core %d", CORE_GPU);
    
    return true;
  }
  
  /** Stop all tasks */
  void stop() {
    if (!running_) return;
    
    running_ = false;
    
    // Wait for tasks to finish
    vTaskDelay(pdMS_TO_TICKS(100));
    
    if (gpuTaskHandle_) {
      vTaskDelete(gpuTaskHandle_);
      gpuTaskHandle_ = nullptr;
    }
    
    ESP_LOGI(TAG, "Application tasks stopped");
  }
  
  /** Shutdown and cleanup */
  void shutdown() {
    stop();
    
    if (stateMutex_) {
      vSemaphoreDelete(stateMutex_);
      stateMutex_ = nullptr;
    }
    
    initialized_ = false;
  }
  
  // ========================================================
  // State Access (Thread-Safe)
  // ========================================================
  
  /** Lock state for reading/writing
   * @param timeoutMs Maximum wait time
   * @return true if lock acquired
   */
  bool lockState(uint32_t timeoutMs = 100) {
    if (!stateMutex_) return false;
    return xSemaphoreTake(stateMutex_, pdMS_TO_TICKS(timeoutMs)) == pdTRUE;
  }
  
  /** Unlock state after access */
  void unlockState() {
    if (stateMutex_) {
      xSemaphoreGive(stateMutex_);
    }
  }
  
  /** Get mutable reference to state (must hold lock!) */
  ApplicationState& state() { return state_; }
  
  /** Get const reference to state (must hold lock!) */
  const ApplicationState& state() const { return state_; }
  
  // ========================================================
  // Convenience Methods (Auto-Locking)
  // ========================================================
  
  /** Set eye look position */
  void setLookPosition(float x, float y) {
    if (lockState()) {
      state_.lookX = x;
      state_.lookY = y;
      state_.requestUpdate = true;
      unlockState();
    }
  }
  
  /** Set blink progress (0 = open, 1 = closed) */
  void setBlink(float progress) {
    if (lockState()) {
      state_.blinkProgress = progress;
      state_.requestUpdate = true;
      unlockState();
    }
  }
  
  /** Set expression/eye set */
  void setExpression(uint8_t id, float blend = 1.0f) {
    if (lockState()) {
      state_.expressionId = id;
      state_.emotionBlend = blend;
      state_.requestUpdate = true;
      unlockState();
    }
  }
  
  /** Set shader type and speed */
  void setShader(uint8_t type, float speed = 1.0f) {
    if (lockState()) {
      state_.shaderType = type;
      state_.shaderSpeed = speed;
      state_.requestUpdate = true;
      unlockState();
    }
  }
  
  /** Set primary color */
  void setPrimaryColor(uint8_t r, uint8_t g, uint8_t b) {
    if (lockState()) {
      state_.primaryR = r;
      state_.primaryG = g;
      state_.primaryB = b;
      state_.requestUpdate = true;
      unlockState();
    }
  }
  
  /** Set brightness (0-100) */
  void setBrightness(uint8_t brightness) {
    if (lockState()) {
      state_.brightness = brightness;
      state_.requestUpdate = true;
      unlockState();
    }
  }
  
  // ========================================================
  // Statistics
  // ========================================================
  
  CoreStats getGpuStats() const { return gpuStats_; }
  bool isRunning() const { return running_; }
  bool isGpuReady() const { return state_.gpuReady; }
  
private:
  // State
  bool initialized_;
  bool running_;
  ApplicationState state_;
  SemaphoreHandle_t stateMutex_;
  
  // Task handles
  TaskHandle_t gpuTaskHandle_;
  TaskHandle_t sensorTaskHandle_;
  
  // Statistics
  CoreStats gpuStats_;
  
  // ========================================================
  // GPU Task (Core 1)
  // ========================================================
  
  static void gpuTaskEntry(void* param) {
    ApplicationCore* self = static_cast<ApplicationCore*>(param);
    self->gpuTask();
  }
  
  void gpuTask();  // Implemented in ApplicationCore.cpp
};

// Forward declare sprite types for header
class SpriteManager;
class SpriteGpuProtocol;

// Helper function for preloading sprites
void preloadBuiltinSprites(SpriteManager& mgr);

// ============================================================
// Global Instance Accessor
// ============================================================

/** Get the global application core instance */
inline ApplicationCore& getApplicationCore() {
  static ApplicationCore instance;
  return instance;
}

} // namespace Application

#endif // ARCOS_APPLICATION_CORE_HPP_
