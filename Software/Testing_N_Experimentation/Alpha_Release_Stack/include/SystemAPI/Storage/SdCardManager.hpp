/*****************************************************************
 * File:      SdCardManager.hpp
 * Category:  include/SystemAPI/Storage
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Bulletproof SD card management with:
 *    - Thread-safe mutex protection for multicore access
 *    - Extensive logging and debugging
 *    - Simple, hard-to-misuse API
 *    - Automatic verification of writes
 *    - File browser support for web interface
 * 
 * Usage:
 *    auto& sd = SdCardManager::instance();
 *    if (sd.isReady()) {
 *        sd.writeFile("/sprites/test.bin", data, size);
 *    }
 *****************************************************************/

#ifndef ARCOS_INCLUDE_SYSTEMAPI_STORAGE_SDCARDMANAGER_HPP_
#define ARCOS_INCLUDE_SYSTEMAPI_STORAGE_SDCARDMANAGER_HPP_

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>
#include <string>
#include <functional>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cerrno>

namespace SystemAPI {
namespace Storage {

// ============================================================
// File Entry Structure (for directory listings)
// ============================================================
struct FileEntry {
    std::string name;        // File name only
    std::string path;        // Full path from mount point
    uint32_t size;           // Size in bytes
    bool isDirectory;        // true if directory
    time_t modTime;          // Modification time
};

// ============================================================
// Operation Result (for detailed error reporting)
// ============================================================
enum class SdResult {
    OK = 0,
    NOT_INITIALIZED,
    NOT_MOUNTED,
    FILE_NOT_FOUND,
    DIR_NOT_FOUND,
    OPEN_FAILED,
    READ_FAILED,
    WRITE_FAILED,
    VERIFY_FAILED,
    DELETE_FAILED,
    CREATE_DIR_FAILED,
    MUTEX_TIMEOUT,
    OUT_OF_MEMORY,
    INVALID_PATH,
    PATH_TOO_LONG
};

// Convert result to string for logging
inline const char* sdResultToString(SdResult r) {
    switch (r) {
        case SdResult::OK: return "OK";
        case SdResult::NOT_INITIALIZED: return "NOT_INITIALIZED";
        case SdResult::NOT_MOUNTED: return "NOT_MOUNTED";
        case SdResult::FILE_NOT_FOUND: return "FILE_NOT_FOUND";
        case SdResult::DIR_NOT_FOUND: return "DIR_NOT_FOUND";
        case SdResult::OPEN_FAILED: return "OPEN_FAILED";
        case SdResult::READ_FAILED: return "READ_FAILED";
        case SdResult::WRITE_FAILED: return "WRITE_FAILED";
        case SdResult::VERIFY_FAILED: return "VERIFY_FAILED";
        case SdResult::DELETE_FAILED: return "DELETE_FAILED";
        case SdResult::CREATE_DIR_FAILED: return "CREATE_DIR_FAILED";
        case SdResult::MUTEX_TIMEOUT: return "MUTEX_TIMEOUT";
        case SdResult::OUT_OF_MEMORY: return "OUT_OF_MEMORY";
        case SdResult::INVALID_PATH: return "INVALID_PATH";
        case SdResult::PATH_TOO_LONG: return "PATH_TOO_LONG";
        default: return "UNKNOWN";
    }
}

// ============================================================
// SD Card Manager Singleton
// ============================================================
class SdCardManager {
private:
    static constexpr const char* TAG = "SdCardMgr";
    static constexpr const char* MOUNT_POINT = "/sdcard";
    static constexpr size_t MAX_PATH_LEN = 128;
    static constexpr TickType_t MUTEX_TIMEOUT = pdMS_TO_TICKS(5000);  // 5 second timeout
    
    // Hardware state
    bool initialized_ = false;
    bool mounted_ = false;
    sdmmc_card_t* card_ = nullptr;
    spi_host_device_t spiHost_ = SPI2_HOST;
    
    // Thread safety
    SemaphoreHandle_t mutex_ = nullptr;
    
    // Statistics for debugging
    uint32_t readCount_ = 0;
    uint32_t writeCount_ = 0;
    uint32_t errorCount_ = 0;
    uint32_t verifyFailCount_ = 0;
    SdResult lastError_ = SdResult::OK;
    
    // Pin config
    int misoPin_ = 14;
    int mosiPin_ = 47;
    int clkPin_ = 21;
    int csPin_ = 48;
    
    // Singleton
    SdCardManager() {
        mutex_ = xSemaphoreCreateMutex();
        if (!mutex_) {
            ESP_LOGE(TAG, "CRITICAL: Failed to create mutex!");
        }
    }
    
public:
    ~SdCardManager() {
        if (mounted_) unmount();
        if (mutex_) vSemaphoreDelete(mutex_);
    }
    
    // Delete copy/move
    SdCardManager(const SdCardManager&) = delete;
    SdCardManager& operator=(const SdCardManager&) = delete;
    
    /** Get singleton instance */
    static SdCardManager& instance() {
        static SdCardManager inst;
        return inst;
    }
    
    // ========================================================
    // Initialization
    // ========================================================
    
    /**
     * Initialize SD card with default pins
     * Call this once at startup from Core 0
     */
    SdResult init() {
        return init(misoPin_, mosiPin_, clkPin_, csPin_);
    }
    
    /**
     * Initialize SD card with custom pins
     */
    SdResult init(int miso, int mosi, int clk, int cs) {
        if (!takeMutex()) return SdResult::MUTEX_TIMEOUT;
        
        ESP_LOGI(TAG, "========================================");
        ESP_LOGI(TAG, "SD Card Manager Initializing...");
        ESP_LOGI(TAG, "  MISO: GPIO%d", miso);
        ESP_LOGI(TAG, "  MOSI: GPIO%d", mosi);
        ESP_LOGI(TAG, "  CLK:  GPIO%d", clk);
        ESP_LOGI(TAG, "  CS:   GPIO%d", cs);
        ESP_LOGI(TAG, "========================================");
        
        if (initialized_) {
            ESP_LOGW(TAG, "Already initialized, skipping");
            giveMutex();
            return SdResult::OK;
        }
        
        misoPin_ = miso;
        mosiPin_ = mosi;
        clkPin_ = clk;
        csPin_ = cs;
        
        // Configure SPI bus
        spi_bus_config_t bus_cfg = {};
        bus_cfg.mosi_io_num = mosi;
        bus_cfg.miso_io_num = miso;
        bus_cfg.sclk_io_num = clk;
        bus_cfg.quadwp_io_num = -1;
        bus_cfg.quadhd_io_num = -1;
        bus_cfg.max_transfer_sz = 4096;
        
        esp_err_t ret = spi_bus_initialize(spiHost_, &bus_cfg, SDSPI_DEFAULT_DMA);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "SPI bus init FAILED: %s", esp_err_to_name(ret));
            giveMutex();
            lastError_ = SdResult::NOT_INITIALIZED;
            errorCount_++;
            return SdResult::NOT_INITIALIZED;
        }
        
        initialized_ = true;
        ESP_LOGI(TAG, "SPI bus initialized successfully");
        
        // Try to mount
        SdResult mountResult = mountInternal();
        giveMutex();
        
        return mountResult;
    }
    
    /**
     * Mount the SD card (call after init if card was inserted later)
     */
    SdResult mount() {
        if (!takeMutex()) return SdResult::MUTEX_TIMEOUT;
        SdResult result = mountInternal();
        giveMutex();
        return result;
    }
    
    /**
     * Unmount the SD card
     */
    SdResult unmount() {
        if (!takeMutex()) return SdResult::MUTEX_TIMEOUT;
        
        if (!mounted_) {
            giveMutex();
            return SdResult::NOT_MOUNTED;
        }
        
        ESP_LOGI(TAG, "Unmounting SD card...");
        esp_vfs_fat_sdcard_unmount(MOUNT_POINT, card_);
        card_ = nullptr;
        mounted_ = false;
        
        ESP_LOGI(TAG, "SD card unmounted");
        giveMutex();
        return SdResult::OK;
    }
    
    // ========================================================
    // Status
    // ========================================================
    
    bool isInitialized() const { return initialized_; }
    bool isMounted() const { return mounted_; }
    bool isReady() const { return initialized_ && mounted_; }
    
    /** Get last error for debugging */
    SdResult getLastError() const { return lastError_; }
    const char* getLastErrorString() const { return sdResultToString(lastError_); }
    
    /** Get statistics */
    uint32_t getReadCount() const { return readCount_; }
    uint32_t getWriteCount() const { return writeCount_; }
    uint32_t getErrorCount() const { return errorCount_; }
    uint32_t getVerifyFailCount() const { return verifyFailCount_; }
    
    /** Get card info */
    uint64_t getTotalBytes() const {
        if (!mounted_ || !card_) return 0;
        return (uint64_t)card_->csd.capacity * card_->csd.sector_size;
    }
    
    uint64_t getFreeBytes() const {
        if (!mounted_) return 0;
        FATFS* fs;
        DWORD fre_clust;
        if (f_getfree("0:", &fre_clust, &fs) != FR_OK) return 0;
        return (uint64_t)fre_clust * fs->csize * 512;
    }
    
    // ========================================================
    // FILE OPERATIONS (Thread-Safe with Verification)
    // ========================================================
    
    /**
     * Write data to file with automatic verification
     * @param path Path relative to /sdcard (e.g., "/sprites/test.bin")
     * @param data Data to write
     * @param size Size in bytes
     * @return SdResult::OK on success
     */
    SdResult writeFile(const char* path, const void* data, size_t size) {
        if (!takeMutex()) return SdResult::MUTEX_TIMEOUT;
        
        ESP_LOGI(TAG, "WRITE: %s (%zu bytes)", path, size);
        
        SdResult result = writeFileInternal(path, data, size);
        
        if (result == SdResult::OK) {
            writeCount_++;
            ESP_LOGI(TAG, "WRITE OK: %s", path);
        } else {
            errorCount_++;
            lastError_ = result;
            ESP_LOGE(TAG, "WRITE FAILED: %s - %s", path, sdResultToString(result));
        }
        
        giveMutex();
        return result;
    }
    
    /**
     * Read entire file into allocated buffer
     * @param path Path relative to /sdcard
     * @param outData Output pointer (caller must free with free())
     * @param outSize Output size
     * @return SdResult::OK on success
     */
    SdResult readFile(const char* path, uint8_t** outData, size_t* outSize) {
        if (!takeMutex()) return SdResult::MUTEX_TIMEOUT;
        
        ESP_LOGI(TAG, "READ: %s", path);
        
        SdResult result = readFileInternal(path, outData, outSize);
        
        if (result == SdResult::OK) {
            readCount_++;
            ESP_LOGI(TAG, "READ OK: %s (%zu bytes)", path, *outSize);
        } else {
            errorCount_++;
            lastError_ = result;
            ESP_LOGE(TAG, "READ FAILED: %s - %s", path, sdResultToString(result));
        }
        
        giveMutex();
        return result;
    }
    
    /**
     * Read file into provided buffer
     * @param path Path relative to /sdcard
     * @param buffer Pre-allocated buffer
     * @param bufferSize Buffer size
     * @param bytesRead Actual bytes read (output)
     * @return SdResult::OK on success
     */
    SdResult readFileToBuffer(const char* path, void* buffer, size_t bufferSize, size_t* bytesRead) {
        if (!takeMutex()) return SdResult::MUTEX_TIMEOUT;
        
        SdResult result = readFileToBufferInternal(path, buffer, bufferSize, bytesRead);
        
        if (result == SdResult::OK) {
            readCount_++;
        } else {
            errorCount_++;
            lastError_ = result;
        }
        
        giveMutex();
        return result;
    }
    
    /**
     * Check if file exists
     */
    bool fileExists(const char* path) {
        if (!takeMutex()) return false;
        bool result = fileExistsInternal(path);
        giveMutex();
        return result;
    }
    
    /**
     * Check if directory exists
     */
    bool dirExists(const char* path) {
        if (!takeMutex()) return false;
        bool result = dirExistsInternal(path);
        giveMutex();
        return result;
    }
    
    /**
     * Get file size
     */
    int64_t getFileSize(const char* path) {
        if (!takeMutex()) return -1;
        int64_t result = getFileSizeInternal(path);
        giveMutex();
        return result;
    }
    
    /**
     * Delete file
     */
    SdResult deleteFile(const char* path) {
        if (!takeMutex()) return SdResult::MUTEX_TIMEOUT;
        
        ESP_LOGI(TAG, "DELETE: %s", path);
        SdResult result = deleteFileInternal(path);
        
        if (result == SdResult::OK) {
            ESP_LOGI(TAG, "DELETE OK: %s", path);
        } else {
            errorCount_++;
            lastError_ = result;
            ESP_LOGE(TAG, "DELETE FAILED: %s - %s", path, sdResultToString(result));
        }
        
        giveMutex();
        return result;
    }
    
    /**
     * Create directory (and parents if needed)
     */
    SdResult createDir(const char* path) {
        if (!takeMutex()) return SdResult::MUTEX_TIMEOUT;
        
        ESP_LOGI(TAG, "MKDIR: %s", path);
        SdResult result = createDirInternal(path);
        
        if (result == SdResult::OK) {
            ESP_LOGI(TAG, "MKDIR OK: %s", path);
        } else {
            errorCount_++;
            lastError_ = result;
            ESP_LOGE(TAG, "MKDIR FAILED: %s - %s", path, sdResultToString(result));
        }
        
        giveMutex();
        return result;
    }
    
    // ========================================================
    // DIRECTORY LISTING (for File Browser)
    // ========================================================
    
    /**
     * List directory contents
     * @param path Directory path (e.g., "/" or "/sprites")
     * @param entries Output vector of file entries
     * @return SdResult::OK on success
     */
    SdResult listDir(const char* path, std::vector<FileEntry>& entries) {
        if (!takeMutex()) return SdResult::MUTEX_TIMEOUT;
        
        ESP_LOGI(TAG, "LIST: %s", path);
        SdResult result = listDirInternal(path, entries);
        
        if (result == SdResult::OK) {
            ESP_LOGI(TAG, "LIST OK: %s (%zu entries)", path, entries.size());
        } else {
            errorCount_++;
            lastError_ = result;
            ESP_LOGE(TAG, "LIST FAILED: %s - %s", path, sdResultToString(result));
        }
        
        giveMutex();
        return result;
    }
    
    /**
     * Get directory tree as JSON (for web API)
     */
    std::string getDirectoryJson(const char* path) {
        std::vector<FileEntry> entries;
        if (listDir(path, entries) != SdResult::OK) {
            return "{\"error\":\"" + std::string(getLastErrorString()) + "\"}";
        }
        
        std::string json = "{\"path\":\"" + std::string(path) + "\",\"entries\":[";
        bool first = true;
        for (const auto& e : entries) {
            if (!first) json += ",";
            first = false;
            json += "{\"name\":\"" + e.name + "\"";
            json += ",\"path\":\"" + e.path + "\"";
            json += ",\"size\":" + std::to_string(e.size);
            json += ",\"isDir\":" + std::string(e.isDirectory ? "true" : "false");
            json += "}";
        }
        json += "]}";
        return json;
    }
    
    /**
     * Get status as JSON (for web API)
     */
    std::string getStatusJson() {
        char json[512];
        snprintf(json, sizeof(json),
            "{\"ready\":%s,"
            "\"initialized\":%s,"
            "\"mounted\":%s,"
            "\"totalMB\":%.1f,"
            "\"freeMB\":%.1f,"
            "\"reads\":%lu,"
            "\"writes\":%lu,"
            "\"errors\":%lu,"
            "\"verifyFails\":%lu,"
            "\"lastError\":\"%s\"}",
            isReady() ? "true" : "false",
            initialized_ ? "true" : "false",
            mounted_ ? "true" : "false",
            getTotalBytes() / 1048576.0f,
            getFreeBytes() / 1048576.0f,
            readCount_,
            writeCount_,
            errorCount_,
            verifyFailCount_,
            getLastErrorString()
        );
        return std::string(json);
    }
    
    /**
     * Read raw bytes for hex display in browser
     */
    std::string getFileHexPreview(const char* path, size_t maxBytes = 256) {
        uint8_t* data = nullptr;
        size_t size = 0;
        
        if (readFile(path, &data, &size) != SdResult::OK || !data) {
            return "Error reading file";
        }
        
        size_t displaySize = (size < maxBytes) ? size : maxBytes;
        std::string result;
        result.reserve(displaySize * 3 + 100);
        
        char buf[8];
        for (size_t i = 0; i < displaySize; i++) {
            snprintf(buf, sizeof(buf), "%02X ", data[i]);
            result += buf;
            if ((i + 1) % 16 == 0) result += "\n";
        }
        
        if (size > maxBytes) {
            snprintf(buf, sizeof(buf), "\n... (%zu more bytes)", size - maxBytes);
            result += buf;
        }
        
        free(data);
        return result;
    }
    
private:
    // ========================================================
    // Internal helpers (call with mutex held)
    // ========================================================
    
    bool takeMutex() {
        if (!mutex_) {
            ESP_LOGE(TAG, "Mutex is NULL!");
            return false;
        }
        if (xSemaphoreTake(mutex_, MUTEX_TIMEOUT) != pdTRUE) {
            ESP_LOGE(TAG, "Mutex timeout after 5 seconds!");
            return false;
        }
        return true;
    }
    
    void giveMutex() {
        if (mutex_) {
            xSemaphoreGive(mutex_);
        }
    }
    
    SdResult mountInternal() {
        if (!initialized_) return SdResult::NOT_INITIALIZED;
        if (mounted_) return SdResult::OK;
        
        ESP_LOGI(TAG, "Mounting SD card...");
        
        esp_vfs_fat_sdmmc_mount_config_t mount_config = {};
        mount_config.format_if_mount_failed = false;
        mount_config.max_files = 8;
        mount_config.allocation_unit_size = 16 * 1024;
        
        sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
        slot_config.gpio_cs = static_cast<gpio_num_t>(csPin_);
        slot_config.host_id = spiHost_;
        
        sdmmc_host_t host = SDSPI_HOST_DEFAULT();
        host.slot = spiHost_;
        
        esp_err_t ret = esp_vfs_fat_sdspi_mount(MOUNT_POINT, &host, &slot_config, &mount_config, &card_);
        
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Mount FAILED: %s", esp_err_to_name(ret));
            return SdResult::NOT_MOUNTED;
        }
        
        mounted_ = true;
        ESP_LOGI(TAG, "SD card mounted successfully!");
        ESP_LOGI(TAG, "  Total: %.1f MB", getTotalBytes() / 1048576.0f);
        ESP_LOGI(TAG, "  Free:  %.1f MB", getFreeBytes() / 1048576.0f);
        
        return SdResult::OK;
    }
    
    std::string buildFullPath(const char* path) {
        std::string full = MOUNT_POINT;
        if (path && path[0] != '\0') {
            if (path[0] != '/') full += "/";
            full += path;
        }
        return full;
    }
    
    SdResult writeFileInternal(const char* path, const void* data, size_t size) {
        if (!mounted_) return SdResult::NOT_MOUNTED;
        if (!path || !data) return SdResult::INVALID_PATH;
        
        std::string fullPath = buildFullPath(path);
        if (fullPath.length() > MAX_PATH_LEN) return SdResult::PATH_TOO_LONG;
        
        // Ensure parent directory exists
        std::string dirPath = fullPath.substr(0, fullPath.find_last_of('/'));
        if (!dirPath.empty() && dirPath != MOUNT_POINT) {
            struct stat st;
            if (stat(dirPath.c_str(), &st) != 0) {
                ESP_LOGI(TAG, "Creating parent dir: %s", dirPath.c_str());
                mkdir(dirPath.c_str(), 0755);
                vTaskDelay(pdMS_TO_TICKS(10));
            }
        }
        
        // Delete existing file first (avoid FAT issues)
        struct stat st;
        if (stat(fullPath.c_str(), &st) == 0) {
            ESP_LOGI(TAG, "Removing existing: %s", fullPath.c_str());
            remove(fullPath.c_str());
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        
        // Open and write
        FILE* f = fopen(fullPath.c_str(), "wb");
        if (!f) {
            ESP_LOGE(TAG, "fopen failed: %s (errno=%d)", fullPath.c_str(), errno);
            return SdResult::OPEN_FAILED;
        }
        
        size_t written = fwrite(data, 1, size, f);
        fflush(f);
        
        int fd = fileno(f);
        if (fd >= 0) fsync(fd);
        
        fclose(f);
        
        if (written != size) {
            ESP_LOGE(TAG, "Write incomplete: %zu/%zu bytes", written, size);
            return SdResult::WRITE_FAILED;
        }
        
        // VERIFY: Re-read and check size
        vTaskDelay(pdMS_TO_TICKS(10));  // Let FAT settle
        
        struct stat st2;
        if (stat(fullPath.c_str(), &st2) != 0) {
            ESP_LOGE(TAG, "Verify failed: file not found after write!");
            verifyFailCount_++;
            return SdResult::VERIFY_FAILED;
        }
        
        if ((size_t)st2.st_size != size) {
            ESP_LOGE(TAG, "Verify failed: size mismatch (expected %zu, got %lld)", 
                     size, (long long)st2.st_size);
            verifyFailCount_++;
            return SdResult::VERIFY_FAILED;
        }
        
        ESP_LOGI(TAG, "Verified: %s (%zu bytes)", path, size);
        return SdResult::OK;
    }
    
    SdResult readFileInternal(const char* path, uint8_t** outData, size_t* outSize) {
        if (!mounted_) return SdResult::NOT_MOUNTED;
        if (!path || !outData || !outSize) return SdResult::INVALID_PATH;
        
        *outData = nullptr;
        *outSize = 0;
        
        std::string fullPath = buildFullPath(path);
        
        struct stat st;
        if (stat(fullPath.c_str(), &st) != 0) {
            ESP_LOGW(TAG, "File not found: %s", fullPath.c_str());
            return SdResult::FILE_NOT_FOUND;
        }
        
        size_t fileSize = st.st_size;
        if (fileSize == 0) {
            *outSize = 0;
            return SdResult::OK;
        }
        
        uint8_t* buffer = (uint8_t*)malloc(fileSize);
        if (!buffer) {
            ESP_LOGE(TAG, "Out of memory for %zu bytes", fileSize);
            return SdResult::OUT_OF_MEMORY;
        }
        
        FILE* f = fopen(fullPath.c_str(), "rb");
        if (!f) {
            free(buffer);
            return SdResult::OPEN_FAILED;
        }
        
        size_t bytesRead = fread(buffer, 1, fileSize, f);
        fclose(f);
        
        if (bytesRead != fileSize) {
            ESP_LOGE(TAG, "Read incomplete: %zu/%zu", bytesRead, fileSize);
            free(buffer);
            return SdResult::READ_FAILED;
        }
        
        *outData = buffer;
        *outSize = fileSize;
        return SdResult::OK;
    }
    
    SdResult readFileToBufferInternal(const char* path, void* buffer, size_t bufferSize, size_t* bytesRead) {
        if (!mounted_) return SdResult::NOT_MOUNTED;
        if (!path || !buffer) return SdResult::INVALID_PATH;
        
        std::string fullPath = buildFullPath(path);
        
        FILE* f = fopen(fullPath.c_str(), "rb");
        if (!f) return SdResult::OPEN_FAILED;
        
        *bytesRead = fread(buffer, 1, bufferSize, f);
        fclose(f);
        
        return SdResult::OK;
    }
    
    bool fileExistsInternal(const char* path) {
        if (!mounted_) return false;
        std::string fullPath = buildFullPath(path);
        struct stat st;
        return (stat(fullPath.c_str(), &st) == 0 && !S_ISDIR(st.st_mode));
    }
    
    bool dirExistsInternal(const char* path) {
        if (!mounted_) return false;
        std::string fullPath = buildFullPath(path);
        struct stat st;
        return (stat(fullPath.c_str(), &st) == 0 && S_ISDIR(st.st_mode));
    }
    
    int64_t getFileSizeInternal(const char* path) {
        if (!mounted_) return -1;
        std::string fullPath = buildFullPath(path);
        struct stat st;
        if (stat(fullPath.c_str(), &st) != 0) return -1;
        return st.st_size;
    }
    
    SdResult deleteFileInternal(const char* path) {
        if (!mounted_) return SdResult::NOT_MOUNTED;
        std::string fullPath = buildFullPath(path);
        
        if (remove(fullPath.c_str()) != 0) {
            if (errno == ENOENT) return SdResult::FILE_NOT_FOUND;
            return SdResult::DELETE_FAILED;
        }
        return SdResult::OK;
    }
    
    SdResult createDirInternal(const char* path) {
        if (!mounted_) return SdResult::NOT_MOUNTED;
        std::string fullPath = buildFullPath(path);
        
        // Check if already exists
        struct stat st;
        if (stat(fullPath.c_str(), &st) == 0) {
            if (S_ISDIR(st.st_mode)) return SdResult::OK;
            return SdResult::CREATE_DIR_FAILED;  // File exists with same name
        }
        
        // Create directory (and parents if needed)
        std::string current = MOUNT_POINT;
        std::string remaining = path;
        if (remaining[0] == '/') remaining = remaining.substr(1);
        
        size_t pos = 0;
        while ((pos = remaining.find('/')) != std::string::npos || !remaining.empty()) {
            std::string part;
            if (pos != std::string::npos) {
                part = remaining.substr(0, pos);
                remaining = remaining.substr(pos + 1);
            } else {
                part = remaining;
                remaining = "";
            }
            
            if (part.empty()) continue;
            
            current += "/" + part;
            
            struct stat st2;
            if (stat(current.c_str(), &st2) != 0) {
                if (mkdir(current.c_str(), 0755) != 0) {
                    ESP_LOGE(TAG, "mkdir failed: %s", current.c_str());
                    return SdResult::CREATE_DIR_FAILED;
                }
            }
        }
        
        return SdResult::OK;
    }
    
    SdResult listDirInternal(const char* path, std::vector<FileEntry>& entries) {
        if (!mounted_) return SdResult::NOT_MOUNTED;
        
        entries.clear();
        std::string fullPath = buildFullPath(path);
        
        DIR* dir = opendir(fullPath.c_str());
        if (!dir) {
            return SdResult::DIR_NOT_FOUND;
        }
        
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }
            
            FileEntry fe;
            fe.name = entry->d_name;
            
            // Build relative path
            if (!path || path[0] == '\0' || strcmp(path, "/") == 0) {
                fe.path = "/" + fe.name;
            } else {
                fe.path = std::string(path);
                if (fe.path.back() != '/') fe.path += "/";
                fe.path += fe.name;
            }
            
            // Get stats
            std::string entryFull = fullPath + "/" + fe.name;
            struct stat st;
            if (stat(entryFull.c_str(), &st) == 0) {
                fe.isDirectory = S_ISDIR(st.st_mode);
                fe.size = fe.isDirectory ? 0 : st.st_size;
                fe.modTime = st.st_mtime;
            } else {
                fe.isDirectory = (entry->d_type == DT_DIR);
                fe.size = 0;
                fe.modTime = 0;
            }
            
            entries.push_back(fe);
        }
        
        closedir(dir);
        return SdResult::OK;
    }
};

// Convenience macro
#define SDCARD SdCardManager::instance()

}  // namespace Storage
}  // namespace SystemAPI

#endif  // ARCOS_INCLUDE_SYSTEMAPI_STORAGE_SDCARDMANAGER_HPP_
