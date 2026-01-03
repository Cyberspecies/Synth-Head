/*****************************************************************
 * File:      GpuHardwareTestRunner.hpp
 * Category:  GPU Driver / Hardware Test Framework
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Comprehensive hardware test execution framework for continuous
 *    validation on real GPU devices. Supports automated execution,
 *    long-duration stress testing, and detection of timing-dependent,
 *    precision-sensitive, and concurrency-related issues.
 * 
 * Features:
 *    - End-to-end test execution on real hardware
 *    - Continuous validation loops
 *    - Performance counter capture
 *    - Diagnostic logging with reproduction details
 *    - Regression tracking and comparison
 *    - Thermal and timing condition monitoring
 *****************************************************************/

#ifndef GPU_HARDWARE_TEST_RUNNER_HPP_
#define GPU_HARDWARE_TEST_RUNNER_HPP_

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <math.h>
#include "GpuISA.hpp"
#include "GpuTestFramework.hpp"

namespace gpu {
namespace hardware_test {

using namespace gpu::isa;
using namespace gpu::test;

// ============================================================
// Hardware Test Constants
// ============================================================

constexpr int MAX_TEST_ITERATIONS = 1000000;
constexpr int MAX_FAILURE_LOG = 1024;
constexpr int MAX_PERF_SAMPLES = 4096;
constexpr int MAX_THERMAL_SAMPLES = 256;
constexpr int WATCHDOG_TIMEOUT_MS = 5000;

// ============================================================
// Hardware Configuration
// ============================================================

struct HardwareConfig {
  // Device identification
  char device_id[32];
  char firmware_version[16];
  char driver_version[16];
  uint32_t hardware_revision;
  
  // Clock configuration
  uint32_t cpu_freq_mhz;
  uint32_t gpu_core_freq_mhz;
  uint32_t memory_freq_mhz;
  uint32_t uart_baud_rate;
  
  // Memory configuration
  uint32_t total_ram_kb;
  uint32_t free_ram_kb;
  uint32_t framebuffer_size;
  uint32_t script_memory_size;
  
  // Feature flags
  bool has_hardware_sdf;
  bool has_hardware_aa;
  bool has_dma_transfer;
  bool has_double_buffer;
  
  HardwareConfig() {
    memset(this, 0, sizeof(HardwareConfig));
    strcpy(device_id, "ESP32-S3-GPU");
    strcpy(firmware_version, "1.0.0");
    strcpy(driver_version, "2.0.0");
    cpu_freq_mhz = 240;
    gpu_core_freq_mhz = 240;
    uart_baud_rate = 2000000;
    total_ram_kb = 512;
    has_double_buffer = true;
  }
};

// ============================================================
// Test Execution Context
// ============================================================

enum class TestCondition : uint8_t {
  NORMAL          = 0x00,
  HIGH_LOAD       = 0x01,
  LOW_MEMORY      = 0x02,
  HIGH_TEMP       = 0x03,
  CLOCK_STRESS    = 0x04,
  UART_STRESS     = 0x05,
  CONCURRENT_OPS  = 0x06,
  POWER_CYCLE     = 0x07,
};

enum class FailureCategory : uint8_t {
  NONE            = 0x00,
  TIMING          = 0x01,  // Timing-dependent failure
  PRECISION       = 0x02,  // Precision/accuracy failure
  RACE_CONDITION  = 0x03,  // Concurrency issue
  MEMORY_CORRUPT  = 0x04,  // Memory coherency violation
  SYNC_ERROR      = 0x05,  // Synchronization failure
  WATCHDOG        = 0x06,  // Timeout/hang
  VISUAL_MISMATCH = 0x07,  // Rendering differs from reference
  PERF_REGRESSION = 0x08,  // Performance degradation
  THERMAL         = 0x09,  // Thermal-related failure
  UNKNOWN         = 0xFF,
};

// ============================================================
// Performance Metrics
// ============================================================

struct PerformanceMetrics {
  // Timing (microseconds)
  uint32_t min_frame_time_us;
  uint32_t max_frame_time_us;
  uint32_t avg_frame_time_us;
  uint32_t frame_time_std_dev;
  
  // Throughput
  uint32_t frames_per_second;
  uint32_t commands_per_second;
  uint32_t pixels_per_second;
  uint32_t bytes_transferred;
  
  // Latency
  uint32_t command_latency_us;
  uint32_t render_latency_us;
  uint32_t display_latency_us;
  
  // Resource usage
  uint32_t peak_memory_kb;
  uint32_t avg_cpu_percent;
  uint32_t gpu_utilization_percent;
  
  // Counters
  uint64_t total_frames;
  uint64_t total_commands;
  uint64_t dropped_frames;
  uint64_t buffer_overruns;
  
  PerformanceMetrics() {
    memset(this, 0, sizeof(PerformanceMetrics));
    min_frame_time_us = UINT32_MAX;
  }
  
  void updateFrameTime(uint32_t frame_time_us) {
    if (frame_time_us < min_frame_time_us) min_frame_time_us = frame_time_us;
    if (frame_time_us > max_frame_time_us) max_frame_time_us = frame_time_us;
    
    // Running average
    total_frames++;
    avg_frame_time_us = (uint32_t)(
      ((uint64_t)avg_frame_time_us * (total_frames - 1) + frame_time_us) / total_frames
    );
    
    // FPS calculation
    if (avg_frame_time_us > 0) {
      frames_per_second = 1000000 / avg_frame_time_us;
    }
  }
  
  void reset() {
    *this = PerformanceMetrics();
  }
};

// ============================================================
// Thermal Monitoring
// ============================================================

struct ThermalState {
  float cpu_temp_c;
  float gpu_temp_c;
  float ambient_temp_c;
  uint32_t timestamp_ms;
  bool throttling_active;
  
  ThermalState() : cpu_temp_c(25.0f), gpu_temp_c(25.0f), 
                   ambient_temp_c(25.0f), timestamp_ms(0),
                   throttling_active(false) {}
};

class ThermalMonitor {
public:
  ThermalMonitor() : sample_count_(0), throttle_count_(0) {}
  
  void recordSample(const ThermalState& state) {
    if (sample_count_ < MAX_THERMAL_SAMPLES) {
      samples_[sample_count_++] = state;
    }
    
    if (state.throttling_active) {
      throttle_count_++;
    }
    
    // Update stats
    if (state.cpu_temp_c > peak_cpu_temp_) peak_cpu_temp_ = state.cpu_temp_c;
    if (state.gpu_temp_c > peak_gpu_temp_) peak_gpu_temp_ = state.gpu_temp_c;
    
    avg_cpu_temp_ = (avg_cpu_temp_ * (sample_count_ - 1) + state.cpu_temp_c) / sample_count_;
    avg_gpu_temp_ = (avg_gpu_temp_ * (sample_count_ - 1) + state.gpu_temp_c) / sample_count_;
  }
  
  float getPeakCpuTemp() const { return peak_cpu_temp_; }
  float getPeakGpuTemp() const { return peak_gpu_temp_; }
  float getAvgCpuTemp() const { return avg_cpu_temp_; }
  float getAvgGpuTemp() const { return avg_gpu_temp_; }
  int getThrottleCount() const { return throttle_count_; }
  int getSampleCount() const { return sample_count_; }
  
  bool isOverheating(float threshold_c = 80.0f) const {
    return peak_cpu_temp_ > threshold_c || peak_gpu_temp_ > threshold_c;
  }
  
  void reset() {
    sample_count_ = 0;
    throttle_count_ = 0;
    peak_cpu_temp_ = 0;
    peak_gpu_temp_ = 0;
    avg_cpu_temp_ = 0;
    avg_gpu_temp_ = 0;
  }

private:
  ThermalState samples_[MAX_THERMAL_SAMPLES];
  int sample_count_;
  int throttle_count_;
  float peak_cpu_temp_ = 0;
  float peak_gpu_temp_ = 0;
  float avg_cpu_temp_ = 0;
  float avg_gpu_temp_ = 0;
};

// ============================================================
// Failure Record
// ============================================================

struct FailureRecord {
  // Identification
  uint32_t failure_id;
  uint32_t test_id;
  char test_name[64];
  uint32_t iteration;
  
  // Classification
  FailureCategory category;
  TestCondition condition;
  uint32_t error_code;
  
  // Timing
  uint32_t timestamp_ms;
  uint32_t elapsed_ms;
  uint32_t frame_number;
  
  // Context
  float cpu_temp_c;
  float gpu_temp_c;
  uint32_t free_memory_kb;
  uint32_t frame_time_us;
  
  // Expected vs Actual
  float expected_value;
  float actual_value;
  float tolerance;
  float deviation;
  
  // Reproduction info
  uint32_t random_seed;
  uint8_t bytecode_hash[16];
  uint32_t register_state[16];
  
  // Description
  char message[128];
  
  FailureRecord() {
    memset(this, 0, sizeof(FailureRecord));
    category = FailureCategory::NONE;
    condition = TestCondition::NORMAL;
  }
  
  void setMessage(const char* msg) {
    strncpy(message, msg, sizeof(message) - 1);
  }
  
  bool isReproducible() const {
    // Failures with known seeds are reproducible
    return random_seed != 0;
  }
  
  bool isTimingRelated() const {
    return category == FailureCategory::TIMING ||
           category == FailureCategory::RACE_CONDITION ||
           category == FailureCategory::SYNC_ERROR;
  }
  
  bool isThermalRelated() const {
    return category == FailureCategory::THERMAL ||
           cpu_temp_c > 75.0f || gpu_temp_c > 75.0f;
  }
};

// ============================================================
// Failure Log
// ============================================================

class FailureLog {
public:
  FailureLog() : failure_count_(0), next_id_(1) {}
  
  uint32_t logFailure(const FailureRecord& record) {
    if (failure_count_ >= MAX_FAILURE_LOG) {
      // Circular buffer - overwrite oldest
      int oldest = failure_count_ % MAX_FAILURE_LOG;
      failures_[oldest] = record;
      failures_[oldest].failure_id = next_id_++;
      failure_count_++;
      return failures_[oldest].failure_id;
    }
    
    failures_[failure_count_] = record;
    failures_[failure_count_].failure_id = next_id_++;
    return failures_[failure_count_++].failure_id;
  }
  
  const FailureRecord* getFailure(int index) const {
    if (index < 0 || index >= getStoredCount()) return nullptr;
    return &failures_[index];
  }
  
  const FailureRecord* findById(uint32_t id) const {
    for (int i = 0; i < getStoredCount(); i++) {
      if (failures_[i].failure_id == id) {
        return &failures_[i];
      }
    }
    return nullptr;
  }
  
  int getStoredCount() const {
    return (failure_count_ < MAX_FAILURE_LOG) ? failure_count_ : MAX_FAILURE_LOG;
  }
  
  int getTotalCount() const { return failure_count_; }
  
  // Category statistics
  int countByCategory(FailureCategory cat) const {
    int count = 0;
    for (int i = 0; i < getStoredCount(); i++) {
      if (failures_[i].category == cat) count++;
    }
    return count;
  }
  
  // Find patterns (failures that occur multiple times)
  bool hasRepeatingPattern(const char* test_name, int threshold = 3) const {
    int count = 0;
    for (int i = 0; i < getStoredCount(); i++) {
      if (strcmp(failures_[i].test_name, test_name) == 0) {
        count++;
        if (count >= threshold) return true;
      }
    }
    return false;
  }
  
  void clear() {
    failure_count_ = 0;
    // Keep next_id to ensure unique IDs across clears
  }

private:
  FailureRecord failures_[MAX_FAILURE_LOG];
  int failure_count_;
  uint32_t next_id_;
};

// ============================================================
// Reference Comparison
// ============================================================

struct ReferenceFrame {
  uint8_t* pixels;     // RGB888 reference image
  int width;
  int height;
  uint32_t checksum;
  float tolerance;     // Per-pixel tolerance (0-1)
  
  ReferenceFrame() : pixels(nullptr), width(0), height(0), 
                     checksum(0), tolerance(0.01f) {}
  
  uint32_t computeChecksum() const {
    if (!pixels) return 0;
    
    uint32_t sum = 0;
    int size = width * height * 3;
    for (int i = 0; i < size; i++) {
      sum = sum * 31 + pixels[i];
    }
    return sum;
  }
  
  // Compare against captured frame
  bool compare(const uint8_t* actual, int& diff_pixels, float& max_diff) const {
    if (!pixels || !actual) return false;
    
    diff_pixels = 0;
    max_diff = 0.0f;
    
    int size = width * height;
    for (int i = 0; i < size; i++) {
      float dr = fabsf((float)actual[i*3+0] - (float)pixels[i*3+0]) / 255.0f;
      float dg = fabsf((float)actual[i*3+1] - (float)pixels[i*3+1]) / 255.0f;
      float db = fabsf((float)actual[i*3+2] - (float)pixels[i*3+2]) / 255.0f;
      
      float diff = (dr + dg + db) / 3.0f;
      
      if (diff > max_diff) max_diff = diff;
      if (diff > tolerance) diff_pixels++;
    }
    
    return diff_pixels == 0;
  }
};

// ============================================================
// Hardware Test Case
// ============================================================

enum class HWTestType : uint8_t {
  UNIT_TEST       = 0x00,  // Single operation test
  INTEGRATION     = 0x01,  // Multi-component test
  STRESS          = 0x02,  // Long-duration stress
  PERFORMANCE     = 0x03,  // Performance benchmark
  VISUAL          = 0x04,  // Visual output comparison
  CONCURRENCY     = 0x05,  // Multi-threaded/async test
  REGRESSION      = 0x06,  // Regression verification
};

struct HardwareTestCase {
  // Identification
  uint32_t test_id;
  char name[64];
  char category[32];
  HWTestType type;
  
  // Execution parameters
  int min_iterations;
  int max_iterations;
  uint32_t timeout_ms;
  bool stop_on_first_failure;
  
  // Test conditions
  TestCondition condition;
  bool require_thermal_stable;
  float max_temp_c;
  
  // Reference data
  ReferenceFrame* reference_frame;
  float expected_fps;
  float fps_tolerance;
  
  // Test function pointer
  typedef bool (*TestFunc)(HardwareTestCase*, void* context);
  TestFunc test_func;
  void* user_data;
  
  HardwareTestCase() {
    memset(this, 0, sizeof(HardwareTestCase));
    min_iterations = 1;
    max_iterations = 1;
    timeout_ms = WATCHDOG_TIMEOUT_MS;
    stop_on_first_failure = false;
    max_temp_c = 80.0f;
  }
};

// ============================================================
// Test Session
// ============================================================

struct TestSession {
  // Session info
  uint32_t session_id;
  uint32_t start_time_ms;
  uint32_t end_time_ms;
  
  // Configuration
  HardwareConfig hw_config;
  TestCondition conditions[8];
  int condition_count;
  
  // Results
  int total_tests;
  int passed_tests;
  int failed_tests;
  int skipped_tests;
  
  // Metrics
  PerformanceMetrics overall_perf;
  ThermalMonitor thermal;
  
  // Failures
  FailureLog failures;
  
  TestSession() : session_id(0), start_time_ms(0), end_time_ms(0),
                  condition_count(0), total_tests(0), passed_tests(0),
                  failed_tests(0), skipped_tests(0) {}
  
  float getPassRate() const {
    if (total_tests == 0) return 0.0f;
    return (float)passed_tests / (float)total_tests * 100.0f;
  }
  
  uint32_t getDurationMs() const {
    return end_time_ms - start_time_ms;
  }
  
  bool isComplete() const {
    return end_time_ms > start_time_ms;
  }
  
  bool isPassing() const {
    return failed_tests == 0 && total_tests > 0;
  }
};

// ============================================================
// Hardware Test Runner
// ============================================================

class HardwareTestRunner {
public:
  // Callbacks for hardware interaction
  typedef uint32_t (*GetTimeFunc)();
  typedef void (*DelayFunc)(uint32_t ms);
  typedef bool (*SendCommandFunc)(const uint8_t* data, size_t len);
  typedef bool (*ReceiveDataFunc)(uint8_t* buffer, size_t len, uint32_t timeout_ms);
  typedef ThermalState (*GetThermalFunc)();
  typedef uint32_t (*GetFreeMemoryFunc)();
  typedef void (*LogFunc)(const char* message);
  
  struct Callbacks {
    GetTimeFunc get_time;
    DelayFunc delay;
    SendCommandFunc send_command;
    ReceiveDataFunc receive_data;
    GetThermalFunc get_thermal;
    GetFreeMemoryFunc get_free_memory;
    LogFunc log;
    
    Callbacks() : get_time(nullptr), delay(nullptr), send_command(nullptr),
                  receive_data(nullptr), get_thermal(nullptr),
                  get_free_memory(nullptr), log(nullptr) {}
  };
  
  HardwareTestRunner() : test_count_(0), running_(false), 
                          current_test_(nullptr), session_count_(0) {}
  
  void setCallbacks(const Callbacks& cb) { callbacks_ = cb; }
  void setHardwareConfig(const HardwareConfig& config) { hw_config_ = config; }
  
  // Test registration
  int registerTest(const HardwareTestCase& test) {
    if (test_count_ >= 256) return -1;
    tests_[test_count_] = test;
    tests_[test_count_].test_id = test_count_;
    return test_count_++;
  }
  
  // Run all registered tests
  TestSession runAll(int iterations = 1) {
    TestSession session;
    session.session_id = ++session_count_;
    session.hw_config = hw_config_;
    session.start_time_ms = getTime();
    
    running_ = true;
    
    for (int iter = 0; iter < iterations && running_; iter++) {
      log("=== Iteration %d/%d ===", iter + 1, iterations);
      
      for (int i = 0; i < test_count_ && running_; i++) {
        runSingleTest(tests_[i], session, iter);
      }
    }
    
    session.end_time_ms = getTime();
    running_ = false;
    
    return session;
  }
  
  // Run tests by category
  TestSession runCategory(const char* category, int iterations = 1) {
    TestSession session;
    session.session_id = ++session_count_;
    session.hw_config = hw_config_;
    session.start_time_ms = getTime();
    
    running_ = true;
    
    for (int iter = 0; iter < iterations && running_; iter++) {
      for (int i = 0; i < test_count_ && running_; i++) {
        if (strcmp(tests_[i].category, category) == 0) {
          runSingleTest(tests_[i], session, iter);
        }
      }
    }
    
    session.end_time_ms = getTime();
    running_ = false;
    
    return session;
  }
  
  // Run stress test (continuous until stopped or failure limit)
  TestSession runStressTest(int test_id, int max_iterations = MAX_TEST_ITERATIONS,
                            int max_failures = 100) {
    TestSession session;
    session.session_id = ++session_count_;
    session.hw_config = hw_config_;
    session.start_time_ms = getTime();
    
    if (test_id < 0 || test_id >= test_count_) {
      session.end_time_ms = getTime();
      return session;
    }
    
    running_ = true;
    HardwareTestCase& test = tests_[test_id];
    
    int iteration = 0;
    int failure_streak = 0;
    
    while (running_ && iteration < max_iterations) {
      // Check thermal
      if (test.require_thermal_stable) {
        ThermalState thermal = getThermal();
        session.thermal.recordSample(thermal);
        
        if (thermal.cpu_temp_c > test.max_temp_c || 
            thermal.gpu_temp_c > test.max_temp_c) {
          log("Thermal limit reached, cooling down...");
          delay(5000);  // Cool down period
          continue;
        }
      }
      
      bool passed = executeSingleIteration(test, session, iteration);
      session.total_tests++;
      
      if (passed) {
        session.passed_tests++;
        failure_streak = 0;
      } else {
        session.failed_tests++;
        failure_streak++;
        
        if (failure_streak >= max_failures) {
          log("Max consecutive failures reached, stopping");
          break;
        }
      }
      
      iteration++;
      
      // Progress report every 1000 iterations
      if (iteration % 1000 == 0) {
        log("Progress: %d iterations, %d failures (%.2f%% pass rate)",
            iteration, session.failed_tests, session.getPassRate());
      }
    }
    
    session.end_time_ms = getTime();
    running_ = false;
    
    return session;
  }
  
  // Run continuous validation (runs indefinitely until externally stopped)
  void runContinuousValidation(int report_interval_ms = 60000) {
    running_ = true;
    uint32_t last_report = getTime();
    int total_sessions = 0;
    int total_failures = 0;
    
    log("Starting continuous validation...");
    
    while (running_) {
      TestSession session = runAll(1);
      total_sessions++;
      total_failures += session.failed_tests;
      
      uint32_t now = getTime();
      if (now - last_report >= (uint32_t)report_interval_ms) {
        log("=== Continuous Validation Report ===");
        log("Sessions: %d, Total Failures: %d", total_sessions, total_failures);
        log("Last session: %d/%d passed (%.2f%%)",
            session.passed_tests, session.total_tests, session.getPassRate());
        log("Peak temps: CPU=%.1fC, GPU=%.1fC",
            session.thermal.getPeakCpuTemp(), session.thermal.getPeakGpuTemp());
        last_report = now;
      }
      
      // Brief pause between sessions
      delay(100);
    }
    
    log("Continuous validation stopped after %d sessions", total_sessions);
  }
  
  // Stop execution
  void stop() { running_ = false; }
  bool isRunning() const { return running_; }
  
  // Get test info
  int getTestCount() const { return test_count_; }
  const HardwareTestCase* getTest(int index) const {
    if (index < 0 || index >= test_count_) return nullptr;
    return &tests_[index];
  }

private:
  void runSingleTest(HardwareTestCase& test, TestSession& session, int iteration) {
    current_test_ = &test;
    
    log("[%s] Starting (iter %d)...", test.name, iteration);
    
    // Check preconditions
    if (test.require_thermal_stable) {
      ThermalState thermal = getThermal();
      if (thermal.cpu_temp_c > test.max_temp_c) {
        log("[%s] SKIPPED: CPU temp %.1fC exceeds limit %.1fC",
            test.name, thermal.cpu_temp_c, test.max_temp_c);
        session.skipped_tests++;
        session.total_tests++;
        return;
      }
    }
    
    bool passed = executeSingleIteration(test, session, iteration);
    session.total_tests++;
    
    if (passed) {
      session.passed_tests++;
      log("[%s] PASSED", test.name);
    } else {
      session.failed_tests++;
      log("[%s] FAILED", test.name);
    }
    
    current_test_ = nullptr;
  }
  
  bool executeSingleIteration(HardwareTestCase& test, TestSession& session, int iteration) {
    uint32_t start_time = getTime();
    bool passed = false;
    
    // Execute test function
    if (test.test_func) {
      passed = test.test_func(&test, test.user_data);
    }
    
    uint32_t elapsed = getTime() - start_time;
    
    // Check timeout
    if (elapsed > test.timeout_ms) {
      FailureRecord failure;
      failure.test_id = test.test_id;
      strncpy(failure.test_name, test.name, sizeof(failure.test_name) - 1);
      failure.iteration = iteration;
      failure.category = FailureCategory::WATCHDOG;
      failure.condition = test.condition;
      failure.timestamp_ms = start_time;
      failure.elapsed_ms = elapsed;
      failure.setMessage("Test exceeded timeout");
      
      session.failures.logFailure(failure);
      return false;
    }
    
    // Record performance
    session.overall_perf.updateFrameTime(elapsed * 1000);  // Convert to us
    
    // Log failure details if failed
    if (!passed) {
      FailureRecord failure;
      failure.test_id = test.test_id;
      strncpy(failure.test_name, test.name, sizeof(failure.test_name) - 1);
      failure.iteration = iteration;
      failure.category = FailureCategory::UNKNOWN;
      failure.condition = test.condition;
      failure.timestamp_ms = start_time;
      failure.elapsed_ms = elapsed;
      
      // Capture thermal state
      ThermalState thermal = getThermal();
      failure.cpu_temp_c = thermal.cpu_temp_c;
      failure.gpu_temp_c = thermal.gpu_temp_c;
      failure.free_memory_kb = getFreeMemory();
      
      session.failures.logFailure(failure);
    }
    
    return passed;
  }
  
  // Callback wrappers with null checks
  uint32_t getTime() {
    return callbacks_.get_time ? callbacks_.get_time() : 0;
  }
  
  void delay(uint32_t ms) {
    if (callbacks_.delay) callbacks_.delay(ms);
  }
  
  ThermalState getThermal() {
    return callbacks_.get_thermal ? callbacks_.get_thermal() : ThermalState();
  }
  
  uint32_t getFreeMemory() {
    return callbacks_.get_free_memory ? callbacks_.get_free_memory() : 0;
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
  
  // State
  Callbacks callbacks_;
  HardwareConfig hw_config_;
  
  HardwareTestCase tests_[256];
  int test_count_;
  
  bool running_;
  HardwareTestCase* current_test_;
  int session_count_;
};

// ============================================================
// Test Result Analyzer
// ============================================================

class TestResultAnalyzer {
public:
  struct AnalysisReport {
    // Summary
    int total_sessions;
    int total_tests;
    int total_failures;
    float overall_pass_rate;
    
    // Failure breakdown
    int timing_failures;
    int precision_failures;
    int race_condition_failures;
    int memory_failures;
    int thermal_failures;
    int other_failures;
    
    // Patterns
    bool has_intermittent_failures;
    bool has_thermal_correlation;
    bool has_timing_correlation;
    
    // Performance
    float avg_frame_time_ms;
    float frame_time_variance;
    
    // Recommendations
    char recommendations[512];
    
    AnalysisReport() {
      memset(this, 0, sizeof(AnalysisReport));
    }
  };
  
  static AnalysisReport analyze(const TestSession& session) {
    AnalysisReport report;
    
    report.total_sessions = 1;
    report.total_tests = session.total_tests;
    report.total_failures = session.failed_tests;
    report.overall_pass_rate = session.getPassRate();
    
    // Count failures by category
    const FailureLog& log = session.failures;
    report.timing_failures = log.countByCategory(FailureCategory::TIMING) +
                             log.countByCategory(FailureCategory::RACE_CONDITION) +
                             log.countByCategory(FailureCategory::SYNC_ERROR);
    report.precision_failures = log.countByCategory(FailureCategory::PRECISION);
    report.race_condition_failures = log.countByCategory(FailureCategory::RACE_CONDITION);
    report.memory_failures = log.countByCategory(FailureCategory::MEMORY_CORRUPT);
    report.thermal_failures = log.countByCategory(FailureCategory::THERMAL);
    report.other_failures = session.failed_tests - 
                            report.timing_failures - report.precision_failures -
                            report.memory_failures - report.thermal_failures;
    
    // Check for patterns
    report.has_thermal_correlation = session.thermal.isOverheating(70.0f) &&
                                     report.thermal_failures > 0;
    
    // Calculate timing variance
    float avg = (float)session.overall_perf.avg_frame_time_us / 1000.0f;
    float min_t = (float)session.overall_perf.min_frame_time_us / 1000.0f;
    float max_t = (float)session.overall_perf.max_frame_time_us / 1000.0f;
    report.avg_frame_time_ms = avg;
    report.frame_time_variance = max_t - min_t;
    
    report.has_timing_correlation = report.frame_time_variance > avg * 0.5f &&
                                    report.timing_failures > 0;
    
    // Generate recommendations
    generateRecommendations(report);
    
    return report;
  }
  
private:
  static void generateRecommendations(AnalysisReport& report) {
    char* rec = report.recommendations;
    size_t remaining = sizeof(report.recommendations);
    
    if (report.thermal_failures > 0 || report.has_thermal_correlation) {
      int written = snprintf(rec, remaining, 
        "- Add thermal throttling or improve cooling\n");
      rec += written;
      remaining -= written;
    }
    
    if (report.timing_failures > 0 || report.has_timing_correlation) {
      int written = snprintf(rec, remaining,
        "- Investigate timing-sensitive code paths\n"
        "- Consider adding synchronization barriers\n");
      rec += written;
      remaining -= written;
    }
    
    if (report.race_condition_failures > 0) {
      int written = snprintf(rec, remaining,
        "- Review concurrent access patterns\n"
        "- Add memory barriers where needed\n");
      rec += written;
      remaining -= written;
    }
    
    if (report.precision_failures > 0) {
      int written = snprintf(rec, remaining,
        "- Consider increasing numerical precision\n"
        "- Review fixed-point overflow handling\n");
      rec += written;
      remaining -= written;
    }
    
    if (report.memory_failures > 0) {
      int written = snprintf(rec, remaining,
        "- Check for buffer overflows\n"
        "- Verify DMA coherency\n");
      rec += written;
      remaining -= written;
    }
    
    if (report.overall_pass_rate >= 100.0f) {
      snprintf(rec, remaining, "All tests passing - system ready for deployment\n");
    }
  }
};

} // namespace hardware_test
} // namespace gpu

#endif // GPU_HARDWARE_TEST_RUNNER_HPP_
