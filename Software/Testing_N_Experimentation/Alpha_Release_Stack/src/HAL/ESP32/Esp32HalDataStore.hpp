/*****************************************************************
 * File:      Esp32HalDataStore.hpp
 * Category:  src/HAL/ESP32
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    ESP32 implementation of Data Store HAL interface.
 *    Uses ESP32 NVS (Non-Volatile Storage) for persistent
 *    key-value storage.
 *****************************************************************/

#ifndef ARCOS_SRC_HAL_ESP32_HAL_DATASTORE_HPP_
#define ARCOS_SRC_HAL_ESP32_HAL_DATASTORE_HPP_

#include "HAL/IHalDataStore.hpp"
#include "HAL/IHalLog.hpp"
#include <nvs_flash.h>
#include <nvs.h>
#include <cstring>

namespace arcos::hal::esp32{

/**
 * @brief ESP32 NVS Data Store Implementation
 * 
 * Provides persistent key-value storage using ESP32's NVS
 * (Non-Volatile Storage) partition.
 */
class Esp32HalDataStore : public IHalDataStore{
private:
  static constexpr const char* TAG = "DATASTORE";
  
  IHalLog* log_ = nullptr;
  nvs_handle_t handle_ = 0;
  bool initialized_ = false;
  char namespace_[16] = {0};

public:
  explicit Esp32HalDataStore(IHalLog* log = nullptr) : log_(log){}
  
  ~Esp32HalDataStore() override{
    deinit();
  }
  
  HalResult init(const DataStoreConfig& config = DataStoreConfig{}) override{
    if(initialized_){
      if(log_) log_->warn(TAG, "DataStore already initialized");
      return HalResult::ALREADY_INITIALIZED;
    }
    
    // Initialize NVS flash if not already done
    esp_err_t err = nvs_flash_init();
    if(err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND){
      // NVS partition was truncated and needs to be erased
      if(log_) log_->warn(TAG, "Erasing NVS flash...");
      ESP_ERROR_CHECK(nvs_flash_erase());
      err = nvs_flash_init();
    }
    
    if(err != ESP_OK){
      if(log_) log_->error(TAG, "NVS flash init failed: %s", esp_err_to_name(err));
      return HalResult::HARDWARE_FAULT;
    }
    
    // Store namespace name
    strncpy(namespace_, config.namespace_name, sizeof(namespace_) - 1);
    
    // Open NVS handle
    nvs_open_mode_t mode = config.read_only ? NVS_READONLY : NVS_READWRITE;
    err = nvs_open(namespace_, mode, &handle_);
    
    if(err != ESP_OK){
      if(log_) log_->error(TAG, "NVS open failed: %s", esp_err_to_name(err));
      return HalResult::HARDWARE_FAULT;
    }
    
    initialized_ = true;
    if(log_) log_->info(TAG, "DataStore initialized (namespace: %s)", namespace_);
    return HalResult::OK;
  }
  
  HalResult deinit() override{
    if(!initialized_) return HalResult::NOT_INITIALIZED;
    
    nvs_close(handle_);
    handle_ = 0;
    initialized_ = false;
    
    if(log_) log_->info(TAG, "DataStore deinitialized");
    return HalResult::OK;
  }
  
  bool isInitialized() const override{
    return initialized_;
  }
  
  // --------------------------------------------------------
  // String Operations
  // --------------------------------------------------------
  
  HalResult setString(const char* key, const char* value) override{
    if(!initialized_) return HalResult::NOT_INITIALIZED;
    if(!key || !value) return HalResult::INVALID_PARAM;
    
    esp_err_t err = nvs_set_str(handle_, key, value);
    if(err != ESP_OK){
      if(log_) log_->error(TAG, "setString failed for '%s': %s", key, esp_err_to_name(err));
      return HalResult::WRITE_FAILED;
    }
    
    // Commit changes
    err = nvs_commit(handle_);
    if(err != ESP_OK){
      if(log_) log_->error(TAG, "commit failed: %s", esp_err_to_name(err));
      return HalResult::WRITE_FAILED;
    }
    
    if(log_) log_->debug(TAG, "setString: %s = %s", key, value);
    return HalResult::OK;
  }
  
  HalResult getString(const char* key, char* value, size_t max_len) override{
    if(!initialized_) return HalResult::NOT_INITIALIZED;
    if(!key || !value || max_len == 0) return HalResult::INVALID_PARAM;
    
    size_t required_size = max_len;
    esp_err_t err = nvs_get_str(handle_, key, value, &required_size);
    
    if(err == ESP_ERR_NVS_NOT_FOUND){
      return HalResult::KEY_NOT_FOUND;
    }
    
    if(err != ESP_OK){
      if(log_) log_->error(TAG, "getString failed for '%s': %s", key, esp_err_to_name(err));
      return HalResult::READ_FAILED;
    }
    
    if(log_) log_->debug(TAG, "getString: %s = %s", key, value);
    return HalResult::OK;
  }
  
  // --------------------------------------------------------
  // Integer Operations
  // --------------------------------------------------------
  
  HalResult setU8(const char* key, uint8_t value) override{
    if(!initialized_) return HalResult::NOT_INITIALIZED;
    if(!key) return HalResult::INVALID_PARAM;
    
    esp_err_t err = nvs_set_u8(handle_, key, value);
    if(err != ESP_OK) return HalResult::WRITE_FAILED;
    
    err = nvs_commit(handle_);
    return (err == ESP_OK) ? HalResult::OK : HalResult::WRITE_FAILED;
  }
  
  HalResult getU8(const char* key, uint8_t* value) override{
    if(!initialized_) return HalResult::NOT_INITIALIZED;
    if(!key || !value) return HalResult::INVALID_PARAM;
    
    esp_err_t err = nvs_get_u8(handle_, key, value);
    if(err == ESP_ERR_NVS_NOT_FOUND) return HalResult::KEY_NOT_FOUND;
    return (err == ESP_OK) ? HalResult::OK : HalResult::READ_FAILED;
  }
  
  HalResult setU16(const char* key, uint16_t value) override{
    if(!initialized_) return HalResult::NOT_INITIALIZED;
    if(!key) return HalResult::INVALID_PARAM;
    
    esp_err_t err = nvs_set_u16(handle_, key, value);
    if(err != ESP_OK) return HalResult::WRITE_FAILED;
    
    err = nvs_commit(handle_);
    return (err == ESP_OK) ? HalResult::OK : HalResult::WRITE_FAILED;
  }
  
  HalResult getU16(const char* key, uint16_t* value) override{
    if(!initialized_) return HalResult::NOT_INITIALIZED;
    if(!key || !value) return HalResult::INVALID_PARAM;
    
    esp_err_t err = nvs_get_u16(handle_, key, value);
    if(err == ESP_ERR_NVS_NOT_FOUND) return HalResult::KEY_NOT_FOUND;
    return (err == ESP_OK) ? HalResult::OK : HalResult::READ_FAILED;
  }
  
  HalResult setU32(const char* key, uint32_t value) override{
    if(!initialized_) return HalResult::NOT_INITIALIZED;
    if(!key) return HalResult::INVALID_PARAM;
    
    esp_err_t err = nvs_set_u32(handle_, key, value);
    if(err != ESP_OK) return HalResult::WRITE_FAILED;
    
    err = nvs_commit(handle_);
    return (err == ESP_OK) ? HalResult::OK : HalResult::WRITE_FAILED;
  }
  
  HalResult getU32(const char* key, uint32_t* value) override{
    if(!initialized_) return HalResult::NOT_INITIALIZED;
    if(!key || !value) return HalResult::INVALID_PARAM;
    
    esp_err_t err = nvs_get_u32(handle_, key, value);
    if(err == ESP_ERR_NVS_NOT_FOUND) return HalResult::KEY_NOT_FOUND;
    return (err == ESP_OK) ? HalResult::OK : HalResult::READ_FAILED;
  }
  
  HalResult setU64(const char* key, uint64_t value) override{
    if(!initialized_) return HalResult::NOT_INITIALIZED;
    if(!key) return HalResult::INVALID_PARAM;
    
    esp_err_t err = nvs_set_u64(handle_, key, value);
    if(err != ESP_OK) return HalResult::WRITE_FAILED;
    
    err = nvs_commit(handle_);
    return (err == ESP_OK) ? HalResult::OK : HalResult::WRITE_FAILED;
  }
  
  HalResult getU64(const char* key, uint64_t* value) override{
    if(!initialized_) return HalResult::NOT_INITIALIZED;
    if(!key || !value) return HalResult::INVALID_PARAM;
    
    esp_err_t err = nvs_get_u64(handle_, key, value);
    if(err == ESP_ERR_NVS_NOT_FOUND) return HalResult::KEY_NOT_FOUND;
    return (err == ESP_OK) ? HalResult::OK : HalResult::READ_FAILED;
  }
  
  // --------------------------------------------------------
  // Signed Integer Operations
  // --------------------------------------------------------
  
  HalResult setI32(const char* key, int32_t value) override{
    if(!initialized_) return HalResult::NOT_INITIALIZED;
    if(!key) return HalResult::INVALID_PARAM;
    
    esp_err_t err = nvs_set_i32(handle_, key, value);
    if(err != ESP_OK) return HalResult::WRITE_FAILED;
    
    err = nvs_commit(handle_);
    return (err == ESP_OK) ? HalResult::OK : HalResult::WRITE_FAILED;
  }
  
  HalResult getI32(const char* key, int32_t* value) override{
    if(!initialized_) return HalResult::NOT_INITIALIZED;
    if(!key || !value) return HalResult::INVALID_PARAM;
    
    esp_err_t err = nvs_get_i32(handle_, key, value);
    if(err == ESP_ERR_NVS_NOT_FOUND) return HalResult::KEY_NOT_FOUND;
    return (err == ESP_OK) ? HalResult::OK : HalResult::READ_FAILED;
  }
  
  // --------------------------------------------------------
  // Binary Blob Operations
  // --------------------------------------------------------
  
  HalResult setBlob(const char* key, const void* data, size_t length) override{
    if(!initialized_) return HalResult::NOT_INITIALIZED;
    if(!key || !data || length == 0) return HalResult::INVALID_PARAM;
    
    esp_err_t err = nvs_set_blob(handle_, key, data, length);
    if(err != ESP_OK){
      if(log_) log_->error(TAG, "setBlob failed for '%s': %s", key, esp_err_to_name(err));
      return HalResult::WRITE_FAILED;
    }
    
    err = nvs_commit(handle_);
    return (err == ESP_OK) ? HalResult::OK : HalResult::WRITE_FAILED;
  }
  
  HalResult getBlob(const char* key, void* data, size_t* length) override{
    if(!initialized_) return HalResult::NOT_INITIALIZED;
    if(!key || !data || !length || *length == 0) return HalResult::INVALID_PARAM;
    
    esp_err_t err = nvs_get_blob(handle_, key, data, length);
    if(err == ESP_ERR_NVS_NOT_FOUND) return HalResult::KEY_NOT_FOUND;
    
    if(err != ESP_OK){
      if(log_) log_->error(TAG, "getBlob failed for '%s': %s", key, esp_err_to_name(err));
      return HalResult::READ_FAILED;
    }
    
    return HalResult::OK;
  }
  
  // --------------------------------------------------------
  // Key Management
  // --------------------------------------------------------
  
  bool keyExists(const char* key) override{
    if(!initialized_ || !key) return false;
    
    // Try to get the size of a blob to check if key exists
    size_t size = 0;
    esp_err_t err = nvs_get_str(handle_, key, nullptr, &size);
    
    // Key exists if we get OK or buffer too small (ESP_ERR_NVS_INVALID_LENGTH)
    if(err == ESP_OK || err == ESP_ERR_NVS_INVALID_LENGTH){
      return true;
    }
    
    // Try as other types
    uint8_t u8_val;
    err = nvs_get_u8(handle_, key, &u8_val);
    if(err == ESP_OK) return true;
    
    uint16_t u16_val;
    err = nvs_get_u16(handle_, key, &u16_val);
    if(err == ESP_OK) return true;
    
    uint32_t u32_val;
    err = nvs_get_u32(handle_, key, &u32_val);
    if(err == ESP_OK) return true;
    
    return false;
  }
  
  HalResult eraseKey(const char* key) override{
    if(!initialized_) return HalResult::NOT_INITIALIZED;
    if(!key) return HalResult::INVALID_PARAM;
    
    esp_err_t err = nvs_erase_key(handle_, key);
    if(err == ESP_ERR_NVS_NOT_FOUND) return HalResult::KEY_NOT_FOUND;
    
    if(err != ESP_OK){
      if(log_) log_->error(TAG, "eraseKey failed for '%s': %s", key, esp_err_to_name(err));
      return HalResult::WRITE_FAILED;
    }
    
    err = nvs_commit(handle_);
    return (err == ESP_OK) ? HalResult::OK : HalResult::WRITE_FAILED;
  }
  
  HalResult eraseAll() override{
    if(!initialized_) return HalResult::NOT_INITIALIZED;
    
    esp_err_t err = nvs_erase_all(handle_);
    if(err != ESP_OK){
      if(log_) log_->error(TAG, "eraseAll failed: %s", esp_err_to_name(err));
      return HalResult::WRITE_FAILED;
    }
    
    err = nvs_commit(handle_);
    if(log_) log_->info(TAG, "All keys erased from namespace '%s'", namespace_);
    return (err == ESP_OK) ? HalResult::OK : HalResult::WRITE_FAILED;
  }
  
  HalResult commit() override{
    if(!initialized_) return HalResult::NOT_INITIALIZED;
    
    esp_err_t err = nvs_commit(handle_);
    return (err == ESP_OK) ? HalResult::OK : HalResult::WRITE_FAILED;
  }
};

} // namespace arcos::hal::esp32

#endif // ARCOS_SRC_HAL_ESP32_HAL_DATASTORE_HPP_
