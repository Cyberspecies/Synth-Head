/*****************************************************************
 * File:      GpuTestCoordinator.hpp
 * Category:  GPU Driver / Test Coordination
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    CPU-side test coordinator that runs on ESP32-S3 (Arduino).
 *    Orchestrates test execution on the GPU via UART protocol.
 * 
 * Features:
 *    - UART communication with GPU test harness
 *    - Test scheduling and sequencing
 *    - Result collection and aggregation
 *    - Continuous validation orchestration
 *    - Serial console reporting
 *****************************************************************/

#ifndef GPU_TEST_COORDINATOR_HPP_
#define GPU_TEST_COORDINATOR_HPP_

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "GpuISA.hpp"
#include "GpuHardwareHarness.hpp"
#include "GpuRegressionTracker.hpp"
#include "GpuContinuousValidation.hpp"

// Arduino includes would go here in actual implementation
// #include <HardwareSerial.h>

namespace gpu {
namespace coordinator {

using namespace gpu::harness::protocol;
using namespace gpu::regression;
using namespace gpu::validation;

// ============================================================
// UART Communication
// ============================================================

class UARTComm {
public:
  UARTComm() : timeout_ms_(1000) {}
  
  void begin(uint32_t baud) {
    // In actual implementation: Serial1.begin(baud, SERIAL_8N1, RX_PIN, TX_PIN);
    (void)baud;
  }
  
  void setTimeout(uint32_t ms) { timeout_ms_ = ms; }
  
  bool sendPacket(const TestPacket& packet) {
    // Calculate total size
    size_t total = 4 + packet.length + 1;  // magic + cmd + len + payload + crc
    
    // In actual implementation:
    // return Serial1.write((uint8_t*)&packet, total) == total;
    (void)packet;
    (void)total;
    return true;
  }
  
  bool receivePacket(TestPacket& packet) {
    // In actual implementation, would read with timeout
    // Wait for magic bytes
    // Read header
    // Read payload
    // Validate CRC
    (void)packet;
    return true;
  }
  
  bool sendAndReceive(const TestPacket& request, TestPacket& response) {
    if (!sendPacket(request)) return false;
    return receivePacket(response);
  }
  
  void flush() {
    // Serial1.flush();
  }

private:
  uint32_t timeout_ms_;
};

// ============================================================
// Test Results Storage
// ============================================================

struct TestResultEntry {
  uint32_t test_id;
  char test_name[48];
  uint32_t seed;
  bool passed;
  uint32_t duration_ms;
  uint32_t memory_used;
  float temperature;
  uint32_t frame_checksum;
  char message[128];
  uint32_t timestamp;
  
  TestResultEntry() : test_id(0), seed(0), passed(false), duration_ms(0),
                      memory_used(0), temperature(0), frame_checksum(0),
                      timestamp(0) {
    test_name[0] = '\0';
    message[0] = '\0';
  }
};

class TestResultStorage {
public:
  static constexpr int MAX_RESULTS = 256;
  
  TestResultStorage() : result_count_(0) {}
  
  void addResult(const TestResultEntry& result) {
    if (result_count_ >= MAX_RESULTS) {
      // Shift array (lose oldest)
      memmove(&results_[0], &results_[1], sizeof(TestResultEntry) * (MAX_RESULTS - 1));
      result_count_ = MAX_RESULTS - 1;
    }
    results_[result_count_++] = result;
  }
  
  int getResultCount() const { return result_count_; }
  
  const TestResultEntry* getResult(int index) const {
    if (index < 0 || index >= result_count_) return nullptr;
    return &results_[index];
  }
  
  // Get pass rate for a specific test
  float getPassRate(const char* test_name) const {
    int passes = 0, total = 0;
    for (int i = 0; i < result_count_; i++) {
      if (strcmp(results_[i].test_name, test_name) == 0) {
        total++;
        if (results_[i].passed) passes++;
      }
    }
    return total > 0 ? (float)passes / (float)total * 100.0f : 0;
  }
  
  // Get overall statistics
  struct Stats {
    int total;
    int passed;
    int failed;
    float pass_rate;
    uint32_t avg_duration_ms;
    uint32_t max_duration_ms;
  };
  
  Stats getStats() const {
    Stats s = {0, 0, 0, 0, 0, 0};
    uint32_t total_duration = 0;
    
    for (int i = 0; i < result_count_; i++) {
      s.total++;
      if (results_[i].passed) s.passed++;
      else s.failed++;
      
      total_duration += results_[i].duration_ms;
      if (results_[i].duration_ms > s.max_duration_ms) {
        s.max_duration_ms = results_[i].duration_ms;
      }
    }
    
    if (s.total > 0) {
      s.pass_rate = (float)s.passed / (float)s.total * 100.0f;
      s.avg_duration_ms = total_duration / s.total;
    }
    
    return s;
  }
  
  void clear() { result_count_ = 0; }

private:
  TestResultEntry results_[MAX_RESULTS];
  int result_count_;
};

// ============================================================
// Test List
// ============================================================

struct TestDefinition {
  uint32_t test_id;
  char name[48];
  uint32_t timeout_ms;
  int priority;
  bool enabled;
  
  TestDefinition() : test_id(0), timeout_ms(5000), priority(0), enabled(true) {
    name[0] = '\0';
  }
};

class TestList {
public:
  static constexpr int MAX_TESTS = 128;
  
  TestList() : test_count_(0) {}
  
  void addTest(uint32_t id, const char* name, uint32_t timeout_ms, int priority = 0) {
    if (test_count_ >= MAX_TESTS) return;
    
    TestDefinition& t = tests_[test_count_++];
    t.test_id = id;
    strncpy(t.name, name, sizeof(t.name) - 1);
    t.timeout_ms = timeout_ms;
    t.priority = priority;
    t.enabled = true;
  }
  
  void enableTest(const char* name) { setTestEnabled(name, true); }
  void disableTest(const char* name) { setTestEnabled(name, false); }
  
  void setTestEnabled(const char* name, bool enabled) {
    for (int i = 0; i < test_count_; i++) {
      if (strcmp(tests_[i].name, name) == 0) {
        tests_[i].enabled = enabled;
        return;
      }
    }
  }
  
  int getTestCount() const { return test_count_; }
  int getEnabledCount() const {
    int count = 0;
    for (int i = 0; i < test_count_; i++) {
      if (tests_[i].enabled) count++;
    }
    return count;
  }
  
  const TestDefinition* getTest(int index) const {
    if (index < 0 || index >= test_count_) return nullptr;
    return &tests_[index];
  }
  
  const TestDefinition* findByName(const char* name) const {
    for (int i = 0; i < test_count_; i++) {
      if (strcmp(tests_[i].name, name) == 0) {
        return &tests_[i];
      }
    }
    return nullptr;
  }
  
  void sortByPriority() {
    // Simple bubble sort (test count is small)
    for (int i = 0; i < test_count_ - 1; i++) {
      for (int j = 0; j < test_count_ - i - 1; j++) {
        if (tests_[j].priority < tests_[j + 1].priority) {
          TestDefinition temp = tests_[j];
          tests_[j] = tests_[j + 1];
          tests_[j + 1] = temp;
        }
      }
    }
  }

private:
  TestDefinition tests_[MAX_TESTS];
  int test_count_;
};

// ============================================================
// Logging Interface
// ============================================================

enum class LogTarget {
  SERIAL,
  FILE,
  BOTH,
};

class Logger {
public:
  Logger() : target_(LogTarget::SERIAL), verbose_(false) {}
  
  void setTarget(LogTarget target) { target_ = target; }
  void setVerbose(bool verbose) { verbose_ = verbose; }
  
  void info(const char* fmt, ...) {
    if (target_ == LogTarget::FILE) return;
    
    va_list args;
    va_start(args, fmt);
    vprint("[INFO] ", fmt, args);
    va_end(args);
  }
  
  void debug(const char* fmt, ...) {
    if (!verbose_) return;
    if (target_ == LogTarget::FILE) return;
    
    va_list args;
    va_start(args, fmt);
    vprint("[DEBUG] ", fmt, args);
    va_end(args);
  }
  
  void warn(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprint("[WARN] ", fmt, args);
    va_end(args);
  }
  
  void error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprint("[ERROR] ", fmt, args);
    va_end(args);
  }
  
  void result(const char* test_name, bool passed, uint32_t duration_ms, 
              const char* message = nullptr) {
    if (passed) {
      info("%s: PASS (%dms)", test_name, duration_ms);
    } else {
      error("%s: FAIL (%dms) - %s", test_name, duration_ms, 
            message ? message : "Unknown");
    }
  }

private:
  LogTarget target_;
  bool verbose_;
  
  void vprint(const char* prefix, const char* fmt, va_list args) {
    // In actual implementation: Serial.print(prefix); Serial.vprintf(fmt, args);
    (void)prefix;
    (void)fmt;
    (void)args;
  }
};

// ============================================================
// Test Coordinator
// ============================================================

class TestCoordinator {
public:
  TestCoordinator() 
    : running_(false), current_seed_(0x12345678),
      total_iterations_(0), max_iterations_(0) {}
  
  // --------------------------------------------------------
  // Setup
  // --------------------------------------------------------
  
  bool initialize(uint32_t baud = 921600) {
    uart_.begin(baud);
    uart_.setTimeout(2000);
    
    // Ping GPU to verify connection
    if (!pingGpu()) {
      logger_.error("Failed to connect to GPU");
      return false;
    }
    
    logger_.info("GPU test harness connected");
    
    // Fetch test list from GPU
    if (!fetchTestList()) {
      logger_.warn("Could not fetch test list from GPU");
    }
    
    return true;
  }
  
  // --------------------------------------------------------
  // Test Execution
  // --------------------------------------------------------
  
  bool runSingleTest(const char* name, uint32_t seed = 0) {
    const TestDefinition* test = tests_.findByName(name);
    if (!test) {
      logger_.error("Test not found: %s", name);
      return false;
    }
    
    if (seed == 0) seed = generateSeed();
    
    return executeTest(*test, seed);
  }
  
  bool runAllTests(int iterations = 1) {
    bool all_passed = true;
    
    for (int iter = 0; iter < iterations; iter++) {
      logger_.info("=== Iteration %d/%d ===", iter + 1, iterations);
      
      for (int i = 0; i < tests_.getTestCount(); i++) {
        const TestDefinition* test = tests_.getTest(i);
        if (!test || !test->enabled) continue;
        
        uint32_t seed = generateSeed();
        bool passed = executeTest(*test, seed);
        
        if (!passed) all_passed = false;
        
        if (!running_) break;
      }
      
      if (!running_) break;
      total_iterations_++;
    }
    
    printSummary();
    return all_passed;
  }
  
  // Run until all tests pass N times consistently
  bool runUntilStable(int required_passes = 5, int max_iters = 100) {
    running_ = true;
    max_iterations_ = max_iters;
    
    // Track consecutive passes per test
    int pass_counts[TestList::MAX_TESTS] = {0};
    
    for (int iter = 0; iter < max_iters && running_; iter++) {
      logger_.info("=== Stability iteration %d ===", iter + 1);
      
      bool all_stable = true;
      
      for (int i = 0; i < tests_.getTestCount(); i++) {
        const TestDefinition* test = tests_.getTest(i);
        if (!test || !test->enabled) continue;
        
        if (pass_counts[i] >= required_passes) continue;
        
        uint32_t seed = generateSeed();
        bool passed = executeTest(*test, seed);
        
        if (passed) {
          pass_counts[i]++;
        } else {
          pass_counts[i] = 0;  // Reset on failure
          all_stable = false;
        }
        
        if (pass_counts[i] < required_passes) {
          all_stable = false;
        }
      }
      
      total_iterations_++;
      
      if (all_stable) {
        logger_.info("All tests stable after %d iterations", iter + 1);
        running_ = false;
        return true;
      }
    }
    
    running_ = false;
    return false;
  }
  
  // Continuous validation mode
  void runContinuous() {
    running_ = true;
    
    logger_.info("=== Continuous Validation Started ===");
    
    while (running_) {
      runAllTests(1);
      
      // Print periodic status
      if (total_iterations_ % 10 == 0) {
        printStatus();
      }
    }
    
    logger_.info("=== Continuous Validation Stopped ===");
    printSummary();
  }
  
  void stop() { running_ = false; }
  bool isRunning() const { return running_; }
  
  // --------------------------------------------------------
  // Test List Management
  // --------------------------------------------------------
  
  TestList& tests() { return tests_; }
  const TestList& tests() const { return tests_; }
  
  // --------------------------------------------------------
  // Results
  // --------------------------------------------------------
  
  TestResultStorage& results() { return results_; }
  const TestResultStorage& results() const { return results_; }
  
  // --------------------------------------------------------
  // Configuration
  // --------------------------------------------------------
  
  void setBaseSeed(uint32_t seed) { current_seed_ = seed; }
  
  Logger& logger() { return logger_; }
  
  // --------------------------------------------------------
  // GPU Control
  // --------------------------------------------------------
  
  bool setGpuClock(uint32_t freq_mhz) {
    TestPacket request, response;
    request.command = CMD_SET_CLOCK;
    memcpy(request.payload, &freq_mhz, 4);
    request.length = 4;
    request.calculateCRC();
    
    if (!uart_.sendAndReceive(request, response)) {
      return false;
    }
    
    return response.payload[0] == RSP_OK;
  }
  
  float getGpuTemperature() {
    TestPacket request, response;
    request.command = CMD_GET_TEMP;
    request.length = 0;
    request.calculateCRC();
    
    if (!uart_.sendAndReceive(request, response)) {
      return -1.0f;
    }
    
    if (response.payload[0] != RSP_OK) return -1.0f;
    
    float temp;
    memcpy(&temp, &response.payload[1], 4);
    return temp;
  }
  
  bool resetGpu() {
    TestPacket request, response;
    request.command = CMD_RESET;
    request.length = 0;
    request.calculateCRC();
    
    return uart_.sendAndReceive(request, response) && 
           response.payload[0] == RSP_OK;
  }

private:
  UARTComm uart_;
  TestList tests_;
  TestResultStorage results_;
  Logger logger_;
  
  bool running_;
  uint32_t current_seed_;
  int total_iterations_;
  int max_iterations_;
  
  // --------------------------------------------------------
  // Private Helpers
  // --------------------------------------------------------
  
  bool pingGpu() {
    TestPacket request, response;
    request.command = CMD_PING;
    request.length = 0;
    request.calculateCRC();
    
    return uart_.sendAndReceive(request, response) && 
           response.payload[0] == RSP_OK;
  }
  
  bool fetchTestList() {
    // In actual implementation, would query GPU for registered tests
    // For now, return true and let caller add tests manually
    return true;
  }
  
  uint32_t generateSeed() {
    // Simple LCG
    current_seed_ = current_seed_ * 1103515245 + 12345;
    return current_seed_;
  }
  
  bool executeTest(const TestDefinition& test, uint32_t seed) {
    logger_.debug("Running test '%s' with seed 0x%08X", test.name, seed);
    
    // Build request
    TestPacket request, response;
    request.command = CMD_RUN_TEST;
    
    RunTestRequest test_req;
    test_req.test_id = test.test_id;
    test_req.seed = seed;
    test_req.timeout_ms = test.timeout_ms;
    test_req.flags = RunTestRequest::FLAG_CAPTURE_METRICS;
    
    memcpy(request.payload, &test_req, sizeof(test_req));
    request.length = sizeof(test_req);
    request.calculateCRC();
    
    // Execute
    if (!uart_.sendAndReceive(request, response)) {
      logger_.error("Communication failed for test '%s'", test.name);
      recordResult(test, seed, false, 0, 0, 0, 0, "UART communication failed");
      return false;
    }
    
    // Parse response
    RunTestResponse test_rsp;
    memcpy(&test_rsp, response.payload, sizeof(test_rsp));
    
    bool passed = (test_rsp.result == RSP_OK);
    
    // Log result
    logger_.result(test.name, passed, test_rsp.duration_ms, test_rsp.message);
    
    // Store result
    recordResult(test, seed, passed, test_rsp.duration_ms, 
                 test_rsp.memory_used, test_rsp.temperature,
                 test_rsp.frame_checksum, test_rsp.message);
    
    return passed;
  }
  
  void recordResult(const TestDefinition& test, uint32_t seed, bool passed,
                    uint32_t duration_ms, uint32_t memory_used,
                    float temperature, uint32_t frame_checksum,
                    const char* message) {
    TestResultEntry entry;
    entry.test_id = test.test_id;
    strncpy(entry.test_name, test.name, sizeof(entry.test_name) - 1);
    entry.seed = seed;
    entry.passed = passed;
    entry.duration_ms = duration_ms;
    entry.memory_used = memory_used;
    entry.temperature = temperature;
    entry.frame_checksum = frame_checksum;
    strncpy(entry.message, message, sizeof(entry.message) - 1);
    entry.timestamp = 0;  // Would be set from millis()
    
    results_.addResult(entry);
  }
  
  void printStatus() {
    auto stats = results_.getStats();
    float temp = getGpuTemperature();
    
    logger_.info("--- Status ---");
    logger_.info("Iterations: %d", total_iterations_);
    logger_.info("Pass rate: %.1f%% (%d/%d)", stats.pass_rate, stats.passed, stats.total);
    logger_.info("GPU temp: %.1fC", temp);
  }
  
  void printSummary() {
    auto stats = results_.getStats();
    
    logger_.info("=== Test Summary ===");
    logger_.info("Total: %d, Passed: %d, Failed: %d", 
                 stats.total, stats.passed, stats.failed);
    logger_.info("Pass rate: %.1f%%", stats.pass_rate);
    logger_.info("Avg duration: %dms, Max: %dms", 
                 stats.avg_duration_ms, stats.max_duration_ms);
    
    // List failed tests
    if (stats.failed > 0) {
      logger_.info("--- Failed Tests ---");
      for (int i = 0; i < tests_.getTestCount(); i++) {
        const TestDefinition* test = tests_.getTest(i);
        if (!test) continue;
        
        float pass_rate = results_.getPassRate(test->name);
        if (pass_rate < 100.0f) {
          logger_.error("  %s: %.1f%% pass rate", test->name, pass_rate);
        }
      }
    }
  }
};

// ============================================================
// Preset Test Suites
// ============================================================

namespace suites {

inline void addCoreTests(TestList& tests) {
  tests.addTest(1, "ISA_NOP", 1000, 100);
  tests.addTest(2, "ISA_SET_PIXEL", 1000, 100);
  tests.addTest(3, "ISA_FILL_RECT", 1000, 100);
  tests.addTest(4, "ISA_DRAW_LINE", 1000, 100);
  tests.addTest(5, "ISA_DRAW_CIRCLE", 1000, 100);
  tests.addTest(6, "ISA_DRAW_TRIANGLE", 1000, 100);
}

inline void addRenderingTests(TestList& tests) {
  tests.addTest(10, "RENDER_CLEAR", 2000, 90);
  tests.addTest(11, "RENDER_FLIP", 2000, 90);
  tests.addTest(12, "RENDER_BLEND", 2000, 90);
  tests.addTest(13, "RENDER_GRADIENT", 2000, 90);
  tests.addTest(14, "RENDER_SPRITE", 2000, 90);
  tests.addTest(15, "RENDER_TEXT", 2000, 90);
}

inline void addAnimationTests(TestList& tests) {
  tests.addTest(20, "ANIM_LINEAR", 3000, 80);
  tests.addTest(21, "ANIM_EASE_IN_OUT", 3000, 80);
  tests.addTest(22, "ANIM_BEZIER", 3000, 80);
  tests.addTest(23, "ANIM_LOOP", 3000, 80);
  tests.addTest(24, "ANIM_CHAIN", 3000, 80);
}

inline void addSDFTests(TestList& tests) {
  tests.addTest(30, "SDF_CIRCLE", 3000, 70);
  tests.addTest(31, "SDF_BOX", 3000, 70);
  tests.addTest(32, "SDF_UNION", 3000, 70);
  tests.addTest(33, "SDF_INTERSECT", 3000, 70);
  tests.addTest(34, "SDF_SUBTRACT", 3000, 70);
  tests.addTest(35, "SDF_BLEND", 3000, 70);
}

inline void addStressTests(TestList& tests) {
  tests.addTest(100, "STRESS_MEMORY", 30000, 50);
  tests.addTest(101, "STRESS_COMMANDS", 30000, 50);
  tests.addTest(102, "STRESS_PRECISION", 30000, 50);
  tests.addTest(103, "STRESS_THERMAL", 60000, 50);
}

inline void addAllTests(TestList& tests) {
  addCoreTests(tests);
  addRenderingTests(tests);
  addAnimationTests(tests);
  addSDFTests(tests);
  addStressTests(tests);
}

} // namespace suites

} // namespace coordinator
} // namespace gpu

#endif // GPU_TEST_COORDINATOR_HPP_
