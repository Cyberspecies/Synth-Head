/*****************************************************************
 * File:      BaseSystemAPI.hpp
 * Category:  include/BaseAPI
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Master header for the Base System API layer.
 *    Includes all Base API components for unified access.
 * 
 * Architecture:
 *    ┌─────────────────────────────────────────┐
 *    │       Higher Layers (Future)            │
 *    │  Graphics, Animation, UI, Network, etc  │
 *    ├─────────────────────────────────────────┤
 *    │         Base System API                 │ ← THIS LAYER
 *    │  - Telemetry (unified sensor data)      │
 *    │  - Communication (CPU↔GPU protocol)     │
 *    │  - Display Manager (frames/displays)    │
 *    │  - LED Manager (strips/effects)         │
 *    │  - System State (status/errors)         │
 *    ├─────────────────────────────────────────┤
 *    │              HAL Layer                  │
 *    │  GPIO, I2C, SPI, UART, Sensors, etc     │
 *    ├─────────────────────────────────────────┤
 *    │           Hardware (ESP32-S3)           │
 *    └─────────────────────────────────────────┘
 * 
 * Usage:
 *    #include "BaseAPI/BaseSystemAPI.hpp"
 *    
 *    using namespace arcos::base;
 *    
 *    TelemetryProcessor telemetry;
 *    DisplayManager displays;
 *    LedManager leds;
 *    SystemManager system;
 * 
 * Design Principles:
 *    - Hardware Agnostic: No platform-specific code in this layer
 *    - Unified Data: All sensors fused into single TelemetryData
 *    - Easy Communication: PacketBuilder/Parser handle protocol
 *    - Safety: Brightness limits, power budgets, error tracking
 *    - Extensible: Interfaces allow different implementations
 *****************************************************************/

#ifndef ARCOS_INCLUDE_BASEAPI_BASE_SYSTEM_API_HPP_
#define ARCOS_INCLUDE_BASEAPI_BASE_SYSTEM_API_HPP_

// Core types and math
#include "BaseTypes.hpp"

// Telemetry system (sensor fusion)
#include "Telemetry.hpp"

// Communication protocol (CPU-GPU)
#include "CommProtocol.hpp"

// Display management
#include "DisplayManager.hpp"

// LED management
#include "LedManager.hpp"

// System state and errors
#include "SystemState.hpp"

namespace arcos::base{

/**
 * BaseAPI - Main entry point for the Base System API
 * 
 * This class provides a convenient way to access all Base API
 * components through a single instance. Alternatively, components
 * can be instantiated individually for more control.
 */
class BaseAPI{
public:
  // Component instances
  TelemetryProcessor telemetry;
  DisplayManager displays;
  LedManager leds;
  SystemManager system;
  
  /** Initialize the Base API with default configuration */
  Result init(DeviceRole role, const char* device_name = "ARCOS"){
    // Initialize system manager
    Result res = system.init(role, device_name);
    if(res != Result::OK) return res;
    
    // Initialize telemetry with default config
    FusionConfig fusion_config;
    res = telemetry.init(fusion_config);
    if(res != Result::OK){
      system.reportError(ErrorCode::IMU_INIT_FAIL, ErrorSeverity::WARNING,
                        "Telemetry init failed");
    }
    
    // Mark initialization complete
    system.setMode(SystemMode::IDLE);
    
    return Result::OK;
  }
  
  /** Update all components (call in main loop) */
  void update(){
    system.updateMetrics();
    system.nextFrame();
  }
  
  /** Get the device role */
  DeviceRole getRole() const{
    return system.getState().role;
  }
  
  /** Check if this is the CPU */
  bool isCPU() const{
    return system.getState().role == DeviceRole::CPU;
  }
  
  /** Check if this is the GPU */
  bool isGPU() const{
    return system.getState().role == DeviceRole::GPU;
  }
};

// ============================================================
// Version Information
// ============================================================

namespace version{
  constexpr uint8_t MAJOR = 0;
  constexpr uint8_t MINOR = 1;
  constexpr uint8_t PATCH = 0;
  constexpr const char* STRING = "0.1.0";
  constexpr const char* NAME = "Base System API";
}

} // namespace arcos::base

#endif // ARCOS_INCLUDE_BASEAPI_BASE_SYSTEM_API_HPP_
