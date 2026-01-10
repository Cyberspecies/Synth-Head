/*****************************************************************
 * @file SystemAPI.hpp
 * @brief Unified System API - Single header to include entire stack
 * 
 * SystemAPI is the single entry point for the entire software stack.
 * Including this header gives you access to:
 * - HAL Layer (Hardware Abstraction)
 * - BaseAPI Layer (Display, LED, Telemetry, Communication)
 * - FrameworkAPI Layer (Physics, Input, Visual Composer)
 * - SystemAPI Layer (UI, Web, Config, Health, Lifecycle)
 * 
 * @section Architecture
 * ```
 * ┌─────────────────────────────────────────────────────────────┐
 * │               APPLICATION LAYER                             │
 * │      BootMode.cpp / CurrentMode.cpp                        │
 * │      (Include only SystemAPI.hpp)                          │
 * ├─────────────────────────────────────────────────────────────┤
 * │                    SYSTEM API                               │ ← THIS
 * │  ┌──────────┬──────────┬───────────┬──────────┬─────────┐  │
 * │  │    UI    │   Web    │  Config   │  Health  │ Lifecy. │  │
 * │  └──────────┴──────────┴───────────┴──────────┴─────────┘  │
 * ├─────────────────────────────────────────────────────────────┤
 * │                   FRAMEWORK API                             │
 * │  ┌──────────┬──────────┬───────────┬──────────┬─────────┐  │
 * │  │  Input   │ Physics  │  Visual   │ Metrics  │ Network │  │
 * │  └──────────┴──────────┴───────────┴──────────┴─────────┘  │
 * ├─────────────────────────────────────────────────────────────┤
 * │                     BASE API                                │
 * │  ┌──────────┬──────────┬───────────┬──────────┬─────────┐  │
 * │  │ Display  │   LED    │ Telemetry │  Comms   │  State  │  │
 * │  └──────────┴──────────┴───────────┴──────────┴─────────┘  │
 * ├─────────────────────────────────────────────────────────────┤
 * │                     HAL LAYER                               │
 * │  ┌──────────┬──────────┬───────────┬──────────┬─────────┐  │
 * │  │   I2C    │   I2S    │   UART    │   GPIO   │   SPI   │  │
 * │  └──────────┴──────────┴───────────┴──────────┴─────────┘  │
 * └─────────────────────────────────────────────────────────────┘
 * ```
 * 
 * @section Usage
 * ```cpp
 * #include "SystemAPI/SystemAPI.hpp"
 * 
 * // Now you have access to everything:
 * // HAL::*, BaseAPI::*, FrameworkAPI::*, UI::*, etc.
 * ```
 * 
 * @author ARCOS
 * @version 2.0
 *****************************************************************/

#pragma once

//=============================================================================
// HAL Layer - Hardware Abstraction (Master header includes all interfaces)
//=============================================================================
#include "HAL/hal.hpp"

//=============================================================================
// Base API Layer - Core System Services (Master header includes all)
//=============================================================================
#include "BaseAPI/BaseSystemAPI.hpp"

//=============================================================================
// Framework API Layer - High-Level Services (Master header includes all)
// NOTE: FrameworkAPI has some internal inconsistencies that need to be resolved.
//       Define SYSTEMAPI_INCLUDE_FRAMEWORK to include it anyway.
//=============================================================================
#if defined(SYSTEMAPI_INCLUDE_FRAMEWORK) || defined(ARDUINO)
  #include "FrameworkAPI/FrameworkAPI.hpp"
  #define SYSTEMAPI_HAS_FRAMEWORK 1
#else
  #define SYSTEMAPI_HAS_FRAMEWORK 0
#endif

//=============================================================================
// SystemAPI Namespace - Convenience functions and version info
//=============================================================================
namespace SystemAPI {

/**
 * @brief System API version information
 */
constexpr const char* VERSION = "2.0.0";
constexpr int VERSION_MAJOR = 2;
constexpr int VERSION_MINOR = 0;
constexpr int VERSION_PATCH = 0;

} // namespace SystemAPI
