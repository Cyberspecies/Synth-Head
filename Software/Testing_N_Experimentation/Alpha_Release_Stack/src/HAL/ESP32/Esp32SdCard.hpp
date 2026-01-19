/*****************************************************************
 * File:      Esp32SdCard.hpp
 * Category:  src/HAL/ESP32
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    ESP-IDF native SD card implementation.
 *    Uses VFS FAT filesystem for SD card access via SPI.
 *****************************************************************/

#ifndef ARCOS_SRC_HAL_ESP32_SDCARD_HPP_
#define ARCOS_SRC_HAL_ESP32_SDCARD_HPP_

#include "HAL/IHalStorage.hpp"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "esp_log.h"
#include <dirent.h>
#include <sys/stat.h>
#include <cstring>
#include <cstdio>

namespace arcos::hal::esp32 {

/**
 * @brief ESP32 SD Card HAL Implementation using ESP-IDF
 * 
 * Low-level SD card driver using SPI interface.
 * Provides basic file system operations.
 */
class Esp32SdCard : public IHalStorage {
private:
    static constexpr const char* TAG = "SDCARD_HAL";
    static constexpr const char* MOUNT_POINT = "/sdcard";
    
    SdCardConfig config_;
    bool initialized_ = false;
    bool mounted_ = false;
    sdmmc_card_t* card_ = nullptr;
    spi_host_device_t spiHost_ = SPI2_HOST;
    char cardName_[16] = "N/A";

public:
    Esp32SdCard() = default;
    
    ~Esp32SdCard() override {
        if (initialized_) {
            deinit();
        }
    }
    
    HalResult init(const SdCardConfig& config) override {
        if (initialized_) {
            ESP_LOGW(TAG, "Already initialized");
            return HalResult::ALREADY_INITIALIZED;
        }
        
        config_ = config;
        
        ESP_LOGI(TAG, "Initializing SD card SPI bus...");
        ESP_LOGI(TAG, "  MISO: GPIO%d, MOSI: GPIO%d, CLK: GPIO%d, CS: GPIO%d",
                 config_.miso_pin, config_.mosi_pin, config_.clk_pin, config_.cs_pin);
        
        // Configure SPI bus
        spi_bus_config_t bus_cfg = {};
        bus_cfg.mosi_io_num = config_.mosi_pin;
        bus_cfg.miso_io_num = config_.miso_pin;
        bus_cfg.sclk_io_num = config_.clk_pin;
        bus_cfg.quadwp_io_num = -1;
        bus_cfg.quadhd_io_num = -1;
        bus_cfg.max_transfer_sz = 4000;
        
        esp_err_t ret = spi_bus_initialize(spiHost_, &bus_cfg, SDSPI_DEFAULT_DMA);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "SPI bus init failed: %s", esp_err_to_name(ret));
            return HalResult::HARDWARE_FAULT;
        }
        
        initialized_ = true;
        ESP_LOGI(TAG, "SPI bus initialized");
        
        // Attempt to mount - return failure if mount fails
        HalResult mountResult = mount();
        if (mountResult != HalResult::OK) {
            ESP_LOGE(TAG, "Auto-mount failed (result=%d), SD card not usable", 
                     static_cast<int>(mountResult));
            // Clean up SPI bus since we can't use the card
            spi_bus_free(spiHost_);
            initialized_ = false;
            return mountResult;
        }
        
        return HalResult::OK;
    }
    
    HalResult deinit() override {
        if (!initialized_) {
            return HalResult::NOT_INITIALIZED;
        }
        
        if (mounted_) {
            unmount();
        }
        
        spi_bus_free(spiHost_);
        initialized_ = false;
        
        ESP_LOGI(TAG, "SD card deinitialized");
        return HalResult::OK;
    }
    
    bool isInitialized() const override {
        return initialized_;
    }
    
    bool isMounted() const override {
        return mounted_;
    }
    
    HalResult mount() override {
        if (!initialized_) {
            return HalResult::NOT_INITIALIZED;
        }
        if (mounted_) {
            return HalResult::ALREADY_INITIALIZED;
        }
        
        esp_vfs_fat_sdmmc_mount_config_t mount_config = {};
        mount_config.format_if_mount_failed = false;
        mount_config.max_files = 8;  // Increased for sprite operations
        mount_config.allocation_unit_size = 16 * 1024;
        
        sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
        slot_config.gpio_cs = static_cast<gpio_num_t>(config_.cs_pin);
        slot_config.host_id = spiHost_;
        
        sdmmc_host_t host = SDSPI_HOST_DEFAULT();
        host.slot = spiHost_;
        
        // Retry mount up to 3 times with delays (SD card may need time at boot)
        esp_err_t ret = ESP_FAIL;
        const int maxRetries = 3;
        for (int attempt = 1; attempt <= maxRetries; attempt++) {
            ESP_LOGI(TAG, "Mounting SD card (attempt %d/%d)...", attempt, maxRetries);
            ret = esp_vfs_fat_sdspi_mount(MOUNT_POINT, &host, &slot_config, &mount_config, &card_);
            
            if (ret == ESP_OK) {
                break;  // Success!
            }
            
            ESP_LOGW(TAG, "Mount attempt %d failed: %s", attempt, esp_err_to_name(ret));
            
            if (attempt < maxRetries) {
                // Wait a bit before retrying
                vTaskDelay(pdMS_TO_TICKS(500));
            }
        }
        
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Mount failed after %d attempts: %s", maxRetries, esp_err_to_name(ret));
            return HalResult::HARDWARE_FAULT;
        }
        
        mounted_ = true;
        
        // Store card name
        if (card_ && card_->cid.name[0]) {
            strncpy(cardName_, card_->cid.name, sizeof(cardName_) - 1);
            cardName_[sizeof(cardName_) - 1] = '\0';
        }
        
        ESP_LOGI(TAG, "SD card mounted: %s, %llu MB", 
                 cardName_, getTotalSize() / (1024 * 1024));
        
        return HalResult::OK;
    }
    
    HalResult unmount() override {
        if (!mounted_) {
            return HalResult::INVALID_STATE;
        }
        
        esp_vfs_fat_sdcard_unmount(MOUNT_POINT, card_);
        card_ = nullptr;
        mounted_ = false;
        strcpy(cardName_, "N/A");
        
        ESP_LOGI(TAG, "SD card unmounted");
        return HalResult::OK;
    }
    
    uint64_t getTotalSize() const override {
        if (!mounted_ || !card_) return 0;
        return (uint64_t)card_->csd.capacity * card_->csd.sector_size;
    }
    
    uint64_t getFreeSpace() const override {
        if (!mounted_) return 0;
        
        FATFS* fs;
        DWORD fre_clust;
        if (f_getfree("0:", &fre_clust, &fs) != FR_OK) {
            return 0;
        }
        return (uint64_t)fre_clust * fs->csize * 512;
    }
    
    bool fileExists(const char* path) override {
        if (!mounted_ || !path) return false;
        
        char fullPath[160];
        buildFullPath(fullPath, sizeof(fullPath), path);
        
        struct stat st;
        return (stat(fullPath, &st) == 0 && !S_ISDIR(st.st_mode));
    }
    
    bool dirExists(const char* path) override {
        if (!mounted_ || !path) return false;
        
        char fullPath[160];
        buildFullPath(fullPath, sizeof(fullPath), path);
        
        struct stat st;
        return (stat(fullPath, &st) == 0 && S_ISDIR(st.st_mode));
    }
    
    HalResult createDir(const char* path) override {
        if (!mounted_) return HalResult::NOT_INITIALIZED;
        if (!path) return HalResult::INVALID_PARAM;
        
        char fullPath[160];
        buildFullPath(fullPath, sizeof(fullPath), path);
        
        if (mkdir(fullPath, 0755) == 0) {
            return HalResult::OK;
        }
        
        // Check if already exists
        if (dirExists(path)) {
            return HalResult::OK;
        }
        
        return HalResult::WRITE_FAILED;
    }
    
    HalResult deleteFile(const char* path) override {
        if (!mounted_) return HalResult::NOT_INITIALIZED;
        if (!path) return HalResult::INVALID_PARAM;
        
        char fullPath[160];
        buildFullPath(fullPath, sizeof(fullPath), path);
        
        if (remove(fullPath) == 0) {
            return HalResult::OK;
        }
        return HalResult::WRITE_FAILED;
    }
    
    HalResult deleteDir(const char* path) override {
        if (!mounted_) return HalResult::NOT_INITIALIZED;
        if (!path) return HalResult::INVALID_PARAM;
        
        char fullPath[160];
        buildFullPath(fullPath, sizeof(fullPath), path);
        
        if (rmdir(fullPath) == 0) {
            return HalResult::OK;
        }
        return HalResult::WRITE_FAILED;
    }
    
    HalResult rename(const char* oldPath, const char* newPath) override {
        if (!mounted_) return HalResult::NOT_INITIALIZED;
        if (!oldPath || !newPath) return HalResult::INVALID_PARAM;
        
        char fullOld[160], fullNew[160];
        buildFullPath(fullOld, sizeof(fullOld), oldPath);
        buildFullPath(fullNew, sizeof(fullNew), newPath);
        
        if (::rename(fullOld, fullNew) == 0) {
            return HalResult::OK;
        }
        return HalResult::WRITE_FAILED;
    }
    
    uint64_t getFileSize(const char* path) override {
        if (!mounted_ || !path) return 0;
        
        char fullPath[160];
        buildFullPath(fullPath, sizeof(fullPath), path);
        
        struct stat st;
        if (stat(fullPath, &st) == 0) {
            return st.st_size;
        }
        return 0;
    }
    
    HalResult format() override {
        if (!initialized_) return HalResult::NOT_INITIALIZED;
        
        ESP_LOGW(TAG, "Formatting SD card...");
        
        // Unmount first
        if (mounted_) {
            unmount();
        }
        
        // Mount with format option
        esp_vfs_fat_sdmmc_mount_config_t mount_config = {};
        mount_config.format_if_mount_failed = true;
        mount_config.max_files = 8;  // Match normal mount
        mount_config.allocation_unit_size = 16 * 1024;
        
        sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
        slot_config.gpio_cs = static_cast<gpio_num_t>(config_.cs_pin);
        slot_config.host_id = spiHost_;
        
        sdmmc_host_t host = SDSPI_HOST_DEFAULT();
        host.slot = spiHost_;
        
        // Force format by making mount fail first, then retry
        esp_err_t ret = esp_vfs_fat_sdspi_mount(MOUNT_POINT, &host, &slot_config, &mount_config, &card_);
        
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Format failed: %s", esp_err_to_name(ret));
            return HalResult::WRITE_FAILED;
        }
        
        mounted_ = true;
        if (card_ && card_->cid.name[0]) {
            strncpy(cardName_, card_->cid.name, sizeof(cardName_) - 1);
        }
        
        ESP_LOGI(TAG, "SD card formatted successfully");
        return HalResult::OK;
    }
    
    const char* getCardName() const override {
        return cardName_;
    }
    
    const char* getMountPoint() const override {
        return MOUNT_POINT;
    }
    
    // ========== Additional HAL-level helpers ==========
    
    /**
     * @brief Build full path from relative path
     * Smart handling: if path already starts with mount point, use as-is
     */
    void buildFullPath(char* dest, size_t destSize, const char* relativePath) const {
        if (!relativePath) {
            snprintf(dest, destSize, "%s", MOUNT_POINT);
            return;
        }
        
        // If path already starts with mount point, use as-is (no double prefix)
        if (strncmp(relativePath, MOUNT_POINT, strlen(MOUNT_POINT)) == 0) {
            snprintf(dest, destSize, "%s", relativePath);
        } else if (relativePath[0] == '/') {
            snprintf(dest, destSize, "%s%s", MOUNT_POINT, relativePath);
        } else {
            snprintf(dest, destSize, "%s/%s", MOUNT_POINT, relativePath);
        }
    }
    
    /**
     * @brief Delete directory contents recursively
     */
    HalResult clearAllFiles() {
        if (!mounted_) return HalResult::NOT_INITIALIZED;
        
        ESP_LOGW(TAG, "Clearing all files...");
        return deleteContentsRecursive(MOUNT_POINT);
    }

private:
    HalResult deleteContentsRecursive(const char* dirPath) {
        DIR* dir = opendir(dirPath);
        if (!dir) return HalResult::OK;
        
        struct dirent* entry;
        char entryPath[256];
        
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
            
            snprintf(entryPath, sizeof(entryPath), "%s/%s", dirPath, safeName);
            
            struct stat st;
            if (stat(entryPath, &st) == 0) {
                if (S_ISDIR(st.st_mode)) {
                    deleteContentsRecursive(entryPath);
                    rmdir(entryPath);
                } else {
                    remove(entryPath);
                }
            }
        }
        
        closedir(dir);
        return HalResult::OK;
    }
};

} // namespace arcos::hal::esp32

#endif // ARCOS_SRC_HAL_ESP32_SDCARD_HPP_
