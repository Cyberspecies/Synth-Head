/*****************************************************************
 * File:      SpriteStorage.hpp
 * Category:  include/SystemAPI/Storage
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Simplified, robust sprite storage manager using
 *    FileSystemService for all SD card operations.
 *    
 * Design Goals:
 *    - Single responsibility: only handles sprite persistence
 *    - Uses FileSystemService for all file operations
 *    - Atomic writes via temp file + rename
 *    - Automatic recovery of orphaned pixel files
 *    - No raw fopen/fclose in this module
 *****************************************************************/

#ifndef ARCOS_INCLUDE_SYSTEMAPI_STORAGE_SPRITE_STORAGE_HPP_
#define ARCOS_INCLUDE_SYSTEMAPI_STORAGE_SPRITE_STORAGE_HPP_

#include "SystemAPI/Utils/FileSystemService.hpp"
#include "cJSON.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include <string>
#include <vector>
#include <cmath>
#include <sys/stat.h>
#include <dirent.h>
#include <cstring>

namespace SystemAPI {
namespace Storage {

static const char* SPRITE_STORAGE_TAG = "SpriteStorage";

// ============================================================
// Sprite Data Structure
// ============================================================

/** Sprite metadata and pixel data */
struct SpriteData {
    int id = 0;
    std::string name;
    int width = 64;
    int height = 32;
    int scale = 100;
    std::string preview;           // Base64 PNG thumbnail for web UI
    std::vector<uint8_t> pixels;   // Raw RGB888 pixel data
    bool loaded = false;           // Whether pixels are loaded from disk
    
    /** Get expected pixel data size (RGB888) */
    size_t expectedPixelSize() const {
        return (size_t)width * height * 3;
    }
    
    /** Check if pixel data is valid */
    bool hasValidPixels() const {
        return !pixels.empty() && pixels.size() == expectedPixelSize();
    }
};

// ============================================================
// Sprite Storage Service (Singleton)
// ============================================================

/**
 * @brief Robust sprite storage manager
 * 
 * Handles all sprite persistence with proper error handling,
 * atomic writes, and automatic recovery mechanisms.
 */
class SpriteStorage {
private:
    static constexpr const char* SPRITES_DIR = "/sdcard/Sprites";
    static constexpr const char* INDEX_FILE = "/sdcard/Sprites/index.json";
    static constexpr const char* TEMP_INDEX = "/sdcard/Sprites/index_new.json";
    
    std::vector<SpriteData> sprites_;
    int nextId_ = 1;
    bool initialized_ = false;
    SemaphoreHandle_t mutex_ = nullptr;
    
    SpriteStorage() {
        mutex_ = xSemaphoreCreateMutex();
    }
    
    ~SpriteStorage() {
        if (mutex_) vSemaphoreDelete(mutex_);
    }
    
    /** RAII lock guard */
    class Lock {
        SemaphoreHandle_t mtx_;
    public:
        explicit Lock(SemaphoreHandle_t m) : mtx_(m) {
            if (mtx_) xSemaphoreTake(mtx_, portMAX_DELAY);
        }
        ~Lock() { if (mtx_) xSemaphoreGive(mtx_); }
    };
    
public:
    // Delete copy/move
    SpriteStorage(const SpriteStorage&) = delete;
    SpriteStorage& operator=(const SpriteStorage&) = delete;
    
    /** Get singleton instance */
    static SpriteStorage& instance() {
        static SpriteStorage inst;
        return inst;
    }
    
    // ========== Initialization ==========
    
    /**
     * @brief Initialize storage and load sprites from SD card
     * @return true if storage is available
     */
    bool init() {
        Lock lock(mutex_);
        
        if (initialized_) return true;
        
        auto& fs = Utils::FileSystemService::instance();
        if (!fs.isReady() || !fs.isMounted()) {
            ESP_LOGW(SPRITE_STORAGE_TAG, "SD card not available");
            return false;
        }
        
        // Ensure directory exists
        if (!fs.dirExists(SPRITES_DIR)) {
            if (!fs.createDir(SPRITES_DIR)) {
                ESP_LOGE(SPRITE_STORAGE_TAG, "Failed to create sprites directory");
                return false;
            }
            ESP_LOGI(SPRITE_STORAGE_TAG, "Created sprites directory");
        }
        
        // Load existing sprites
        loadIndex();
        
        initialized_ = true;
        ESP_LOGI(SPRITE_STORAGE_TAG, "Initialized with %d sprites", (int)sprites_.size());
        return true;
    }
    
    /** Check if storage is ready */
    bool isReady() const { return initialized_; }
    
    // ========== Sprite Access ==========
    
    /** Get all sprites (metadata) */
    std::vector<SpriteData> getAllSprites() {
        Lock lock(mutex_);
        return sprites_;
    }
    
    /** Get number of sprites */
    size_t getCount() {
        Lock lock(mutex_);
        return sprites_.size();
    }
    
    /** Find sprite by ID */
    const SpriteData* findById(int id) {
        Lock lock(mutex_);
        for (auto& s : sprites_) {
            if (s.id == id) return &s;
        }
        return nullptr;
    }
    
    /** Get sprite by ID (with pixel loading if needed) */
    bool getSprite(int id, SpriteData& out) {
        Lock lock(mutex_);
        
        for (auto& s : sprites_) {
            if (s.id == id) {
                // Load pixels if not already loaded
                if (!s.loaded && s.pixels.empty()) {
                    loadPixels(s);
                }
                out = s;
                return true;
            }
        }
        return false;
    }
    
    // ========== Sprite Modification ==========
    
    /**
     * @brief Save or update a sprite
     * @param name Sprite name
     * @param width Sprite width
     * @param height Sprite height
     * @param pixels RGB888 pixel data
     * @param preview Base64 preview image
     * @param existingId ID to update, or 0 to create new
     * @return Sprite ID on success, -1 on failure
     */
    int saveSprite(const std::string& name, int width, int height,
                   const std::vector<uint8_t>& pixels, 
                   const std::string& preview = "",
                   int existingId = 0) {
        Lock lock(mutex_);
        
        if (!initialized_) {
            ESP_LOGE(SPRITE_STORAGE_TAG, "Storage not initialized");
            return -1;
        }
        
        // Validate pixel data
        size_t expected = (size_t)width * height * 3;
        if (pixels.size() != expected) {
            ESP_LOGE(SPRITE_STORAGE_TAG, "Invalid pixel data: got %u, expected %u",
                     (unsigned)pixels.size(), (unsigned)expected);
            return -1;
        }
        
        SpriteData* sprite = nullptr;
        
        // Find existing or create new
        if (existingId > 0) {
            for (auto& s : sprites_) {
                if (s.id == existingId) {
                    sprite = &s;
                    break;
                }
            }
        }
        
        if (!sprite) {
            sprites_.emplace_back();
            sprite = &sprites_.back();
            sprite->id = nextId_++;
        }
        
        sprite->name = name;
        sprite->width = width;
        sprite->height = height;
        sprite->pixels = pixels;
        sprite->preview = preview;
        sprite->loaded = true;
        
        ESP_LOGI(SPRITE_STORAGE_TAG, "Saving sprite '%s' id=%d (%dx%d, %u bytes)",
                 name.c_str(), sprite->id, width, height, (unsigned)pixels.size());
        
        // Write pixel file first
        if (!writePixelFile(sprite->id, pixels)) {
            ESP_LOGE(SPRITE_STORAGE_TAG, "Failed to write pixel file");
            // Don't fail completely - sprite is in memory
        }
        
        // Write index file
        if (!saveIndex()) {
            ESP_LOGE(SPRITE_STORAGE_TAG, "Failed to write index file");
            // Don't fail completely - sprite is in memory
        }
        
        return sprite->id;
    }
    
    /**
     * @brief Delete a sprite
     * @return true if deleted
     */
    bool deleteSprite(int id) {
        Lock lock(mutex_);
        
        for (auto it = sprites_.begin(); it != sprites_.end(); ++it) {
            if (it->id == id) {
                // Delete pixel file
                char path[64];
                snprintf(path, sizeof(path), "%s/sprite_%d.bin", SPRITES_DIR, id);
                Utils::FileSystemService::instance().deleteFile(path);
                
                sprites_.erase(it);
                saveIndex();
                
                ESP_LOGI(SPRITE_STORAGE_TAG, "Deleted sprite %d", id);
                return true;
            }
        }
        return false;
    }
    
    /**
     * @brief Rename a sprite
     */
    bool renameSprite(int id, const std::string& newName) {
        Lock lock(mutex_);
        
        for (auto& s : sprites_) {
            if (s.id == id) {
                s.name = newName;
                saveIndex();
                return true;
            }
        }
        return false;
    }
    
    // ========== JSON Serialization for Web API ==========
    
    /**
     * @brief Get sprites as JSON array for API response
     * @return cJSON array (caller must delete)
     */
    cJSON* toJson() {
        Lock lock(mutex_);
        
        cJSON* arr = cJSON_CreateArray();
        for (const auto& s : sprites_) {
            cJSON* item = cJSON_CreateObject();
            cJSON_AddNumberToObject(item, "id", s.id);
            cJSON_AddStringToObject(item, "name", s.name.c_str());
            cJSON_AddNumberToObject(item, "width", s.width);
            cJSON_AddNumberToObject(item, "height", s.height);
            cJSON_AddNumberToObject(item, "scale", s.scale);
            cJSON_AddStringToObject(item, "preview", s.preview.c_str());
            cJSON_AddBoolToObject(item, "hasPixels", !s.pixels.empty() || s.loaded);
            cJSON_AddItemToArray(arr, item);
        }
        return arr;
    }
    
private:
    // ========== Internal File Operations ==========
    
    /**
     * @brief Write pixel data to separate binary file
     */
    bool writePixelFile(int id, const std::vector<uint8_t>& pixels) {
        if (pixels.empty()) return true;
        
        auto& fs = Utils::FileSystemService::instance();
        
        char path[64];
        snprintf(path, sizeof(path), "%s/sprite_%d.bin", SPRITES_DIR, id);
        
        // Write via FileSystemService (handles full path correctly)
        bool ok = fs.writeFile(path, pixels.data(), pixels.size());
        
        if (ok) {
            ESP_LOGI(SPRITE_STORAGE_TAG, "Wrote pixel file: %s (%u bytes)", 
                     path, (unsigned)pixels.size());
        } else {
            ESP_LOGE(SPRITE_STORAGE_TAG, "Failed to write pixel file: %s", path);
        }
        
        // Small delay to let FAT filesystem settle
        vTaskDelay(pdMS_TO_TICKS(50));
        
        return ok;
    }
    
    /**
     * @brief Load pixels from binary file
     */
    bool loadPixels(SpriteData& sprite) {
        auto& fs = Utils::FileSystemService::instance();
        
        char path[64];
        snprintf(path, sizeof(path), "%s/sprite_%d.bin", SPRITES_DIR, sprite.id);
        
        // Check file exists
        if (!fs.fileExists(path)) {
            ESP_LOGW(SPRITE_STORAGE_TAG, "Pixel file not found: %s", path);
            return false;
        }
        
        uint64_t fileSize = fs.getFileSize(path);
        if (fileSize == 0 || fileSize > 1024 * 1024) {  // Max 1MB
            ESP_LOGW(SPRITE_STORAGE_TAG, "Invalid pixel file size: %llu", fileSize);
            return false;
        }
        
        sprite.pixels.resize((size_t)fileSize);
        
        int bytesRead = fs.readFile(path, sprite.pixels.data(), sprite.pixels.size());
        if (bytesRead != (int)fileSize) {
            ESP_LOGE(SPRITE_STORAGE_TAG, "Pixel read error: got %d, expected %llu",
                     bytesRead, fileSize);
            sprite.pixels.clear();
            return false;
        }
        
        sprite.loaded = true;
        ESP_LOGI(SPRITE_STORAGE_TAG, "Loaded pixels for sprite %d (%d bytes)",
                 sprite.id, bytesRead);
        return true;
    }
    
    /**
     * @brief Save index to JSON file (atomic write)
     */
    bool saveIndex() {
        auto& fs = Utils::FileSystemService::instance();
        
        // Build JSON
        cJSON* root = cJSON_CreateObject();
        cJSON_AddNumberToObject(root, "version", 1);
        cJSON_AddNumberToObject(root, "nextId", nextId_);
        cJSON_AddNumberToObject(root, "count", (int)sprites_.size());
        
        cJSON* arr = cJSON_CreateArray();
        for (const auto& s : sprites_) {
            cJSON* item = cJSON_CreateObject();
            cJSON_AddNumberToObject(item, "id", s.id);
            cJSON_AddStringToObject(item, "name", s.name.c_str());
            cJSON_AddNumberToObject(item, "width", s.width);
            cJSON_AddNumberToObject(item, "height", s.height);
            cJSON_AddNumberToObject(item, "scale", s.scale);
            // Don't store preview in index - it's in the response only
            cJSON_AddNumberToObject(item, "pixelSize", (int)s.pixels.size());
            cJSON_AddItemToArray(arr, item);
        }
        cJSON_AddItemToObject(root, "sprites", arr);
        
        char* json = cJSON_PrintUnformatted(root);
        cJSON_Delete(root);
        
        if (!json) {
            ESP_LOGE(SPRITE_STORAGE_TAG, "JSON serialization failed");
            return false;
        }
        
        size_t jsonLen = strlen(json);
        ESP_LOGI(SPRITE_STORAGE_TAG, "Writing index: %u bytes", (unsigned)jsonLen);
        
        // Atomic write: write to temp file, then rename
        // First delete any existing temp file
        fs.deleteFile(TEMP_INDEX);
        
        // Write to temp
        bool ok = fs.writeFile(TEMP_INDEX, json, jsonLen);
        free(json);
        
        if (!ok) {
            ESP_LOGE(SPRITE_STORAGE_TAG, "Failed to write temp index");
            return false;
        }
        
        // Small delay
        vTaskDelay(pdMS_TO_TICKS(30));
        
        // Delete old index
        fs.deleteFile(INDEX_FILE);
        vTaskDelay(pdMS_TO_TICKS(20));
        
        // Rename temp to final
        ok = fs.rename(TEMP_INDEX, INDEX_FILE);
        
        if (!ok) {
            // Rename failed - try direct write as fallback
            ESP_LOGW(SPRITE_STORAGE_TAG, "Rename failed, trying direct write");
            
            json = cJSON_PrintUnformatted(root);
            if (json) {
                ok = fs.writeFile(INDEX_FILE, json, strlen(json));
                free(json);
            }
        }
        
        if (ok) {
            ESP_LOGI(SPRITE_STORAGE_TAG, "Index saved successfully");
        } else {
            ESP_LOGE(SPRITE_STORAGE_TAG, "Index save failed");
        }
        
        return ok;
    }
    
    /**
     * @brief Load index from JSON file
     */
    void loadIndex() {
        auto& fs = Utils::FileSystemService::instance();
        
        // Try to load index file
        char* data = nullptr;
        size_t size = 0;
        
        if (!fs.readFile(INDEX_FILE, &data, &size) || !data) {
            ESP_LOGI(SPRITE_STORAGE_TAG, "No index file, scanning for orphans");
            recoverOrphans();
            return;
        }
        
        cJSON* root = cJSON_Parse(data);
        free(data);
        
        if (!root) {
            ESP_LOGE(SPRITE_STORAGE_TAG, "Index parse failed");
            recoverOrphans();
            return;
        }
        
        // Parse nextId
        cJSON* nextId = cJSON_GetObjectItem(root, "nextId");
        if (nextId && cJSON_IsNumber(nextId)) {
            nextId_ = nextId->valueint;
        }
        
        // Parse sprites array
        cJSON* arr = cJSON_GetObjectItem(root, "sprites");
        if (arr && cJSON_IsArray(arr)) {
            sprites_.clear();
            
            cJSON* item = nullptr;
            cJSON_ArrayForEach(item, arr) {
                SpriteData s;
                
                cJSON* id = cJSON_GetObjectItem(item, "id");
                cJSON* name = cJSON_GetObjectItem(item, "name");
                cJSON* width = cJSON_GetObjectItem(item, "width");
                cJSON* height = cJSON_GetObjectItem(item, "height");
                cJSON* scale = cJSON_GetObjectItem(item, "scale");
                
                if (id && cJSON_IsNumber(id)) s.id = id->valueint;
                if (name && cJSON_IsString(name)) s.name = name->valuestring;
                if (width && cJSON_IsNumber(width)) s.width = width->valueint;
                if (height && cJSON_IsNumber(height)) s.height = height->valueint;
                if (scale && cJSON_IsNumber(scale)) s.scale = scale->valueint;
                
                // Check if pixel file exists
                char pixPath[64];
                snprintf(pixPath, sizeof(pixPath), "%s/sprite_%d.bin", SPRITES_DIR, s.id);
                
                if (fs.fileExists(pixPath)) {
                    // Load pixels immediately for small sprites, defer for large
                    uint64_t pixSize = fs.getFileSize(pixPath);
                    if (pixSize > 0 && pixSize < 32768) {  // < 32KB, load now
                        loadPixels(s);
                    }
                    s.loaded = !s.pixels.empty();
                }
                
                sprites_.push_back(std::move(s));
                ESP_LOGI(SPRITE_STORAGE_TAG, "Loaded: id=%d '%s' %dx%d pixels=%s",
                         sprites_.back().id, sprites_.back().name.c_str(),
                         sprites_.back().width, sprites_.back().height,
                         sprites_.back().loaded ? "YES" : "DEFERRED");
            }
        }
        
        cJSON_Delete(root);
        ESP_LOGI(SPRITE_STORAGE_TAG, "Loaded %d sprites from index", (int)sprites_.size());
    }
    
    /**
     * @brief Scan for orphaned pixel files and recover them
     */
    void recoverOrphans() {
        auto& fs = Utils::FileSystemService::instance();
        
        std::vector<int> foundIds;
        
        // List directory
        fs.listDir(SPRITES_DIR, [&](const Utils::FileInfo& info) {
            if (info.isDirectory) return true;
            
            int id;
            if (sscanf(info.name, "sprite_%d.bin", &id) == 1) {
                foundIds.push_back(id);
            }
            return true;
        });
        
        if (foundIds.empty()) return;
        
        ESP_LOGI(SPRITE_STORAGE_TAG, "Found %d orphaned files", (int)foundIds.size());
        
        for (int id : foundIds) {
            // Skip if already known
            bool exists = false;
            for (const auto& s : sprites_) {
                if (s.id == id) { exists = true; break; }
            }
            if (exists) continue;
            
            // Create recovered sprite entry
            char path[64];
            snprintf(path, sizeof(path), "%s/sprite_%d.bin", SPRITES_DIR, id);
            
            uint64_t fileSize = fs.getFileSize(path);
            if (fileSize == 0) continue;
            
            SpriteData s;
            s.id = id;
            s.name = "Recovered_" + std::to_string(id);
            
            // Guess dimensions from file size (RGB888)
            int pixels = (int)(fileSize / 3);
            if (pixels == 1024) { s.width = 32; s.height = 32; }
            else if (pixels == 961) { s.width = 31; s.height = 31; }
            else if (pixels == 4096) { s.width = 64; s.height = 64; }
            else if (pixels == 2048) { s.width = 64; s.height = 32; }
            else {
                s.width = (int)sqrt(pixels);
                s.height = pixels / s.width;
            }
            
            // Load the actual pixel data
            loadPixels(s);
            
            if (s.loaded) {
                sprites_.push_back(std::move(s));
                if (id >= nextId_) nextId_ = id + 1;
                ESP_LOGI(SPRITE_STORAGE_TAG, "Recovered sprite %d (%dx%d)",
                         id, s.width, s.height);
            }
        }
        
        // Save the recovered index
        if (!foundIds.empty()) {
            ESP_LOGI(SPRITE_STORAGE_TAG, "Saving recovered sprites...");
            vTaskDelay(pdMS_TO_TICKS(100));
            saveIndex();
        }
    }
};

} // namespace Storage
} // namespace SystemAPI

// Convenience macro
#define SPRITE_STORAGE SystemAPI::Storage::SpriteStorage::instance()

#endif // ARCOS_INCLUDE_SYSTEMAPI_STORAGE_SPRITE_STORAGE_HPP_
