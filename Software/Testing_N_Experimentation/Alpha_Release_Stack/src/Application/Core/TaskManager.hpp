/*****************************************************************
 * File:      TaskManager.hpp
 * Category:  Application/Core
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    FreeRTOS task manager for dual-core application.
 *    Manages task creation, priorities, and stack monitoring.
 * 
 *    Core 0 Tasks:
 *    - Sensor polling (IMU, Environmental, GPS)
 *    - Network/Web server
 *    - Input processing (buttons)
 *    - Animation state updates
 * 
 *    Core 1 Tasks:
 *    - GPU Pipeline (rendering + GPU commands)
 *    - Frame timing
 *****************************************************************/

#ifndef ARCOS_APPLICATION_TASK_MANAGER_HPP_
#define ARCOS_APPLICATION_TASK_MANAGER_HPP_

#include <stdint.h>
#include <cstring>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_timer.h"

namespace Application {

// ============================================================
// Task Configuration
// ============================================================

struct TaskConfig {
  const char* name;
  uint32_t stackSize;     // In words (not bytes)
  uint8_t priority;
  int coreId;             // 0, 1, or -1 for any
  TaskFunction_t entry;
  void* param;
};

// ============================================================
// Task Statistics
// ============================================================

struct TaskStats {
  const char* name;
  uint32_t runCount;
  uint32_t highWaterMark;   // Minimum free stack (words)
  uint32_t lastRunTimeUs;
  uint32_t avgRunTimeUs;
  uint32_t maxRunTimeUs;
};

// ============================================================
// Task Manager
// ============================================================

class TaskManager {
public:
  static constexpr const char* TAG = "TaskMgr";
  static constexpr int MAX_TASKS = 16;
  
  TaskManager()
    : taskCount_(0)
    , initialized_(false)
  {
    memset(handles_, 0, sizeof(handles_));
    memset(configs_, 0, sizeof(configs_));
  }
  
  /** Initialize the task manager */
  bool init() {
    if (initialized_) return true;
    
    ESP_LOGI(TAG, "Initializing task manager");
    initialized_ = true;
    return true;
  }
  
  /** Register a task (doesn't start it yet)
   * @param config Task configuration
   * @return Task ID (index) or -1 on failure
   */
  int registerTask(const TaskConfig& config) {
    if (taskCount_ >= MAX_TASKS) {
      ESP_LOGE(TAG, "Max tasks reached");
      return -1;
    }
    
    int id = taskCount_++;
    configs_[id] = config;
    handles_[id] = nullptr;
    
    // Initialize stats
    stats_[id].name = config.name;
    stats_[id].runCount = 0;
    stats_[id].highWaterMark = config.stackSize;
    stats_[id].lastRunTimeUs = 0;
    stats_[id].avgRunTimeUs = 0;
    stats_[id].maxRunTimeUs = 0;
    
    ESP_LOGI(TAG, "Registered task: %s (id=%d, core=%d, prio=%d)",
             config.name, id, config.coreId, config.priority);
    
    return id;
  }
  
  /** Start a registered task
   * @param taskId Task ID from registerTask
   * @return true if task started
   */
  bool startTask(int taskId) {
    if (taskId < 0 || taskId >= taskCount_) return false;
    if (handles_[taskId] != nullptr) return true;  // Already running
    
    const TaskConfig& cfg = configs_[taskId];
    
    BaseType_t result;
    if (cfg.coreId >= 0 && cfg.coreId <= 1) {
      result = xTaskCreatePinnedToCore(
        cfg.entry,
        cfg.name,
        cfg.stackSize,
        cfg.param,
        cfg.priority,
        &handles_[taskId],
        cfg.coreId
      );
    } else {
      result = xTaskCreate(
        cfg.entry,
        cfg.name,
        cfg.stackSize,
        cfg.param,
        cfg.priority,
        &handles_[taskId]
      );
    }
    
    if (result != pdPASS) {
      ESP_LOGE(TAG, "Failed to start task: %s", cfg.name);
      return false;
    }
    
    ESP_LOGI(TAG, "Started task: %s", cfg.name);
    return true;
  }
  
  /** Stop a running task
   * @param taskId Task ID
   */
  void stopTask(int taskId) {
    if (taskId < 0 || taskId >= taskCount_) return;
    if (handles_[taskId] == nullptr) return;
    
    ESP_LOGI(TAG, "Stopping task: %s", configs_[taskId].name);
    vTaskDelete(handles_[taskId]);
    handles_[taskId] = nullptr;
  }
  
  /** Start all registered tasks */
  void startAll() {
    ESP_LOGI(TAG, "Starting all %d tasks", taskCount_);
    for (int i = 0; i < taskCount_; i++) {
      startTask(i);
    }
  }
  
  /** Stop all tasks */
  void stopAll() {
    ESP_LOGI(TAG, "Stopping all tasks");
    for (int i = 0; i < taskCount_; i++) {
      stopTask(i);
    }
  }
  
  /** Update task statistics (call from task to update run time) */
  void updateTaskStats(int taskId, uint32_t runTimeUs) {
    if (taskId < 0 || taskId >= taskCount_) return;
    
    TaskStats& st = stats_[taskId];
    st.runCount++;
    st.lastRunTimeUs = runTimeUs;
    
    // Update average (simple moving average)
    if (st.avgRunTimeUs == 0) {
      st.avgRunTimeUs = runTimeUs;
    } else {
      st.avgRunTimeUs = (st.avgRunTimeUs * 7 + runTimeUs) / 8;
    }
    
    if (runTimeUs > st.maxRunTimeUs) {
      st.maxRunTimeUs = runTimeUs;
    }
    
    // Update stack high water mark
    if (handles_[taskId]) {
      st.highWaterMark = uxTaskGetStackHighWaterMark(handles_[taskId]);
    }
  }
  
  /** Get task statistics */
  TaskStats getStats(int taskId) const {
    if (taskId < 0 || taskId >= taskCount_) return TaskStats{};
    return stats_[taskId];
  }
  
  /** Print all task statistics */
  void printStats() {
    ESP_LOGI(TAG, "=== Task Statistics ===");
    for (int i = 0; i < taskCount_; i++) {
      const TaskStats& st = stats_[i];
      ESP_LOGI(TAG, "%s: runs=%lu, avg=%luus, max=%luus, stack=%lu",
               st.name, 
               (unsigned long)st.runCount,
               (unsigned long)st.avgRunTimeUs,
               (unsigned long)st.maxRunTimeUs,
               (unsigned long)st.highWaterMark);
    }
  }
  
  /** Check if task is running */
  bool isTaskRunning(int taskId) const {
    if (taskId < 0 || taskId >= taskCount_) return false;
    return handles_[taskId] != nullptr;
  }
  
  /** Get task handle for direct FreeRTOS operations */
  TaskHandle_t getHandle(int taskId) const {
    if (taskId < 0 || taskId >= taskCount_) return nullptr;
    return handles_[taskId];
  }
  
  /** Get task count */
  int getTaskCount() const { return taskCount_; }
  
private:
  TaskConfig configs_[MAX_TASKS];
  TaskHandle_t handles_[MAX_TASKS];
  TaskStats stats_[MAX_TASKS];
  int taskCount_;
  bool initialized_;
};

// ============================================================
// Global Instance
// ============================================================

inline TaskManager& getTaskManager() {
  static TaskManager instance;
  return instance;
}

} // namespace Application

#endif // ARCOS_APPLICATION_TASK_MANAGER_HPP_
