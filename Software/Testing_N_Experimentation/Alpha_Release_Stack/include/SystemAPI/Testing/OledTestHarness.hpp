/*****************************************************************
 * @file OledTestHarness.hpp
 * @brief Comprehensive OLED Display Test Suite
 * 
 * This test harness provides automated testing for the 128x128
 * monochrome OLED display including:
 *   - Basic drawing primitives (pixels, lines, shapes)
 *   - Text rendering at various scales
 *   - UI widgets (progress bars, buttons, checkboxes)
 *   - Animation and pattern tests
 *   - Stress tests
 * 
 * Display: SH1107 128x128 monochrome (1-bit, on/off)
 * Interface: CPU → UART → GPU → I2C → OLED
 * 
 * Commands (via Serial):
 *   OLED:HELP           - Show all commands
 *   OLED:FULL           - Run full test suite
 *   OLED:QUICK          - Quick visual demo
 *   OLED:TEXT           - Text rendering tests
 *   OLED:SHAPES         - Shape drawing tests
 *   OLED:WIDGETS        - UI widget tests
 *   OLED:PATTERNS       - Pattern tests
 *   OLED:STRESS         - Stress tests
 *   OLED:CLEAR          - Clear display
 * 
 * @author ARCOS Framework
 * @version 1.0
 *****************************************************************/

#pragma once

#include <cstdint>
#include <cstring>
#include <cmath>
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "GpuDriver/GpuCommands.hpp"
#include "Drivers/OledHandler.hpp"

namespace SystemAPI {
namespace Testing {

static const char* OLED_TEST_TAG = "OLED_TEST";

/**
 * @brief Comprehensive OLED Display Test Harness
 */
class OledTestHarness {
public:
    //=========================================================================
    // Configuration
    //=========================================================================
    
    static constexpr int16_t WIDTH = 128;
    static constexpr int16_t HEIGHT = 128;
    
    //=========================================================================
    // Initialization
    //=========================================================================
    
    OledTestHarness() : initialized_(false), gpu_(nullptr) {}
    
    /**
     * @brief Initialize the test harness
     * @param gpu Pointer to initialized GpuCommands
     * @return true if successful
     */
    bool init(GpuCommands* gpu) {
        if (!gpu || !gpu->isInitialized()) {
            ESP_LOGE(OLED_TEST_TAG, "GpuCommands not initialized!");
            return false;
        }
        
        gpu_ = gpu;
        
        if (!oled_.init(gpu)) {
            ESP_LOGE(OLED_TEST_TAG, "Failed to initialize OledHandler!");
            return false;
        }
        
        initialized_ = true;
        
        ESP_LOGI(OLED_TEST_TAG, "╔════════════════════════════════════════════════════════════╗");
        ESP_LOGI(OLED_TEST_TAG, "║         OLED TEST HARNESS INITIALIZED                     ║");
        ESP_LOGI(OLED_TEST_TAG, "║         Display: 128x128 Monochrome                       ║");
        ESP_LOGI(OLED_TEST_TAG, "╚════════════════════════════════════════════════════════════╝");
        
        return true;
    }
    
    bool isInitialized() const { return initialized_; }
    
    //=========================================================================
    // Command Handler
    //=========================================================================
    
    /**
     * @brief Process OLED test commands
     * @param cmd Command string (e.g., "OLED:TEXT")
     * @return true if command was handled
     */
    bool processCommand(const char* cmd) {
        if (!cmd || strncmp(cmd, "OLED:", 5) != 0) {
            return false;
        }
        
        const char* subCmd = cmd + 5;
        
        if (strcmp(subCmd, "HELP") == 0) {
            printHelp();
        } else if (strcmp(subCmd, "FULL") == 0) {
            runFullTestSuite();
        } else if (strcmp(subCmd, "QUICK") == 0) {
            runQuickDemo();
        } else if (strcmp(subCmd, "TEXT") == 0) {
            runTextTests();
        } else if (strcmp(subCmd, "SHAPES") == 0) {
            runShapeTests();
        } else if (strcmp(subCmd, "WIDGETS") == 0) {
            runWidgetTests();
        } else if (strcmp(subCmd, "PATTERNS") == 0) {
            runPatternTests();
        } else if (strcmp(subCmd, "STRESS") == 0) {
            runStressTests();
        } else if (strcmp(subCmd, "CLEAR") == 0) {
            oled_.clear();
            oled_.present();
            ESP_LOGI(OLED_TEST_TAG, "Display cleared");
        } else {
            ESP_LOGW(OLED_TEST_TAG, "Unknown command: %s", cmd);
            return false;
        }
        
        return true;
    }
    
    //=========================================================================
    // Test Suites
    //=========================================================================
    
    /**
     * @brief Run the complete test suite
     */
    void runFullTestSuite() {
        ESP_LOGI(OLED_TEST_TAG, "╔════════════════════════════════════════════════════════════╗");
        ESP_LOGI(OLED_TEST_TAG, "║         STARTING FULL OLED TEST SUITE                     ║");
        ESP_LOGI(OLED_TEST_TAG, "╚════════════════════════════════════════════════════════════╝");
        
        uint64_t startTime = esp_timer_get_time();
        
        runTextTests();
        vTaskDelay(pdMS_TO_TICKS(1500));
        
        runShapeTests();
        vTaskDelay(pdMS_TO_TICKS(1500));
        
        runWidgetTests();
        vTaskDelay(pdMS_TO_TICKS(1500));
        
        runPatternTests();
        vTaskDelay(pdMS_TO_TICKS(1500));
        
        runStressTests();
        
        uint64_t elapsed = (esp_timer_get_time() - startTime) / 1000;
        
        ESP_LOGI(OLED_TEST_TAG, "╔════════════════════════════════════════════════════════════╗");
        ESP_LOGI(OLED_TEST_TAG, "║         FULL TEST SUITE COMPLETE                          ║");
        ESP_LOGI(OLED_TEST_TAG, "║         Duration: %lu ms                                  ║", (unsigned long)elapsed);
        ESP_LOGI(OLED_TEST_TAG, "╚════════════════════════════════════════════════════════════╝");
    }
    
    /**
     * @brief Run a quick visual demonstration
     */
    void runQuickDemo() {
        if (!initialized_) return;
        
        ESP_LOGI(OLED_TEST_TAG, ">>> Running Quick Demo...");
        
        // Clear
        oled_.clear();
        oled_.present();
        vTaskDelay(pdMS_TO_TICKS(200));
        
        // Welcome text
        oled_.drawTextCentered(10, "OLED TEST", 2);
        oled_.drawTextCentered(35, "128x128 Mono", 1);
        oled_.drawLine(10, 50, 118, 50);
        oled_.present();
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        // Shapes demo
        oled_.clear();
        oled_.drawText(2, 2, "Shapes Demo", 1);
        oled_.drawRect(10, 20, 40, 30);
        oled_.fillRect(70, 20, 40, 30);
        oled_.drawCircle(30, 85, 20);
        oled_.fillCircle(95, 85, 20);
        oled_.present();
        vTaskDelay(pdMS_TO_TICKS(1500));
        
        // Animation demo
        oled_.clear();
        oled_.drawText(2, 2, "Animation", 1);
        oled_.present();
        
        for (int i = 0; i < 60; i++) {
            oled_.clear();
            oled_.drawText(2, 2, "Animation", 1);
            
            // Bouncing circle
            int x = 64 + (int)(40 * sin(i * 0.15));
            int y = 70 + (int)(30 * cos(i * 0.2));
            oled_.fillCircle(x, y, 10);
            
            oled_.present();
            vTaskDelay(pdMS_TO_TICKS(30));
        }
        
        // Complete
        oled_.clear();
        oled_.drawTextCentered(55, "Demo Complete!", 1);
        oled_.present();
        
        ESP_LOGI(OLED_TEST_TAG, ">>> Quick Demo Complete");
    }
    
    /**
     * @brief Test text rendering capabilities
     */
    void runTextTests() {
        if (!initialized_) return;
        
        ESP_LOGI(OLED_TEST_TAG, ">>> Running Text Tests...");
        
        // Test 1: Basic text
        oled_.clear();
        oled_.drawText(0, 0, "TEXT RENDERING", 1);
        oled_.drawLine(0, 10, 127, 10);
        oled_.drawText(0, 15, "Scale 1 (5x7)", 1);
        oled_.drawText(0, 30, "Scale 2", 2);
        oled_.drawText(0, 55, "Abc", 3);
        oled_.present();
        vTaskDelay(pdMS_TO_TICKS(1500));
        
        // Test 2: Character set
        oled_.clear();
        oled_.drawText(0, 0, "CHARACTER SET", 1);
        oled_.drawLine(0, 10, 127, 10);
        oled_.drawText(0, 15, "ABCDEFGHIJKLM", 1);
        oled_.drawText(0, 25, "NOPQRSTUVWXYZ", 1);
        oled_.drawText(0, 35, "abcdefghijklm", 1);
        oled_.drawText(0, 45, "nopqrstuvwxyz", 1);
        oled_.drawText(0, 55, "0123456789", 1);
        oled_.drawText(0, 65, "!@#$%^&*()-=+", 1);
        oled_.drawText(0, 75, "[]{}|;:',.<>/?", 1);
        oled_.present();
        vTaskDelay(pdMS_TO_TICKS(2000));
        
        // Test 3: Alignment
        oled_.clear();
        oled_.drawText(0, 0, "TEXT ALIGNMENT", 1);
        oled_.drawLine(0, 10, 127, 10);
        oled_.drawTextAligned(0, 20, "Left Align", Drivers::OledHandler::TextAlign::LEFT);
        oled_.drawTextAligned(64, 35, "Center", Drivers::OledHandler::TextAlign::CENTER);
        oled_.drawTextAligned(127, 50, "Right", Drivers::OledHandler::TextAlign::RIGHT);
        oled_.drawTextCentered(70, "Centered Text", 1);
        oled_.present();
        vTaskDelay(pdMS_TO_TICKS(1500));
        
        // Test 4: Numbers
        oled_.clear();
        oled_.drawText(0, 0, "NUMBERS", 1);
        oled_.drawLine(0, 10, 127, 10);
        oled_.drawText(0, 20, "Integer:", 1);
        oled_.drawInt(60, 20, 12345);
        oled_.drawText(0, 35, "Negative:", 1);
        oled_.drawInt(60, 35, -9876);
        oled_.drawText(0, 50, "Float:", 1);
        oled_.drawFloat(60, 50, 3.14159, 4);
        oled_.present();
        vTaskDelay(pdMS_TO_TICKS(1500));
        
        ESP_LOGI(OLED_TEST_TAG, ">>> Text Tests Complete");
    }
    
    /**
     * @brief Test shape drawing capabilities
     */
    void runShapeTests() {
        if (!initialized_) return;
        
        ESP_LOGI(OLED_TEST_TAG, ">>> Running Shape Tests...");
        
        // Test 1: Lines
        oled_.clear();
        oled_.drawText(0, 0, "LINES", 1);
        oled_.drawLine(0, 10, 127, 10);
        
        // Various line angles
        for (int i = 0; i < 8; i++) {
            int x = 64 + (int)(50 * cos(i * 3.14159 / 4));
            int y = 70 + (int)(40 * sin(i * 3.14159 / 4));
            oled_.drawLine(64, 70, x, y);
        }
        oled_.present();
        vTaskDelay(pdMS_TO_TICKS(1500));
        
        // Test 2: Rectangles
        oled_.clear();
        oled_.drawText(0, 0, "RECTANGLES", 1);
        oled_.drawLine(0, 10, 127, 10);
        
        // Outline rectangles
        oled_.drawRect(5, 20, 55, 30);
        oled_.drawRect(15, 30, 35, 10);
        
        // Filled rectangles
        oled_.fillRect(70, 20, 50, 30);
        oled_.fillRect(75, 25, 40, 20, false);  // Erase inside
        
        // Rounded rectangles
        oled_.drawRoundedRect(5, 60, 55, 30, 8);
        oled_.fillRoundedRect(70, 60, 50, 30, 8);
        
        oled_.present();
        vTaskDelay(pdMS_TO_TICKS(1500));
        
        // Test 3: Circles
        oled_.clear();
        oled_.drawText(0, 0, "CIRCLES", 1);
        oled_.drawLine(0, 10, 127, 10);
        
        // Outline circles
        oled_.drawCircle(30, 50, 25);
        oled_.drawCircle(30, 50, 15);
        oled_.drawCircle(30, 50, 5);
        
        // Filled circles
        oled_.fillCircle(95, 50, 25);
        oled_.fillCircle(95, 50, 15, false);
        oled_.fillCircle(95, 50, 5);
        
        // Small circles
        for (int i = 0; i < 10; i++) {
            oled_.drawCircle(12 + i * 10, 100, 4);
        }
        
        oled_.present();
        vTaskDelay(pdMS_TO_TICKS(1500));
        
        // Test 4: Triangles
        oled_.clear();
        oled_.drawText(0, 0, "TRIANGLES", 1);
        oled_.drawLine(0, 10, 127, 10);
        
        // Outline triangle
        oled_.drawTriangle(10, 110, 55, 25, 60, 110);
        
        // Filled triangle
        oled_.fillTriangle(70, 110, 95, 25, 120, 110);
        
        oled_.present();
        vTaskDelay(pdMS_TO_TICKS(1500));
        
        // Test 5: Mixed shapes
        oled_.clear();
        oled_.drawText(0, 0, "MIXED SHAPES", 1);
        oled_.drawLine(0, 10, 127, 10);
        
        // House
        oled_.drawRect(20, 60, 40, 50);          // Body
        oled_.fillTriangle(20, 60, 40, 30, 60, 60);  // Roof
        oled_.fillRect(35, 85, 15, 25);          // Door
        oled_.fillRect(25, 70, 10, 10);          // Window
        
        // Sun
        oled_.fillCircle(100, 35, 12);
        for (int i = 0; i < 8; i++) {
            int x1 = 100 + (int)(16 * cos(i * 3.14159 / 4));
            int y1 = 35 + (int)(16 * sin(i * 3.14159 / 4));
            int x2 = 100 + (int)(24 * cos(i * 3.14159 / 4));
            int y2 = 35 + (int)(24 * sin(i * 3.14159 / 4));
            oled_.drawLine(x1, y1, x2, y2);
        }
        
        oled_.present();
        vTaskDelay(pdMS_TO_TICKS(2000));
        
        ESP_LOGI(OLED_TEST_TAG, ">>> Shape Tests Complete");
    }
    
    /**
     * @brief Test UI widgets
     */
    void runWidgetTests() {
        if (!initialized_) return;
        
        ESP_LOGI(OLED_TEST_TAG, ">>> Running Widget Tests...");
        
        // Test 1: Progress bars
        oled_.clear();
        oled_.drawText(0, 0, "PROGRESS BARS", 1);
        oled_.drawLine(0, 10, 127, 10);
        
        oled_.drawText(0, 20, "0%");
        oled_.drawProgressBar(30, 18, 90, 10, 0);
        
        oled_.drawText(0, 40, "50%");
        oled_.drawProgressBar(30, 38, 90, 10, 50);
        
        oled_.drawText(0, 60, "100%");
        oled_.drawProgressBar(30, 58, 90, 10, 100);
        
        oled_.present();
        vTaskDelay(pdMS_TO_TICKS(1500));
        
        // Animated progress bar
        oled_.clear();
        oled_.drawText(0, 0, "LOADING...", 1);
        oled_.drawLine(0, 10, 127, 10);
        
        for (int i = 0; i <= 100; i += 5) {
            oled_.fillRect(0, 50, 128, 20, false);  // Clear area
            oled_.drawProgressBar(10, 55, 108, 12, i);
            oled_.fillRect(55, 75, 30, 10, false);
            char buf[8];
            snprintf(buf, sizeof(buf), "%d%%", i);
            oled_.drawTextCentered(80, buf);
            oled_.present();
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        vTaskDelay(pdMS_TO_TICKS(500));
        
        // Test 2: Buttons
        oled_.clear();
        oled_.drawText(0, 0, "BUTTONS", 1);
        oled_.drawLine(0, 10, 127, 10);
        
        oled_.drawButton(10, 25, "OK");
        oled_.drawButton(50, 25, "Cancel");
        oled_.drawButton(10, 45, "Selected", true);
        oled_.drawButton(10, 65, "Apply");
        oled_.drawButton(60, 65, "Reset");
        
        oled_.present();
        vTaskDelay(pdMS_TO_TICKS(1500));
        
        // Test 3: Checkboxes
        oled_.clear();
        oled_.drawText(0, 0, "CHECKBOXES", 1);
        oled_.drawLine(0, 10, 127, 10);
        
        oled_.drawCheckbox(10, 25, true, "Option 1");
        oled_.drawCheckbox(10, 40, false, "Option 2");
        oled_.drawCheckbox(10, 55, true, "Option 3");
        oled_.drawCheckbox(10, 70, false, "Disabled");
        
        oled_.present();
        vTaskDelay(pdMS_TO_TICKS(1500));
        
        // Test 4: Sliders
        oled_.clear();
        oled_.drawText(0, 0, "SLIDERS", 1);
        oled_.drawLine(0, 10, 127, 10);
        
        oled_.drawText(0, 25, "0:");
        oled_.drawSlider(25, 23, 95, 0);
        
        oled_.drawText(0, 45, "50:");
        oled_.drawSlider(25, 43, 95, 50);
        
        oled_.drawText(0, 65, "100:");
        oled_.drawSlider(25, 63, 95, 100);
        
        oled_.present();
        vTaskDelay(pdMS_TO_TICKS(1500));
        
        // Test 5: Frames
        oled_.clear();
        oled_.drawText(0, 0, "FRAMES", 1);
        oled_.drawLine(0, 10, 127, 10);
        
        oled_.drawFrame(5, 25, 55, 45, "Info");
        oled_.drawText(10, 35, "Status:", 1);
        oled_.drawText(10, 50, "OK", 1);
        
        oled_.drawFrame(68, 25, 55, 45, "Data");
        oled_.drawText(73, 35, "Temp:", 1);
        oled_.drawText(73, 50, "25.3C", 1);
        
        oled_.present();
        vTaskDelay(pdMS_TO_TICKS(1500));
        
        ESP_LOGI(OLED_TEST_TAG, ">>> Widget Tests Complete");
    }
    
    /**
     * @brief Test pattern drawing
     */
    void runPatternTests() {
        if (!initialized_) return;
        
        ESP_LOGI(OLED_TEST_TAG, ">>> Running Pattern Tests...");
        
        // Test 1: Checkerboard
        oled_.clear();
        oled_.drawText(0, 0, "CHECKERBOARD", 1);
        oled_.drawLine(0, 10, 127, 10);
        oled_.drawCheckerboard(10, 20, 50, 50, 5);
        oled_.drawCheckerboard(70, 20, 50, 50, 10);
        oled_.present();
        vTaskDelay(pdMS_TO_TICKS(1500));
        
        // Test 2: Crosshatch
        oled_.clear();
        oled_.drawText(0, 0, "CROSSHATCH", 1);
        oled_.drawLine(0, 10, 127, 10);
        oled_.drawCrosshatch(10, 20, 50, 50, 6);
        oled_.drawCrosshatch(70, 20, 50, 50, 12);
        oled_.present();
        vTaskDelay(pdMS_TO_TICKS(1500));
        
        // Test 3: Dashed lines
        oled_.clear();
        oled_.drawText(0, 0, "DASHED LINES", 1);
        oled_.drawLine(0, 10, 127, 10);
        
        oled_.drawDashedLine(10, 30, 117, 30, 4, 2);
        oled_.drawDashedLine(10, 50, 117, 50, 8, 4);
        oled_.drawDashedLine(10, 70, 117, 70, 2, 2);
        oled_.drawDashedLine(64, 85, 10, 120, 4, 2);
        oled_.drawDashedLine(64, 85, 117, 120, 4, 2);
        
        oled_.present();
        vTaskDelay(pdMS_TO_TICKS(1500));
        
        // Test 4: Concentric shapes
        oled_.clear();
        oled_.drawText(0, 0, "CONCENTRIC", 1);
        oled_.drawLine(0, 10, 127, 10);
        
        for (int r = 5; r <= 50; r += 5) {
            oled_.drawCircle(64, 75, r);
        }
        oled_.present();
        vTaskDelay(pdMS_TO_TICKS(1500));
        
        // Test 5: Spiral
        oled_.clear();
        oled_.drawText(0, 0, "SPIRAL", 1);
        oled_.drawLine(0, 10, 127, 10);
        
        float cx = 64, cy = 75;
        float angle = 0;
        float radius = 5;
        float prevX = cx, prevY = cy;
        
        while (radius < 50 && angle < 20) {
            float x = cx + radius * cos(angle);
            float y = cy + radius * sin(angle);
            oled_.drawLine((int16_t)prevX, (int16_t)prevY, (int16_t)x, (int16_t)y);
            prevX = x;
            prevY = y;
            angle += 0.2;
            radius += 0.4;
        }
        oled_.present();
        vTaskDelay(pdMS_TO_TICKS(1500));
        
        ESP_LOGI(OLED_TEST_TAG, ">>> Pattern Tests Complete");
    }
    
    /**
     * @brief Run stress tests
     */
    void runStressTests() {
        if (!initialized_) return;
        
        ESP_LOGI(OLED_TEST_TAG, ">>> Running Stress Tests...");
        
        // Test 1: Rapid drawing
        oled_.clear();
        oled_.drawText(0, 0, "RAPID DRAW", 1);
        oled_.drawLine(0, 10, 127, 10);
        oled_.present();
        
        uint64_t start = esp_timer_get_time();
        int frameCount = 0;
        
        while ((esp_timer_get_time() - start) < 3000000) {  // 3 seconds
            oled_.clear();
            
            // Random-ish lines
            for (int i = 0; i < 10; i++) {
                int x1 = (frameCount * 17 + i * 23) % 128;
                int y1 = 20 + (frameCount * 13 + i * 31) % 100;
                int x2 = (frameCount * 19 + i * 29) % 128;
                int y2 = 20 + (frameCount * 11 + i * 37) % 100;
                oled_.drawLine(x1, y1, x2, y2);
            }
            
            oled_.present();
            frameCount++;
        }
        
        float fps = frameCount / 3.0f;
        ESP_LOGI(OLED_TEST_TAG, "Rapid draw: %d frames in 3s (%.1f FPS)", frameCount, fps);
        
        // Test 2: Fill/clear cycle
        oled_.clear();
        oled_.drawText(0, 0, "FILL TEST", 1);
        oled_.present();
        vTaskDelay(pdMS_TO_TICKS(500));
        
        start = esp_timer_get_time();
        frameCount = 0;
        
        for (int i = 0; i < 30; i++) {
            oled_.fill(i % 2 == 0);
            oled_.present();
            frameCount++;
        }
        
        uint64_t elapsed = (esp_timer_get_time() - start) / 1000;
        fps = frameCount * 1000.0f / elapsed;
        ESP_LOGI(OLED_TEST_TAG, "Fill cycle: %d frames in %lu ms (%.1f FPS)", 
                 frameCount, (unsigned long)elapsed, fps);
        
        // Test 3: Text scrolling
        oled_.clear();
        const char* scrollText = "SCROLLING TEXT DEMO - OLED 128x128 MONOCHROME DISPLAY TEST";
        int textLen = strlen(scrollText);
        int textW = oled_.textWidth(scrollText, 1);
        
        start = esp_timer_get_time();
        frameCount = 0;
        
        for (int offset = 0; offset < textW + 128; offset += 2) {
            oled_.clear();
            oled_.drawText(128 - offset, 60, scrollText);
            oled_.present();
            frameCount++;
            vTaskDelay(pdMS_TO_TICKS(20));
        }
        
        elapsed = (esp_timer_get_time() - start) / 1000;
        fps = frameCount * 1000.0f / elapsed;
        ESP_LOGI(OLED_TEST_TAG, "Text scroll: %d frames in %lu ms (%.1f FPS)", 
                 frameCount, (unsigned long)elapsed, fps);
        
        // Complete
        oled_.clear();
        oled_.drawTextCentered(55, "STRESS TEST", 1);
        oled_.drawTextCentered(70, "COMPLETE", 1);
        oled_.present();
        
        ESP_LOGI(OLED_TEST_TAG, ">>> Stress Tests Complete");
    }
    
    //=========================================================================
    // Auto-Start Test Task
    //=========================================================================
    
    /**
     * @brief Create FreeRTOS task that auto-starts tests
     */
    static void createAutoStartTask(GpuCommands* gpu) {
        struct TaskParams {
            GpuCommands* gpu;
        };
        
        static TaskParams params;
        params.gpu = gpu;
        
        xTaskCreate(
            [](void* arg) {
                TaskParams* p = (TaskParams*)arg;
                
                // Wait for system to settle
                vTaskDelay(pdMS_TO_TICKS(1000));
                
                OledTestHarness harness;
                if (!harness.init(p->gpu)) {
                    ESP_LOGE(OLED_TEST_TAG, "Failed to init harness in task!");
                    vTaskDelete(nullptr);
                    return;
                }
                
                // Run full test suite
                harness.runFullTestSuite();
                
                vTaskDelete(nullptr);
            },
            "oled_test",
            8192,
            &params,
            3,
            nullptr
        );
    }
    
    //=========================================================================
    // Accessors
    //=========================================================================
    
    Drivers::OledHandler* getOled() { return &oled_; }
    GpuCommands* getGpu() { return gpu_; }
    
private:
    bool initialized_;
    GpuCommands* gpu_;
    Drivers::OledHandler oled_;
    
    void printHelp() {
        ESP_LOGI(OLED_TEST_TAG, "╔════════════════════════════════════════════════════════════╗");
        ESP_LOGI(OLED_TEST_TAG, "║         OLED TEST HARNESS COMMANDS                        ║");
        ESP_LOGI(OLED_TEST_TAG, "╠════════════════════════════════════════════════════════════╣");
        ESP_LOGI(OLED_TEST_TAG, "║  OLED:HELP      - Show this help                          ║");
        ESP_LOGI(OLED_TEST_TAG, "║  OLED:FULL      - Run full test suite                     ║");
        ESP_LOGI(OLED_TEST_TAG, "║  OLED:QUICK     - Quick visual demo                       ║");
        ESP_LOGI(OLED_TEST_TAG, "║  OLED:TEXT      - Text rendering tests                    ║");
        ESP_LOGI(OLED_TEST_TAG, "║  OLED:SHAPES    - Shape drawing tests                     ║");
        ESP_LOGI(OLED_TEST_TAG, "║  OLED:WIDGETS   - UI widget tests                         ║");
        ESP_LOGI(OLED_TEST_TAG, "║  OLED:PATTERNS  - Pattern tests                           ║");
        ESP_LOGI(OLED_TEST_TAG, "║  OLED:STRESS    - Stress tests                            ║");
        ESP_LOGI(OLED_TEST_TAG, "║  OLED:CLEAR     - Clear display                           ║");
        ESP_LOGI(OLED_TEST_TAG, "╚════════════════════════════════════════════════════════════╝");
    }
};

} // namespace Testing
} // namespace SystemAPI
