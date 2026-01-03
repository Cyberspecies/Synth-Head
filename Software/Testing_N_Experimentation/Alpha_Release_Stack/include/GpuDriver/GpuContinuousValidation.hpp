/*****************************************************************
 * File:      GpuContinuousValidation.hpp
 * Category:  GPU Driver / Continuous Validation
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Continuous validation loop that runs tests across multiple
 *    configurations, versions, and thermal conditions until all
 *    tests pass reliably.
 * 
 * Features:
 *    - Multi-configuration test matrix
 *    - Firmware/driver version iteration
 *    - Thermal condition cycling
 *    - Long-duration soak testing
 *    - Automatic retry with different seeds
 *    - Flaky test detection
 *    - Convergence tracking
 *****************************************************************/

#ifndef GPU_CONTINUOUS_VALIDATION_HPP_
#define GPU_CONTINUOUS_VALIDATION_HPP_

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "GpuISA.hpp"
#include "GpuHardwareTestRunner.hpp"
#include "GpuStressTest.hpp"
#include "GpuDiagnostics.hpp"
#include "GpuRegressionTracker.hpp"

namespace gpu {
namespace validation {

using namespace gpu::testing;
using namespace gpu::stress;
using namespace gpu::diagnostics;
using namespace gpu::regression;

// ============================================================
// Constants
// ============================================================

constexpr int MAX_CONFIGS = 16;
constexpr int MAX_TESTS_PER_RUN = 256;
constexpr int MAX_FLAKY_TESTS = 64;
constexpr int MIN_CONSISTENT_PASSES = 5;

// ============================================================
// Configuration Matrix
// ============================================================

struct ClockConfig {
  uint32_t cpu_freq_mhz;
  uint32_t memory_freq_mhz;
  uint32_t uart_baud;
  
  ClockConfig() : cpu_freq_mhz(240), memory_freq_mhz(80), uart_baud(921600) {}
  ClockConfig(uint32_t cpu, uint32_t mem, uint32_t baud) 
    : cpu_freq_mhz(cpu), memory_freq_mhz(mem), uart_baud(baud) {}
};

struct ThermalConfig {
  float target_temp_c;
  float tolerance_c;
  bool throttle_enabled;
  
  ThermalConfig() : target_temp_c(25.0f), tolerance_c(5.0f), throttle_enabled(true) {}
  ThermalConfig(float target, float tol, bool throttle)
    : target_temp_c(target), tolerance_c(tol), throttle_enabled(throttle) {}
};

struct ValidationConfig {
  char name[32];
  
  VersionInfo driver_version;
  VersionInfo firmware_version;
  ClockConfig clock;
  ThermalConfig thermal;
  
  // Test parameters
  uint32_t random_seed;
  int retry_count;
  
  ValidationConfig() : random_seed(0), retry_count(3) {
    name[0] = '\0';
  }
};

// ============================================================
// Test Result Aggregation
// ============================================================

struct TestRunResult {
  char test_name[48];
  int pass_count;
  int fail_count;
  int flaky_count;
  
  // First failure info
  uint32_t first_fail_seed;
  char first_fail_reason[128];
  
  // Timing
  uint32_t avg_duration_ms;
  uint32_t max_duration_ms;
  uint32_t min_duration_ms;
  
  TestRunResult() : pass_count(0), fail_count(0), flaky_count(0),
                    first_fail_seed(0), avg_duration_ms(0),
                    max_duration_ms(0), min_duration_ms(UINT32_MAX) {
    test_name[0] = '\0';
    first_fail_reason[0] = '\0';
  }
  
  float passRate() const {
    int total = pass_count + fail_count;
    return total > 0 ? (float)pass_count / (float)total * 100.0f : 0;
  }
  
  bool isFlaky() const {
    return pass_count > 0 && fail_count > 0;
  }
  
  bool isReliablyPassing() const {
    return pass_count >= MIN_CONSISTENT_PASSES && fail_count == 0;
  }
};

// ============================================================
// Validation Session
// ============================================================

struct ValidationSession {
  uint32_t session_id;
  uint32_t start_timestamp;
  uint32_t end_timestamp;
  
  ValidationConfig config;
  
  TestRunResult test_results[MAX_TESTS_PER_RUN];
  int test_count;
  
  // Summary
  int total_runs;
  int total_passes;
  int total_failures;
  int flaky_tests;
  int reliable_tests;
  
  // Thermal
  float peak_temp_c;
  float avg_temp_c;
  bool thermal_throttled;
  
  ValidationSession() : session_id(0), start_timestamp(0), end_timestamp(0),
                        test_count(0), total_runs(0), total_passes(0),
                        total_failures(0), flaky_tests(0), reliable_tests(0),
                        peak_temp_c(0), avg_temp_c(0), thermal_throttled(false) {}
  
  float overallPassRate() const {
    return total_runs > 0 ? (float)total_passes / (float)total_runs * 100.0f : 0;
  }
  
  bool allTestsReliable() const {
    return reliable_tests == test_count && test_count > 0;
  }
};

// ============================================================
// Flaky Test Tracker
// ============================================================

struct FlakyTest {
  char test_name[48];
  int pass_count;
  int fail_count;
  
  // Seeds that caused failures
  uint32_t fail_seeds[16];
  int fail_seed_count;
  
  // Failure reasons
  char common_reason[128];
  
  FlakyTest() : pass_count(0), fail_count(0), fail_seed_count(0) {
    test_name[0] = '\0';
    common_reason[0] = '\0';
  }
  
  void recordFailure(uint32_t seed, const char* reason) {
    fail_count++;
    
    if (fail_seed_count < 16) {
      fail_seeds[fail_seed_count++] = seed;
    }
    
    if (common_reason[0] == '\0') {
      strncpy(common_reason, reason, sizeof(common_reason) - 1);
    }
  }
  
  void recordPass() { pass_count++; }
  
  float flakinessScore() const {
    int total = pass_count + fail_count;
    if (total < 5) return 0;  // Not enough data
    
    // Higher score = more flaky
    float rate = (float)fail_count / (float)total;
    return rate * (1.0f - rate) * 4.0f;  // Peaks at 50% failure rate
  }
};

// ============================================================
// Hardware Callbacks Interface
// ============================================================

struct HardwareCallbacks {
  // Device control
  bool (*setClockConfig)(const ClockConfig& config);
  bool (*waitForTemperature)(float target_c, float tolerance_c, uint32_t timeout_ms);
  float (*readTemperature)();
  
  // Test execution
  bool (*executeTest)(const char* test_name, uint32_t seed, char* result, size_t result_size);
  bool (*captureFrame)(uint8_t* pixels, int width, int height);
  
  // Communication
  bool (*resetDevice)();
  bool (*flashFirmware)(const uint8_t* data, size_t size);
  
  // Timing
  uint32_t (*getTimestamp)();
  void (*delayMs)(uint32_t ms);
  
  void* user_data;
};

// ============================================================
// Continuous Validation Engine
// ============================================================

class ContinuousValidator {
public:
  ContinuousValidator() 
    : running_(false), next_session_id_(1), iteration_(0) {
    memset(&callbacks_, 0, sizeof(callbacks_));
  }
  
  // --------------------------------------------------------
  // Setup
  // --------------------------------------------------------
  
  void setCallbacks(const HardwareCallbacks& callbacks) {
    callbacks_ = callbacks;
  }
  
  void addConfig(const ValidationConfig& config) {
    if (config_count_ < MAX_CONFIGS) {
      configs_[config_count_++] = config;
    }
  }
  
  void clearConfigs() {
    config_count_ = 0;
  }
  
  void setTestList(const char** test_names, int count) {
    test_names_ = test_names;
    test_count_ = count;
  }
  
  // --------------------------------------------------------
  // Execution Control
  // --------------------------------------------------------
  
  void start() {
    running_ = true;
    iteration_ = 0;
    clearFlakyTests();
  }
  
  void stop() {
    running_ = false;
  }
  
  bool isRunning() const { return running_; }
  
  // Run one iteration across all configs
  ValidationSession runIteration() {
    ValidationSession session;
    session.session_id = next_session_id_++;
    session.start_timestamp = callbacks_.getTimestamp ? callbacks_.getTimestamp() : 0;
    
    // Run each test across all configurations
    for (int t = 0; t < test_count_ && running_; t++) {
      const char* test_name = test_names_[t];
      
      TestRunResult& result = session.test_results[session.test_count++];
      strncpy(result.test_name, test_name, sizeof(result.test_name) - 1);
      
      // Run test with different seeds
      for (int retry = 0; retry < current_config().retry_count && running_; retry++) {
        uint32_t seed = generateSeed(iteration_, t, retry);
        
        uint32_t start_time = callbacks_.getTimestamp ? callbacks_.getTimestamp() : 0;
        char test_result[256] = "";
        
        bool passed = executeTestWithRetry(test_name, seed, test_result, sizeof(test_result));
        
        uint32_t duration = callbacks_.getTimestamp ? 
                           callbacks_.getTimestamp() - start_time : 0;
        
        if (passed) {
          result.pass_count++;
          updateFlakyTest(test_name, true, seed, "");
        } else {
          result.fail_count++;
          if (result.fail_count == 1) {
            result.first_fail_seed = seed;
            strncpy(result.first_fail_reason, test_result, sizeof(result.first_fail_reason) - 1);
          }
          updateFlakyTest(test_name, false, seed, test_result);
        }
        
        // Track timing
        result.avg_duration_ms = (result.avg_duration_ms * (retry) + duration) / (retry + 1);
        if (duration > result.max_duration_ms) result.max_duration_ms = duration;
        if (duration < result.min_duration_ms) result.min_duration_ms = duration;
        
        session.total_runs++;
        if (passed) session.total_passes++;
        else session.total_failures++;
      }
      
      // Update flaky/reliable counts
      if (result.isFlaky()) session.flaky_tests++;
      if (result.isReliablyPassing()) session.reliable_tests++;
      
      // Check thermal
      if (callbacks_.readTemperature) {
        float temp = callbacks_.readTemperature();
        if (temp > session.peak_temp_c) session.peak_temp_c = temp;
        session.avg_temp_c = (session.avg_temp_c * t + temp) / (t + 1);
        
        // Check for throttling
        if (temp > current_config().thermal.target_temp_c + current_config().thermal.tolerance_c) {
          session.thermal_throttled = true;
        }
      }
    }
    
    session.config = current_config();
    session.end_timestamp = callbacks_.getTimestamp ? callbacks_.getTimestamp() : 0;
    iteration_++;
    
    return session;
  }
  
  // Run until all tests pass reliably
  bool runUntilStable(int max_iterations = 100) {
    start();
    
    for (int i = 0; i < max_iterations && running_; i++) {
      ValidationSession session = runIteration();
      
      // Check if all tests are reliable
      if (session.allTestsReliable()) {
        stop();
        return true;
      }
      
      // Cycle to next configuration if needed
      advanceConfig();
    }
    
    stop();
    return false;
  }
  
  // Run for specified duration
  void runForDuration(uint32_t duration_ms) {
    start();
    
    uint32_t start = callbacks_.getTimestamp ? callbacks_.getTimestamp() : 0;
    while (running_) {
      runIteration();
      
      uint32_t elapsed = callbacks_.getTimestamp ? 
                        callbacks_.getTimestamp() - start : 0;
      if (elapsed >= duration_ms) break;
      
      advanceConfig();
    }
    
    stop();
  }
  
  // --------------------------------------------------------
  // Flaky Test Analysis
  // --------------------------------------------------------
  
  int getFlakyTestCount() const { return flaky_test_count_; }
  
  const FlakyTest* getFlakyTest(int index) const {
    if (index < 0 || index >= flaky_test_count_) return nullptr;
    return &flaky_tests_[index];
  }
  
  const FlakyTest* getMostFlakyTest() const {
    const FlakyTest* most_flaky = nullptr;
    float max_score = 0;
    
    for (int i = 0; i < flaky_test_count_; i++) {
      float score = flaky_tests_[i].flakinessScore();
      if (score > max_score) {
        max_score = score;
        most_flaky = &flaky_tests_[i];
      }
    }
    
    return most_flaky;
  }
  
  // --------------------------------------------------------
  // Convergence Tracking
  // --------------------------------------------------------
  
  struct ConvergenceStatus {
    int stable_tests;
    int unstable_tests;
    int untested_tests;
    int iterations_completed;
    float estimated_iterations_remaining;
    bool converged;
    
    ConvergenceStatus() : stable_tests(0), unstable_tests(0), untested_tests(0),
                          iterations_completed(0), estimated_iterations_remaining(0),
                          converged(false) {}
  };
  
  ConvergenceStatus getConvergenceStatus() const {
    ConvergenceStatus status;
    status.iterations_completed = iteration_;
    
    for (int i = 0; i < flaky_test_count_; i++) {
      const FlakyTest& ft = flaky_tests_[i];
      int total = ft.pass_count + ft.fail_count;
      
      if (total < MIN_CONSISTENT_PASSES) {
        status.untested_tests++;
      } else if (ft.fail_count == 0 && ft.pass_count >= MIN_CONSISTENT_PASSES) {
        status.stable_tests++;
      } else {
        status.unstable_tests++;
      }
    }
    
    status.converged = (status.unstable_tests == 0 && status.untested_tests == 0);
    
    // Estimate remaining iterations
    if (status.unstable_tests > 0 && iteration_ > 0) {
      // Simple heuristic: assume each unstable test needs ~10 more iterations
      status.estimated_iterations_remaining = (float)status.unstable_tests * 10.0f;
    }
    
    return status;
  }
  
  // --------------------------------------------------------
  // Reporting
  // --------------------------------------------------------
  
  void generateReport(char* buffer, size_t buffer_size) const {
    int offset = 0;
    
    auto status = getConvergenceStatus();
    
    offset += snprintf(buffer + offset, buffer_size - offset,
      "=== Continuous Validation Report ===\n\n"
      "Iterations: %d\n"
      "Convergence: %s\n\n"
      "Tests:\n"
      "  Stable: %d\n"
      "  Unstable: %d\n"
      "  Untested: %d\n\n",
      iteration_,
      status.converged ? "CONVERGED" : "IN PROGRESS",
      status.stable_tests, status.unstable_tests, status.untested_tests);
    
    // List flaky tests
    if (flaky_test_count_ > 0) {
      offset += snprintf(buffer + offset, buffer_size - offset,
        "--- Flaky Tests ---\n");
      
      for (int i = 0; i < flaky_test_count_ && offset < (int)buffer_size - 200; i++) {
        const FlakyTest& ft = flaky_tests_[i];
        if (ft.flakinessScore() > 0) {
          offset += snprintf(buffer + offset, buffer_size - offset,
            "  %s: %.0f%% flaky (pass=%d, fail=%d)\n"
            "    Reason: %s\n",
            ft.test_name, ft.flakinessScore() * 100,
            ft.pass_count, ft.fail_count,
            ft.common_reason);
        }
      }
    }
    
    // List configs
    offset += snprintf(buffer + offset, buffer_size - offset,
      "\n--- Configurations (%d) ---\n", config_count_);
    
    for (int i = 0; i < config_count_ && offset < (int)buffer_size - 100; i++) {
      offset += snprintf(buffer + offset, buffer_size - offset,
        "  %s: CPU %dMHz, Temp %.0fC\n",
        configs_[i].name,
        configs_[i].clock.cpu_freq_mhz,
        configs_[i].thermal.target_temp_c);
    }
    
    buffer[offset] = '\0';
  }

private:
  bool running_;
  uint32_t next_session_id_;
  int iteration_;
  
  ValidationConfig configs_[MAX_CONFIGS];
  int config_count_ = 0;
  int current_config_index_ = 0;
  
  const char** test_names_ = nullptr;
  int test_count_ = 0;
  
  FlakyTest flaky_tests_[MAX_FLAKY_TESTS];
  int flaky_test_count_ = 0;
  
  HardwareCallbacks callbacks_;
  
  // --------------------------------------------------------
  // Private Helpers
  // --------------------------------------------------------
  
  const ValidationConfig& current_config() const {
    if (config_count_ == 0) {
      static ValidationConfig default_config;
      return default_config;
    }
    return configs_[current_config_index_ % config_count_];
  }
  
  void advanceConfig() {
    if (config_count_ > 0) {
      current_config_index_ = (current_config_index_ + 1) % config_count_;
      
      // Apply new configuration
      if (callbacks_.setClockConfig) {
        callbacks_.setClockConfig(current_config().clock);
      }
      
      if (callbacks_.waitForTemperature && 
          current_config().thermal.target_temp_c > 0) {
        callbacks_.waitForTemperature(
          current_config().thermal.target_temp_c,
          current_config().thermal.tolerance_c,
          30000  // 30s timeout
        );
      }
    }
  }
  
  uint32_t generateSeed(int iteration, int test_index, int retry) {
    // Reproducible seed generation
    uint32_t seed = (uint32_t)iteration * 1000000 + 
                    (uint32_t)test_index * 1000 + 
                    (uint32_t)retry;
    return seed ^ (seed >> 16) ^ 0xDEADBEEF;
  }
  
  bool executeTestWithRetry(const char* test_name, uint32_t seed,
                            char* result, size_t result_size) {
    if (!callbacks_.executeTest) {
      strcpy(result, "No test executor configured");
      return false;
    }
    
    return callbacks_.executeTest(test_name, seed, result, result_size);
  }
  
  void updateFlakyTest(const char* test_name, bool passed, 
                       uint32_t seed, const char* reason) {
    // Find or create flaky test entry
    FlakyTest* ft = nullptr;
    for (int i = 0; i < flaky_test_count_; i++) {
      if (strcmp(flaky_tests_[i].test_name, test_name) == 0) {
        ft = &flaky_tests_[i];
        break;
      }
    }
    
    if (!ft && flaky_test_count_ < MAX_FLAKY_TESTS) {
      ft = &flaky_tests_[flaky_test_count_++];
      strncpy(ft->test_name, test_name, sizeof(ft->test_name) - 1);
    }
    
    if (ft) {
      if (passed) {
        ft->recordPass();
      } else {
        ft->recordFailure(seed, reason);
      }
    }
  }
  
  void clearFlakyTests() {
    flaky_test_count_ = 0;
    memset(flaky_tests_, 0, sizeof(flaky_tests_));
  }
};

// ============================================================
// Preset Configurations
// ============================================================

namespace presets {

inline ValidationConfig normalConfig() {
  ValidationConfig c;
  strcpy(c.name, "Normal");
  c.clock = ClockConfig(240, 80, 921600);
  c.thermal = ThermalConfig(25.0f, 5.0f, true);
  c.retry_count = 3;
  return c;
}

inline ValidationConfig highSpeedConfig() {
  ValidationConfig c;
  strcpy(c.name, "HighSpeed");
  c.clock = ClockConfig(240, 80, 2000000);
  c.thermal = ThermalConfig(25.0f, 5.0f, true);
  c.retry_count = 3;
  return c;
}

inline ValidationConfig lowSpeedConfig() {
  ValidationConfig c;
  strcpy(c.name, "LowSpeed");
  c.clock = ClockConfig(160, 40, 115200);
  c.thermal = ThermalConfig(25.0f, 5.0f, true);
  c.retry_count = 3;
  return c;
}

inline ValidationConfig hotConfig() {
  ValidationConfig c;
  strcpy(c.name, "Hot");
  c.clock = ClockConfig(240, 80, 921600);
  c.thermal = ThermalConfig(50.0f, 5.0f, true);
  c.retry_count = 3;
  return c;
}

inline ValidationConfig coldConfig() {
  ValidationConfig c;
  strcpy(c.name, "Cold");
  c.clock = ClockConfig(240, 80, 921600);
  c.thermal = ThermalConfig(10.0f, 5.0f, true);
  c.retry_count = 3;
  return c;
}

inline ValidationConfig stressConfig() {
  ValidationConfig c;
  strcpy(c.name, "Stress");
  c.clock = ClockConfig(240, 80, 921600);
  c.thermal = ThermalConfig(45.0f, 5.0f, false);  // Throttling disabled
  c.retry_count = 10;
  return c;
}

} // namespace presets

} // namespace validation
} // namespace gpu

#endif // GPU_CONTINUOUS_VALIDATION_HPP_
