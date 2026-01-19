/*****************************************************************
 * GpuUartMutex.hpp - Global Mutex for GPU UART Access
 * 
 * This mutex must be acquired by ANY code that sends commands to
 * the GPU over UART. This prevents race conditions between:
 * - Core 0: GpuCommands (web callbacks, sprite uploads)
 * - Core 1: GpuPipeline/GpuProtocol (animation rendering)
 * 
 * Without this mutex, interleaved UART writes corrupt the command
 * stream, causing sprite uploads to fail while animation runs.
 * 
 * Usage:
 *   #include "GpuDriver/GpuUartMutex.hpp"
 *   
 *   void sendSomething() {
 *       GpuUartLock lock;  // Automatically acquires/releases mutex
 *       uart_write_bytes(...);
 *   }
 * 
 * The mutex is created on first use (lazy initialization).
 *****************************************************************/

#ifndef GPU_UART_MUTEX_HPP_
#define GPU_UART_MUTEX_HPP_

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"

namespace GpuUart {

/**
 * Get the global GPU UART mutex.
 * Creates the mutex on first call (thread-safe via FreeRTOS).
 */
inline SemaphoreHandle_t getMutex() {
    static SemaphoreHandle_t s_mutex = nullptr;
    
    if (s_mutex == nullptr) {
        s_mutex = xSemaphoreCreateMutex();
        if (s_mutex == nullptr) {
            ESP_LOGE("GpuUart", "Failed to create GPU UART mutex!");
        } else {
            ESP_LOGI("GpuUart", "GPU UART mutex created");
        }
    }
    
    return s_mutex;
}

/**
 * RAII lock guard for GPU UART mutex.
 * Acquires mutex on construction, releases on destruction.
 */
class GpuUartLock {
public:
    GpuUartLock(uint32_t timeoutMs = 500) : acquired_(false) {
        SemaphoreHandle_t mutex = getMutex();
        if (mutex != nullptr) {
            acquired_ = (xSemaphoreTake(mutex, pdMS_TO_TICKS(timeoutMs)) == pdTRUE);
            if (!acquired_) {
                ESP_LOGW("GpuUart", "Failed to acquire GPU UART mutex (timeout=%lu ms)", timeoutMs);
            }
        }
    }
    
    ~GpuUartLock() {
        if (acquired_) {
            SemaphoreHandle_t mutex = getMutex();
            if (mutex != nullptr) {
                xSemaphoreGive(mutex);
            }
        }
    }
    
    bool isAcquired() const { return acquired_; }
    
    // Non-copyable
    GpuUartLock(const GpuUartLock&) = delete;
    GpuUartLock& operator=(const GpuUartLock&) = delete;
    
private:
    bool acquired_;
};

} // namespace GpuUart

#endif // GPU_UART_MUTEX_HPP_
