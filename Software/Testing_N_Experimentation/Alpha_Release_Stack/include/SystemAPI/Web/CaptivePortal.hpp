/*****************************************************************
 * @file CaptivePortal.hpp
 * @brief WiFi Captive Portal - Main Entry Point
 * 
 * Unified facade for the captive portal system. Coordinates:
 * - WiFi Access Point (WifiManager)
 * - HTTP Server (HttpServer)
 * - DNS Server (DnsServer)
 * - Web Content (Content namespace)
 * 
 * Usage:
 *   #include "SystemAPI/Web/CaptivePortal.hpp"
 *   
 *   // Initialize
 *   CAPTIVE_PORTAL.init("MySSID", "password");
 *   
 *   // In loop
 *   CAPTIVE_PORTAL.update();
 * 
 * @author ARCOS
 * @version 2.0 (Modular Architecture)
 *****************************************************************/

#pragma once

// Core types and interfaces
#include "SystemAPI/Web/WebTypes.hpp"
#include "SystemAPI/Web/Interfaces/ICommandHandler.hpp"

// Server components
#include "SystemAPI/Web/Server/WifiManager.hpp"
#include "SystemAPI/Web/Server/HttpServer.hpp"
#include "SystemAPI/Web/Server/DnsServer.hpp"

// Content
#include "SystemAPI/Web/Content/WebContent.hpp"

// System dependencies
#include "SystemAPI/Misc/SyncState.hpp"
#include "SystemAPI/Security/SecurityDriver.hpp"

#include "esp_timer.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"

namespace SystemAPI {
namespace Web {

static const char* PORTAL_TAG = "CaptivePortal";

/**
 * @brief Captive Portal Manager
 * 
 * Main facade class that coordinates all captive portal components.
 * Provides a simple interface for initialization and runtime updates.
 */
class CaptivePortal {
public:
    /**
     * @brief Get singleton instance
     */
    static CaptivePortal& instance() {
        static CaptivePortal inst;
        return inst;
    }
    
    /**
     * @brief Initialize the captive portal
     * @param ssid Access point name
     * @param password Optional password (empty for open)
     * @return true on success
     */
    bool init(const char* ssid = "Lucidius-AP", const char* password = "") {
        if (initialized_) return true;
        
        ESP_LOGI(PORTAL_TAG, "Initializing Captive Portal: SSID=%s", ssid);
        
        // Configure portal
        PortalConfig config;
        strncpy(config.ssid, ssid, MAX_SSID_LENGTH);
        strncpy(config.password, password, MAX_PASSWORD_LENGTH);
        
        // Initialize WiFi
        if (!WIFI_MANAGER.init(config)) {
            ESP_LOGE(PORTAL_TAG, "Failed to initialize WiFi");
            return false;
        }
        
        // Update sync state
        auto& state = SYNC_STATE.state();
        strncpy(state.ssid, ssid, sizeof(state.ssid) - 1);
        snprintf(state.ipAddress, sizeof(state.ipAddress), "%s", PORTAL_IP);
        
        // Start HTTP server
        if (!HTTP_SERVER.start()) {
            ESP_LOGE(PORTAL_TAG, "Failed to start HTTP server");
            return false;
        }
        
        // Start DNS server
        if (!DNS_SERVER.start()) {
            ESP_LOGE(PORTAL_TAG, "Failed to start DNS server");
            return false;
        }
        
        initialized_ = true;
        ESP_LOGI(PORTAL_TAG, "Captive Portal initialized successfully");
        return true;
    }
    
    /**
     * @brief Update the portal (call in loop)
     * 
     * Updates statistics in sync state including uptime,
     * heap usage, and client count.
     */
    void update() {
        if (!initialized_) return;
        
        auto& state = SYNC_STATE.state();
        state.uptime = esp_timer_get_time() / 1000000;  // Convert to seconds
        state.freeHeap = esp_get_free_heap_size();
        state.wifiClients = WIFI_MANAGER.getClientCount();
        
        // Check external WiFi connection status if enabled
        if (state.extWifiConnected) {
            wifi_mode_t mode;
            esp_wifi_get_mode(&mode);
            
            if (mode == WIFI_MODE_APSTA) {
                wifi_ap_record_t ap_info;
                esp_err_t err = esp_wifi_sta_get_ap_info(&ap_info);
                
                if (err == ESP_OK) {
                    // Connected!
                    if (!state.extWifiIsConnected) {
                        ESP_LOGI(PORTAL_TAG, "External WiFi connected to: %s", ap_info.ssid);
                    }
                    state.extWifiIsConnected = true;
                    state.extWifiRSSI = ap_info.rssi;
                    
                    // Get IP address
                    esp_netif_t* sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
                    if (sta_netif) {
                        esp_netif_ip_info_t ip_info;
                        if (esp_netif_get_ip_info(sta_netif, &ip_info) == ESP_OK && ip_info.ip.addr != 0) {
                            snprintf(state.extWifiIP, sizeof(state.extWifiIP), IPSTR, IP2STR(&ip_info.ip));
                        }
                    }
                } else {
                    // Not connected yet or disconnected
                    state.extWifiIsConnected = false;
                }
            }
        } else {
            state.extWifiIsConnected = false;
        }
    }
    
    /**
     * @brief Check if portal is initialized
     */
    bool isInitialized() const { return initialized_; }
    
    /**
     * @brief Get number of connected clients
     */
    uint8_t getClientCount() const {
        return WIFI_MANAGER.getClientCount();
    }
    
    /**
     * @brief Send a notification (placeholder for future WebSocket)
     */
    void sendNotification(const char* title, const char* message, const char* type = "info") {
        ESP_LOGI(PORTAL_TAG, "Notification [%s]: %s - %s", type, title, message);
        // TODO: Implement WebSocket notifications
    }
    
    /**
     * @brief Access WiFi manager
     */
    WifiManager& wifi() { return WIFI_MANAGER; }
    
    /**
     * @brief Access HTTP server
     */
    HttpServer& http() { return HTTP_SERVER; }
    
    /**
     * @brief Access DNS server
     */
    DnsServer& dns() { return DNS_SERVER; }

private:
    CaptivePortal() = default;
    ~CaptivePortal() = default;
    
    // Prevent copying
    CaptivePortal(const CaptivePortal&) = delete;
    CaptivePortal& operator=(const CaptivePortal&) = delete;
    
    bool initialized_ = false;
};

// Convenience macro
#define CAPTIVE_PORTAL CaptivePortal::instance()

} // namespace Web
} // namespace SystemAPI
