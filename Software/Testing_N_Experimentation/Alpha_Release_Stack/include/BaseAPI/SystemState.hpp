/*****************************************************************
 * File:      SystemState.hpp
 * Category:  include/BaseAPI
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    System-wide state management for the Base System API.
 *    Provides:
 *    - Global system state and status
 *    - Error tracking and reporting
 *    - Performance metrics
 *    - Device identification
 * 
 * Layer:
 *    HAL Layer -> [Base System API - System] -> Application
 *****************************************************************/

#ifndef ARCOS_INCLUDE_BASEAPI_SYSTEM_STATE_HPP_
#define ARCOS_INCLUDE_BASEAPI_SYSTEM_STATE_HPP_

#include "BaseTypes.hpp"
#include <cstring>

namespace arcos::base{

// ============================================================
// System Modes
// ============================================================

/** Operating mode enumeration */
enum class SystemMode : uint8_t{
  INIT = 0,          // Initializing
  IDLE,              // Idle/standby
  ACTIVE,            // Normal operation
  ANIMATION,         // Animation mode
  CALIBRATION,       // Sensor calibration
  DIAGNOSTIC,        // Diagnostic/test mode
  ERROR,             // Error state
  SHUTDOWN           // Shutting down
};

/** Connection state */
enum class ConnectionState : uint8_t{
  DISCONNECTED = 0,
  CONNECTING,
  CONNECTED,
  ERROR
};

// ============================================================
// Error Tracking
// ============================================================

/** Error severity levels */
enum class ErrorSeverity : uint8_t{
  INFO = 0,
  WARNING,
  ERROR,
  FATAL
};

/** Error codes */
enum class ErrorCode : uint8_t{
  NONE = 0,
  
  // Communication errors (0x10-0x1F)
  UART_TX_FAIL = 0x10,
  UART_RX_FAIL,
  UART_TIMEOUT,
  PROTOCOL_ERROR,
  CRC_MISMATCH,
  SYNC_LOST,
  
  // Sensor errors (0x20-0x2F)
  IMU_INIT_FAIL = 0x20,
  IMU_READ_FAIL,
  IMU_CALIBRATION_FAIL,
  ENV_INIT_FAIL,
  ENV_READ_FAIL,
  GPS_INIT_FAIL,
  GPS_NO_FIX,
  MIC_INIT_FAIL,
  MIC_READ_FAIL,
  
  // Display errors (0x30-0x3F)
  DISPLAY_INIT_FAIL = 0x30,
  DISPLAY_UPDATE_FAIL,
  OLED_INIT_FAIL,
  HUB75_INIT_FAIL,
  
  // LED errors (0x40-0x4F)
  LED_INIT_FAIL = 0x40,
  LED_UPDATE_FAIL,
  LED_POWER_LIMIT,
  
  // Storage errors (0x50-0x5F)
  STORAGE_INIT_FAIL = 0x50,
  STORAGE_MOUNT_FAIL,
  STORAGE_READ_FAIL,
  STORAGE_WRITE_FAIL,
  
  // System errors (0xF0-0xFF)
  OUT_OF_MEMORY = 0xF0,
  WATCHDOG_TIMEOUT,
  UNKNOWN = 0xFF
};

/** Error record */
struct ErrorRecord{
  ErrorCode code;
  ErrorSeverity severity;
  Timestamp timestamp;
  char message[48];
  
  ErrorRecord()
    : code(ErrorCode::NONE)
    , severity(ErrorSeverity::INFO)
    , timestamp(0)
  {
    message[0] = '\0';
  }
};

// ============================================================
// Performance Metrics
// ============================================================

/** Performance metrics structure */
struct PerformanceMetrics{
  // Timing
  Timestamp uptime_ms;           // System uptime
  float loop_rate_hz;            // Main loop rate
  float sensor_rate_hz;          // Sensor update rate
  float display_fps;             // Display frame rate
  float comm_rate_hz;            // Communication rate
  
  // Processing time (microseconds)
  uint32_t loop_time_us;         // Main loop time
  uint32_t sensor_time_us;       // Sensor processing time
  uint32_t fusion_time_us;       // Fusion algorithm time
  uint32_t display_time_us;      // Display update time
  uint32_t comm_time_us;         // Communication time
  
  // Memory
  uint32_t free_heap_bytes;      // Free heap memory
  uint32_t min_free_heap_bytes;  // Minimum free heap (watermark)
  
  // Communication
  uint32_t packets_sent;
  uint32_t packets_received;
  uint32_t packets_dropped;
  uint32_t bytes_sent;
  uint32_t bytes_received;
  
  // Errors
  uint32_t error_count;
  uint32_t warning_count;
  
  PerformanceMetrics(){
    memset(this, 0, sizeof(*this));
  }
};

// ============================================================
// System State
// ============================================================

/** Complete system state structure */
struct SystemState{
  // Identity
  DeviceRole role;
  Version version;
  char device_name[16];
  
  // Current state
  SystemMode mode;
  ConnectionState connection;
  Timestamp timestamp;
  uint32_t frame_number;
  
  // Status flags
  bool initialized;
  bool sensors_ready;
  bool displays_ready;
  bool leds_ready;
  bool comm_ready;
  bool storage_ready;
  
  // Latest error
  ErrorRecord last_error;
  
  // Metrics
  PerformanceMetrics metrics;
  
  SystemState(){
    role = DeviceRole::UNKNOWN;
    memset(device_name, 0, sizeof(device_name));
    mode = SystemMode::INIT;
    connection = ConnectionState::DISCONNECTED;
    timestamp = 0;
    frame_number = 0;
    initialized = false;
    sensors_ready = false;
    displays_ready = false;
    leds_ready = false;
    comm_ready = false;
    storage_ready = false;
  }
};

// ============================================================
// System Manager Interface
// ============================================================

/**
 * ISystemManager - Interface for system-wide management
 */
class ISystemManager{
public:
  virtual ~ISystemManager() = default;
  
  /** Initialize system manager */
  virtual Result init(DeviceRole role, const char* name) = 0;
  
  /** Get current system state */
  virtual const SystemState& getState() const = 0;
  
  /** Set operating mode */
  virtual void setMode(SystemMode mode) = 0;
  
  /** Get operating mode */
  virtual SystemMode getMode() const = 0;
  
  /** Report an error */
  virtual void reportError(ErrorCode code, ErrorSeverity severity, 
                          const char* message = nullptr) = 0;
  
  /** Clear last error */
  virtual void clearError() = 0;
  
  /** Update metrics */
  virtual void updateMetrics() = 0;
  
  /** Get performance metrics */
  virtual const PerformanceMetrics& getMetrics() const = 0;
  
  /** Get uptime in milliseconds */
  virtual Timestamp getUptime() const = 0;
  
  /** Increment frame counter */
  virtual void nextFrame() = 0;
  
  /** Get current frame number */
  virtual uint32_t getFrameNumber() const = 0;
};

// ============================================================
// Default Implementation
// ============================================================

/**
 * SystemManager - Default system manager implementation
 */
class SystemManager : public ISystemManager{
public:
  SystemManager()
    : start_time_(0)
    , last_loop_time_(0)
    , loop_count_(0)
  {}
  
  Result init(DeviceRole role, const char* name) override{
    state_.role = role;
    if(name){
      strncpy(state_.device_name, name, sizeof(state_.device_name) - 1);
    }
    state_.version = Version(0, 1, 0);  // Version 0.1.0
    state_.mode = SystemMode::INIT;
    state_.initialized = true;
    start_time_ = getCurrentTime();
    return Result::OK;
  }
  
  const SystemState& getState() const override{ return state_; }
  
  void setMode(SystemMode mode) override{
    state_.mode = mode;
  }
  
  SystemMode getMode() const override{ return state_.mode; }
  
  void reportError(ErrorCode code, ErrorSeverity severity,
                   const char* message = nullptr) override{
    state_.last_error.code = code;
    state_.last_error.severity = severity;
    state_.last_error.timestamp = getUptime();
    
    if(message){
      strncpy(state_.last_error.message, message, 
              sizeof(state_.last_error.message) - 1);
    }else{
      state_.last_error.message[0] = '\0';
    }
    
    if(severity == ErrorSeverity::ERROR || severity == ErrorSeverity::FATAL){
      state_.metrics.error_count++;
    }else if(severity == ErrorSeverity::WARNING){
      state_.metrics.warning_count++;
    }
    
    // Set error mode for fatal errors
    if(severity == ErrorSeverity::FATAL){
      state_.mode = SystemMode::ERROR;
    }
  }
  
  void clearError() override{
    state_.last_error = ErrorRecord();
  }
  
  void updateMetrics() override{
    Timestamp now = getCurrentTime();
    state_.timestamp = now;
    state_.metrics.uptime_ms = now - start_time_;
    
    // Calculate loop rate
    if(last_loop_time_ > 0){
      uint32_t delta = now - last_loop_time_;
      if(delta > 0){
        state_.metrics.loop_rate_hz = 1000.0f / delta;
      }
    }
    last_loop_time_ = now;
    loop_count_++;
  }
  
  const PerformanceMetrics& getMetrics() const override{
    return state_.metrics;
  }
  
  Timestamp getUptime() const override{
    return getCurrentTime() - start_time_;
  }
  
  void nextFrame() override{
    state_.frame_number++;
  }
  
  uint32_t getFrameNumber() const override{
    return state_.frame_number;
  }
  
  // Setters for subsystem status
  void setSensorsReady(bool ready){ state_.sensors_ready = ready; }
  void setDisplaysReady(bool ready){ state_.displays_ready = ready; }
  void setLedsReady(bool ready){ state_.leds_ready = ready; }
  void setCommReady(bool ready){ 
    state_.comm_ready = ready;
    state_.connection = ready ? ConnectionState::CONNECTED : ConnectionState::DISCONNECTED;
  }
  void setStorageReady(bool ready){ state_.storage_ready = ready; }
  
  // Metrics setters
  void setLoopTime(uint32_t us){ state_.metrics.loop_time_us = us; }
  void setSensorTime(uint32_t us){ state_.metrics.sensor_time_us = us; }
  void setFusionTime(uint32_t us){ state_.metrics.fusion_time_us = us; }
  void setDisplayTime(uint32_t us){ state_.metrics.display_time_us = us; }
  void setCommTime(uint32_t us){ state_.metrics.comm_time_us = us; }
  void setFreeHeap(uint32_t bytes){ 
    state_.metrics.free_heap_bytes = bytes;
    if(bytes < state_.metrics.min_free_heap_bytes || state_.metrics.min_free_heap_bytes == 0){
      state_.metrics.min_free_heap_bytes = bytes;
    }
  }
  void setDisplayFps(float fps){ state_.metrics.display_fps = fps; }
  void setSensorRate(float hz){ state_.metrics.sensor_rate_hz = hz; }
  void setCommRate(float hz){ state_.metrics.comm_rate_hz = hz; }
  
  void addPacketSent(uint32_t bytes){
    state_.metrics.packets_sent++;
    state_.metrics.bytes_sent += bytes;
  }
  void addPacketReceived(uint32_t bytes){
    state_.metrics.packets_received++;
    state_.metrics.bytes_received += bytes;
  }
  void addPacketDropped(){
    state_.metrics.packets_dropped++;
  }

protected:
  /** Platform-specific: get current time in ms */
  virtual Timestamp getCurrentTime() const{
    // Override in platform-specific implementation
    return 0;
  }

private:
  SystemState state_;
  Timestamp start_time_;
  Timestamp last_loop_time_;
  uint32_t loop_count_;
};

// ============================================================
// Utility Functions
// ============================================================

/** Get string name for system mode */
inline const char* modeToString(SystemMode mode){
  switch(mode){
    case SystemMode::INIT:        return "INIT";
    case SystemMode::IDLE:        return "IDLE";
    case SystemMode::ACTIVE:      return "ACTIVE";
    case SystemMode::ANIMATION:   return "ANIMATION";
    case SystemMode::CALIBRATION: return "CALIBRATION";
    case SystemMode::DIAGNOSTIC:  return "DIAGNOSTIC";
    case SystemMode::ERROR:       return "ERROR";
    case SystemMode::SHUTDOWN:    return "SHUTDOWN";
    default:                      return "UNKNOWN";
  }
}

/** Get string name for error code */
inline const char* errorCodeToString(ErrorCode code){
  switch(code){
    case ErrorCode::NONE:              return "NONE";
    case ErrorCode::UART_TX_FAIL:      return "UART_TX_FAIL";
    case ErrorCode::UART_RX_FAIL:      return "UART_RX_FAIL";
    case ErrorCode::UART_TIMEOUT:      return "UART_TIMEOUT";
    case ErrorCode::PROTOCOL_ERROR:    return "PROTOCOL_ERROR";
    case ErrorCode::CRC_MISMATCH:      return "CRC_MISMATCH";
    case ErrorCode::SYNC_LOST:         return "SYNC_LOST";
    case ErrorCode::IMU_INIT_FAIL:     return "IMU_INIT_FAIL";
    case ErrorCode::IMU_READ_FAIL:     return "IMU_READ_FAIL";
    case ErrorCode::ENV_INIT_FAIL:     return "ENV_INIT_FAIL";
    case ErrorCode::GPS_INIT_FAIL:     return "GPS_INIT_FAIL";
    case ErrorCode::DISPLAY_INIT_FAIL: return "DISPLAY_INIT_FAIL";
    case ErrorCode::LED_INIT_FAIL:     return "LED_INIT_FAIL";
    case ErrorCode::OUT_OF_MEMORY:     return "OUT_OF_MEMORY";
    default:                           return "UNKNOWN";
  }
}

} // namespace arcos::base

#endif // ARCOS_INCLUDE_BASEAPI_SYSTEM_STATE_HPP_
