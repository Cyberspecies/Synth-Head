/*****************************************************************
 * File:      CPU_TestCoordinator.cpp
 * Category:  GPU Driver / CPU Test Coordinator Example
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    CPU-side example showing how to orchestrate GPU testing
 *    using the test coordinator (Arduino Framework).
 * 
 * Hardware:
 *    - ESP32-S3 CPU (COM15)
 *    - UART: TX=GPIO12, RX=GPIO11
 *****************************************************************/

// Uncomment for actual build:
// #include <Arduino.h>
// #include <HardwareSerial.h>

#include "GpuDriver/GpuTestCoordinator.hpp"
#include "GpuDriver/GpuRegressionTracker.hpp"
#include "GpuDriver/GpuContinuousValidation.hpp"

using namespace gpu::coordinator;
using namespace gpu::regression;
using namespace gpu::validation;

// ============================================================
// Configuration
// ============================================================

constexpr uint32_t GPU_UART_BAUD = 921600;
constexpr int GPU_UART_RX = 11;
constexpr int GPU_UART_TX = 12;

// ============================================================
// Global Objects
// ============================================================

TestCoordinator coordinator;
AdvancedRegressionTracker regressionTracker;

// ============================================================
// Serial Console Interface
// ============================================================

void printHelp() {
  Serial.println("=== GPU Test Coordinator ===");
  Serial.println("Commands:");
  Serial.println("  help       - Show this help");
  Serial.println("  list       - List available tests");
  Serial.println("  run <name> - Run a single test");
  Serial.println("  all        - Run all tests once");
  Serial.println("  all <n>    - Run all tests n times");
  Serial.println("  stable     - Run until stable");
  Serial.println("  continuous - Run continuously");
  Serial.println("  stop       - Stop testing");
  Serial.println("  status     - Show current status");
  Serial.println("  results    - Show test results");
  Serial.println("  temp       - Read GPU temperature");
  Serial.println("  reset      - Reset GPU");
  Serial.println("  seed <n>   - Set random seed");
  Serial.println("  verbose    - Toggle verbose mode");
}

void handleCommand(const char* cmd) {
  char arg[64] = "";
  
  // Parse command and argument
  char command[32];
  if (sscanf(cmd, "%31s %63s", command, arg) < 1) {
    return;
  }
  
  // Process commands
  if (strcmp(command, "help") == 0 || strcmp(command, "?") == 0) {
    printHelp();
  }
  else if (strcmp(command, "list") == 0) {
    Serial.println("=== Available Tests ===");
    const TestList& tests = coordinator.tests();
    for (int i = 0; i < tests.getTestCount(); i++) {
      const TestDefinition* t = tests.getTest(i);
      if (t) {
        Serial.printf("  [%d] %s (%dms) %s\n", 
                      t->test_id, t->name, t->timeout_ms,
                      t->enabled ? "" : "[DISABLED]");
      }
    }
    Serial.printf("Total: %d enabled\n", tests.getEnabledCount());
  }
  else if (strcmp(command, "run") == 0) {
    if (arg[0] == '\0') {
      Serial.println("Usage: run <test_name>");
      return;
    }
    coordinator.runSingleTest(arg);
  }
  else if (strcmp(command, "all") == 0) {
    int iterations = 1;
    if (arg[0] != '\0') {
      iterations = atoi(arg);
      if (iterations < 1) iterations = 1;
      if (iterations > 1000) iterations = 1000;
    }
    coordinator.runAllTests(iterations);
  }
  else if (strcmp(command, "stable") == 0) {
    Serial.println("Running until stable (Ctrl+C to stop)...");
    int passes = 5;
    int max_iters = 100;
    if (arg[0] != '\0') passes = atoi(arg);
    bool success = coordinator.runUntilStable(passes, max_iters);
    Serial.printf("Stability test %s\n", success ? "PASSED" : "FAILED");
  }
  else if (strcmp(command, "continuous") == 0) {
    Serial.println("Starting continuous validation (type 'stop' to end)...");
    coordinator.runContinuous();
  }
  else if (strcmp(command, "stop") == 0) {
    coordinator.stop();
    Serial.println("Testing stopped");
  }
  else if (strcmp(command, "status") == 0) {
    auto stats = coordinator.results().getStats();
    float temp = coordinator.getGpuTemperature();
    
    Serial.println("=== Status ===");
    Serial.printf("Running: %s\n", coordinator.isRunning() ? "Yes" : "No");
    Serial.printf("Tests executed: %d\n", stats.total);
    Serial.printf("Pass rate: %.1f%%\n", stats.pass_rate);
    Serial.printf("GPU temperature: %.1fC\n", temp);
  }
  else if (strcmp(command, "results") == 0) {
    auto stats = coordinator.results().getStats();
    
    Serial.println("=== Test Results ===");
    Serial.printf("Total: %d, Passed: %d, Failed: %d\n",
                  stats.total, stats.passed, stats.failed);
    Serial.printf("Pass rate: %.1f%%\n", stats.pass_rate);
    Serial.printf("Avg duration: %dms, Max: %dms\n",
                  stats.avg_duration_ms, stats.max_duration_ms);
    
    // Show per-test pass rates
    Serial.println("\nPer-test pass rates:");
    const TestList& tests = coordinator.tests();
    for (int i = 0; i < tests.getTestCount(); i++) {
      const TestDefinition* t = tests.getTest(i);
      if (t) {
        float rate = coordinator.results().getPassRate(t->name);
        Serial.printf("  %s: %.1f%%\n", t->name, rate);
      }
    }
  }
  else if (strcmp(command, "temp") == 0) {
    float temp = coordinator.getGpuTemperature();
    Serial.printf("GPU temperature: %.1fC\n", temp);
  }
  else if (strcmp(command, "reset") == 0) {
    if (coordinator.resetGpu()) {
      Serial.println("GPU reset successful");
    } else {
      Serial.println("GPU reset failed");
    }
  }
  else if (strcmp(command, "seed") == 0) {
    if (arg[0] != '\0') {
      uint32_t seed = strtoul(arg, NULL, 0);
      coordinator.setBaseSeed(seed);
      Serial.printf("Seed set to 0x%08X\n", seed);
    } else {
      Serial.println("Usage: seed <value>");
    }
  }
  else if (strcmp(command, "verbose") == 0) {
    static bool verbose = false;
    verbose = !verbose;
    coordinator.logger().setVerbose(verbose);
    Serial.printf("Verbose mode: %s\n", verbose ? "ON" : "OFF");
  }
  else if (strcmp(command, "clock") == 0) {
    if (arg[0] != '\0') {
      uint32_t freq = atoi(arg);
      if (coordinator.setGpuClock(freq)) {
        Serial.printf("GPU clock set to %dMHz\n", freq);
      } else {
        Serial.println("Failed to set GPU clock");
      }
    } else {
      Serial.println("Usage: clock <freq_mhz>");
    }
  }
  else if (strcmp(command, "regression") == 0) {
    char report[2048];
    regressionTracker.generateReport(report, sizeof(report));
    Serial.println(report);
  }
  else {
    Serial.printf("Unknown command: %s\n", command);
    Serial.println("Type 'help' for available commands");
  }
}

// ============================================================
// Arduino Setup
// ============================================================

void setup() {
  // Initialize debug serial
  Serial.begin(115200);
  while (!Serial) delay(10);
  
  Serial.println();
  Serial.println("=== GPU Test Coordinator ===");
  Serial.println("Initializing...");
  
  // Initialize GPU UART
  // Serial1.begin(GPU_UART_BAUD, SERIAL_8N1, GPU_UART_RX, GPU_UART_TX);
  
  // Initialize coordinator
  if (!coordinator.initialize(GPU_UART_BAUD)) {
    Serial.println("ERROR: Failed to initialize coordinator");
    Serial.println("Check GPU connection and UART pins");
    while (1) delay(1000);
  }
  
  Serial.println("GPU connected successfully");
  
  // Add test definitions
  suites::addAllTests(coordinator.tests());
  coordinator.tests().sortByPriority();
  
  Serial.printf("Loaded %d tests\n", coordinator.tests().getTestCount());
  
  // Configure logger
  coordinator.logger().setVerbose(false);
  
  // Print help
  printHelp();
  Serial.println();
  Serial.print("> ");
}

// ============================================================
// Arduino Loop
// ============================================================

void loop() {
  // Read serial commands
  static char cmdBuffer[128];
  static int cmdIndex = 0;
  
  while (Serial.available()) {
    char c = Serial.read();
    
    if (c == '\n' || c == '\r') {
      if (cmdIndex > 0) {
        cmdBuffer[cmdIndex] = '\0';
        Serial.println();
        handleCommand(cmdBuffer);
        Serial.print("> ");
        cmdIndex = 0;
      }
    }
    else if (c == '\b' || c == 127) {
      if (cmdIndex > 0) {
        cmdIndex--;
        Serial.print("\b \b");
      }
    }
    else if (cmdIndex < (int)sizeof(cmdBuffer) - 1) {
      cmdBuffer[cmdIndex++] = c;
      Serial.print(c);
    }
  }
  
  // Yield to other tasks
  delay(1);
}

// ============================================================
// Example: Automated Test Sequence
// ============================================================

void runAutomatedTestSequence() {
  Serial.println("=== Automated Test Sequence ===");
  
  // Phase 1: Quick sanity check
  Serial.println("\n--- Phase 1: Sanity Check ---");
  coordinator.runAllTests(1);
  
  auto stats = coordinator.results().getStats();
  if (stats.pass_rate < 90.0f) {
    Serial.println("ERROR: Sanity check failed, aborting");
    return;
  }
  
  // Phase 2: Thermal check
  Serial.println("\n--- Phase 2: Thermal Check ---");
  float temp = coordinator.getGpuTemperature();
  Serial.printf("GPU temperature: %.1fC\n", temp);
  
  if (temp > 60.0f) {
    Serial.println("WARNING: GPU too hot, waiting to cool down...");
    while (coordinator.getGpuTemperature() > 45.0f) {
      delay(5000);
    }
  }
  
  // Phase 3: Stability testing
  Serial.println("\n--- Phase 3: Stability Testing ---");
  coordinator.results().clear();
  bool stable = coordinator.runUntilStable(5, 50);
  
  if (!stable) {
    Serial.println("WARNING: Not all tests stable");
    
    // Show flaky tests
    const TestList& tests = coordinator.tests();
    for (int i = 0; i < tests.getTestCount(); i++) {
      const TestDefinition* t = tests.getTest(i);
      if (t) {
        float rate = coordinator.results().getPassRate(t->name);
        if (rate > 0 && rate < 100.0f) {
          Serial.printf("  FLAKY: %s (%.1f%% pass)\n", t->name, rate);
        }
      }
    }
  }
  
  // Phase 4: Stress testing
  Serial.println("\n--- Phase 4: Stress Testing ---");
  coordinator.tests().disableTest("STRESS_THERMAL");  // Skip long thermal test
  coordinator.runSingleTest("STRESS_MEMORY");
  coordinator.runSingleTest("STRESS_COMMANDS");
  coordinator.runSingleTest("STRESS_PRECISION");
  
  // Final summary
  Serial.println("\n=== Final Summary ===");
  stats = coordinator.results().getStats();
  Serial.printf("Total tests: %d\n", stats.total);
  Serial.printf("Pass rate: %.1f%%\n", stats.pass_rate);
  Serial.printf("Stability: %s\n", stable ? "STABLE" : "UNSTABLE");
  
  if (stats.pass_rate >= 99.0f && stable) {
    Serial.println("\n*** ALL TESTS PASSED ***");
  } else {
    Serial.println("\n*** TESTS NEED ATTENTION ***");
  }
}

// ============================================================
// Example: Continuous Integration Mode
// ============================================================

int runCIMode() {
  Serial.println("=== CI Mode ===");
  
  // Run all tests 3 times
  coordinator.runAllTests(3);
  
  auto stats = coordinator.results().getStats();
  
  // Generate regression report
  char report[2048];
  regressionTracker.generateReport(report, sizeof(report));
  Serial.println(report);
  
  // Return exit code
  if (stats.pass_rate >= 100.0f) {
    Serial.println("CI: PASSED");
    return 0;
  } else if (stats.pass_rate >= 90.0f) {
    Serial.println("CI: PASSED WITH WARNINGS");
    return 1;
  } else {
    Serial.println("CI: FAILED");
    return 2;
  }
}
