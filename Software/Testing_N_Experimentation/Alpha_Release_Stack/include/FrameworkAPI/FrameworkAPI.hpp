/*****************************************************************
 * File:      FrameworkAPI.hpp
 * Category:  include/FrameworkAPI
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Master header for the Framework API middleware layer.
 *    Sits above BaseAPI and provides high-level services:
 *    
 *    - Network Services (WiFi, Captive Portal, mDNS)
 *    - Metrics/Telemetry Pub-Sub (ROS2-like subscriptions)
 *    - Input Management (buttons, gestures, abstracted events)
 *    - Visual Composition (animation tools, effects, layouts)
 *    - Physics2D (collision, gravity, rigid body dynamics)
 * 
 * Architecture:
 *    ┌─────────────────────────────────────────┐
 *    │        Application Layer                │
 *    │  User code, animations, behaviors       │
 *    ├─────────────────────────────────────────┤
 *    │         Framework API                   │ ← THIS LAYER
 *    │  - NetworkService (WiFi, Captive)       │
 *    │  - MetricsHub (Pub-Sub telemetry)       │
 *    │  - InputManager (buttons, gestures)     │
 *    │  - VisualComposer (animations, FX)      │
 *    │  - Physics2D (collisions, gravity)      │
 *    ├─────────────────────────────────────────┤
 *    │         Base System API                 │
 *    │  Telemetry, Comms, Display, LED, State  │
 *    ├─────────────────────────────────────────┤
 *    │              HAL Layer                  │
 *    │  GPIO, I2C, SPI, UART, Sensors, etc     │
 *    ├─────────────────────────────────────────┤
 *    │           Hardware (ESP32-S3)           │
 *    └─────────────────────────────────────────┘
 * 
 * Usage:
 *    #include "FrameworkAPI/FrameworkAPI.hpp"
 *    
 *    using namespace arcos::framework;
 *    
 *    Framework fw;
 *    fw.init();
 *    
 *    // Subscribe to sensor data
 *    fw.metrics.subscribe<Vec3>("imu/accel", [](const Vec3& a) {
 *      // Handle accelerometer update
 *    });
 *    
 *    // Setup captive portal
 *    fw.network.startCaptivePortal("SynthHead-Setup");
 *    
 *    // Create animation
 *    auto anim = fw.visuals.createTextScroll("Hello!", {
 *      .speed = 30.0f,
 *      .color = Color::fromHex(0xFF00FF),
 *      .loop = true
 *    });
 *    fw.visuals.play(anim);
 *****************************************************************/

#ifndef ARCOS_INCLUDE_FRAMEWORKAPI_FRAMEWORK_API_HPP_
#define ARCOS_INCLUDE_FRAMEWORKAPI_FRAMEWORK_API_HPP_

// Framework components
#include "FrameworkTypes.hpp"
#include "NetworkService.hpp"
#include "MetricsHub.hpp"
#include "InputManager.hpp"
#include "VisualComposer.hpp"
#include "Physics2D.hpp"

// Optional: Base layer access (forward declare if not available)
#if __has_include("BaseAPI/BaseSystemAPI.hpp")
  #include "BaseAPI/BaseSystemAPI.hpp"
  #define FRAMEWORK_HAS_BASE_API 1
#else
  #define FRAMEWORK_HAS_BASE_API 0
#endif

#if __has_include("GpuDriver/GpuDriver.hpp")
  #include "GpuDriver/GpuDriver.hpp"
  #define FRAMEWORK_HAS_GPU_DRIVER 1
#else
  #define FRAMEWORK_HAS_GPU_DRIVER 0
#endif

namespace arcos::framework {

/**
 * Framework Configuration
 */
struct FrameworkConfig {
  // Network settings
  const char* device_name = "SynthHead";
  const char* ap_password = nullptr;  // nullptr = open network
  bool auto_start_captive = false;
  
  // Metrics settings
  uint32_t metrics_publish_rate_hz = 50;
  uint32_t metrics_buffer_size = 32;
  
  // Input settings
  uint32_t button_debounce_ms = 50;
  uint32_t long_press_ms = 1000;
  uint32_t double_click_ms = 300;
  
  // Visual settings
  uint8_t default_brightness = 128;
};

/**
 * Framework - Main entry point for the Framework API
 * 
 * Provides unified access to all framework services.
 * Initialize once at startup and use throughout application.
 * 
 * The framework can work standalone (without BaseAPI/GpuDriver)
 * for testing, or fully integrated for production.
 */
class Framework {
public:
  // Service instances (public for direct access)
  NetworkService network;
  MetricsHub metrics;
  InputManager input;
  VisualComposer visuals;
  
  /**
   * Initialize the framework with configuration
   * @param config Framework configuration
   * @return Result::OK on success
   */
  Result init(const FrameworkConfig& config = FrameworkConfig()) {
    config_ = config;
    
    #if FRAMEWORK_HAS_BASE_API
    // Initialize base layer first
    Result res = base_api_.init(base::DeviceRole::CPU, config.device_name);
    if (res != Result::OK) {
      // Non-fatal: framework can work without BaseAPI
    }
    #endif
    
    #if FRAMEWORK_HAS_GPU_DRIVER
    // Initialize GPU driver
    gpu_driver_.init();
    #endif
    
    // Initialize framework services
    Result res = metrics.init(config.metrics_publish_rate_hz, config.metrics_buffer_size);
    if (res != Result::OK) return res;
    
    res = input.init(config.button_debounce_ms, config.long_press_ms, 
                     config.double_click_ms);
    if (res != Result::OK) return res;
    
    res = visuals.init();
    if (res != Result::OK) return res;
    
    visuals.setBrightness(config.default_brightness);
    
    res = network.init(config.device_name);
    if (res != Result::OK) return res;
    
    if (config.auto_start_captive) {
      network.startCaptivePortal();
    }
    
    initialized_ = true;
    return Result::OK;
  }
  
  /**
   * Update framework - call this in your main loop
   * Processes inputs, publishes metrics, updates animations
   * @param dt_ms Time since last update in milliseconds
   */
  void update(uint32_t dt_ms) {
    if (!initialized_) return;
    
    // Update input manager (polls buttons, processes gestures)
    input.update(dt_ms);
    
    #if FRAMEWORK_HAS_BASE_API
    // Update metrics hub (publishes telemetry to subscribers)
    base_api_.telemetry.update(dt_ms / 1000.0f);
    metrics.publishTelemetry(base_api_.telemetry.getData());
    #endif
    
    // Update visual composer (advances animations, renders frames)
    visuals.update(dt_ms);
    
    // Handle network events
    network.update(dt_ms);
  }
  
  /**
   * Check if framework is initialized
   */
  bool isInitialized() const { return initialized_; }
  
  #if FRAMEWORK_HAS_BASE_API
  /**
   * Get BaseAPI for direct access if needed
   */
  base::BaseAPI& base() { return base_api_; }
  #endif
  
  #if FRAMEWORK_HAS_GPU_DRIVER
  /**
   * Get GPU driver for direct access if needed
   */
  gpu::GpuDriver& gpu() { return gpu_driver_; }
  #endif
  
private:
  bool initialized_ = false;
  FrameworkConfig config_;
  
  #if FRAMEWORK_HAS_BASE_API
  base::BaseAPI base_api_;
  #endif
  
  #if FRAMEWORK_HAS_GPU_DRIVER
  gpu::GpuDriver gpu_driver_;
  #endif
};

} // namespace arcos::framework

#endif // ARCOS_INCLUDE_FRAMEWORKAPI_FRAMEWORK_API_HPP_
