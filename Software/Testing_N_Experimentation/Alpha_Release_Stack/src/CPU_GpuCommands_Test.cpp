/*****************************************************************
 * CPU_GpuCommands_Test.cpp - Test for GpuCommands wrapper
 * 
 * Tests the stable GPU command interface with:
 * - OLED text rendering
 * - OLED primitives (rectangles, circles, lines)
 * - UI widgets (buttons, sliders, progress bars)
 * - HUB75 basic shapes
 *****************************************************************/

#include <cstdio>
#include <cstring>
#include <cmath>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "GpuDriver/GpuCommands.hpp"

static const char* TAG = "GPU_CMD_TEST";

// GPU command interface
static GpuCommands gpu;

// Menu state
static int selectedItem = 0;
static int menuItems = 6;
static int sliderValue = 50;
static bool toggle1 = false;
static bool toggle2 = true;
static int brightness = 75;

// Simulated sensor values
static float temperature = 23.5f;
static float humidity = 45.0f;

static void drawOledUI() {
    gpu.oledClear();
    
    // Title bar
    gpu.oledFill(0, 0, 128, 12, true);
    gpu.oledText(4, 2, "GPU CMD TEST", 1, false);
    
    // Menu items
    const char* items[] = {
        "Brightness",
        "Slider",
        "Toggle 1",
        "Toggle 2",
        "Reset",
        "About"
    };
    
    int yPos = 16;
    for (int i = 0; i < menuItems; i++) {
        bool selected = (i == selectedItem);
        
        if (selected) {
            // Highlighted background
            gpu.oledFill(0, yPos - 1, 128, 11, true);
            gpu.oledText(4, yPos, items[i], 1, false);
            
            // Show value for some items
            char buf[16];
            switch (i) {
                case 0:  // Brightness
                    snprintf(buf, sizeof(buf), "%d%%", brightness);
                    gpu.oledText(80, yPos, buf, 1, false);
                    break;
                case 1:  // Slider
                    snprintf(buf, sizeof(buf), "%d", sliderValue);
                    gpu.oledText(80, yPos, buf, 1, false);
                    break;
                case 2:  // Toggle 1
                    gpu.oledText(80, yPos, toggle1 ? "ON" : "OFF", 1, false);
                    break;
                case 3:  // Toggle 2
                    gpu.oledText(80, yPos, toggle2 ? "ON" : "OFF", 1, false);
                    break;
            }
        } else {
            gpu.oledText(4, yPos, items[i], 1, true);
            
            // Show value
            char buf[16];
            switch (i) {
                case 0:
                    snprintf(buf, sizeof(buf), "%d%%", brightness);
                    gpu.oledText(80, yPos, buf, 1, true);
                    break;
                case 1:
                    snprintf(buf, sizeof(buf), "%d", sliderValue);
                    gpu.oledText(80, yPos, buf, 1, true);
                    break;
                case 2:
                    gpu.oledText(80, yPos, toggle1 ? "ON" : "OFF", 1, true);
                    break;
                case 3:
                    gpu.oledText(80, yPos, toggle2 ? "ON" : "OFF", 1, true);
                    break;
            }
        }
        
        yPos += 12;
    }
    
    // Slider visualization
    gpu.oledRect(4, 90, 120, 10, true);
    int sliderW = (sliderValue * 116) / 100;
    if (sliderW > 0) {
        gpu.oledFill(6, 92, sliderW, 6, true);
    }
    
    // Status bar at bottom
    gpu.oledHLine(0, 105, 128, true);
    
    char status[32];
    snprintf(status, sizeof(status), "T:%.1fC H:%.0f%%", temperature, humidity);
    gpu.oledText(4, 108, status, 1, true);
    
    // Frame counter
    static uint32_t frames = 0;
    char fps[16];
    snprintf(fps, sizeof(fps), "F:%lu", (unsigned long)frames++);
    gpu.oledText(90, 118, fps, 1, true);
    
    gpu.oledPresent();
}

static void drawHUB75Display() {
    gpu.hub75Clear(0, 0, 0);
    
    // Draw some test shapes
    static float angle = 0;
    angle += 0.05f;
    
    // Animated circle on left panel
    int cx = 32 + (int)(10 * sinf(angle));
    int cy = 16 + (int)(5 * cosf(angle));
    gpu.hub75Circle(cx, cy, 8, 255, 0, 128);
    
    // Rectangle on right panel
    int rx = 80 + (int)(10 * sinf(angle * 0.7f));
    gpu.hub75Rect(rx, 8, 16, 16, 0, 255, 255);
    
    // Brightness bar
    int barW = (brightness * 60) / 100;
    gpu.hub75Fill(64 - barW/2, 28, barW, 3, 255, 128, 0);
    
    gpu.hub75Present();
}

static void simulateSensors() {
    // Fake sensor drift
    static float t_offset = 0;
    t_offset += 0.01f;
    temperature = 23.5f + 2.0f * sinf(t_offset);
    humidity = 45.0f + 10.0f * sinf(t_offset * 0.3f);
}

static void simulateInput() {
    // Simulate menu navigation (cycle through items every 2 seconds)
    static int64_t lastChange = 0;
    int64_t now = esp_timer_get_time();
    
    if (now - lastChange > 2000000) {  // 2 seconds
        selectedItem = (selectedItem + 1) % menuItems;
        
        // Animate slider
        sliderValue = (sliderValue + 10) % 101;
        
        // Toggle sometimes
        if (selectedItem == 2) toggle1 = !toggle1;
        if (selectedItem == 3) toggle2 = !toggle2;
        
        // Vary brightness
        brightness = 50 + (rand() % 50);
        
        lastChange = now;
    }
}

extern "C" void app_main() {
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, " GPU Commands Wrapper Test");
    ESP_LOGI(TAG, "========================================");
    
    // Initialize GPU communication
    if (!gpu.init()) {
        ESP_LOGE(TAG, "Failed to initialize GPU communication!");
        return;
    }
    ESP_LOGI(TAG, "GPU communication initialized");
    
    // Wait for GPU to be ready
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // Send ping
    gpu.ping();
    ESP_LOGI(TAG, "Sent PING to GPU");
    
    // Main loop
    ESP_LOGI(TAG, "Starting main loop...");
    
    int64_t lastOledUpdate = 0;
    int64_t lastHub75Update = 0;
    
    while (true) {
        int64_t now = esp_timer_get_time();
        
        // Update OLED at ~15 FPS (slower due to I2C)
        if (now - lastOledUpdate > 66000) {  // ~15 FPS
            simulateSensors();
            simulateInput();
            drawOledUI();
            lastOledUpdate = now;
        }
        
        // Update HUB75 at ~30 FPS
        if (now - lastHub75Update > 33000) {  // ~30 FPS
            drawHUB75Display();
            lastHub75Update = now;
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
