/**
 * @file CPU_OledUITest.cpp
 * @brief Minimal test file for OLED UI Framework
 * 
 * Tests the core UI system with basic elements.
 * Build with: pio run -e CPU_OledUITest
 */

#include <cstdio>
#include <cstdint>
#include <cmath>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"

#include "GpuDriver/GpuCommands.hpp"

// Just include the core types for input events
#include "FrameworkAPI/OledUI/Core/Types.hpp"

using namespace OledUI;

static const char* TAG = "OLEDUI_TEST";

//=============================================================================
// Hardware Configuration
//=============================================================================
static constexpr int UART_TX_PIN = 12;
static constexpr int UART_RX_PIN = 11;
static constexpr int UART_BAUD = 10000000;

// Button pins (from LifecycleController: A=5, B=6, C=7, D=15)
// All 4 buttons working with new button board
static constexpr gpio_num_t BTN_A = GPIO_NUM_5;   // UP
static constexpr gpio_num_t BTN_B = GPIO_NUM_6;   // SELECT
static constexpr gpio_num_t BTN_C = GPIO_NUM_7;   // DOWN
static constexpr gpio_num_t BTN_D = GPIO_NUM_15;  // BACK

//=============================================================================
// Globals
//=============================================================================
static GpuCommands gpu;
static int menuIndex = 0;
static constexpr int MENU_ITEMS = 5;
static const char* menuItems[] = {
    "Item 1",
    "Item 2", 
    "Item 3",
    "Item 4",
    "Item 5"
};

// Button state tracking (edge detection)
static bool lastBtnA = true;      // true = released (high)
static bool lastBtnB = true;
static bool lastBtnC = true;
static bool lastBtnD = true;
static uint32_t lastButtonTime = 0;
static constexpr uint32_t DEBOUNCE_MS = 50;

// Helper function to get milliseconds
static inline uint32_t millis() {
    return (uint32_t)(esp_timer_get_time() / 1000);
}

//=============================================================================
// Button Reading (edge-triggered with debounce)
//=============================================================================
InputEvent readButtons() {
    uint32_t now = millis();
    
    // Read current button states (LOW = pressed with pull-up)
    int rawA = gpio_get_level(BTN_A);
    int rawB = gpio_get_level(BTN_B);
    int rawC = gpio_get_level(BTN_C);
    int rawD = gpio_get_level(BTN_D);
    
    // Debug: Print raw GPIO states every 500ms (more frequent for testing)
    static uint32_t lastDebugTime = 0;
    if (now - lastDebugTime >= 500) {
        ESP_LOGI(TAG, "Buttons: A(5)=%d B(6)=%d C(7)=%d D(15)=%d  [0=pressed, 1=released]", 
                 rawA, rawB, rawC, rawD);
        lastDebugTime = now;
    }
    
    // Debounce - ignore reads within DEBOUNCE_MS of last button event
    if (now - lastButtonTime < DEBOUNCE_MS) {
        return InputEvent::NONE;
    }
    
    bool btnA = rawA != 0;  // UP - true=released, false=pressed
    bool btnB = rawB != 0;  // SELECT
    bool btnC = rawC != 0;  // DOWN
    bool btnD = rawD != 0;  // LEFT/BACK
    
    InputEvent event = InputEvent::NONE;
    
    // Check for falling edge (was released, now pressed)
    if (lastBtnA && !btnA) {
        event = InputEvent::UP;
        lastButtonTime = now;
        ESP_LOGI(TAG, ">>> BUTTON A (GPIO5) PRESSED - UP");
    } else if (lastBtnC && !btnC) {
        event = InputEvent::DOWN;
        lastButtonTime = now;
        ESP_LOGI(TAG, ">>> BUTTON C (GPIO7) PRESSED - DOWN");
    } else if (lastBtnB && !btnB) {
        event = InputEvent::SELECT;
        lastButtonTime = now;
        ESP_LOGI(TAG, ">>> BUTTON B (GPIO6) PRESSED - SELECT");
    } else if (lastBtnD && !btnD) {
        event = InputEvent::BACK;
        lastButtonTime = now;
        ESP_LOGI(TAG, ">>> BUTTON D (GPIO15) PRESSED - BACK");
    }
    
    // Update last states
    lastBtnA = btnA;
    lastBtnB = btnB;
    lastBtnC = btnC;
    lastBtnD = btnD;
    
    return event;
}

//=============================================================================
// GPIO Initialization
//=============================================================================
void initButtons() {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BTN_A) | (1ULL << BTN_B) | 
                        (1ULL << BTN_C) | (1ULL << BTN_D),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
}

//=============================================================================
// Simple UI Rendering (manual for now until framework is complete)
//=============================================================================
void renderUI() {
    ESP_LOGI(TAG, "Rendering UI - menuIndex=%d", menuIndex);
    
    gpu.oledClear();
    
    // Draw a simple border
    gpu.oledRect(0, 0, 128, 128, true);
    
    // Draw title
    gpu.oledText(10, 5, "OLED UI Test", 1, true);
    
    // Draw horizontal line
    gpu.oledLine(0, 16, 127, 16, true);
    
    // Draw the currently selected item with its index
    char buf[32];
    snprintf(buf, sizeof(buf), "Selected: %d", menuIndex);
    gpu.oledText(10, 30, buf, 1, true);
    
    // Draw the item name
    gpu.oledText(10, 50, menuItems[menuIndex], 2, true);  // Scale 2 for visibility
    
    // Draw navigation hint
    gpu.oledText(2, 110, "A=^ C=v B=OK D=<", 1, true);
    
    gpu.oledPresent();
}

//=============================================================================
// Main Entry Point
//=============================================================================
extern "C" void app_main(void) {
    ESP_LOGI(TAG, "\n=== OLED UI Framework Test ===");
    
    // Initialize buttons
    initButtons();
    
    // Initialize GPU - wait for GPU to boot up
    ESP_LOGI(TAG, "Initializing GPU UART...");
    gpu.init(UART_NUM_1, UART_TX_PIN, UART_RX_PIN, UART_BAUD);
    
    // GPU needs time to boot up before accepting commands
    ESP_LOGI(TAG, "Waiting for GPU to boot (2 seconds)...");
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // Clear displays
    gpu.hub75Clear(0, 0, 0);
    gpu.hub75Present();
    gpu.oledClear();
    gpu.oledPresent();
    
    ESP_LOGI(TAG, "UI initialized. Use UP/DOWN buttons to navigate.");
    
    // Initial render
    renderUI();
    
    // Track time for periodic refresh (GPU needs commands every 3s or shows "No Signal")
    uint32_t lastRenderTime = millis();
    static constexpr uint32_t REFRESH_INTERVAL_MS = 1000;  // Refresh every 1 second
    
    // Main loop
    while (true) {
        // Read input
        InputEvent input = readButtons();
        
        bool needsRender = false;
        
        switch (input) {
            case InputEvent::UP:
                if (menuIndex > 0) {
                    menuIndex--;
                    needsRender = true;
                }
                ESP_LOGI(TAG, "UP - index: %d", menuIndex);
                break;
                
            case InputEvent::DOWN:
                if (menuIndex < MENU_ITEMS - 1) {
                    menuIndex++;
                    needsRender = true;
                }
                ESP_LOGI(TAG, "DOWN - index: %d", menuIndex);
                break;
                
            case InputEvent::SELECT:
                ESP_LOGI(TAG, "SELECT - item: %s", menuItems[menuIndex]);
                // Flash visual feedback - invert screen briefly
                gpu.oledClear();
                gpu.oledFill(0, 0, 128, 128, true);  // Fill white
                gpu.oledText(20, 50, "SELECTED!", 2, false);  // Black text on white
                gpu.oledText(10, 80, menuItems[menuIndex], 1, false);
                gpu.oledPresent();
                vTaskDelay(pdMS_TO_TICKS(200));  // Show for 200ms
                needsRender = true;
                break;
                
            case InputEvent::BACK:
                ESP_LOGI(TAG, "BACK - resetting to item 0");
                menuIndex = 0;  // Reset to first item
                // Flash visual feedback
                gpu.oledClear();
                gpu.oledFill(0, 0, 128, 128, true);  // Fill white
                gpu.oledText(30, 60, "BACK!", 2, false);  // Black text
                gpu.oledPresent();
                vTaskDelay(pdMS_TO_TICKS(200));
                needsRender = true;
                break;
                
            default:
                break;
        }
        
        // Periodic refresh to keep GPU alive (prevents "No Signal")
        uint32_t now = millis();
        if (now - lastRenderTime >= REFRESH_INTERVAL_MS) {
            needsRender = true;
        }
        
        if (needsRender) {
            renderUI();
            lastRenderTime = millis();
        }
        
        // Small delay (~60fps)
        vTaskDelay(pdMS_TO_TICKS(16));
    }
}
