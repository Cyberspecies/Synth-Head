/*****************************************************************
 * @file Lifecycle.hpp
 * @brief System Lifecycle Management - Power modes, states, transitions
 * 
 * Manages the system's operational state including:
 * - Power modes (Active, Idle, LowPower, Sleep, DeepSleep)
 * - System states (Boot, Running, Paused, Error, Shutdown)
 * - State transitions with callbacks
 * - Automatic power management based on activity
 * 
 * @author ARCOS
 * @version 1.0
 *****************************************************************/

#pragma once

#include <cstdint>
#include <functional>
#include <vector>

namespace SystemAPI {
namespace Lifecycle {

// ============================================================
// Power Modes
// ============================================================

/**
 * @brief System power modes
 */
enum class PowerMode : uint8_t {
  ACTIVE,       ///< Full power, all systems active
  IDLE,         ///< Reduced power, quick resume
  LOW_POWER,    ///< Significant power reduction, slower resume
  SLEEP,        ///< Minimal power, preserves RAM
  DEEP_SLEEP,   ///< Ultra-low power, loses RAM, requires reboot
  PERFORMANCE,  ///< Maximum performance, may increase power/heat
};

/**
 * @brief Get power mode name
 */
inline const char* getPowerModeName(PowerMode mode) {
  switch (mode) {
    case PowerMode::ACTIVE:      return "Active";
    case PowerMode::IDLE:        return "Idle";
    case PowerMode::LOW_POWER:   return "Low Power";
    case PowerMode::SLEEP:       return "Sleep";
    case PowerMode::DEEP_SLEEP:  return "Deep Sleep";
    case PowerMode::PERFORMANCE: return "Performance";
    default:                     return "Unknown";
  }
}

// ============================================================
// System States
// ============================================================

/**
 * @brief System operational states
 */
enum class SystemState : uint8_t {
  BOOT,         ///< System is booting
  INITIALIZING, ///< Subsystems initializing
  RUNNING,      ///< Normal operation
  PAUSED,       ///< Temporarily paused (user requested)
  SUSPENDED,    ///< Suspended (power saving)
  ERROR,        ///< Error state, limited functionality
  RECOVERY,     ///< Attempting recovery from error
  SHUTDOWN,     ///< Shutting down
};

/**
 * @brief Get system state name
 */
inline const char* getSystemStateName(SystemState state) {
  switch (state) {
    case SystemState::BOOT:         return "Boot";
    case SystemState::INITIALIZING: return "Initializing";
    case SystemState::RUNNING:      return "Running";
    case SystemState::PAUSED:       return "Paused";
    case SystemState::SUSPENDED:    return "Suspended";
    case SystemState::ERROR:        return "Error";
    case SystemState::RECOVERY:     return "Recovery";
    case SystemState::SHUTDOWN:     return "Shutdown";
    default:                        return "Unknown";
  }
}

// ============================================================
// Lifecycle Events
// ============================================================

/**
 * @brief Lifecycle event types
 */
enum class Event : uint8_t {
  STATE_CHANGED,      ///< System state changed
  POWER_MODE_CHANGED, ///< Power mode changed
  IDLE_TIMEOUT,       ///< Idle timeout reached
  LOW_BATTERY,        ///< Battery level critical
  THERMAL_WARNING,    ///< Temperature warning
  THERMAL_CRITICAL,   ///< Temperature critical
  USER_ACTIVITY,      ///< User activity detected
  EXTERNAL_WAKE,      ///< External wake event
  ERROR_OCCURRED,     ///< Error occurred
  RECOVERY_COMPLETE,  ///< Recovery completed
};

/**
 * @brief Lifecycle event data
 */
struct EventData {
  Event type;
  SystemState previousState;
  SystemState currentState;
  PowerMode previousPower;
  PowerMode currentPower;
  uint32_t timestamp;
  int errorCode;
  const char* message;
};

/**
 * @brief Lifecycle event callback type
 */
using EventCallback = std::function<void(const EventData&)>;

// ============================================================
// Power Configuration
// ============================================================

/**
 * @brief Power management configuration
 */
struct PowerConfig {
  uint32_t idleTimeoutMs = 30000;       ///< Time before entering idle (ms)
  uint32_t lowPowerTimeoutMs = 60000;   ///< Time before low power (ms)
  uint32_t sleepTimeoutMs = 300000;     ///< Time before sleep (ms)
  bool autoIdleEnabled = true;          ///< Enable automatic idle
  bool autoLowPowerEnabled = true;      ///< Enable automatic low power
  bool autoSleepEnabled = false;        ///< Enable automatic sleep
  uint8_t cpuFreqActive = 240;          ///< CPU freq in active mode (MHz)
  uint8_t cpuFreqIdle = 160;            ///< CPU freq in idle mode
  uint8_t cpuFreqLowPower = 80;         ///< CPU freq in low power
  float thermalWarningTemp = 70.0f;     ///< Temperature warning threshold
  float thermalCriticalTemp = 85.0f;    ///< Temperature critical threshold
};

// ============================================================
// Lifecycle Manager
// ============================================================

/**
 * @brief Singleton lifecycle manager
 * 
 * @example
 * ```cpp
 * auto& lifecycle = Lifecycle::Manager::instance();
 * 
 * // Register for events
 * lifecycle.onEvent([](const EventData& e) {
 *   if (e.type == Event::STATE_CHANGED) {
 *     printf("State: %s -> %s\n", 
 *            getSystemStateName(e.previousState),
 *            getSystemStateName(e.currentState));
 *   }
 * });
 * 
 * // Request power mode change
 * lifecycle.requestPowerMode(PowerMode::LOW_POWER);
 * 
 * // Signal user activity (resets idle timer)
 * lifecycle.signalActivity();
 * ```
 */
class Manager {
public:
  static Manager& instance() {
    static Manager inst;
    return inst;
  }
  
  // ---- Initialization ----
  
  void initialize() {
    state_ = SystemState::INITIALIZING;
    powerMode_ = PowerMode::ACTIVE;
    lastActivityTime_ = 0;  // Will be set by first update
    initialized_ = true;
  }
  
  void shutdown() {
    setState(SystemState::SHUTDOWN);
    initialized_ = false;
  }
  
  // ---- State Management ----
  
  /**
   * @brief Get current system state
   */
  SystemState getState() const { return state_; }
  
  /**
   * @brief Get current power mode
   */
  PowerMode getPowerMode() const { return powerMode_; }
  
  /**
   * @brief Request a state change
   * @return true if transition is valid and completed
   */
  bool requestState(SystemState newState) {
    if (!isValidTransition(state_, newState)) {
      return false;
    }
    setState(newState);
    return true;
  }
  
  /**
   * @brief Request a power mode change
   * @return true if change was applied
   */
  bool requestPowerMode(PowerMode newMode) {
    if (powerMode_ == newMode) return true;
    
    PowerMode oldMode = powerMode_;
    powerMode_ = newMode;
    
    // Apply power mode settings
    applyPowerMode(newMode);
    
    // Emit event
    emitEvent(Event::POWER_MODE_CHANGED, oldMode, newMode);
    return true;
  }
  
  // ---- Activity Tracking ----
  
  /**
   * @brief Signal user activity (resets idle timers)
   */
  void signalActivity() {
    lastActivityTime_ = currentTimeMs_;
    
    // Wake up from idle/low power if needed
    if (powerMode_ == PowerMode::IDLE || powerMode_ == PowerMode::LOW_POWER) {
      requestPowerMode(PowerMode::ACTIVE);
    }
    
    emitEvent(Event::USER_ACTIVITY);
  }
  
  /**
   * @brief Get time since last activity (ms)
   */
  uint32_t getIdleTime() const {
    return currentTimeMs_ - lastActivityTime_;
  }
  
  // ---- Configuration ----
  
  /**
   * @brief Get power configuration
   */
  PowerConfig& getConfig() { return config_; }
  const PowerConfig& getConfig() const { return config_; }
  
  /**
   * @brief Set power configuration
   */
  void setConfig(const PowerConfig& cfg) { config_ = cfg; }
  
  // ---- Event Handling ----
  
  /**
   * @brief Register event callback
   * @return Callback ID for removal
   */
  int onEvent(EventCallback callback) {
    int id = nextCallbackId_++;
    callbacks_.push_back({id, callback});
    return id;
  }
  
  /**
   * @brief Remove event callback
   */
  void removeCallback(int id) {
    callbacks_.erase(
      std::remove_if(callbacks_.begin(), callbacks_.end(),
        [id](const auto& p) { return p.first == id; }),
      callbacks_.end());
  }
  
  // ---- Update ----
  
  /**
   * @brief Update lifecycle manager (call each frame)
   */
  void update(float deltaTime) {
    currentTimeMs_ += (uint32_t)(deltaTime * 1000);
    
    // Auto power management
    if (config_.autoIdleEnabled || config_.autoLowPowerEnabled || config_.autoSleepEnabled) {
      uint32_t idle = getIdleTime();
      
      if (config_.autoSleepEnabled && idle >= config_.sleepTimeoutMs && 
          powerMode_ != PowerMode::SLEEP && powerMode_ != PowerMode::DEEP_SLEEP) {
        requestPowerMode(PowerMode::SLEEP);
        emitEvent(Event::IDLE_TIMEOUT);
      }
      else if (config_.autoLowPowerEnabled && idle >= config_.lowPowerTimeoutMs &&
               powerMode_ == PowerMode::IDLE) {
        requestPowerMode(PowerMode::LOW_POWER);
        emitEvent(Event::IDLE_TIMEOUT);
      }
      else if (config_.autoIdleEnabled && idle >= config_.idleTimeoutMs &&
               powerMode_ == PowerMode::ACTIVE) {
        requestPowerMode(PowerMode::IDLE);
        emitEvent(Event::IDLE_TIMEOUT);
      }
    }
  }
  
  // ---- Queries ----
  
  bool isInitialized() const { return initialized_; }
  bool isRunning() const { return state_ == SystemState::RUNNING; }
  bool isPaused() const { return state_ == SystemState::PAUSED; }
  bool isError() const { return state_ == SystemState::ERROR; }
  bool isLowPower() const { 
    return powerMode_ == PowerMode::LOW_POWER || 
           powerMode_ == PowerMode::SLEEP || 
           powerMode_ == PowerMode::DEEP_SLEEP; 
  }
  
private:
  Manager() = default;
  Manager(const Manager&) = delete;
  Manager& operator=(const Manager&) = delete;
  
  bool initialized_ = false;
  SystemState state_ = SystemState::BOOT;
  PowerMode powerMode_ = PowerMode::ACTIVE;
  PowerConfig config_;
  uint32_t currentTimeMs_ = 0;
  uint32_t lastActivityTime_ = 0;
  
  std::vector<std::pair<int, EventCallback>> callbacks_;
  int nextCallbackId_ = 1;
  
  void setState(SystemState newState) {
    if (state_ == newState) return;
    
    SystemState oldState = state_;
    state_ = newState;
    
    // Emit state change event
    EventData event = {};
    event.type = Event::STATE_CHANGED;
    event.previousState = oldState;
    event.currentState = newState;
    event.timestamp = currentTimeMs_;
    
    for (auto& cb : callbacks_) {
      cb.second(event);
    }
  }
  
  void emitEvent(Event type, PowerMode oldPower = PowerMode::ACTIVE, 
                 PowerMode newPower = PowerMode::ACTIVE) {
    EventData event = {};
    event.type = type;
    event.previousState = state_;
    event.currentState = state_;
    event.previousPower = oldPower;
    event.currentPower = newPower;
    event.timestamp = currentTimeMs_;
    
    for (auto& cb : callbacks_) {
      cb.second(event);
    }
  }
  
  bool isValidTransition(SystemState from, SystemState to) {
    // Define valid state transitions
    switch (from) {
      case SystemState::BOOT:
        return to == SystemState::INITIALIZING || to == SystemState::ERROR;
      case SystemState::INITIALIZING:
        return to == SystemState::RUNNING || to == SystemState::ERROR;
      case SystemState::RUNNING:
        return to == SystemState::PAUSED || to == SystemState::SUSPENDED || 
               to == SystemState::ERROR || to == SystemState::SHUTDOWN;
      case SystemState::PAUSED:
        return to == SystemState::RUNNING || to == SystemState::SHUTDOWN;
      case SystemState::SUSPENDED:
        return to == SystemState::RUNNING || to == SystemState::SHUTDOWN;
      case SystemState::ERROR:
        return to == SystemState::RECOVERY || to == SystemState::SHUTDOWN;
      case SystemState::RECOVERY:
        return to == SystemState::RUNNING || to == SystemState::ERROR || 
               to == SystemState::SHUTDOWN;
      case SystemState::SHUTDOWN:
        return false;  // Terminal state
      default:
        return false;
    }
  }
  
  void applyPowerMode(PowerMode mode) {
    // Platform-specific power management would go here
    // For now, this is a placeholder
    (void)mode;
  }
};

} // namespace Lifecycle
} // namespace SystemAPI
