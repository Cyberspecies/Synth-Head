/*****************************************************************
 * File:      SpriteSystem.hpp
 * Category:  Application/Pipeline
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Complete sprite management system for CPU -> GPU rendering
 *    
 *    Features:
 *    - Sprite caching on GPU (upload once, blit many times)
 *    - BMP file format support for SD card storage
 *    - Smooth float-based movement with sub-pixel precision
 *    - Multiple sprite instances with velocity and animation
 *    - Rotation via pre-computed sprite variants
 *    
 *    File Format: BMP (Windows Bitmap)
 *    - Fast to decode (no compression by default)
 *    - Direct RGB888 pixel data
 *    - Max sprite size: 32x32 for efficiency
 *****************************************************************/

#ifndef ARCOS_APPLICATION_SPRITE_SYSTEM_HPP_
#define ARCOS_APPLICATION_SPRITE_SYSTEM_HPP_

#include <stdint.h>
#include <cstring>
#include <cmath>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "GpuDriver/GpuUartMutex.hpp"  // Thread-safe UART access

namespace Application {

// ============================================================
// Constants
// ============================================================

static constexpr int MAX_SPRITES = 64;          // GPU can hold 64 sprites
static constexpr int MAX_SPRITE_SIZE = 32;      // Max 32x32 pixels
static constexpr int SPRITE_DATA_SIZE = MAX_SPRITE_SIZE * MAX_SPRITE_SIZE * 3; // RGB888
static constexpr int MAX_INSTANCES = 16;        // Max active sprite instances

// ============================================================
// GPU Protocol for Sprites
// ============================================================

class SpriteGpuProtocol {
public:
    static constexpr uint8_t SYNC0 = 0xAA;
    static constexpr uint8_t SYNC1 = 0x55;
    
    enum Cmd : uint8_t {
        NOP           = 0x00,
        UPLOAD_SPRITE = 0x20,
        DELETE_SPRITE = 0x21,
        DRAW_PIXEL    = 0x40,
        DRAW_LINE     = 0x41,
        DRAW_RECT     = 0x42,
        DRAW_FILL     = 0x43,
        BLIT_SPRITE   = 0x46,
        CLEAR         = 0x47,
        DRAW_LINE_F   = 0x48,  // Float line (8.8 fixed point)
        DRAW_CIRCLE_F = 0x49,  // Float circle
        DRAW_RECT_F   = 0x4A,  // Float rectangle
        SET_TARGET    = 0x50,
        PRESENT       = 0x51,
        OLED_CLEAR    = 0x60,
        OLED_PRESENT  = 0x65,
    };
    
    SpriteGpuProtocol() : port_(UART_NUM_1), initialized_(false) {}
    
    bool init(uart_port_t port) {
        port_ = port;
        if (!uart_is_driver_installed(port_)) {
            return false;
        }
        initialized_ = true;
        return true;
    }
    
    bool isInitialized() const { return initialized_; }
    
    // Send raw command (thread-safe with GPU UART mutex)
    void sendCmd(Cmd cmd, const uint8_t* payload = nullptr, uint16_t len = 0) {
        if (!initialized_) return;
        
        // Acquire mutex to prevent race conditions with other cores
        GpuUart::GpuUartLock lock;
        if (!lock.isAcquired()) {
            ESP_LOGW("SpriteGpu", "sendCmd: mutex timeout, command 0x%02X dropped", cmd);
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
    
    // Upload sprite to GPU cache
    // Format: [spriteId][width][height][format][...pixel data...]
    bool uploadSprite(uint8_t spriteId, uint8_t width, uint8_t height, 
                      const uint8_t* rgbData, bool rgb888 = true) {
        if (!initialized_ || spriteId >= MAX_SPRITES) return false;
        
        uint16_t dataSize = width * height * (rgb888 ? 3 : 1);
        uint16_t totalLen = 4 + dataSize;  // header + pixel data
        
        // Build payload
        uint8_t header[4] = {
            spriteId,
            width,
            height,
            (uint8_t)(rgb888 ? 0 : 1)  // 0=RGB888, 1=1bpp
        };
        
        // Send command header
        uint8_t cmdHeader[5] = {
            SYNC0, SYNC1, UPLOAD_SPRITE,
            (uint8_t)(totalLen & 0xFF),
            (uint8_t)((totalLen >> 8) & 0xFF)
        };
        uart_write_bytes(port_, cmdHeader, 5);
        uart_write_bytes(port_, header, 4);
        uart_write_bytes(port_, rgbData, dataSize);
        
        waitTxDone(100);  // Wait for large upload
        return true;
    }
    
    // Delete sprite from GPU cache
    void deleteSprite(uint8_t spriteId) {
        sendCmd(DELETE_SPRITE, &spriteId, 1);
    }
    
    // Blit sprite at integer position
    void blitSprite(uint8_t spriteId, int16_t x, int16_t y) {
        uint8_t p[5] = {
            spriteId,
            (uint8_t)(x & 0xFF), (uint8_t)((x >> 8) & 0xFF),
            (uint8_t)(y & 0xFF), (uint8_t)((y >> 8) & 0xFF)
        };
        sendCmd(BLIT_SPRITE, p, 5);
    }
    
    // Draw filled rectangle
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
    
    // Draw line with 8.8 fixed-point coordinates (sub-pixel precision)
    void drawLineFloat(float x1, float y1, float x2, float y2,
                       uint8_t r, uint8_t g, uint8_t b) {
        auto toFixed88 = [](float v) -> uint16_t {
            int16_t intPart = (int16_t)v;
            uint8_t fracPart = (uint8_t)((v - intPart) * 256);
            return (uint16_t)((intPart << 8) | fracPart);
        };
        
        uint16_t fx1 = toFixed88(x1);
        uint16_t fy1 = toFixed88(y1);
        uint16_t fx2 = toFixed88(x2);
        uint16_t fy2 = toFixed88(y2);
        
        uint8_t p[11] = {
            (uint8_t)(fx1 & 0xFF), (uint8_t)(fx1 >> 8),
            (uint8_t)(fy1 & 0xFF), (uint8_t)(fy1 >> 8),
            (uint8_t)(fx2 & 0xFF), (uint8_t)(fx2 >> 8),
            (uint8_t)(fy2 & 0xFF), (uint8_t)(fy2 >> 8),
            r, g, b
        };
        sendCmd(DRAW_LINE_F, p, 11);
    }
    
private:
    uart_port_t port_;
    bool initialized_;
};

// ============================================================
// Sprite Definition (metadata only - pixel data is on GPU)
// ============================================================

struct SpriteDefinition {
    uint8_t id;                    // GPU sprite slot ID
    uint8_t width;
    uint8_t height;
    bool loaded;                   // True if uploaded to GPU
    char name[32];                 // Sprite name (for UI)
    // Note: Pixel data is NOT stored on CPU - only sent to GPU during upload
};

// ============================================================
// Sprite Instance (runtime state for animation)
// ============================================================

struct SpriteInstance {
    // Position (float for smooth movement)
    float x;
    float y;
    
    // Velocity (pixels per second)
    float vx;
    float vy;
    
    // Acceleration (for physics effects)
    float ax;
    float ay;
    
    // Rotation (degrees)
    float angle;
    float rotationSpeed;  // degrees per second
    
    // Sprite reference
    uint8_t baseSpriteId;   // Base sprite ID
    uint8_t rotationFrames; // Number of rotation variants (0=no rotation)
    
    // Animation state
    bool active;
    bool bounceX;           // Bounce off X boundaries
    bool bounceY;           // Bounce off Y boundaries
    
    // Movement smoothing
    float smoothingFactor;  // 0.0 = instant, 1.0 = max smooth
    float targetX;
    float targetY;
    bool useSmoothing;
};

// ============================================================
// Sprite Manager
// ============================================================

class SpriteManager {
public:
    static constexpr const char* TAG = "SpriteMgr";
    
    SpriteManager() : initialized_(false), gpu_(nullptr), numSprites_(0), numInstances_(0) {
        mutex_ = xSemaphoreCreateMutex();
        memset(sprites_, 0, sizeof(sprites_));
        memset(instances_, 0, sizeof(instances_));
    }
    
    ~SpriteManager() {
        if (mutex_) vSemaphoreDelete(mutex_);
    }
    
    // Initialize with GPU protocol
    bool init(SpriteGpuProtocol* gpu) {
        if (!gpu || !gpu->isInitialized()) return false;
        gpu_ = gpu;
        initialized_ = true;
        ESP_LOGI(TAG, "Sprite Manager initialized");
        return true;
    }
    
    // ========== Sprite Definition Management ==========
    
    // Create a solid color sprite
    int createSolidSprite(uint8_t width, uint8_t height, 
                          uint8_t r, uint8_t g, uint8_t b,
                          const char* name = "solid") {
        if (!initialized_ || numSprites_ >= MAX_SPRITES) return -1;
        
        // Allocate temporary buffer for pixel data
        int dataSize = width * height * 3;
        uint8_t* data = (uint8_t*)malloc(dataSize);
        if (!data) {
            ESP_LOGE(TAG, "Out of memory for sprite data");
            return -1;
        }
        
        // Fill with solid color
        int idx = 0;
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                data[idx++] = r;
                data[idx++] = g;
                data[idx++] = b;
            }
        }
        
        // Upload to GPU
        if (!gpu_->uploadSprite(numSprites_, width, height, data)) {
            ESP_LOGE(TAG, "Failed to upload sprite %d", numSprites_);
            free(data);
            return -1;
        }
        free(data);  // GPU has the data now
        
        // Store metadata only
        SpriteDefinition& sprite = sprites_[numSprites_];
        sprite.id = numSprites_;
        sprite.width = width;
        sprite.height = height;
        sprite.loaded = true;
        strncpy(sprite.name, name, sizeof(sprite.name) - 1);
        
        ESP_LOGI(TAG, "Created solid sprite %d: %dx%d RGB(%d,%d,%d)", 
                 sprite.id, width, height, r, g, b);
        
        return numSprites_++;
    }
    
    // Create sprite from 1-bit shape array
    int createFromShape(const uint8_t shape[][8], uint8_t width, uint8_t height,
                        uint8_t r, uint8_t g, uint8_t b,
                        const char* name = "shape") {
        if (!initialized_ || numSprites_ >= MAX_SPRITES) return -1;
        
        // Allocate temporary buffer for pixel data
        int dataSize = width * height * 3;
        uint8_t* data = (uint8_t*)malloc(dataSize);
        if (!data) {
            ESP_LOGE(TAG, "Out of memory for sprite data");
            return -1;
        }
        
        // Convert 1-bit shape to RGB888
        int idx = 0;
        for (int y = 0; y < height && y < 8; y++) {
            for (int x = 0; x < width && x < 8; x++) {
                if (shape[y][x]) {
                    data[idx++] = r;
                    data[idx++] = g;
                    data[idx++] = b;
                } else {
                    data[idx++] = 0;
                    data[idx++] = 0;
                    data[idx++] = 0;
                }
            }
        }
        
        // Upload to GPU
        if (!gpu_->uploadSprite(numSprites_, width, height, data)) {
            ESP_LOGE(TAG, "Failed to upload sprite %d", numSprites_);
            free(data);
            return -1;
        }
        free(data);  // GPU has the data now
        
        // Store metadata only
        SpriteDefinition& sprite = sprites_[numSprites_];
        sprite.id = numSprites_;
        sprite.width = width;
        sprite.height = height;
        sprite.loaded = true;
        strncpy(sprite.name, name, sizeof(sprite.name) - 1);
        
        ESP_LOGI(TAG, "Created shape sprite %d: %s", sprite.id, name);
        
        return numSprites_++;
    }
    
    // Create sprite from raw RGB888 data
    int createFromRGB(const uint8_t* rgbData, uint8_t width, uint8_t height,
                      const char* name = "custom") {
        if (!initialized_ || numSprites_ >= MAX_SPRITES) return -1;
        
        // Upload directly to GPU (data is already in correct format)
        if (!gpu_->uploadSprite(numSprites_, width, height, rgbData)) {
            ESP_LOGE(TAG, "Failed to upload sprite %d", numSprites_);
            return -1;
        }
        
        // Store metadata only
        SpriteDefinition& sprite = sprites_[numSprites_];
        sprite.id = numSprites_;
        sprite.width = width;
        sprite.height = height;
        sprite.loaded = true;
        strncpy(sprite.name, name, sizeof(sprite.name) - 1);
        
        ESP_LOGI(TAG, "Created custom sprite %d: %dx%d", sprite.id, width, height);
        
        return numSprites_++;
    }
    
    // Load sprite from BMP file on SD card
    // BMP format: Simple Windows bitmap, 24-bit RGB, no compression
    int loadFromBMP(const char* filename, const char* name = nullptr) {
        if (!initialized_ || numSprites_ >= MAX_SPRITES) return -1;
        
        FILE* f = fopen(filename, "rb");
        if (!f) {
            ESP_LOGE(TAG, "Failed to open BMP: %s", filename);
            return -1;
        }
        
        // Read BMP header
        uint8_t header[54];
        if (fread(header, 1, 54, f) != 54) {
            fclose(f);
            ESP_LOGE(TAG, "Invalid BMP header");
            return -1;
        }
        
        // Verify BMP signature
        if (header[0] != 'B' || header[1] != 'M') {
            fclose(f);
            ESP_LOGE(TAG, "Not a BMP file");
            return -1;
        }
        
        // Extract dimensions (little endian)
        int32_t width = *(int32_t*)&header[18];
        int32_t height = *(int32_t*)&header[22];
        uint16_t bpp = *(uint16_t*)&header[28];
        
        // Validate
        if (width > MAX_SPRITE_SIZE || height > MAX_SPRITE_SIZE) {
            fclose(f);
            ESP_LOGE(TAG, "BMP too large: %ldx%ld (max %d)", width, height, MAX_SPRITE_SIZE);
            return -1;
        }
        
        if (bpp != 24) {
            fclose(f);
            ESP_LOGE(TAG, "Unsupported BMP format: %d bpp (need 24)", bpp);
            return -1;
        }
        
        // Allocate temporary buffer for pixel data
        int absHeight = abs(height);
        int dataSize = width * absHeight * 3;
        uint8_t* data = (uint8_t*)malloc(dataSize);
        if (!data) {
            fclose(f);
            ESP_LOGE(TAG, "Out of memory for BMP pixel data");
            return -1;
        }
        
        // BMP rows are padded to 4 bytes
        int rowSize = ((width * 3 + 3) / 4) * 4;
        uint8_t* row = (uint8_t*)malloc(rowSize);
        if (!row) {
            free(data);
            fclose(f);
            ESP_LOGE(TAG, "Out of memory for BMP row");
            return -1;
        }
        
        // Seek to pixel data
        uint32_t dataOffset = *(uint32_t*)&header[10];
        fseek(f, dataOffset, SEEK_SET);
        
        // Read pixel data (BMP is bottom-up by default unless height is negative)
        bool bottomUp = (height > 0);
        
        for (int y = 0; y < absHeight; y++) {
            if (fread(row, 1, rowSize, f) != (size_t)rowSize) break;
            
            int destY = bottomUp ? (absHeight - 1 - y) : y;
            int destIdx = destY * width * 3;
            
            for (int x = 0; x < width; x++) {
                // BMP stores BGR, we need RGB
                data[destIdx + x * 3 + 0] = row[x * 3 + 2]; // R
                data[destIdx + x * 3 + 1] = row[x * 3 + 1]; // G
                data[destIdx + x * 3 + 2] = row[x * 3 + 0]; // B
            }
        }
        
        free(row);
        fclose(f);
        
        // Upload to GPU
        if (!gpu_->uploadSprite(numSprites_, width, absHeight, data)) {
            ESP_LOGE(TAG, "Failed to upload sprite %d", numSprites_);
            free(data);
            return -1;
        }
        free(data);  // GPU has the data now
        
        // Store metadata only
        SpriteDefinition& sprite = sprites_[numSprites_];
        sprite.id = numSprites_;
        sprite.width = width;
        sprite.height = absHeight;
        sprite.loaded = true;
        
        if (name) {
            strncpy(sprite.name, name, sizeof(sprite.name) - 1);
        } else {
            // Extract filename from path
            const char* p = strrchr(filename, '/');
            if (!p) p = strrchr(filename, '\\');
            strncpy(sprite.name, p ? p + 1 : filename, sizeof(sprite.name) - 1);
        }
        
        ESP_LOGI(TAG, "Loaded BMP sprite %d: %s (%dx%d)", 
                 sprite.id, sprite.name, sprite.width, sprite.height);
        
        return numSprites_++;
    }
    
    // Save sprite to BMP - NOTE: Cannot save since pixel data is on GPU only
    // This will save the sprite's metadata as a placeholder BMP
    bool saveToBMP(uint8_t spriteId, const char* filename) {
        if (spriteId >= numSprites_ || !sprites_[spriteId].loaded) {
            ESP_LOGE(TAG, "Invalid sprite ID: %d", spriteId);
            return false;
        }
        
        ESP_LOGW(TAG, "saveToBMP: Pixel data is on GPU only - cannot save original");
        
        SpriteDefinition& sprite = sprites_[spriteId];
        
        FILE* f = fopen(filename, "wb");
        if (!f) {
            ESP_LOGE(TAG, "Failed to create BMP: %s", filename);
            return false;
        }
        
        int width = sprite.width;
        int height = sprite.height;
        int rowSize = ((width * 3 + 3) / 4) * 4;
        int dataSize = rowSize * height;
        int fileSize = 54 + dataSize;
        
        // BMP file header (14 bytes)
        uint8_t fileHeader[14] = {
            'B', 'M',                               // Signature
            (uint8_t)(fileSize & 0xFF),             // File size
            (uint8_t)((fileSize >> 8) & 0xFF),
            (uint8_t)((fileSize >> 16) & 0xFF),
            (uint8_t)((fileSize >> 24) & 0xFF),
            0, 0, 0, 0,                             // Reserved
            54, 0, 0, 0                             // Data offset
        };
        
        // BMP info header (40 bytes)
        uint8_t infoHeader[40] = {0};
        infoHeader[0] = 40;                         // Header size
        *(int32_t*)&infoHeader[4] = width;          // Width
        *(int32_t*)&infoHeader[8] = height;         // Height (positive = bottom-up)
        *(uint16_t*)&infoHeader[12] = 1;            // Color planes
        *(uint16_t*)&infoHeader[14] = 24;           // Bits per pixel
        *(int32_t*)&infoHeader[20] = dataSize;      // Image size
        
        fwrite(fileHeader, 1, 14, f);
        fwrite(infoHeader, 1, 40, f);
        
        // Write placeholder pixel data (gray color)
        uint8_t* row = (uint8_t*)malloc(rowSize);
        if (!row) {
            fclose(f);
            return false;
        }
        
        // Fill row with gray placeholder color
        for (int x = 0; x < width; x++) {
            row[x * 3 + 0] = 128; // B
            row[x * 3 + 1] = 128; // G
            row[x * 3 + 2] = 128; // R
        }
        // Zero padding bytes
        for (int x = width * 3; x < rowSize; x++) {
            row[x] = 0;
        }
        
        // Write all rows with placeholder color
        for (int y = 0; y < height; y++) {
            fwrite(row, 1, rowSize, f);
        }
        
        free(row);
        fclose(f);
        
        ESP_LOGI(TAG, "Saved placeholder BMP for sprite %d: %s", spriteId, filename);
        return true;
    }
    
    // Save RGB data directly to BMP (for user-created content)
    bool saveRGBToBMP(const uint8_t* rgbData, int width, int height, const char* filename) {
        FILE* f = fopen(filename, "wb");
        if (!f) {
            ESP_LOGE(TAG, "Failed to create BMP: %s", filename);
            return false;
        }
        
        int rowSize = ((width * 3 + 3) / 4) * 4;
        int dataSize = rowSize * height;
        int fileSize = 54 + dataSize;
        
        // BMP file header (14 bytes)
        uint8_t fileHeader[14] = {
            'B', 'M',                               // Signature
            (uint8_t)(fileSize & 0xFF),             // File size
            (uint8_t)((fileSize >> 8) & 0xFF),
            (uint8_t)((fileSize >> 16) & 0xFF),
            (uint8_t)((fileSize >> 24) & 0xFF),
            0, 0, 0, 0,                             // Reserved
            54, 0, 0, 0                             // Data offset
        };
        
        // BMP info header (40 bytes)
        uint8_t infoHeader[40] = {0};
        infoHeader[0] = 40;                         // Header size
        *(int32_t*)&infoHeader[4] = width;          // Width
        *(int32_t*)&infoHeader[8] = height;         // Height (positive = bottom-up)
        *(uint16_t*)&infoHeader[12] = 1;            // Color planes
        *(uint16_t*)&infoHeader[14] = 24;           // Bits per pixel
        *(int32_t*)&infoHeader[20] = dataSize;      // Image size
        
        fwrite(fileHeader, 1, 14, f);
        fwrite(infoHeader, 1, 40, f);
        
        // Write pixel data (bottom-up, BGR format)
        uint8_t* row = (uint8_t*)malloc(rowSize);
        if (!row) {
            fclose(f);
            return false;
        }
        memset(row, 0, rowSize);
        
        for (int y = height - 1; y >= 0; y--) {
            int srcIdx = y * width * 3;
            for (int x = 0; x < width; x++) {
                // Convert RGB to BGR
                row[x * 3 + 0] = rgbData[srcIdx + x * 3 + 2]; // B
                row[x * 3 + 1] = rgbData[srcIdx + x * 3 + 1]; // G
                row[x * 3 + 2] = rgbData[srcIdx + x * 3 + 0]; // R
            }
            fwrite(row, 1, rowSize, f);
        }
        
        free(row);
        fclose(f);
        
        ESP_LOGI(TAG, "Saved BMP: %s (%dx%d)", filename, width, height);
        return true;
    }
    
    // ========== Sprite Instance Management ==========
    
    // Create a sprite instance for animation
    int createInstance(uint8_t spriteId, float x, float y) {
        if (numInstances_ >= MAX_INSTANCES || spriteId >= numSprites_) return -1;
        
        SpriteInstance& inst = instances_[numInstances_];
        inst.x = x;
        inst.y = y;
        inst.vx = 0;
        inst.vy = 0;
        inst.ax = 0;
        inst.ay = 0;
        inst.angle = 0;
        inst.rotationSpeed = 0;
        inst.baseSpriteId = spriteId;
        inst.rotationFrames = 0;
        inst.active = true;
        inst.bounceX = false;
        inst.bounceY = false;
        inst.smoothingFactor = 0.1f;
        inst.targetX = x;
        inst.targetY = y;
        inst.useSmoothing = false;
        
        return numInstances_++;
    }
    
    // Set instance velocity
    void setInstanceVelocity(int instanceId, float vx, float vy) {
        if (instanceId < 0 || instanceId >= numInstances_) return;
        instances_[instanceId].vx = vx;
        instances_[instanceId].vy = vy;
    }
    
    // Set instance rotation
    void setInstanceRotation(int instanceId, float angle, float speed, uint8_t frames) {
        if (instanceId < 0 || instanceId >= numInstances_) return;
        instances_[instanceId].angle = angle;
        instances_[instanceId].rotationSpeed = speed;
        instances_[instanceId].rotationFrames = frames;
    }
    
    // Enable smooth movement
    void setInstanceSmoothing(int instanceId, float factor, bool enable) {
        if (instanceId < 0 || instanceId >= numInstances_) return;
        instances_[instanceId].smoothingFactor = factor;
        instances_[instanceId].useSmoothing = enable;
    }
    
    // Set target position for smooth movement
    void setInstanceTarget(int instanceId, float x, float y) {
        if (instanceId < 0 || instanceId >= numInstances_) return;
        instances_[instanceId].targetX = x;
        instances_[instanceId].targetY = y;
    }
    
    // Enable bouncing
    void setInstanceBounce(int instanceId, bool bounceX, bool bounceY) {
        if (instanceId < 0 || instanceId >= numInstances_) return;
        instances_[instanceId].bounceX = bounceX;
        instances_[instanceId].bounceY = bounceY;
    }
    
    // Set instance position directly
    void setInstancePosition(int instanceId, float x, float y) {
        if (instanceId < 0 || instanceId >= numInstances_) return;
        instances_[instanceId].x = x;
        instances_[instanceId].y = y;
        if (!instances_[instanceId].useSmoothing) {
            instances_[instanceId].targetX = x;
            instances_[instanceId].targetY = y;
        }
    }
    
    // Get instance position
    void getInstancePosition(int instanceId, float* x, float* y) {
        if (instanceId < 0 || instanceId >= numInstances_) return;
        if (x) *x = instances_[instanceId].x;
        if (y) *y = instances_[instanceId].y;
    }
    
    // ========== Update & Render ==========
    
    // Update all instances (call at 60fps)
    void update(float deltaTime) {
        if (!initialized_) return;
        
        for (int i = 0; i < numInstances_; i++) {
            SpriteInstance& inst = instances_[i];
            if (!inst.active) continue;
            
            // Update rotation
            if (inst.rotationSpeed != 0) {
                inst.angle += inst.rotationSpeed * deltaTime;
                while (inst.angle >= 360.0f) inst.angle -= 360.0f;
                while (inst.angle < 0.0f) inst.angle += 360.0f;
            }
            
            // Smooth movement towards target
            if (inst.useSmoothing) {
                float dx = inst.targetX - inst.x;
                float dy = inst.targetY - inst.y;
                inst.x += dx * inst.smoothingFactor;
                inst.y += dy * inst.smoothingFactor;
            } else {
                // Apply acceleration
                inst.vx += inst.ax * deltaTime;
                inst.vy += inst.ay * deltaTime;
                
                // Apply velocity
                inst.x += inst.vx * deltaTime * 60.0f;  // Scale for 60fps reference
                inst.y += inst.vy * deltaTime * 60.0f;
            }
            
            // Get sprite dimensions for bounds checking
            int w = 8, h = 8;  // Default
            if (inst.baseSpriteId < numSprites_) {
                w = sprites_[inst.baseSpriteId].width;
                h = sprites_[inst.baseSpriteId].height;
            }
            
            // Bounce off boundaries (128x32 display)
            if (inst.bounceX) {
                if (inst.x < 0) { inst.x = 0; inst.vx = -inst.vx; }
                if (inst.x > 128 - w) { inst.x = 128 - w; inst.vx = -inst.vx; }
            }
            if (inst.bounceY) {
                if (inst.y < 0) { inst.y = 0; inst.vy = -inst.vy; }
                if (inst.y > 32 - h) { inst.y = 32 - h; inst.vy = -inst.vy; }
            }
        }
    }
    
    // Render all active instances
    void render(uint8_t bgR = 0, uint8_t bgG = 0, uint8_t bgB = 0) {
        if (!initialized_ || !gpu_) return;
        
        gpu_->setTarget(0);  // HUB75
        gpu_->clear(bgR, bgG, bgB);
        
        for (int i = 0; i < numInstances_; i++) {
            SpriteInstance& inst = instances_[i];
            if (!inst.active) continue;
            
            // Determine which sprite to use (rotation variant)
            uint8_t spriteId = inst.baseSpriteId;
            if (inst.rotationFrames > 0) {
                int frame = ((int)(inst.angle / (360.0f / inst.rotationFrames))) % inst.rotationFrames;
                spriteId = inst.baseSpriteId + frame;
            }
            
            // Blit at integer position (GPU handles sub-pixel if supported)
            gpu_->blitSprite(spriteId, (int16_t)inst.x, (int16_t)inst.y);
        }
        
        gpu_->present();
    }
    
    // Render single sprite at position (for scene mode)
    void renderSingle(uint8_t spriteId, float x, float y,
                      uint8_t bgR = 0, uint8_t bgG = 0, uint8_t bgB = 0) {
        if (!initialized_ || !gpu_ || spriteId >= numSprites_) return;
        
        gpu_->setTarget(0);
        gpu_->clear(bgR, bgG, bgB);
        gpu_->blitSprite(spriteId, (int16_t)x, (int16_t)y);
        gpu_->present();
    }
    
    // Get sprite count
    int getSpriteCount() const { return numSprites_; }
    int getInstanceCount() const { return numInstances_; }
    
    // Get sprite info
    const SpriteDefinition* getSprite(uint8_t id) const {
        if (id >= numSprites_) return nullptr;
        return &sprites_[id];
    }
    
    // Get instance
    SpriteInstance* getInstance(int id) {
        if (id < 0 || id >= numInstances_) return nullptr;
        return &instances_[id];
    }
    
private:
    bool initialized_;
    SpriteGpuProtocol* gpu_;
    SemaphoreHandle_t mutex_;
    
    SpriteDefinition sprites_[MAX_SPRITES];
    SpriteInstance instances_[MAX_INSTANCES];
    int numSprites_;
    int numInstances_;
};

// ============================================================
// Global Accessor
// ============================================================

inline SpriteManager& getSpriteManager() {
    static SpriteManager instance;
    return instance;
}

} // namespace Application

#endif // ARCOS_APPLICATION_SPRITE_SYSTEM_HPP_
