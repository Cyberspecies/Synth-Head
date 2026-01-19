/*****************************************************************
 * File:      SpriteDriver.hpp
 * Category:  Application/Sprites
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Complete Sprite System Driver with full debug tracing.
 *    Handles the entire sprite pipeline:
 *    
 *    1. SD Card Loading   - Find and read BMP files from /sdcard/Sprites/
 *    2. RAM Storage       - Decode BMP to RGB888 and cache in memory
 *    3. Inter-Core Queue  - Transfer sprite data from Core 0 to Core 1
 *    4. GPU Upload        - Core 1 uploads sprite to GPU cache
 *    5. GPU Blit          - Core 1 renders sprite at position
 *    
 *    Each stage has comprehensive logging with [SPRITE-STAGE] prefixes.
 *****************************************************************/

#ifndef ARCOS_APPLICATION_SPRITES_DRIVER_HPP_
#define ARCOS_APPLICATION_SPRITES_DRIVER_HPP_

#include <cstdint>
#include <cstring>
#include <cstdio>
#include <vector>
#include <string>
#include <map>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "dirent.h"
#include "sys/stat.h"
#include "GpuDriver/GpuUartMutex.hpp"  // Thread-safe UART access

namespace Application {
namespace Sprites {

// ============================================================
// Debug Tags for Each Stage
// ============================================================
static const char* TAG_SD      = "SPRITE-SD";      // Stage 1: SD card operations
static const char* TAG_RAM     = "SPRITE-RAM";     // Stage 2: RAM storage
static const char* TAG_QUEUE   = "SPRITE-QUEUE";   // Stage 3: Inter-core queue
static const char* TAG_GPU     = "SPRITE-GPU";     // Stage 4: GPU upload
static const char* TAG_BLIT    = "SPRITE-BLIT";    // Stage 5: GPU blit/render

// ============================================================
// Configuration Constants
// ============================================================
static constexpr const char* SPRITES_PATH = "/sdcard/Sprites";
static constexpr uint8_t MAX_SPRITES = 32;         // Max sprites in RAM cache
static constexpr uint8_t MAX_GPU_SPRITES = 16;     // Max sprites in GPU cache
static constexpr uint32_t MAX_SPRITE_SIZE = 64*64; // Max 64x64 pixels
static constexpr uint32_t SPRITE_QUEUE_SIZE = 8;   // Commands pending to Core 1

// ============================================================
// Sprite Data Structure (RAM Cache)
// ============================================================
struct SpriteData {
    uint8_t id = 0;                    // Unique sprite ID
    char name[32] = {0};               // Sprite name (from filename)
    uint16_t width = 0;                // Width in pixels
    uint16_t height = 0;               // Height in pixels
    uint32_t dataSize = 0;             // Size of pixel data (w*h*3)
    uint8_t* pixels = nullptr;         // RGB888 pixel data (owned)
    bool inGpuCache = false;           // Has been uploaded to GPU
    uint8_t gpuSlot = 0xFF;            // GPU cache slot (0xFF = not assigned)
    
    // File info
    char filePath[64] = {0};           // Full path on SD card
    uint32_t fileSize = 0;             // Original file size
    
    // Debug timestamps
    uint32_t loadedTimeMs = 0;         // When loaded from SD
    uint32_t uploadedTimeMs = 0;       // When uploaded to GPU
    
    // Free allocated memory
    void free() {
        if (pixels) {
            ::free(pixels);
            pixels = nullptr;
        }
        dataSize = 0;
    }
    
    ~SpriteData() { free(); }
};

// ============================================================
// BMP File Header Structures
// ============================================================
#pragma pack(push, 1)
struct BMPFileHeader {
    uint16_t signature;      // 'BM' = 0x4D42
    uint32_t fileSize;
    uint16_t reserved1;
    uint16_t reserved2;
    uint32_t dataOffset;     // Offset to pixel data
};

struct BMPInfoHeader {
    uint32_t headerSize;     // Usually 40
    int32_t  width;
    int32_t  height;         // Negative = top-down
    uint16_t planes;         // Must be 1
    uint16_t bitsPerPixel;   // 24 or 32
    uint32_t compression;    // 0 = none
    uint32_t imageSize;
    int32_t  xPelsPerMeter;
    int32_t  yPelsPerMeter;
    uint32_t colorsUsed;
    uint32_t colorsImportant;
};
#pragma pack(pop)

// ============================================================
// Inter-Core Command Types
// ============================================================
enum class SpriteCommand : uint8_t {
    UPLOAD = 1,      // Upload sprite to GPU cache
    BLIT = 2,        // Draw sprite at position
    DELETE = 3,      // Remove from GPU cache
    CLEAR = 4,       // Clear display
};

// Command packet for Core 1 queue
struct SpriteQueueItem {
    SpriteCommand cmd;
    uint8_t spriteId;
    int16_t x;
    int16_t y;
    uint8_t bgR, bgG, bgB;
    // For upload commands, we include a copy of sprite data
    uint16_t width;
    uint16_t height;
    uint8_t* pixelsCopy;     // Allocated copy (Core 1 frees after upload)
    uint32_t pixelsSize;
};

// ============================================================
// GPU Protocol (Minimal - just what we need)
// ============================================================
class SpriteGpuProtocol {
public:
    static constexpr uint8_t SYNC0 = 0xAA;
    static constexpr uint8_t SYNC1 = 0x55;
    
    enum Cmd : uint8_t {
        SET_TARGET     = 0x01,
        CLEAR          = 0x47,
        PRESENT        = 0x51,
        UPLOAD_SPRITE  = 0x20,
        DELETE_SPRITE  = 0x21,
        BLIT_SPRITE    = 0x46,
    };
    
private:
    uart_port_t port_ = UART_NUM_1;
    bool initialized_ = false;
    
public:
    bool init(uart_port_t port) {
        port_ = port;
        initialized_ = true;
        ESP_LOGI(TAG_GPU, "GPU Protocol initialized on UART%d", port);
        return true;
    }
    
    void sendCmd(uint8_t cmd, const uint8_t* payload, uint16_t len) {
        if (!initialized_) return;
        
        // Acquire mutex to prevent race conditions with other cores
        GpuUart::GpuUartLock lock;
        if (!lock.isAcquired()) {
            ESP_LOGW(TAG_GPU, "sendCmd: mutex timeout, command 0x%02X dropped", cmd);
            return;
        }
        
        uint8_t header[5] = {
            SYNC0, SYNC1, cmd,
            (uint8_t)(len & 0xFF),
            (uint8_t)((len >> 8) & 0xFF)
        };
        uart_write_bytes(port_, header, 5);
        if (payload && len > 0) {
            uart_write_bytes(port_, payload, len);
        }
    }
    
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
    
    bool uploadSprite(uint8_t id, uint16_t w, uint16_t h, const uint8_t* rgb) {
        if (!initialized_ || !rgb) {
            ESP_LOGE(TAG_GPU, "uploadSprite: not initialized or null data");
            return false;
        }
        
        uint32_t dataSize = w * h * 3;
        uint32_t totalLen = 5 + dataSize;  // header + pixels
        
        ESP_LOGI(TAG_GPU, "[UPLOAD] Sprite %d: %dx%d = %u bytes", id, w, h, dataSize);
        
        // Build header: [id:1][w:2][h:2]
        uint8_t hdr[5] = {
            id,
            (uint8_t)(w & 0xFF), (uint8_t)((w >> 8) & 0xFF),
            (uint8_t)(h & 0xFF), (uint8_t)((h >> 8) & 0xFF)
        };
        
        // Send command header
        uint8_t cmdHdr[5] = {
            SYNC0, SYNC1, UPLOAD_SPRITE,
            (uint8_t)(totalLen & 0xFF),
            (uint8_t)((totalLen >> 8) & 0xFF)
        };
        uart_write_bytes(port_, cmdHdr, 5);
        uart_write_bytes(port_, hdr, 5);
        uart_write_bytes(port_, rgb, dataSize);
        
        // Wait for TX to complete
        uart_wait_tx_done(port_, pdMS_TO_TICKS(100));
        
        ESP_LOGI(TAG_GPU, "[UPLOAD] Complete - sent %u bytes total", totalLen + 5);
        return true;
    }
    
    void blitSprite(uint8_t id, int16_t x, int16_t y) {
        uint8_t p[5] = {
            id,
            (uint8_t)(x & 0xFF), (uint8_t)((x >> 8) & 0xFF),
            (uint8_t)(y & 0xFF), (uint8_t)((y >> 8) & 0xFF)
        };
        sendCmd(BLIT_SPRITE, p, 5);
        ESP_LOGD(TAG_BLIT, "[BLIT] Sprite %d at (%d, %d)", id, x, y);
    }
    
    void deleteSprite(uint8_t id) {
        sendCmd(DELETE_SPRITE, &id, 1);
        ESP_LOGI(TAG_GPU, "[DELETE] Sprite %d", id);
    }
};

// ============================================================
// Sprite Driver - Main Class
// ============================================================
class SpriteDriver {
private:
    // RAM cache of loaded sprites
    SpriteData sprites_[MAX_SPRITES];
    uint8_t numSprites_ = 0;
    SemaphoreHandle_t cacheMutex_ = nullptr;
    
    // Inter-core queue
    QueueHandle_t commandQueue_ = nullptr;
    
    // GPU protocol (used by Core 1)
    SpriteGpuProtocol* gpu_ = nullptr;
    
    // State
    bool initialized_ = false;
    uint8_t nextGpuSlot_ = 0;
    
    // Stats
    uint32_t spritesLoaded_ = 0;
    uint32_t spritesUploaded_ = 0;
    uint32_t blitCount_ = 0;
    
public:
    // ========================================================
    // Initialization
    // ========================================================
    
    bool init() {
        ESP_LOGI(TAG_RAM, "========================================");
        ESP_LOGI(TAG_RAM, "  SPRITE DRIVER INITIALIZATION");
        ESP_LOGI(TAG_RAM, "========================================");
        
        // Create mutex for cache access
        cacheMutex_ = xSemaphoreCreateMutex();
        if (!cacheMutex_) {
            ESP_LOGE(TAG_RAM, "Failed to create cache mutex!");
            return false;
        }
        ESP_LOGI(TAG_RAM, "[INIT] Cache mutex created");
        
        // Create command queue for Core 0 -> Core 1
        commandQueue_ = xQueueCreate(SPRITE_QUEUE_SIZE, sizeof(SpriteQueueItem));
        if (!commandQueue_) {
            ESP_LOGE(TAG_QUEUE, "Failed to create command queue!");
            return false;
        }
        ESP_LOGI(TAG_QUEUE, "[INIT] Command queue created (size=%d)", SPRITE_QUEUE_SIZE);
        
        // Clear sprite cache
        memset(sprites_, 0, sizeof(sprites_));
        numSprites_ = 0;
        
        initialized_ = true;
        ESP_LOGI(TAG_RAM, "[INIT] Sprite Driver ready");
        return true;
    }
    
    void setGpuProtocol(SpriteGpuProtocol* gpu) {
        gpu_ = gpu;
        ESP_LOGI(TAG_GPU, "[INIT] GPU protocol set: %p", gpu);
    }
    
    // ========================================================
    // Stage 1: SD Card Operations
    // ========================================================
    
    /**
     * Scan SD card for BMP files
     * Returns number of files found
     */
    int scanSdCard() {
        ESP_LOGI(TAG_SD, "========================================");
        ESP_LOGI(TAG_SD, "  STAGE 1: SCANNING SD CARD");
        ESP_LOGI(TAG_SD, "  Path: %s", SPRITES_PATH);
        ESP_LOGI(TAG_SD, "========================================");
        
        DIR* dir = opendir(SPRITES_PATH);
        if (!dir) {
            ESP_LOGE(TAG_SD, "[SCAN] Failed to open directory: %s", SPRITES_PATH);
            ESP_LOGE(TAG_SD, "[SCAN] Make sure SD card is mounted and directory exists");
            return -1;
        }
        
        int count = 0;
        struct dirent* entry;
        
        ESP_LOGI(TAG_SD, "[SCAN] Directory opened, listing files...");
        
        while ((entry = readdir(dir)) != nullptr) {
            // Skip . and ..
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }
            
            // Check if it's a BMP file
            const char* ext = strrchr(entry->d_name, '.');
            if (ext && (strcasecmp(ext, ".bmp") == 0)) {
                char fullPath[280];  // Larger buffer to avoid truncation warning
                snprintf(fullPath, sizeof(fullPath), "%s/%s", SPRITES_PATH, entry->d_name);
                
                // Get file size
                struct stat st;
                if (stat(fullPath, &st) == 0) {
                    ESP_LOGI(TAG_SD, "[SCAN] Found: %s (%d bytes)", entry->d_name, (int)st.st_size);
                    count++;
                }
            }
        }
        
        closedir(dir);
        
        ESP_LOGI(TAG_SD, "[SCAN] Complete: %d BMP files found", count);
        return count;
    }
    
    /**
     * Load a BMP file from SD card into RAM
     */
    bool loadBmpFromSd(const char* filename, uint8_t spriteId) {
        ESP_LOGI(TAG_SD, "========================================");
        ESP_LOGI(TAG_SD, "  STAGE 1: LOADING BMP FILE");
        ESP_LOGI(TAG_SD, "  File: %s -> Sprite ID %d", filename, spriteId);
        ESP_LOGI(TAG_SD, "========================================");
        
        // Build full path
        char fullPath[128];
        if (filename[0] == '/') {
            strncpy(fullPath, filename, sizeof(fullPath));
        } else {
            snprintf(fullPath, sizeof(fullPath), "%s/%s", SPRITES_PATH, filename);
        }
        
        ESP_LOGI(TAG_SD, "[LOAD] Opening: %s", fullPath);
        
        // Open file
        FILE* file = fopen(fullPath, "rb");
        if (!file) {
            ESP_LOGE(TAG_SD, "[LOAD] FAILED to open file!");
            return false;
        }
        
        // Get file size
        fseek(file, 0, SEEK_END);
        long fileSize = ftell(file);
        fseek(file, 0, SEEK_SET);
        ESP_LOGI(TAG_SD, "[LOAD] File size: %ld bytes", fileSize);
        
        // Read BMP file header
        BMPFileHeader fileHdr;
        if (fread(&fileHdr, sizeof(fileHdr), 1, file) != 1) {
            ESP_LOGE(TAG_SD, "[LOAD] Failed to read file header!");
            fclose(file);
            return false;
        }
        
        // Validate signature
        if (fileHdr.signature != 0x4D42) {  // 'BM'
            ESP_LOGE(TAG_SD, "[LOAD] Invalid BMP signature: 0x%04X (expected 0x4D42)", fileHdr.signature);
            fclose(file);
            return false;
        }
        ESP_LOGI(TAG_SD, "[LOAD] BMP signature valid");
        ESP_LOGI(TAG_SD, "[LOAD] Data offset: %u bytes", fileHdr.dataOffset);
        
        // Read info header
        BMPInfoHeader infoHdr;
        if (fread(&infoHdr, sizeof(infoHdr), 1, file) != 1) {
            ESP_LOGE(TAG_SD, "[LOAD] Failed to read info header!");
            fclose(file);
            return false;
        }
        
        // Log BMP properties
        int width = infoHdr.width;
        int height = infoHdr.height;
        bool topDown = (height < 0);
        if (height < 0) height = -height;
        
        ESP_LOGI(TAG_SD, "[LOAD] Dimensions: %d x %d", width, height);
        ESP_LOGI(TAG_SD, "[LOAD] Bits/pixel: %d", infoHdr.bitsPerPixel);
        ESP_LOGI(TAG_SD, "[LOAD] Compression: %u", infoHdr.compression);
        ESP_LOGI(TAG_SD, "[LOAD] Row order: %s", topDown ? "top-down" : "bottom-up");
        
        // Validate format
        if (infoHdr.bitsPerPixel != 24 && infoHdr.bitsPerPixel != 32) {
            ESP_LOGE(TAG_SD, "[LOAD] Unsupported bit depth: %d (need 24 or 32)", infoHdr.bitsPerPixel);
            fclose(file);
            return false;
        }
        
        if (infoHdr.compression != 0) {
            ESP_LOGE(TAG_SD, "[LOAD] Compressed BMPs not supported!");
            fclose(file);
            return false;
        }
        
        // Validate size
        if (width <= 0 || height <= 0 || width > 256 || height > 256) {
            ESP_LOGE(TAG_SD, "[LOAD] Invalid dimensions: %dx%d (max 256x256)", width, height);
            fclose(file);
            return false;
        }
        
        // Now we proceed to Stage 2: Load into RAM
        return loadBmpToRam(file, &fileHdr, &infoHdr, spriteId, fullPath, fileSize);
    }
    
    // ========================================================
    // Stage 2: RAM Storage
    // ========================================================
    
private:
    bool loadBmpToRam(FILE* file, BMPFileHeader* fileHdr, BMPInfoHeader* infoHdr,
                      uint8_t spriteId, const char* filePath, long fileSize) {
        ESP_LOGI(TAG_RAM, "========================================");
        ESP_LOGI(TAG_RAM, "  STAGE 2: LOADING TO RAM");
        ESP_LOGI(TAG_RAM, "  Sprite ID: %d", spriteId);
        ESP_LOGI(TAG_RAM, "========================================");
        
        int width = infoHdr->width;
        int height = abs(infoHdr->height);
        bool topDown = (infoHdr->height < 0);
        int bpp = infoHdr->bitsPerPixel;
        int bytesPerPixel = bpp / 8;
        
        // Calculate row padding (BMP rows are 4-byte aligned)
        int rowSize = width * bytesPerPixel;
        int padding = (4 - (rowSize % 4)) % 4;
        int rowSizeWithPadding = rowSize + padding;
        
        ESP_LOGI(TAG_RAM, "[RAM] Row size: %d + %d padding = %d bytes", rowSize, padding, rowSizeWithPadding);
        
        // Allocate RGB888 buffer
        uint32_t pixelDataSize = width * height * 3;  // RGB888
        ESP_LOGI(TAG_RAM, "[RAM] Allocating %u bytes for RGB888 data", pixelDataSize);
        
        uint8_t* pixels = (uint8_t*)malloc(pixelDataSize);
        if (!pixels) {
            ESP_LOGE(TAG_RAM, "[RAM] FAILED to allocate pixel buffer!");
            fclose(file);
            return false;
        }
        ESP_LOGI(TAG_RAM, "[RAM] Pixel buffer allocated at %p", pixels);
        
        // Allocate row buffer
        uint8_t* rowBuf = (uint8_t*)malloc(rowSizeWithPadding);
        if (!rowBuf) {
            ESP_LOGE(TAG_RAM, "[RAM] FAILED to allocate row buffer!");
            free(pixels);
            fclose(file);
            return false;
        }
        
        // Seek to pixel data
        fseek(file, fileHdr->dataOffset, SEEK_SET);
        ESP_LOGI(TAG_RAM, "[RAM] Seeking to pixel data at offset %u", fileHdr->dataOffset);
        
        // Read and convert pixel data
        ESP_LOGI(TAG_RAM, "[RAM] Reading %d rows...", height);
        
        for (int y = 0; y < height; y++) {
            // Read one row
            if (fread(rowBuf, rowSizeWithPadding, 1, file) != 1) {
                ESP_LOGE(TAG_RAM, "[RAM] Failed to read row %d!", y);
                free(pixels);
                free(rowBuf);
                fclose(file);
                return false;
            }
            
            // Calculate destination row (BMP is usually bottom-up)
            int destY = topDown ? y : (height - 1 - y);
            uint8_t* destRow = pixels + (destY * width * 3);
            
            // Convert BGR(A) to RGB
            for (int x = 0; x < width; x++) {
                uint8_t* src = rowBuf + (x * bytesPerPixel);
                uint8_t* dst = destRow + (x * 3);
                
                // BMP stores BGR, we need RGB
                dst[0] = src[2];  // R <- B
                dst[1] = src[1];  // G <- G
                dst[2] = src[0];  // B <- R
            }
        }
        
        free(rowBuf);
        fclose(file);
        
        ESP_LOGI(TAG_RAM, "[RAM] Pixel data converted to RGB888");
        
        // Store in cache
        if (!xSemaphoreTake(cacheMutex_, pdMS_TO_TICKS(100))) {
            ESP_LOGE(TAG_RAM, "[RAM] Failed to acquire cache mutex!");
            free(pixels);
            return false;
        }
        
        // Find or create slot
        int slot = -1;
        for (int i = 0; i < MAX_SPRITES; i++) {
            if (sprites_[i].id == spriteId || sprites_[i].pixels == nullptr) {
                slot = i;
                break;
            }
        }
        
        if (slot < 0) {
            ESP_LOGE(TAG_RAM, "[RAM] No free sprite slots!");
            xSemaphoreGive(cacheMutex_);
            free(pixels);
            return false;
        }
        
        // Free existing data if overwriting
        if (sprites_[slot].pixels) {
            free(sprites_[slot].pixels);
        }
        
        // Store sprite data
        SpriteData& sprite = sprites_[slot];
        sprite.id = spriteId;
        sprite.width = width;
        sprite.height = height;
        sprite.dataSize = pixelDataSize;
        sprite.pixels = pixels;
        sprite.inGpuCache = false;
        sprite.gpuSlot = 0xFF;
        sprite.fileSize = fileSize;
        sprite.loadedTimeMs = (uint32_t)(esp_timer_get_time() / 1000);
        strncpy(sprite.filePath, filePath, sizeof(sprite.filePath) - 1);
        
        // Extract name from filename
        const char* basename = strrchr(filePath, '/');
        if (basename) basename++;
        else basename = filePath;
        strncpy(sprite.name, basename, sizeof(sprite.name) - 1);
        // Remove extension
        char* dot = strrchr(sprite.name, '.');
        if (dot) *dot = '\0';
        
        if (slot >= numSprites_) {
            numSprites_ = slot + 1;
        }
        spritesLoaded_++;
        
        xSemaphoreGive(cacheMutex_);
        
        ESP_LOGI(TAG_RAM, "========================================");
        ESP_LOGI(TAG_RAM, "  STAGE 2: COMPLETE");
        ESP_LOGI(TAG_RAM, "  Sprite '%s' loaded to RAM", sprite.name);
        ESP_LOGI(TAG_RAM, "  ID: %d, Size: %dx%d", sprite.id, sprite.width, sprite.height);
        ESP_LOGI(TAG_RAM, "  Data: %u bytes at %p", sprite.dataSize, sprite.pixels);
        ESP_LOGI(TAG_RAM, "  Cache slot: %d", slot);
        ESP_LOGI(TAG_RAM, "========================================");
        
        return true;
    }
    
public:
    // ========================================================
    // Stage 3: Inter-Core Queue (Core 0 -> Core 1)
    // ========================================================
    
    /**
     * Queue sprite for upload to GPU (called from Core 0)
     */
    bool queueSpriteUpload(uint8_t spriteId) {
        ESP_LOGI(TAG_QUEUE, "========================================");
        ESP_LOGI(TAG_QUEUE, "  STAGE 3: QUEUING SPRITE UPLOAD");
        ESP_LOGI(TAG_QUEUE, "  Sprite ID: %d", spriteId);
        ESP_LOGI(TAG_QUEUE, "========================================");
        
        if (!commandQueue_) {
            ESP_LOGE(TAG_QUEUE, "[QUEUE] Command queue not initialized!");
            return false;
        }
        
        // Find sprite in cache
        SpriteData* sprite = nullptr;
        if (xSemaphoreTake(cacheMutex_, pdMS_TO_TICKS(100))) {
            for (int i = 0; i < numSprites_; i++) {
                if (sprites_[i].id == spriteId && sprites_[i].pixels) {
                    sprite = &sprites_[i];
                    break;
                }
            }
            
            if (!sprite) {
                ESP_LOGE(TAG_QUEUE, "[QUEUE] Sprite ID %d not found in cache!", spriteId);
                xSemaphoreGive(cacheMutex_);
                return false;
            }
            
            ESP_LOGI(TAG_QUEUE, "[QUEUE] Found sprite '%s' (%dx%d, %u bytes)",
                     sprite->name, sprite->width, sprite->height, sprite->dataSize);
            
            // Create copy of pixel data for Core 1
            // (Core 1 will free this after upload)
            uint8_t* pixelsCopy = (uint8_t*)malloc(sprite->dataSize);
            if (!pixelsCopy) {
                ESP_LOGE(TAG_QUEUE, "[QUEUE] Failed to allocate pixel copy!");
                xSemaphoreGive(cacheMutex_);
                return false;
            }
            memcpy(pixelsCopy, sprite->pixels, sprite->dataSize);
            ESP_LOGI(TAG_QUEUE, "[QUEUE] Copied %u bytes to %p", sprite->dataSize, pixelsCopy);
            
            // Build queue item
            SpriteQueueItem item;
            memset(&item, 0, sizeof(item));
            item.cmd = SpriteCommand::UPLOAD;
            item.spriteId = spriteId;
            item.width = sprite->width;
            item.height = sprite->height;
            item.pixelsCopy = pixelsCopy;
            item.pixelsSize = sprite->dataSize;
            
            xSemaphoreGive(cacheMutex_);
            
            // Send to queue
            if (xQueueSend(commandQueue_, &item, pdMS_TO_TICKS(100)) != pdTRUE) {
                ESP_LOGE(TAG_QUEUE, "[QUEUE] Failed to send to queue (full?)");
                free(pixelsCopy);
                return false;
            }
            
            ESP_LOGI(TAG_QUEUE, "[QUEUE] Upload command queued successfully");
            return true;
        }
        
        ESP_LOGE(TAG_QUEUE, "[QUEUE] Failed to acquire cache mutex!");
        return false;
    }
    
    /**
     * Queue sprite blit command (called from Core 0)
     */
    bool queueSpriteBlit(uint8_t spriteId, int16_t x, int16_t y,
                         uint8_t bgR = 0, uint8_t bgG = 0, uint8_t bgB = 0) {
        ESP_LOGI(TAG_QUEUE, "[QUEUE] Queuing BLIT: sprite=%d pos=(%d,%d) bg=(%d,%d,%d)",
                 spriteId, x, y, bgR, bgG, bgB);
        
        if (!commandQueue_) {
            ESP_LOGE(TAG_QUEUE, "[QUEUE] Command queue not initialized!");
            return false;
        }
        
        SpriteQueueItem item;
        memset(&item, 0, sizeof(item));
        item.cmd = SpriteCommand::BLIT;
        item.spriteId = spriteId;
        item.x = x;
        item.y = y;
        item.bgR = bgR;
        item.bgG = bgG;
        item.bgB = bgB;
        item.pixelsCopy = nullptr;
        
        if (xQueueSend(commandQueue_, &item, pdMS_TO_TICKS(100)) != pdTRUE) {
            ESP_LOGE(TAG_QUEUE, "[QUEUE] Failed to queue blit command!");
            return false;
        }
        
        ESP_LOGI(TAG_QUEUE, "[QUEUE] Blit command queued");
        return true;
    }
    
    /**
     * Queue upload AND blit (convenience)
     */
    bool queueUploadAndBlit(uint8_t spriteId, int16_t x, int16_t y,
                            uint8_t bgR = 0, uint8_t bgG = 0, uint8_t bgB = 0) {
        ESP_LOGI(TAG_QUEUE, "========================================");
        ESP_LOGI(TAG_QUEUE, "  STAGE 3: UPLOAD AND BLIT");
        ESP_LOGI(TAG_QUEUE, "========================================");
        
        if (!queueSpriteUpload(spriteId)) {
            return false;
        }
        return queueSpriteBlit(spriteId, x, y, bgR, bgG, bgB);
    }
    
    // ========================================================
    // Stage 4 & 5: GPU Upload and Blit (Called from Core 1)
    // ========================================================
    
    /**
     * Process pending commands (called from Core 1's render loop)
     */
    void processCommands() {
        if (!commandQueue_ || !gpu_) return;
        
        SpriteQueueItem item;
        
        // Process all pending commands
        while (xQueueReceive(commandQueue_, &item, 0) == pdTRUE) {
            switch (item.cmd) {
                case SpriteCommand::UPLOAD:
                    processUploadCommand(item);
                    break;
                    
                case SpriteCommand::BLIT:
                    processBlitCommand(item);
                    break;
                    
                case SpriteCommand::DELETE:
                    gpu_->deleteSprite(item.spriteId);
                    break;
                    
                case SpriteCommand::CLEAR:
                    gpu_->setTarget(0);
                    gpu_->clear(item.bgR, item.bgG, item.bgB);
                    gpu_->present();
                    break;
            }
        }
    }
    
private:
    void processUploadCommand(SpriteQueueItem& item) {
        ESP_LOGI(TAG_GPU, "========================================");
        ESP_LOGI(TAG_GPU, "  STAGE 4: GPU UPLOAD (Core 1)");
        ESP_LOGI(TAG_GPU, "  Sprite ID: %d", item.spriteId);
        ESP_LOGI(TAG_GPU, "  Dimensions: %dx%d", item.width, item.height);
        ESP_LOGI(TAG_GPU, "  Data size: %u bytes", item.pixelsSize);
        ESP_LOGI(TAG_GPU, "  Data ptr: %p", item.pixelsCopy);
        ESP_LOGI(TAG_GPU, "========================================");
        
        if (!item.pixelsCopy || !gpu_) {
            ESP_LOGE(TAG_GPU, "[GPU] Invalid data or GPU not set!");
            if (item.pixelsCopy) free(item.pixelsCopy);
            return;
        }
        
        // Assign GPU slot (use sprite ID directly for simplicity)
        uint8_t gpuSlot = item.spriteId % MAX_GPU_SPRITES;
        
        ESP_LOGI(TAG_GPU, "[GPU] Uploading to GPU slot %d...", gpuSlot);
        
        // Upload to GPU
        bool success = gpu_->uploadSprite(gpuSlot, item.width, item.height, item.pixelsCopy);
        
        if (success) {
            ESP_LOGI(TAG_GPU, "[GPU] Upload SUCCESS!");
            spritesUploaded_++;
            
            // Update cache metadata
            if (xSemaphoreTake(cacheMutex_, pdMS_TO_TICKS(50))) {
                for (int i = 0; i < numSprites_; i++) {
                    if (sprites_[i].id == item.spriteId) {
                        sprites_[i].inGpuCache = true;
                        sprites_[i].gpuSlot = gpuSlot;
                        sprites_[i].uploadedTimeMs = (uint32_t)(esp_timer_get_time() / 1000);
                        break;
                    }
                }
                xSemaphoreGive(cacheMutex_);
            }
        } else {
            ESP_LOGE(TAG_GPU, "[GPU] Upload FAILED!");
        }
        
        // Free the pixel copy
        free(item.pixelsCopy);
        item.pixelsCopy = nullptr;
        
        ESP_LOGI(TAG_GPU, "[GPU] Stage 4 complete");
    }
    
    void processBlitCommand(SpriteQueueItem& item) {
        ESP_LOGI(TAG_BLIT, "========================================");
        ESP_LOGI(TAG_BLIT, "  STAGE 5: GPU BLIT (Core 1)");
        ESP_LOGI(TAG_BLIT, "  Sprite ID: %d at (%d, %d)", item.spriteId, item.x, item.y);
        ESP_LOGI(TAG_BLIT, "  Background: RGB(%d, %d, %d)", item.bgR, item.bgG, item.bgB);
        ESP_LOGI(TAG_BLIT, "========================================");
        
        if (!gpu_) {
            ESP_LOGE(TAG_BLIT, "[BLIT] GPU not set!");
            return;
        }
        
        // Find GPU slot for this sprite
        uint8_t gpuSlot = item.spriteId % MAX_GPU_SPRITES;
        
        // Check if sprite is in GPU cache
        bool inCache = false;
        if (xSemaphoreTake(cacheMutex_, pdMS_TO_TICKS(50))) {
            for (int i = 0; i < numSprites_; i++) {
                if (sprites_[i].id == item.spriteId) {
                    inCache = sprites_[i].inGpuCache;
                    gpuSlot = sprites_[i].gpuSlot;
                    break;
                }
            }
            xSemaphoreGive(cacheMutex_);
        }
        
        if (!inCache) {
            ESP_LOGW(TAG_BLIT, "[BLIT] Sprite %d not in GPU cache! Skipping blit.", item.spriteId);
            return;
        }
        
        // Target HUB75
        gpu_->setTarget(0);
        
        // Clear with background
        gpu_->clear(item.bgR, item.bgG, item.bgB);
        
        // Blit sprite
        ESP_LOGI(TAG_BLIT, "[BLIT] Blitting GPU slot %d at (%d, %d)", gpuSlot, item.x, item.y);
        gpu_->blitSprite(gpuSlot, item.x, item.y);
        
        // Present
        gpu_->present();
        
        blitCount_++;
        ESP_LOGI(TAG_BLIT, "[BLIT] Stage 5 complete (total blits: %u)", blitCount_);
    }
    
public:
    // ========================================================
    // Query Methods
    // ========================================================
    
    /**
     * Get sprite from cache
     */
    SpriteData* getSprite(uint8_t spriteId) {
        for (int i = 0; i < numSprites_; i++) {
            if (sprites_[i].id == spriteId && sprites_[i].pixels) {
                return &sprites_[i];
            }
        }
        return nullptr;
    }
    
    /**
     * Check if sprite is loaded in RAM
     */
    bool isLoaded(uint8_t spriteId) {
        for (int i = 0; i < numSprites_; i++) {
            if (sprites_[i].id == spriteId && sprites_[i].pixels) {
                return true;
            }
        }
        return false;
    }
    
    /**
     * Check if sprite is in GPU cache
     */
    bool isInGpuCache(uint8_t spriteId) {
        for (int i = 0; i < numSprites_; i++) {
            if (sprites_[i].id == spriteId) {
                return sprites_[i].inGpuCache;
            }
        }
        return false;
    }
    
    /**
     * Get number of loaded sprites
     */
    int getLoadedCount() const { return numSprites_; }
    
    /**
     * Print debug status
     */
    void printStatus() {
        ESP_LOGI(TAG_RAM, "========================================");
        ESP_LOGI(TAG_RAM, "  SPRITE DRIVER STATUS");
        ESP_LOGI(TAG_RAM, "========================================");
        ESP_LOGI(TAG_RAM, "  Initialized: %s", initialized_ ? "YES" : "NO");
        ESP_LOGI(TAG_RAM, "  Sprites in RAM: %d / %d", numSprites_, MAX_SPRITES);
        ESP_LOGI(TAG_RAM, "  Total loaded: %u", spritesLoaded_);
        ESP_LOGI(TAG_RAM, "  Total uploaded: %u", spritesUploaded_);
        ESP_LOGI(TAG_RAM, "  Total blits: %u", blitCount_);
        ESP_LOGI(TAG_RAM, "----------------------------------------");
        
        for (int i = 0; i < numSprites_; i++) {
            if (sprites_[i].pixels) {
                ESP_LOGI(TAG_RAM, "  [%d] ID=%d '%s' %dx%d %uB GPU=%s slot=%d",
                         i, sprites_[i].id, sprites_[i].name,
                         sprites_[i].width, sprites_[i].height, sprites_[i].dataSize,
                         sprites_[i].inGpuCache ? "YES" : "NO",
                         sprites_[i].gpuSlot);
            }
        }
        ESP_LOGI(TAG_RAM, "========================================");
    }
    
    /**
     * Load all BMP files from SD card
     */
    int loadAllFromSd() {
        ESP_LOGI(TAG_SD, "========================================");
        ESP_LOGI(TAG_SD, "  LOADING ALL SPRITES FROM SD CARD");
        ESP_LOGI(TAG_SD, "========================================");
        
        DIR* dir = opendir(SPRITES_PATH);
        if (!dir) {
            ESP_LOGE(TAG_SD, "[LOAD-ALL] Failed to open: %s", SPRITES_PATH);
            return -1;
        }
        
        int loaded = 0;
        uint8_t nextId = 1;
        struct dirent* entry;
        
        while ((entry = readdir(dir)) != nullptr && nextId < MAX_SPRITES) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }
            
            const char* ext = strrchr(entry->d_name, '.');
            if (ext && strcasecmp(ext, ".bmp") == 0) {
                if (loadBmpFromSd(entry->d_name, nextId)) {
                    loaded++;
                    nextId++;
                }
            }
        }
        
        closedir(dir);
        
        ESP_LOGI(TAG_SD, "[LOAD-ALL] Complete: %d sprites loaded", loaded);
        return loaded;
    }
};

// ============================================================
// Global Instance
// ============================================================
inline SpriteDriver& getSpriteDriver() {
    static SpriteDriver instance;
    return instance;
}

} // namespace Sprites
} // namespace Application

#endif // ARCOS_APPLICATION_SPRITES_DRIVER_HPP_
