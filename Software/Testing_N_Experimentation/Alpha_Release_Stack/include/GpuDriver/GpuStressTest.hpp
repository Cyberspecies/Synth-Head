/*****************************************************************
 * File:      GpuStressTest.hpp
 * Category:  GPU Driver / Stress Testing Framework
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Long-duration stress testing for detecting rare or non-deterministic
 *    failures including race conditions, memory coherency violations,
 *    synchronization errors, and precision drift over time.
 * 
 * Features:
 *    - Configurable stress test scenarios
 *    - Memory pressure testing
 *    - Concurrency stress testing
 *    - Thermal endurance testing
 *    - Clock/timing stress testing
 *    - Visual regression detection
 *    - Performance degradation monitoring
 *****************************************************************/

#ifndef GPU_STRESS_TEST_HPP_
#define GPU_STRESS_TEST_HPP_

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <math.h>
#include "GpuISA.hpp"
#include "GpuHardwareTestRunner.hpp"

namespace gpu {
namespace stress_test {

using namespace gpu::isa;
using namespace gpu::hardware_test;

// ============================================================
// Stress Test Constants
// ============================================================

constexpr int STRESS_DURATION_SHORT_MS = 60000;      // 1 minute
constexpr int STRESS_DURATION_MEDIUM_MS = 300000;    // 5 minutes
constexpr int STRESS_DURATION_LONG_MS = 3600000;     // 1 hour
constexpr int STRESS_DURATION_SOAK_MS = 86400000;    // 24 hours

// ============================================================
// Stress Test Types
// ============================================================

enum class StressType : uint8_t {
  MEMORY_PRESSURE     = 0x01,  // Allocate/free at high rate
  COMMAND_FLOOD       = 0x02,  // Maximum command throughput
  ANIMATION_LOAD      = 0x03,  // Many concurrent animations
  SDF_COMPLEXITY      = 0x04,  // Complex SDF scene evaluation
  RENDER_THRASH       = 0x05,  // Rapidly changing render state
  BUFFER_CYCLING      = 0x06,  // Buffer swap stress
  UART_SATURATION     = 0x07,  // Maximum UART bandwidth
  PRECISION_DRIFT     = 0x08,  // Detect accumulating errors
  THERMAL_ENDURANCE   = 0x09,  // Sustained high load
  RANDOM_OPERATIONS   = 0x0A,  // Random valid operations
  CONCURRENCY         = 0x0B,  // Async operation stress
  EDGE_CASE_FUZZING   = 0x0C,  // Boundary condition testing
};

// ============================================================
// Stress Test Configuration
// ============================================================

struct StressConfig {
  StressType type;
  uint32_t duration_ms;
  uint32_t intensity;          // 1-100
  uint32_t random_seed;
  
  // Memory stress
  uint32_t alloc_size_min;
  uint32_t alloc_size_max;
  uint32_t max_allocations;
  
  // Command stress
  uint32_t commands_per_frame;
  uint32_t target_fps;
  
  // Animation stress
  uint32_t animation_count;
  uint32_t keyframes_per_anim;
  
  // SDF stress
  uint32_t sdf_node_count;
  uint32_t sdf_operations;
  
  // Precision tracking
  float precision_tolerance;
  float drift_threshold;
  
  // Thermal limits
  float max_temp_c;
  float target_temp_c;
  
  // Callbacks
  bool stop_on_failure;
  int max_failures;
  
  StressConfig() {
    memset(this, 0, sizeof(StressConfig));
    type = StressType::RANDOM_OPERATIONS;
    duration_ms = STRESS_DURATION_SHORT_MS;
    intensity = 50;
    random_seed = 12345;
    
    alloc_size_min = 64;
    alloc_size_max = 4096;
    max_allocations = 100;
    
    commands_per_frame = 100;
    target_fps = 60;
    
    animation_count = 16;
    keyframes_per_anim = 32;
    
    sdf_node_count = 32;
    sdf_operations = 64;
    
    precision_tolerance = 0.001f;
    drift_threshold = 0.01f;
    
    max_temp_c = 85.0f;
    target_temp_c = 70.0f;
    
    stop_on_failure = false;
    max_failures = 1000;
  }
};

// ============================================================
// Stress Test Results
// ============================================================

struct StressResult {
  // Execution info
  StressType type;
  uint32_t duration_ms;
  uint32_t iterations;
  
  // Pass/fail
  bool passed;
  int failure_count;
  FailureCategory primary_failure;
  
  // Performance
  PerformanceMetrics perf_start;
  PerformanceMetrics perf_end;
  float perf_degradation_percent;
  
  // Precision
  float max_precision_error;
  float accumulated_drift;
  int precision_failures;
  
  // Memory
  uint32_t peak_memory_kb;
  uint32_t leak_detected_bytes;
  int allocation_failures;
  
  // Thermal
  float peak_temp_c;
  float avg_temp_c;
  int thermal_throttle_count;
  
  // Timing
  uint32_t worst_frame_time_us;
  uint32_t timing_violations;
  
  // Errors captured
  FailureLog failures;
  
  StressResult() {
    memset(this, 0, sizeof(StressResult));
    passed = true;
  }
  
  void recordFailure(const FailureRecord& failure) {
    failures.logFailure(failure);
    failure_count++;
    passed = false;
    
    if (primary_failure == FailureCategory::NONE) {
      primary_failure = failure.category;
    }
  }
};

// ============================================================
// Random Number Generator (for reproducible tests)
// ============================================================

class StressRNG {
public:
  StressRNG(uint32_t seed = 12345) : state_(seed) {}
  
  void setSeed(uint32_t seed) { state_ = seed; }
  uint32_t getSeed() const { return state_; }
  
  uint32_t next() {
    // xorshift32
    state_ ^= state_ << 13;
    state_ ^= state_ >> 17;
    state_ ^= state_ << 5;
    return state_;
  }
  
  uint32_t range(uint32_t min, uint32_t max) {
    if (min >= max) return min;
    return min + (next() % (max - min + 1));
  }
  
  float nextFloat() {
    return (float)(next() & 0xFFFFFF) / (float)0xFFFFFF;
  }
  
  float rangeFloat(float min, float max) {
    return min + nextFloat() * (max - min);
  }
  
  bool coinFlip(float probability = 0.5f) {
    return nextFloat() < probability;
  }

private:
  uint32_t state_;
};

// ============================================================
// Memory Stress Test
// ============================================================

class MemoryStressTest {
public:
  struct Allocation {
    uint8_t* ptr;
    uint32_t size;
    uint32_t pattern;
    bool valid;
  };
  
  MemoryStressTest(const StressConfig& config) 
    : config_(config), rng_(config.random_seed), alloc_count_(0) {}
  
  void reset() {
    // Free all allocations
    for (int i = 0; i < alloc_count_; i++) {
      if (allocations_[i].ptr) {
        delete[] allocations_[i].ptr;
        allocations_[i].ptr = nullptr;
      }
    }
    alloc_count_ = 0;
  }
  
  bool runIteration(StressResult& result) {
    bool success = true;
    
    // Random operation: allocate, free, or verify
    int op = rng_.range(0, 2);
    
    if (op == 0 && alloc_count_ < (int)config_.max_allocations) {
      // Allocate
      success = doAllocate(result);
    } else if (op == 1 && alloc_count_ > 0) {
      // Free random allocation
      success = doFree(result);
    } else {
      // Verify all allocations
      success = doVerify(result);
    }
    
    return success;
  }
  
private:
  bool doAllocate(StressResult& result) {
    uint32_t size = rng_.range(config_.alloc_size_min, config_.alloc_size_max);
    
    uint8_t* ptr = new (std::nothrow) uint8_t[size];
    if (!ptr) {
      result.allocation_failures++;
      return false;
    }
    
    // Fill with pattern
    uint32_t pattern = rng_.next();
    for (uint32_t i = 0; i < size; i++) {
      ptr[i] = (uint8_t)((pattern + i) & 0xFF);
    }
    
    // Store allocation
    Allocation& alloc = allocations_[alloc_count_++];
    alloc.ptr = ptr;
    alloc.size = size;
    alloc.pattern = pattern;
    alloc.valid = true;
    
    return true;
  }
  
  bool doFree(StressResult& result) {
    if (alloc_count_ == 0) return true;
    
    int idx = rng_.range(0, alloc_count_ - 1);
    
    // Verify before freeing
    if (!verifyAllocation(allocations_[idx])) {
      FailureRecord failure;
      failure.category = FailureCategory::MEMORY_CORRUPT;
      failure.setMessage("Memory corruption detected before free");
      result.recordFailure(failure);
      return false;
    }
    
    delete[] allocations_[idx].ptr;
    
    // Swap with last
    allocations_[idx] = allocations_[--alloc_count_];
    
    return true;
  }
  
  bool doVerify(StressResult& result) {
    for (int i = 0; i < alloc_count_; i++) {
      if (!verifyAllocation(allocations_[i])) {
        FailureRecord failure;
        failure.category = FailureCategory::MEMORY_CORRUPT;
        failure.setMessage("Memory corruption detected");
        result.recordFailure(failure);
        return false;
      }
    }
    return true;
  }
  
  bool verifyAllocation(const Allocation& alloc) {
    if (!alloc.ptr || !alloc.valid) return true;
    
    for (uint32_t i = 0; i < alloc.size; i++) {
      uint8_t expected = (uint8_t)((alloc.pattern + i) & 0xFF);
      if (alloc.ptr[i] != expected) {
        return false;
      }
    }
    return true;
  }
  
  StressConfig config_;
  StressRNG rng_;
  Allocation allocations_[256];
  int alloc_count_;
};

// ============================================================
// Precision Drift Test
// ============================================================

class PrecisionDriftTest {
public:
  PrecisionDriftTest(const StressConfig& config)
    : config_(config), iteration_(0) {}
  
  void reset() {
    iteration_ = 0;
    accumulated_error_ = 0.0f;
    max_error_ = 0.0f;
    
    // Initialize test values
    fixed_accumulator_ = Fixed16_16::fromFloat(0.0f);
    float_accumulator_ = 0.0f;
  }
  
  bool runIteration(StressResult& result) {
    iteration_++;
    
    // Perform same operation in fixed-point and float
    float delta = 0.001f;  // Small increment
    
    fixed_accumulator_ = fixed_accumulator_ + Fixed16_16::fromFloat(delta);
    float_accumulator_ += delta;
    
    // Compare results periodically
    if (iteration_ % 1000 == 0) {
      float fixed_value = fixed_accumulator_.toFloat();
      float error = fabsf(fixed_value - float_accumulator_);
      
      accumulated_error_ = error;
      if (error > max_error_) max_error_ = error;
      
      result.accumulated_drift = accumulated_error_;
      result.max_precision_error = max_error_;
      
      // Check for drift threshold
      if (error > config_.drift_threshold) {
        FailureRecord failure;
        failure.category = FailureCategory::PRECISION;
        failure.expected_value = float_accumulator_;
        failure.actual_value = fixed_value;
        failure.deviation = error;
        failure.setMessage("Precision drift exceeded threshold");
        result.recordFailure(failure);
        return false;
      }
    }
    
    // Also test trigonometric drift
    if (iteration_ % 100 == 0) {
      return testTrigDrift(result);
    }
    
    return true;
  }
  
private:
  bool testTrigDrift(StressResult& result) {
    // Sin/cos should satisfy identity
    float angle = (float)iteration_ * 0.01f;
    float s = sinf(angle);
    float c = cosf(angle);
    float identity = s * s + c * c;
    
    float error = fabsf(identity - 1.0f);
    
    if (error > config_.precision_tolerance) {
      FailureRecord failure;
      failure.category = FailureCategory::PRECISION;
      failure.expected_value = 1.0f;
      failure.actual_value = identity;
      failure.deviation = error;
      failure.setMessage("Trig identity drift");
      result.recordFailure(failure);
      return false;
    }
    
    return true;
  }
  
  StressConfig config_;
  int iteration_;
  float accumulated_error_ = 0.0f;
  float max_error_ = 0.0f;
  
  Fixed16_16 fixed_accumulator_;
  float float_accumulator_;
};

// ============================================================
// Command Flood Test
// ============================================================

class CommandFloodTest {
public:
  CommandFloodTest(const StressConfig& config)
    : config_(config), rng_(config.random_seed), frame_count_(0) {}
  
  void reset() {
    frame_count_ = 0;
  }
  
  // Generate a frame's worth of commands
  int generateFrame(uint8_t* buffer, size_t buffer_size) {
    BytecodeWriter writer(buffer, buffer_size);
    
    int commands = config_.commands_per_frame;
    
    for (int i = 0; i < commands && writer.size() < buffer_size - 20; i++) {
      generateRandomCommand(writer);
    }
    
    // End with sync
    writer.writeOpcode(Opcode::SYNC);
    writer.writeU8(0);  // Sync type
    
    frame_count_++;
    return writer.size();
  }
  
  bool validateFrameTiming(uint32_t frame_time_us, StressResult& result) {
    uint32_t target_time_us = 1000000 / config_.target_fps;
    
    if (frame_time_us > result.worst_frame_time_us) {
      result.worst_frame_time_us = frame_time_us;
    }
    
    // Allow 50% tolerance
    if (frame_time_us > target_time_us * 1.5f) {
      result.timing_violations++;
      
      if (result.timing_violations > 10) {
        FailureRecord failure;
        failure.category = FailureCategory::TIMING;
        failure.expected_value = (float)target_time_us;
        failure.actual_value = (float)frame_time_us;
        failure.frame_time_us = frame_time_us;
        failure.setMessage("Frame timing violation");
        result.recordFailure(failure);
        return false;
      }
    }
    
    return true;
  }

private:
  void generateRandomCommand(BytecodeWriter& writer) {
    // Weight towards drawing commands
    int cmd_type = rng_.range(0, 9);
    
    switch (cmd_type) {
      case 0:
      case 1:
      case 2:
        // Draw pixel
        writer.writeOpcode(Opcode::SET_PIXEL);
        writer.writeU16(rng_.range(0, 127));  // x
        writer.writeU16(rng_.range(0, 63));   // y
        writer.writeU8(rng_.range(0, 255));   // r
        writer.writeU8(rng_.range(0, 255));   // g
        writer.writeU8(rng_.range(0, 255));   // b
        break;
        
      case 3:
      case 4:
        // Draw line
        writer.writeOpcode(Opcode::DRAW_LINE);
        writer.writeU16(rng_.range(0, 127));  // x1
        writer.writeU16(rng_.range(0, 63));   // y1
        writer.writeU16(rng_.range(0, 127));  // x2
        writer.writeU16(rng_.range(0, 63));   // y2
        writer.writeU8(rng_.range(0, 255));   // color
        break;
        
      case 5:
      case 6:
        // Draw rect
        writer.writeOpcode(Opcode::FILL_RECT);
        writer.writeU16(rng_.range(0, 100));  // x
        writer.writeU16(rng_.range(0, 50));   // y
        writer.writeU16(rng_.range(1, 30));   // w
        writer.writeU16(rng_.range(1, 15));   // h
        writer.writeU8(rng_.range(0, 255));   // color
        break;
        
      case 7:
        // Draw circle
        writer.writeOpcode(Opcode::FILL_CIRCLE);
        writer.writeU16(rng_.range(10, 117)); // cx
        writer.writeU16(rng_.range(10, 53));  // cy
        writer.writeU16(rng_.range(2, 15));   // r
        writer.writeU8(rng_.range(0, 255));   // color
        break;
        
      case 8:
        // Arithmetic operation
        writer.writeOpcode(Opcode::ADD);
        writer.writeU8(rng_.range(0, 15));    // dest
        writer.writeU8(rng_.range(0, 15));    // src1
        writer.writeU8(rng_.range(0, 15));    // src2
        break;
        
      case 9:
        // NOP (padding)
        writer.writeOpcode(Opcode::NOP);
        break;
    }
  }
  
  StressConfig config_;
  StressRNG rng_;
  int frame_count_;
};

// ============================================================
// Concurrent Operations Test
// ============================================================

class ConcurrencyStressTest {
public:
  ConcurrencyStressTest(const StressConfig& config)
    : config_(config), rng_(config.random_seed) {}
  
  // Generate interleaved operations that could cause race conditions
  int generateConcurrentBatch(uint8_t* buffer, size_t buffer_size, int batch) {
    BytecodeWriter writer(buffer, buffer_size);
    
    // Shared resource IDs
    uint8_t shared_sprite = (batch % 4);
    uint8_t shared_anim = (batch % 8);
    
    // Multiple operations on same resources
    for (int i = 0; i < 10; i++) {
      // Randomly choose operation type
      switch (rng_.range(0, 4)) {
        case 0:
          // Read from shared sprite
          writer.writeOpcode(Opcode::SPRITE_DRAW);
          writer.writeU8(shared_sprite);
          writer.writeU16(rng_.range(0, 100));
          writer.writeU16(rng_.range(0, 50));
          break;
          
        case 1:
          // Modify shared animation
          writer.writeOpcode(Opcode::ANIM_KEYFRAME);
          writer.writeU8(shared_anim);
          writer.writeFloat(rng_.nextFloat());
          writer.writeU8(0);  // Property
          writer.writeFloat(rng_.rangeFloat(0, 100));
          break;
          
        case 2:
          // Play/stop animation
          if (rng_.coinFlip()) {
            writer.writeOpcode(Opcode::ANIM_PLAY);
          } else {
            writer.writeOpcode(Opcode::ANIM_STOP);
          }
          writer.writeU8(shared_anim);
          break;
          
        case 3:
          // Buffer swap (if mid-frame could cause tearing)
          writer.writeOpcode(Opcode::PRESENT);
          break;
          
        case 4:
          // Memory operation on shared region
          writer.writeOpcode(Opcode::STORE);
          writer.writeU8(0);  // Value register
          writer.writeU16(batch * 64 + i);  // Overlapping addresses
          break;
      }
    }
    
    return writer.size();
  }
  
  bool validateMemoryCoherency(const uint8_t* memory, size_t size, 
                                StressResult& result) {
    // Check for unexpected values indicating race conditions
    // This is simplified - real implementation would track expected state
    
    // Look for obviously corrupted patterns
    int suspicious = 0;
    for (size_t i = 0; i < size - 4; i++) {
      // Check for repeating patterns that shouldn't occur
      if (memory[i] == 0xDE && memory[i+1] == 0xAD &&
          memory[i+2] == 0xBE && memory[i+3] == 0xEF) {
        suspicious++;
      }
    }
    
    if (suspicious > 0) {
      FailureRecord failure;
      failure.category = FailureCategory::RACE_CONDITION;
      failure.setMessage("Memory coherency violation detected");
      result.recordFailure(failure);
      return false;
    }
    
    return true;
  }

private:
  StressConfig config_;
  StressRNG rng_;
};

// ============================================================
// Stress Test Executor
// ============================================================

class StressTestExecutor {
public:
  typedef uint32_t (*GetTimeFunc)();
  typedef void (*DelayFunc)(uint32_t ms);
  typedef bool (*SendCommandFunc)(const uint8_t* data, size_t len);
  typedef ThermalState (*GetThermalFunc)();
  typedef void (*LogFunc)(const char* message);
  
  struct Callbacks {
    GetTimeFunc get_time;
    DelayFunc delay;
    SendCommandFunc send_command;
    GetThermalFunc get_thermal;
    LogFunc log;
  };
  
  StressTestExecutor() {}
  
  void setCallbacks(const Callbacks& cb) { callbacks_ = cb; }
  
  StressResult runMemoryStress(const StressConfig& config) {
    StressResult result;
    result.type = StressType::MEMORY_PRESSURE;
    
    MemoryStressTest test(config);
    uint32_t start = getTime();
    uint32_t end_time = start + config.duration_ms;
    
    log("Starting memory stress test (duration: %dms)", config.duration_ms);
    
    while (getTime() < end_time && result.failure_count < config.max_failures) {
      if (!test.runIteration(result)) {
        if (config.stop_on_failure) break;
      }
      result.iterations++;
      
      // Progress report
      if (result.iterations % 10000 == 0) {
        log("Memory stress: %d iterations, %d failures", 
            result.iterations, result.failure_count);
      }
    }
    
    test.reset();
    result.duration_ms = getTime() - start;
    result.passed = (result.failure_count == 0);
    
    log("Memory stress complete: %s (%d iterations, %d failures)",
        result.passed ? "PASSED" : "FAILED", result.iterations, result.failure_count);
    
    return result;
  }
  
  StressResult runPrecisionDriftTest(const StressConfig& config) {
    StressResult result;
    result.type = StressType::PRECISION_DRIFT;
    
    PrecisionDriftTest test(config);
    test.reset();
    
    uint32_t start = getTime();
    uint32_t end_time = start + config.duration_ms;
    
    log("Starting precision drift test (duration: %dms)", config.duration_ms);
    
    while (getTime() < end_time && result.failure_count < config.max_failures) {
      if (!test.runIteration(result)) {
        if (config.stop_on_failure) break;
      }
      result.iterations++;
      
      if (result.iterations % 100000 == 0) {
        log("Precision drift: %d iterations, drift=%.6f, max_error=%.6f",
            result.iterations, result.accumulated_drift, result.max_precision_error);
      }
    }
    
    result.duration_ms = getTime() - start;
    result.passed = (result.failure_count == 0);
    
    log("Precision drift test complete: %s (drift=%.6f)",
        result.passed ? "PASSED" : "FAILED", result.accumulated_drift);
    
    return result;
  }
  
  StressResult runCommandFloodTest(const StressConfig& config) {
    StressResult result;
    result.type = StressType::COMMAND_FLOOD;
    
    CommandFloodTest test(config);
    uint8_t buffer[4096];
    
    uint32_t start = getTime();
    uint32_t end_time = start + config.duration_ms;
    uint32_t target_frame_time = 1000000 / config.target_fps;
    
    log("Starting command flood test (duration: %dms, target: %dfps)",
        config.duration_ms, config.target_fps);
    
    while (getTime() < end_time && result.failure_count < config.max_failures) {
      uint32_t frame_start = getTime();
      
      // Generate and send frame
      int size = test.generateFrame(buffer, sizeof(buffer));
      
      if (!sendCommand(buffer, size)) {
        FailureRecord failure;
        failure.category = FailureCategory::SYNC_ERROR;
        failure.setMessage("Command send failed");
        result.recordFailure(failure);
        
        if (config.stop_on_failure) break;
      }
      
      uint32_t frame_time = (getTime() - frame_start) * 1000;  // Convert to us
      
      if (!test.validateFrameTiming(frame_time, result)) {
        if (config.stop_on_failure) break;
      }
      
      result.iterations++;
      
      // Maintain target frame rate
      if (frame_time < target_frame_time) {
        delay((target_frame_time - frame_time) / 1000);
      }
      
      // Check thermal
      ThermalState thermal = getThermal();
      if (thermal.cpu_temp_c > result.peak_temp_c) {
        result.peak_temp_c = thermal.cpu_temp_c;
      }
      
      if (thermal.cpu_temp_c > config.max_temp_c) {
        log("Thermal limit reached (%.1fC), cooling...", thermal.cpu_temp_c);
        delay(5000);
        result.thermal_throttle_count++;
      }
    }
    
    result.duration_ms = getTime() - start;
    result.passed = (result.failure_count == 0);
    
    log("Command flood test complete: %s (%d frames, worst: %dus)",
        result.passed ? "PASSED" : "FAILED", 
        result.iterations, result.worst_frame_time_us);
    
    return result;
  }
  
  StressResult runThermalEnduranceTest(const StressConfig& config) {
    StressResult result;
    result.type = StressType::THERMAL_ENDURANCE;
    
    uint32_t start = getTime();
    uint32_t end_time = start + config.duration_ms;
    
    float temp_sum = 0;
    int temp_samples = 0;
    
    log("Starting thermal endurance test (duration: %dms, target: %.1fC)",
        config.duration_ms, config.target_temp_c);
    
    // Generate load until thermal target reached
    uint8_t buffer[1024];
    CommandFloodTest flood(config);
    
    while (getTime() < end_time) {
      // Generate load
      int size = flood.generateFrame(buffer, sizeof(buffer));
      sendCommand(buffer, size);
      result.iterations++;
      
      // Sample thermal
      ThermalState thermal = getThermal();
      temp_sum += thermal.cpu_temp_c;
      temp_samples++;
      
      if (thermal.cpu_temp_c > result.peak_temp_c) {
        result.peak_temp_c = thermal.cpu_temp_c;
      }
      
      // Check for thermal failures
      if (thermal.cpu_temp_c > config.max_temp_c) {
        FailureRecord failure;
        failure.category = FailureCategory::THERMAL;
        failure.cpu_temp_c = thermal.cpu_temp_c;
        failure.setMessage("Exceeded thermal limit");
        result.recordFailure(failure);
        
        // Mandatory cool-down
        log("Thermal failure at %.1fC, cooling...", thermal.cpu_temp_c);
        delay(10000);
        result.thermal_throttle_count++;
        
        if (config.stop_on_failure) break;
      }
      
      // Report every 10 seconds
      if (result.iterations % 600 == 0) {  // Assuming ~60fps
        log("Thermal endurance: current=%.1fC, peak=%.1fC, avg=%.1fC",
            thermal.cpu_temp_c, result.peak_temp_c, temp_sum / temp_samples);
      }
    }
    
    result.avg_temp_c = temp_sum / temp_samples;
    result.duration_ms = getTime() - start;
    result.passed = (result.failure_count == 0);
    
    log("Thermal endurance test complete: %s (peak=%.1fC, avg=%.1fC)",
        result.passed ? "PASSED" : "FAILED", result.peak_temp_c, result.avg_temp_c);
    
    return result;
  }
  
  // Run all stress tests in sequence
  void runFullStressSuite(const StressConfig& base_config) {
    log("=== Starting Full Stress Test Suite ===");
    
    // Memory stress
    StressConfig mem_config = base_config;
    mem_config.type = StressType::MEMORY_PRESSURE;
    mem_config.duration_ms = STRESS_DURATION_SHORT_MS;
    StressResult mem_result = runMemoryStress(mem_config);
    
    // Precision drift
    StressConfig prec_config = base_config;
    prec_config.type = StressType::PRECISION_DRIFT;
    prec_config.duration_ms = STRESS_DURATION_SHORT_MS;
    StressResult prec_result = runPrecisionDriftTest(prec_config);
    
    // Command flood
    StressConfig flood_config = base_config;
    flood_config.type = StressType::COMMAND_FLOOD;
    flood_config.duration_ms = STRESS_DURATION_SHORT_MS;
    StressResult flood_result = runCommandFloodTest(flood_config);
    
    // Thermal endurance
    StressConfig thermal_config = base_config;
    thermal_config.type = StressType::THERMAL_ENDURANCE;
    thermal_config.duration_ms = STRESS_DURATION_MEDIUM_MS;
    StressResult thermal_result = runThermalEnduranceTest(thermal_config);
    
    // Summary
    log("=== Stress Test Suite Complete ===");
    log("Memory:    %s", mem_result.passed ? "PASSED" : "FAILED");
    log("Precision: %s", prec_result.passed ? "PASSED" : "FAILED");
    log("Flood:     %s", flood_result.passed ? "PASSED" : "FAILED");
    log("Thermal:   %s", thermal_result.passed ? "PASSED" : "FAILED");
  }

private:
  uint32_t getTime() {
    return callbacks_.get_time ? callbacks_.get_time() : 0;
  }
  
  void delay(uint32_t ms) {
    if (callbacks_.delay) callbacks_.delay(ms);
  }
  
  bool sendCommand(const uint8_t* data, size_t len) {
    return callbacks_.send_command ? callbacks_.send_command(data, len) : false;
  }
  
  ThermalState getThermal() {
    return callbacks_.get_thermal ? callbacks_.get_thermal() : ThermalState();
  }
  
  void log(const char* fmt, ...) {
    if (!callbacks_.log) return;
    
    char buffer[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    
    callbacks_.log(buffer);
  }
  
  Callbacks callbacks_;
};

} // namespace stress_test
} // namespace gpu

#endif // GPU_STRESS_TEST_HPP_
