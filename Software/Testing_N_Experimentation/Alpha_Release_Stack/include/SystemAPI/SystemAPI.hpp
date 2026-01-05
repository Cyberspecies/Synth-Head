/*****************************************************************
 * @file SystemAPI.hpp
 * @brief Top-level System API - Final middleware layer before Application
 * 
 * SystemAPI provides the highest level of abstraction before the
 * Application layer. It manages:
 * - System lifecycle and power modes
 * - Health monitoring and diagnostics
 * - Hardware configuration abstraction (apps don't need to know pins!)
 * - Web server hosting utilities
 * - Complete UI framework for OLED HUD
 * 
 * @section Architecture
 * ```
 * ┌─────────────────────────────────────────┐
 * │         APPLICATION LAYER               │ ← Scene composition, websites, UI
 * ├─────────────────────────────────────────┤
 * │           SYSTEM API                    │ ← THIS LAYER
 * │  ┌─────────┬──────────┬──────────────┐  │
 * │  │Lifecycle│  Health  │   Config     │  │
 * │  │ Manager │ Monitor  │   Manager    │  │
 * │  ├─────────┴──────────┴──────────────┤  │
 * │  │         Web Server Tools          │  │
 * │  ├───────────────────────────────────┤  │
 * │  │      UI Framework (OLED HUD)      │  │
 * │  │  Menus, Buttons, Dropdowns, etc.  │  │
 * │  └───────────────────────────────────┘  │
 * ├─────────────────────────────────────────┤
 * │          FRAMEWORK API                  │ ← GpuLink, Sensors, Physics
 * ├─────────────────────────────────────────┤
 * │             HAL LAYER                   │ ← Hardware abstraction
 * └─────────────────────────────────────────┘
 * ```
 * 
 * @author ARCOS
 * @version 1.0
 *****************************************************************/

#pragma once

// Core System Components
#include "Lifecycle.hpp"
#include "HealthMonitor.hpp"
#include "ConfigManager.hpp"
#include "WebServer.hpp"
#include "SyncState.hpp"

// UI Framework
#include "UI/UICore.hpp"
#include "UI/UIStyle.hpp"
#include "UI/UIElement.hpp"
#include "UI/UIText.hpp"
#include "UI/UIIcon.hpp"
#include "UI/UIButton.hpp"
#include "UI/UIProgressBar.hpp"
#include "UI/UISlider.hpp"
#include "UI/UICheckbox.hpp"
#include "UI/UIDropdown.hpp"
#include "UI/UIMenu.hpp"
#include "UI/UIContainer.hpp"
#include "UI/UIScrollView.hpp"
#include "UI/UIGrid.hpp"
#include "UI/UINotification.hpp"
#include "UI/UIDialog.hpp"
#include "UI/UIRenderer.hpp"
#include "UI/UIManager.hpp"

namespace SystemAPI {

/**
 * @brief System API version information
 */
constexpr const char* VERSION = "1.0.0";
constexpr int VERSION_MAJOR = 1;
constexpr int VERSION_MINOR = 0;
constexpr int VERSION_PATCH = 0;

/**
 * @brief Initialize the entire SystemAPI layer
 * 
 * This initializes all subsystems in the correct order:
 * 1. Configuration Manager (loads hardware config)
 * 2. Health Monitor (starts monitoring)
 * 3. Lifecycle Manager (sets initial state)
 * 4. UI Manager (prepares rendering)
 * 
 * @param configPath Path to configuration file (optional)
 * @return true if initialization successful
 */
inline bool initialize(const char* configPath = nullptr) {
  // Initialize config manager first
  if (!Config::Manager::instance().initialize(configPath)) {
    return false;
  }
  
  // Initialize health monitor
  if (!Health::Monitor::instance().initialize()) {
    return false;
  }
  
  // Initialize lifecycle manager
  Lifecycle::Manager::instance().initialize();
  
  // Initialize UI manager
  UI::Manager::instance().initialize();
  
  return true;
}

/**
 * @brief Shutdown the SystemAPI layer
 */
inline void shutdown() {
  UI::Manager::instance().shutdown();
  Health::Monitor::instance().shutdown();
  Lifecycle::Manager::instance().shutdown();
  Config::Manager::instance().shutdown();
}

/**
 * @brief Update all SystemAPI subsystems (call each frame)
 * @param deltaTime Time since last update in seconds
 */
inline void update(float deltaTime) {
  Lifecycle::Manager::instance().update(deltaTime);
  Health::Monitor::instance().update(deltaTime);
  UI::Manager::instance().update(deltaTime);
}

} // namespace SystemAPI
