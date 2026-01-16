/*****************************************************************
 * File:      SyncBuffer.hpp
 * Category:  Application/Core
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Thread-safe double/triple buffering system for passing data
 *    between cores without blocking. Uses lock-free atomic
 *    operations where possible.
 * 
 *    Use cases:
 *    - Passing animation parameters from Core 0 to Core 1
 *    - Passing sensor data from polling tasks to main loop
 *    - Framebuffer swapping
 *****************************************************************/

#ifndef ARCOS_APPLICATION_SYNC_BUFFER_HPP_
#define ARCOS_APPLICATION_SYNC_BUFFER_HPP_

#include <stdint.h>
#include <cstring>
#include <atomic>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

namespace Application {

// ============================================================
// Simple Double Buffer (Mutex-based)
// ============================================================

/** Thread-safe double buffer for any copyable type
 * @tparam T Data type (must be copyable)
 */
template<typename T>
class DoubleBuffer {
public:
  DoubleBuffer()
    : writeIndex_(0)
    , readIndex_(0)
    , hasNew_(false)
    , mutex_(nullptr)
  {
    mutex_ = xSemaphoreCreateMutex();
  }
  
  ~DoubleBuffer() {
    if (mutex_) {
      vSemaphoreDelete(mutex_);
    }
  }
  
  /** Write new data (producer, e.g., Core 0) */
  bool write(const T& data) {
    if (!mutex_) return false;
    
    if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(10)) != pdTRUE) {
      return false;
    }
    
    int idx = 1 - readIndex_;  // Write to non-active buffer
    buffers_[idx] = data;
    writeIndex_ = idx;
    hasNew_ = true;
    
    xSemaphoreGive(mutex_);
    return true;
  }
  
  /** Read latest data (consumer, e.g., Core 1)
   * @param out Output data
   * @return true if new data was available
   */
  bool read(T& out) {
    if (!mutex_) return false;
    
    if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(10)) != pdTRUE) {
      return false;
    }
    
    bool wasNew = hasNew_;
    if (hasNew_) {
      readIndex_ = writeIndex_;
      hasNew_ = false;
    }
    out = buffers_[readIndex_];
    
    xSemaphoreGive(mutex_);
    return wasNew;
  }
  
  /** Peek at current read buffer without consuming */
  bool peek(T& out) {
    if (!mutex_) return false;
    
    if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(10)) != pdTRUE) {
      return false;
    }
    
    out = buffers_[readIndex_];
    bool wasNew = hasNew_;
    
    xSemaphoreGive(mutex_);
    return wasNew;
  }
  
  /** Check if new data is available */
  bool hasNewData() const {
    return hasNew_;
  }
  
private:
  T buffers_[2];
  volatile int writeIndex_;
  volatile int readIndex_;
  volatile bool hasNew_;
  SemaphoreHandle_t mutex_;
};

// ============================================================
// Triple Buffer (Lock-Free)
// ============================================================

/** Lock-free triple buffer for high-frequency data transfer
 * Producer can write continuously without blocking consumer.
 * @tparam T Data type (must be copyable)
 */
template<typename T>
class TripleBuffer {
public:
  TripleBuffer()
    : writeIdx_(0)
    , cleanIdx_(1)
    , readIdx_(2)
    , newWrite_(false)
  {}
  
  /** Producer: Get buffer to write into */
  T& getWriteBuffer() {
    return buffers_[writeIdx_.load(std::memory_order_relaxed)];
  }
  
  /** Producer: Mark write complete and swap */
  void publishWrite() {
    // Swap write and clean indices
    int w = writeIdx_.load(std::memory_order_relaxed);
    int c = cleanIdx_.exchange(w, std::memory_order_acq_rel);
    writeIdx_.store(c, std::memory_order_release);
    newWrite_.store(true, std::memory_order_release);
  }
  
  /** Consumer: Check if new data available */
  bool hasNewData() const {
    return newWrite_.load(std::memory_order_acquire);
  }
  
  /** Consumer: Get buffer to read from
   * @return true if this is new data
   */
  bool swapAndRead(T& out) {
    bool wasNew = newWrite_.exchange(false, std::memory_order_acq_rel);
    
    if (wasNew) {
      // Swap read and clean indices
      int r = readIdx_.load(std::memory_order_relaxed);
      int c = cleanIdx_.exchange(r, std::memory_order_acq_rel);
      readIdx_.store(c, std::memory_order_release);
    }
    
    out = buffers_[readIdx_.load(std::memory_order_acquire)];
    return wasNew;
  }
  
  /** Consumer: Peek without swapping */
  const T& peekRead() const {
    return buffers_[readIdx_.load(std::memory_order_acquire)];
  }
  
private:
  T buffers_[3];
  std::atomic<int> writeIdx_;
  std::atomic<int> cleanIdx_;
  std::atomic<int> readIdx_;
  std::atomic<bool> newWrite_;
};

// ============================================================
// Animation State Buffer
// ============================================================

/** Animation parameters passed from Core 0 to Core 1 */
struct AnimationParams {
  // Eye position
  float lookX = 0.0f;
  float lookY = 0.0f;
  float blinkProgress = 0.0f;
  
  // Expression
  uint8_t expressionId = 0;
  float expressionBlend = 0.0f;
  
  // Shader
  uint8_t shaderType = 1;     // 0=solid, 1=rainbow, 2=gradient, 3=pulse, 4=plasma
  float shaderSpeed = 1.0f;
  
  // Colors
  uint8_t primaryR = 255;
  uint8_t primaryG = 255;
  uint8_t primaryB = 255;
  uint8_t secondaryR = 0;
  uint8_t secondaryG = 0;
  uint8_t secondaryB = 255;
  
  // Display settings
  uint8_t brightness = 80;
  bool mirrorMode = true;
  
  // Manual scene control (pauses animation pipeline)
  bool paused = true;  // Default to PAUSED - animation only runs when explicitly enabled
  
  // Frame counter from producer (for debugging)
  uint32_t frameId = 0;
};

/** Convenience alias for animation parameter buffer */
using AnimationBuffer = TripleBuffer<AnimationParams>;

// ============================================================
// Sensor Data Buffer
// ============================================================

/** Sensor data passed from sensor task to main loop */
struct SensorData {
  // IMU
  float accelX = 0.0f;
  float accelY = 0.0f;
  float accelZ = 0.0f;
  float gyroX = 0.0f;
  float gyroY = 0.0f;
  float gyroZ = 0.0f;
  float pitch = 0.0f;
  float roll = 0.0f;
  float yaw = 0.0f;
  
  // Environmental
  float temperature = 0.0f;
  float humidity = 0.0f;
  float pressure = 0.0f;
  
  // GPS
  float latitude = 0.0f;
  float longitude = 0.0f;
  float altitude = 0.0f;
  float speed = 0.0f;
  uint8_t satellites = 0;
  bool gpsValid = false;
  
  // Microphone
  float audioLevel = -60.0f;   // dB
  uint8_t audioLevelPercent = 0;
  
  // Timestamp
  uint32_t timestampMs = 0;
};

/** Convenience alias for sensor data buffer */
using SensorBuffer = TripleBuffer<SensorData>;

// ============================================================
// Ring Buffer for Commands/Events
// ============================================================

/** Simple thread-safe ring buffer for command queues
 * @tparam T Item type
 * @tparam SIZE Buffer capacity
 */
template<typename T, size_t SIZE>
class RingBuffer {
public:
  RingBuffer()
    : head_(0)
    , tail_(0)
    , count_(0)
    , mutex_(nullptr)
  {
    mutex_ = xSemaphoreCreateMutex();
  }
  
  ~RingBuffer() {
    if (mutex_) {
      vSemaphoreDelete(mutex_);
    }
  }
  
  /** Push item (producer) */
  bool push(const T& item) {
    if (!mutex_) return false;
    
    if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(10)) != pdTRUE) {
      return false;
    }
    
    if (count_ >= SIZE) {
      xSemaphoreGive(mutex_);
      return false;  // Full
    }
    
    buffer_[head_] = item;
    head_ = (head_ + 1) % SIZE;
    count_ = count_ + 1;
    
    xSemaphoreGive(mutex_);
    return true;
  }
  
  /** Pop item (consumer) */
  bool pop(T& out) {
    if (!mutex_) return false;
    
    if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(10)) != pdTRUE) {
      return false;
    }
    
    if (count_ == 0) {
      xSemaphoreGive(mutex_);
      return false;  // Empty
    }
    
    out = buffer_[tail_];
    tail_ = (tail_ + 1) % SIZE;
    count_ = count_ - 1;
    
    xSemaphoreGive(mutex_);
    return true;
  }
  
  /** Check if empty */
  bool isEmpty() const {
    return count_ == 0;
  }
  
  /** Check if full */
  bool isFull() const {
    return count_ >= SIZE;
  }
  
  /** Get current count */
  size_t size() const {
    return count_;
  }
  
  /** Clear buffer */
  void clear() {
    if (!mutex_) return;
    if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(10)) == pdTRUE) {
      head_ = 0;
      tail_ = 0;
      count_ = 0;
      xSemaphoreGive(mutex_);
    }
  }
  
private:
  T buffer_[SIZE];
  volatile size_t head_;
  volatile size_t tail_;
  volatile size_t count_;
  SemaphoreHandle_t mutex_;
};

// ============================================================
// Event Types
// ============================================================

enum class EventType : uint8_t {
  NONE = 0,
  BUTTON_PRESS,
  BUTTON_RELEASE,
  BLINK_START,
  BLINK_END,
  EXPRESSION_CHANGE,
  LOOK_UPDATE,
  SHAKE_DETECTED,
  SOUND_PEAK,
  TIMER_TICK,
};

struct Event {
  EventType type = EventType::NONE;
  uint32_t timestamp = 0;
  union {
    uint8_t buttonId;
    uint8_t expressionId;
    float floatValue;
    uint32_t intValue;
  } data;
  
  Event() : type(EventType::NONE), timestamp(0) { data.intValue = 0; }
  Event(EventType t, uint32_t ts = 0) : type(t), timestamp(ts) { data.intValue = 0; }
};

/** Event queue - producer can be any core, consumer is typically Core 0 */
using EventQueue = RingBuffer<Event, 32>;

} // namespace Application

#endif // ARCOS_APPLICATION_SYNC_BUFFER_HPP_
