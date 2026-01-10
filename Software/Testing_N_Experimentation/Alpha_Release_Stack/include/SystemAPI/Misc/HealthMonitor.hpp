/*****************************************************************
 * @file HealthMonitor.hpp
 * @brief System Health Monitoring - Resource usage, diagnostics
 * 
 * Monitors system health including:
 * - CPU/Memory usage
 * - Temperature monitoring
 * - Sensor health status
 * - Communication link quality
 * - Error tracking and logging
 * - Watchdog management
 * 
 * @author ARCOS
 * @version 1.0
 *****************************************************************/

#pragma once

#include <cstdint>
#include <cstring>
#include <functional>
#include <vector>

namespace SystemAPI {
namespace Health {

// ============================================================
// Health Status Types
// ============================================================

/**
 * @brief Component health status
 */
enum class Status : uint8_t {
  OK,           ///< Component working normally
  WARNING,      ///< Minor issues, still functional
  DEGRADED,     ///< Reduced functionality
  ERROR,        ///< Component has errors
  CRITICAL,     ///< Critical failure
  UNKNOWN,      ///< Status unknown (not initialized)
  DISABLED,     ///< Component intentionally disabled
};

inline const char* getStatusName(Status status) {
  switch (status) {
    case Status::OK:       return "OK";
    case Status::WARNING:  return "Warning";
    case Status::DEGRADED: return "Degraded";
    case Status::ERROR:    return "Error";
    case Status::CRITICAL: return "Critical";
    case Status::UNKNOWN:  return "Unknown";
    case Status::DISABLED: return "Disabled";
    default:               return "Invalid";
  }
}

/**
 * @brief System component identifiers
 */
enum class Component : uint8_t {
  CPU,
  MEMORY,
  GPU_LINK,
  DISPLAY_HUB75,
  DISPLAY_OLED,
  IMU,
  GPS,
  ENVIRONMENTAL,
  MICROPHONE,
  WIFI,
  BLUETOOTH,
  STORAGE,
  POWER,
  THERMAL,
  WATCHDOG,
  COMPONENT_COUNT
};

inline const char* getComponentName(Component comp) {
  switch (comp) {
    case Component::CPU:            return "CPU";
    case Component::MEMORY:         return "Memory";
    case Component::GPU_LINK:       return "GPU Link";
    case Component::DISPLAY_HUB75:  return "HUB75 Display";
    case Component::DISPLAY_OLED:   return "OLED Display";
    case Component::IMU:            return "IMU";
    case Component::GPS:            return "GPS";
    case Component::ENVIRONMENTAL:  return "Environmental";
    case Component::MICROPHONE:     return "Microphone";
    case Component::WIFI:           return "WiFi";
    case Component::BLUETOOTH:      return "Bluetooth";
    case Component::STORAGE:        return "Storage";
    case Component::POWER:          return "Power";
    case Component::THERMAL:        return "Thermal";
    case Component::WATCHDOG:       return "Watchdog";
    default:                        return "Unknown";
  }
}

// ============================================================
// Health Data Structures
// ============================================================

/**
 * @brief CPU health information
 */
struct CpuHealth {
  float usagePercent = 0;       ///< CPU usage 0-100%
  float frequencyMHz = 0;       ///< Current frequency
  float temperatureC = 0;       ///< CPU temperature
  uint32_t uptime = 0;          ///< System uptime in seconds
  uint8_t coreCount = 2;        ///< Number of cores
  float coreUsage[2] = {0, 0};  ///< Per-core usage
};

/**
 * @brief Memory health information
 */
struct MemoryHealth {
  uint32_t totalBytes = 0;      ///< Total RAM
  uint32_t usedBytes = 0;       ///< Used RAM
  uint32_t freeBytes = 0;       ///< Free RAM
  uint32_t largestFreeBlock = 0;///< Largest contiguous block
  float usagePercent = 0;       ///< Usage percentage
  uint32_t heapHighWater = 0;   ///< Heap high water mark
  bool fragmentationWarning = false;
};

/**
 * @brief Communication link health
 */
struct LinkHealth {
  Status status = Status::UNKNOWN;
  uint32_t packetsTotal = 0;
  uint32_t packetsLost = 0;
  float packetLossPercent = 0;
  uint32_t latencyUs = 0;       ///< Round-trip latency
  uint32_t throughputBps = 0;   ///< Current throughput
  int8_t signalStrength = 0;    ///< RSSI for wireless
  uint32_t lastResponseMs = 0;  ///< Time since last response
};

/**
 * @brief Sensor health information
 */
struct SensorHealth {
  Status status = Status::UNKNOWN;
  bool connected = false;
  bool calibrated = false;
  uint32_t sampleCount = 0;
  uint32_t errorCount = 0;
  float dataRate = 0;           ///< Samples per second
  uint32_t lastUpdateMs = 0;
  float accuracy = 0;           ///< Estimated accuracy 0-100%
};

/**
 * @brief Power health information
 */
struct PowerHealth {
  Status status = Status::UNKNOWN;
  float voltageV = 0;           ///< System voltage
  float currentA = 0;           ///< Current draw
  float powerW = 0;             ///< Power consumption
  float batteryPercent = -1;    ///< Battery level (-1 if no battery)
  bool isCharging = false;
  bool onBattery = false;
  uint32_t batteryTimeRemaining = 0; ///< Seconds remaining
};

/**
 * @brief Thermal health information
 */
struct ThermalHealth {
  Status status = Status::OK;
  float cpuTempC = 0;
  float gpuTempC = 0;
  float ambientTempC = 0;
  float maxTempC = 0;           ///< Highest recorded temp
  bool throttling = false;      ///< Thermal throttling active
  bool fanActive = false;
  uint8_t fanSpeedPercent = 0;
};

/**
 * @brief Overall system health summary
 */
struct SystemHealth {
  Status overallStatus = Status::UNKNOWN;
  CpuHealth cpu;
  MemoryHealth memory;
  LinkHealth gpuLink;
  SensorHealth imu;
  SensorHealth gps;
  SensorHealth environmental;
  SensorHealth microphone;
  LinkHealth wifi;
  LinkHealth bluetooth;
  PowerHealth power;
  ThermalHealth thermal;
  
  uint32_t totalErrors = 0;
  uint32_t totalWarnings = 0;
  uint32_t uptimeSeconds = 0;
  
  /**
   * @brief Get worst component status
   */
  Status getWorstStatus() const {
    Status worst = Status::OK;
    auto check = [&worst](Status s) {
      if (s > worst && s != Status::UNKNOWN && s != Status::DISABLED) {
        worst = s;
      }
    };
    check(cpu.usagePercent > 90 ? Status::WARNING : Status::OK);
    check(memory.usagePercent > 90 ? Status::WARNING : Status::OK);
    check(gpuLink.status);
    check(imu.status);
    check(gps.status);
    check(environmental.status);
    check(microphone.status);
    check(thermal.status);
    check(power.status);
    return worst;
  }
};

// ============================================================
// Error Log Entry
// ============================================================

/**
 * @brief Error/warning log entry
 */
struct LogEntry {
  uint32_t timestamp;
  Status severity;
  Component component;
  int errorCode;
  char message[64];
};

// ============================================================
// Health Callbacks
// ============================================================

/**
 * @brief Health alert callback type
 */
using AlertCallback = std::function<void(Component, Status, const char*)>;

// ============================================================
// Health Monitor
// ============================================================

/**
 * @brief Singleton health monitor
 * 
 * @example
 * ```cpp
 * auto& health = Health::Monitor::instance();
 * 
 * // Get overall health
 * auto status = health.getOverallStatus();
 * printf("System health: %s\n", getStatusName(status));
 * 
 * // Get detailed health
 * auto& sys = health.getSystemHealth();
 * printf("CPU: %.1f%%, Temp: %.1f°C\n", sys.cpu.usagePercent, sys.thermal.cpuTempC);
 * printf("Memory: %lu / %lu bytes\n", sys.memory.usedBytes, sys.memory.totalBytes);
 * 
 * // Register for alerts
 * health.onAlert([](Component c, Status s, const char* msg) {
 *   printf("ALERT [%s]: %s - %s\n", getComponentName(c), getStatusName(s), msg);
 * });
 * ```
 */
class Monitor {
public:
  static Monitor& instance() {
    static Monitor inst;
    return inst;
  }
  
  // ---- Initialization ----
  
  bool initialize() {
    initialized_ = true;
    health_.overallStatus = Status::OK;
    return true;
  }
  
  void shutdown() {
    initialized_ = false;
  }
  
  // ---- Health Queries ----
  
  /**
   * @brief Get overall system status
   */
  Status getOverallStatus() const { return health_.overallStatus; }
  
  /**
   * @brief Get component status
   */
  Status getComponentStatus(Component comp) const {
    switch (comp) {
      case Component::CPU:           return health_.cpu.usagePercent > 95 ? Status::WARNING : Status::OK;
      case Component::MEMORY:        return health_.memory.usagePercent > 90 ? Status::WARNING : Status::OK;
      case Component::GPU_LINK:      return health_.gpuLink.status;
      case Component::IMU:           return health_.imu.status;
      case Component::GPS:           return health_.gps.status;
      case Component::ENVIRONMENTAL: return health_.environmental.status;
      case Component::MICROPHONE:    return health_.microphone.status;
      case Component::WIFI:          return health_.wifi.status;
      case Component::BLUETOOTH:     return health_.bluetooth.status;
      case Component::THERMAL:       return health_.thermal.status;
      case Component::POWER:         return health_.power.status;
      default:                       return Status::UNKNOWN;
    }
  }
  
  /**
   * @brief Get full system health data
   */
  const SystemHealth& getSystemHealth() const { return health_; }
  SystemHealth& getSystemHealth() { return health_; }
  
  /**
   * @brief Get specific health structures
   */
  const CpuHealth& getCpuHealth() const { return health_.cpu; }
  const MemoryHealth& getMemoryHealth() const { return health_.memory; }
  const LinkHealth& getGpuLinkHealth() const { return health_.gpuLink; }
  const ThermalHealth& getThermalHealth() const { return health_.thermal; }
  const PowerHealth& getPowerHealth() const { return health_.power; }
  
  // ---- Status Updates (called by subsystems) ----
  
  void updateCpuHealth(const CpuHealth& h) { health_.cpu = h; }
  void updateMemoryHealth(const MemoryHealth& h) { health_.memory = h; }
  void updateGpuLinkHealth(const LinkHealth& h) { health_.gpuLink = h; }
  void updateThermalHealth(const ThermalHealth& h) { health_.thermal = h; }
  void updatePowerHealth(const PowerHealth& h) { health_.power = h; }
  
  void updateSensorHealth(Component sensor, const SensorHealth& h) {
    switch (sensor) {
      case Component::IMU:           health_.imu = h; break;
      case Component::GPS:           health_.gps = h; break;
      case Component::ENVIRONMENTAL: health_.environmental = h; break;
      case Component::MICROPHONE:    health_.microphone = h; break;
      default: break;
    }
  }
  
  // ---- Error Logging ----
  
  /**
   * @brief Log an error or warning
   */
  void logError(Component comp, Status severity, int errorCode, const char* message) {
    if (logCount_ < MAX_LOG_ENTRIES) {
      LogEntry& entry = errorLog_[logCount_++];
      entry.timestamp = uptimeMs_;
      entry.severity = severity;
      entry.component = comp;
      entry.errorCode = errorCode;
      strncpy(entry.message, message, sizeof(entry.message) - 1);
      entry.message[sizeof(entry.message) - 1] = '\0';
    }
    
    if (severity >= Status::WARNING) {
      health_.totalWarnings++;
    }
    if (severity >= Status::ERROR) {
      health_.totalErrors++;
    }
    
    // Trigger alerts
    for (auto& cb : alertCallbacks_) {
      cb.second(comp, severity, message);
    }
  }
  
  /**
   * @brief Get error log
   */
  const LogEntry* getErrorLog(int& count) const {
    count = logCount_;
    return errorLog_;
  }
  
  /**
   * @brief Clear error log
   */
  void clearErrorLog() {
    logCount_ = 0;
  }
  
  // ---- Alert Registration ----
  
  /**
   * @brief Register alert callback
   * @return Callback ID
   */
  int onAlert(AlertCallback callback) {
    int id = nextCallbackId_++;
    alertCallbacks_.push_back({id, callback});
    return id;
  }
  
  /**
   * @brief Remove alert callback
   */
  void removeAlert(int id) {
    alertCallbacks_.erase(
      std::remove_if(alertCallbacks_.begin(), alertCallbacks_.end(),
        [id](const auto& p) { return p.first == id; }),
      alertCallbacks_.end());
  }
  
  // ---- Update ----
  
  void update(float deltaTime) {
    uptimeMs_ += (uint32_t)(deltaTime * 1000);
    health_.uptimeSeconds = uptimeMs_ / 1000;
    
    // Update overall status
    health_.overallStatus = health_.getWorstStatus();
    
    // Platform-specific health collection would go here
    collectPlatformHealth();
  }
  
  // ---- Utilities ----
  
  /**
   * @brief Get formatted health report
   */
  void getHealthReport(char* buffer, int bufferSize) const {
    snprintf(buffer, bufferSize,
      "=== System Health Report ===\n"
      "Overall: %s\n"
      "Uptime: %lu seconds\n"
      "CPU: %.1f%% @ %.0f MHz\n"
      "Memory: %.1f%% (%lu/%lu bytes)\n"
      "Temperature: %.1f°C\n"
      "Errors: %lu, Warnings: %lu\n",
      getStatusName(health_.overallStatus),
      health_.uptimeSeconds,
      health_.cpu.usagePercent, health_.cpu.frequencyMHz,
      health_.memory.usagePercent, health_.memory.usedBytes, health_.memory.totalBytes,
      health_.thermal.cpuTempC,
      health_.totalErrors, health_.totalWarnings);
  }
  
  bool isInitialized() const { return initialized_; }
  
private:
  Monitor() = default;
  Monitor(const Monitor&) = delete;
  Monitor& operator=(const Monitor&) = delete;
  
  bool initialized_ = false;
  SystemHealth health_;
  uint32_t uptimeMs_ = 0;
  
  static constexpr int MAX_LOG_ENTRIES = 32;
  LogEntry errorLog_[MAX_LOG_ENTRIES];
  int logCount_ = 0;
  
  std::vector<std::pair<int, AlertCallback>> alertCallbacks_;
  int nextCallbackId_ = 1;
  
  void collectPlatformHealth() {
    // This would collect actual health data on the platform
    // For now, it's a placeholder
  }
};

} // namespace Health
} // namespace SystemAPI
