/*****************************************************************
 * File:      GpuRegressionTracker.hpp
 * Category:  GPU Driver / Regression Testing
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Advanced regression tracking system for detecting performance
 *    degradation, visual differences, and behavioral changes across
 *    driver versions, firmware revisions, and configuration changes.
 * 
 * Features:
 *    - Baseline storage and comparison
 *    - Multi-version tracking
 *    - Performance trend analysis
 *    - Visual regression detection
 *    - Automated alerting
 *    - Historical data management
 *****************************************************************/

#ifndef GPU_REGRESSION_TRACKER_HPP_
#define GPU_REGRESSION_TRACKER_HPP_

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <math.h>
#include "GpuISA.hpp"
#include "GpuDiagnostics.hpp"

namespace gpu {
namespace regression {

using namespace gpu::diagnostics;

// ============================================================
// Constants
// ============================================================

constexpr int MAX_BASELINES = 128;
constexpr int MAX_TEST_RESULTS = 512;
constexpr int MAX_VERSIONS = 32;
constexpr int MAX_TREND_POINTS = 256;

// ============================================================
// Version Information
// ============================================================

struct VersionInfo {
  uint8_t major;
  uint8_t minor;
  uint8_t patch;
  uint8_t build;
  char commit_hash[12];
  uint32_t build_timestamp;
  
  VersionInfo() : major(0), minor(0), patch(0), build(0), build_timestamp(0) {
    commit_hash[0] = '\0';
  }
  
  VersionInfo(uint8_t maj, uint8_t min, uint8_t pat, uint8_t bld = 0)
    : major(maj), minor(min), patch(pat), build(bld), build_timestamp(0) {
    commit_hash[0] = '\0';
  }
  
  uint32_t toInt() const {
    return ((uint32_t)major << 24) | ((uint32_t)minor << 16) |
           ((uint32_t)patch << 8) | build;
  }
  
  bool operator==(const VersionInfo& other) const {
    return toInt() == other.toInt();
  }
  
  bool operator<(const VersionInfo& other) const {
    return toInt() < other.toInt();
  }
  
  void toString(char* buf, size_t size) const {
    snprintf(buf, size, "%d.%d.%d.%d", major, minor, patch, build);
  }
};

// ============================================================
// Test Configuration
// ============================================================

struct TestConfiguration {
  // Hardware config
  uint32_t clock_freq_mhz;
  uint32_t memory_kb;
  uint8_t display_count;
  bool hardware_acceleration;
  
  // Software config
  VersionInfo driver_version;
  VersionInfo firmware_version;
  
  // Test environment
  float ambient_temp_c;
  uint32_t test_seed;
  
  TestConfiguration() : clock_freq_mhz(240), memory_kb(320), display_count(1),
                        hardware_acceleration(true), ambient_temp_c(25.0f),
                        test_seed(0) {}
  
  uint32_t hash() const {
    uint32_t h = clock_freq_mhz;
    h = h * 31 + memory_kb;
    h = h * 31 + display_count;
    h = h * 31 + hardware_acceleration;
    h = h * 31 + driver_version.toInt();
    h = h * 31 + firmware_version.toInt();
    return h;
  }
};

// ============================================================
// Baseline Data
// ============================================================

enum class BaselineType : uint8_t {
  VISUAL,        // Frame output checksum
  PERFORMANCE,   // Timing metrics
  MEMORY,        // Memory usage
  CORRECTNESS,   // Numerical accuracy
  FULL,          // All of the above
};

struct BaselineMetrics {
  // Visual
  uint32_t frame_checksum;
  float frame_similarity;  // Expected similarity to reference
  
  // Performance
  float avg_frame_time_ms;
  float p95_frame_time_ms;
  float p99_frame_time_ms;
  float min_fps;
  float max_fps;
  
  // Memory
  uint32_t peak_memory_kb;
  uint32_t avg_memory_kb;
  int allocation_count;
  
  // Correctness
  float max_numerical_error;
  float avg_numerical_error;
  int precision_failures;
  
  BaselineMetrics() {
    memset(this, 0, sizeof(BaselineMetrics));
    frame_similarity = 100.0f;
  }
};

struct Baseline {
  uint32_t baseline_id;
  char test_name[48];
  BaselineType type;
  TestConfiguration config;
  BaselineMetrics metrics;
  uint32_t created_timestamp;
  uint32_t iterations;  // How many times tested
  
  // Tolerance for comparison
  float performance_tolerance;  // Percent deviation allowed
  float memory_tolerance;
  float precision_tolerance;
  
  Baseline() : baseline_id(0), type(BaselineType::FULL),
               created_timestamp(0), iterations(0),
               performance_tolerance(0.1f), memory_tolerance(0.1f),
               precision_tolerance(0.001f) {
    test_name[0] = '\0';
  }
};

// ============================================================
// Test Result
// ============================================================

enum class RegressionStatus : uint8_t {
  PASS,
  WARN,       // Within warning threshold
  REGRESSION, // Performance/memory degraded
  VISUAL_DIFF,// Visual output differs
  FAILURE,    // Test failed to complete
};

struct TestResult {
  uint32_t result_id;
  uint32_t baseline_id;
  char test_name[48];
  
  TestConfiguration config;
  BaselineMetrics measured;
  
  RegressionStatus status;
  float deviation_percent;  // From baseline
  
  uint32_t timestamp;
  uint32_t duration_ms;
  
  char failure_reason[128];
  
  TestResult() : result_id(0), baseline_id(0), status(RegressionStatus::PASS),
                 deviation_percent(0), timestamp(0), duration_ms(0) {
    test_name[0] = '\0';
    failure_reason[0] = '\0';
  }
};

// ============================================================
// Trend Data Point
// ============================================================

struct TrendPoint {
  uint32_t timestamp;
  VersionInfo version;
  float value;
  RegressionStatus status;
  
  TrendPoint() : timestamp(0), value(0), status(RegressionStatus::PASS) {}
};

struct Trend {
  char metric_name[32];
  char test_name[48];
  
  TrendPoint points[MAX_TREND_POINTS];
  int point_count;
  
  // Statistics
  float min_value;
  float max_value;
  float avg_value;
  float trend_slope;  // Positive = getting worse (for time metrics)
  
  Trend() : point_count(0), min_value(0), max_value(0),
            avg_value(0), trend_slope(0) {
    metric_name[0] = '\0';
    test_name[0] = '\0';
  }
  
  void addPoint(uint32_t timestamp, const VersionInfo& ver,
                float value, RegressionStatus status) {
    if (point_count >= MAX_TREND_POINTS) {
      // Shift array (lose oldest)
      memmove(&points[0], &points[1], sizeof(TrendPoint) * (MAX_TREND_POINTS - 1));
      point_count = MAX_TREND_POINTS - 1;
    }
    
    TrendPoint& p = points[point_count++];
    p.timestamp = timestamp;
    p.version = ver;
    p.value = value;
    p.status = status;
    
    recalculateStats();
  }
  
  void recalculateStats() {
    if (point_count == 0) return;
    
    min_value = points[0].value;
    max_value = points[0].value;
    float sum = 0;
    
    for (int i = 0; i < point_count; i++) {
      if (points[i].value < min_value) min_value = points[i].value;
      if (points[i].value > max_value) max_value = points[i].value;
      sum += points[i].value;
    }
    
    avg_value = sum / point_count;
    
    // Calculate trend slope (simple linear regression)
    if (point_count >= 2) {
      float sum_x = 0, sum_y = 0, sum_xy = 0, sum_xx = 0;
      for (int i = 0; i < point_count; i++) {
        float x = (float)i;
        float y = points[i].value;
        sum_x += x;
        sum_y += y;
        sum_xy += x * y;
        sum_xx += x * x;
      }
      
      float n = (float)point_count;
      float denom = n * sum_xx - sum_x * sum_x;
      if (fabsf(denom) > 0.0001f) {
        trend_slope = (n * sum_xy - sum_x * sum_y) / denom;
      }
    }
  }
};

// ============================================================
// Regression Alert
// ============================================================

enum class AlertSeverity : uint8_t {
  INFO,
  WARNING,
  CRITICAL,
};

struct RegressionAlert {
  AlertSeverity severity;
  char test_name[48];
  char metric_name[32];
  char message[128];
  
  VersionInfo baseline_version;
  VersionInfo current_version;
  
  float baseline_value;
  float current_value;
  float deviation_percent;
  
  uint32_t timestamp;
  
  RegressionAlert() : severity(AlertSeverity::INFO), baseline_value(0),
                      current_value(0), deviation_percent(0), timestamp(0) {
    test_name[0] = '\0';
    metric_name[0] = '\0';
    message[0] = '\0';
  }
};

// ============================================================
// Alert Callback
// ============================================================

typedef void (*AlertCallback)(const RegressionAlert& alert, void* user_data);

// ============================================================
// Regression Tracker
// ============================================================

class AdvancedRegressionTracker {
public:
  AdvancedRegressionTracker() 
    : baseline_count_(0), result_count_(0), next_baseline_id_(1),
      next_result_id_(1), alert_callback_(nullptr), alert_user_data_(nullptr) {
    
    // Default thresholds
    warning_threshold_ = 0.05f;   // 5% deviation
    regression_threshold_ = 0.10f;  // 10% deviation
    critical_threshold_ = 0.25f;   // 25% deviation
  }
  
  // --------------------------------------------------------
  // Baseline Management
  // --------------------------------------------------------
  
  int createBaseline(const char* test_name, BaselineType type,
                     const TestConfiguration& config,
                     const BaselineMetrics& metrics) {
    if (baseline_count_ >= MAX_BASELINES) return -1;
    
    Baseline& b = baselines_[baseline_count_++];
    b.baseline_id = next_baseline_id_++;
    strncpy(b.test_name, test_name, sizeof(b.test_name) - 1);
    b.type = type;
    b.config = config;
    b.metrics = metrics;
    b.created_timestamp = 0;  // Would be set by caller
    b.iterations = 1;
    
    return b.baseline_id;
  }
  
  const Baseline* getBaseline(int id) const {
    for (int i = 0; i < baseline_count_; i++) {
      if (baselines_[i].baseline_id == (uint32_t)id) {
        return &baselines_[i];
      }
    }
    return nullptr;
  }
  
  const Baseline* findBaseline(const char* test_name,
                               const TestConfiguration& config) const {
    for (int i = 0; i < baseline_count_; i++) {
      if (strcmp(baselines_[i].test_name, test_name) == 0) {
        // Check if config is compatible
        if (baselines_[i].config.hash() == config.hash()) {
          return &baselines_[i];
        }
      }
    }
    return nullptr;
  }
  
  bool updateBaseline(int id, const BaselineMetrics& metrics) {
    for (int i = 0; i < baseline_count_; i++) {
      if (baselines_[i].baseline_id == (uint32_t)id) {
        // Running average for performance metrics
        Baseline& b = baselines_[i];
        float n = (float)b.iterations;
        b.metrics.avg_frame_time_ms = (b.metrics.avg_frame_time_ms * n + 
                                        metrics.avg_frame_time_ms) / (n + 1);
        b.metrics.peak_memory_kb = (uint32_t)((b.metrics.peak_memory_kb * n +
                                                metrics.peak_memory_kb) / (n + 1));
        b.iterations++;
        return true;
      }
    }
    return false;
  }
  
  // --------------------------------------------------------
  // Test Execution and Comparison
  // --------------------------------------------------------
  
  TestResult compareToBaseline(const char* test_name,
                               const TestConfiguration& config,
                               const BaselineMetrics& measured) {
    TestResult result;
    result.result_id = next_result_id_++;
    strncpy(result.test_name, test_name, sizeof(result.test_name) - 1);
    result.config = config;
    result.measured = measured;
    
    const Baseline* baseline = findBaseline(test_name, config);
    if (!baseline) {
      // No baseline - create one
      result.baseline_id = createBaseline(test_name, BaselineType::FULL,
                                           config, measured);
      result.status = RegressionStatus::PASS;
      result.deviation_percent = 0;
      strcpy(result.failure_reason, "New baseline created");
    } else {
      result.baseline_id = baseline->baseline_id;
      
      // Compare metrics
      result.status = compareMetrics(*baseline, measured, result.deviation_percent,
                                      result.failure_reason, sizeof(result.failure_reason));
      
      // Generate alerts if needed
      if (result.status != RegressionStatus::PASS) {
        generateAlerts(*baseline, measured, result);
      }
    }
    
    // Store result
    if (result_count_ < MAX_TEST_RESULTS) {
      results_[result_count_++] = result;
    }
    
    // Update trends
    updateTrends(test_name, config.driver_version, measured, result.status);
    
    return result;
  }
  
  // --------------------------------------------------------
  // Trend Analysis
  // --------------------------------------------------------
  
  const Trend* getTrend(const char* test_name, const char* metric_name) const {
    for (int i = 0; i < trend_count_; i++) {
      if (strcmp(trends_[i].test_name, test_name) == 0 &&
          strcmp(trends_[i].metric_name, metric_name) == 0) {
        return &trends_[i];
      }
    }
    return nullptr;
  }
  
  bool isRegressing(const char* test_name, const char* metric_name) const {
    const Trend* trend = getTrend(test_name, metric_name);
    if (!trend || trend->point_count < 5) return false;
    
    // Check if trend is significantly negative (values increasing = worse for time)
    return trend->trend_slope > 0.01f;  // 1% per data point
  }
  
  // --------------------------------------------------------
  // Alert Configuration
  // --------------------------------------------------------
  
  void setAlertCallback(AlertCallback callback, void* user_data) {
    alert_callback_ = callback;
    alert_user_data_ = user_data;
  }
  
  void setThresholds(float warning, float regression, float critical) {
    warning_threshold_ = warning;
    regression_threshold_ = regression;
    critical_threshold_ = critical;
  }
  
  // --------------------------------------------------------
  // Reporting
  // --------------------------------------------------------
  
  struct Summary {
    int total_tests;
    int passed;
    int warnings;
    int regressions;
    int visual_diffs;
    int failures;
    
    float pass_rate;
    int baseline_count;
    
    Summary() : total_tests(0), passed(0), warnings(0), regressions(0),
                visual_diffs(0), failures(0), pass_rate(0), baseline_count(0) {}
  };
  
  Summary getSummary() const {
    Summary s;
    s.total_tests = result_count_;
    s.baseline_count = baseline_count_;
    
    for (int i = 0; i < result_count_; i++) {
      switch (results_[i].status) {
        case RegressionStatus::PASS: s.passed++; break;
        case RegressionStatus::WARN: s.warnings++; break;
        case RegressionStatus::REGRESSION: s.regressions++; break;
        case RegressionStatus::VISUAL_DIFF: s.visual_diffs++; break;
        case RegressionStatus::FAILURE: s.failures++; break;
      }
    }
    
    if (s.total_tests > 0) {
      s.pass_rate = (float)s.passed / (float)s.total_tests * 100.0f;
    }
    
    return s;
  }
  
  void generateReport(char* buffer, size_t buffer_size) const {
    Summary s = getSummary();
    
    int offset = 0;
    offset += snprintf(buffer + offset, buffer_size - offset,
      "=== Regression Test Report ===\n\n"
      "Total Tests: %d\n"
      "Pass Rate: %.1f%%\n\n"
      "Results:\n"
      "  Passed: %d\n"
      "  Warnings: %d\n"
      "  Regressions: %d\n"
      "  Visual Diffs: %d\n"
      "  Failures: %d\n\n"
      "Baselines: %d\n\n",
      s.total_tests, s.pass_rate,
      s.passed, s.warnings, s.regressions, s.visual_diffs, s.failures,
      s.baseline_count);
    
    // List regressions
    offset += snprintf(buffer + offset, buffer_size - offset,
      "--- Regressions ---\n");
    
    for (int i = 0; i < result_count_ && offset < (int)buffer_size - 200; i++) {
      if (results_[i].status == RegressionStatus::REGRESSION ||
          results_[i].status == RegressionStatus::FAILURE) {
        offset += snprintf(buffer + offset, buffer_size - offset,
          "  [%s] %.1f%% deviation: %s\n",
          results_[i].test_name,
          results_[i].deviation_percent,
          results_[i].failure_reason);
      }
    }
    
    buffer[offset] = '\0';
  }
  
  // --------------------------------------------------------
  // Persistence
  // --------------------------------------------------------
  
  // Serialize baselines to buffer
  size_t serializeBaselines(uint8_t* buffer, size_t max_size) const {
    if (max_size < 4) return 0;
    
    // Header: count
    buffer[0] = (baseline_count_ >> 0) & 0xFF;
    buffer[1] = (baseline_count_ >> 8) & 0xFF;
    buffer[2] = 0;  // Reserved
    buffer[3] = 1;  // Version
    
    size_t offset = 4;
    for (int i = 0; i < baseline_count_; i++) {
      size_t needed = sizeof(Baseline);
      if (offset + needed > max_size) break;
      
      memcpy(buffer + offset, &baselines_[i], sizeof(Baseline));
      offset += sizeof(Baseline);
    }
    
    return offset;
  }
  
  // Deserialize baselines from buffer
  bool deserializeBaselines(const uint8_t* buffer, size_t size) {
    if (size < 4) return false;
    
    int count = buffer[0] | (buffer[1] << 8);
    uint8_t version = buffer[3];
    
    if (version != 1) return false;
    
    size_t offset = 4;
    baseline_count_ = 0;
    
    for (int i = 0; i < count && i < MAX_BASELINES; i++) {
      if (offset + sizeof(Baseline) > size) break;
      
      memcpy(&baselines_[baseline_count_], buffer + offset, sizeof(Baseline));
      baseline_count_++;
      offset += sizeof(Baseline);
      
      // Track max ID
      if (baselines_[i].baseline_id >= next_baseline_id_) {
        next_baseline_id_ = baselines_[i].baseline_id + 1;
      }
    }
    
    return true;
  }
  
  // --------------------------------------------------------
  // Cleanup
  // --------------------------------------------------------
  
  void clearResults() {
    result_count_ = 0;
  }
  
  void clearAll() {
    baseline_count_ = 0;
    result_count_ = 0;
    trend_count_ = 0;
    next_baseline_id_ = 1;
    next_result_id_ = 1;
  }

private:
  Baseline baselines_[MAX_BASELINES];
  int baseline_count_;
  
  TestResult results_[MAX_TEST_RESULTS];
  int result_count_;
  
  Trend trends_[64];
  int trend_count_ = 0;
  
  uint32_t next_baseline_id_;
  uint32_t next_result_id_;
  
  AlertCallback alert_callback_;
  void* alert_user_data_;
  
  float warning_threshold_;
  float regression_threshold_;
  float critical_threshold_;
  
  // --------------------------------------------------------
  // Private Helpers
  // --------------------------------------------------------
  
  RegressionStatus compareMetrics(const Baseline& baseline,
                                   const BaselineMetrics& measured,
                                   float& deviation,
                                   char* reason, size_t reason_size) {
    deviation = 0;
    reason[0] = '\0';
    
    // Check visual first (binary)
    if (baseline.type == BaselineType::VISUAL || 
        baseline.type == BaselineType::FULL) {
      if (baseline.metrics.frame_checksum != measured.frame_checksum) {
        snprintf(reason, reason_size, "Visual output differs (checksum mismatch)");
        return RegressionStatus::VISUAL_DIFF;
      }
    }
    
    // Check performance
    if (baseline.type == BaselineType::PERFORMANCE || 
        baseline.type == BaselineType::FULL) {
      if (baseline.metrics.avg_frame_time_ms > 0) {
        float perf_dev = (measured.avg_frame_time_ms - baseline.metrics.avg_frame_time_ms) /
                         baseline.metrics.avg_frame_time_ms;
        
        if (perf_dev > deviation) deviation = perf_dev;
        
        if (perf_dev > critical_threshold_) {
          snprintf(reason, reason_size, "Performance regression: %.1f%% slower",
                   perf_dev * 100);
          return RegressionStatus::REGRESSION;
        }
      }
    }
    
    // Check memory
    if (baseline.type == BaselineType::MEMORY || 
        baseline.type == BaselineType::FULL) {
      if (baseline.metrics.peak_memory_kb > 0) {
        float mem_dev = (float)(measured.peak_memory_kb - baseline.metrics.peak_memory_kb) /
                        (float)baseline.metrics.peak_memory_kb;
        
        if (mem_dev > deviation) deviation = mem_dev;
        
        if (mem_dev > critical_threshold_) {
          snprintf(reason, reason_size, "Memory regression: %.1f%% more",
                   mem_dev * 100);
          return RegressionStatus::REGRESSION;
        }
      }
    }
    
    // Check correctness
    if (baseline.type == BaselineType::CORRECTNESS || 
        baseline.type == BaselineType::FULL) {
      if (measured.precision_failures > baseline.metrics.precision_failures) {
        snprintf(reason, reason_size, "Precision regression: %d new failures",
                 measured.precision_failures - baseline.metrics.precision_failures);
        return RegressionStatus::REGRESSION;
      }
    }
    
    // Check warnings
    if (deviation > regression_threshold_) {
      snprintf(reason, reason_size, "Overall deviation %.1f%% exceeds threshold",
               deviation * 100);
      return RegressionStatus::REGRESSION;
    }
    
    if (deviation > warning_threshold_) {
      snprintf(reason, reason_size, "Minor deviation %.1f%%", deviation * 100);
      return RegressionStatus::WARN;
    }
    
    return RegressionStatus::PASS;
  }
  
  void generateAlerts(const Baseline& baseline,
                      const BaselineMetrics& measured,
                      const TestResult& result) {
    if (!alert_callback_) return;
    
    RegressionAlert alert;
    strncpy(alert.test_name, result.test_name, sizeof(alert.test_name) - 1);
    alert.baseline_version = baseline.config.driver_version;
    alert.current_version = result.config.driver_version;
    alert.deviation_percent = result.deviation_percent * 100;
    strncpy(alert.message, result.failure_reason, sizeof(alert.message) - 1);
    
    switch (result.status) {
      case RegressionStatus::WARN:
        alert.severity = AlertSeverity::WARNING;
        break;
      case RegressionStatus::REGRESSION:
      case RegressionStatus::VISUAL_DIFF:
        alert.severity = AlertSeverity::CRITICAL;
        break;
      case RegressionStatus::FAILURE:
        alert.severity = AlertSeverity::CRITICAL;
        break;
      default:
        return;  // No alert for PASS
    }
    
    // Determine which metric caused the issue
    if (result.status == RegressionStatus::VISUAL_DIFF) {
      strcpy(alert.metric_name, "frame_checksum");
      alert.baseline_value = (float)baseline.metrics.frame_checksum;
      alert.current_value = (float)measured.frame_checksum;
    } else {
      strcpy(alert.metric_name, "avg_frame_time");
      alert.baseline_value = baseline.metrics.avg_frame_time_ms;
      alert.current_value = measured.avg_frame_time_ms;
    }
    
    alert_callback_(alert, alert_user_data_);
  }
  
  void updateTrends(const char* test_name, const VersionInfo& version,
                    const BaselineMetrics& measured, RegressionStatus status) {
    // Update frame time trend
    Trend* trend = findOrCreateTrend(test_name, "frame_time");
    if (trend) {
      trend->addPoint(0, version, measured.avg_frame_time_ms, status);
    }
    
    // Update memory trend
    trend = findOrCreateTrend(test_name, "memory");
    if (trend) {
      trend->addPoint(0, version, (float)measured.peak_memory_kb, status);
    }
  }
  
  Trend* findOrCreateTrend(const char* test_name, const char* metric_name) {
    // Find existing
    for (int i = 0; i < trend_count_; i++) {
      if (strcmp(trends_[i].test_name, test_name) == 0 &&
          strcmp(trends_[i].metric_name, metric_name) == 0) {
        return &trends_[i];
      }
    }
    
    // Create new
    if (trend_count_ < 64) {
      Trend& t = trends_[trend_count_++];
      strncpy(t.test_name, test_name, sizeof(t.test_name) - 1);
      strncpy(t.metric_name, metric_name, sizeof(t.metric_name) - 1);
      return &t;
    }
    
    return nullptr;
  }
};

} // namespace regression
} // namespace gpu

#endif // GPU_REGRESSION_TRACKER_HPP_
