/*****************************************************************
 * File:      StorageManager.hpp
 * Category:  include/SystemAPI/Storage
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Unified storage manager that orchestrates data between
 *    Flash (NVS) and SD Card storage with intelligent placement.
 *    
 * Storage Strategy:
 *    FLASH (NVS) - Small, critical, fast-access:
 *      - WiFi credentials (SSID, password)
 *      - Authentication settings
 *      - Device identity (name, ID)
 *      - Basic system settings
 *    
 *    SD CARD - Large, user data, can be dynamic:
 *      - IMU calibration data
 *      - Sprites and pixel data
 *      - Animation configurations
 *      - Equations/formulas
 *      - User preferences
 *****************************************************************/

#ifndef ARCOS_INCLUDE_SYSTEMAPI_STORAGE_STORAGE_MANAGER_HPP_
#define ARCOS_INCLUDE_SYSTEMAPI_STORAGE_STORAGE_MANAGER_HPP_

#include "SystemAPI/Utils/FileSystemService.hpp"
#include "nvs_flash.h"
#include "nvs.h"
#include "cJSON.h"
#include "esp_log.h"

#include <cstdint>
#include <cstring>
#include <functional>

namespace SystemAPI {
namespace Storage {

static const char* STORAGE_TAG = "StorageManager";

// ============================================================
// SD Card Directory Structure
// ============================================================

namespace Paths {
    // Root directories
    static const char* CALIBRATION_DIR = "/sdcard/calibration";
    static const char* SPRITES_DIR = "/sdcard/sprites";
    static const char* ANIMATIONS_DIR = "/sdcard/animations";
    static const char* CONFIGS_DIR = "/sdcard/configs";
    static const char* EQUATIONS_DIR = "/sdcard/equations";
    static const char* CACHE_DIR = "/sdcard/cache";
    
    // Index files
    static const char* SPRITE_INDEX = "/sdcard/sprites/index.json";
    static const char* ANIMATION_INDEX = "/sdcard/animations/index.json";
    static const char* CONFIG_INDEX = "/sdcard/configs/index.json";
    static const char* EQUATION_INDEX = "/sdcard/equations/index.json";
    
    // Calibration files
    static const char* IMU_CALIBRATION = "/sdcard/calibration/imu.json";
    static const char* BME_CALIBRATION = "/sdcard/calibration/bme.json";
    static const char* DISPLAY_CALIBRATION = "/sdcard/calibration/display.json";
}

// ============================================================
// Calibration Data Structures
// ============================================================

/** IMU calibration data - stored on SD card */
struct ImuCalibrationData {
    bool valid = false;
    float matrix[9] = {1,0,0, 0,1,0, 0,0,1};  // 3x3 rotation matrix
    float gyroOffset[3] = {0, 0, 0};           // Gyro zero offsets
    float accelBias[3] = {0, 0, 0};            // Accelerometer biases
    uint32_t timestamp = 0;                     // When calibration was done
    
    void reset() {
        valid = false;
        matrix[0] = 1; matrix[1] = 0; matrix[2] = 0;
        matrix[3] = 0; matrix[4] = 1; matrix[5] = 0;
        matrix[6] = 0; matrix[7] = 0; matrix[8] = 1;
        gyroOffset[0] = gyroOffset[1] = gyroOffset[2] = 0;
        accelBias[0] = accelBias[1] = accelBias[2] = 0;
        timestamp = 0;
    }
};

// ============================================================
// Storage Manager (Singleton)
// ============================================================

class StorageManager {
private:
    bool initialized_ = false;
    bool sdCardReady_ = false;
    
    // Cached calibration data (loaded from SD on init)
    ImuCalibrationData imuCalib_;
    
    StorageManager() = default;
    
public:
    // Delete copy/move
    StorageManager(const StorageManager&) = delete;
    StorageManager& operator=(const StorageManager&) = delete;
    
    /** Get singleton instance */
    static StorageManager& instance() {
        static StorageManager inst;
        return inst;
    }
    
    // ========== Initialization ==========
    
    /**
     * @brief Initialize storage manager
     * Creates directory structure on SD card if needed
     */
    bool init() {
        if (initialized_) return true;
        
        // Check if SD card is ready
        auto& fs = Utils::FileSystemService::instance();
        sdCardReady_ = fs.isReady() && fs.isMounted();
        
        if (sdCardReady_) {
            createDirectoryStructure();
            loadCachedData();
            ESP_LOGI(STORAGE_TAG, "Storage manager initialized with SD card");
        } else {
            ESP_LOGW(STORAGE_TAG, "Storage manager initialized without SD card - using defaults");
        }
        
        initialized_ = true;
        return true;
    }
    
    /** Check if SD card storage is available */
    bool hasSDCard() const { return sdCardReady_; }
    
    // ========== IMU Calibration ==========
    
    /** Get IMU calibration data (from cache) */
    const ImuCalibrationData& getImuCalibration() const {
        return imuCalib_;
    }
    
    /** Save IMU calibration to SD card */
    bool saveImuCalibration(const ImuCalibrationData& calib) {
        if (!sdCardReady_) return false;
        
        imuCalib_ = calib;
        
        cJSON* root = cJSON_CreateObject();
        cJSON_AddBoolToObject(root, "valid", calib.valid);
        cJSON_AddNumberToObject(root, "timestamp", calib.timestamp);
        
        // Matrix as array
        cJSON* matrix = cJSON_CreateArray();
        for (int i = 0; i < 9; i++) {
            cJSON_AddItemToArray(matrix, cJSON_CreateNumber(calib.matrix[i]));
        }
        cJSON_AddItemToObject(root, "matrix", matrix);
        
        // Gyro offsets
        cJSON* gyro = cJSON_CreateArray();
        for (int i = 0; i < 3; i++) {
            cJSON_AddItemToArray(gyro, cJSON_CreateNumber(calib.gyroOffset[i]));
        }
        cJSON_AddItemToObject(root, "gyroOffset", gyro);
        
        // Accel bias
        cJSON* accel = cJSON_CreateArray();
        for (int i = 0; i < 3; i++) {
            cJSON_AddItemToArray(accel, cJSON_CreateNumber(calib.accelBias[i]));
        }
        cJSON_AddItemToObject(root, "accelBias", accel);
        
        char* json = cJSON_Print(root);
        cJSON_Delete(root);
        
        bool success = false;
        if (json) {
            success = Utils::FileSystemService::instance().writeFile(
                Paths::IMU_CALIBRATION, json, strlen(json));
            free(json);
        }
        
        if (success) {
            ESP_LOGI(STORAGE_TAG, "IMU calibration saved to SD card");
        }
        return success;
    }
    
    /** Load IMU calibration from SD card (into cache) */
    bool loadImuCalibration() {
        if (!sdCardReady_) return false;
        
        auto& fs = Utils::FileSystemService::instance();
        
        // Check if file exists
        char* data = nullptr;
        size_t size = 0;
        if (!fs.readFile(Paths::IMU_CALIBRATION, &data, &size) || !data) {
            ESP_LOGI(STORAGE_TAG, "No IMU calibration file found");
            return false;
        }
        
        cJSON* root = cJSON_Parse(data);
        free(data);
        
        if (!root) {
            ESP_LOGE(STORAGE_TAG, "Failed to parse IMU calibration JSON");
            return false;
        }
        
        // Parse data
        cJSON* valid = cJSON_GetObjectItem(root, "valid");
        if (valid) imuCalib_.valid = cJSON_IsTrue(valid);
        
        cJSON* timestamp = cJSON_GetObjectItem(root, "timestamp");
        if (timestamp) imuCalib_.timestamp = (uint32_t)timestamp->valuedouble;
        
        cJSON* matrix = cJSON_GetObjectItem(root, "matrix");
        if (matrix && cJSON_IsArray(matrix)) {
            int i = 0;
            cJSON* item;
            cJSON_ArrayForEach(item, matrix) {
                if (i < 9) imuCalib_.matrix[i++] = (float)item->valuedouble;
            }
        }
        
        cJSON* gyro = cJSON_GetObjectItem(root, "gyroOffset");
        if (gyro && cJSON_IsArray(gyro)) {
            int i = 0;
            cJSON* item;
            cJSON_ArrayForEach(item, gyro) {
                if (i < 3) imuCalib_.gyroOffset[i++] = (float)item->valuedouble;
            }
        }
        
        cJSON* accel = cJSON_GetObjectItem(root, "accelBias");
        if (accel && cJSON_IsArray(accel)) {
            int i = 0;
            cJSON* item;
            cJSON_ArrayForEach(item, accel) {
                if (i < 3) imuCalib_.accelBias[i++] = (float)item->valuedouble;
            }
        }
        
        cJSON_Delete(root);
        
        ESP_LOGI(STORAGE_TAG, "IMU calibration loaded from SD card (valid=%d)", imuCalib_.valid);
        return imuCalib_.valid;
    }
    
    /** 
     * Load IMU calibration from SD card into provided struct
     * @param outCalib Reference to output calibration data
     * @return true if valid calibration was loaded
     */
    bool loadImuCalibration(ImuCalibrationData& outCalib) {
        if (loadImuCalibration()) {
            outCalib = imuCalib_;
            return outCalib.valid;
        }
        return false;
    }
    
    /** Clear IMU calibration */
    bool clearImuCalibration() {
        imuCalib_.reset();
        
        if (sdCardReady_) {
            // Delete the file
            Utils::FileSystemService::instance().deleteFile(Paths::IMU_CALIBRATION);
        }
        return true;
    }
    
    // ========== Generic JSON File Operations ==========
    
    /**
     * @brief Save JSON data to SD card path
     * @param path Relative path (will be prefixed with /sdcard)
     * @param json JSON string
     */
    bool saveJson(const char* path, const char* json) {
        if (!sdCardReady_ || !json) return false;
        return Utils::FileSystemService::instance().writeFile(path, json, strlen(json));
    }
    
    /**
     * @brief Load JSON data from SD card
     * @param path Relative path
     * @param outJson Output buffer (caller must free)
     * @param outSize Output size
     */
    bool loadJson(const char* path, char** outJson, size_t* outSize) {
        if (!sdCardReady_) return false;
        return Utils::FileSystemService::instance().readFile(path, outJson, outSize);
    }
    
    /**
     * @brief Delete file from SD card
     */
    bool deleteFile(const char* path) {
        if (!sdCardReady_) return false;
        return Utils::FileSystemService::instance().deleteFile(path);
    }
    
    /**
     * @brief Check if file exists
     */
    bool fileExists(const char* path) {
        if (!sdCardReady_) return false;
        return Utils::FileSystemService::instance().fileExists(path);
    }

private:
    /**
     * @brief Create SD card directory structure
     */
    void createDirectoryStructure() {
        auto& fs = Utils::FileSystemService::instance();
        
        // Create all required directories
        const char* dirs[] = {
            Paths::CALIBRATION_DIR,
            Paths::SPRITES_DIR,
            Paths::ANIMATIONS_DIR,
            Paths::CONFIGS_DIR,
            Paths::EQUATIONS_DIR,
            Paths::CACHE_DIR
        };
        
        for (const char* dir : dirs) {
            fs.createDir(dir);
        }
        
        ESP_LOGI(STORAGE_TAG, "SD card directory structure created");
    }
    
    /**
     * @brief Load cached data from SD card on init
     */
    void loadCachedData() {
        // Load IMU calibration into cache
        loadImuCalibration();
    }
};

} // namespace Storage
} // namespace SystemAPI

#endif // ARCOS_INCLUDE_SYSTEMAPI_STORAGE_STORAGE_MANAGER_HPP_
