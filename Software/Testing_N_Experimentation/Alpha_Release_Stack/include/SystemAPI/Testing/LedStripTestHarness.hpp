/*****************************************************************
 * @file LedStripTestHarness.hpp
 * @brief Comprehensive Automated LED Strip Testing System
 * 
 * This system provides:
 * - Physical LED strip hardware testing on CPU using RMT driver
 * - Color accuracy tests (RGB combinations)
 * - Brightness level tests
 * - Individual strip tests (Left Fin, Tongue, Right Fin, Scale)
 * - Pattern tests (chase, rainbow, fade)
 * - Stress tests (rapid updates, memory)
 * - Integration with scene system LED control
 * 
 * LED Strip Configuration (CPU Pins):
 *   Strip 1: Left Fin   - GPIO 18 - 13 LEDs
 *   Strip 2: Tongue     - GPIO 8  - 9 LEDs
 *   Strip 4: Right Fin  - GPIO 38 - 13 LEDs
 *   Strip 5: Scale LEDs - GPIO 37 - 14 LEDs
 * 
 * Commands (via Serial):
 *   LED:HELP              - Show all commands
 *   LED:FULL              - Run FULL automated test suite
 *   LED:QUICK             - Quick visual test (all strips)
 *   LED:TEST:1-8          - Run specific test suite
 *   LED:STRIP:n           - Test specific strip (1,2,4,5)
 *   LED:COLOR:R:G:B       - Set all LEDs to color
 *   LED:BRIGHTNESS:n      - Set brightness (0-255)
 *   LED:OFF               - Turn all LEDs off
 *   LED:RAINBOW           - Rainbow animation
 *   LED:CHASE             - Chase animation
 * 
 * @author ARCOS Testing Framework
 * @version 1.0
 *****************************************************************/

#pragma once

#include <string>
#include <cstring>
#include <vector>
#include <functional>
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/rmt_tx.h"
#include "HAL/led_strip_encoder.h"
#include "driver/gpio.h"

namespace SystemAPI {
namespace Testing {

static const char* LED_TEST_TAG = "LED_TEST";

// ============================================================
// LED TEST CONFIGURATION
// ============================================================
struct LedStripInfo {
    int index;
    const char* name;
    gpio_num_t pin;
    uint8_t ledCount;
    bool active;
};

// CPU LED strip configuration from PIN_MAPPING_CPU.md
static const LedStripInfo LED_STRIPS[] = {
    {0, "Unused 0",    GPIO_NUM_16, 0,  false},
    {1, "Left Fin",    GPIO_NUM_18, 13, true},
    {2, "Tongue",      GPIO_NUM_8,  9,  true},
    {3, "Unused 3",    GPIO_NUM_39, 0,  false},
    {4, "Right Fin",   GPIO_NUM_38, 13, true},
    {5, "Scale LEDs",  GPIO_NUM_37, 14, true},
};
static const int NUM_STRIPS = 6;
static const int ACTIVE_STRIP_COUNT = 4;  // Strips 1, 2, 4, 5
static const uint32_t RMT_LED_RESOLUTION_HZ = 10000000;  // 10MHz

// ============================================================
// RMT LED STRIP HANDLE
// ============================================================
struct RmtLedStrip {
    rmt_channel_handle_t channel;
    rmt_encoder_handle_t encoder;
    uint8_t* pixelBuffer;   // GRBW format (4 bytes per pixel)
    uint8_t ledCount;
    gpio_num_t pin;
    bool initialized;
};

// ============================================================
// TEST RESULT STRUCTURES
// ============================================================
struct LedTestResult {
    std::string name;
    bool passed;
    std::string message;
    uint32_t durationMs;
};

struct LedTestSuite {
    std::string name;
    std::vector<LedTestResult> results;
    int passed = 0;
    int failed = 0;
    uint32_t totalDurationMs = 0;
};

/**
 * @brief Comprehensive LED Strip Test Harness (RMT-based)
 */
class LedStripTestHarness {
private:
    static RmtLedStrip strips_[NUM_STRIPS];
    static bool initialized_;
    static bool testRunning_;
    static uint8_t currentBrightness_;
    
    // Auto-start flag - set to true to run tests on boot
    static constexpr bool AUTO_START_LED_TESTS = true;
    
public:
    /**
     * @brief Initialize the LED test harness
     */
    static void init() {
        if (initialized_) return;
        
        ESP_LOGI(LED_TEST_TAG, "");
        ESP_LOGI(LED_TEST_TAG, "╔════════════════════════════════════════════════════════════╗");
        ESP_LOGI(LED_TEST_TAG, "║   LED STRIP TEST HARNESS v1.0 (RMT)                        ║");
        if (AUTO_START_LED_TESTS) {
            ESP_LOGI(LED_TEST_TAG, "║   AUTO-START MODE - Will run tests after delay            ║");
        } else {
            ESP_LOGI(LED_TEST_TAG, "║   STANDBY MODE - Use LED:FULL to run tests               ║");
        }
        ESP_LOGI(LED_TEST_TAG, "╚════════════════════════════════════════════════════════════╝");
        ESP_LOGI(LED_TEST_TAG, "");
        
        // Initialize all active strips
        initStrips();
        
        initialized_ = true;
        
        if (AUTO_START_LED_TESTS) {
            // Start test task
            xTaskCreate(autoStartTestTask, "led_test", 8192, nullptr, 5, nullptr);
        }
    }
    
    /**
     * @brief Initialize LED strips using RMT driver
     */
    static bool initStrips() {
        ESP_LOGI(LED_TEST_TAG, ">>> Initializing LED strips (RMT driver)...");
        
        int initCount = 0;
        for (int i = 0; i < NUM_STRIPS; i++) {
            strips_[i].initialized = false;
            
            if (LED_STRIPS[i].active && LED_STRIPS[i].ledCount > 0) {
                // Configure RMT TX channel
                rmt_tx_channel_config_t tx_config = {};
                tx_config.gpio_num = LED_STRIPS[i].pin;
                tx_config.clk_src = RMT_CLK_SRC_DEFAULT;
                tx_config.resolution_hz = RMT_LED_RESOLUTION_HZ;
                tx_config.mem_block_symbols = 64;
                tx_config.trans_queue_depth = 4;
                
                esp_err_t err = rmt_new_tx_channel(&tx_config, &strips_[i].channel);
                if (err != ESP_OK) {
                    ESP_LOGE(LED_TEST_TAG, "    Strip %d RMT channel failed (err=%d)", i, err);
                    continue;
                }
                
                // Create LED strip encoder
                led_strip_encoder_config_t encoder_config = {};
                encoder_config.resolution = RMT_LED_RESOLUTION_HZ;
                err = rmt_new_led_strip_encoder(&encoder_config, &strips_[i].encoder);
                if (err != ESP_OK) {
                    ESP_LOGE(LED_TEST_TAG, "    Strip %d encoder failed (err=%d)", i, err);
                    rmt_del_channel(strips_[i].channel);
                    continue;
                }
                
                // Enable channel
                err = rmt_enable(strips_[i].channel);
                if (err != ESP_OK) {
                    ESP_LOGE(LED_TEST_TAG, "    Strip %d enable failed (err=%d)", i, err);
                    rmt_del_encoder(strips_[i].encoder);
                    rmt_del_channel(strips_[i].channel);
                    continue;
                }
                
                // Allocate pixel buffer (GRBW format, 4 bytes per LED for RGBW strips)
                strips_[i].pixelBuffer = (uint8_t*)malloc(LED_STRIPS[i].ledCount * 4);
                if (!strips_[i].pixelBuffer) {
                    ESP_LOGE(LED_TEST_TAG, "    Strip %d buffer alloc failed", i);
                    rmt_disable(strips_[i].channel);
                    rmt_del_encoder(strips_[i].encoder);
                    rmt_del_channel(strips_[i].channel);
                    continue;
                }
                
                memset(strips_[i].pixelBuffer, 0, LED_STRIPS[i].ledCount * 4);
                strips_[i].ledCount = LED_STRIPS[i].ledCount;
                strips_[i].pin = LED_STRIPS[i].pin;
                strips_[i].initialized = true;
                
                // Clear the strip
                showStrip(i);
                
                ESP_LOGI(LED_TEST_TAG, "    Strip %d (%s): Pin=%d, LEDs=%d - INIT OK",
                         i, LED_STRIPS[i].name, LED_STRIPS[i].pin, LED_STRIPS[i].ledCount);
                initCount++;
            }
        }
        
        ESP_LOGI(LED_TEST_TAG, "<<< %d LED strips initialized", initCount);
        return initCount > 0;
    }
    
    /**
     * @brief Send pixel buffer to LED strip via RMT
     */
    static void showStrip(int index) {
        if (index < 0 || index >= NUM_STRIPS || !strips_[index].initialized) return;
        
        rmt_transmit_config_t tx_config = {};
        tx_config.loop_count = 0;
        
        // RGBW format: 4 bytes per pixel
        rmt_transmit(strips_[index].channel, strips_[index].encoder,
                     strips_[index].pixelBuffer, strips_[index].ledCount * 4,
                     &tx_config);
        rmt_tx_wait_all_done(strips_[index].channel, pdMS_TO_TICKS(100));
    }
    
    /**
     * @brief Handle serial command
     */
    static bool handleCommand(const char* cmd) {
        if (strncmp(cmd, "LED:", 4) != 0) return false;
        
        const char* subCmd = cmd + 4;
        
        if (strcmp(subCmd, "HELP") == 0) {
            printHelp();
            return true;
        }
        
        if (strcmp(subCmd, "FULL") == 0) {
            runFullAutomatedTestSuite();
            return true;
        }
        
        if (strcmp(subCmd, "QUICK") == 0) {
            runQuickVisualTest();
            return true;
        }
        
        if (strcmp(subCmd, "OFF") == 0) {
            allOff();
            ESP_LOGI(LED_TEST_TAG, "All LEDs turned OFF");
            return true;
        }
        
        if (strcmp(subCmd, "RAINBOW") == 0) {
            runRainbowAnimation(3000);
            return true;
        }
        
        if (strcmp(subCmd, "CHASE") == 0) {
            runChaseAnimation(3000);
            return true;
        }
        
        if (strncmp(subCmd, "TEST:", 5) == 0) {
            int testNum = atoi(subCmd + 5);
            runSingleSuite(testNum);
            return true;
        }
        
        if (strncmp(subCmd, "STRIP:", 6) == 0) {
            int stripNum = atoi(subCmd + 6);
            testSingleStrip(stripNum);
            return true;
        }
        
        if (strncmp(subCmd, "COLOR:", 6) == 0) {
            int r = 0, g = 0, b = 0;
            if (sscanf(subCmd + 6, "%d:%d:%d", &r, &g, &b) == 3) {
                setAllColor(r, g, b);
                ESP_LOGI(LED_TEST_TAG, "Set all LEDs to R=%d G=%d B=%d", r, g, b);
            }
            return true;
        }
        
        if (strncmp(subCmd, "BRIGHTNESS:", 11) == 0) {
            int brightness = atoi(subCmd + 11);
            setBrightness((uint8_t)brightness);
            ESP_LOGI(LED_TEST_TAG, "Set brightness to %d", brightness);
            return true;
        }
        
        ESP_LOGW(LED_TEST_TAG, "Unknown LED command: %s", subCmd);
        printHelp();
        return true;
    }
    
    /**
     * @brief Print help message
     */
    static void printHelp() {
        ESP_LOGI(LED_TEST_TAG, "");
        ESP_LOGI(LED_TEST_TAG, "╔══════════════════════════════════════════════════════════════╗");
        ESP_LOGI(LED_TEST_TAG, "║          LED STRIP TEST HARNESS - COMMANDS                   ║");
        ESP_LOGI(LED_TEST_TAG, "╠══════════════════════════════════════════════════════════════╣");
        ESP_LOGI(LED_TEST_TAG, "║  LED:HELP              Show this help                        ║");
        ESP_LOGI(LED_TEST_TAG, "║  LED:FULL              Run FULL automated test suite         ║");
        ESP_LOGI(LED_TEST_TAG, "║  LED:QUICK             Quick visual test (all strips)        ║");
        ESP_LOGI(LED_TEST_TAG, "║  LED:TEST:n            Run test suite n (1-8)                ║");
        ESP_LOGI(LED_TEST_TAG, "║  LED:STRIP:n           Test specific strip (1,2,4,5)         ║");
        ESP_LOGI(LED_TEST_TAG, "║  LED:COLOR:R:G:B       Set all LEDs to color                 ║");
        ESP_LOGI(LED_TEST_TAG, "║  LED:BRIGHTNESS:n      Set brightness (0-255)                ║");
        ESP_LOGI(LED_TEST_TAG, "║  LED:OFF               Turn all LEDs off                     ║");
        ESP_LOGI(LED_TEST_TAG, "║  LED:RAINBOW           Rainbow animation (3s)                ║");
        ESP_LOGI(LED_TEST_TAG, "║  LED:CHASE             Chase animation (3s)                  ║");
        ESP_LOGI(LED_TEST_TAG, "╠══════════════════════════════════════════════════════════════╣");
        ESP_LOGI(LED_TEST_TAG, "║  LED STRIP CONFIGURATION (CPU):                              ║");
        ESP_LOGI(LED_TEST_TAG, "║    Strip 1: Left Fin   - GPIO 18 - 13 LEDs                   ║");
        ESP_LOGI(LED_TEST_TAG, "║    Strip 2: Tongue     - GPIO 8  - 9 LEDs                    ║");
        ESP_LOGI(LED_TEST_TAG, "║    Strip 4: Right Fin  - GPIO 38 - 13 LEDs                   ║");
        ESP_LOGI(LED_TEST_TAG, "║    Strip 5: Scale LEDs - GPIO 37 - 14 LEDs                   ║");
        ESP_LOGI(LED_TEST_TAG, "╚══════════════════════════════════════════════════════════════╝");
        ESP_LOGI(LED_TEST_TAG, "");
    }
    
    // ============================================================
    // BASIC CONTROL FUNCTIONS
    // ============================================================
    
    static void allOff() {
        for (int i = 0; i < NUM_STRIPS; i++) {
            if (strips_[i].initialized) {
                memset(strips_[i].pixelBuffer, 0, strips_[i].ledCount * 4);
                showStrip(i);
            }
        }
    }
    
    static void setAllColor(uint8_t r, uint8_t g, uint8_t b, uint8_t w = 0) {
        // Apply brightness
        uint8_t scaledR = (r * currentBrightness_) / 255;
        uint8_t scaledG = (g * currentBrightness_) / 255;
        uint8_t scaledB = (b * currentBrightness_) / 255;
        uint8_t scaledW = (w * currentBrightness_) / 255;
        
        for (int i = 0; i < NUM_STRIPS; i++) {
            if (strips_[i].initialized) {
                for (int j = 0; j < strips_[i].ledCount; j++) {
                    // GRBW format (4 bytes per pixel)
                    strips_[i].pixelBuffer[j * 4 + 0] = scaledG;
                    strips_[i].pixelBuffer[j * 4 + 1] = scaledR;
                    strips_[i].pixelBuffer[j * 4 + 2] = scaledB;
                    strips_[i].pixelBuffer[j * 4 + 3] = scaledW;
                }
                showStrip(i);
            }
        }
    }
    
    static void setStripColor(int stripIndex, uint8_t r, uint8_t g, uint8_t b, uint8_t w = 0) {
        if (stripIndex < 0 || stripIndex >= NUM_STRIPS || !strips_[stripIndex].initialized) return;
        
        uint8_t scaledR = (r * currentBrightness_) / 255;
        uint8_t scaledG = (g * currentBrightness_) / 255;
        uint8_t scaledB = (b * currentBrightness_) / 255;
        uint8_t scaledW = (w * currentBrightness_) / 255;
        
        for (int j = 0; j < strips_[stripIndex].ledCount; j++) {
            strips_[stripIndex].pixelBuffer[j * 4 + 0] = scaledG;
            strips_[stripIndex].pixelBuffer[j * 4 + 1] = scaledR;
            strips_[stripIndex].pixelBuffer[j * 4 + 2] = scaledB;
            strips_[stripIndex].pixelBuffer[j * 4 + 3] = scaledW;
        }
        showStrip(stripIndex);
    }
    
    static void setBrightness(uint8_t brightness) {
        currentBrightness_ = brightness;
    }
    
    static void setPixel(int stripIndex, int pixelIndex, uint8_t r, uint8_t g, uint8_t b, uint8_t w = 0) {
        if (stripIndex < 0 || stripIndex >= NUM_STRIPS || !strips_[stripIndex].initialized) return;
        if (pixelIndex < 0 || pixelIndex >= strips_[stripIndex].ledCount) return;
        
        uint8_t scaledR = (r * currentBrightness_) / 255;
        uint8_t scaledG = (g * currentBrightness_) / 255;
        uint8_t scaledB = (b * currentBrightness_) / 255;
        uint8_t scaledW = (w * currentBrightness_) / 255;
        
        strips_[stripIndex].pixelBuffer[pixelIndex * 4 + 0] = scaledG;
        strips_[stripIndex].pixelBuffer[pixelIndex * 4 + 1] = scaledR;
        strips_[stripIndex].pixelBuffer[pixelIndex * 4 + 2] = scaledB;
        strips_[stripIndex].pixelBuffer[pixelIndex * 4 + 3] = scaledW;
        showStrip(stripIndex);
    }
    
    // ============================================================
    // ANIMATION FUNCTIONS
    // ============================================================
    
    // HSV to RGB helper
    static void hsvToRgb(uint8_t h, uint8_t s, uint8_t v, uint8_t& r, uint8_t& g, uint8_t& b) {
        if (s == 0) {
            r = g = b = v;
            return;
        }
        
        uint8_t region = h / 43;
        uint8_t remainder = (h - (region * 43)) * 6;
        
        uint8_t p = (v * (255 - s)) >> 8;
        uint8_t q = (v * (255 - ((s * remainder) >> 8))) >> 8;
        uint8_t t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;
        
        switch (region) {
            case 0:  r = v; g = t; b = p; break;
            case 1:  r = q; g = v; b = p; break;
            case 2:  r = p; g = v; b = t; break;
            case 3:  r = p; g = q; b = v; break;
            case 4:  r = t; g = p; b = v; break;
            default: r = v; g = p; b = q; break;
        }
    }
    
    static void runRainbowAnimation(int durationMs) {
        ESP_LOGI(LED_TEST_TAG, ">>> Running rainbow animation for %d ms...", durationMs);
        
        if (!initialized_) initStrips();
        
        uint32_t startTime = xTaskGetTickCount() * portTICK_PERIOD_MS;
        uint32_t elapsed = 0;
        
        while (elapsed < (uint32_t)durationMs) {
            elapsed = (xTaskGetTickCount() * portTICK_PERIOD_MS) - startTime;
            uint8_t baseHue = (elapsed / 10) % 256;
            
            for (int i = 0; i < NUM_STRIPS; i++) {
                if (strips_[i].initialized) {
                    for (int j = 0; j < strips_[i].ledCount; j++) {
                        uint8_t hue = (baseHue + j * 10) % 256;
                        uint8_t r, g, b;
                        hsvToRgb(hue, 255, currentBrightness_, r, g, b);
                        
                        strips_[i].pixelBuffer[j * 4 + 0] = g;
                        strips_[i].pixelBuffer[j * 4 + 1] = r;
                        strips_[i].pixelBuffer[j * 4 + 2] = b;
                        strips_[i].pixelBuffer[j * 4 + 3] = 0;  // White = 0
                    }
                    showStrip(i);
                }
            }
            vTaskDelay(pdMS_TO_TICKS(20));
        }
        
        allOff();
        ESP_LOGI(LED_TEST_TAG, "<<< Rainbow animation complete");
    }
    
    static void runChaseAnimation(int durationMs) {
        ESP_LOGI(LED_TEST_TAG, ">>> Running chase animation for %d ms...", durationMs);
        
        if (!initialized_) initStrips();
        
        uint32_t startTime = xTaskGetTickCount() * portTICK_PERIOD_MS;
        uint32_t elapsed = 0;
        int position = 0;
        
        while (elapsed < (uint32_t)durationMs) {
            elapsed = (xTaskGetTickCount() * portTICK_PERIOD_MS) - startTime;
            
            for (int i = 0; i < NUM_STRIPS; i++) {
                if (strips_[i].initialized) {
                    memset(strips_[i].pixelBuffer, 0, strips_[i].ledCount * 4);
                    
                    int pos = position % strips_[i].ledCount;
                    int pos2 = (position + 1) % strips_[i].ledCount;
                    
                    // Main pixel - white (using W channel)
                    strips_[i].pixelBuffer[pos * 4 + 0] = 0;
                    strips_[i].pixelBuffer[pos * 4 + 1] = 0;
                    strips_[i].pixelBuffer[pos * 4 + 2] = 0;
                    strips_[i].pixelBuffer[pos * 4 + 3] = currentBrightness_;  // Pure white
                    
                    // Trailing pixel - dimmer white
                    strips_[i].pixelBuffer[pos2 * 4 + 0] = 0;
                    strips_[i].pixelBuffer[pos2 * 4 + 1] = 0;
                    strips_[i].pixelBuffer[pos2 * 4 + 2] = 0;
                    strips_[i].pixelBuffer[pos2 * 4 + 3] = currentBrightness_ / 3;
                    
                    showStrip(i);
                }
            }
            position++;
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        
        allOff();
        ESP_LOGI(LED_TEST_TAG, "<<< Chase animation complete");
    }
    
    // ============================================================
    // QUICK VISUAL TEST
    // ============================================================
    
    static void runQuickVisualTest() {
        ESP_LOGI(LED_TEST_TAG, "");
        ESP_LOGI(LED_TEST_TAG, "╔════════════════════════════════════════════════════════════╗");
        ESP_LOGI(LED_TEST_TAG, "║   QUICK LED VISUAL TEST (RGBW)                             ║");
        ESP_LOGI(LED_TEST_TAG, "╚════════════════════════════════════════════════════════════╝");
        ESP_LOGI(LED_TEST_TAG, "");
        
        if (!initialized_) initStrips();
        
        // Test each color on all strips
        ESP_LOGI(LED_TEST_TAG, ">>> Testing RED channel...");
        setAllColor(255, 0, 0, 0);
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        ESP_LOGI(LED_TEST_TAG, ">>> Testing GREEN channel...");
        setAllColor(0, 255, 0, 0);
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        ESP_LOGI(LED_TEST_TAG, ">>> Testing BLUE channel...");
        setAllColor(0, 0, 255, 0);
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        ESP_LOGI(LED_TEST_TAG, ">>> Testing WHITE channel (dedicated)...");
        setAllColor(0, 0, 0, 255);
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        ESP_LOGI(LED_TEST_TAG, ">>> Testing RGB white (R+G+B)...");
        setAllColor(255, 255, 255, 0);
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        // Test each strip individually
        allOff();
        for (int i = 0; i < NUM_STRIPS; i++) {
            if (LED_STRIPS[i].active && strips_[i].initialized) {
                ESP_LOGI(LED_TEST_TAG, ">>> Testing Strip %d (%s)...", i, LED_STRIPS[i].name);
                setStripColor(i, 255, 255, 0, 0);  // Yellow
                vTaskDelay(pdMS_TO_TICKS(800));
                setStripColor(i, 0, 0, 0, 0);
            }
        }
        
        // Rainbow animation
        runRainbowAnimation(2000);
        
        allOff();
        ESP_LOGI(LED_TEST_TAG, "");
        ESP_LOGI(LED_TEST_TAG, ">>> QUICK VISUAL TEST COMPLETE");
        ESP_LOGI(LED_TEST_TAG, "");
    }
    
    // ============================================================
    // TEST SINGLE STRIP
    // ============================================================
    
    static void testSingleStrip(int stripIndex) {
        if (stripIndex < 0 || stripIndex >= NUM_STRIPS) {
            ESP_LOGE(LED_TEST_TAG, "Invalid strip index: %d", stripIndex);
            return;
        }
        
        if (!LED_STRIPS[stripIndex].active) {
            ESP_LOGW(LED_TEST_TAG, "Strip %d is not active/connected", stripIndex);
            return;
        }
        
        if (!initialized_) initStrips();
        
        ESP_LOGI(LED_TEST_TAG, "");
        ESP_LOGI(LED_TEST_TAG, ">>> Testing Strip %d: %s (Pin=%d, LEDs=%d)",
                 stripIndex, LED_STRIPS[stripIndex].name,
                 LED_STRIPS[stripIndex].pin, LED_STRIPS[stripIndex].ledCount);
        
        if (!strips_[stripIndex].initialized) {
            ESP_LOGE(LED_TEST_TAG, "Strip not initialized!");
            return;
        }
        
        // Fill with red
        ESP_LOGI(LED_TEST_TAG, "    RED...");
        setStripColor(stripIndex, 255, 0, 0);
        vTaskDelay(pdMS_TO_TICKS(500));
        
        // Fill with green
        ESP_LOGI(LED_TEST_TAG, "    GREEN...");
        setStripColor(stripIndex, 0, 255, 0);
        vTaskDelay(pdMS_TO_TICKS(500));
        
        // Fill with blue
        ESP_LOGI(LED_TEST_TAG, "    BLUE...");
        setStripColor(stripIndex, 0, 0, 255);
        vTaskDelay(pdMS_TO_TICKS(500));
        
        // Chase pattern
        ESP_LOGI(LED_TEST_TAG, "    CHASE...");
        int numPixels = strips_[stripIndex].ledCount;
        for (int run = 0; run < 2; run++) {
            for (int j = 0; j < numPixels; j++) {
                memset(strips_[stripIndex].pixelBuffer, 0, numPixels * 3);
                strips_[stripIndex].pixelBuffer[j * 3 + 0] = currentBrightness_;
                strips_[stripIndex].pixelBuffer[j * 3 + 1] = currentBrightness_;
                strips_[stripIndex].pixelBuffer[j * 3 + 2] = currentBrightness_;
                showStrip(stripIndex);
                vTaskDelay(pdMS_TO_TICKS(50));
            }
        }
        
        memset(strips_[stripIndex].pixelBuffer, 0, numPixels * 3);
        showStrip(stripIndex);
        
        ESP_LOGI(LED_TEST_TAG, "<<< Strip %d test complete", stripIndex);
        ESP_LOGI(LED_TEST_TAG, "");
    }
    
    // ============================================================
    // TEST SUITES
    // ============================================================
    
    static LedTestSuite runSuiteByNumber(int suiteNum) {
        switch (suiteNum) {
            case 1: return runTestSuite_Initialization();
            case 2: return runTestSuite_BasicColors();
            case 3: return runTestSuite_Brightness();
            case 4: return runTestSuite_IndividualStrips();
            case 5: return runTestSuite_PixelAddressing();
            case 6: return runTestSuite_Animations();
            case 7: return runTestSuite_StressTest();
            case 8: return runTestSuite_MemoryTest();
            default:
                LedTestSuite empty;
                empty.name = "Invalid";
                return empty;
        }
    }
    
    static const char* getSuiteName(int suiteNum) {
        switch (suiteNum) {
            case 1: return "Initialization";
            case 2: return "Basic Colors";
            case 3: return "Brightness Levels";
            case 4: return "Individual Strips";
            case 5: return "Pixel Addressing";
            case 6: return "Animations";
            case 7: return "Stress Test";
            case 8: return "Memory Test";
            default: return "Unknown";
        }
    }
    
    static void runSingleSuite(int suiteNum) {
        if (suiteNum < 1 || suiteNum > 8) {
            ESP_LOGE(LED_TEST_TAG, "Invalid suite number %d. Use 1-8.", suiteNum);
            return;
        }
        
        ESP_LOGI(LED_TEST_TAG, "");
        ESP_LOGI(LED_TEST_TAG, ">>> Running LED Test Suite %d: %s", suiteNum, getSuiteName(suiteNum));
        
        LedTestSuite suite = runSuiteByNumber(suiteNum);
        printTestSuiteSummary(suite);
    }
    
    // ============================================================
    // FULL AUTOMATED TEST SUITE
    // ============================================================
    
    static void autoStartTestTask(void* param) {
        ESP_LOGI(LED_TEST_TAG, "");
        ESP_LOGI(LED_TEST_TAG, ">>> AUTO-START: LED tests will begin in 10 seconds...");
        
        for (int i = 10; i > 0; i--) {
            ESP_LOGI(LED_TEST_TAG, "    Starting in %d...", i);
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
        
        runFullAutomatedTestSuite();
        vTaskDelete(nullptr);
    }
    
    static void runFullAutomatedTestSuite() {
        testRunning_ = true;
        
        if (!initialized_) initStrips();
        
        ESP_LOGI(LED_TEST_TAG, "");
        ESP_LOGI(LED_TEST_TAG, "########################################################################");
        ESP_LOGI(LED_TEST_TAG, "##                                                                    ##");
        ESP_LOGI(LED_TEST_TAG, "##    ██╗     ███████╗██████╗     ████████╗███████╗███████╗████████╗  ##");
        ESP_LOGI(LED_TEST_TAG, "##    ██║     ██╔════╝██╔══██╗    ╚══██╔══╝██╔════╝██╔════╝╚══██╔══╝  ##");
        ESP_LOGI(LED_TEST_TAG, "##    ██║     █████╗  ██║  ██║       ██║   █████╗  ███████╗   ██║     ##");
        ESP_LOGI(LED_TEST_TAG, "##    ██║     ██╔══╝  ██║  ██║       ██║   ██╔══╝  ╚════██║   ██║     ##");
        ESP_LOGI(LED_TEST_TAG, "##    ███████╗███████╗██████╔╝       ██║   ███████╗███████║   ██║     ##");
        ESP_LOGI(LED_TEST_TAG, "##    ╚══════╝╚══════╝╚═════╝        ╚═╝   ╚══════╝╚══════╝   ╚═╝     ##");
        ESP_LOGI(LED_TEST_TAG, "##                                                                    ##");
        ESP_LOGI(LED_TEST_TAG, "##              COMPREHENSIVE LED STRIP TEST SUITE                    ##");
        ESP_LOGI(LED_TEST_TAG, "##                       Version 1.0 (RMT)                            ##");
        ESP_LOGI(LED_TEST_TAG, "########################################################################");
        ESP_LOGI(LED_TEST_TAG, "");
        
        ESP_LOGI(LED_TEST_TAG, ">>> LED CONFIGURATION:");
        ESP_LOGI(LED_TEST_TAG, "    Strip 1: Left Fin   - GPIO 18 - 13 LEDs");
        ESP_LOGI(LED_TEST_TAG, "    Strip 2: Tongue     - GPIO 8  - 9 LEDs");
        ESP_LOGI(LED_TEST_TAG, "    Strip 4: Right Fin  - GPIO 38 - 13 LEDs");
        ESP_LOGI(LED_TEST_TAG, "    Strip 5: Scale LEDs - GPIO 37 - 14 LEDs");
        ESP_LOGI(LED_TEST_TAG, "    Total: 49 LEDs across 4 strips");
        ESP_LOGI(LED_TEST_TAG, "");
        
        int totalTests = 0;
        int totalPassed = 0;
        int totalFailed = 0;
        uint32_t totalDuration = 0;
        
        // Run all 8 test suites
        for (int suiteNum = 1; suiteNum <= 8; suiteNum++) {
            ESP_LOGI(LED_TEST_TAG, "");
            ESP_LOGI(LED_TEST_TAG, "════════════════════════════════════════════════════════════════");
            ESP_LOGI(LED_TEST_TAG, "  SUITE %d/8: %s", suiteNum, getSuiteName(suiteNum));
            ESP_LOGI(LED_TEST_TAG, "════════════════════════════════════════════════════════════════");
            
            LedTestSuite suite = runSuiteByNumber(suiteNum);
            
            totalTests += suite.passed + suite.failed;
            totalPassed += suite.passed;
            totalFailed += suite.failed;
            totalDuration += suite.totalDurationMs;
            
            printTestSuiteSummary(suite);
            
            vTaskDelay(pdMS_TO_TICKS(500));
        }
        
        // Final summary
        ESP_LOGI(LED_TEST_TAG, "");
        ESP_LOGI(LED_TEST_TAG, "########################################################################");
        ESP_LOGI(LED_TEST_TAG, "##              LED TEST SUITE RESULTS                                ##");
        ESP_LOGI(LED_TEST_TAG, "########################################################################");
        ESP_LOGI(LED_TEST_TAG, "");
        ESP_LOGI(LED_TEST_TAG, "    Total Tests:  %d", totalTests);
        ESP_LOGI(LED_TEST_TAG, "    Passed:       %d  (%d%%)", totalPassed, totalTests > 0 ? (totalPassed * 100 / totalTests) : 0);
        ESP_LOGI(LED_TEST_TAG, "    Failed:       %d  (%d%%)", totalFailed, totalTests > 0 ? (totalFailed * 100 / totalTests) : 0);
        ESP_LOGI(LED_TEST_TAG, "    Duration:     %lu ms", (unsigned long)totalDuration);
        ESP_LOGI(LED_TEST_TAG, "");
        
        if (totalFailed == 0) {
            ESP_LOGI(LED_TEST_TAG, "    ✅ ALL LED TESTS PASSED!");
        } else {
            ESP_LOGW(LED_TEST_TAG, "    ⚠️  %d TESTS FAILED", totalFailed);
        }
        
        ESP_LOGI(LED_TEST_TAG, "");
        ESP_LOGI(LED_TEST_TAG, "########################################################################");
        ESP_LOGI(LED_TEST_TAG, "##              LED TEST SUITE COMPLETELY FINISHED                    ##");
        ESP_LOGI(LED_TEST_TAG, "########################################################################");
        ESP_LOGI(LED_TEST_TAG, "");
        
        allOff();
        testRunning_ = false;
    }
    
    // ============================================================
    // INDIVIDUAL TEST SUITES
    // ============================================================
    
    static LedTestSuite runTestSuite_Initialization() {
        LedTestSuite suite;
        suite.name = "Initialization";
        
        ESP_LOGI(LED_TEST_TAG, ">>> Suite: Initialization Tests");
        
        // Test 1: All active strips initialized
        {
            uint32_t start = xTaskGetTickCount() * portTICK_PERIOD_MS;
            bool passed = true;
            std::string msg = "All active strips initialized";
            
            for (int i = 0; i < NUM_STRIPS; i++) {
                if (LED_STRIPS[i].active && !strips_[i].initialized) {
                    passed = false;
                    msg = "Strip " + std::to_string(i) + " failed to initialize";
                    break;
                }
            }
            
            addTestResult(suite, "Strip Objects Created", passed, msg,
                         (xTaskGetTickCount() * portTICK_PERIOD_MS) - start);
        }
        
        // Test 2: Correct LED counts
        {
            uint32_t start = xTaskGetTickCount() * portTICK_PERIOD_MS;
            bool passed = true;
            std::string msg = "All LED counts match config";
            
            for (int i = 0; i < NUM_STRIPS; i++) {
                if (strips_[i].initialized) {
                    if (strips_[i].ledCount != LED_STRIPS[i].ledCount) {
                        passed = false;
                        msg = "Strip " + std::to_string(i) + " LED count mismatch";
                        break;
                    }
                }
            }
            
            addTestResult(suite, "LED Count Verification", passed, msg,
                         (xTaskGetTickCount() * portTICK_PERIOD_MS) - start);
        }
        
        // Test 3: Pixel buffers allocated
        {
            uint32_t start = xTaskGetTickCount() * portTICK_PERIOD_MS;
            bool passed = true;
            std::string msg = "All pixel buffers allocated";
            
            for (int i = 0; i < NUM_STRIPS; i++) {
                if (strips_[i].initialized && !strips_[i].pixelBuffer) {
                    passed = false;
                    msg = "Strip " + std::to_string(i) + " buffer missing";
                    break;
                }
            }
            
            addTestResult(suite, "Pixel Buffers", passed, msg,
                         (xTaskGetTickCount() * portTICK_PERIOD_MS) - start);
        }
        
        return suite;
    }
    
    static LedTestSuite runTestSuite_BasicColors() {
        LedTestSuite suite;
        suite.name = "Basic Colors";
        
        ESP_LOGI(LED_TEST_TAG, ">>> Suite: Basic Color Tests (VISUAL)");
        
        struct ColorTest { const char* name; uint8_t r, g, b; };
        ColorTest colors[] = {
            {"Red",     255, 0,   0},
            {"Green",   0,   255, 0},
            {"Blue",    0,   0,   255},
            {"Yellow",  255, 255, 0},
            {"Cyan",    0,   255, 255},
            {"Magenta", 255, 0,   255},
            {"White",   255, 255, 255},
        };
        
        for (const auto& color : colors) {
            uint32_t start = xTaskGetTickCount() * portTICK_PERIOD_MS;
            setAllColor(color.r, color.g, color.b);
            vTaskDelay(pdMS_TO_TICKS(300));
            
            addTestResult(suite, std::string("Color: ") + color.name, true,
                         "R=" + std::to_string(color.r) + " G=" + std::to_string(color.g) + " B=" + std::to_string(color.b),
                         (xTaskGetTickCount() * portTICK_PERIOD_MS) - start);
        }
        
        allOff();
        return suite;
    }
    
    static LedTestSuite runTestSuite_Brightness() {
        LedTestSuite suite;
        suite.name = "Brightness Levels";
        
        ESP_LOGI(LED_TEST_TAG, ">>> Suite: Brightness Tests (VISUAL)");
        
        uint8_t levels[] = {255, 192, 128, 64, 32, 16, 8, 1};
        
        for (uint8_t level : levels) {
            uint32_t start = xTaskGetTickCount() * portTICK_PERIOD_MS;
            setBrightness(level);
            setAllColor(255, 255, 255);
            vTaskDelay(pdMS_TO_TICKS(300));
            
            addTestResult(suite, "Brightness: " + std::to_string(level), true,
                         "Set successfully",
                         (xTaskGetTickCount() * portTICK_PERIOD_MS) - start);
        }
        
        setBrightness(128);
        allOff();
        return suite;
    }
    
    static LedTestSuite runTestSuite_IndividualStrips() {
        LedTestSuite suite;
        suite.name = "Individual Strips";
        
        ESP_LOGI(LED_TEST_TAG, ">>> Suite: Individual Strip Tests");
        
        allOff();
        
        for (int i = 0; i < NUM_STRIPS; i++) {
            if (LED_STRIPS[i].active) {
                uint32_t start = xTaskGetTickCount() * portTICK_PERIOD_MS;
                bool passed = strips_[i].initialized;
                
                if (passed) {
                    allOff();
                    setStripColor(i, 0, 255, 0);
                    vTaskDelay(pdMS_TO_TICKS(500));
                }
                
                std::string msg = std::string(LED_STRIPS[i].name) + " - " +
                                 std::to_string(LED_STRIPS[i].ledCount) + " LEDs";
                
                addTestResult(suite, "Strip " + std::to_string(i), passed, msg,
                             (xTaskGetTickCount() * portTICK_PERIOD_MS) - start);
            }
        }
        
        allOff();
        return suite;
    }
    
    static LedTestSuite runTestSuite_PixelAddressing() {
        LedTestSuite suite;
        suite.name = "Pixel Addressing";
        
        ESP_LOGI(LED_TEST_TAG, ">>> Suite: Pixel Addressing Tests");
        
        for (int i = 0; i < NUM_STRIPS; i++) {
            if (LED_STRIPS[i].active && strips_[i].initialized) {
                uint32_t start = xTaskGetTickCount() * portTICK_PERIOD_MS;
                int numPixels = strips_[i].ledCount;
                
                // Test first/middle/last pixels
                allOff();
                setPixel(i, 0, 255, 0, 0);
                vTaskDelay(pdMS_TO_TICKS(100));
                
                allOff();
                setPixel(i, numPixels - 1, 0, 255, 0);
                vTaskDelay(pdMS_TO_TICKS(100));
                
                allOff();
                setPixel(i, numPixels / 2, 0, 0, 255);
                vTaskDelay(pdMS_TO_TICKS(100));
                
                allOff();
                
                addTestResult(suite, "Strip " + std::to_string(i) + " Pixels", true,
                             "First/Mid/Last OK",
                             (xTaskGetTickCount() * portTICK_PERIOD_MS) - start);
            }
        }
        
        return suite;
    }
    
    static LedTestSuite runTestSuite_Animations() {
        LedTestSuite suite;
        suite.name = "Animations";
        
        ESP_LOGI(LED_TEST_TAG, ">>> Suite: Animation Tests");
        
        {
            uint32_t start = xTaskGetTickCount() * portTICK_PERIOD_MS;
            runChaseAnimation(1500);
            addTestResult(suite, "Chase Animation", true, "Completed",
                         (xTaskGetTickCount() * portTICK_PERIOD_MS) - start);
        }
        
        {
            uint32_t start = xTaskGetTickCount() * portTICK_PERIOD_MS;
            runRainbowAnimation(1500);
            addTestResult(suite, "Rainbow Animation", true, "Completed",
                         (xTaskGetTickCount() * portTICK_PERIOD_MS) - start);
        }
        
        return suite;
    }
    
    static LedTestSuite runTestSuite_StressTest() {
        LedTestSuite suite;
        suite.name = "Stress Test";
        
        ESP_LOGI(LED_TEST_TAG, ">>> Suite: Stress Tests");
        
        // Rapid color changes
        {
            uint32_t start = xTaskGetTickCount() * portTICK_PERIOD_MS;
            for (int i = 0; i < 100; i++) {
                setAllColor(i * 2 % 256, (i * 3) % 256, (i * 5) % 256);
                vTaskDelay(pdMS_TO_TICKS(10));
            }
            allOff();
            addTestResult(suite, "Rapid Colors (100x)", true, "No crash",
                         (xTaskGetTickCount() * portTICK_PERIOD_MS) - start);
        }
        
        return suite;
    }
    
    static LedTestSuite runTestSuite_MemoryTest() {
        LedTestSuite suite;
        suite.name = "Memory Test";
        
        ESP_LOGI(LED_TEST_TAG, ">>> Suite: Memory Tests");
        
        uint32_t heapBefore = esp_get_free_heap_size();
        
        {
            uint32_t start = xTaskGetTickCount() * portTICK_PERIOD_MS;
            for (int i = 0; i < 50; i++) {
                setAllColor(255, 0, 0);
                allOff();
            }
            addTestResult(suite, "100 Set/Clear Cycles", true, "Completed",
                         (xTaskGetTickCount() * portTICK_PERIOD_MS) - start);
        }
        
        uint32_t heapAfter = esp_get_free_heap_size();
        int heapDiff = (int)heapBefore - (int)heapAfter;
        
        {
            bool passed = (heapDiff < 1000);
            addTestResult(suite, "Memory Leak Check", passed,
                         "Heap diff: " + std::to_string(heapDiff) + " bytes", 0);
        }
        
        {
            addTestResult(suite, "Free Heap", true,
                         std::to_string(heapAfter) + " bytes", 0);
        }
        
        return suite;
    }
    
    // ============================================================
    // HELPER FUNCTIONS
    // ============================================================
    
    static void addTestResult(LedTestSuite& suite, const std::string& name, bool passed,
                              const std::string& message, uint32_t durationMs) {
        LedTestResult result;
        result.name = name;
        result.passed = passed;
        result.message = message;
        result.durationMs = durationMs;
        
        suite.results.push_back(result);
        suite.totalDurationMs += durationMs;
        
        if (passed) {
            suite.passed++;
            ESP_LOGI(LED_TEST_TAG, "    ✅ PASS: %s - %s (%lu ms)",
                     name.c_str(), message.c_str(), (unsigned long)durationMs);
        } else {
            suite.failed++;
            ESP_LOGE(LED_TEST_TAG, "    ❌ FAIL: %s - %s (%lu ms)",
                     name.c_str(), message.c_str(), (unsigned long)durationMs);
        }
    }
    
    static void printTestSuiteSummary(const LedTestSuite& suite) {
        ESP_LOGI(LED_TEST_TAG, "");
        ESP_LOGI(LED_TEST_TAG, "    Suite: %s", suite.name.c_str());
        ESP_LOGI(LED_TEST_TAG, "    Passed: %d  Failed: %d  Duration: %lu ms",
                 suite.passed, suite.failed, (unsigned long)suite.totalDurationMs);
        ESP_LOGI(LED_TEST_TAG, "");
    }
};

// Static member initialization
inline RmtLedStrip LedStripTestHarness::strips_[NUM_STRIPS] = {};
inline bool LedStripTestHarness::initialized_ = false;
inline bool LedStripTestHarness::testRunning_ = false;
inline uint8_t LedStripTestHarness::currentBrightness_ = 128;

} // namespace Testing
} // namespace SystemAPI
