/**
 * @file BootMode.cpp
 * @brief Boot mode implementation using SystemAPI
 * 
 * SystemAPI includes all layers: HAL, BaseAPI, FrameworkAPI
 * Use the appropriate layer for your needs.
 */

#include "BootMode.hpp"
#include "SystemAPI/SystemAPI.hpp"
#include "SystemAPI/Security/SecurityDriver.hpp"
#include "SystemAPI/Web/CaptivePortal.hpp"
#include "SystemAPI/Misc/SyncState.hpp"
#include "HAL/ESP32/Esp32HalDataStore.hpp"
#include "HAL/ESP32/Esp32HalLog.hpp"
#include "driver/gpio.h"
#include "esp_timer.h"
#include <cstdio>

namespace Modes {

//=============================================================================
// Constants
//=============================================================================

// Button B pin for factory reset
static constexpr gpio_num_t FACTORY_RESET_BUTTON = GPIO_NUM_6;
static constexpr uint32_t FACTORY_RESET_HOLD_MS = 15000;  // 15 seconds

//=============================================================================
// Static HAL Instances (Boot Lifetime)
//=============================================================================

static arcos::hal::esp32::Esp32HalLog g_logger;
static arcos::hal::esp32::Esp32HalDataStore g_datastore(&g_logger);

//=============================================================================
// Factory Reset Check
//=============================================================================

/**
 * @brief Check if B button is held for 15 seconds during boot
 * 
 * This is a blocking check that runs ONLY during early boot.
 * The button must be held continuously for the full duration.
 * Progress is shown to help user know it's working.
 * 
 * @return true if factory reset should be performed
 */
static bool checkFactoryReset() {
    // Configure button pin as input with pullup
    gpio_config_t btn_cfg = {
        .pin_bit_mask = (1ULL << FACTORY_RESET_BUTTON),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&btn_cfg);
    
    // Check if button is pressed (active low)
    if (gpio_get_level(FACTORY_RESET_BUTTON) != 0) {
        // Button not pressed at boot start - skip factory reset check
        return false;
    }
    
    printf("  ┌────────────────────────────────────┐\n");
    printf("  │    FACTORY RESET DETECTED          │\n");
    printf("  │    Hold B button for 15 seconds    │\n");
    printf("  │    Release to cancel               │\n");
    printf("  └────────────────────────────────────┘\n");
    
    uint64_t startTime = esp_timer_get_time() / 1000;  // ms
    uint32_t lastProgress = 0;
    
    while (true) {
        uint64_t elapsed = (esp_timer_get_time() / 1000) - startTime;
        
        // Check if button released
        if (gpio_get_level(FACTORY_RESET_BUTTON) != 0) {
            printf("\n  Factory reset cancelled (button released)\n\n");
            return false;
        }
        
        // Show progress every second
        uint32_t seconds = elapsed / 1000;
        if (seconds > lastProgress) {
            lastProgress = seconds;
            printf("  [%2lu/15] ", (unsigned long)seconds);
            for (uint32_t i = 0; i < seconds; i++) printf("#");
            for (uint32_t i = seconds; i < 15; i++) printf("-");
            printf("\n");
        }
        
        // Check if hold time reached
        if (elapsed >= FACTORY_RESET_HOLD_MS) {
            printf("\n  *** FACTORY RESET TRIGGERED ***\n\n");
            return true;
        }
        
        // Small delay to prevent busy loop
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

//=============================================================================
// BootMode Implementation
//=============================================================================

bool BootMode::onBoot() {
    printf("\n");
    printf("  ╔════════════════════════════════════╗\n");
    printf("  ║          BOOT SEQUENCE             ║\n");
    printf("  ╚════════════════════════════════════╝\n\n");
    
    printf("  SystemAPI Version: %s\n\n", SystemAPI::VERSION);
    
    // Initialize logger
    g_logger.init(arcos::hal::LogLevel::INFO);
    g_logger.info("BOOT", "Logger initialized");
    
    // Initialize DataStore (NVS)
    arcos::hal::DataStoreConfig ds_config;
    ds_config.namespace_name = "synthhead";
    if(g_datastore.init(ds_config) != arcos::hal::HalResult::OK){
        g_logger.error("BOOT", "Failed to initialize DataStore!");
        // Continue anyway - some features won't work
    }else{
        g_logger.info("BOOT", "DataStore initialized");
    }
    
    // Check for factory reset (B button held for 15 seconds)
    // This must be done BEFORE initializing SecurityDriver
    bool performFactoryReset = checkFactoryReset();
    
    // Initialize Security Driver
    auto& security = arcos::security::SecurityDriver::instance();
    if(!security.init(&g_datastore, &g_logger, "Lucidius", "DX.3")){
        g_logger.error("BOOT", "Failed to initialize SecurityDriver!");
    }else{
        g_logger.info("BOOT", "SecurityDriver initialized");
        
        // Perform factory reset if requested
        if (performFactoryReset) {
            printf("  *** PERFORMING FULL FACTORY RESET ***\n\n");
            
            // 1. Reset external WiFi settings (clears from NVS)
            printf("  [1/2] Clearing external WiFi settings...\n");
            security.resetExtWifiSettings();
            
            // 2. Regenerate main WiFi SSID and password
            printf("  [2/2] Regenerating WiFi credentials...\n");
            security.regenerateCredentials();
            
            // Also clear the SyncState to ensure clean slate
            auto& state = SystemAPI::SYNC_STATE.state();
            state.extWifiEnabled = false;
            state.extWifiConnected = false;
            state.extWifiIsConnected = false;
            memset(state.extWifiSSID, 0, sizeof(state.extWifiSSID));
            memset(state.extWifiPassword, 0, sizeof(state.extWifiPassword));
            memset(state.extWifiIP, 0, sizeof(state.extWifiIP));
            state.extWifiRSSI = -100;
            state.authEnabled = false;
            strncpy(state.authUsername, "admin", sizeof(state.authUsername));
            memset(state.authPassword, 0, sizeof(state.authPassword));
            memset(state.authSessionToken, 0, sizeof(state.authSessionToken));
            
            printf("\n  Factory reset complete:\n");
            printf("  - External network mode: DISABLED\n");
            printf("  - External network credentials: CLEARED\n");
            printf("  - Authentication: DISABLED\n");
            printf("  - WiFi SSID/Password: REGENERATED\n\n");
        }
        
        printf("  ┌────────────────────────────────────┐\n");
        printf("  │         WiFi Credentials           │\n");
        printf("  ├────────────────────────────────────┤\n");
        printf("  │  SSID: %-26s │\n", security.getSSID());
        printf("  │  Pass: %-26s │\n", security.getPassword());
        printf("  └────────────────────────────────────┘\n\n");
    }
    
    // Load saved external WiFi settings into SyncState
    auto& state = SystemAPI::SYNC_STATE.state();
    bool extEnabled = false;
    bool connectNow = false;
    bool authEnabled = false;
    
    security.loadExtWifiSettings(
        extEnabled, connectNow, state.extWifiSSID, sizeof(state.extWifiSSID),
        state.extWifiPassword, sizeof(state.extWifiPassword),
        authEnabled, state.authUsername, sizeof(state.authUsername),
        state.authPassword, sizeof(state.authPassword)
    );
    state.extWifiEnabled = extEnabled;
    state.authEnabled = authEnabled;
    state.extWifiConnected = connectNow;  // Restore connect toggle state
    state.extWifiIsConnected = false;     // Not actually connected yet
    
    if (extEnabled) {
        printf("  External WiFi configured: %s\n", state.extWifiSSID);
        if (connectNow) {
            printf("  Auto-connect enabled - will connect after WiFi init\n");
        } else {
            printf("  Auto-connect disabled\n");
        }
        if (authEnabled) {
            printf("  Authentication: ENABLED (user: %s)\n", state.authUsername);
        }
        printf("\n");
    }
    
    // Initialize Captive Portal with security credentials
    auto& portal = SystemAPI::Web::CaptivePortal::instance();
    if(!portal.init(security.getSSID(), security.getPassword())){
        g_logger.error("BOOT", "Failed to initialize CaptivePortal!");
    }else{
        g_logger.info("BOOT", "CaptivePortal initialized");
        printf("  WiFi Access Point Started!\n");
        printf("  Connect to: %s\n", security.getSSID());
        printf("  Password: %s\n", security.getPassword());
        printf("  Portal opens automatically on connect.\n\n");
        
        // Load IMU calibration from NVS if available
        SystemAPI::Web::HttpServer::loadImuCalibration();
        
        // Auto-connect to external network if it was enabled before reboot
        if (extEnabled && connectNow && strlen(state.extWifiSSID) > 0) {
            printf("  Connecting to external network: %s...\n", state.extWifiSSID);
            
            // Ensure STA netif exists
            esp_netif_t* sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
            if (!sta_netif) {
                sta_netif = esp_netif_create_default_wifi_sta();
            }
            
            // Configure and connect
            wifi_config_t sta_config = {};
            strncpy((char*)sta_config.sta.ssid, state.extWifiSSID, sizeof(sta_config.sta.ssid));
            strncpy((char*)sta_config.sta.password, state.extWifiPassword, sizeof(sta_config.sta.password));
            sta_config.sta.threshold.authmode = WIFI_AUTH_WPA_PSK;
            sta_config.sta.pmf_cfg.capable = true;
            sta_config.sta.pmf_cfg.required = false;
            
            // Switch to AP+STA mode
            esp_wifi_set_mode(WIFI_MODE_APSTA);
            esp_wifi_set_config(WIFI_IF_STA, &sta_config);
            esp_wifi_connect();
            
            printf("  External WiFi connection initiated.\n\n");
        }
    }
    
    // TODO: Initialize additional sensors via HAL layer
    // Example: HAL::I2C, HAL::IMU, HAL::Environmental, etc.
    
    printf("  Boot complete!\n\n");
    
    // Boot succeeds even if some components fail
    return true;
}

void BootMode::onDebugBoot() {
    printf("  Debug Boot - Minimal initialization\n");
}

} // namespace Modes
