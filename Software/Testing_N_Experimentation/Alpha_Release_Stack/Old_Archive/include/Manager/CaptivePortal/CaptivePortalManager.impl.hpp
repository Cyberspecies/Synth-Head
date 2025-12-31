/*****************************************************************
 * File:      CaptivePortalManager.impl.hpp
 * Category:  Manager/WiFi
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Core implementation of captive portal manager.
 *    Handles initialization, credentials, DNS setup, and data management.
 *****************************************************************/

#ifndef CAPTIVE_PORTAL_MANAGER_IMPL_HPP
#define CAPTIVE_PORTAL_MANAGER_IMPL_HPP

#include <esp_random.h>

namespace arcos::manager {

constexpr const char* DEVICE_BASE_NAME = "SynthHead";
constexpr uint16_t DNS_PORT = 53;
constexpr uint32_t AP_CHANNEL = 1;
constexpr uint32_t MAX_CONNECTIONS = 4;

// ============================================================================
// Constructor / Destructor
// ============================================================================

inline CaptivePortalManager::CaptivePortalManager()
  : server_(nullptr)
  , dns_server_(nullptr)
  , device_base_name_(DEVICE_BASE_NAME)
  , use_custom_credentials_(false)
  , sensor_data_mutex_(nullptr)
{
  sensor_data_mutex_ = xSemaphoreCreateMutex();
  memset(&sensor_data_, 0, sizeof(sensor_data_));
}

inline CaptivePortalManager::~CaptivePortalManager() {
  if (server_) {
    delete server_;
  }
  if (dns_server_) {
    delete dns_server_;
  }
  if (sensor_data_mutex_) {
    vSemaphoreDelete(sensor_data_mutex_);
  }
}

// ============================================================================
// Public API - Inline getters
// ============================================================================

inline String CaptivePortalManager::getSSID() const {
  return current_ssid_;
}

inline String CaptivePortalManager::getPassword() const {
  return current_password_;
}

inline bool CaptivePortalManager::isCustomCredentials() const {
  return use_custom_credentials_;
}

inline int CaptivePortalManager::getClientCount() const {
  return WiFi.softAPgetStationNum();
}

// ============================================================================
// Initialization
// ============================================================================

inline bool CaptivePortalManager::initialize() {
  Serial.println("");
  Serial.println("========================================");
  Serial.println("  CAPTIVE PORTAL INITIALIZATION");
  Serial.println("========================================");
  
  // Load stored credentials if available
  loadCredentials();
  
  // Generate random credentials if not using custom
  if (!use_custom_credentials_) {
    String suffix = generateRandomSuffix();
    current_ssid_ = device_base_name_ + "_" + suffix;
    current_password_ = generateRandomPassword();
    
    Serial.println("WIFI: Generated random credentials");
  }
  
  Serial.printf("WIFI: SSID: %s\n", current_ssid_.c_str());
  Serial.printf("WIFI: Password: %s\n", current_password_.c_str());
  Serial.printf("WIFI: Type: %s\n", use_custom_credentials_ ? "CUSTOM" : "RANDOM");
  
  // Configure Access Point with custom IP
  WiFi.mode(WIFI_AP);
  
  // Set custom IP configuration: 10.0.0.1
  IPAddress local_ip(10, 0, 0, 1);
  IPAddress gateway(10, 0, 0, 1);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.softAPConfig(local_ip, gateway, subnet);
  
  WiFi.softAP(current_ssid_.c_str(), current_password_.c_str(), AP_CHANNEL, 0, MAX_CONNECTIONS);
  
  IPAddress ip = WiFi.softAPIP();
  Serial.printf("WIFI: AP IP Address: %s\n", ip.toString().c_str());
  
  // Setup DNS server for captive portal
  setupDNSServer();
  
  // Setup web server
  setupWebServer();
  
  Serial.println("WIFI: Captive portal ready!");
  Serial.println("========================================");
  Serial.println("");
  
  return true;
}

// ============================================================================
// DNS Setup
// ============================================================================

inline void CaptivePortalManager::setupDNSServer() {
  dns_server_ = new DNSServer();
  
  // Redirect all DNS requests to our AP IP
  IPAddress ap_ip = WiFi.softAPIP();
  dns_server_->start(DNS_PORT, "*", ap_ip);
  
  Serial.println("WIFI: DNS server started (captive portal redirect)");
}

// ============================================================================
// Update Loop
// ============================================================================

inline void CaptivePortalManager::update() {
  if (dns_server_) {
    dns_server_->processNextRequest();
  }
}

// ============================================================================
// Credential Management
// ============================================================================

inline String CaptivePortalManager::generateRandomSuffix() {
  // Generate 4-digit random number (0000-9999)
  uint32_t random_num = esp_random() % 10000;
  char suffix[5];
  snprintf(suffix, sizeof(suffix), "%04lu", random_num);
  return String(suffix);
}

inline String CaptivePortalManager::generateRandomPassword() {
  // Generate 12-character password: A-Z and 0-9
  const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
  const int charset_size = sizeof(charset) - 1;
  
  char password[13];
  for (int i = 0; i < 12; i++) {
    uint32_t random_index = esp_random() % charset_size;
    password[i] = charset[random_index];
  }
  password[12] = '\0';
  
  return String(password);
}

inline void CaptivePortalManager::loadCredentials() {
  preferences_.begin("wifi_config", false);
  
  use_custom_credentials_ = preferences_.getBool("use_custom", false);
  
  if (use_custom_credentials_) {
    current_ssid_ = preferences_.getString("ssid", "");
    current_password_ = preferences_.getString("password", "");
    
    // Validate loaded credentials
    if (current_ssid_.length() == 0 || current_password_.length() < 8) {
      Serial.println("WIFI: Invalid stored credentials, generating random");
      use_custom_credentials_ = false;
    } else {
      Serial.println("WIFI: Loaded custom credentials from flash");
      Serial.printf("WIFI: SSID: %s\n", current_ssid_.c_str());
    }
  }
  
  preferences_.end();
}

inline void CaptivePortalManager::saveCredentials() {
  preferences_.begin("wifi_config", false);
  
  preferences_.putBool("use_custom", use_custom_credentials_);
  preferences_.putString("ssid", current_ssid_);
  preferences_.putString("password", current_password_);
  
  preferences_.end();
  
  Serial.println("WIFI: Credentials saved to flash");
}

// ============================================================================
// Sensor Data Management
// ============================================================================

inline String CaptivePortalManager::getSensorDataJSON() {
  static uint32_t json_count = 0;
  String json = "{";
  
  // Add device uptime (always fresh, not from sensor data)
  json += "\"uptime\":" + String(millis()) + ",";
  
  if (xSemaphoreTake(sensor_data_mutex_, pdMS_TO_TICKS(10)) == pdTRUE) {
    json_count++;
    
    // Debug every 8 API calls (web polls at 4Hz, so this is every 2 seconds)
    if(json_count % 8 == 0) {
      Serial.printf("DEBUG [PORTAL-JSON]: Request #%u - Reading from portal storage: Temp=%.1f°C, Accel=(%.2f,%.2f,%.2f)\n",
        json_count, sensor_data_.temperature,
        sensor_data_.accel_x, sensor_data_.accel_y, sensor_data_.accel_z);
    }
    
    json += "\"accel_x\":" + String(sensor_data_.accel_x, 2) + ",";
    json += "\"accel_y\":" + String(sensor_data_.accel_y, 2) + ",";
    json += "\"accel_z\":" + String(sensor_data_.accel_z, 2) + ",";
    json += "\"gyro_x\":" + String(sensor_data_.gyro_x, 1) + ",";
    json += "\"gyro_y\":" + String(sensor_data_.gyro_y, 1) + ",";
    json += "\"gyro_z\":" + String(sensor_data_.gyro_z, 1) + ",";
    json += "\"temperature\":" + String(sensor_data_.temperature, 1) + ",";
    json += "\"humidity\":" + String(sensor_data_.humidity, 1) + ",";
    json += "\"pressure\":" + String(sensor_data_.pressure, 0) + ",";
    json += "\"altitude\":" + String(sensor_data_.altitude, 1) + ",";
    json += "\"gps_lat\":" + String(sensor_data_.latitude, 6) + ",";
    json += "\"gps_lon\":" + String(sensor_data_.longitude, 6) + ",";
    json += "\"gps_speed\":" + String(sensor_data_.speed_knots * 1.852f, 1) + ",";  // Convert knots to km/h
    json += "\"gps_satellites\":" + String(sensor_data_.gps_satellites) + ",";
    json += "\"gps_hour\":" + String(sensor_data_.gps_hour) + ",";
    json += "\"gps_minute\":" + String(sensor_data_.gps_minute) + ",";
    json += "\"gps_second\":" + String(sensor_data_.gps_second) + ",";
    json += "\"button_a\":" + String(sensor_data_.getButtonA() ? "true" : "false") + ",";
    json += "\"button_b\":" + String(sensor_data_.getButtonB() ? "true" : "false") + ",";
    json += "\"button_c\":" + String(sensor_data_.getButtonC() ? "true" : "false") + ",";
    json += "\"button_d\":" + String(sensor_data_.getButtonD() ? "true" : "false");
    
    xSemaphoreGive(sensor_data_mutex_);
  }
  
  json += "}";
  return json;
}

inline void CaptivePortalManager::updateSensorData(const arcos::communication::SensorDataPayload& data) {
  static uint32_t update_count = 0;
  static uint32_t last_debug = 0;
  
  // Debug: Track incoming data BEFORE mutex
  update_count++;
  uint32_t now = millis();
  if(now - last_debug >= 2000) {
    Serial.printf("DEBUG [PORTAL-UPDATE]: Received #%u from Core1-Web - Incoming: Temp=%.1f°C, Accel=(%.2f,%.2f,%.2f)\n",
      update_count, data.temperature, data.accel_x, data.accel_y, data.accel_z);
  }
  
  if (xSemaphoreTake(sensor_data_mutex_, pdMS_TO_TICKS(10)) == pdTRUE) {
    // Update all sensor data (this includes merged physical+web button states)
    memcpy(&sensor_data_, &data, sizeof(sensor_data_));
    
    // Debug every 2 seconds to verify data is flowing through portal's internal storage
    if(now - last_debug >= 2000) {
      Serial.printf("DEBUG [PORTAL-UPDATE]: Mutex acquired - Stored in portal: Temp=%.1f°C, Accel=(%.2f,%.2f,%.2f)\n",
        sensor_data_.temperature, sensor_data_.accel_x, sensor_data_.accel_y, sensor_data_.accel_z);
      last_debug = now;
    }
    
    xSemaphoreGive(sensor_data_mutex_);
  } else {
    Serial.println("DEBUG [PORTAL-UPDATE]: ERROR - MUTEX TIMEOUT in updateSensorData!");
  }
}

inline void CaptivePortalManager::getSensorData(arcos::communication::SensorDataPayload& data) const {
  if (xSemaphoreTake(sensor_data_mutex_, pdMS_TO_TICKS(10)) == pdTRUE) {
    memcpy(&data, &sensor_data_, sizeof(data));
    xSemaphoreGive(sensor_data_mutex_);
  }
}

} // namespace arcos::manager

// Include web page generators and route handlers
#include "WebPages_Setup.hpp"
#include "WebPages_Dashboard.hpp"
#include "WebServer_Routes.hpp"

#endif // CAPTIVE_PORTAL_MANAGER_IMPL_HPP
