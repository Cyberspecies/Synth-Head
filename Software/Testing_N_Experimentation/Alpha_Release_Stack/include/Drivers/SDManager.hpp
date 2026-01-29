/*****************************************************************
 * @file SDManager.hpp
 * @brief SD Card Manager for ESP32
 * 
 * Provides a simple interface for SD card operations:
 * - Text file read/write
 * - Binary file read/write
 * - Directory listing
 * - File management (delete, exists, etc.)
 * 
 * Uses SPI mode for SD card communication.
 * 
 * @author ARCOS
 * @version 1.0
 *****************************************************************/

#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/unistd.h>

#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"

namespace Drivers {

//=============================================================================
// FileInfo Structure
//=============================================================================

/**
 * @brief Information about a file or directory
 */
struct FileInfo {
    std::string name;       ///< File/directory name
    size_t size;            ///< File size in bytes (0 for directories)
    bool isDirectory;       ///< True if this is a directory
    
    FileInfo() : size(0), isDirectory(false) {}
    FileInfo(const std::string& n, size_t s, bool isDir)
        : name(n), size(s), isDirectory(isDir) {}
};

//=============================================================================
// SDManager Class
//=============================================================================

/**
 * @brief Static SD card manager for ESP32
 * 
 * All methods are static for easy global access.
 * Call init() once at startup with your SPI pins.
 * 
 * Example usage:
 * @code
 *   if (SDManager::init(14, 47, 21, 48)) {
 *       SDManager::writeText("/config.yml", yamlContent);
 *       std::string content = SDManager::readText("/config.yml");
 *   }
 * @endcode
 */
class SDManager {
public:
    //=========================================================================
    // Initialization
    //=========================================================================
    
    /**
     * @brief Initialize SD card in SPI mode
     * @param miso MISO GPIO pin
     * @param mosi MOSI GPIO pin
     * @param clk CLK GPIO pin
     * @param cs CS GPIO pin
     * @return true if initialization successful
     */
    static bool init(int miso, int mosi, int clk, int cs) {
        if (s_mounted) {
            ESP_LOGW(TAG, "SD card already mounted");
            return true;
        }
        
        s_miso = miso;
        s_mosi = mosi;
        s_clk = clk;
        s_cs = cs;
        
        ESP_LOGI(TAG, "Initializing SD card (MISO=%d, MOSI=%d, CLK=%d, CS=%d)",
                 miso, mosi, clk, cs);
        
        // SPI bus configuration
        spi_bus_config_t bus_cfg = {};
        bus_cfg.mosi_io_num = mosi;
        bus_cfg.miso_io_num = miso;
        bus_cfg.sclk_io_num = clk;
        bus_cfg.quadwp_io_num = -1;
        bus_cfg.quadhd_io_num = -1;
        bus_cfg.max_transfer_sz = 4096;
        
        esp_err_t ret = spi_bus_initialize(SPI2_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
        if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
            ESP_LOGE(TAG, "Failed to initialize SPI bus: %s", esp_err_to_name(ret));
            return false;
        }
        
        // SD card slot configuration
        sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
        slot_config.gpio_cs = static_cast<gpio_num_t>(cs);
        slot_config.host_id = SPI2_HOST;
        
        // Mount configuration
        esp_vfs_fat_sdmmc_mount_config_t mount_config = {};
        mount_config.format_if_mount_failed = false;
        mount_config.max_files = 5;
        mount_config.allocation_unit_size = 16 * 1024;
        
        // Host configuration
        sdmmc_host_t host = SDSPI_HOST_DEFAULT();
        host.slot = SPI2_HOST;
        
        // Mount the filesystem
        ret = esp_vfs_fat_sdspi_mount(MOUNT_POINT, &host, &slot_config, 
                                       &mount_config, &s_card);
        
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to mount SD card: %s", esp_err_to_name(ret));
            return false;
        }
        
        s_mounted = true;
        
        // Print card info
        sdmmc_card_print_info(stdout, s_card);
        
        ESP_LOGI(TAG, "SD card mounted successfully at %s", MOUNT_POINT);
        return true;
    }
    
    /**
     * @brief Unmount SD card
     */
    static void deinit() {
        if (s_mounted) {
            esp_vfs_fat_sdcard_unmount(MOUNT_POINT, s_card);
            spi_bus_free(SPI2_HOST);
            s_mounted = false;
            s_card = nullptr;
            ESP_LOGI(TAG, "SD card unmounted");
        }
    }
    
    /**
     * @brief Check if SD card is mounted
     * @return true if mounted and ready
     */
    static bool isMounted() {
        return s_mounted;
    }
    
    //=========================================================================
    // Path Utilities
    //=========================================================================
    
    /**
     * @brief Get full path with mount point prefix
     * @param path Relative path (e.g., "/config.yml")
     * @return Full path (e.g., "/sdcard/config.yml")
     */
    static std::string getFullPath(const std::string& path) {
        if (path.empty()) return MOUNT_POINT;
        if (path[0] == '/') {
            return std::string(MOUNT_POINT) + path;
        }
        return std::string(MOUNT_POINT) + "/" + path;
    }
    
    //=========================================================================
    // Text File Operations
    //=========================================================================
    
    /**
     * @brief Read entire text file
     * @param path File path (relative to mount point)
     * @return File contents, or empty string on error
     */
    static std::string readText(const std::string& path) {
        if (!s_mounted) {
            ESP_LOGE(TAG, "SD card not mounted");
            return "";
        }
        
        std::string fullPath = getFullPath(path);
        FILE* f = fopen(fullPath.c_str(), "r");
        if (!f) {
            ESP_LOGE(TAG, "Failed to open file for reading: %s", fullPath.c_str());
            return "";
        }
        
        // Get file size
        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        fseek(f, 0, SEEK_SET);
        
        if (size <= 0) {
            fclose(f);
            return "";
        }
        
        // Read content
        std::string content;
        content.resize(size);
        size_t read = fread(&content[0], 1, size, f);
        content.resize(read);
        
        fclose(f);
        return content;
    }
    
    /**
     * @brief Write text to file (overwrites existing)
     * @param path File path (relative to mount point)
     * @param content Text content to write
     * @return true if successful
     */
    static bool writeText(const std::string& path, const std::string& content) {
        if (!s_mounted) {
            ESP_LOGE(TAG, "SD card not mounted");
            return false;
        }
        
        std::string fullPath = getFullPath(path);
        
        // Create parent directories if needed
        createParentDirs(path);
        
        FILE* f = fopen(fullPath.c_str(), "w");
        if (!f) {
            ESP_LOGE(TAG, "Failed to open file for writing: %s", fullPath.c_str());
            return false;
        }
        
        size_t written = fwrite(content.c_str(), 1, content.size(), f);
        fclose(f);
        
        if (written != content.size()) {
            ESP_LOGE(TAG, "Write incomplete: %zu of %zu bytes", written, content.size());
            return false;
        }
        
        return true;
    }
    
    /**
     * @brief Append text to file
     * @param path File path (relative to mount point)
     * @param content Text content to append
     * @return true if successful
     */
    static bool appendText(const std::string& path, const std::string& content) {
        if (!s_mounted) {
            ESP_LOGE(TAG, "SD card not mounted");
            return false;
        }
        
        std::string fullPath = getFullPath(path);
        FILE* f = fopen(fullPath.c_str(), "a");
        if (!f) {
            ESP_LOGE(TAG, "Failed to open file for appending: %s", fullPath.c_str());
            return false;
        }
        
        size_t written = fwrite(content.c_str(), 1, content.size(), f);
        fclose(f);
        
        return written == content.size();
    }
    
    //=========================================================================
    // Binary File Operations
    //=========================================================================
    
    /**
     * @brief Read entire binary file
     * @param path File path (relative to mount point)
     * @return File contents as byte vector, or empty on error
     */
    static std::vector<uint8_t> readBinary(const std::string& path) {
        if (!s_mounted) {
            ESP_LOGE(TAG, "SD card not mounted");
            return {};
        }
        
        std::string fullPath = getFullPath(path);
        FILE* f = fopen(fullPath.c_str(), "rb");
        if (!f) {
            ESP_LOGE(TAG, "Failed to open binary file: %s", fullPath.c_str());
            return {};
        }
        
        // Get file size
        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        fseek(f, 0, SEEK_SET);
        
        if (size <= 0) {
            fclose(f);
            return {};
        }
        
        // Read content
        std::vector<uint8_t> data(size);
        size_t read = fread(data.data(), 1, size, f);
        data.resize(read);
        
        fclose(f);
        return data;
    }
    
    /**
     * @brief Write binary data to file (overwrites existing)
     * @param path File path (relative to mount point)
     * @param data Pointer to binary data
     * @param size Size of data in bytes
     * @return true if successful
     */
    static bool writeBinary(const std::string& path, const uint8_t* data, size_t size) {
        if (!s_mounted) {
            ESP_LOGE(TAG, "SD card not mounted");
            return false;
        }
        
        std::string fullPath = getFullPath(path);
        
        // Create parent directories if needed
        createParentDirs(path);
        
        FILE* f = fopen(fullPath.c_str(), "wb");
        if (!f) {
            ESP_LOGE(TAG, "Failed to open file for binary writing: %s", fullPath.c_str());
            return false;
        }
        
        size_t written = fwrite(data, 1, size, f);
        fclose(f);
        
        if (written != size) {
            ESP_LOGE(TAG, "Binary write incomplete: %zu of %zu bytes", written, size);
            return false;
        }
        
        return true;
    }
    
    /**
     * @brief Write binary vector to file
     */
    static bool writeBinary(const std::string& path, const std::vector<uint8_t>& data) {
        return writeBinary(path, data.data(), data.size());
    }
    
    //=========================================================================
    // Directory Operations
    //=========================================================================
    
    /**
     * @brief List contents of a directory
     * @param path Directory path (relative to mount point)
     * @return Vector of FileInfo structures
     */
    static std::vector<FileInfo> listDirectory(const std::string& path) {
        std::vector<FileInfo> result;
        
        if (!s_mounted) {
            ESP_LOGE(TAG, "SD card not mounted");
            return result;
        }
        
        std::string fullPath = getFullPath(path);
        DIR* dir = opendir(fullPath.c_str());
        if (!dir) {
            ESP_LOGE(TAG, "Failed to open directory: %s", fullPath.c_str());
            return result;
        }
        
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            // Skip . and ..
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }
            
            FileInfo info;
            info.name = entry->d_name;
            info.isDirectory = (entry->d_type == DT_DIR);
            
            // Get file size for files
            if (!info.isDirectory) {
                std::string filePath = fullPath + "/" + info.name;
                struct stat st;
                if (stat(filePath.c_str(), &st) == 0) {
                    info.size = st.st_size;
                }
            }
            
            result.push_back(info);
        }
        
        closedir(dir);
        return result;
    }
    
    /**
     * @brief Create a directory (and parent directories if needed)
     * @param path Directory path (relative to mount point)
     * @return true if successful or already exists
     */
    static bool createDirectory(const std::string& path) {
        if (!s_mounted) return false;
        
        std::string fullPath = getFullPath(path);
        
        // Check if already exists
        struct stat st;
        if (stat(fullPath.c_str(), &st) == 0) {
            return true; // Already exists
        }
        
        // Create parent directories first
        createParentDirs(path);
        
        int ret = mkdir(fullPath.c_str(), 0755);
        return (ret == 0);
    }
    
    //=========================================================================
    // File Management
    //=========================================================================
    
    /**
     * @brief Check if file or directory exists
     * @param path Path (relative to mount point)
     * @return true if exists
     */
    static bool exists(const std::string& path) {
        if (!s_mounted) return false;
        
        std::string fullPath = getFullPath(path);
        struct stat st;
        return (stat(fullPath.c_str(), &st) == 0);
    }
    
    /**
     * @brief Delete a file
     * @param path File path (relative to mount point)
     * @return true if deleted (or didn't exist)
     */
    static bool deleteFile(const std::string& path) {
        if (!s_mounted) return false;
        
        std::string fullPath = getFullPath(path);
        
        // Check if exists
        struct stat st;
        if (stat(fullPath.c_str(), &st) != 0) {
            return true; // Doesn't exist, consider success
        }
        
        int ret = unlink(fullPath.c_str());
        if (ret != 0) {
            ESP_LOGE(TAG, "Failed to delete file: %s", fullPath.c_str());
            return false;
        }
        
        return true;
    }
    
    /**
     * @brief Delete a directory (must be empty)
     * @param path Directory path (relative to mount point)
     * @return true if deleted
     */
    static bool deleteDirectory(const std::string& path) {
        if (!s_mounted) return false;
        
        std::string fullPath = getFullPath(path);
        int ret = rmdir(fullPath.c_str());
        return (ret == 0);
    }
    
    /**
     * @brief Get file size
     * @param path File path (relative to mount point)
     * @return File size in bytes, or 0 if not found
     */
    static size_t getFileSize(const std::string& path) {
        if (!s_mounted) return 0;
        
        std::string fullPath = getFullPath(path);
        struct stat st;
        if (stat(fullPath.c_str(), &st) == 0) {
            return st.st_size;
        }
        return 0;
    }
    
    /**
     * @brief Rename/move a file
     * @param oldPath Current path
     * @param newPath New path
     * @return true if successful
     */
    static bool rename(const std::string& oldPath, const std::string& newPath) {
        if (!s_mounted) return false;
        
        std::string fullOld = getFullPath(oldPath);
        std::string fullNew = getFullPath(newPath);
        
        int ret = ::rename(fullOld.c_str(), fullNew.c_str());
        return (ret == 0);
    }

private:
    static constexpr const char* TAG = "SDManager";
    static constexpr const char* MOUNT_POINT = "/sdcard";
    
    static inline bool s_mounted = false;
    static inline sdmmc_card_t* s_card = nullptr;
    static inline int s_miso = -1;
    static inline int s_mosi = -1;
    static inline int s_clk = -1;
    static inline int s_cs = -1;
    
    /**
     * @brief Create parent directories for a path
     */
    static void createParentDirs(const std::string& path) {
        std::string fullPath = getFullPath(path);
        
        // Find and create each directory level
        size_t pos = strlen(MOUNT_POINT) + 1;
        while ((pos = fullPath.find('/', pos)) != std::string::npos) {
            std::string dir = fullPath.substr(0, pos);
            struct stat st;
            if (stat(dir.c_str(), &st) != 0) {
                mkdir(dir.c_str(), 0755);
            }
            pos++;
        }
    }
};

} // namespace Drivers
