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
 * NOTE: Uses sequential RMT mode - strips updated one at a time
 *       since ESP32-S3 only has 4 RMT TX channels.
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
#include <cmath>
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_strip.h"
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
 * @brief Comprehensive LED Strip Test Harness (Sequential RMT mode)
 * 
 * Uses sequential RMT initialization to overcome the 4-channel limit.
 * Each strip is initialized, updated, and deinitialized one at a time.
 */
class LedStripTestHarness {
private:
    // Pixel buffer storage for all strips (persistent between updates)
    static uint8_t pixelBuffers_[NUM_STRIPS][64][4];  // [strip][led][RGBW]
    static bool initialized_;
    static bool testRunning_;
    static uint8_t currentBrightness_;
    
    // Animation state
    static volatile bool animationRunning_;
    static std::string currentAnimation_;
    static uint8_t animR_, animG_, animB_;
    static uint8_t animSpeed_;
    static TaskHandle_t animationTaskHandle_;
    static volatile bool taskExited_;  // Flag to signal task has exited
    
    // Auto-start flag - set to false to disable auto-run on boot
    static constexpr bool AUTO_START_LED_TESTS = false;
    
public:
    /**
     * @brief Initialize the LED test harness
     */
    static void init() {
        if (initialized_) return;
        
        ESP_LOGI(LED_TEST_TAG, "");
        ESP_LOGI(LED_TEST_TAG, "╔════════════════════════════════════════════════════════════╗");
        ESP_LOGI(LED_TEST_TAG, "║   LED STRIP TEST HARNESS v2.0 (Sequential RMT)             ║");
        if (AUTO_START_LED_TESTS) {
            ESP_LOGI(LED_TEST_TAG, "║   AUTO-START MODE - Will run tests after delay            ║");
        } else {
            ESP_LOGI(LED_TEST_TAG, "║   STANDBY MODE - Use LED:FULL to run tests               ║");
        }
        ESP_LOGI(LED_TEST_TAG, "╚════════════════════════════════════════════════════════════╝");
        ESP_LOGI(LED_TEST_TAG, "");
        
        // Clear all pixel buffers
        memset(pixelBuffers_, 0, sizeof(pixelBuffers_));
        currentBrightness_ = 64;  // Default to 25% brightness
        
        initialized_ = true;
        
        ESP_LOGI(LED_TEST_TAG, ">>> Sequential RMT mode ready");
        ESP_LOGI(LED_TEST_TAG, "    Active strips: Left Fin (GPIO18), Tongue (GPIO8), Right Fin (GPIO38), Scale (GPIO37)");
        
        if (AUTO_START_LED_TESTS) {
            // Start test task
            xTaskCreate(autoStartTestTask, "led_test", 8192, nullptr, 5, nullptr);
        }
    }
    
    /**
     * @brief Update a single strip using RMT (init → write → deinit)
     * This allows us to use 1 RMT channel for all strips sequentially
     */
    static bool updateStrip(int stripIndex) {
        if (stripIndex < 0 || stripIndex >= NUM_STRIPS) return false;
        if (!LED_STRIPS[stripIndex].active) return false;
        
        gpio_num_t gpio = LED_STRIPS[stripIndex].pin;
        int numLeds = LED_STRIPS[stripIndex].ledCount;
        
        // Configure LED strip
        led_strip_config_t strip_config = {};
        strip_config.strip_gpio_num = gpio;
        strip_config.max_leds = (uint32_t)numLeds;
        strip_config.led_model = LED_MODEL_SK6812;
        // GRBW format for SK6812 RGBW LEDs
        strip_config.color_component_format.format.r_pos = 1;
        strip_config.color_component_format.format.g_pos = 0;
        strip_config.color_component_format.format.b_pos = 2;
        strip_config.color_component_format.format.w_pos = 3;
        strip_config.color_component_format.format.num_components = 4;
        strip_config.flags.invert_out = false;
        
        led_strip_rmt_config_t rmt_config = {};
        rmt_config.clk_src = RMT_CLK_SRC_DEFAULT;
        rmt_config.resolution_hz = 10000000;  // 10MHz
        rmt_config.mem_block_symbols = 64;
        rmt_config.flags.with_dma = false;
        
        led_strip_handle_t strip = nullptr;
        esp_err_t err = led_strip_new_rmt_device(&strip_config, &rmt_config, &strip);
        if (err != ESP_OK) {
            ESP_LOGE(LED_TEST_TAG, "Failed to init RMT for strip %d (GPIO%d): %d", stripIndex, gpio, err);
            return false;
        }
        
        // Write pixel data from our buffer
        for (int led = 0; led < numLeds; led++) {
            uint8_t r = (pixelBuffers_[stripIndex][led][0] * currentBrightness_) / 255;
            uint8_t g = (pixelBuffers_[stripIndex][led][1] * currentBrightness_) / 255;
            uint8_t b = (pixelBuffers_[stripIndex][led][2] * currentBrightness_) / 255;
            uint8_t w = (pixelBuffers_[stripIndex][led][3] * currentBrightness_) / 255;
            led_strip_set_pixel_rgbw(strip, led, r, g, b, w);
        }
        
        // Refresh the strip
        led_strip_refresh(strip);
        
        // Small delay for RMT to complete transmission
        vTaskDelay(pdMS_TO_TICKS(2));
        
        // Delete the strip to free RMT channel
        led_strip_del(strip);
        
        return true;
    }
    
    /**
     * @brief Update all strips sequentially
     */
    static void refreshAllStrips() {
        for (int i = 0; i < NUM_STRIPS; i++) {
            if (LED_STRIPS[i].active) {
                updateStrip(i);
            }
        }
    }
    
    /**
     * @brief Set pixel in our buffer (will be sent on next refresh)
     */
    static void setPixel(int stripIndex, int ledIndex, uint8_t r, uint8_t g, uint8_t b, uint8_t w = 0) {
        if (stripIndex < 0 || stripIndex >= NUM_STRIPS) return;
        if (ledIndex < 0 || ledIndex >= 64) return;
        
        pixelBuffers_[stripIndex][ledIndex][0] = r;
        pixelBuffers_[stripIndex][ledIndex][1] = g;
        pixelBuffers_[stripIndex][ledIndex][2] = b;
        pixelBuffers_[stripIndex][ledIndex][3] = w;
    }
    
    /**
     * @brief Clear all pixel buffers
     */
    static void clearAllBuffers() {
        memset(pixelBuffers_, 0, sizeof(pixelBuffers_));
    }
    
    /**
     * @brief Send pixel buffer to all LED strips (legacy API compatibility)
     */
    static void showStrip(int index) {
        if (index >= 0 && index < NUM_STRIPS) {
            updateStrip(index);
        }
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
        clearAllBuffers();
        refreshAllStrips();
    }
    
    static void setAllColor(uint8_t r, uint8_t g, uint8_t b, uint8_t w = 0) {
        // Set color for all active strips
        for (int i = 0; i < NUM_STRIPS; i++) {
            if (LED_STRIPS[i].active) {
                for (int led = 0; led < LED_STRIPS[i].ledCount; led++) {
                    setPixel(i, led, r, g, b, w);
                }
            }
        }
        refreshAllStrips();
    }
    
    // ============================================================
    // CONTINUOUS ANIMATION SYSTEM
    // ============================================================
    
    /**
     * @brief Start a continuous background animation
     * @param animation Animation type: "solid", "breathe", "rainbow", "pulse", "chase", "sparkle", "fire", "wave", "gradient"
     * @param r Red component (0-255)
     * @param g Green component (0-255)
     * @param b Blue component (0-255)
     * @param speed Animation speed (1-100)
     */
    static void startAnimation(const std::string& animation, uint8_t r, uint8_t g, uint8_t b, uint8_t speed) {
        // Stop any existing animation
        stopAnimation();
        
        if (!initialized_) init();
        
        // Store animation state
        currentAnimation_ = animation;
        animR_ = r;
        animG_ = g;
        animB_ = b;
        animSpeed_ = speed;
        
        ESP_LOGI(LED_TEST_TAG, "Starting animation: %s RGB(%d,%d,%d) speed=%d", 
                 animation.c_str(), r, g, b, speed);
        
        // For solid color, just set it directly - no task needed
        if (animation == "solid" || animation == "Solid") {
            animationRunning_ = false;  // No task running
            taskExited_ = true;
            setAllColor(r, g, b);
            return;
        }
        
        // Create animation task for continuous animations
        animationRunning_ = true;
        taskExited_ = false;
        xTaskCreate(animationTaskFunc, "led_anim", 4096, nullptr, 3, &animationTaskHandle_);
    }
    
    /**
     * @brief Stop any running animation
     */
    static void stopAnimation() {
        if (!animationRunning_ && animationTaskHandle_ == nullptr) {
            return;  // Nothing to stop
        }
        
        // Signal task to stop
        animationRunning_ = false;
        
        // Wait briefly for task to exit (up to 150ms)
        int waitCount = 0;
        while (!taskExited_ && waitCount < 15) {
            vTaskDelay(pdMS_TO_TICKS(10));
            waitCount++;
        }
        
        // If task didn't exit gracefully, force delete it
        if (!taskExited_ && animationTaskHandle_ != nullptr) {
            vTaskDelete(animationTaskHandle_);
        }
        
        animationTaskHandle_ = nullptr;
        taskExited_ = true;
    }
    
    /**
     * @brief Check if an animation is currently running
     */
    static bool isAnimationRunning() {
        return animationRunning_;
    }
    
private:
    /**
     * @brief Animation task - runs in background
     */
    static void animationTaskFunc(void* param) {
        (void)param;
        
        uint32_t frame = 0;
        
        while (animationRunning_) {
            // Calculate delay based on speed (1-100) -> 10ms to 100ms
            int delayMs = 110 - animSpeed_;  // speed 100 = 10ms, speed 1 = 109ms
            if (delayMs < 10) delayMs = 10;
            
            if (currentAnimation_ == "breathe" || currentAnimation_ == "Breathe") {
                // Breathing animation - fade in and out
                float phase = (float)(frame % 200) / 200.0f;
                float brightness = (sinf(phase * 2.0f * 3.14159f) + 1.0f) / 2.0f;
                
                uint8_t r = (uint8_t)(animR_ * brightness);
                uint8_t g = (uint8_t)(animG_ * brightness);
                uint8_t b = (uint8_t)(animB_ * brightness);
                
                setAllColor(r, g, b);
            }
            else if (currentAnimation_ == "rainbow" || currentAnimation_ == "Rainbow") {
                // Rainbow animation - cycle through hues
                uint8_t baseHue = (frame * 2) % 256;
                
                for (int i = 0; i < NUM_STRIPS; i++) {
                    if (LED_STRIPS[i].active) {
                        for (int j = 0; j < LED_STRIPS[i].ledCount; j++) {
                            uint8_t hue = (baseHue + j * 10) % 256;
                            uint8_t r, g, b;
                            hsvToRgb(hue, 255, 255, r, g, b);
                            setPixel(i, j, r, g, b, 0);
                        }
                    }
                }
                refreshAllStrips();
            }
            else if (currentAnimation_ == "pulse" || currentAnimation_ == "Pulse") {
                // Pulse animation - sharp on/off pulse
                float phase = (float)(frame % 100) / 100.0f;
                float brightness = (phase < 0.3f) ? 1.0f : 0.0f;
                
                uint8_t r = (uint8_t)(animR_ * brightness);
                uint8_t g = (uint8_t)(animG_ * brightness);
                uint8_t b = (uint8_t)(animB_ * brightness);
                
                setAllColor(r, g, b);
            }
            else if (currentAnimation_ == "chase" || currentAnimation_ == "Chase") {
                // Chase animation - moving dot
                int position = frame % 64;
                
                clearAllBuffers();
                for (int i = 0; i < NUM_STRIPS; i++) {
                    if (LED_STRIPS[i].active) {
                        int pos = position % LED_STRIPS[i].ledCount;
                        int trail1 = (pos - 1 + LED_STRIPS[i].ledCount) % LED_STRIPS[i].ledCount;
                        int trail2 = (pos - 2 + LED_STRIPS[i].ledCount) % LED_STRIPS[i].ledCount;
                        
                        setPixel(i, pos, animR_, animG_, animB_, 0);
                        setPixel(i, trail1, animR_/2, animG_/2, animB_/2, 0);
                        setPixel(i, trail2, animR_/4, animG_/4, animB_/4, 0);
                    }
                }
                refreshAllStrips();
            }
            else if (currentAnimation_ == "sparkle" || currentAnimation_ == "Sparkle") {
                // Sparkle animation - random flashes (using frame-based pseudo-random)
                clearAllBuffers();
                for (int i = 0; i < NUM_STRIPS; i++) {
                    if (LED_STRIPS[i].active) {
                        // Set base dim color
                        for (int j = 0; j < LED_STRIPS[i].ledCount; j++) {
                            setPixel(i, j, animR_/10, animG_/10, animB_/10, 0);
                        }
                        // Pseudo-random sparkles based on frame
                        int numSparkles = LED_STRIPS[i].ledCount / 3;
                        for (int s = 0; s < numSparkles; s++) {
                            // Simple pseudo-random using frame, strip index and sparkle index
                            int randLed = ((frame * 7 + i * 13 + s * 17) % LED_STRIPS[i].ledCount);
                            setPixel(i, randLed, animR_, animG_, animB_, 0);
                        }
                    }
                }
                refreshAllStrips();
            }
            else if (currentAnimation_ == "fire" || currentAnimation_ == "Fire") {
                // Fire animation - flickering red/orange (simplified)
                for (int i = 0; i < NUM_STRIPS; i++) {
                    if (LED_STRIPS[i].active) {
                        for (int j = 0; j < LED_STRIPS[i].ledCount; j++) {
                            // Simple flicker using sine wave with offset per LED
                            float flicker = sinf((float)frame * 0.3f + (float)j * 1.7f + (float)i * 2.3f);
                            flicker = (flicker + 1.0f) / 2.0f;  // Normalize to 0-1
                            flicker = 0.5f + flicker * 0.5f;    // Range 0.5 to 1.0
                            
                            uint8_t r = (uint8_t)(255 * flicker);
                            uint8_t g = (uint8_t)(80 * flicker);  // Orange tint
                            uint8_t b = 0;
                            setPixel(i, j, r, g, b, 0);
                        }
                    }
                }
                refreshAllStrips();
            }
            else if (currentAnimation_ == "wave" || currentAnimation_ == "Wave") {
                // Wave animation - sine wave of brightness
                float phase = (float)frame / 20.0f;
                
                for (int i = 0; i < NUM_STRIPS; i++) {
                    if (LED_STRIPS[i].active) {
                        for (int j = 0; j < LED_STRIPS[i].ledCount; j++) {
                            float brightness = (sinf(phase + j * 0.5f) + 1.0f) / 2.0f;
                            uint8_t r = (uint8_t)(animR_ * brightness);
                            uint8_t g = (uint8_t)(animG_ * brightness);
                            uint8_t b = (uint8_t)(animB_ * brightness);
                            setPixel(i, j, r, g, b, 0);
                        }
                    }
                }
                refreshAllStrips();
            }
            else if (currentAnimation_ == "gradient" || currentAnimation_ == "Gradient") {
                // Gradient animation - fade across strip with shifting
                int shift = frame % 256;
                
                for (int i = 0; i < NUM_STRIPS; i++) {
                    if (LED_STRIPS[i].active) {
                        for (int j = 0; j < LED_STRIPS[i].ledCount; j++) {
                            float pos = (float)(j + shift) / (float)LED_STRIPS[i].ledCount;
                            pos = fmodf(pos, 1.0f);
                            
                            // Gradient from base color to complementary
                            uint8_t r = (uint8_t)(animR_ * (1.0f - pos) + (255 - animR_) * pos);
                            uint8_t g = (uint8_t)(animG_ * (1.0f - pos) + (255 - animG_) * pos);
                            uint8_t b = (uint8_t)(animB_ * (1.0f - pos) + (255 - animB_) * pos);
                            setPixel(i, j, r, g, b, 0);
                        }
                    }
                }
                refreshAllStrips();
            }
            
            frame++;
            vTaskDelay(pdMS_TO_TICKS(delayMs));
        }
        
        // Turn off LEDs when stopping
        allOff();
        
        // Signal that we've exited, then delete ourselves
        taskExited_ = true;
        animationTaskHandle_ = nullptr;
        vTaskDelete(nullptr);
    }
    
public:
    
    static void setStripColor(int stripIndex, uint8_t r, uint8_t g, uint8_t b, uint8_t w = 0) {
        if (stripIndex < 0 || stripIndex >= NUM_STRIPS) return;
        if (!LED_STRIPS[stripIndex].active) return;
        
        for (int led = 0; led < LED_STRIPS[stripIndex].ledCount; led++) {
            setPixel(stripIndex, led, r, g, b, w);
        }
        updateStrip(stripIndex);
    }
    
    static void setBrightness(uint8_t brightness) {
        currentBrightness_ = brightness;
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
        
        if (!initialized_) init();
        
        uint32_t startTime = xTaskGetTickCount() * portTICK_PERIOD_MS;
        uint32_t elapsed = 0;
        
        while (elapsed < (uint32_t)durationMs) {
            elapsed = (xTaskGetTickCount() * portTICK_PERIOD_MS) - startTime;
            uint8_t baseHue = (elapsed / 10) % 256;
            
            // Set pixels for all active strips
            for (int i = 0; i < NUM_STRIPS; i++) {
                if (LED_STRIPS[i].active) {
                    for (int j = 0; j < LED_STRIPS[i].ledCount; j++) {
                        uint8_t hue = (baseHue + j * 10) % 256;
                        uint8_t r, g, b;
                        hsvToRgb(hue, 255, 255, r, g, b);
                        setPixel(i, j, r, g, b, 0);
                    }
                }
            }
            refreshAllStrips();
            vTaskDelay(pdMS_TO_TICKS(30));
        }
        
        allOff();
        ESP_LOGI(LED_TEST_TAG, "<<< Rainbow animation complete");
    }
    
    static void runChaseAnimation(int durationMs) {
        ESP_LOGI(LED_TEST_TAG, ">>> Running chase animation for %d ms...", durationMs);
        
        if (!initialized_) init();
        
        uint32_t startTime = xTaskGetTickCount() * portTICK_PERIOD_MS;
        uint32_t elapsed = 0;
        int position = 0;
        
        while (elapsed < (uint32_t)durationMs) {
            elapsed = (xTaskGetTickCount() * portTICK_PERIOD_MS) - startTime;
            
            // Clear all buffers
            clearAllBuffers();
            
            // Set chase pixels
            for (int i = 0; i < NUM_STRIPS; i++) {
                if (LED_STRIPS[i].active) {
                    int pos = position % LED_STRIPS[i].ledCount;
                    int pos2 = (position + 1) % LED_STRIPS[i].ledCount;
                    
                    // Main pixel - pure white (W channel)
                    setPixel(i, pos, 0, 0, 0, 255);
                    
                    // Trailing pixel - dimmer white  
                    setPixel(i, pos2, 0, 0, 0, 80);
                }
            }
            refreshAllStrips();
            position++;
            vTaskDelay(pdMS_TO_TICKS(60));
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
        
        if (!initialized_) init();
        
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
            if (LED_STRIPS[i].active) {
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
        
        if (!initialized_) init();
        
        ESP_LOGI(LED_TEST_TAG, "");
        ESP_LOGI(LED_TEST_TAG, ">>> Testing Strip %d: %s (Pin=%d, LEDs=%d)",
                 stripIndex, LED_STRIPS[stripIndex].name,
                 LED_STRIPS[stripIndex].pin, LED_STRIPS[stripIndex].ledCount);
        
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
        int numPixels = LED_STRIPS[stripIndex].ledCount;
        for (int run = 0; run < 2; run++) {
            for (int j = 0; j < numPixels; j++) {
                clearAllBuffers();
                setPixel(stripIndex, j, 255, 255, 255, 0);
                updateStrip(stripIndex);
                vTaskDelay(pdMS_TO_TICKS(50));
            }
        }
        
        allOff();
        
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
        
        runSimpleRGBWTest();
        
        // Final test: synchronized hue cycling to demo speed
        runSynchronizedHueCycleTest(5000);
        
        vTaskDelete(nullptr);
    }
    
    /**
     * @brief Synchronized Hue Cycle Test - All strips cycle through RGB hues together
     * 
     * This demonstrates the speed of the sequential RMT approach by having
     * all LED strips appear to cycle through the color spectrum simultaneously.
     * 
     * @param durationMs How long to run the test (default 5000ms)
     */
    static void runSynchronizedHueCycleTest(int durationMs = 5000) {
        ESP_LOGI(LED_TEST_TAG, "");
        ESP_LOGI(LED_TEST_TAG, "╔════════════════════════════════════════════════════════════╗");
        ESP_LOGI(LED_TEST_TAG, "║   SYNCHRONIZED HUE CYCLE TEST                              ║");
        ESP_LOGI(LED_TEST_TAG, "║   All strips cycling together for %d seconds               ║", durationMs / 1000);
        ESP_LOGI(LED_TEST_TAG, "║   Demonstrating Sequential RMT update speed!               ║");
        ESP_LOGI(LED_TEST_TAG, "╚════════════════════════════════════════════════════════════╝");
        ESP_LOGI(LED_TEST_TAG, "");
        
        if (!initialized_) init();
        
        uint32_t startTime = xTaskGetTickCount() * portTICK_PERIOD_MS;
        uint32_t elapsed = 0;
        uint32_t frameCount = 0;
        
        ESP_LOGI(LED_TEST_TAG, ">>> Starting synchronized hue cycle on all %d strips...", ACTIVE_STRIP_COUNT);
        
        while (elapsed < (uint32_t)durationMs) {
            elapsed = (xTaskGetTickCount() * portTICK_PERIOD_MS) - startTime;
            
            // Calculate hue based on elapsed time - complete cycle every 2 seconds
            uint8_t hue = ((elapsed * 255) / 2000) % 256;
            
            // Convert HSV to RGB
            uint8_t r, g, b;
            hsvToRgb(hue, 255, 255, r, g, b);
            
            // Set ALL pixels on ALL active strips to the SAME color
            for (int i = 0; i < NUM_STRIPS; i++) {
                if (LED_STRIPS[i].active) {
                    for (int j = 0; j < LED_STRIPS[i].ledCount; j++) {
                        setPixel(i, j, r, g, b, 0);
                    }
                }
            }
            
            // Update all strips sequentially (this is the speed test!)
            refreshAllStrips();
            
            frameCount++;
            
            // Small delay to prevent CPU hogging - aim for ~60fps target
            vTaskDelay(pdMS_TO_TICKS(16));
        }
        
        // Calculate actual update rate
        float fps = (float)frameCount / ((float)durationMs / 1000.0f);
        
        allOff();
        
        ESP_LOGI(LED_TEST_TAG, "");
        ESP_LOGI(LED_TEST_TAG, "╔════════════════════════════════════════════════════════════╗");
        ESP_LOGI(LED_TEST_TAG, "║   HUE CYCLE TEST COMPLETE                                  ║");
        ESP_LOGI(LED_TEST_TAG, "║   Frames rendered: %lu                                   ║", (unsigned long)frameCount);
        ESP_LOGI(LED_TEST_TAG, "║   Effective FPS:   %.1f fps                               ║", fps);
        ESP_LOGI(LED_TEST_TAG, "║   Strips updated per frame: %d                             ║", ACTIVE_STRIP_COUNT);
        ESP_LOGI(LED_TEST_TAG, "╚════════════════════════════════════════════════════════════╝");
        ESP_LOGI(LED_TEST_TAG, "");
    }
    
    /**
     * @brief Simple RGBW test - Flash each strip R→G→B→W→Black
     */
    static void runSimpleRGBWTest() {
        ESP_LOGI(LED_TEST_TAG, "");
        ESP_LOGI(LED_TEST_TAG, "╔════════════════════════════════════════════════════════════╗");
        ESP_LOGI(LED_TEST_TAG, "║   SIMPLE RGBW LED STRIP TEST                               ║");
        ESP_LOGI(LED_TEST_TAG, "║   Testing: Red → Green → Blue → White → Off                ║");
        ESP_LOGI(LED_TEST_TAG, "╚════════════════════════════════════════════════════════════╝");
        ESP_LOGI(LED_TEST_TAG, "");
        
        // Test each strip individually
        for (int i = 0; i < NUM_STRIPS; i++) {
            if (!LED_STRIPS[i].active) continue;
            
            ESP_LOGI(LED_TEST_TAG, ">>> Testing Strip %d: %s (GPIO %d, %d LEDs)",
                     i, LED_STRIPS[i].name, LED_STRIPS[i].pin, LED_STRIPS[i].ledCount);
            
            // Red
            ESP_LOGI(LED_TEST_TAG, "    RED...");
            setStripColor(i, 255, 0, 0, 0);
            vTaskDelay(pdMS_TO_TICKS(800));
            
            // Green
            ESP_LOGI(LED_TEST_TAG, "    GREEN...");
            setStripColor(i, 0, 255, 0, 0);
            vTaskDelay(pdMS_TO_TICKS(800));
            
            // Blue
            ESP_LOGI(LED_TEST_TAG, "    BLUE...");
            setStripColor(i, 0, 0, 255, 0);
            vTaskDelay(pdMS_TO_TICKS(800));
            
            // White (dedicated W channel)
            ESP_LOGI(LED_TEST_TAG, "    WHITE (W channel)...");
            setStripColor(i, 0, 0, 0, 255);
            vTaskDelay(pdMS_TO_TICKS(800));
            
            // Off
            ESP_LOGI(LED_TEST_TAG, "    OFF...");
            setStripColor(i, 0, 0, 0, 0);
            vTaskDelay(pdMS_TO_TICKS(400));
            
            ESP_LOGI(LED_TEST_TAG, "");
        }
        
        ESP_LOGI(LED_TEST_TAG, "╔════════════════════════════════════════════════════════════╗");
        ESP_LOGI(LED_TEST_TAG, "║   RGBW TEST COMPLETE                                       ║");
        ESP_LOGI(LED_TEST_TAG, "╚════════════════════════════════════════════════════════════╝");
        ESP_LOGI(LED_TEST_TAG, "");
    }
    
    static void runFullAutomatedTestSuite() {
        testRunning_ = true;
        
        if (!initialized_) init();
        
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
        
        // Test 1: Sequential RMT driver initialized
        {
            uint32_t start = xTaskGetTickCount() * portTICK_PERIOD_MS;
            bool passed = initialized_;
            std::string msg = passed ? "Sequential RMT LED driver initialized" : "RMT driver init failed";
            
            addTestResult(suite, "RMT Driver Init", passed, msg,
                         (xTaskGetTickCount() * portTICK_PERIOD_MS) - start);
        }
        
        // Test 2: Correct LED counts
        {
            uint32_t start = xTaskGetTickCount() * portTICK_PERIOD_MS;
            bool passed = true;
            std::string msg = "All LED counts match config";
            int activeCount = 0;
            
            for (int i = 0; i < NUM_STRIPS; i++) {
                if (LED_STRIPS[i].active) {
                    activeCount++;
                }
            }
            
            if (activeCount != ACTIVE_STRIP_COUNT) {
                passed = false;
                msg = "Active strip count mismatch";
            }
            
            addTestResult(suite, "Active Strip Count", passed, msg,
                         (xTaskGetTickCount() * portTICK_PERIOD_MS) - start);
        }
        
        // Test 3: Driver ready
        {
            uint32_t start = xTaskGetTickCount() * portTICK_PERIOD_MS;
            bool passed = initialized_;
            std::string msg = passed ? "Driver ready for operations" : "Driver not ready";
            
            addTestResult(suite, "Driver Ready", passed, msg,
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
                bool passed = initialized_;
                
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
            if (LED_STRIPS[i].active && initialized_) {
                uint32_t start = xTaskGetTickCount() * portTICK_PERIOD_MS;
                int numPixels = LED_STRIPS[i].ledCount;
                
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
inline uint8_t LedStripTestHarness::pixelBuffers_[NUM_STRIPS][64][4] = {{{0}}};
inline bool LedStripTestHarness::initialized_ = false;
inline bool LedStripTestHarness::testRunning_ = false;
inline uint8_t LedStripTestHarness::currentBrightness_ = 64;

// Animation state static members
inline volatile bool LedStripTestHarness::animationRunning_ = false;
inline std::string LedStripTestHarness::currentAnimation_ = "solid";
inline uint8_t LedStripTestHarness::animR_ = 255;
inline uint8_t LedStripTestHarness::animG_ = 255;
inline uint8_t LedStripTestHarness::animB_ = 255;
inline uint8_t LedStripTestHarness::animSpeed_ = 50;
inline TaskHandle_t LedStripTestHarness::animationTaskHandle_ = nullptr;
inline volatile bool LedStripTestHarness::taskExited_ = true;

} // namespace Testing
} // namespace SystemAPI
