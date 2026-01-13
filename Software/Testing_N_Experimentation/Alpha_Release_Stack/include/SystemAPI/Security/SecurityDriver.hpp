/*****************************************************************
 * @file SecurityDriver.hpp
 * @brief Security Driver with TRNG and Credential Management
 * 
 * Provides hardware-based true random number generation (TRNG)
 * and secure credential management for WiFi AP passwords.
 * 
 * Features:
 * - Hardware TRNG using ESP32's RNG peripheral
 * - Password generation (12-char lowercase alphanumeric)
 * - Persistent storage via HAL DataStore
 * - SSID generation: "{Name} ({Model}) - {4 digits}"
 * - Custom SSID/Password support with reset to auto
 * 
 * @author XCR1793 (Feather Forge)
 * @version 1.1
 *****************************************************************/

#ifndef ARCOS_INCLUDE_SYSTEMAPI_SECURITY_SECURITYDRIVER_HPP_
#define ARCOS_INCLUDE_SYSTEMAPI_SECURITY_SECURITYDRIVER_HPP_

#include "HAL/IHalDataStore.hpp"
#include "HAL/IHalLog.hpp"
#include <cstring>
#include <cstdio>

// ESP32 hardware RNG
#include <esp_random.h>

namespace arcos::security{

// ============================================================
// Constants
// ============================================================

/** Password length (12 characters) */
constexpr size_t PASSWORD_LENGTH = 12;

/** Maximum SSID length (WiFi spec max is 32, but we use 48 for safety) */
constexpr size_t MAX_SSID_LENGTH = 48;

/** Maximum name/model length */
constexpr size_t MAX_NAME_LENGTH = 16;
constexpr size_t MAX_MODEL_LENGTH = 8;

/** NVS keys for credential storage */
namespace nvs_keys{
  constexpr const char* WIFI_PASSWORD = "wifi_pwd";
  constexpr const char* WIFI_SSID = "wifi_ssid";
  constexpr const char* DEVICE_ID = "device_id";
  constexpr const char* INITIALIZED = "sec_init";
  constexpr const char* CUSTOM_SSID = "cust_ssid";
  constexpr const char* CUSTOM_PWD = "cust_pwd";
  constexpr const char* USE_CUSTOM = "use_custom";
  constexpr const char* DEVICE_NAME = "dev_name";
  constexpr const char* DEVICE_MODEL = "dev_model";
  constexpr const char* DEVICE_SUFFIX = "dev_suffix";
  // External WiFi settings
  constexpr const char* EXT_WIFI_ENABLED = "ext_enabled";
  constexpr const char* EXT_WIFI_CONNECT = "ext_connect";  // Connect now toggle state
  constexpr const char* EXT_WIFI_SSID = "ext_ssid";
  constexpr const char* EXT_WIFI_PWD = "ext_pwd";
  constexpr const char* EXT_AUTH_ENABLED = "ext_auth";
  constexpr const char* EXT_AUTH_USER = "ext_user";
  constexpr const char* EXT_AUTH_PWD = "ext_authpwd";
}

// ============================================================
// TRNG - True Random Number Generator
// ============================================================

/**
 * @brief Hardware True Random Number Generator
 * 
 * Uses ESP32's hardware RNG for cryptographically secure
 * random number generation.
 */
class TRNG{
public:
  /**
   * @brief Get random bytes from hardware RNG
   * @param buf Buffer to fill with random bytes
   * @param len Number of bytes to generate
   * @return true on success
   */
  static bool getRandomBytes(uint8_t* buf, size_t len){
    if(!buf || len == 0) return false;
    
    // ESP32 esp_fill_random uses hardware RNG
    esp_fill_random(buf, len);
    return true;
  }
  
  /**
   * @brief Get a random 32-bit unsigned integer
   * @return Random uint32_t value
   */
  static uint32_t getRandomU32(){
    return esp_random();
  }
  
  /**
   * @brief Get a random value in range [0, max)
   * @param max Upper bound (exclusive)
   * @return Random value in range
   */
  static uint32_t getRandomRange(uint32_t max){
    if(max == 0) return 0;
    return esp_random() % max;
  }
  
  /**
   * @brief Generate alphanumeric string (lowercase letters + digits)
   * @param buf Buffer to store result (must include space for null terminator)
   * @param len Length of string to generate (not including null terminator)
   * @return true on success
   * 
   * Characters: a-z (26) + 0-9 (10) = 36 possible characters
   */
  static bool generateAlphanumeric(char* buf, size_t len){
    if(!buf || len == 0) return false;
    
    static const char charset[] = "abcdefghijklmnopqrstuvwxyz0123456789";
    static const size_t charset_size = sizeof(charset) - 1;  // Exclude null terminator
    
    for(size_t i = 0; i < len; i++){
      uint32_t idx = getRandomRange(charset_size);
      buf[i] = charset[idx];
    }
    buf[len] = '\0';
    
    return true;
  }
  
  /**
   * @brief Generate hex string from random bytes
   * @param buf Buffer to store hex string (must be 2*len + 1 bytes)
   * @param len Number of random bytes (hex string will be 2*len chars)
   * @return true on success
   */
  static bool generateHex(char* buf, size_t len){
    if(!buf || len == 0) return false;
    
    uint8_t temp[32];
    if(len > sizeof(temp)) len = sizeof(temp);
    
    getRandomBytes(temp, len);
    
    for(size_t i = 0; i < len; i++){
      sprintf(buf + (i * 2), "%02x", temp[i]);
    }
    buf[len * 2] = '\0';
    
    return true;
  }
};

// ============================================================
// Credential Manager
// ============================================================

/**
 * @brief WiFi Credential Manager
 * 
 * Manages WiFi AP credentials with persistent storage.
 * Generates and stores passwords securely.
 * Supports custom SSID/password with reset to auto-generated.
 */
class CredentialManager{
private:
  static constexpr const char* TAG = "SECURITY";
  
  arcos::hal::IHalDataStore* datastore_ = nullptr;
  arcos::hal::IHalLog* log_ = nullptr;
  
  char ssid_[MAX_SSID_LENGTH + 1] = {0};
  char password_[PASSWORD_LENGTH + 1] = {0};
  char device_name_[MAX_NAME_LENGTH + 1] = {0};
  char device_model_[MAX_MODEL_LENGTH + 1] = {0};
  uint16_t device_suffix_ = 0;
  uint32_t device_id_ = 0;
  bool use_custom_ = false;
  bool initialized_ = false;

public:
  /**
   * @brief Constructor
   * @param datastore HAL DataStore for persistent storage
   * @param log Optional logger
   */
  CredentialManager(arcos::hal::IHalDataStore* datastore, arcos::hal::IHalLog* log = nullptr)
    : datastore_(datastore), log_(log){}
  
  /**
   * @brief Initialize credential manager
   * 
   * Loads existing credentials from NVS or generates new ones
   * if not found.
   * 
   * @param name Device name (e.g., "Lucidius")
   * @param model Device model (e.g., "DX.3")
   * @return true on success
   */
  bool init(const char* name = "Lucidius", const char* model = "DX.3"){
    if(!datastore_ || !datastore_->isInitialized()){
      if(log_) log_->error(TAG, "DataStore not initialized");
      return false;
    }
    
    if(initialized_){
      if(log_) log_->warn(TAG, "CredentialManager already initialized");
      return true;
    }
    
    // Store name and model
    strncpy(device_name_, name, MAX_NAME_LENGTH);
    strncpy(device_model_, model, MAX_MODEL_LENGTH);
    
    // Check if credentials already exist
    uint8_t sec_initialized = 0;
    arcos::hal::HalResult result = datastore_->getU8(nvs_keys::INITIALIZED, &sec_initialized);
    
    if(result == arcos::hal::HalResult::OK && sec_initialized == 1){
      // Load existing credentials
      if(loadCredentials()){
        if(log_) log_->info(TAG, "Loaded existing credentials");
        initialized_ = true;
        return true;
      }
      // Fall through to generate new if load failed
      if(log_) log_->warn(TAG, "Failed to load credentials, generating new");
    }
    
    // Generate new credentials
    if(!generateCredentials()){
      if(log_) log_->error(TAG, "Failed to generate credentials");
      return false;
    }
    
    // Save to NVS
    if(!saveCredentials()){
      if(log_) log_->error(TAG, "Failed to save credentials");
      return false;
    }
    
    initialized_ = true;
    if(log_) log_->info(TAG, "Generated and saved new credentials");
    return true;
  }
  
  /**
   * @brief Get WiFi SSID (returns custom or auto-generated based on mode)
   * @return SSID string (e.g., "Lucidius (DX.3) - 2043")
   */
  const char* getSSID() const{
    return ssid_;
  }
  
  /**
   * @brief Get WiFi password
   * @return 12-character password string
   */
  const char* getPassword() const{
    return password_;
  }
  
  /**
   * @brief Get device ID
   * @return Unique 32-bit device identifier
   */
  uint32_t getDeviceID() const{
    return device_id_;
  }
  
  /**
   * @brief Get device name
   */
  const char* getDeviceName() const{
    return device_name_;
  }
  
  /**
   * @brief Get device model
   */
  const char* getDeviceModel() const{
    return device_model_;
  }
  
  /**
   * @brief Get device suffix (4 digit number)
   */
  uint16_t getDeviceSuffix() const{
    return device_suffix_;
  }
  
  /**
   * @brief Check if using custom credentials
   */
  bool isUsingCustom() const{
    return use_custom_;
  }
  
  /**
   * @brief Check if credentials are initialized
   * @return true if valid credentials available
   */
  bool isInitialized() const{
    return initialized_;
  }
  
  /**
   * @brief Set custom SSID and password
   * @param ssid Custom SSID (max 32 chars)
   * @param password Custom password (min 8 chars for WPA2)
   * @return true on success
   */
  bool setCustomCredentials(const char* ssid, const char* password){
    if(!datastore_ || !datastore_->isInitialized()){
      return false;
    }
    
    if(!ssid || strlen(ssid) == 0 || strlen(ssid) > MAX_SSID_LENGTH){
      if(log_) log_->error(TAG, "Invalid SSID");
      return false;
    }
    
    if(!password || strlen(password) < 8 || strlen(password) > PASSWORD_LENGTH){
      if(log_) log_->error(TAG, "Invalid password (must be 8-12 chars)");
      return false;
    }
    
    // Save custom credentials
    datastore_->setString(nvs_keys::CUSTOM_SSID, ssid);
    datastore_->setString(nvs_keys::CUSTOM_PWD, password);
    datastore_->setU8(nvs_keys::USE_CUSTOM, 1);
    datastore_->commit();
    
    // Update active credentials
    strncpy(ssid_, ssid, MAX_SSID_LENGTH);
    strncpy(password_, password, PASSWORD_LENGTH);
    use_custom_ = true;
    
    if(log_) log_->info(TAG, "Custom credentials set: SSID=%s", ssid_);
    return true;
  }
  
  /**
   * @brief Reset to auto-generated credentials
   * @return true on success
   */
  bool resetToAuto(){
    if(!datastore_ || !datastore_->isInitialized()){
      return false;
    }
    
    // Clear custom flag
    datastore_->setU8(nvs_keys::USE_CUSTOM, 0);
    datastore_->commit();
    use_custom_ = false;
    
    // Restore auto-generated credentials
    char auto_ssid[MAX_SSID_LENGTH + 1];
    char auto_pwd[PASSWORD_LENGTH + 1];
    
    if(datastore_->getString(nvs_keys::WIFI_SSID, auto_ssid, sizeof(auto_ssid)) == arcos::hal::HalResult::OK &&
       datastore_->getString(nvs_keys::WIFI_PASSWORD, auto_pwd, sizeof(auto_pwd)) == arcos::hal::HalResult::OK){
      strncpy(ssid_, auto_ssid, MAX_SSID_LENGTH);
      strncpy(password_, auto_pwd, PASSWORD_LENGTH);
    }
    
    if(log_) log_->info(TAG, "Reset to auto credentials: SSID=%s", ssid_);
    return true;
  }
  
  /**
   * @brief Force regeneration of auto credentials
   * @return true on success
   */
  bool regenerate(){
    if(!datastore_ || !datastore_->isInitialized()){
      return false;
    }
    
    // Clear initialization flag
    datastore_->setU8(nvs_keys::INITIALIZED, 0);
    datastore_->setU8(nvs_keys::USE_CUSTOM, 0);
    use_custom_ = false;
    initialized_ = false;
    
    // Generate new credentials
    if(!generateCredentials()){
      return false;
    }
    
    // Save new credentials
    if(!saveCredentials()){
      return false;
    }
    
    initialized_ = true;
    if(log_) log_->info(TAG, "Credentials regenerated: SSID=%s", ssid_);
    return true;
  }
  
  /**
   * @brief Generate SSID in format: "{Name} ({Model}) - {4 digits}"
   * @param buf Buffer to store SSID
   * @param name Device name
   * @param model Device model
   * @param suffix 4-digit suffix (0-9999)
   */
  static void generateSSID(char* buf, const char* name, const char* model, uint16_t suffix){
    snprintf(buf, MAX_SSID_LENGTH, "%s (%s) - %04d", name, model, suffix % 10000);
  }
  
  /**
   * @brief Generate password into buffer
   * @param buf Buffer to store password (must be >= PASSWORD_LENGTH + 1)
   */
  static void generatePassword(char* buf){
    TRNG::generateAlphanumeric(buf, PASSWORD_LENGTH);
  }

private:
  /**
   * @brief Generate new credentials
   */
  bool generateCredentials(){
    // Generate device ID
    device_id_ = TRNG::getRandomU32();
    
    // Generate 4-digit suffix (0-9999)
    device_suffix_ = TRNG::getRandomRange(10000);
    
    // Generate SSID: "{Name} ({Model}) - {4 digits}"
    generateSSID(ssid_, device_name_, device_model_, device_suffix_);
    
    // Generate 12-character password
    generatePassword(password_);
    
    use_custom_ = false;
    
    if(log_){
      log_->info(TAG, "Generated SSID: %s", ssid_);
      log_->info(TAG, "Generated Password: %s", password_);
      log_->info(TAG, "Device ID: 0x%08X", device_id_);
    }
    
    return true;
  }
  
  /**
   * @brief Load credentials from NVS
   */
  bool loadCredentials(){
    using arcos::hal::HalResult;
    
    // Load device info
    datastore_->getString(nvs_keys::DEVICE_NAME, device_name_, sizeof(device_name_));
    datastore_->getString(nvs_keys::DEVICE_MODEL, device_model_, sizeof(device_model_));
    datastore_->getU16(nvs_keys::DEVICE_SUFFIX, &device_suffix_);
    datastore_->getU32(nvs_keys::DEVICE_ID, &device_id_);
    
    // Check if using custom credentials
    uint8_t use_custom = 0;
    datastore_->getU8(nvs_keys::USE_CUSTOM, &use_custom);
    use_custom_ = (use_custom == 1);
    
    if(use_custom_){
      // Load custom credentials
      HalResult result = datastore_->getString(nvs_keys::CUSTOM_SSID, ssid_, sizeof(ssid_));
      if(result != HalResult::OK){
        use_custom_ = false;
      }else{
        result = datastore_->getString(nvs_keys::CUSTOM_PWD, password_, sizeof(password_));
        if(result != HalResult::OK){
          use_custom_ = false;
        }
      }
    }
    
    if(!use_custom_){
      // Load auto-generated credentials
      HalResult result = datastore_->getString(nvs_keys::WIFI_SSID, ssid_, sizeof(ssid_));
      if(result != HalResult::OK){
        if(log_) log_->error(TAG, "Failed to load SSID");
        return false;
      }
      
      result = datastore_->getString(nvs_keys::WIFI_PASSWORD, password_, sizeof(password_));
      if(result != HalResult::OK){
        if(log_) log_->error(TAG, "Failed to load password");
        return false;
      }
    }
    
    if(log_){
      log_->info(TAG, "Loaded SSID: %s%s", ssid_, use_custom_ ? " (custom)" : "");
      log_->info(TAG, "Loaded Password: %s", password_);
      log_->info(TAG, "Device ID: 0x%08X", device_id_);
    }
    
    return true;
  }
  
  /**
   * @brief Save credentials to NVS
   */
  bool saveCredentials(){
    using arcos::hal::HalResult;
    
    // Save device info
    datastore_->setString(nvs_keys::DEVICE_NAME, device_name_);
    datastore_->setString(nvs_keys::DEVICE_MODEL, device_model_);
    datastore_->setU16(nvs_keys::DEVICE_SUFFIX, device_suffix_);
    
    // Save auto-generated credentials
    HalResult result = datastore_->setString(nvs_keys::WIFI_PASSWORD, password_);
    if(result != HalResult::OK){
      if(log_) log_->error(TAG, "Failed to save password");
      return false;
    }
    
    result = datastore_->setString(nvs_keys::WIFI_SSID, ssid_);
    if(result != HalResult::OK){
      if(log_) log_->error(TAG, "Failed to save SSID");
      return false;
    }
    
    // Save device ID
    result = datastore_->setU32(nvs_keys::DEVICE_ID, device_id_);
    if(result != HalResult::OK){
      if(log_) log_->error(TAG, "Failed to save device ID");
      return false;
    }
    
    // Clear custom flag for new credentials
    datastore_->setU8(nvs_keys::USE_CUSTOM, 0);
    
    // Mark as initialized
    result = datastore_->setU8(nvs_keys::INITIALIZED, 1);
    if(result != HalResult::OK){
      if(log_) log_->error(TAG, "Failed to save initialized flag");
      return false;
    }
    
    // Commit all changes
    datastore_->commit();
    
    return true;
  }
};

// ============================================================
// Security Driver (Singleton wrapper)
// ============================================================

/**
 * @brief Security Driver Singleton
 * 
 * Provides global access to security services.
 * Must be initialized with a DataStore before use.
 */
class SecurityDriver{
private:
  arcos::hal::IHalDataStore* datastore_ = nullptr;
  arcos::hal::IHalLog* log_ = nullptr;
  CredentialManager* credentials_ = nullptr;
  bool initialized_ = false;
  
  SecurityDriver() = default;

public:
  /**
   * @brief Get singleton instance
   */
  static SecurityDriver& instance(){
    static SecurityDriver inst;
    return inst;
  }
  
  // Prevent copying
  SecurityDriver(const SecurityDriver&) = delete;
  SecurityDriver& operator=(const SecurityDriver&) = delete;
  
  /**
   * @brief Initialize security driver
   * @param datastore HAL DataStore for persistent storage
   * @param log Optional logger
   * @param name Device name (e.g., "Lucidius")
   * @param model Device model (e.g., "DX.3")
   * @return true on success
   */
  bool init(arcos::hal::IHalDataStore* datastore, 
            arcos::hal::IHalLog* log = nullptr,
            const char* name = "Lucidius",
            const char* model = "DX.3"){
    if(initialized_){
      if(log) log->warn("SECURITY", "SecurityDriver already initialized");
      return true;
    }
    
    datastore_ = datastore;
    log_ = log;
    
    // Create credential manager
    credentials_ = new CredentialManager(datastore_, log_);
    if(!credentials_->init(name, model)){
      delete credentials_;
      credentials_ = nullptr;
      return false;
    }
    
    initialized_ = true;
    return true;
  }
  
  /**
   * @brief Check if driver is initialized
   */
  bool isInitialized() const{
    return initialized_;
  }
  
  /**
   * @brief Get credential manager
   */
  CredentialManager* credentials(){
    return credentials_;
  }
  
  /**
   * @brief Get WiFi SSID
   */
  const char* getSSID() const{
    return credentials_ ? credentials_->getSSID() : "Lucidius (DX.3) - 0000";
  }
  
  /**
   * @brief Get WiFi password
   */
  const char* getPassword() const{
    return credentials_ ? credentials_->getPassword() : "";
  }
  
  /**
   * @brief Check if using custom credentials
   */
  bool isUsingCustom() const{
    return credentials_ ? credentials_->isUsingCustom() : false;
  }
  
  /**
   * @brief Set custom credentials
   */
  bool setCustomCredentials(const char* ssid, const char* password){
    return credentials_ ? credentials_->setCustomCredentials(ssid, password) : false;
  }
  
  /**
   * @brief Reset to auto-generated credentials
   */
  bool resetToAuto(){
    return credentials_ ? credentials_->resetToAuto() : false;
  }
  
  /**
   * @brief Regenerate credentials
   */
  bool regenerateCredentials(){
    return credentials_ ? credentials_->regenerate() : false;
  }
  
  /**
   * @brief Get DataStore reference for direct access
   */
  arcos::hal::IHalDataStore* getDataStore(){
    return datastore_;
  }
  
  /**
   * @brief Save external WiFi settings to NVS
   */
  bool saveExtWifiSettings(bool enabled, bool connectNow, const char* ssid, const char* password,
                           bool authEnabled, const char* authUser, const char* authPwd){
    if(!datastore_ || !datastore_->isInitialized()) return false;
    
    datastore_->setU8(nvs_keys::EXT_WIFI_ENABLED, enabled ? 1 : 0);
    datastore_->setU8(nvs_keys::EXT_WIFI_CONNECT, connectNow ? 1 : 0);
    datastore_->setString(nvs_keys::EXT_WIFI_SSID, ssid ? ssid : "");
    datastore_->setString(nvs_keys::EXT_WIFI_PWD, password ? password : "");
    datastore_->setU8(nvs_keys::EXT_AUTH_ENABLED, authEnabled ? 1 : 0);
    datastore_->setString(nvs_keys::EXT_AUTH_USER, authUser ? authUser : "admin");
    datastore_->setString(nvs_keys::EXT_AUTH_PWD, authPwd ? authPwd : "");
    datastore_->commit();
    
    if(log_) log_->info("SECURITY", "External WiFi settings saved (connect=%d)", connectNow);
    return true;
  }
  
  /**
   * @brief Load external WiFi settings from NVS
   */
  bool loadExtWifiSettings(bool& enabled, bool& connectNow, char* ssid, size_t ssidLen, 
                           char* password, size_t pwdLen,
                           bool& authEnabled, char* authUser, size_t userLen,
                           char* authPwd, size_t authPwdLen){
    if(!datastore_ || !datastore_->isInitialized()) return false;
    
    uint8_t en = 0;
    if(datastore_->getU8(nvs_keys::EXT_WIFI_ENABLED, &en) == arcos::hal::HalResult::OK){
      enabled = (en == 1);
    } else {
      enabled = false;
    }
    
    uint8_t conn = 0;
    if(datastore_->getU8(nvs_keys::EXT_WIFI_CONNECT, &conn) == arcos::hal::HalResult::OK){
      connectNow = (conn == 1);
    } else {
      connectNow = false;
    }
    
    if(ssid && ssidLen > 0){
      if(datastore_->getString(nvs_keys::EXT_WIFI_SSID, ssid, ssidLen) != arcos::hal::HalResult::OK){
        ssid[0] = '\0';
      }
    }
    
    if(password && pwdLen > 0){
      if(datastore_->getString(nvs_keys::EXT_WIFI_PWD, password, pwdLen) != arcos::hal::HalResult::OK){
        password[0] = '\0';
      }
    }
    
    uint8_t authEn = 0;
    if(datastore_->getU8(nvs_keys::EXT_AUTH_ENABLED, &authEn) == arcos::hal::HalResult::OK){
      authEnabled = (authEn == 1);
    } else {
      authEnabled = false;
    }
    
    if(authUser && userLen > 0){
      if(datastore_->getString(nvs_keys::EXT_AUTH_USER, authUser, userLen) != arcos::hal::HalResult::OK){
        strncpy(authUser, "admin", userLen);
      }
    }
    
    if(authPwd && authPwdLen > 0){
      if(datastore_->getString(nvs_keys::EXT_AUTH_PWD, authPwd, authPwdLen) != arcos::hal::HalResult::OK){
        authPwd[0] = '\0';
      }
    }
    
    if(log_) log_->info("SECURITY", "External WiFi settings loaded: enabled=%d", enabled);
    return true;
  }
  
  /**
   * @brief Reset external WiFi settings to defaults
   */
  bool resetExtWifiSettings(){
    if(!datastore_ || !datastore_->isInitialized()) return false;
    
    datastore_->setU8(nvs_keys::EXT_WIFI_ENABLED, 0);
    datastore_->setU8(nvs_keys::EXT_WIFI_CONNECT, 0);
    datastore_->setString(nvs_keys::EXT_WIFI_SSID, "");
    datastore_->setString(nvs_keys::EXT_WIFI_PWD, "");
    datastore_->setU8(nvs_keys::EXT_AUTH_ENABLED, 0);
    datastore_->setString(nvs_keys::EXT_AUTH_USER, "admin");
    datastore_->setString(nvs_keys::EXT_AUTH_PWD, "");
    datastore_->commit();
    
    if(log_) log_->info("SECURITY", "External WiFi settings reset to defaults");
    return true;
  }
};

} // namespace arcos::security

#endif // ARCOS_INCLUDE_SYSTEMAPI_SECURITY_SECURITYDRIVER_HPP_
