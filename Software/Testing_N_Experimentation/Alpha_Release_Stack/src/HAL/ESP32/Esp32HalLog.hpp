/*****************************************************************
 * File:      Esp32HalLog.hpp
 * Category:  src/HAL/ESP32
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    ESP32 implementation of HAL logging interface using
 *    Arduino Serial for output.
 *****************************************************************/

#ifndef ARCOS_SRC_HAL_ESP32_HAL_LOG_HPP_
#define ARCOS_SRC_HAL_ESP32_HAL_LOG_HPP_

#include "HAL/IHalLog.hpp"
#include <Arduino.h>
#include <stdarg.h>
#include <stdio.h>

namespace arcos::hal::esp32{

/** ESP32 Serial Logger Implementation */
class Esp32HalLog : public IHalLog{
private:
  static constexpr size_t LOG_BUFFER_SIZE = 256;
  
  LogLevel level_ = LogLevel::INFO;
  char buffer_[LOG_BUFFER_SIZE];
  bool initialized_ = false;
  
  void printLog(LogLevel lvl, const char* tag, const char* format, va_list args){
    if(lvl > level_ || !initialized_) return;
    
    unsigned long ms = millis();
    int len = snprintf(buffer_, LOG_BUFFER_SIZE, "[%c][%lu][%s] ", 
                       logLevelChar(lvl), ms, tag);
    
    if(len > 0 && len < (int)LOG_BUFFER_SIZE - 1){
      vsnprintf(buffer_ + len, LOG_BUFFER_SIZE - len, format, args);
    }
    
    Serial.println(buffer_);
  }

public:
  Esp32HalLog() = default;
  
  HalResult init(LogLevel level = LogLevel::INFO) override{
    level_ = level;
    initialized_ = true;
    return HalResult::OK;
  }
  
  void setLevel(LogLevel level) override{
    level_ = level;
  }
  
  LogLevel getLevel() const override{
    return level_;
  }
  
  void error(const char* tag, const char* format, ...) override{
    va_list args;
    va_start(args, format);
    printLog(LogLevel::ERROR, tag, format, args);
    va_end(args);
  }
  
  void warn(const char* tag, const char* format, ...) override{
    va_list args;
    va_start(args, format);
    printLog(LogLevel::WARN, tag, format, args);
    va_end(args);
  }
  
  void info(const char* tag, const char* format, ...) override{
    va_list args;
    va_start(args, format);
    printLog(LogLevel::INFO, tag, format, args);
    va_end(args);
  }
  
  void debug(const char* tag, const char* format, ...) override{
    va_list args;
    va_start(args, format);
    printLog(LogLevel::DEBUG, tag, format, args);
    va_end(args);
  }
  
  void verbose(const char* tag, const char* format, ...) override{
    va_list args;
    va_start(args, format);
    printLog(LogLevel::VERBOSE, tag, format, args);
    va_end(args);
  }
  
  void log(LogLevel level, const char* tag, const char* format, ...) override{
    va_list args;
    va_start(args, format);
    printLog(level, tag, format, args);
    va_end(args);
  }
  
  void logResult(HalResult result, const char* tag, const char* operation) override{
    if(result == HalResult::OK){
      info(tag, "%s: OK", operation);
    }else{
      error(tag, "%s: FAILED (%s)", operation, halResultToString(result));
    }
  }
  
  void flush() override{
    Serial.flush();
  }
};

/** ESP32 Error Handler Implementation */
class Esp32HalErrorHandler : public IHalErrorHandler{
private:
  HalResult last_result_ = HalResult::OK;
  const char* last_tag_ = "";
  char last_message_[128] = {0};
  uint32_t error_count_ = 0;
  ErrorCallback callback_ = nullptr;
  IHalLog* logger_ = nullptr;

public:
  Esp32HalErrorHandler(IHalLog* logger = nullptr) : logger_(logger){}
  
  HalResult init() override{
    last_result_ = HalResult::OK;
    last_tag_ = "";
    last_message_[0] = '\0';
    error_count_ = 0;
    return HalResult::OK;
  }
  
  void reportError(HalResult result, const char* tag, const char* message) override{
    last_result_ = result;
    last_tag_ = tag;
    strncpy(last_message_, message, sizeof(last_message_) - 1);
    last_message_[sizeof(last_message_) - 1] = '\0';
    error_count_++;
    
    if(logger_){
      logger_->error(tag, "ERROR: %s (%s)", message, halResultToString(result));
    }
    
    if(callback_){
      callback_(result, tag, message);
    }
  }
  
  void setCallback(ErrorCallback callback) override{
    callback_ = callback;
  }
  
  HalResult getLastError() const override{
    return last_result_;
  }
  
  const char* getLastErrorTag() const override{
    return last_tag_;
  }
  
  const char* getLastErrorMessage() const override{
    return last_message_;
  }
  
  uint32_t getErrorCount() const override{
    return error_count_;
  }
  
  void clearError() override{
    last_result_ = HalResult::OK;
    last_tag_ = "";
    last_message_[0] = '\0';
  }
  
  bool hasError() const override{
    return last_result_ != HalResult::OK;
  }
  
  void setLogger(IHalLog* logger){
    logger_ = logger;
  }
};

} // namespace arcos::hal::esp32

#endif // ARCOS_SRC_HAL_ESP32_HAL_LOG_HPP_
