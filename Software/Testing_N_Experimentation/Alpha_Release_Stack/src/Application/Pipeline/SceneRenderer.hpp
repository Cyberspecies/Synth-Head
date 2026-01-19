/*****************************************************************
 * File:      SceneRenderer.hpp
 * Category:  Application/Pipeline
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Scene Renderer for Core 1 - handles manual scene rendering
 *    at 60fps continuous. Works alongside AnimationPipeline.
 *    
 *    When a manual scene is active (e.g., static sprite display),
 *    this takes over rendering from AnimationPipeline.
 *    
 *    Features:
 *    - Static sprite scene rendering
 *    - Background color fill
 *    - Test pattern rendering (placeholder for sprites)
 *    - Continuous 60fps updates to GPU
 *****************************************************************/

#ifndef ARCOS_APPLICATION_SCENE_RENDERER_HPP_
#define ARCOS_APPLICATION_SCENE_RENDERER_HPP_

#include <stdint.h>
#include <cstring>
#include <atomic>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "GpuDriver/GpuUartMutex.hpp"  // Thread-safe UART access

namespace Application {

// ============================================================
// Scene Types
// ============================================================

enum class SceneType : uint8_t {
    NONE = 0,           // No scene active - use AnimationPipeline
    STATIC_SPRITE,      // Static sprite at position with background
    ANIMATED_SPRITE,    // Sprite with smooth movement
    SOLID_COLOR,        // Solid color fill
    TEST_PATTERN,       // Test pattern for debugging
};

// ============================================================
// Scene Configuration
// ============================================================

struct SceneConfig {
    SceneType type = SceneType::NONE;
    
    // Background color
    uint8_t bgR = 0;
    uint8_t bgG = 0;
    uint8_t bgB = 0;
    
    // Sprite configuration (GPU cached sprite ID)
    int spriteId = 0;
    
    // Position (float for smooth movement)
    float posX = 0.0f;
    float posY = 0.0f;
    
    // Target position (for smooth interpolation)
    float targetX = 0.0f;
    float targetY = 0.0f;
    
    // Size (for test pattern rendering)
    int16_t width = 32;
    int16_t height = 32;
    
    // Sprite color (for test pattern)
    uint8_t spriteR = 0;
    uint8_t spriteG = 255;
    uint8_t spriteB = 128;
    
    // Smooth movement config
    float smoothingFactor = 0.1f;  // 0.0 = instant, 1.0 = max smooth
    bool useSmoothing = false;
    
    // Version counter for change detection
    uint32_t version = 0;
};

// ============================================================
// GPU Command Protocol
// ============================================================

class GpuProtocol {
public:
    // Protocol constants
    static constexpr uint8_t SYNC0 = 0xAA;
    static constexpr uint8_t SYNC1 = 0x55;
    
    // Command opcodes
    enum Cmd : uint8_t {
        NOP           = 0x00,
        UPLOAD_SPRITE = 0x20,
        DELETE_SPRITE = 0x21,
        DRAW_PIXEL    = 0x40,
        DRAW_LINE     = 0x41,
        DRAW_RECT     = 0x42,
        DRAW_FILL     = 0x43,
        DRAW_CIRCLE   = 0x44,
        DRAW_POLY     = 0x45,
        BLIT_SPRITE   = 0x46,
        CLEAR         = 0x47,
        DRAW_LINE_F   = 0x48,  // Float (8.8 fixed point)
        DRAW_CIRCLE_F = 0x49,
        DRAW_RECT_F   = 0x4A,
        SET_TARGET    = 0x50,
        PRESENT       = 0x51,
        OLED_CLEAR    = 0x60,
        OLED_PRESENT  = 0x65,
        RESET         = 0xFF,
    };
    
    GpuProtocol() : port_(UART_NUM_1), initialized_(false) {}
    
    /** Initialize - just sets the port, does NOT install UART driver
     *  UART driver should already be installed by GpuPipeline
     */
    bool init(uart_port_t port) {
        if (initialized_) return true;
        
        port_ = port;
        
        // Just verify UART driver is already installed (by GpuPipeline)
        if (!uart_is_driver_installed(port_)) {
            ESP_LOGE("GpuProto", "UART driver not installed! GpuPipeline must init first.");
            return false;
        }
        
        initialized_ = true;
        ESP_LOGI("GpuProto", "Using existing UART%d driver", port_);
        return true;
    }
    
    /** Check if initialized */
    bool isInitialized() const { return initialized_; }
    
    // Send raw command (thread-safe with GPU UART mutex)
    void sendCmd(Cmd cmd, const uint8_t* payload = nullptr, uint16_t len = 0) {
        if (!initialized_) return;
        
        // Acquire mutex to prevent race conditions with Core 0 operations
        GpuUart::GpuUartLock lock;
        if (!lock.isAcquired()) {
            ESP_LOGW("GpuProto", "sendCmd: mutex timeout, command 0x%02X dropped", cmd);
            return;
        }
        
        uint8_t header[5] = {
            SYNC0, SYNC1, cmd,
            (uint8_t)(len & 0xFF),
            (uint8_t)((len >> 8) & 0xFF)
        };
        uart_write_bytes(port_, header, 5);
        if (len > 0 && payload) {
            uart_write_bytes(port_, payload, len);
        }
    }
    
    // Wait for transmission complete
    void waitTxDone(uint32_t timeoutMs = 50) {
        if (initialized_) {
            uart_wait_tx_done(port_, pdMS_TO_TICKS(timeoutMs));
        }
    }
    
    // ========== High-level commands ==========
    
    void setTarget(uint8_t target) {
        sendCmd(SET_TARGET, &target, 1);
    }
    
    void clear(uint8_t r, uint8_t g, uint8_t b) {
        uint8_t p[3] = {r, g, b};
        sendCmd(CLEAR, p, 3);
    }
    
    void present() {
        sendCmd(PRESENT, nullptr, 0);
    }
    
    void oledClear() {
        sendCmd(OLED_CLEAR, nullptr, 0);
    }
    
    void oledPresent() {
        sendCmd(OLED_PRESENT, nullptr, 0);
    }
    
    void drawPixel(int16_t x, int16_t y, uint8_t r, uint8_t g, uint8_t b) {
        uint8_t p[7] = {
            (uint8_t)(x & 0xFF), (uint8_t)((x >> 8) & 0xFF),
            (uint8_t)(y & 0xFF), (uint8_t)((y >> 8) & 0xFF),
            r, g, b
        };
        sendCmd(DRAW_PIXEL, p, 7);
    }
    
    void drawLine(int16_t x1, int16_t y1, int16_t x2, int16_t y2, 
                  uint8_t r, uint8_t g, uint8_t b) {
        uint8_t p[11] = {
            (uint8_t)(x1 & 0xFF), (uint8_t)((x1 >> 8) & 0xFF),
            (uint8_t)(y1 & 0xFF), (uint8_t)((y1 >> 8) & 0xFF),
            (uint8_t)(x2 & 0xFF), (uint8_t)((x2 >> 8) & 0xFF),
            (uint8_t)(y2 & 0xFF), (uint8_t)((y2 >> 8) & 0xFF),
            r, g, b
        };
        sendCmd(DRAW_LINE, p, 11);
    }
    
    void drawRect(int16_t x, int16_t y, int16_t w, int16_t h,
                  uint8_t r, uint8_t g, uint8_t b) {
        uint8_t p[11] = {
            (uint8_t)(x & 0xFF), (uint8_t)((x >> 8) & 0xFF),
            (uint8_t)(y & 0xFF), (uint8_t)((y >> 8) & 0xFF),
            (uint8_t)(w & 0xFF), (uint8_t)((w >> 8) & 0xFF),
            (uint8_t)(h & 0xFF), (uint8_t)((h >> 8) & 0xFF),
            r, g, b
        };
        sendCmd(DRAW_RECT, p, 11);
    }
    
    void drawFill(int16_t x, int16_t y, int16_t w, int16_t h,
                  uint8_t r, uint8_t g, uint8_t b) {
        uint8_t p[11] = {
            (uint8_t)(x & 0xFF), (uint8_t)((x >> 8) & 0xFF),
            (uint8_t)(y & 0xFF), (uint8_t)((y >> 8) & 0xFF),
            (uint8_t)(w & 0xFF), (uint8_t)((w >> 8) & 0xFF),
            (uint8_t)(h & 0xFF), (uint8_t)((h >> 8) & 0xFF),
            r, g, b
        };
        sendCmd(DRAW_FILL, p, 11);
    }
    
    // ========== Sprite commands ==========
    
    /** Upload sprite to GPU cache (call once per sprite) */
    void uploadSprite(uint8_t spriteId, uint8_t width, uint8_t height, 
                      const uint8_t* rgb888Data) {
        // Payload: spriteId(1) + width(1) + height(1) + format(1) + pixels
        size_t pixelCount = width * height;
        size_t dataLen = 4 + (pixelCount * 3);  // RGB888 = 3 bytes per pixel
        
        uint8_t* payload = new uint8_t[dataLen];
        payload[0] = spriteId;
        payload[1] = width;
        payload[2] = height;
        payload[3] = 0;  // Format: 0 = RGB888
        memcpy(&payload[4], rgb888Data, pixelCount * 3);
        
        sendCmd(UPLOAD_SPRITE, payload, dataLen);
        delete[] payload;
        
        ESP_LOGI("GpuProto", "Uploaded sprite %d (%dx%d, %zu bytes)", 
                 spriteId, width, height, dataLen);
    }
    
    /** Blit cached sprite at integer position */
    void blitSprite(uint8_t spriteId, int16_t x, int16_t y) {
        uint8_t p[5] = {
            spriteId,
            (uint8_t)(x & 0xFF), (uint8_t)((x >> 8) & 0xFF),
            (uint8_t)(y & 0xFF), (uint8_t)((y >> 8) & 0xFF)
        };
        sendCmd(BLIT_SPRITE, p, 5);
    }
    
    /** Blit sprite with float position (converts to integer, smooth later) */
    void blitSpriteF(uint8_t spriteId, float x, float y) {
        blitSprite(spriteId, (int16_t)x, (int16_t)y);
    }
    
    /** Delete sprite from GPU cache */
    void deleteSprite(uint8_t spriteId) {
        sendCmd(DELETE_SPRITE, &spriteId, 1);
    }
    
private:
    uart_port_t port_;
    bool initialized_;
};

// ============================================================
// Scene Renderer Class
// ============================================================

class SceneRenderer {
public:
    static constexpr const char* TAG = "SceneRend";
    
    SceneRenderer() 
        : initialized_(false)
        , lastRenderedVersion_(0)
    {
        mutex_ = xSemaphoreCreateMutex();
    }
    
    ~SceneRenderer() {
        if (mutex_) {
            vSemaphoreDelete(mutex_);
        }
    }
    
    /** Initialize the scene renderer */
    bool init() {
        if (initialized_) return true;
        
        // GPU protocol will be initialized externally or shared
        // We just mark ourselves as ready
        initialized_ = true;
        
        ESP_LOGI(TAG, "Scene Renderer initialized, gpu_=%p", (void*)gpu_);
        return true;
    }
    
    /** Set the GPU protocol instance to use */
    void setGpuProtocol(GpuProtocol* gpu) {
        gpu_ = gpu;
        ESP_LOGI(TAG, "GPU protocol set: %p, initialized=%d", (void*)gpu_, gpu_ ? gpu_->isInitialized() : 0);
    }
    
    /** Set active scene configuration (thread-safe, called from Core 0) */
    void setScene(const SceneConfig& config) {
        if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(100)) == pdTRUE) {
            pendingConfig_ = config;
            pendingConfig_.version = configVersion_.fetch_add(1) + 1;
            xSemaphoreGive(mutex_);
            ESP_LOGI(TAG, "Scene set: type=%d ver=%lu", (int)config.type, pendingConfig_.version);
        }
    }
    
    /** Clear the active scene (returns to animation mode) */
    void clearScene() {
        SceneConfig empty;
        empty.type = SceneType::NONE;
        setScene(empty);
    }
    
    /** Check if a manual scene is active */
    bool isSceneActive() const {
        return activeConfig_.type != SceneType::NONE;
    }
    
    /** Check if renderer is initialized */
    bool isInitialized() const {
        return initialized_;
    }
    
    /** Render one frame (called from Core 1 at 60fps) 
     *  Returns true if scene was rendered, false if no scene active
     */
    bool renderFrame() {
        if (!initialized_ || !gpu_) {
            return false;
        }
        
        // Check for new configuration
        if (xSemaphoreTake(mutex_, 0) == pdTRUE) {
            if (pendingConfig_.version != activeConfig_.version) {
                activeConfig_ = pendingConfig_;
                ESP_LOGI(TAG, "Config updated: type=%d ver=%lu spriteId=%d pos=(%.1f,%.1f)", 
                         (int)activeConfig_.type, activeConfig_.version, 
                         activeConfig_.spriteId, activeConfig_.posX, activeConfig_.posY);
            }
            xSemaphoreGive(mutex_);
        }
        
        // If no scene active, return false to let AnimationPipeline render
        if (activeConfig_.type == SceneType::NONE) {
            return false;
        }
        
        // Render the active scene
        renderScene(activeConfig_);
        return true;
    }
    
    /** Get current configuration (for debugging) */
    SceneConfig getActiveConfig() const { return activeConfig_; }
    
private:
    void renderScene(const SceneConfig& config) {
        static uint32_t renderCount = 0;
        static uint32_t lastVersion = 0;
        static uint32_t skipCounter = 0;
        
        renderCount++;
        
        // Throttle to ~30fps (skip every other frame) to prevent GPU buffer overflow
        // The GPU can only process so many commands per second
        skipCounter++;
        if (skipCounter < 2) {
            return;  // Skip this frame
        }
        skipCounter = 0;
        
        // Log when config changes for debugging
        if (config.version != lastVersion) {
            ESP_LOGI(TAG, "Scene render: type=%d ver=%lu (prev=%lu)", 
                     (int)config.type, config.version, lastVersion);
            lastVersion = config.version;
        }
        
        // Log every 30 actual renders (once per second at 30fps) for debugging
        if (renderCount % 60 == 0) {
            ESP_LOGI(TAG, "renderScene: frame=%lu type=%d spriteId=%d", 
                     renderCount, (int)config.type, config.spriteId);
        }
        
        // Target HUB75
        gpu_->setTarget(0);
        
        // Clear with background color
        gpu_->clear(config.bgR, config.bgG, config.bgB);
        
        switch (config.type) {
            case SceneType::STATIC_SPRITE:
                // Use BLIT_SPRITE with cached sprite ID
                gpu_->blitSpriteF(config.spriteId, config.posX, config.posY);
                break;
                
            case SceneType::ANIMATED_SPRITE:
                // Apply smooth interpolation if enabled
                if (config.useSmoothing) {
                    // Lerp current position toward target
                    float lerpX = config.posX + (config.targetX - config.posX) * config.smoothingFactor;
                    float lerpY = config.posY + (config.targetY - config.posY) * config.smoothingFactor;
                    gpu_->blitSpriteF(config.spriteId, lerpX, lerpY);
                } else {
                    gpu_->blitSpriteF(config.spriteId, config.posX, config.posY);
                }
                break;
                
            case SceneType::TEST_PATTERN:
                renderTestPattern(config);
                break;
                
            case SceneType::SOLID_COLOR:
                // Already cleared with bg color, just present
                break;
                
            default:
                break;
        }
        
        // Wait for UART to finish sending
        gpu_->waitTxDone(50);
        
        // Present the frame
        gpu_->present();
    }
    
    void renderTestPattern(const SceneConfig& config) {
        // Convert float to int for test pattern
        int16_t x = (int16_t)config.posX;
        int16_t y = (int16_t)config.posY;
        
        // Draw a filled rectangle as placeholder for sprite
        gpu_->drawFill(x, y, config.width, config.height,
                       config.spriteR, config.spriteG, config.spriteB);
        
        // Draw white border
        gpu_->drawRect(x, y, config.width, config.height,
                       255, 255, 255);
        
        // Draw X pattern through the rectangle
        gpu_->drawLine(x, y,
                       x + config.width - 1, y + config.height - 1,
                       255, 0, 255);
        gpu_->drawLine(x + config.width - 1, y,
                       x, y + config.height - 1,
                       255, 0, 255);
    }
    
    bool initialized_;
    GpuProtocol* gpu_ = nullptr;
    
    SemaphoreHandle_t mutex_;
    std::atomic<uint32_t> configVersion_{0};
    
    SceneConfig pendingConfig_;  // Written by Core 0
    SceneConfig activeConfig_;   // Used by Core 1
    uint32_t lastRenderedVersion_;
};

// ============================================================
// Global Accessor - Singleton pattern with explicit storage
// ============================================================

// Singleton instances - defined in ApplicationCore.cpp
extern SceneRenderer& getSceneRenderer();
extern GpuProtocol& getGpuProtocol();

} // namespace Application

#endif // ARCOS_APPLICATION_SCENE_RENDERER_HPP_
