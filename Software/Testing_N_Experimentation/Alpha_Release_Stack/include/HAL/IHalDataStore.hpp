/*****************************************************************
 * File:      IHalDataStore.hpp
 * Category:  include/HAL
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Data Store Hardware Abstraction Layer interface.
 *    Provides platform-independent access to persistent key-value
 *    storage (NVS, EEPROM, Flash).
 * 
 * Note:
 *    This is a storage HAL interface for simple key-value pairs.
 *    For file system operations, use IHalStorage instead.
 *****************************************************************/

#ifndef ARCOS_INCLUDE_HAL_IHAL_DATASTORE_HPP_
#define ARCOS_INCLUDE_HAL_IHAL_DATASTORE_HPP_

#include "HalTypes.hpp"
#include <stdint.h>
#include <stddef.h>

namespace arcos::hal{

// ============================================================
// Data Store Configuration
// ============================================================

/** Data store namespace configuration */
struct DataStoreConfig{
  const char* namespace_name = "arcos";  // NVS namespace
  bool read_only = false;                // Open in read-only mode
};

// ============================================================
// Data Store Interface
// ============================================================

/**
 * @brief Data Store Hardware Abstraction Interface
 * 
 * Provides platform-independent access to persistent key-value storage.
 * Typically backed by NVS (Non-Volatile Storage) on ESP32.
 */
class IHalDataStore{
public:
  virtual ~IHalDataStore() = default;
  
  /**
   * @brief Initialize data store
   * @param config Data store configuration
   * @return HalResult::OK on success
   */
  virtual HalResult init(const DataStoreConfig& config = DataStoreConfig{}) = 0;
  
  /**
   * @brief Deinitialize data store
   * @return HalResult::OK on success
   */
  virtual HalResult deinit() = 0;
  
  /**
   * @brief Check if data store is initialized
   * @return true if initialized
   */
  virtual bool isInitialized() const = 0;
  
  // --------------------------------------------------------
  // String Operations
  // --------------------------------------------------------
  
  /**
   * @brief Set string value
   * @param key Key name (max 15 chars)
   * @param value String value to store
   * @return HalResult::OK on success
   */
  virtual HalResult setString(const char* key, const char* value) = 0;
  
  /**
   * @brief Get string value
   * @param key Key name
   * @param value Buffer to store value
   * @param max_len Maximum buffer length
   * @return HalResult::OK on success, NO_DATA if not found
   */
  virtual HalResult getString(const char* key, char* value, size_t max_len) = 0;
  
  // --------------------------------------------------------
  // Integer Operations
  // --------------------------------------------------------
  
  /**
   * @brief Set 8-bit unsigned integer
   * @param key Key name
   * @param value Value to store
   * @return HalResult::OK on success
   */
  virtual HalResult setU8(const char* key, uint8_t value) = 0;
  
  /**
   * @brief Get 8-bit unsigned integer
   * @param key Key name
   * @param value Pointer to store value
   * @return HalResult::OK on success, NO_DATA if not found
   */
  virtual HalResult getU8(const char* key, uint8_t* value) = 0;
  
  /**
   * @brief Set 16-bit unsigned integer
   * @param key Key name
   * @param value Value to store
   * @return HalResult::OK on success
   */
  virtual HalResult setU16(const char* key, uint16_t value) = 0;
  
  /**
   * @brief Get 16-bit unsigned integer
   * @param key Key name
   * @param value Pointer to store value
   * @return HalResult::OK on success, NO_DATA if not found
   */
  virtual HalResult getU16(const char* key, uint16_t* value) = 0;
  
  /**
   * @brief Set 32-bit unsigned integer
   * @param key Key name
   * @param value Value to store
   * @return HalResult::OK on success
   */
  virtual HalResult setU32(const char* key, uint32_t value) = 0;
  
  /**
   * @brief Get 32-bit unsigned integer
   * @param key Key name
   * @param value Pointer to store value
   * @return HalResult::OK on success, NO_DATA if not found
   */
  virtual HalResult getU32(const char* key, uint32_t* value) = 0;
  
  /**
   * @brief Set 64-bit unsigned integer
   * @param key Key name
   * @param value Value to store
   * @return HalResult::OK on success
   */
  virtual HalResult setU64(const char* key, uint64_t value) = 0;
  
  /**
   * @brief Get 64-bit unsigned integer
   * @param key Key name
   * @param value Pointer to store value
   * @return HalResult::OK on success, NO_DATA if not found
   */
  virtual HalResult getU64(const char* key, uint64_t* value) = 0;
  
  // --------------------------------------------------------
  // Signed Integer Operations
  // --------------------------------------------------------
  
  /**
   * @brief Set 32-bit signed integer
   * @param key Key name
   * @param value Value to store
   * @return HalResult::OK on success
   */
  virtual HalResult setI32(const char* key, int32_t value) = 0;
  
  /**
   * @brief Get 32-bit signed integer
   * @param key Key name
   * @param value Pointer to store value
   * @return HalResult::OK on success, NO_DATA if not found
   */
  virtual HalResult getI32(const char* key, int32_t* value) = 0;
  
  // --------------------------------------------------------
  // Binary Blob Operations
  // --------------------------------------------------------
  
  /**
   * @brief Set binary blob
   * @param key Key name
   * @param data Binary data to store
   * @param length Data length in bytes
   * @return HalResult::OK on success
   */
  virtual HalResult setBlob(const char* key, const void* data, size_t length) = 0;
  
  /**
   * @brief Get binary blob
   * @param key Key name
   * @param data Buffer to store data
   * @param length In: buffer size, Out: actual data size
   * @return HalResult::OK on success, NO_DATA if not found
   */
  virtual HalResult getBlob(const char* key, void* data, size_t* length) = 0;
  
  // --------------------------------------------------------
  // Key Management
  // --------------------------------------------------------
  
  /**
   * @brief Check if key exists
   * @param key Key name
   * @return true if key exists
   */
  virtual bool keyExists(const char* key) = 0;
  
  /**
   * @brief Erase a single key
   * @param key Key name
   * @return HalResult::OK on success
   */
  virtual HalResult eraseKey(const char* key) = 0;
  
  /**
   * @brief Erase all keys in namespace
   * @return HalResult::OK on success
   */
  virtual HalResult eraseAll() = 0;
  
  /**
   * @brief Commit pending writes (if applicable)
   * @return HalResult::OK on success
   */
  virtual HalResult commit() = 0;
};

} // namespace arcos::hal

#endif // ARCOS_INCLUDE_HAL_IHAL_DATASTORE_HPP_
