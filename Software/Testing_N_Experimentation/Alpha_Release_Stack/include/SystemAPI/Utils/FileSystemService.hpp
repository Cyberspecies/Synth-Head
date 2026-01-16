/*****************************************************************
 * File:      FileSystemService.hpp
 * Category:  include/SystemAPI/Utils
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    High-level file system service for SD card operations.
 *    Thin wrapper around HAL storage with convenient APIs.
 *    
 * Architecture:
 *    This service uses the HAL layer (arcos::hal::IHalStorage)
 *    to provide platform-independent file system access.
 *****************************************************************/

#ifndef ARCOS_INCLUDE_SYSTEMAPI_UTILS_FILESYSTEM_SERVICE_HPP_
#define ARCOS_INCLUDE_SYSTEMAPI_UTILS_FILESYSTEM_SERVICE_HPP_

#include "HAL/IHalStorage.hpp"
#include "src/HAL/ESP32/Esp32SdCard.hpp"
#include <cstdint>
#include <cstring>
#include <functional>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cerrno>
#include "esp_log.h"

namespace SystemAPI {
namespace Utils {

// ============================================================
// File Info Structure
// ============================================================

/** File/directory information for listings */
struct FileInfo {
    char name[64];          ///< File name (not full path)
    char path[128];         ///< Full path relative to mount
    uint32_t size;          ///< Size in bytes (0 for directories)
    bool isDirectory;       ///< true if directory
};

/** Directory listing callback - return false to stop enumeration */
using FileEnumCallback = std::function<bool(const FileInfo& info)>;

// ============================================================
// SD Card Pin Configuration
// ============================================================

/** SD Card pin configuration */
struct SdCardPins {
    int miso = 14;
    int mosi = 47;
    int clk = 21;
    int cs = 48;
};

// ============================================================
// FileSystem Service (Singleton)
// ============================================================

/**
 * @brief High-level file system service
 * 
 * Provides convenient APIs for SD card operations including
 * file listing, search, format, and metadata retrieval.
 * Uses HAL layer for platform independence.
 */
class FileSystemService {
private:
    arcos::hal::esp32::Esp32SdCard hal_;  ///< HAL implementation
    bool ready_ = false;
    
    // Singleton
    FileSystemService() = default;
    
public:
    // Delete copy/move
    FileSystemService(const FileSystemService&) = delete;
    FileSystemService& operator=(const FileSystemService&) = delete;
    
    /** Get singleton instance */
    static FileSystemService& instance() {
        static FileSystemService inst;
        return inst;
    }
    
    // ========== Lifecycle ==========
    
    /**
     * @brief Initialize SD card with pin configuration
     * @param pins SD card SPI pins
     * @return true on success
     */
    bool init(const SdCardPins& pins = SdCardPins{}) {
        if (ready_) return true;
        
        arcos::hal::SdCardConfig config;
        config.miso_pin = pins.miso;
        config.mosi_pin = pins.mosi;
        config.clk_pin = pins.clk;
        config.cs_pin = pins.cs;
        
        auto result = hal_.init(config);
        ready_ = (result == arcos::hal::HalResult::OK);
        return ready_;
    }
    
    /** Deinitialize SD card */
    void deinit() {
        if (ready_) {
            hal_.deinit();
            ready_ = false;
        }
    }
    
    // ========== Status ==========
    
    /** Check if service is ready */
    bool isReady() const { return ready_; }
    
    /** Check if SD card is mounted */
    bool isMounted() const { return hal_.isMounted(); }
    
    /** Get total capacity in bytes */
    uint64_t getTotalBytes() const { return hal_.getTotalSize(); }
    
    /** Get free space in bytes */
    uint64_t getFreeBytes() const { return hal_.getFreeSpace(); }
    
    /** Get used space in bytes */
    uint64_t getUsedBytes() const {
        uint64_t total = getTotalBytes();
        uint64_t free = getFreeBytes();
        return (total > free) ? (total - free) : 0;
    }
    
    /** Get card name */
    const char* getCardName() const { return hal_.getCardName(); }
    
    /** Get mount point */
    const char* getMountPoint() const { return hal_.getMountPoint(); }
    
    // ========== File Operations ==========
    
    /** Check if file exists */
    bool fileExists(const char* path) { return hal_.fileExists(path); }
    
    /** Check if directory exists */
    bool dirExists(const char* path) { return hal_.dirExists(path); }
    
    /** Get file size */
    uint64_t getFileSize(const char* path) { return hal_.getFileSize(path); }
    
    /** Create directory */
    bool createDir(const char* path) {
        return hal_.createDir(path) == arcos::hal::HalResult::OK;
    }
    
    /** Delete file */
    bool deleteFile(const char* path) {
        return hal_.deleteFile(path) == arcos::hal::HalResult::OK;
    }
    
    /** Delete directory (must be empty) */
    bool deleteDir(const char* path) {
        return hal_.deleteDir(path) == arcos::hal::HalResult::OK;
    }
    
    /** Rename/move file or directory */
    bool rename(const char* oldPath, const char* newPath) {
        return hal_.rename(oldPath, newPath) == arcos::hal::HalResult::OK;
    }
    
    // ========== High-Level Operations ==========
    
    /**
     * @brief Format SD card (ERASES ALL DATA!)
     * @return true on success
     */
    bool format() {
        return hal_.format() == arcos::hal::HalResult::OK;
    }
    
    /**
     * @brief Clear all files from SD card (keeps filesystem)
     * @return true on success
     */
    bool clearAll() {
        return hal_.clearAllFiles() == arcos::hal::HalResult::OK;
    }
    
    /**
     * @brief List directory contents
     * @param path Directory path (relative to mount)
     * @param callback Called for each entry
     * @return Number of entries, -1 on error
     */
    int listDir(const char* path, FileEnumCallback callback) {
        if (!hal_.isMounted()) return -1;
        
        char fullPath[160];
        hal_.buildFullPath(fullPath, sizeof(fullPath), path);
        
        DIR* dir = opendir(fullPath);
        if (!dir) return -1;
        
        int count = 0;
        struct dirent* entry;
        
        // Limit name length to avoid format-truncation warnings
        constexpr size_t MAX_NAME = 64;
        char safeName[MAX_NAME + 1];
        
        while ((entry = readdir(dir)) != nullptr) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }
            
            // Copy name with safe truncation
            strncpy(safeName, entry->d_name, MAX_NAME);
            safeName[MAX_NAME] = '\0';
            
            FileInfo info;
            strncpy(info.name, safeName, sizeof(info.name) - 1);
            info.name[sizeof(info.name) - 1] = '\0';
            
            // Build relative path
            if (!path || path[0] == '\0' || strcmp(path, "/") == 0) {
                snprintf(info.path, sizeof(info.path), "/%s", safeName);
            } else {
                snprintf(info.path, sizeof(info.path), "%s/%s", path, safeName);
            }
            info.path[sizeof(info.path) - 1] = '\0';
            
            // Get file stats
            char entryFullPath[256];
            snprintf(entryFullPath, sizeof(entryFullPath), "%s/%s", fullPath, safeName);
            
            struct stat st;
            if (stat(entryFullPath, &st) == 0) {
                info.isDirectory = S_ISDIR(st.st_mode);
                info.size = info.isDirectory ? 0 : st.st_size;
            } else {
                info.isDirectory = (entry->d_type == DT_DIR);
                info.size = 0;
            }
            
            count++;
            
            if (callback && !callback(info)) {
                break;
            }
        }
        
        closedir(dir);
        return count;
    }
    
    /**
     * @brief Write data to file with proper sync
     * @return true on success
     */
    bool writeFile(const char* path, const void* data, size_t size) {
        if (!hal_.isMounted()) {
            ESP_LOGE("FSService", "SD card not mounted");
            return false;
        }
        
        char fullPath[160];
        hal_.buildFullPath(fullPath, sizeof(fullPath), path);
        
        // Ensure parent directory exists
        char dirPath[160];
        strncpy(dirPath, fullPath, sizeof(dirPath));
        char* lastSlash = strrchr(dirPath, '/');
        if (lastSlash && lastSlash != dirPath) {
            *lastSlash = '\0';
            struct stat st;
            if (stat(dirPath, &st) != 0 || !S_ISDIR(st.st_mode)) {
                ESP_LOGI("FSService", "Creating directory: %s", dirPath);
                mkdir(dirPath, 0755);
                vTaskDelay(pdMS_TO_TICKS(20));
            }
        }
        
        // Remove existing file first
        struct stat fst;
        if (stat(fullPath, &fst) == 0) {
            ESP_LOGI("FSService", "Removing existing file: %s", fullPath);
            remove(fullPath);
            vTaskDelay(pdMS_TO_TICKS(20));
        }
        
        // Open file for writing
        ESP_LOGI("FSService", "Opening file: %s", fullPath);
        FILE* f = fopen(fullPath, "wb");
        if (!f) {
            ESP_LOGE("FSService", "Failed to open file for writing: %s (errno=%d)", fullPath, errno);
            return false;
        }
        
        size_t written = fwrite(data, 1, size, f);
        
        // Flush and sync to ensure data is written to SD card
        fflush(f);
        int fd = fileno(f);
        if (fd >= 0) {
            fsync(fd);
        }
        
        fclose(f);
        
        if (written != size) {
            ESP_LOGE("FSService", "Write size mismatch: expected %zu, wrote %zu", size, written);
            return false;
        }
        
        ESP_LOGI("FSService", "Successfully wrote %zu bytes to %s", size, fullPath);
        return true;
    }
    
    /**
     * @brief Read file into buffer
     * @return Bytes read, -1 on error
     */
    int readFile(const char* path, void* buffer, size_t maxSize) {
        if (!hal_.isMounted()) return -1;
        
        char fullPath[160];
        hal_.buildFullPath(fullPath, sizeof(fullPath), path);
        
        FILE* f = fopen(fullPath, "rb");
        if (!f) return -1;
        
        size_t bytesRead = fread(buffer, 1, maxSize, f);
        fclose(f);
        
        return (int)bytesRead;
    }
    
    /**
     * @brief Append data to file
     * @return true on success
     */
    bool appendFile(const char* path, const void* data, size_t size) {
        if (!hal_.isMounted()) return false;
        
        char fullPath[160];
        hal_.buildFullPath(fullPath, sizeof(fullPath), path);
        
        FILE* f = fopen(fullPath, "ab");
        if (!f) return false;
        
        size_t written = fwrite(data, 1, size, f);
        fclose(f);
        
        return written == size;
    }
    
    /**
     * @brief Read entire file with allocation
     * @param path File path
     * @param outData Output pointer (caller must free)
     * @param outSize Output size
     * @return true on success
     */
    bool readFile(const char* path, char** outData, size_t* outSize) {
        if (!hal_.isMounted() || !outData || !outSize) return false;
        
        *outData = nullptr;
        *outSize = 0;
        
        char fullPath[160];
        hal_.buildFullPath(fullPath, sizeof(fullPath), path);
        
        struct stat st;
        if (stat(fullPath, &st) != 0) return false;
        
        FILE* f = fopen(fullPath, "rb");
        if (!f) return false;
        
        char* buffer = (char*)malloc(st.st_size + 1);
        if (!buffer) {
            fclose(f);
            return false;
        }
        
        size_t bytesRead = fread(buffer, 1, st.st_size, f);
        fclose(f);
        
        buffer[bytesRead] = '\0';
        *outData = buffer;
        *outSize = bytesRead;
        
        return true;
    }
};

} // namespace Utils
} // namespace SystemAPI

// Convenience macro
#define SDCARD_SERVICE SystemAPI::Utils::FileSystemService::instance()

#endif // ARCOS_INCLUDE_SYSTEMAPI_UTILS_FILESYSTEM_SERVICE_HPP_
