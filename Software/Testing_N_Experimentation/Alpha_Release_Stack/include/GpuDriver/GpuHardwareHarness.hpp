/*****************************************************************
 * File:      GpuHardwareHarness.hpp
 * Category:  GPU Driver / Hardware Harness
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    GPU-side test harness that runs on ESP32-S3 (ESP-IDF).
 *    Implements hardware-specific callbacks for the test framework.
 * 
 * Features:
 *    - Direct hardware access for ESP32-S3
 *    - Temperature sensor integration
 *    - UART test command protocol
 *    - Display capture and comparison
 *    - Memory inspection
 *    - Timing measurement with hardware timers
 *****************************************************************/

#ifndef GPU_HARDWARE_HARNESS_HPP_
#define GPU_HARDWARE_HARNESS_HPP_

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "GpuISA.hpp"
#include "GpuHardwareTestRunner.hpp"
#include "GpuDiagnostics.hpp"
#include "GpuContinuousValidation.hpp"

// ESP-IDF includes would go here in actual implementation
// #include "driver/uart.h"
// #include "driver/temp_sensor.h"
// #include "esp_timer.h"
// #include "esp_heap_caps.h"

namespace gpu {
namespace harness {

using namespace gpu::testing;
using namespace gpu::diagnostics;
using namespace gpu::validation;

// ============================================================
// UART Test Protocol
// ============================================================

namespace protocol {

// Command IDs
constexpr uint8_t CMD_PING           = 0x01;
constexpr uint8_t CMD_RUN_TEST       = 0x10;
constexpr uint8_t CMD_RUN_STRESS     = 0x11;
constexpr uint8_t CMD_CAPTURE_FRAME  = 0x20;
constexpr uint8_t CMD_GET_METRICS    = 0x30;
constexpr uint8_t CMD_GET_STATE      = 0x31;
constexpr uint8_t CMD_GET_TEMP       = 0x32;
constexpr uint8_t CMD_SET_CLOCK      = 0x40;
constexpr uint8_t CMD_RESET          = 0xF0;
constexpr uint8_t CMD_BOOTLOADER     = 0xFF;

// Response codes
constexpr uint8_t RSP_OK             = 0x00;
constexpr uint8_t RSP_ERROR          = 0x01;
constexpr uint8_t RSP_BUSY           = 0x02;
constexpr uint8_t RSP_TIMEOUT        = 0x03;
constexpr uint8_t RSP_CRC_ERROR      = 0x04;
constexpr uint8_t RSP_NOT_FOUND      = 0x05;

// Packet structure
struct TestPacket {
  uint8_t magic[2];    // 0xAA, 0x55
  uint8_t command;
  uint8_t length;
  uint8_t payload[252];
  uint8_t crc;
  
  TestPacket() {
    magic[0] = 0xAA;
    magic[1] = 0x55;
    command = 0;
    length = 0;
    crc = 0;
  }
  
  void calculateCRC() {
    uint8_t c = command ^ length;
    for (int i = 0; i < length; i++) {
      c ^= payload[i];
    }
    crc = c;
  }
  
  bool validateCRC() const {
    uint8_t c = command ^ length;
    for (int i = 0; i < length; i++) {
      c ^= payload[i];
    }
    return c == crc;
  }
};

// Run test request
struct RunTestRequest {
  uint32_t test_id;
  uint32_t seed;
  uint32_t timeout_ms;
  uint8_t flags;
  
  static constexpr uint8_t FLAG_CAPTURE_FRAME = 0x01;
  static constexpr uint8_t FLAG_CAPTURE_METRICS = 0x02;
  static constexpr uint8_t FLAG_VERBOSE = 0x04;
};

// Run test response
struct RunTestResponse {
  uint8_t result;      // RSP_OK or error
  uint32_t duration_ms;
  uint32_t frame_checksum;
  uint32_t memory_used;
  float temperature;
  char message[128];
};

} // namespace protocol

// ============================================================
// Hardware Abstraction
// ============================================================

class ESP32Hardware {
public:
  ESP32Hardware() : initialized_(false), temp_enabled_(false) {}
  
  bool initialize() {
    // In actual implementation, would initialize hardware
    // uart_driver_install(UART_NUM_1, 4096, 4096, 0, NULL, 0);
    // temp_sensor_install(...);
    initialized_ = true;
    return true;
  }
  
  void deinitialize() {
    initialized_ = false;
  }
  
  // --------------------------------------------------------
  // Timing
  // --------------------------------------------------------
  
  uint32_t getTimestampMs() const {
    // In actual implementation: esp_timer_get_time() / 1000
    return 0;
  }
  
  uint64_t getTimestampUs() const {
    // In actual implementation: esp_timer_get_time()
    return 0;
  }
  
  void delayMs(uint32_t ms) {
    // In actual implementation: vTaskDelay(pdMS_TO_TICKS(ms))
    (void)ms;
  }
  
  // --------------------------------------------------------
  // Temperature
  // --------------------------------------------------------
  
  bool enableTemperatureSensor() {
    // temp_sensor_enable()
    temp_enabled_ = true;
    return true;
  }
  
  float readTemperature() {
    if (!temp_enabled_) return 0.0f;
    // float temp;
    // temp_sensor_read_celsius(&temp);
    // return temp;
    return 25.0f;  // Placeholder
  }
  
  // --------------------------------------------------------
  // Memory
  // --------------------------------------------------------
  
  uint32_t getFreeHeap() const {
    // return heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
    return 320 * 1024;  // Placeholder
  }
  
  uint32_t getLargestFreeBlock() const {
    // return heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT);
    return 100 * 1024;  // Placeholder
  }
  
  uint32_t getMinFreeHeap() const {
    // return heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT);
    return 200 * 1024;  // Placeholder
  }
  
  // --------------------------------------------------------
  // Clock Control
  // --------------------------------------------------------
  
  bool setCpuFrequency(uint32_t freq_mhz) {
    // esp_pm_config_esp32s3_t config = { .max_freq_mhz = freq_mhz };
    // esp_pm_configure(&config);
    (void)freq_mhz;
    return true;
  }
  
  uint32_t getCpuFrequency() const {
    // return esp_clk_cpu_freq() / 1000000;
    return 240;
  }
  
  // --------------------------------------------------------
  // UART
  // --------------------------------------------------------
  
  int uartWrite(const uint8_t* data, size_t len) {
    // return uart_write_bytes(UART_NUM_1, data, len);
    (void)data;
    return (int)len;
  }
  
  int uartRead(uint8_t* data, size_t len, uint32_t timeout_ms) {
    // return uart_read_bytes(UART_NUM_1, data, len, pdMS_TO_TICKS(timeout_ms));
    (void)data;
    (void)timeout_ms;
    return (int)len;
  }
  
  void uartFlush() {
    // uart_flush(UART_NUM_1);
  }

private:
  bool initialized_;
  bool temp_enabled_;
};

// ============================================================
// Test Registry
// ============================================================

typedef bool (*TestFunction)(uint32_t seed, char* result, size_t result_size);

struct RegisteredTest {
  uint32_t test_id;
  char name[48];
  TestFunction function;
  uint32_t timeout_ms;
  
  RegisteredTest() : test_id(0), function(nullptr), timeout_ms(5000) {
    name[0] = '\0';
  }
};

class TestRegistry {
public:
  static constexpr int MAX_TESTS = 128;
  
  TestRegistry() : test_count_(0), next_id_(1) {}
  
  uint32_t registerTest(const char* name, TestFunction func, uint32_t timeout_ms = 5000) {
    if (test_count_ >= MAX_TESTS) return 0;
    
    RegisteredTest& t = tests_[test_count_++];
    t.test_id = next_id_++;
    strncpy(t.name, name, sizeof(t.name) - 1);
    t.function = func;
    t.timeout_ms = timeout_ms;
    
    return t.test_id;
  }
  
  const RegisteredTest* findByName(const char* name) const {
    for (int i = 0; i < test_count_; i++) {
      if (strcmp(tests_[i].name, name) == 0) {
        return &tests_[i];
      }
    }
    return nullptr;
  }
  
  const RegisteredTest* findById(uint32_t id) const {
    for (int i = 0; i < test_count_; i++) {
      if (tests_[i].test_id == id) {
        return &tests_[i];
      }
    }
    return nullptr;
  }
  
  int getTestCount() const { return test_count_; }
  
  const RegisteredTest* getTest(int index) const {
    if (index < 0 || index >= test_count_) return nullptr;
    return &tests_[index];
  }

private:
  RegisteredTest tests_[MAX_TESTS];
  int test_count_;
  uint32_t next_id_;
};

// ============================================================
// GPU Test Harness
// ============================================================

class GpuTestHarness {
public:
  GpuTestHarness() : running_(false), current_test_(nullptr) {}
  
  bool initialize() {
    if (!hardware_.initialize()) return false;
    hardware_.enableTemperatureSensor();
    diagnostics_.enable();
    return true;
  }
  
  void shutdown() {
    running_ = false;
    hardware_.deinitialize();
  }
  
  // --------------------------------------------------------
  // Test Registration
  // --------------------------------------------------------
  
  TestRegistry& registry() { return registry_; }
  
  // --------------------------------------------------------
  // Test Execution
  // --------------------------------------------------------
  
  bool runTest(const char* name, uint32_t seed, protocol::RunTestResponse& response) {
    const RegisteredTest* test = registry_.findByName(name);
    if (!test) {
      response.result = protocol::RSP_NOT_FOUND;
      strcpy(response.message, "Test not found");
      return false;
    }
    
    return runTestById(test->test_id, seed, response);
  }
  
  bool runTestById(uint32_t id, uint32_t seed, protocol::RunTestResponse& response) {
    const RegisteredTest* test = registry_.findById(id);
    if (!test) {
      response.result = protocol::RSP_NOT_FOUND;
      strcpy(response.message, "Test not found");
      return false;
    }
    
    current_test_ = test;
    
    // Capture pre-test state
    uint32_t start_time = hardware_.getTimestampMs();
    uint32_t start_heap = hardware_.getFreeHeap();
    float start_temp = hardware_.readTemperature();
    
    // Run test
    char result_msg[128] = "";
    bool passed = test->function(seed, result_msg, sizeof(result_msg));
    
    // Capture post-test state
    uint32_t end_time = hardware_.getTimestampMs();
    uint32_t end_heap = hardware_.getFreeHeap();
    float end_temp = hardware_.readTemperature();
    
    // Fill response
    response.result = passed ? protocol::RSP_OK : protocol::RSP_ERROR;
    response.duration_ms = end_time - start_time;
    response.memory_used = start_heap - end_heap;
    response.temperature = end_temp;
    response.frame_checksum = 0;  // Would be computed from framebuffer
    strncpy(response.message, result_msg, sizeof(response.message) - 1);
    
    // Log diagnostics
    if (!passed) {
      diagnostics_.log().error("TEST", "Test '%s' failed: %s", test->name, result_msg);
    }
    
    // Check for memory leaks
    if (response.memory_used > 1024) {
      diagnostics_.log().warning("TEST", "Possible leak: %d bytes not freed", 
                                  response.memory_used);
    }
    
    // Check for thermal issues
    if (end_temp - start_temp > 5.0f) {
      diagnostics_.log().warning("TEST", "Temperature rose %.1fC during test",
                                  end_temp - start_temp);
    }
    
    current_test_ = nullptr;
    return passed;
  }
  
  // --------------------------------------------------------
  // Protocol Handler
  // --------------------------------------------------------
  
  void processCommand(const protocol::TestPacket& request, 
                      protocol::TestPacket& response) {
    response.magic[0] = 0xAA;
    response.magic[1] = 0x55;
    response.command = request.command | 0x80;  // Response flag
    
    switch (request.command) {
      case protocol::CMD_PING:
        handlePing(request, response);
        break;
        
      case protocol::CMD_RUN_TEST:
        handleRunTest(request, response);
        break;
        
      case protocol::CMD_GET_METRICS:
        handleGetMetrics(request, response);
        break;
        
      case protocol::CMD_GET_TEMP:
        handleGetTemp(request, response);
        break;
        
      case protocol::CMD_SET_CLOCK:
        handleSetClock(request, response);
        break;
        
      case protocol::CMD_RESET:
        handleReset(request, response);
        break;
        
      default:
        response.payload[0] = protocol::RSP_ERROR;
        response.length = 1;
        break;
    }
    
    response.calculateCRC();
  }
  
  // --------------------------------------------------------
  // Main Loop
  // --------------------------------------------------------
  
  void run() {
    running_ = true;
    
    while (running_) {
      // Read command packet
      protocol::TestPacket request;
      int bytes = hardware_.uartRead((uint8_t*)&request, sizeof(request), 100);
      
      if (bytes > 0) {
        if (request.magic[0] == 0xAA && request.magic[1] == 0x55) {
          if (request.validateCRC()) {
            protocol::TestPacket response;
            processCommand(request, response);
            hardware_.uartWrite((uint8_t*)&response, 4 + response.length + 1);
          } else {
            diagnostics_.counters().increment(PerfCounter::CRC_ERRORS);
          }
        }
      }
      
      // Update diagnostics
      diagnostics_.counters().increment(PerfCounter::FRAMES_RENDERED);
    }
  }
  
  // --------------------------------------------------------
  // Accessors
  // --------------------------------------------------------
  
  ESP32Hardware& hardware() { return hardware_; }
  DiagnosticsSystem& diagnostics() { return diagnostics_; }

private:
  ESP32Hardware hardware_;
  TestRegistry registry_;
  DiagnosticsSystem diagnostics_;
  
  bool running_;
  const RegisteredTest* current_test_;
  
  // --------------------------------------------------------
  // Command Handlers
  // --------------------------------------------------------
  
  void handlePing(const protocol::TestPacket& req, protocol::TestPacket& rsp) {
    (void)req;
    rsp.payload[0] = protocol::RSP_OK;
    rsp.payload[1] = 0x01;  // Version
    rsp.payload[2] = (uint8_t)registry_.getTestCount();
    rsp.length = 3;
  }
  
  void handleRunTest(const protocol::TestPacket& req, protocol::TestPacket& rsp) {
    if (req.length < sizeof(protocol::RunTestRequest)) {
      rsp.payload[0] = protocol::RSP_ERROR;
      rsp.length = 1;
      return;
    }
    
    protocol::RunTestRequest test_req;
    memcpy(&test_req, req.payload, sizeof(test_req));
    
    protocol::RunTestResponse test_rsp;
    runTestById(test_req.test_id, test_req.seed, test_rsp);
    
    memcpy(rsp.payload, &test_rsp, sizeof(test_rsp));
    rsp.length = sizeof(test_rsp);
  }
  
  void handleGetMetrics(const protocol::TestPacket& req, protocol::TestPacket& rsp) {
    (void)req;
    
    // Pack performance counters
    PerfCounterValue values[16];
    int count = 0;
    diagnostics_.counters().snapshot(values, 16, count);
    
    rsp.payload[0] = protocol::RSP_OK;
    rsp.payload[1] = (uint8_t)count;
    
    int offset = 2;
    for (int i = 0; i < count && offset < 250; i++) {
      rsp.payload[offset++] = (uint8_t)values[i].counter;
      memcpy(&rsp.payload[offset], &values[i].value, 4);
      offset += 4;
    }
    
    rsp.length = offset;
  }
  
  void handleGetTemp(const protocol::TestPacket& req, protocol::TestPacket& rsp) {
    (void)req;
    
    float temp = hardware_.readTemperature();
    
    rsp.payload[0] = protocol::RSP_OK;
    memcpy(&rsp.payload[1], &temp, 4);
    rsp.length = 5;
  }
  
  void handleSetClock(const protocol::TestPacket& req, protocol::TestPacket& rsp) {
    if (req.length < 4) {
      rsp.payload[0] = protocol::RSP_ERROR;
      rsp.length = 1;
      return;
    }
    
    uint32_t freq_mhz;
    memcpy(&freq_mhz, req.payload, 4);
    
    bool success = hardware_.setCpuFrequency(freq_mhz);
    rsp.payload[0] = success ? protocol::RSP_OK : protocol::RSP_ERROR;
    rsp.length = 1;
  }
  
  void handleReset(const protocol::TestPacket& req, protocol::TestPacket& rsp) {
    (void)req;
    
    diagnostics_.reset();
    
    rsp.payload[0] = protocol::RSP_OK;
    rsp.length = 1;
    
    // In actual implementation: esp_restart()
  }
};

// ============================================================
// Test Registration Macros
// ============================================================

#define GPU_TEST(name) \
  static bool test_##name(uint32_t seed, char* result, size_t result_size)

#define GPU_REGISTER_TEST(harness, name, timeout) \
  (harness).registry().registerTest(#name, test_##name, timeout)

} // namespace harness
} // namespace gpu

#endif // GPU_HARDWARE_HARNESS_HPP_
