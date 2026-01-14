/*****************************************************************
 * File:      IHalStorage.hpp
 * Category:  include/HAL
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Storage Hardware Abstraction Layer interface.
 *    Provides platform-independent access to storage media
 *    including SD cards and flash memory.
 * 
 * Note:
 *    This is a storage HAL interface. The middleware layer will
 *    use this to build file system and data logging services.
 *****************************************************************/

#ifndef ARCOS_INCLUDE_HAL_IHAL_STORAGE_HPP_
#define ARCOS_INCLUDE_HAL_IHAL_STORAGE_HPP_

#include "HalTypes.hpp"

namespace arcos::hal{

// ============================================================
// Storage Types
// ============================================================

/** Storage type enumeration */
enum class StorageType : uint8_t{
  SD_CARD,
  SPI_FLASH,
  INTERNAL_FLASH,
  EEPROM
};

/** File open modes */
enum class FileMode : uint8_t{
  READ,
  WRITE,
  APPEND,
  READ_WRITE
};

/** File seek origin */
enum class SeekOrigin : uint8_t{
  BEGIN,
  CURRENT,
  END
};

// ============================================================
// SD Card Configuration
// ============================================================

/** SD card configuration */
struct SdCardConfig{
  gpio_pin_t miso_pin = 0;
  gpio_pin_t mosi_pin = 0;
  gpio_pin_t clk_pin = 0;
  gpio_pin_t cs_pin = 0;
  uint32_t frequency = 20000000;  // 20 MHz default
};

// ============================================================
// Storage Interface
// ============================================================

/** Storage Hardware Abstraction Interface
 * 
 * Provides platform-independent access to storage media.
 * Abstracts file operations for middleware use.
 */
class IHalStorage{
public:
  virtual ~IHalStorage() = default;
  
  /** Initialize storage
   * @param config SD card configuration
   * @return HalResult::OK on success
   */
  virtual HalResult init(const SdCardConfig& config) = 0;
  
  /** Deinitialize storage
   * @return HalResult::OK on success
   */
  virtual HalResult deinit() = 0;
  
  /** Check if storage is initialized
   * @return true if initialized
   */
  virtual bool isInitialized() const = 0;
  
  /** Check if storage is mounted
   * @return true if mounted
   */
  virtual bool isMounted() const = 0;
  
  /** Mount storage
   * @return HalResult::OK on success
   */
  virtual HalResult mount() = 0;
  
  /** Unmount storage
   * @return HalResult::OK on success
   */
  virtual HalResult unmount() = 0;
  
  /** Get total storage size
   * @return Size in bytes
   */
  virtual uint64_t getTotalSize() const = 0;
  
  /** Get free storage space
   * @return Free space in bytes
   */
  virtual uint64_t getFreeSpace() const = 0;
  
  /** Check if file exists
   * @param path File path
   * @return true if file exists
   */
  virtual bool fileExists(const char* path) = 0;
  
  /** Check if directory exists
   * @param path Directory path
   * @return true if directory exists
   */
  virtual bool dirExists(const char* path) = 0;
  
  /** Create directory
   * @param path Directory path
   * @return HalResult::OK on success
   */
  virtual HalResult createDir(const char* path) = 0;
  
  /** Delete file
   * @param path File path
   * @return HalResult::OK on success
   */
  virtual HalResult deleteFile(const char* path) = 0;
  
  /** Delete directory
   * @param path Directory path
   * @return HalResult::OK on success
   */
  virtual HalResult deleteDir(const char* path) = 0;
  
  /** Rename/move file
   * @param old_path Current path
   * @param new_path New path
   * @return HalResult::OK on success
   */
  virtual HalResult rename(const char* old_path, const char* new_path) = 0;
  
  /** Get file size
   * @param path File path
   * @return File size in bytes, or 0 if not found
   */
  virtual uint64_t getFileSize(const char* path) = 0;
  
  /** Format storage (erase all data)
   * @return HalResult::OK on success
   */
  virtual HalResult format() = 0;
  
  /** Get card name/label
   * @return Card name string or "N/A"
   */
  virtual const char* getCardName() const = 0;
  
  /** Get mount point path
   * @return Mount point string
   */
  virtual const char* getMountPoint() const = 0;
};

// ============================================================
// File Handle Interface
// ============================================================

/** File handle for read/write operations */
class IHalFile{
public:
  virtual ~IHalFile() = default;
  
  /** Open file
   * @param path File path
   * @param mode File open mode
   * @return HalResult::OK on success
   */
  virtual HalResult open(const char* path, FileMode mode) = 0;
  
  /** Close file
   * @return HalResult::OK on success
   */
  virtual HalResult close() = 0;
  
  /** Check if file is open
   * @return true if open
   */
  virtual bool isOpen() const = 0;
  
  /** Read data from file
   * @param buffer Buffer to store data
   * @param length Number of bytes to read
   * @param bytes_read Pointer to store actual bytes read
   * @return HalResult::OK on success
   */
  virtual HalResult read(uint8_t* buffer, size_t length, size_t* bytes_read) = 0;
  
  /** Write data to file
   * @param data Data to write
   * @param length Number of bytes to write
   * @param bytes_written Pointer to store actual bytes written
   * @return HalResult::OK on success
   */
  virtual HalResult write(const uint8_t* data, size_t length, size_t* bytes_written) = 0;
  
  /** Seek to position
   * @param offset Offset in bytes
   * @param origin Seek origin (BEGIN, CURRENT, END)
   * @return HalResult::OK on success
   */
  virtual HalResult seek(int64_t offset, SeekOrigin origin) = 0;
  
  /** Get current position
   * @return Current position in bytes
   */
  virtual int64_t tell() const = 0;
  
  /** Get file size
   * @return File size in bytes
   */
  virtual int64_t size() const = 0;
  
  /** Flush write buffer
   * @return HalResult::OK on success
   */
  virtual HalResult flush() = 0;
  
  /** Check if at end of file
   * @return true if EOF
   */
  virtual bool eof() const = 0;
};

} // namespace arcos::hal

#endif // ARCOS_INCLUDE_HAL_IHAL_STORAGE_HPP_
