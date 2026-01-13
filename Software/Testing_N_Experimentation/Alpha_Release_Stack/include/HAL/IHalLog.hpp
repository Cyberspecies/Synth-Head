/*****************************************************************
 * File:      IHalLog.hpp
 * Category:  include/HAL
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Logging Hardware Abstraction Layer interface.
 *    Provides platform-independent logging and error handling
 *    for all HAL and middleware components.
 *****************************************************************/

#ifndef ARCOS_INCLUDE_HAL_IHAL_LOG_HPP_
#define ARCOS_INCLUDE_HAL_IHAL_LOG_HPP_

#include "HalTypes.hpp"

namespace arcos::hal{

// ============================================================
// Log Levels
// ============================================================

/** Log severity levels */
enum class LogLevel : uint8_t{
  NONE = 0,     // No logging
  ERROR = 1,    // Errors only
  WARN = 2,     // Warnings and errors
  INFO = 3,     // Info, warnings, errors
  DEBUG = 4,    // Debug and above
  VERBOSE = 5   // All messages
};

// ============================================================
// Log Interface
// ============================================================

/** Logging Hardware Abstraction Interface
 * 
 * Provides platform-independent logging functionality.
 * Implementations can output to Serial, file, network, etc.
 */
class IHalLog{
public:
  virtual ~IHalLog() = default;
  
  /** Initialize logging system
   * @param level Minimum log level to output
   * @return HalResult::OK on success
   */
  virtual HalResult init(LogLevel level = LogLevel::INFO) = 0;
  
  /** Set log level
   * @param level New log level
   */
  virtual void setLevel(LogLevel level) = 0;
  
  /** Get current log level
   * @return Current log level
   */
  virtual LogLevel getLevel() const = 0;
  
  /** Log error message
   * @param tag Module tag
   * @param format Printf-style format string
   * @param ... Format arguments
   */
  virtual void error(const char* tag, const char* format, ...) = 0;
  
  /** Log warning message
   * @param tag Module tag
   * @param format Printf-style format string
   * @param ... Format arguments
   */
  virtual void warn(const char* tag, const char* format, ...) = 0;
  
  /** Log info message
   * @param tag Module tag
   * @param format Printf-style format string
   * @param ... Format arguments
   */
  virtual void info(const char* tag, const char* format, ...) = 0;
  
  /** Log debug message
   * @param tag Module tag
   * @param format Printf-style format string
   * @param ... Format arguments
   */
  virtual void debug(const char* tag, const char* format, ...) = 0;
  
  /** Log verbose message
   * @param tag Module tag
   * @param format Printf-style format string
   * @param ... Format arguments
   */
  virtual void verbose(const char* tag, const char* format, ...) = 0;
  
  /** Log with specified level
   * @param level Log level
   * @param tag Module tag
   * @param format Printf-style format string
   * @param ... Format arguments
   */
  virtual void log(LogLevel level, const char* tag, const char* format, ...) = 0;
  
  /** Log HalResult with context
   * @param result Result code to log
   * @param tag Module tag
   * @param operation Description of operation
   */
  virtual void logResult(HalResult result, const char* tag, const char* operation) = 0;
  
  /** Flush log buffer (if buffered)
   */
  virtual void flush() = 0;
};

// ============================================================
// Error Handler Interface
// ============================================================

/** Error callback function type */
using ErrorCallback = void(*)(HalResult result, const char* tag, const char* message);

/** Error Handler Interface
 * 
 * Provides centralized error handling and reporting.
 */
class IHalErrorHandler{
public:
  virtual ~IHalErrorHandler() = default;
  
  /** Initialize error handler
   * @return HalResult::OK on success
   */
  virtual HalResult init() = 0;
  
  /** Report an error
   * @param result Error result code
   * @param tag Module tag
   * @param message Error message
   */
  virtual void reportError(HalResult result, const char* tag, const char* message) = 0;
  
  /** Set error callback
   * @param callback Function to call on error
   */
  virtual void setCallback(ErrorCallback callback) = 0;
  
  /** Get last error result
   * @return Last error code
   */
  virtual HalResult getLastError() const = 0;
  
  /** Get last error tag
   * @return Last error module tag
   */
  virtual const char* getLastErrorTag() const = 0;
  
  /** Get last error message
   * @return Last error message
   */
  virtual const char* getLastErrorMessage() const = 0;
  
  /** Get total error count
   * @return Number of errors since init
   */
  virtual uint32_t getErrorCount() const = 0;
  
  /** Clear error state
   */
  virtual void clearError() = 0;
  
  /** Check if system has errors
   * @return true if errors present
   */
  virtual bool hasError() const = 0;
};

// ============================================================
// Helper Functions
// ============================================================

/** Convert HalResult to string
 * @param result HalResult code
 * @return String representation
 */
inline const char* halResultToString(HalResult result){
  switch(result){
    case HalResult::OK:              return "OK";
    case HalResult::ERROR:           return "ERROR";
    case HalResult::TIMEOUT:         return "TIMEOUT";
    case HalResult::BUSY:            return "BUSY";
    case HalResult::INVALID_PARAM:   return "INVALID_PARAM";
    case HalResult::NOT_INITIALIZED: return "NOT_INITIALIZED";
    case HalResult::NOT_SUPPORTED:   return "NOT_SUPPORTED";
    case HalResult::BUFFER_FULL:     return "BUFFER_FULL";
    case HalResult::BUFFER_EMPTY:    return "BUFFER_EMPTY";
    case HalResult::KEY_NOT_FOUND:  return "KEY_NOT_FOUND";
    case HalResult::HARDWARE_FAULT:  return "HARDWARE_FAULT";
    default:                         return "UNKNOWN";
  }
}

/** Convert LogLevel to string
 * @param level LogLevel
 * @return String representation
 */
inline const char* logLevelToString(LogLevel level){
  switch(level){
    case LogLevel::NONE:    return "NONE";
    case LogLevel::ERROR:   return "ERROR";
    case LogLevel::WARN:    return "WARN";
    case LogLevel::INFO:    return "INFO";
    case LogLevel::DEBUG:   return "DEBUG";
    case LogLevel::VERBOSE: return "VERBOSE";
    default:                return "UNKNOWN";
  }
}

/** Get log level prefix character
 * @param level LogLevel
 * @return Single character prefix
 */
inline char logLevelChar(LogLevel level){
  switch(level){
    case LogLevel::ERROR:   return 'E';
    case LogLevel::WARN:    return 'W';
    case LogLevel::INFO:    return 'I';
    case LogLevel::DEBUG:   return 'D';
    case LogLevel::VERBOSE: return 'V';
    default:                return '?';
  }
}

} // namespace arcos::hal

// ============================================================
// Logging Macros (for convenience)
// ============================================================

// These macros require a global logger instance named g_hal_log
// Define HAL_LOG_ENABLED=0 to disable all logging

#ifndef HAL_LOG_ENABLED
#define HAL_LOG_ENABLED 1
#endif

#if HAL_LOG_ENABLED

#define HAL_LOG_E(tag, fmt, ...) \
  if(g_hal_log) g_hal_log->error(tag, fmt, ##__VA_ARGS__)

#define HAL_LOG_W(tag, fmt, ...) \
  if(g_hal_log) g_hal_log->warn(tag, fmt, ##__VA_ARGS__)

#define HAL_LOG_I(tag, fmt, ...) \
  if(g_hal_log) g_hal_log->info(tag, fmt, ##__VA_ARGS__)

#define HAL_LOG_D(tag, fmt, ...) \
  if(g_hal_log) g_hal_log->debug(tag, fmt, ##__VA_ARGS__)

#define HAL_LOG_V(tag, fmt, ...) \
  if(g_hal_log) g_hal_log->verbose(tag, fmt, ##__VA_ARGS__)

#define HAL_LOG_RESULT(result, tag, op) \
  if(g_hal_log) g_hal_log->logResult(result, tag, op)

#else

#define HAL_LOG_E(tag, fmt, ...)
#define HAL_LOG_W(tag, fmt, ...)
#define HAL_LOG_I(tag, fmt, ...)
#define HAL_LOG_D(tag, fmt, ...)
#define HAL_LOG_V(tag, fmt, ...)
#define HAL_LOG_RESULT(result, tag, op)

#endif

#endif // ARCOS_INCLUDE_HAL_IHAL_LOG_HPP_
