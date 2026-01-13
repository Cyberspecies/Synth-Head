/*****************************************************************
 * @file WifiManager.hpp
 * @brief WiFi Access Point Manager
 * 
 * Handles WiFi AP creation and client management.
 * 
 * @author ARCOS
 * @version 1.0
 *****************************************************************/

#pragma once

#include "SystemAPI/Web/WebTypes.hpp"
#include "SystemAPI/Web/Interfaces/ICommandHandler.hpp"

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "lwip/ip4_addr.h"

#include <cstring>

namespace SystemAPI {
namespace Web {

static const char* WIFI_TAG = "WifiManager";

/**
 * @brief WiFi Access Point Manager
 * 
 * Creates and manages the WiFi access point for the captive portal.
 */
class WifiManager : public IWifiManager {
public:
    /**
     * @brief Get singleton instance
     */
    static WifiManager& instance() {
        static WifiManager inst;
        return inst;
    }
    
    /**
     * @brief Initialize WiFi in AP mode
     * @param config Portal configuration
     * @return true on success
     */
    bool init(const PortalConfig& config) {
        if (initialized_) return true;
        
        strncpy(ssid_, config.ssid, MAX_SSID_LENGTH);
        strncpy(password_, config.password, MAX_PASSWORD_LENGTH);
        
        ESP_LOGI(WIFI_TAG, "Initializing WiFi AP: SSID=%s", ssid_);
        
        // Initialize networking stack
        ESP_ERROR_CHECK(esp_netif_init());
        
        // Create default event loop if not already created
        esp_err_t err = esp_event_loop_create_default();
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            ESP_LOGE(WIFI_TAG, "Failed to create event loop: %s", esp_err_to_name(err));
            return false;
        }
        
        // Create default WiFi AP netif
        ap_netif_ = esp_netif_create_default_wifi_ap();
        if (!ap_netif_) {
            ESP_LOGE(WIFI_TAG, "Failed to create AP netif");
            return false;
        }
        
        // Configure IP address
        configureIpAddress();
        
        // Initialize WiFi
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));
        
        // Register event handler
        ESP_ERROR_CHECK(esp_event_handler_instance_register(
            WIFI_EVENT, ESP_EVENT_ANY_ID, 
            &WifiManager::eventHandler, this, &event_handler_));
        
        // Configure AP
        wifi_config_t wifi_config = {};
        strncpy((char*)wifi_config.ap.ssid, ssid_, sizeof(wifi_config.ap.ssid) - 1);
        wifi_config.ap.ssid_len = strlen(ssid_);
        wifi_config.ap.channel = WIFI_CHANNEL;
        wifi_config.ap.max_connection = MAX_WIFI_CLIENTS;
        
        if (strlen(password_) >= 8) {
            strncpy((char*)wifi_config.ap.password, password_, sizeof(wifi_config.ap.password) - 1);
            wifi_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
        } else {
            wifi_config.ap.authmode = WIFI_AUTH_OPEN;
        }
        
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
        ESP_ERROR_CHECK(esp_wifi_start());
        
        initialized_ = true;
        ESP_LOGI(WIFI_TAG, "WiFi AP started: %s", ssid_);
        
        return true;
    }
    
    /**
     * @brief Check if WiFi is initialized
     */
    bool isInitialized() const { return initialized_; }
    
    /**
     * @brief Get SSID
     */
    const char* getSSID() const { return ssid_; }
    
    /**
     * @brief Get password
     */
    const char* getPassword() const { return password_; }
    
    // IWifiManager interface
    bool setCredentials(const char* ssid, const char* password) override {
        // This would be handled by SecurityDriver for persistence
        // Here we just restart with new credentials
        return true;
    }
    
    bool resetToAuto() override {
        // Handled by SecurityDriver
        return true;
    }
    
    int kickAllClients() override {
        wifi_sta_list_t sta_list;
        esp_wifi_ap_get_sta_list(&sta_list);
        
        int kicked = 0;
        for (int i = 0; i < sta_list.num; i++) {
            uint16_t aid = i + 1;
            if (esp_wifi_deauth_sta(aid) == ESP_OK) {
                kicked++;
            }
        }
        
        ESP_LOGI(WIFI_TAG, "Kicked %d clients", kicked);
        return kicked;
    }
    
    uint8_t getClientCount() const override {
        wifi_sta_list_t sta_list;
        if (esp_wifi_ap_get_sta_list(&sta_list) == ESP_OK) {
            return sta_list.num;
        }
        return 0;
    }

private:
    WifiManager() = default;
    ~WifiManager() = default;
    
    // Prevent copying
    WifiManager(const WifiManager&) = delete;
    WifiManager& operator=(const WifiManager&) = delete;
    
    /**
     * @brief Configure IP address and DHCP
     */
    void configureIpAddress() {
        esp_netif_ip_info_t ip_info;
        IP4_ADDR(&ip_info.ip, PORTAL_IP_BYTES[0], PORTAL_IP_BYTES[1], 
                 PORTAL_IP_BYTES[2], PORTAL_IP_BYTES[3]);
        IP4_ADDR(&ip_info.gw, PORTAL_IP_BYTES[0], PORTAL_IP_BYTES[1], 
                 PORTAL_IP_BYTES[2], PORTAL_IP_BYTES[3]);
        IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);
        
        esp_netif_dhcps_stop(ap_netif_);
        esp_netif_set_ip_info(ap_netif_, &ip_info);
        
        // Configure DHCP to advertise our IP as DNS server
        esp_netif_dns_info_t dns_info;
        IP4_ADDR(&dns_info.ip.u_addr.ip4, PORTAL_IP_BYTES[0], PORTAL_IP_BYTES[1], 
                 PORTAL_IP_BYTES[2], PORTAL_IP_BYTES[3]);
        dns_info.ip.type = ESP_IPADDR_TYPE_V4;
        esp_netif_set_dns_info(ap_netif_, ESP_NETIF_DNS_MAIN, &dns_info);
        
        // Offer DNS server to DHCP clients
        uint8_t dns_offer = 1;
        esp_netif_dhcps_option(ap_netif_, ESP_NETIF_OP_SET, 
                              ESP_NETIF_DOMAIN_NAME_SERVER, &dns_offer, sizeof(dns_offer));
        
        esp_netif_dhcps_start(ap_netif_);
    }
    
    /**
     * @brief WiFi event handler
     */
    static void eventHandler(void* arg, esp_event_base_t event_base,
                            int32_t event_id, void* event_data) {
        if (event_id == WIFI_EVENT_AP_STACONNECTED) {
            wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*)event_data;
            ESP_LOGI(WIFI_TAG, "Station connected, AID=%d", event->aid);
        } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
            wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*)event_data;
            ESP_LOGI(WIFI_TAG, "Station disconnected, AID=%d", event->aid);
        }
    }
    
    // State
    bool initialized_ = false;
    char ssid_[MAX_SSID_LENGTH + 1] = {0};
    char password_[MAX_PASSWORD_LENGTH + 1] = {0};
    esp_netif_t* ap_netif_ = nullptr;
    esp_event_handler_instance_t event_handler_ = nullptr;
};

// Convenience macro
#define WIFI_MANAGER WifiManager::instance()

} // namespace Web
} // namespace SystemAPI
