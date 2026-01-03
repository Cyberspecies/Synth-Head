/*****************************************************************
 * File:      Esp32HalStorage.hpp
 * Category:  src/HAL/ESP32
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    ESP32 implementation of Storage HAL interface.
 *    Supports SD card access using ESP32 SD library.
 *****************************************************************/

#ifndef ARCOS_SRC_HAL_ESP32_HAL_STORAGE_HPP_
#define ARCOS_SRC_HAL_ESP32_HAL_STORAGE_HPP_

#include "HAL/IHalStorage.hpp"
#include "HAL/IHalLog.hpp"
#include <Arduino.h>
#include <SD.h>
#include <FS.h>

namespace arcos::hal::esp32{

/** ESP32 SD Card Storage Implementation */
class Esp32HalStorage : public IHalStorage{
private:
  static constexpr const char* TAG = "STORAGE";
  
  IHalLog* log_ = nullptr;
  SdCardConfig config_;
  bool initialized_ = false;
  bool mounted_ = false;

public:
  explicit Esp32HalStorage(IHalLog* log = nullptr) : log_(log){}
  
  ~Esp32HalStorage() override{
    deinit();
  }
  
  HalResult init(const SdCardConfig& config) override{
    if(initialized_){
      if(log_) log_->warn(TAG, "Storage already initialized");
      return HalResult::ALREADY_INITIALIZED;
    }
    
    config_ = config;
    
    // Initialize SPI for SD card
    SPIClass* spi = &SPI;
    spi->begin(config_.clk_pin, config_.miso_pin, config_.mosi_pin, config_.cs_pin);
    
    initialized_ = true;
    if(log_) log_->info(TAG, "Storage initialized: CS=%d, CLK=%d, MOSI=%d, MISO=%d",
                        config_.cs_pin, config_.clk_pin, config_.mosi_pin, config_.miso_pin);
    return HalResult::OK;
  }
  
  HalResult deinit() override{
    if(!initialized_) return HalResult::NOT_INITIALIZED;
    
    if(mounted_){
      unmount();
    }
    
    initialized_ = false;
    if(log_) log_->info(TAG, "Storage deinitialized");
    return HalResult::OK;
  }
  
  bool isInitialized() const override{
    return initialized_;
  }
  
  bool isMounted() const override{
    return mounted_;
  }
  
  HalResult mount() override{
    if(!initialized_) return HalResult::NOT_INITIALIZED;
    if(mounted_) return HalResult::ALREADY_INITIALIZED;
    
    if(!SD.begin(config_.cs_pin)){
      if(log_) log_->error(TAG, "Failed to mount SD card");
      return HalResult::HARDWARE_FAULT;
    }
    
    mounted_ = true;
    
    uint8_t card_type = SD.cardType();
    const char* type_str = "Unknown";
    switch(card_type){
      case CARD_NONE: type_str = "None"; break;
      case CARD_MMC: type_str = "MMC"; break;
      case CARD_SD: type_str = "SD"; break;
      case CARD_SDHC: type_str = "SDHC"; break;
    }
    
    if(log_) log_->info(TAG, "SD card mounted: Type=%s, Size=%llu MB", 
                        type_str, SD.cardSize() / (1024 * 1024));
    return HalResult::OK;
  }
  
  HalResult unmount() override{
    if(!initialized_) return HalResult::NOT_INITIALIZED;
    if(!mounted_) return HalResult::INVALID_STATE;
    
    SD.end();
    mounted_ = false;
    
    if(log_) log_->info(TAG, "SD card unmounted");
    return HalResult::OK;
  }
  
  uint64_t getTotalSize() const override{
    if(!mounted_) return 0;
    return SD.totalBytes();
  }
  
  uint64_t getFreeSpace() const override{
    if(!mounted_) return 0;
    return SD.totalBytes() - SD.usedBytes();
  }
  
  bool fileExists(const char* path) override{
    if(!mounted_ || !path) return false;
    return SD.exists(path);
  }
  
  bool dirExists(const char* path) override{
    if(!mounted_ || !path) return false;
    File dir = SD.open(path);
    if(!dir) return false;
    bool is_dir = dir.isDirectory();
    dir.close();
    return is_dir;
  }
  
  HalResult createDir(const char* path) override{
    if(!mounted_) return HalResult::NOT_INITIALIZED;
    if(!path) return HalResult::INVALID_PARAM;
    
    if(SD.mkdir(path)){
      if(log_) log_->debug(TAG, "Directory created: %s", path);
      return HalResult::OK;
    }
    
    if(log_) log_->error(TAG, "Failed to create directory: %s", path);
    return HalResult::WRITE_FAILED;
  }
  
  HalResult deleteFile(const char* path) override{
    if(!mounted_) return HalResult::NOT_INITIALIZED;
    if(!path) return HalResult::INVALID_PARAM;
    
    if(SD.remove(path)){
      if(log_) log_->debug(TAG, "File deleted: %s", path);
      return HalResult::OK;
    }
    
    if(log_) log_->error(TAG, "Failed to delete file: %s", path);
    return HalResult::WRITE_FAILED;
  }
  
  HalResult deleteDir(const char* path) override{
    if(!mounted_) return HalResult::NOT_INITIALIZED;
    if(!path) return HalResult::INVALID_PARAM;
    
    if(SD.rmdir(path)){
      if(log_) log_->debug(TAG, "Directory deleted: %s", path);
      return HalResult::OK;
    }
    
    if(log_) log_->error(TAG, "Failed to delete directory: %s", path);
    return HalResult::WRITE_FAILED;
  }
  
  HalResult rename(const char* old_path, const char* new_path) override{
    if(!mounted_) return HalResult::NOT_INITIALIZED;
    if(!old_path || !new_path) return HalResult::INVALID_PARAM;
    
    if(SD.rename(old_path, new_path)){
      if(log_) log_->debug(TAG, "Renamed: %s -> %s", old_path, new_path);
      return HalResult::OK;
    }
    
    if(log_) log_->error(TAG, "Failed to rename: %s", old_path);
    return HalResult::WRITE_FAILED;
  }
  
  uint64_t getFileSize(const char* path) override{
    if(!mounted_ || !path) return 0;
    
    File file = SD.open(path);
    if(!file) return 0;
    
    uint64_t size = file.size();
    file.close();
    return size;
  }
};

// ============================================================
// File Handle Implementation
// ============================================================

/** ESP32 File Handle Implementation */
class Esp32HalFile : public IHalFile{
private:
  static constexpr const char* TAG = "FILE";
  
  IHalLog* log_ = nullptr;
  File file_;
  bool is_open_ = false;

public:
  explicit Esp32HalFile(IHalLog* log = nullptr) : log_(log){}
  
  ~Esp32HalFile() override{
    if(is_open_){
      close();
    }
  }
  
  HalResult open(const char* path, FileMode mode) override{
    if(is_open_){
      return HalResult::ALREADY_INITIALIZED;
    }
    if(!path){
      return HalResult::INVALID_PARAM;
    }
    
    const char* mode_str;
    switch(mode){
      case FileMode::READ: mode_str = "r"; break;
      case FileMode::WRITE: mode_str = "w"; break;
      case FileMode::APPEND: mode_str = "a"; break;
      case FileMode::READ_WRITE: mode_str = "r+"; break;
      default: return HalResult::INVALID_PARAM;
    }
    
    file_ = SD.open(path, mode_str);
    if(!file_){
      if(log_) log_->error(TAG, "Failed to open file: %s", path);
      return HalResult::READ_FAILED;
    }
    
    is_open_ = true;
    if(log_) log_->debug(TAG, "Opened file: %s (mode=%s)", path, mode_str);
    return HalResult::OK;
  }
  
  HalResult close() override{
    if(!is_open_){
      return HalResult::NOT_INITIALIZED;
    }
    
    file_.close();
    is_open_ = false;
    
    if(log_) log_->debug(TAG, "File closed");
    return HalResult::OK;
  }
  
  bool isOpen() const override{
    return is_open_;
  }
  
  HalResult read(uint8_t* buffer, size_t length, size_t* bytes_read) override{
    if(!is_open_) return HalResult::NOT_INITIALIZED;
    if(!buffer || length == 0) return HalResult::INVALID_PARAM;
    
    size_t read_count = file_.read(buffer, length);
    if(bytes_read){
      *bytes_read = read_count;
    }
    
    return HalResult::OK;
  }
  
  HalResult write(const uint8_t* data, size_t length, size_t* bytes_written) override{
    if(!is_open_) return HalResult::NOT_INITIALIZED;
    if(!data || length == 0) return HalResult::INVALID_PARAM;
    
    size_t written = file_.write(data, length);
    if(bytes_written){
      *bytes_written = written;
    }
    
    if(written != length){
      if(log_) log_->warn(TAG, "Partial write: %zu/%zu bytes", written, length);
    }
    
    return HalResult::OK;
  }
  
  HalResult seek(int64_t offset, SeekOrigin origin) override{
    if(!is_open_) return HalResult::NOT_INITIALIZED;
    
    SeekMode mode;
    switch(origin){
      case SeekOrigin::BEGIN: mode = SeekSet; break;
      case SeekOrigin::CURRENT: mode = SeekCur; break;
      case SeekOrigin::END: mode = SeekEnd; break;
      default: return HalResult::INVALID_PARAM;
    }
    
    if(!file_.seek(offset, mode)){
      return HalResult::INVALID_PARAM;
    }
    
    return HalResult::OK;
  }
  
  int64_t tell() const override{
    if(!is_open_) return -1;
    return file_.position();
  }
  
  int64_t size() const override{
    if(!is_open_) return -1;
    return file_.size();
  }
  
  HalResult flush() override{
    if(!is_open_) return HalResult::NOT_INITIALIZED;
    file_.flush();
    return HalResult::OK;
  }
  
  bool eof() const override{
    if(!is_open_) return true;
    // Note: Arduino File::available() is not const, so cast away const
    return const_cast<File&>(file_).available() == 0;
  }
};

} // namespace arcos::hal::esp32

#endif // ARCOS_SRC_HAL_ESP32_HAL_STORAGE_HPP_
