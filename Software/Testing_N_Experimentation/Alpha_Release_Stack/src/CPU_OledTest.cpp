/*****************************************************************
 * @file CPU_OledTest.cpp
 * @brief OLED Display Test Application
 * 
 * This application tests the OLED display system by running
 * comprehensive tests including text rendering, shapes, UI widgets,
 * patterns, and stress tests.
 * 
 * Hardware:
 *   - CPU: ESP32-S3 (main controller)
 *   - GPU: ESP32-S3 (display controller)
 *   - OLED: SH1107 128x128 monochrome (connected to GPU via I2C)
 * 
 * Communication:
 *   CPU → UART (10Mbps) → GPU → I2C → OLED
 * 
 * Build:
 *   pio run -e CPU_OledTest
 * 
 * Upload and Monitor:
 *   pio run -e CPU_OledTest --target upload --upload-port COM6
 *   pio device monitor -p COM6 -b 115200
 * 
 * @author ARCOS Framework
 * @version 1.0
 *****************************************************************/

#include <cstdio>
#include <cstring>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"

#include "GpuDriver/GpuCommands.hpp"
#include "SystemAPI/Testing/OledTestHarness.hpp"

static const char* TAG = "CPU_OledTest";

// Global instances
static GpuCommands gpu;
static SystemAPI::Testing::OledTestHarness oledTest;

/**
 * @brief Print startup banner
 */
void printBanner() {
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║                    OLED DISPLAY TEST                          ║\n");
    printf("║                                                               ║\n");
    printf("║  Display: SH1107 128x128 Monochrome                           ║\n");
    printf("║  Interface: CPU -> UART -> GPU -> I2C -> OLED                 ║\n");
    printf("║                                                               ║\n");
    printf("║  Commands:                                                    ║\n");
    printf("║    OLED:FULL     - Full test suite                            ║\n");
    printf("║    OLED:QUICK    - Quick demo                                 ║\n");
    printf("║    OLED:TEXT     - Text tests                                 ║\n");
    printf("║    OLED:SHAPES   - Shape tests                                ║\n");
    printf("║    OLED:WIDGETS  - Widget tests                               ║\n");
    printf("║    OLED:PATTERNS - Pattern tests                              ║\n");
    printf("║    OLED:STRESS   - Stress tests                               ║\n");
    printf("║    OLED:CLEAR    - Clear display                              ║\n");
    printf("║    OLED:HELP     - Show help                                  ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n");
    printf("\n");
}

/**
 * @brief Initialize GPU communication
 */
bool initGpu() {
    ESP_LOGI(TAG, "Initializing GPU communication...");
    
    // Initialize GPU commands (includes 500ms startup delay)
    if (!gpu.init()) {
        ESP_LOGE(TAG, "Failed to initialize GPU!");
        return false;
    }
    
    ESP_LOGI(TAG, "GPU initialized");
    
    // Test GPU connection with ping
    uint32_t uptime = 0;
    if (gpu.pingWithResponse(uptime, 1000)) {
        ESP_LOGI(TAG, "GPU connected! Uptime: %lu ms", (unsigned long)uptime);
    } else {
        ESP_LOGW(TAG, "GPU ping timeout - continuing anyway...");
    }
    
    return true;
}

/**
 * @brief Command input task
 */
void commandTask(void* arg) {
    char cmdBuffer[64];
    int cmdIndex = 0;
    
    ESP_LOGI(TAG, "Command task started. Type commands and press Enter.");
    
    while (true) {
        int c = getchar();
        if (c == EOF) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        
        if (c == '\n' || c == '\r') {
            if (cmdIndex > 0) {
                cmdBuffer[cmdIndex] = '\0';
                printf("\n");
                
                // Convert to uppercase for matching
                for (int i = 0; i < cmdIndex; i++) {
                    if (cmdBuffer[i] >= 'a' && cmdBuffer[i] <= 'z') {
                        cmdBuffer[i] -= 32;
                    }
                }
                
                // Process command
                if (!oledTest.processCommand(cmdBuffer)) {
                    ESP_LOGW(TAG, "Unknown command: %s", cmdBuffer);
                    ESP_LOGI(TAG, "Type OLED:HELP for available commands");
                }
                
                cmdIndex = 0;
            }
        } else if (c == 127 || c == 8) {  // Backspace
            if (cmdIndex > 0) {
                cmdIndex--;
                printf("\b \b");
            }
        } else if (cmdIndex < 62) {
            cmdBuffer[cmdIndex++] = (char)c;
            putchar(c);
        }
    }
}

/**
 * @brief Main test task
 */
void mainTestTask(void* arg) {
    ESP_LOGI(TAG, "Main test task started");
    
    // Initialize OLED test harness
    if (!oledTest.init(&gpu)) {
        ESP_LOGE(TAG, "Failed to initialize OLED test harness!");
        vTaskDelete(nullptr);
        return;
    }
    
    ESP_LOGI(TAG, "OLED test harness initialized");
    
    // Wait a moment for system to settle
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // Run quick demo automatically on startup
    ESP_LOGI(TAG, "Running quick demo on startup...");
    oledTest.runQuickDemo();
    
    // Then run full test suite
    ESP_LOGI(TAG, "Running full test suite...");
    oledTest.runFullTestSuite();
    
    ESP_LOGI(TAG, "Startup tests complete. Use commands for more tests.");
    
    vTaskDelete(nullptr);
}

extern "C" void app_main(void) {
    printBanner();
    
    // Initialize GPU
    if (!initGpu()) {
        ESP_LOGE(TAG, "GPU initialization failed! Halting.");
        while (true) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
    
    // Create command input task
    xTaskCreate(
        commandTask,
        "cmd_task",
        4096,
        nullptr,
        2,
        nullptr
    );
    
    // Create main test task
    xTaskCreate(
        mainTestTask,
        "test_task",
        8192,
        nullptr,
        3,
        nullptr
    );
    
    ESP_LOGI(TAG, "Tasks created. Entering main loop.");
    
    // Main loop - can add periodic status updates here
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        
        // Optional: Print heap status periodically
        ESP_LOGD(TAG, "Free heap: %lu bytes", (unsigned long)esp_get_free_heap_size());
    }
}
