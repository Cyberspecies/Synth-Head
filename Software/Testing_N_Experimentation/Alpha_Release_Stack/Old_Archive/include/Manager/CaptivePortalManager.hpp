/*****************************************************************
 * File:      CaptivePortalManager.hpp
 * Category:  Manager/WiFi
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Captive portal manager for WiFi AP mode with web interface.
 *    - Generates random SSID suffix and 12-char password on boot
 *    - Creates WiFi AP with captive portal
 *    - Provides web interface for WiFi configuration
 *    - Shows sensor data and button states
 * 
 * Features:
 *    - Random credentials generated on each boot
 *    - Setup page for custom SSID/password configuration
 *    - Main dashboard showing sensor data
 *    - DNS server redirects all requests to captive portal
 *****************************************************************/

#ifndef CAPTIVE_PORTAL_MANAGER_HPP
#define CAPTIVE_PORTAL_MANAGER_HPP

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include "Drivers/UART Comms/CpuUartBidirectional.hpp"

namespace arcos::manager {

class CaptivePortalManager {
public:
  CaptivePortalManager();
  ~CaptivePortalManager();
  
  /**
   * @brief Initialize captive portal with random or stored credentials
   * @return true if successful
   */
  bool initialize();
  
  /**
   * @brief Update DNS server (call frequently in loop)
   */
  void update();
  
  /**
   * @brief Get current SSID
   */
  String getSSID() const;
  
  /**
   * @brief Get current password
   */
  String getPassword() const;
  
  /**
   * @brief Check if using custom credentials
   */
  bool isCustomCredentials() const;
  
  /**
   * @brief Update sensor data for web display
   */
  void updateSensorData(const arcos::communication::SensorDataPayload& data);
  
  /**
   * @brief Get current sensor data (for reading web button states)
   */
  void getSensorData(arcos::communication::SensorDataPayload& data) const;
  
  /**
   * @brief Get number of connected clients
   */
  int getClientCount() const;

private:
  // Web server and DNS
  AsyncWebServer* server_;
  DNSServer* dns_server_;
  Preferences preferences_;
  
  // WiFi credentials
  String device_base_name_;
  String current_ssid_;
  String current_password_;
  bool use_custom_credentials_;
  
  // Sensor data for display
  arcos::communication::SensorDataPayload sensor_data_;
  SemaphoreHandle_t sensor_data_mutex_;
  
  /**
   * @brief Generate random SSID suffix (4 digits)
   */
  String generateRandomSuffix();
  
  /**
   * @brief Generate random password (12 chars: A-Z, 0-9)
   */
  String generateRandomPassword();
  
  /**
   * @brief Load credentials from flash storage
   */
  void loadCredentials();
  
  /**
   * @brief Save credentials to flash storage
   */
  void saveCredentials();
  
  /**
   * @brief Setup web server routes
   */
  void setupWebServer();
  
  /**
   * @brief Setup DNS server for captive portal
   */
  void setupDNSServer();
  
  /**
   * @brief Generate HTML for setup page
   */
  String generateSetupPage();
  
  /**
   * @brief Generate HTML for main dashboard
   */
  String generateDashboardPage();
  
  /**
   * @brief Handle setup form submission
   */
  void handleSetupSubmit(AsyncWebServerRequest* request);
  
  /**
   * @brief Handle API request for sensor data (JSON)
   */
  String getSensorDataJSON();
};

} // namespace arcos::manager

// Include implementation
#include "CaptivePortal/CaptivePortalManager.impl.hpp"

#endif // CAPTIVE_PORTAL_MANAGER_HPP
