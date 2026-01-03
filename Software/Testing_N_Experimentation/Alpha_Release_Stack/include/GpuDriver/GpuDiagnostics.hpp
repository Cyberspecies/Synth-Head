/*****************************************************************
 * File:      GpuDiagnostics.hpp
 * Category:  GPU Driver / Diagnostics and Monitoring
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Comprehensive diagnostics capture and monitoring system for
 *    GPU hardware testing. Captures visual output, performance
 *    counters, internal state, and system health metrics.
 * 
 * Features:
 *    - Visual output capture and comparison
 *    - Performance counter collection
 *    - Internal state snapshots
 *    - Memory usage tracking
 *    - UART statistics
 *    - Regression detection
 *    - Diagnostic logging
 *****************************************************************/

#ifndef GPU_DIAGNOSTICS_HPP_
#define GPU_DIAGNOSTICS_HPP_

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <math.h>
#include "GpuISA.hpp"

namespace gpu {
namespace diagnostics {

using namespace gpu::isa;

// ============================================================
// Diagnostic Constants
// ============================================================

constexpr int MAX_PERF_COUNTERS = 32;
constexpr int MAX_STATE_SNAPSHOTS = 64;
constexpr int MAX_LOG_ENTRIES = 1024;
constexpr int MAX_CHECKPOINTS = 256;

// ============================================================
// Performance Counters
// ============================================================

enum class PerfCounter : uint8_t {
  // Frame counters
  FRAMES_RENDERED     = 0x00,
  FRAMES_DROPPED      = 0x01,
  FRAMES_PRESENTED    = 0x02,
  
  // Timing counters
  TOTAL_FRAME_TIME_US = 0x10,
  RENDER_TIME_US      = 0x11,
  PRESENT_TIME_US     = 0x12,
  COMMAND_TIME_US     = 0x13,
  IDLE_TIME_US        = 0x14,
  
  // Command counters
  COMMANDS_RECEIVED   = 0x20,
  COMMANDS_EXECUTED   = 0x21,
  COMMANDS_FAILED     = 0x22,
  BYTES_RECEIVED      = 0x23,
  BYTES_TRANSMITTED   = 0x24,
  
  // Draw counters
  PIXELS_DRAWN        = 0x30,
  LINES_DRAWN         = 0x31,
  RECTS_DRAWN         = 0x32,
  CIRCLES_DRAWN       = 0x33,
  TRIANGLES_DRAWN     = 0x34,
  SPRITES_DRAWN       = 0x35,
  TEXT_CHARS_DRAWN    = 0x36,
  
  // Memory counters
  MEMORY_ALLOCATIONS  = 0x40,
  MEMORY_FREES        = 0x41,
  MEMORY_PEAK_KB      = 0x42,
  MEMORY_CURRENT_KB   = 0x43,
  
  // Animation counters
  ANIMATIONS_ACTIVE   = 0x50,
  KEYFRAMES_EVALUATED = 0x51,
  TRANSITIONS_ACTIVE  = 0x52,
  
  // SDF counters
  SDF_EVALUATIONS     = 0x60,
  SDF_NODES_ACTIVE    = 0x61,
  
  // Error counters
  CRC_ERRORS          = 0x70,
  TIMEOUT_ERRORS      = 0x71,
  BUFFER_OVERFLOWS    = 0x72,
  SYNC_ERRORS         = 0x73,
  
  // UART counters
  UART_TX_BYTES       = 0x80,
  UART_RX_BYTES       = 0x81,
  UART_ERRORS         = 0x82,
  UART_OVERRUNS       = 0x83,
};

struct PerfCounterValue {
  PerfCounter counter;
  uint64_t value;
  uint32_t timestamp_ms;
  
  PerfCounterValue() : counter(PerfCounter::FRAMES_RENDERED), 
                       value(0), timestamp_ms(0) {}
};

class PerformanceCounters {
public:
  PerformanceCounters() {
    reset();
  }
  
  void reset() {
    memset(counters_, 0, sizeof(counters_));
    start_time_ms_ = 0;
  }
  
  void setStartTime(uint32_t ms) { start_time_ms_ = ms; }
  
  void increment(PerfCounter counter, uint64_t amount = 1) {
    int idx = (int)counter;
    if (idx < MAX_PERF_COUNTERS) {
      counters_[idx] += amount;
    }
  }
  
  void set(PerfCounter counter, uint64_t value) {
    int idx = (int)counter;
    if (idx < MAX_PERF_COUNTERS) {
      counters_[idx] = value;
    }
  }
  
  uint64_t get(PerfCounter counter) const {
    int idx = (int)counter;
    if (idx < MAX_PERF_COUNTERS) {
      return counters_[idx];
    }
    return 0;
  }
  
  // Get rate (per second)
  float getRate(PerfCounter counter, uint32_t current_time_ms) const {
    uint32_t elapsed = current_time_ms - start_time_ms_;
    if (elapsed == 0) return 0.0f;
    
    return (float)get(counter) * 1000.0f / (float)elapsed;
  }
  
  // Snapshot all counters
  void snapshot(PerfCounterValue* values, int max_values, int& count) const {
    count = 0;
    for (int i = 0; i < MAX_PERF_COUNTERS && count < max_values; i++) {
      if (counters_[i] > 0) {
        values[count].counter = (PerfCounter)i;
        values[count].value = counters_[i];
        count++;
      }
    }
  }

private:
  uint64_t counters_[MAX_PERF_COUNTERS];
  uint32_t start_time_ms_;
};

// ============================================================
// Internal State Snapshot
// ============================================================

struct RegisterState {
  uint32_t r[32];        // General purpose registers
  uint32_t pc;           // Program counter
  uint32_t sp;           // Stack pointer
  uint32_t flags;        // Status flags
  float fr[16];          // Float registers
};

struct FramebufferState {
  uint32_t checksum;
  int width;
  int height;
  int dirty_regions;
  uint32_t last_update_ms;
};

struct AnimationState {
  int active_count;
  int layer_count;
  float current_time;
  uint8_t playing_mask;
};

struct SystemState {
  // CPU state
  RegisterState registers;
  uint32_t stack_depth;
  uint32_t call_depth;
  
  // Memory state
  uint32_t heap_free_kb;
  uint32_t heap_used_kb;
  uint32_t largest_free_block;
  int allocation_count;
  
  // Rendering state
  FramebufferState framebuffers[2];
  int active_buffer;
  bool vsync_pending;
  
  // Animation state
  AnimationState animation;
  
  // UART state
  uint32_t uart_rx_pending;
  uint32_t uart_tx_pending;
  bool uart_error;
  
  // Timing
  uint32_t timestamp_ms;
  uint32_t uptime_ms;
  uint32_t last_frame_time_us;
  
  SystemState() {
    memset(this, 0, sizeof(SystemState));
  }
};

// ============================================================
// Visual Output Capture
// ============================================================

struct FrameCapture {
  uint8_t* pixels;       // RGB888 data
  int width;
  int height;
  uint32_t checksum;
  uint32_t frame_number;
  uint32_t timestamp_ms;
  uint32_t render_time_us;
  
  FrameCapture() : pixels(nullptr), width(0), height(0), checksum(0),
                   frame_number(0), timestamp_ms(0), render_time_us(0) {}
  
  uint32_t computeChecksum() const {
    if (!pixels) return 0;
    
    uint32_t sum = 0;
    int size = width * height * 3;
    for (int i = 0; i < size; i++) {
      sum = sum * 31 + pixels[i];
    }
    return sum;
  }
  
  // Compare against reference
  float compare(const FrameCapture& reference) const {
    if (!pixels || !reference.pixels) return 0.0f;
    if (width != reference.width || height != reference.height) return 0.0f;
    
    int matches = 0;
    int total = width * height;
    
    for (int i = 0; i < total; i++) {
      int r_diff = abs((int)pixels[i*3+0] - (int)reference.pixels[i*3+0]);
      int g_diff = abs((int)pixels[i*3+1] - (int)reference.pixels[i*3+1]);
      int b_diff = abs((int)pixels[i*3+2] - (int)reference.pixels[i*3+2]);
      
      if (r_diff <= 2 && g_diff <= 2 && b_diff <= 2) {
        matches++;
      }
    }
    
    return (float)matches / (float)total * 100.0f;
  }
  
  // Generate diff image
  void generateDiff(const FrameCapture& reference, uint8_t* diff_pixels) const {
    if (!pixels || !reference.pixels || !diff_pixels) return;
    if (width != reference.width || height != reference.height) return;
    
    int total = width * height;
    for (int i = 0; i < total; i++) {
      int r_diff = abs((int)pixels[i*3+0] - (int)reference.pixels[i*3+0]);
      int g_diff = abs((int)pixels[i*3+1] - (int)reference.pixels[i*3+1]);
      int b_diff = abs((int)pixels[i*3+2] - (int)reference.pixels[i*3+2]);
      
      // Amplify differences for visibility
      diff_pixels[i*3+0] = (uint8_t)(r_diff * 4 > 255 ? 255 : r_diff * 4);
      diff_pixels[i*3+1] = (uint8_t)(g_diff * 4 > 255 ? 255 : g_diff * 4);
      diff_pixels[i*3+2] = (uint8_t)(b_diff * 4 > 255 ? 255 : b_diff * 4);
    }
  }
};

// ============================================================
// Diagnostic Log
// ============================================================

enum class LogLevel : uint8_t {
  TRACE   = 0,
  DEBUG   = 1,
  INFO    = 2,
  WARNING = 3,
  ERROR   = 4,
  FATAL   = 5,
};

struct LogEntry {
  LogLevel level;
  uint32_t timestamp_ms;
  uint32_t frame_number;
  char source[16];
  char message[128];
  
  LogEntry() : level(LogLevel::INFO), timestamp_ms(0), frame_number(0) {
    source[0] = '\0';
    message[0] = '\0';
  }
};

class DiagnosticLog {
public:
  DiagnosticLog() : entry_count_(0), write_index_(0), min_level_(LogLevel::INFO) {}
  
  void setMinLevel(LogLevel level) { min_level_ = level; }
  
  void log(LogLevel level, const char* source, const char* fmt, ...) {
    if (level < min_level_) return;
    
    LogEntry& entry = entries_[write_index_];
    entry.level = level;
    entry.timestamp_ms = 0;  // Would be set by caller
    entry.frame_number = 0;
    
    strncpy(entry.source, source, sizeof(entry.source) - 1);
    
    va_list args;
    va_start(args, fmt);
    vsnprintf(entry.message, sizeof(entry.message), fmt, args);
    va_end(args);
    
    write_index_ = (write_index_ + 1) % MAX_LOG_ENTRIES;
    if (entry_count_ < MAX_LOG_ENTRIES) entry_count_++;
  }
  
  void trace(const char* source, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char msg[128];
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);
    log(LogLevel::TRACE, source, "%s", msg);
  }
  
  void debug(const char* source, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char msg[128];
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);
    log(LogLevel::DEBUG, source, "%s", msg);
  }
  
  void info(const char* source, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char msg[128];
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);
    log(LogLevel::INFO, source, "%s", msg);
  }
  
  void warning(const char* source, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char msg[128];
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);
    log(LogLevel::WARNING, source, "%s", msg);
  }
  
  void error(const char* source, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char msg[128];
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);
    log(LogLevel::ERROR, source, "%s", msg);
  }
  
  int getEntryCount() const { return entry_count_; }
  
  const LogEntry* getEntry(int index) const {
    if (index < 0 || index >= entry_count_) return nullptr;
    
    int actual_index;
    if (entry_count_ < MAX_LOG_ENTRIES) {
      actual_index = index;
    } else {
      actual_index = (write_index_ + index) % MAX_LOG_ENTRIES;
    }
    
    return &entries_[actual_index];
  }
  
  // Count entries by level
  int countByLevel(LogLevel level) const {
    int count = 0;
    for (int i = 0; i < entry_count_; i++) {
      const LogEntry* e = getEntry(i);
      if (e && e->level == level) count++;
    }
    return count;
  }
  
  void clear() {
    entry_count_ = 0;
    write_index_ = 0;
  }

private:
  LogEntry entries_[MAX_LOG_ENTRIES];
  int entry_count_;
  int write_index_;
  LogLevel min_level_;
};

// ============================================================
// Regression Checkpoint
// ============================================================

struct RegressionCheckpoint {
  uint32_t checkpoint_id;
  char name[32];
  
  // State at checkpoint
  uint32_t frame_checksum;
  uint32_t perf_counter_hash;
  float avg_frame_time_ms;
  uint32_t memory_used_kb;
  
  // Timing
  uint32_t timestamp_ms;
  
  RegressionCheckpoint() : checkpoint_id(0), frame_checksum(0),
                           perf_counter_hash(0), avg_frame_time_ms(0),
                           memory_used_kb(0), timestamp_ms(0) {
    name[0] = '\0';
  }
  
  // Compare against another checkpoint
  bool compare(const RegressionCheckpoint& other, float tolerance = 0.1f) const {
    // Frame must match
    if (frame_checksum != other.frame_checksum) return false;
    
    // Performance within tolerance
    float perf_diff = fabsf(avg_frame_time_ms - other.avg_frame_time_ms);
    if (perf_diff > avg_frame_time_ms * tolerance) return false;
    
    // Memory within tolerance
    float mem_diff = fabsf((float)memory_used_kb - (float)other.memory_used_kb);
    if (mem_diff > (float)memory_used_kb * tolerance) return false;
    
    return true;
  }
};

class RegressionTracker {
public:
  RegressionTracker() : checkpoint_count_(0), next_id_(1) {}
  
  int createCheckpoint(const char* name, const FrameCapture& frame,
                       const PerformanceCounters& perf, uint32_t memory_kb) {
    if (checkpoint_count_ >= MAX_CHECKPOINTS) return -1;
    
    RegressionCheckpoint& cp = checkpoints_[checkpoint_count_++];
    cp.checkpoint_id = next_id_++;
    strncpy(cp.name, name, sizeof(cp.name) - 1);
    cp.frame_checksum = frame.checksum;
    cp.avg_frame_time_ms = (float)perf.get(PerfCounter::TOTAL_FRAME_TIME_US) / 1000.0f;
    cp.memory_used_kb = memory_kb;
    
    return cp.checkpoint_id;
  }
  
  const RegressionCheckpoint* getCheckpoint(int id) const {
    for (int i = 0; i < checkpoint_count_; i++) {
      if (checkpoints_[i].checkpoint_id == (uint32_t)id) {
        return &checkpoints_[i];
      }
    }
    return nullptr;
  }
  
  const RegressionCheckpoint* findByName(const char* name) const {
    for (int i = 0; i < checkpoint_count_; i++) {
      if (strcmp(checkpoints_[i].name, name) == 0) {
        return &checkpoints_[i];
      }
    }
    return nullptr;
  }
  
  // Compare current state against checkpoint
  bool verifyCheckpoint(const char* name, const FrameCapture& frame,
                        const PerformanceCounters& perf, uint32_t memory_kb) const {
    const RegressionCheckpoint* cp = findByName(name);
    if (!cp) return false;
    
    RegressionCheckpoint current;
    current.frame_checksum = frame.checksum;
    current.avg_frame_time_ms = (float)perf.get(PerfCounter::TOTAL_FRAME_TIME_US) / 1000.0f;
    current.memory_used_kb = memory_kb;
    
    return cp->compare(current);
  }
  
  int getCheckpointCount() const { return checkpoint_count_; }
  
  void clear() {
    checkpoint_count_ = 0;
  }

private:
  RegressionCheckpoint checkpoints_[MAX_CHECKPOINTS];
  int checkpoint_count_;
  uint32_t next_id_;
};

// ============================================================
// Diagnostics System
// ============================================================

class DiagnosticsSystem {
public:
  DiagnosticsSystem() : enabled_(true), frame_number_(0) {}
  
  void enable() { enabled_ = true; }
  void disable() { enabled_ = false; }
  bool isEnabled() const { return enabled_; }
  
  // Performance counters
  PerformanceCounters& counters() { return counters_; }
  const PerformanceCounters& counters() const { return counters_; }
  
  // Logging
  DiagnosticLog& log() { return log_; }
  const DiagnosticLog& log() const { return log_; }
  
  // Regression tracking
  RegressionTracker& regression() { return regression_; }
  const RegressionTracker& regression() const { return regression_; }
  
  // Frame management
  void beginFrame(uint32_t timestamp_ms) {
    if (!enabled_) return;
    
    frame_start_time_ = timestamp_ms;
    frame_number_++;
  }
  
  void endFrame(uint32_t timestamp_ms) {
    if (!enabled_) return;
    
    uint32_t frame_time = (timestamp_ms - frame_start_time_) * 1000;  // to us
    counters_.increment(PerfCounter::TOTAL_FRAME_TIME_US, frame_time);
    counters_.increment(PerfCounter::FRAMES_RENDERED);
    
    last_state_.last_frame_time_us = frame_time;
    last_state_.timestamp_ms = timestamp_ms;
  }
  
  // State capture
  void captureState(SystemState& state) const {
    state = last_state_;
    state.timestamp_ms = frame_start_time_;
  }
  
  void updateState(const SystemState& state) {
    last_state_ = state;
  }
  
  // Quick status check
  struct HealthStatus {
    bool healthy;
    int warning_count;
    int error_count;
    float fps;
    float memory_percent;
    float temp_c;
  };
  
  HealthStatus getHealthStatus() const {
    HealthStatus status;
    
    status.warning_count = log_.countByLevel(LogLevel::WARNING);
    status.error_count = log_.countByLevel(LogLevel::ERROR);
    status.healthy = (status.error_count == 0);
    
    uint64_t frame_time = counters_.get(PerfCounter::TOTAL_FRAME_TIME_US);
    uint64_t frames = counters_.get(PerfCounter::FRAMES_RENDERED);
    if (frames > 0 && frame_time > 0) {
      float avg_frame = (float)frame_time / (float)frames;
      status.fps = 1000000.0f / avg_frame;
    } else {
      status.fps = 0;
    }
    
    status.memory_percent = 0;  // Would be calculated from state
    status.temp_c = 0;  // Would be from thermal sensor
    
    return status;
  }
  
  // Reset all diagnostics
  void reset() {
    counters_.reset();
    log_.clear();
    regression_.clear();
    frame_number_ = 0;
    last_state_ = SystemState();
  }

private:
  bool enabled_;
  uint32_t frame_number_;
  uint32_t frame_start_time_;
  
  PerformanceCounters counters_;
  DiagnosticLog log_;
  RegressionTracker regression_;
  SystemState last_state_;
};

// ============================================================
// UART Statistics
// ============================================================

struct UARTStatistics {
  // Counters
  uint64_t bytes_sent;
  uint64_t bytes_received;
  uint64_t packets_sent;
  uint64_t packets_received;
  
  // Errors
  uint32_t crc_errors;
  uint32_t framing_errors;
  uint32_t overflow_errors;
  uint32_t timeout_errors;
  
  // Performance
  uint32_t avg_latency_us;
  uint32_t max_latency_us;
  uint32_t min_latency_us;
  
  // Current state
  uint32_t tx_queue_depth;
  uint32_t rx_queue_depth;
  bool tx_busy;
  bool rx_pending;
  
  UARTStatistics() {
    memset(this, 0, sizeof(UARTStatistics));
    min_latency_us = UINT32_MAX;
  }
  
  float getErrorRate() const {
    uint64_t total = packets_sent + packets_received;
    if (total == 0) return 0.0f;
    
    uint32_t errors = crc_errors + framing_errors + overflow_errors + timeout_errors;
    return (float)errors / (float)total * 100.0f;
  }
  
  float getThroughputKBps() const {
    // Would need elapsed time for accurate calculation
    return 0.0f;
  }
  
  void recordLatency(uint32_t latency_us) {
    if (latency_us < min_latency_us) min_latency_us = latency_us;
    if (latency_us > max_latency_us) max_latency_us = latency_us;
    
    // Running average (simplified)
    avg_latency_us = (avg_latency_us * packets_received + latency_us) / 
                     (packets_received + 1);
  }
};

// ============================================================
// Diagnostics Report Generator
// ============================================================

class DiagnosticsReport {
public:
  static void generateTextReport(const DiagnosticsSystem& diag,
                                  char* buffer, size_t buffer_size) {
    int offset = 0;
    
    // Header
    offset += snprintf(buffer + offset, buffer_size - offset,
      "=== GPU Diagnostics Report ===\n\n");
    
    // Health status
    auto health = diag.getHealthStatus();
    offset += snprintf(buffer + offset, buffer_size - offset,
      "Health: %s\n"
      "FPS: %.1f\n"
      "Warnings: %d, Errors: %d\n\n",
      health.healthy ? "HEALTHY" : "UNHEALTHY",
      health.fps,
      health.warning_count, health.error_count);
    
    // Performance counters
    const auto& counters = diag.counters();
    offset += snprintf(buffer + offset, buffer_size - offset,
      "--- Performance ---\n"
      "Frames Rendered: %llu\n"
      "Frames Dropped: %llu\n"
      "Commands Executed: %llu\n"
      "Pixels Drawn: %llu\n\n",
      (unsigned long long)counters.get(PerfCounter::FRAMES_RENDERED),
      (unsigned long long)counters.get(PerfCounter::FRAMES_DROPPED),
      (unsigned long long)counters.get(PerfCounter::COMMANDS_EXECUTED),
      (unsigned long long)counters.get(PerfCounter::PIXELS_DRAWN));
    
    // Errors
    offset += snprintf(buffer + offset, buffer_size - offset,
      "--- Errors ---\n"
      "CRC Errors: %llu\n"
      "Timeout Errors: %llu\n"
      "Buffer Overflows: %llu\n"
      "Sync Errors: %llu\n\n",
      (unsigned long long)counters.get(PerfCounter::CRC_ERRORS),
      (unsigned long long)counters.get(PerfCounter::TIMEOUT_ERRORS),
      (unsigned long long)counters.get(PerfCounter::BUFFER_OVERFLOWS),
      (unsigned long long)counters.get(PerfCounter::SYNC_ERRORS));
    
    // Recent log entries
    const auto& log = diag.log();
    offset += snprintf(buffer + offset, buffer_size - offset,
      "--- Recent Log (last 10) ---\n");
    
    int start = log.getEntryCount() > 10 ? log.getEntryCount() - 10 : 0;
    for (int i = start; i < log.getEntryCount() && offset < (int)buffer_size - 100; i++) {
      const LogEntry* entry = log.getEntry(i);
      if (entry) {
        const char* level_str = "???";
        switch (entry->level) {
          case LogLevel::TRACE: level_str = "TRC"; break;
          case LogLevel::DEBUG: level_str = "DBG"; break;
          case LogLevel::INFO: level_str = "INF"; break;
          case LogLevel::WARNING: level_str = "WRN"; break;
          case LogLevel::ERROR: level_str = "ERR"; break;
          case LogLevel::FATAL: level_str = "FTL"; break;
        }
        offset += snprintf(buffer + offset, buffer_size - offset,
          "[%s][%s] %s\n", level_str, entry->source, entry->message);
      }
    }
    
    // Regression checkpoints
    const auto& regression = diag.regression();
    offset += snprintf(buffer + offset, buffer_size - offset,
      "\n--- Regression Checkpoints (%d) ---\n",
      regression.getCheckpointCount());
    
    buffer[offset] = '\0';
  }
};

} // namespace diagnostics
} // namespace gpu

#endif // GPU_DIAGNOSTICS_HPP_
