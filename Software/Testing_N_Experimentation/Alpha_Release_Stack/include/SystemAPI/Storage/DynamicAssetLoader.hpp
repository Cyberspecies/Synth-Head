/*****************************************************************
 * File:      DynamicAssetLoader.hpp
 * Category:  include/SystemAPI/Storage
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Dynamic asset loader for large data stored on SD card.
 *    Loads assets (sprites, animations) into RAM only when needed
 *    and unloads them to free memory when not in use.
 *    
 * Features:
 *    - LRU cache for recently used assets
 *    - Async loading with callbacks
 *    - Memory pressure handling
 *    - Smooth loading transitions
 *****************************************************************/

#ifndef ARCOS_INCLUDE_SYSTEMAPI_STORAGE_DYNAMIC_ASSET_LOADER_HPP_
#define ARCOS_INCLUDE_SYSTEMAPI_STORAGE_DYNAMIC_ASSET_LOADER_HPP_

#include "SystemAPI/Utils/FileSystemService.hpp"
#include "StorageManager.hpp"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include <cstdint>
#include <cstring>
#include <functional>
#include <vector>

namespace SystemAPI {
namespace Storage {

static const char* LOADER_TAG = "AssetLoader";

// ============================================================
// Asset Types
// ============================================================

enum class AssetType : uint8_t {
    SPRITE,
    ANIMATION,
    CONFIG,
    EQUATION,
    CUSTOM
};

// ============================================================
// Loaded Asset Handle
// ============================================================

struct LoadedAsset {
    int id = -1;                    // Asset ID
    AssetType type = AssetType::SPRITE;
    char name[32] = {0};            // Asset name
    uint8_t* data = nullptr;        // Loaded data (owned by loader)
    size_t size = 0;                // Data size in bytes
    uint32_t lastAccess = 0;        // Last access timestamp (for LRU)
    bool loaded = false;            // Whether data is in RAM
    bool loading = false;           // Loading in progress
    
    void clear() {
        if (data) {
            free(data);
            data = nullptr;
        }
        size = 0;
        loaded = false;
        loading = false;
    }
};

// ============================================================
// Sprite Data Structure (for loading)
// ============================================================

struct SpriteAsset {
    int id = 0;
    char name[32] = {0};
    int width = 64;
    int height = 32;
    int scale = 100;
    uint8_t* pixels = nullptr;      // RGB pixel data
    size_t pixelSize = 0;           // width * height * 3
    char preview[4096] = {0};       // base64 thumbnail
    
    void free() {
        if (pixels) {
            ::free(pixels);
            pixels = nullptr;
        }
        pixelSize = 0;
    }
};

// ============================================================
// Load Callbacks
// ============================================================

using AssetLoadCallback = std::function<void(bool success, LoadedAsset* asset)>;
using SpriteLoadCallback = std::function<void(bool success, SpriteAsset* sprite)>;

// ============================================================
// Dynamic Asset Loader (Singleton)
// ============================================================

class DynamicAssetLoader {
private:
    static constexpr int MAX_CACHED_ASSETS = 8;   // Max assets in RAM
    static constexpr size_t MAX_CACHE_BYTES = 256 * 1024;  // 256KB cache limit
    
    LoadedAsset cache_[MAX_CACHED_ASSETS];
    size_t totalCacheBytes_ = 0;
    SemaphoreHandle_t mutex_ = nullptr;
    bool initialized_ = false;
    
    // Currently loading sprite (for async operations)
    SpriteAsset currentSprite_;
    SpriteLoadCallback spriteCallback_ = nullptr;
    
    DynamicAssetLoader() {
        mutex_ = xSemaphoreCreateMutex();
    }
    
public:
    ~DynamicAssetLoader() {
        if (mutex_) vSemaphoreDelete(mutex_);
        clearCache();
    }
    
    // Delete copy/move
    DynamicAssetLoader(const DynamicAssetLoader&) = delete;
    DynamicAssetLoader& operator=(const DynamicAssetLoader&) = delete;
    
    /** Get singleton instance */
    static DynamicAssetLoader& instance() {
        static DynamicAssetLoader inst;
        return inst;
    }
    
    /** Initialize the loader */
    bool init() {
        if (initialized_) return true;
        
        // Clear cache slots
        for (int i = 0; i < MAX_CACHED_ASSETS; i++) {
            cache_[i] = LoadedAsset{};
        }
        
        initialized_ = true;
        ESP_LOGI(LOADER_TAG, "Dynamic asset loader initialized");
        return true;
    }
    
    // ========== Sprite Loading ==========
    
    /**
     * @brief Load sprite from SD card
     * @param spriteId Sprite ID to load
     * @param outSprite Output sprite structure (pixels allocated by loader)
     * @return true if loaded successfully
     */
    bool loadSprite(int spriteId, SpriteAsset* outSprite) {
        if (!outSprite) return false;
        
        auto& storage = StorageManager::instance();
        if (!storage.hasSDCard()) {
            ESP_LOGW(LOADER_TAG, "SD card not available for sprite loading");
            return false;
        }
        
        // Build sprite file path
        char path[64];
        snprintf(path, sizeof(path), "%s/%d.json", Paths::SPRITES_DIR, spriteId);
        
        // Load JSON metadata
        char* json = nullptr;
        size_t size = 0;
        if (!storage.loadJson(path, &json, &size) || !json) {
            ESP_LOGE(LOADER_TAG, "Failed to load sprite %d", spriteId);
            return false;
        }
        
        bool success = parseSpriteJson(json, outSprite);
        free(json);
        
        if (success) {
            outSprite->id = spriteId;
            
            // Load pixel data if exists
            char pixelPath[64];
            snprintf(pixelPath, sizeof(pixelPath), "%s/%d.bin", Paths::SPRITES_DIR, spriteId);
            
            char* pixelData = nullptr;
            size_t pixelSize = 0;
            if (Utils::FileSystemService::instance().readFile(pixelPath, &pixelData, &pixelSize)) {
                outSprite->pixels = (uint8_t*)pixelData;
                outSprite->pixelSize = pixelSize;
            }
            
            ESP_LOGI(LOADER_TAG, "Loaded sprite %d: %s (%dx%d)", 
                     spriteId, outSprite->name, outSprite->width, outSprite->height);
        }
        
        return success;
    }
    
    /**
     * @brief Save sprite to SD card
     * @param sprite Sprite to save
     * @return true on success
     */
    bool saveSprite(const SpriteAsset& sprite) {
        auto& storage = StorageManager::instance();
        if (!storage.hasSDCard()) return false;
        
        // Build JSON
        cJSON* root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "name", sprite.name);
        cJSON_AddNumberToObject(root, "width", sprite.width);
        cJSON_AddNumberToObject(root, "height", sprite.height);
        cJSON_AddNumberToObject(root, "scale", sprite.scale);
        cJSON_AddStringToObject(root, "preview", sprite.preview);
        
        char* json = cJSON_Print(root);
        cJSON_Delete(root);
        
        if (!json) return false;
        
        // Save metadata JSON
        char path[64];
        snprintf(path, sizeof(path), "%s/%d.json", Paths::SPRITES_DIR, sprite.id);
        bool success = Utils::FileSystemService::instance().writeFile(path, json, strlen(json));
        free(json);
        
        // Save pixel data if present
        if (success && sprite.pixels && sprite.pixelSize > 0) {
            char pixelPath[64];
            snprintf(pixelPath, sizeof(pixelPath), "%s/%d.bin", Paths::SPRITES_DIR, sprite.id);
            success = Utils::FileSystemService::instance().writeFile(
                pixelPath, (const char*)sprite.pixels, sprite.pixelSize);
        }
        
        if (success) {
            ESP_LOGI(LOADER_TAG, "Saved sprite %d: %s", sprite.id, sprite.name);
        }
        
        return success;
    }
    
    /**
     * @brief Delete sprite from SD card
     */
    bool deleteSprite(int spriteId) {
        auto& storage = StorageManager::instance();
        if (!storage.hasSDCard()) return false;
        
        char path[64];
        snprintf(path, sizeof(path), "%s/%d.json", Paths::SPRITES_DIR, spriteId);
        Utils::FileSystemService::instance().deleteFile(path);
        
        char pixelPath[64];
        snprintf(pixelPath, sizeof(pixelPath), "%s/%d.bin", Paths::SPRITES_DIR, spriteId);
        Utils::FileSystemService::instance().deleteFile(pixelPath);
        
        ESP_LOGI(LOADER_TAG, "Deleted sprite %d", spriteId);
        return true;
    }
    
    /**
     * @brief Unload sprite from RAM
     */
    void unloadSprite(SpriteAsset* sprite) {
        if (sprite) {
            sprite->free();
        }
    }
    
    // ========== Cache Management ==========
    
    /**
     * @brief Clear all cached assets from RAM
     */
    void clearCache() {
        if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(100)) == pdTRUE) {
            for (int i = 0; i < MAX_CACHED_ASSETS; i++) {
                cache_[i].clear();
            }
            totalCacheBytes_ = 0;
            xSemaphoreGive(mutex_);
        }
        ESP_LOGI(LOADER_TAG, "Asset cache cleared");
    }
    
    /**
     * @brief Get current cache usage
     */
    size_t getCacheUsage() const { return totalCacheBytes_; }
    
    /**
     * @brief Get max cache size
     */
    size_t getMaxCacheSize() const { return MAX_CACHE_BYTES; }
    
    /**
     * @brief Evict least recently used assets to free memory
     */
    void evictLRU(size_t bytesNeeded) {
        if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(100)) != pdTRUE) return;
        
        while (totalCacheBytes_ + bytesNeeded > MAX_CACHE_BYTES) {
            // Find LRU asset
            int lruIdx = -1;
            uint32_t oldestTime = UINT32_MAX;
            
            for (int i = 0; i < MAX_CACHED_ASSETS; i++) {
                if (cache_[i].loaded && cache_[i].lastAccess < oldestTime) {
                    oldestTime = cache_[i].lastAccess;
                    lruIdx = i;
                }
            }
            
            if (lruIdx >= 0) {
                ESP_LOGI(LOADER_TAG, "Evicting asset %d from cache", cache_[lruIdx].id);
                totalCacheBytes_ -= cache_[lruIdx].size;
                cache_[lruIdx].clear();
            } else {
                break;  // No more assets to evict
            }
        }
        
        xSemaphoreGive(mutex_);
    }

private:
    /**
     * @brief Parse sprite JSON data
     */
    bool parseSpriteJson(const char* json, SpriteAsset* outSprite) {
        cJSON* root = cJSON_Parse(json);
        if (!root) return false;
        
        cJSON* name = cJSON_GetObjectItem(root, "name");
        if (name && cJSON_IsString(name)) {
            strncpy(outSprite->name, name->valuestring, sizeof(outSprite->name) - 1);
        }
        
        cJSON* width = cJSON_GetObjectItem(root, "width");
        if (width) outSprite->width = width->valueint;
        
        cJSON* height = cJSON_GetObjectItem(root, "height");
        if (height) outSprite->height = height->valueint;
        
        cJSON* scale = cJSON_GetObjectItem(root, "scale");
        if (scale) outSprite->scale = scale->valueint;
        
        cJSON* preview = cJSON_GetObjectItem(root, "preview");
        if (preview && cJSON_IsString(preview)) {
            strncpy(outSprite->preview, preview->valuestring, sizeof(outSprite->preview) - 1);
        }
        
        cJSON_Delete(root);
        return true;
    }
    
    /**
     * @brief Get current timestamp for LRU tracking
     */
    uint32_t getTimestamp() {
        return xTaskGetTickCount() * portTICK_PERIOD_MS;
    }
};

} // namespace Storage
} // namespace SystemAPI

#endif // ARCOS_INCLUDE_SYSTEMAPI_STORAGE_DYNAMIC_ASSET_LOADER_HPP_
