/*****************************************************************
 * @file CaptivePortal.hpp
 * @brief WiFi Captive Portal with HTTP server for ESP-IDF
 * 
 * Creates a WiFi access point with a captive portal that serves
 * a web interface. Uses ESP-IDF native APIs for WiFi and HTTP.
 * 
 * @author ARCOS
 * @version 2.0 (ESP-IDF Native)
 *****************************************************************/

#pragma once

#include "SystemAPI/Misc/SyncState.hpp"
#include "SystemAPI/Security/SecurityDriver.hpp"

#include <cstring>
#include <cstdio>
#include <functional>

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "esp_timer.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "cJSON.h"

namespace SystemAPI {
namespace Web {

static const char* TAG = "CaptivePortal";

// Forward declarations of embedded web content
extern const char INDEX_HTML[];
extern const char STYLE_CSS[];
extern const char SCRIPT_JS[];

/**
 * @brief Captive Portal Manager (ESP-IDF Native)
 * 
 * Handles WiFi AP creation, DNS for captive portal,
 * and HTTP server for serving web interface.
 */
class CaptivePortal {
public:
    static CaptivePortal& instance() {
        static CaptivePortal inst;
        return inst;
    }

    /**
     * @brief Initialize the captive portal
     * @param ssid Access point name
     * @param password Optional password (empty for open)
     */
    bool init(const char* ssid = "Lucidius-AP", const char* password = "") {
        if (initialized_) return true;
        
        strncpy(ssid_, ssid, sizeof(ssid_) - 1);
        strncpy(password_, password, sizeof(password_) - 1);
        
        ESP_LOGI(TAG, "Initializing Captive Portal: SSID=%s", ssid_);
        
        // Initialize networking stack
        ESP_ERROR_CHECK(esp_netif_init());
        
        // Create default event loop if not already created
        esp_err_t err = esp_event_loop_create_default();
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            ESP_LOGE(TAG, "Failed to create event loop: %s", esp_err_to_name(err));
            return false;
        }
        
        // Create default WiFi AP netif
        ap_netif_ = esp_netif_create_default_wifi_ap();
        if (!ap_netif_) {
            ESP_LOGE(TAG, "Failed to create AP netif");
            return false;
        }
        
        // Configure custom IP address (192.168.4.1 - standard ESP32 AP address)
        // Also accessible via any domain (e.g. go.to, setup.me, etc.) - DNS redirects all
        esp_netif_ip_info_t ip_info;
        IP4_ADDR(&ip_info.ip, 192, 168, 4, 1);
        IP4_ADDR(&ip_info.gw, 192, 168, 4, 1);
        IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);
        
        esp_netif_dhcps_stop(ap_netif_);
        esp_netif_set_ip_info(ap_netif_, &ip_info);
        
        // Configure DHCP to advertise our IP as the DNS server
        // This is CRITICAL for captive portal - phones must use our DNS
        esp_netif_dns_info_t dns_info;
        IP4_ADDR(&dns_info.ip.u_addr.ip4, 192, 168, 4, 1);
        dns_info.ip.type = ESP_IPADDR_TYPE_V4;
        esp_netif_set_dns_info(ap_netif_, ESP_NETIF_DNS_MAIN, &dns_info);
        
        // Set DHCP option to offer our DNS server to clients
        uint8_t dns_offer = 1;
        esp_netif_dhcps_option(ap_netif_, ESP_NETIF_OP_SET, ESP_NETIF_DOMAIN_NAME_SERVER, &dns_offer, sizeof(dns_offer));
        
        esp_netif_dhcps_start(ap_netif_);
        
        // Initialize WiFi
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));
        
        // Register event handler
        ESP_ERROR_CHECK(esp_event_handler_instance_register(
            WIFI_EVENT, ESP_EVENT_ANY_ID, 
            &CaptivePortal::wifiEventHandler, this, &wifi_event_handler_));
        
        // Configure AP
        wifi_config_t wifi_config = {};
        strncpy((char*)wifi_config.ap.ssid, ssid_, sizeof(wifi_config.ap.ssid) - 1);
        wifi_config.ap.ssid_len = strlen(ssid_);
        wifi_config.ap.channel = 1;
        wifi_config.ap.max_connection = 4;
        
        if (strlen(password_) >= 8) {
            strncpy((char*)wifi_config.ap.password, password_, sizeof(wifi_config.ap.password) - 1);
            wifi_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
        } else {
            wifi_config.ap.authmode = WIFI_AUTH_OPEN;
        }
        
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
        ESP_ERROR_CHECK(esp_wifi_start());
        
        ESP_LOGI(TAG, "WiFi AP started: %s", ssid_);
        
        // Update sync state
        auto& state = SYNC_STATE.state();
        strncpy(state.ssid, ssid_, sizeof(state.ssid) - 1);
        snprintf(state.ipAddress, sizeof(state.ipAddress), "192.168.4.1");
        
        // Start HTTP server
        if (!startHttpServer()) {
            ESP_LOGE(TAG, "Failed to start HTTP server");
            return false;
        }
        
        // Start DNS server task for captive portal
        xTaskCreate(&CaptivePortal::dnsServerTask, "dns_server", 4096, this, 5, &dns_task_);
        
        initialized_ = true;
        ESP_LOGI(TAG, "Captive Portal initialized successfully");
        return true;
    }

    /**
     * @brief Update the portal (call in loop)
     */
    void update() {
        if (!initialized_) return;
        
        // Update stats in sync state
        auto& state = SYNC_STATE.state();
        state.uptime = esp_timer_get_time() / 1000000;  // Convert to seconds
        state.freeHeap = esp_get_free_heap_size();
        
        // Get connected station count
        wifi_sta_list_t sta_list;
        if (esp_wifi_ap_get_sta_list(&sta_list) == ESP_OK) {
            state.wifiClients = sta_list.num;
        }
    }

    /**
     * @brief Get number of connected clients
     */
    uint8_t getClientCount() const {
        wifi_sta_list_t sta_list;
        if (esp_wifi_ap_get_sta_list(&sta_list) == ESP_OK) {
            return sta_list.num;
        }
        return 0;
    }

    /**
     * @brief Send a notification (placeholder for future WebSocket)
     */
    void sendNotification(const char* title, const char* message, const char* type = "info") {
        ESP_LOGI(TAG, "Notification [%s]: %s - %s", type, title, message);
    }

private:
    CaptivePortal() = default;
    ~CaptivePortal() {
        if (http_server_) {
            httpd_stop(http_server_);
        }
        if (dns_task_) {
            vTaskDelete(dns_task_);
        }
    }

    // WiFi event handler
    static void wifiEventHandler(void* arg, esp_event_base_t event_base,
                                  int32_t event_id, void* event_data) {
        CaptivePortal* self = static_cast<CaptivePortal*>(arg);
        
        if (event_id == WIFI_EVENT_AP_STACONNECTED) {
            wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*)event_data;
            ESP_LOGI(TAG, "Station connected, AID=%d", event->aid);
        } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
            wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*)event_data;
            ESP_LOGI(TAG, "Station disconnected, AID=%d", event->aid);
        }
    }

    // HTTP Server Setup
    bool startHttpServer() {
        httpd_config_t config = HTTPD_DEFAULT_CONFIG();
        config.max_uri_handlers = 30;
        config.stack_size = 8192;
        config.lru_purge_enable = true;
        config.uri_match_fn = httpd_uri_match_wildcard;  // Enable wildcard matching
        
        if (httpd_start(&http_server_, &config) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to start HTTP server");
            return false;
        }
        
        // Register URI handlers
        registerUriHandlers();
        
        ESP_LOGI(TAG, "HTTP server started on port 80");
        return true;
    }

    void registerUriHandlers() {
        // Main page
        httpd_uri_t index_uri = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = handleIndex,
            .user_ctx = this
        };
        httpd_register_uri_handler(http_server_, &index_uri);
        
        // CSS
        httpd_uri_t css_uri = {
            .uri = "/style.css",
            .method = HTTP_GET,
            .handler = handleCss,
            .user_ctx = this
        };
        httpd_register_uri_handler(http_server_, &css_uri);
        
        // JavaScript
        httpd_uri_t js_uri = {
            .uri = "/script.js",
            .method = HTTP_GET,
            .handler = handleJs,
            .user_ctx = this
        };
        httpd_register_uri_handler(http_server_, &js_uri);
        
        // API state endpoint
        httpd_uri_t api_uri = {
            .uri = "/api/state",
            .method = HTTP_GET,
            .handler = handleApiState,
            .user_ctx = this
        };
        httpd_register_uri_handler(http_server_, &api_uri);
        
        // API command endpoint
        httpd_uri_t cmd_uri = {
            .uri = "/api/command",
            .method = HTTP_POST,
            .handler = handleApiCommand,
            .user_ctx = this
        };
        httpd_register_uri_handler(http_server_, &cmd_uri);
        
        // Captive portal detection endpoints - comprehensive list
        const char* redirect_paths[] = {
            // Android
            "/generate_204", "/gen_204",
            "/connectivitycheck.gstatic.com",
            // Windows  
            "/connecttest.txt", "/fwlink", "/redirect",
            "/ncsi.txt", "/connecttest.html",
            // Apple iOS/macOS
            "/library/test/success.html",
            "/hotspot-detect.html",
            "/captive.apple.com",
            // Amazon Kindle
            "/kindle-wifi/wifistub.html",
            // Firefox
            "/success.txt", "/canonical.html",
            "/detectportal.firefox.com",
            // Generic
            "/check_network_status.txt",
            "/chat", "/favicon.ico"
        };
        
        for (const char* path : redirect_paths) {
            httpd_uri_t redirect_uri = {
                .uri = path,
                .method = HTTP_GET,
                .handler = handleRedirect,
                .user_ctx = this
            };
            httpd_register_uri_handler(http_server_, &redirect_uri);
        }
        
        // Wildcard catch-all handler - redirects any unknown path to portal
        // Must be registered LAST so specific handlers take precedence
        httpd_uri_t catchall_uri = {
            .uri = "/*",
            .method = HTTP_GET,
            .handler = handleCatchAll,
            .user_ctx = this
        };
        httpd_register_uri_handler(http_server_, &catchall_uri);
    }

    // HTTP Handlers
    static esp_err_t handleIndex(httpd_req_t* req) {
        char host_header[128] = {0};
        httpd_req_get_hdr_value_str(req, "Host", host_header, sizeof(host_header));
        ESP_LOGI(TAG, "Index request from Host: %s", host_header);
        httpd_resp_set_type(req, "text/html");
        httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
        httpd_resp_send(req, INDEX_HTML, HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    static esp_err_t handleCss(httpd_req_t* req) {
        httpd_resp_set_type(req, "text/css");
        httpd_resp_send(req, STYLE_CSS, HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    static esp_err_t handleJs(httpd_req_t* req) {
        httpd_resp_set_type(req, "application/javascript");
        httpd_resp_send(req, SCRIPT_JS, HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    static esp_err_t handleApiState(httpd_req_t* req) {
        CaptivePortal* self = static_cast<CaptivePortal*>(req->user_ctx);
        
        auto& state = SYNC_STATE.state();
        
        cJSON* root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "type", "state");
        cJSON_AddStringToObject(root, "ssid", state.ssid);
        cJSON_AddStringToObject(root, "ip", state.ipAddress);
        cJSON_AddNumberToObject(root, "clients", state.wifiClients);
        cJSON_AddNumberToObject(root, "uptime", state.uptime);
        cJSON_AddNumberToObject(root, "freeHeap", state.freeHeap);
        cJSON_AddNumberToObject(root, "brightness", state.brightness);
        cJSON_AddNumberToObject(root, "mode", (int)state.mode);
        
        char* json_str = cJSON_PrintUnformatted(root);
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, json_str, HTTPD_RESP_USE_STRLEN);
        
        free(json_str);
        cJSON_Delete(root);
        
        return ESP_OK;
    }
    
    static esp_err_t handleApiCommand(httpd_req_t* req) {
        CaptivePortal* self = static_cast<CaptivePortal*>(req->user_ctx);
        
        char buf[512];
        int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
        if (ret <= 0) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No data");
            return ESP_FAIL;
        }
        buf[ret] = '\0';
        
        cJSON* root = cJSON_Parse(buf);
        if (!root) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
            return ESP_FAIL;
        }
        
        cJSON* cmd = cJSON_GetObjectItem(root, "cmd");
        if (cmd && cmd->valuestring) {
            self->processCommand(cmd->valuestring, root);
        }
        
        cJSON_Delete(root);
        
        // Send success response
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"success\":true}", HTTPD_RESP_USE_STRLEN);
        
        return ESP_OK;
    }
    
    static esp_err_t handleRedirect(httpd_req_t* req) {
        // For captive portal detection, return a redirect to trigger the popup
        httpd_resp_set_status(req, "302 Found");
        httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
        httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    }
    
    static esp_err_t handleHotspotDetect(httpd_req_t* req) {
        // Apple captive portal detection:
        // Returning anything OTHER than "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>"
        // will trigger the captive portal popup
        // We redirect to our portal instead
        httpd_resp_set_status(req, "302 Found");
        httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
        httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    }
    
    static esp_err_t handleGenerate204(httpd_req_t* req) {
        // Android expects 204 No Content if internet works
        // Returning anything else (like a redirect) triggers captive portal
        httpd_resp_set_status(req, "302 Found");
        httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
        httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    }
    
    static esp_err_t handleCatchAll(httpd_req_t* req) {
        // Catch-all handler - serve portal directly for ANY request
        // This handles cases where browser tries HTTPS first (connection fails)
        // then falls back to HTTP - we serve the portal instead of redirecting
        char host_header[128] = {0};
        httpd_req_get_hdr_value_str(req, "Host", host_header, sizeof(host_header));
        ESP_LOGI(TAG, "Catch-all request: Host=%s URI=%s", host_header, req->uri);
        
        // Serve portal content directly - no redirect needed
        httpd_resp_set_type(req, "text/html");
        httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
        httpd_resp_send(req, INDEX_HTML, HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }

    void processCommand(const char* cmd, cJSON* root) {
        auto& state = SYNC_STATE.state();
        
        if (strcmp(cmd, "setBrightness") == 0) {
            cJSON* val = cJSON_GetObjectItem(root, "value");
            if (val) SYNC_STATE.setBrightness(val->valueint);
        }
        else if (strcmp(cmd, "setWifiCredentials") == 0) {
            cJSON* ssid = cJSON_GetObjectItem(root, "ssid");
            cJSON* password = cJSON_GetObjectItem(root, "password");
            if (ssid && password && ssid->valuestring && password->valuestring) {
                ESP_LOGI(TAG, "WiFi credentials update requested: %s", ssid->valuestring);
                
                // Save to flash using SecurityDriver
                auto& security = arcos::security::SecurityDriver::instance();
                if (security.setCustomCredentials(ssid->valuestring, password->valuestring)) {
                    ESP_LOGI(TAG, "Custom credentials saved to flash successfully");
                    // Schedule restart after response is sent
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    esp_restart();
                } else {
                    ESP_LOGE(TAG, "Failed to save custom credentials");
                }
            }
        }
        else if (strcmp(cmd, "resetWifiToAuto") == 0) {
            ESP_LOGI(TAG, "WiFi reset to auto requested");
            
            // Reset to auto-generated credentials
            auto& security = arcos::security::SecurityDriver::instance();
            if (security.resetToAuto()) {
                ESP_LOGI(TAG, "Reset to auto credentials successful");
                vTaskDelay(pdMS_TO_TICKS(1000));
                esp_restart();
            } else {
                ESP_LOGE(TAG, "Failed to reset credentials");
            }
        }
        else if (strcmp(cmd, "restart") == 0) {
            ESP_LOGI(TAG, "Restart requested");
            vTaskDelay(pdMS_TO_TICKS(500));
            esp_restart();
        }
        else if (strcmp(cmd, "kickOtherClients") == 0) {
            ESP_LOGI(TAG, "Kick other clients requested");
            // Get list of connected stations
            wifi_sta_list_t sta_list;
            esp_wifi_ap_get_sta_list(&sta_list);
            
            ESP_LOGI(TAG, "Found %d connected clients", sta_list.num);
            
            // Deauth each station by MAC address
            int kicked = 0;
            for (int i = 0; i < sta_list.num; i++) {
                // Use esp_wifi_deauth_sta with AID (1-based index + 1 works in most cases)
                // Alternative: kick all by toggling AP off/on briefly
                uint16_t aid = i + 1;  // AIDs are 1-based
                esp_err_t err = esp_wifi_deauth_sta(aid);
                if (err == ESP_OK) {
                    kicked++;
                    ESP_LOGI(TAG, "Kicked client #%d (AID=%d)", i, aid);
                } else {
                    ESP_LOGW(TAG, "Failed to kick client #%d: %s", i, esp_err_to_name(err));
                }
            }
            ESP_LOGI(TAG, "Kicked %d clients total", kicked);
        }
    }

    // DNS Server for Captive Portal
    static void dnsServerTask(void* param) {
        CaptivePortal* self = static_cast<CaptivePortal*>(param);
        
        int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (sock < 0) {
            ESP_LOGE(TAG, "Failed to create DNS socket");
            vTaskDelete(NULL);
            return;
        }
        
        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        server_addr.sin_port = htons(53);
        
        if (bind(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            ESP_LOGE(TAG, "Failed to bind DNS socket");
            close(sock);
            vTaskDelete(NULL);
            return;
        }
        
        ESP_LOGI(TAG, "DNS server started on port 53");
        
        uint8_t buffer[512];
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        while (true) {
            client_len = sizeof(client_addr);  // Reset each time
            int len = recvfrom(sock, buffer, sizeof(buffer), 0, 
                              (struct sockaddr*)&client_addr, &client_len);
            
            if (len > 12) {
                // Extract domain name from query for logging
                char domain[128] = {0};
                int pos = 12;
                int domain_pos = 0;
                while (pos < len && buffer[pos] != 0 && domain_pos < 126) {
                    int label_len = buffer[pos++];
                    if (domain_pos > 0) domain[domain_pos++] = '.';
                    for (int i = 0; i < label_len && pos < len && domain_pos < 126; i++) {
                        domain[domain_pos++] = buffer[pos++];
                    }
                }
                ESP_LOGI(TAG, "DNS query: %s -> 192.168.4.1", domain);
                
                // Build DNS response pointing to our IP
                uint8_t response[512];
                int resp_len = self->buildDnsResponse(buffer, len, response, sizeof(response));
                
                if (resp_len > 0) {
                    sendto(sock, response, resp_len, 0, 
                          (struct sockaddr*)&client_addr, client_len);
                }
            }
            
            vTaskDelay(pdMS_TO_TICKS(1));  // Faster response
        }
    }

    int buildDnsResponse(uint8_t* query, int query_len, uint8_t* response, int max_len) {
        if (query_len < 12) return 0;
        
        // Copy query header
        memcpy(response, query, query_len);
        
        // Set response flags
        response[2] = 0x81;  // QR=1, Opcode=0, AA=0, TC=0, RD=1
        response[3] = 0x80;  // RA=1, Z=0, RCODE=0
        
        // Set answer count = 1
        response[6] = 0x00;
        response[7] = 0x01;
        
        // Find end of question section
        int qname_end = 12;
        while (qname_end < query_len && query[qname_end] != 0) {
            qname_end += query[qname_end] + 1;
        }
        qname_end++;  // Skip null terminator
        qname_end += 4;  // Skip QTYPE and QCLASS
        
        int pos = qname_end;
        
        // Add answer section
        // Name pointer to question
        response[pos++] = 0xC0;
        response[pos++] = 0x0C;
        
        // Type A (1)
        response[pos++] = 0x00;
        response[pos++] = 0x01;
        
        // Class IN (1)
        response[pos++] = 0x00;
        response[pos++] = 0x01;
        
        // TTL (60 seconds)
        response[pos++] = 0x00;
        response[pos++] = 0x00;
        response[pos++] = 0x00;
        response[pos++] = 0x3C;
        
        // RDLENGTH (4 bytes for IP)
        response[pos++] = 0x00;
        response[pos++] = 0x04;
        
        // RDATA (192.168.4.1)
        response[pos++] = 192;
        response[pos++] = 168;
        response[pos++] = 4;
        response[pos++] = 1;
        
        return pos;
    }

    // State
    bool initialized_ = false;
    char ssid_[32] = "Lucidius-AP";
    char password_[64] = "";
    
    // ESP-IDF handles
    esp_netif_t* ap_netif_ = nullptr;
    httpd_handle_t http_server_ = nullptr;
    TaskHandle_t dns_task_ = nullptr;
    esp_event_handler_instance_t wifi_event_handler_ = nullptr;
};

// Convenience macro
#define CAPTIVE_PORTAL CaptivePortal::instance()

// ============================================================
// Embedded Web Content
// ============================================================

inline const char INDEX_HTML[] = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
  <title>Lucidius Control Panel</title>
  <link rel="stylesheet" href="/style.css">
</head>
<body>
  <div class="container">
    <header>
      <div class="header-content">
        <div class="logo-section">
          <div class="logo-icon">&#x25C8;</div>
          <div class="logo-text">
            <h1>Lucidius</h1>
            <span class="model-tag" id="device-model">DX.3</span>
          </div>
        </div>
        <div class="status-indicator">
          <span id="connection-dot" class="dot disconnected"></span>
          <span id="connection-text">Connecting...</span>
        </div>
      </div>
    </header>
    
    <nav class="tabs">
      <button class="tab active" data-tab="basic">Basic</button>
      <button class="tab" data-tab="advanced">Advanced</button>
      <button class="tab" data-tab="settings">Settings</button>
    </nav>
    
    <!-- Basic Tab -->
    <section id="basic" class="tab-content active">
      <div class="card">
        <div class="card-header">
          <h2>Welcome</h2>
        </div>
        <div class="card-body">
          <p class="welcome-text">
            Connected to <strong id="welcome-ssid">Lucidius (DX.3)</strong>
          </p>
          <div class="info-grid">
            <div class="info-item">
              <span class="info-label">IP Address</span>
              <span class="info-value" id="info-ip">192.168.4.1</span>
            </div>
            <div class="info-item">
              <span class="info-label">Clients</span>
              <span class="info-value" id="info-clients">0</span>
            </div>
          </div>
        </div>
      </div>
      
      <div class="card placeholder-card">
        <div class="card-body center">
          <div class="placeholder-icon">&#127899;</div>
          <p class="placeholder-text">Basic controls coming soon</p>
        </div>
      </div>
    </section>
    
    <!-- Advanced Tab -->
    <section id="advanced" class="tab-content">
      <div class="card placeholder-card">
        <div class="card-body center">
          <div class="placeholder-icon">&#9889;</div>
          <p class="placeholder-text">Advanced features coming soon</p>
        </div>
      </div>
    </section>
    
    <!-- Settings Tab -->
    <section id="settings" class="tab-content">
      <div class="card">
        <div class="card-header">
          <h2>WiFi Configuration</h2>
        </div>
        <div class="card-body">
          <div class="current-wifi">
            <span class="wifi-label">Current Network:</span>
            <span class="wifi-value" id="current-ssid">Loading...</span>
            <span class="wifi-badge" id="wifi-mode-badge">Auto</span>
          </div>
          
          <div class="form-group">
            <label for="custom-ssid">Network Name (SSID)</label>
            <input type="text" id="custom-ssid" class="input" placeholder="Enter custom SSID" maxlength="32">
          </div>
          
          <div class="form-group">
            <label for="custom-password">Password</label>
            <div class="password-input-wrapper">
              <input type="password" id="custom-password" class="input" placeholder="Enter password (8-12 chars)" minlength="8" maxlength="12">
              <button type="button" class="password-toggle" id="toggle-password">&#128065;</button>
            </div>
            <span class="input-hint">Password must be 8-12 characters</span>
          </div>
          
          <div class="button-group">
            <button id="save-wifi-btn" class="btn btn-primary">Save Changes</button>
            <button id="reset-wifi-btn" class="btn btn-secondary">Reset to Auto</button>
          </div>
          
          <div class="warning-box" id="restart-warning" style="display: none;">
            <span class="warning-icon">&#9888;</span>
            <span class="warning-text">Restart required to apply WiFi changes</span>
          </div>
        </div>
      </div>
      
      <div class="card">
        <div class="card-header">
          <h2>Device Info</h2>
        </div>
        <div class="card-body">
          <div class="info-list">
            <div class="info-row">
              <span class="info-label">Firmware</span>
              <span class="info-value">v1.0.0</span>
            </div>
            <div class="info-row">
              <span class="info-label">Uptime</span>
              <span class="info-value" id="device-uptime">00:00:00</span>
            </div>
            <div class="info-row">
              <span class="info-label">Free Memory</span>
              <span class="info-value" id="device-heap">-- KB</span>
            </div>
          </div>
        </div>
      </div>
      
      <div class="card danger-card">
        <div class="card-header">
          <h2>Danger Zone</h2>
        </div>
        <div class="card-body">
          <button id="kick-clients-btn" class="btn btn-warning">Kick All Other Clients</button>
          <button id="restart-btn" class="btn btn-danger">Restart Device</button>
        </div>
      </div>
    </section>
    
    <footer>
      <p>Lucidius &bull; ARCOS Framework</p>
    </footer>
  </div>
  
  <div id="toast" class="toast"></div>
  
  <script src="/script.js"></script>
</body>
</html>
)rawliteral";

inline const char STYLE_CSS[] = R"rawliteral(
:root {
  --bg-primary: #0a0a0a;
  --bg-secondary: #111111;
  --bg-tertiary: #1a1a1a;
  --bg-card: #141414;
  --text-primary: #ffffff;
  --text-secondary: #888888;
  --text-muted: #555555;
  --accent: #ff6b00;
  --accent-hover: #ff8533;
  --accent-glow: rgba(255, 107, 0, 0.3);
  --accent-subtle: rgba(255, 107, 0, 0.1);
  --success: #00cc66;
  --warning: #ffaa00;
  --danger: #ff3333;
  --border: #2a2a2a;
  --border-accent: #ff6b00;
}

* {
  margin: 0;
  padding: 0;
  box-sizing: border-box;
}

body {
  font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
  background: var(--bg-primary);
  color: var(--text-primary);
  min-height: 100vh;
  line-height: 1.6;
}

.container {
  max-width: 480px;
  margin: 0 auto;
  padding: 16px;
}

header {
  padding: 20px 0;
  margin-bottom: 20px;
  border-bottom: 1px solid var(--border);
}

.header-content {
  display: flex;
  justify-content: space-between;
  align-items: center;
}

.logo-section {
  display: flex;
  align-items: center;
  gap: 12px;
}

.logo-icon {
  font-size: 2rem;
  color: var(--accent);
  text-shadow: 0 0 20px var(--accent-glow);
}

.logo-text h1 {
  font-size: 1.5rem;
  font-weight: 700;
  color: var(--text-primary);
  margin: 0;
  line-height: 1.2;
}

.model-tag {
  font-size: 0.7rem;
  color: var(--accent);
  background: var(--accent-subtle);
  padding: 2px 8px;
  border-radius: 4px;
  font-weight: 600;
  letter-spacing: 0.5px;
}

.status-indicator {
  display: flex;
  align-items: center;
  gap: 8px;
  font-size: 0.85rem;
  color: var(--text-secondary);
}

.dot {
  width: 8px;
  height: 8px;
  border-radius: 50%;
  transition: all 0.3s;
}

.dot.connected {
  background: var(--success);
  box-shadow: 0 0 8px var(--success);
}

.dot.disconnected {
  background: var(--danger);
}

.tabs {
  display: flex;
  gap: 8px;
  margin-bottom: 20px;
  background: var(--bg-secondary);
  padding: 4px;
  border-radius: 12px;
  border: 1px solid var(--border);
}

.tab {
  flex: 1;
  background: transparent;
  border: none;
  color: var(--text-secondary);
  padding: 12px 16px;
  border-radius: 8px;
  cursor: pointer;
  font-size: 0.9rem;
  font-weight: 500;
  transition: all 0.2s;
}

.tab:hover {
  color: var(--text-primary);
  background: var(--bg-tertiary);
}

.tab.active {
  background: var(--accent);
  color: var(--bg-primary);
  box-shadow: 0 0 15px var(--accent-glow);
}

.tab-content {
  display: none;
}

.tab-content.active {
  display: block;
  animation: fadeIn 0.3s ease;
}

@keyframes fadeIn {
  from { opacity: 0; transform: translateY(10px); }
  to { opacity: 1; transform: translateY(0); }
}

.card {
  background: var(--bg-card);
  border: 1px solid var(--border);
  border-radius: 16px;
  margin-bottom: 16px;
  overflow: hidden;
}

.card-header {
  padding: 16px 20px;
  border-bottom: 1px solid var(--border);
}

.card-header h2 {
  font-size: 1rem;
  font-weight: 600;
  color: var(--text-primary);
  margin: 0;
}

.card-body {
  padding: 20px;
}

.card-body.center {
  text-align: center;
  padding: 40px 20px;
}

.danger-card {
  border-color: rgba(255, 51, 51, 0.3);
}

.danger-card .card-header {
  border-color: rgba(255, 51, 51, 0.3);
}

.danger-card .card-header h2 {
  color: var(--danger);
}

.placeholder-card {
  border-style: dashed;
  border-color: var(--border);
}

.placeholder-icon {
  font-size: 3rem;
  margin-bottom: 12px;
  opacity: 0.5;
}

.placeholder-text {
  color: var(--text-muted);
  font-size: 0.9rem;
}

.welcome-text {
  color: var(--text-secondary);
  margin-bottom: 20px;
}

.welcome-text strong {
  color: var(--accent);
}

.info-grid {
  display: grid;
  grid-template-columns: repeat(2, 1fr);
  gap: 16px;
}

.info-item {
  background: var(--bg-tertiary);
  padding: 16px;
  border-radius: 12px;
  border-left: 3px solid var(--accent);
}

.info-label {
  display: block;
  font-size: 0.75rem;
  color: var(--text-muted);
  text-transform: uppercase;
  letter-spacing: 0.5px;
  margin-bottom: 4px;
}

.info-value {
  font-size: 1.1rem;
  font-weight: 600;
  color: var(--text-primary);
  font-family: 'SF Mono', Monaco, monospace;
}

.current-wifi {
  display: flex;
  align-items: center;
  gap: 12px;
  padding: 16px;
  background: var(--bg-tertiary);
  border-radius: 12px;
  margin-bottom: 24px;
  border-left: 3px solid var(--accent);
}

.wifi-label {
  color: var(--text-secondary);
  font-size: 0.85rem;
}

.wifi-value {
  flex: 1;
  color: var(--text-primary);
  font-weight: 500;
  font-family: 'SF Mono', Monaco, monospace;
}

.wifi-badge {
  font-size: 0.7rem;
  padding: 4px 10px;
  border-radius: 12px;
  font-weight: 600;
  text-transform: uppercase;
  letter-spacing: 0.5px;
  background: var(--accent-subtle);
  color: var(--accent);
}

.wifi-badge.custom {
  background: rgba(0, 204, 102, 0.15);
  color: var(--success);
}

.form-group {
  margin-bottom: 20px;
}

.form-group label {
  display: block;
  font-size: 0.85rem;
  color: var(--text-secondary);
  margin-bottom: 8px;
  font-weight: 500;
}

.input {
  width: 100%;
  padding: 14px 16px;
  background: var(--bg-tertiary);
  border: 1px solid var(--border);
  border-radius: 10px;
  color: var(--text-primary);
  font-size: 1rem;
  transition: all 0.2s;
}

.input:focus {
  outline: none;
  border-color: var(--accent);
  box-shadow: 0 0 0 3px var(--accent-glow);
}

.input::placeholder {
  color: var(--text-muted);
}

.password-input-wrapper {
  position: relative;
}

.password-input-wrapper .input {
  padding-right: 50px;
}

.password-toggle {
  position: absolute;
  right: 12px;
  top: 50%;
  transform: translateY(-50%);
  background: none;
  border: none;
  color: var(--text-muted);
  cursor: pointer;
  font-size: 1.1rem;
  padding: 4px;
}

.password-toggle:hover {
  color: var(--text-secondary);
}

.input-hint {
  display: block;
  font-size: 0.75rem;
  color: var(--text-muted);
  margin-top: 6px;
}

.button-group {
  display: flex;
  gap: 12px;
  margin-top: 24px;
}

.btn {
  flex: 1;
  padding: 14px 20px;
  border: none;
  border-radius: 10px;
  font-size: 0.95rem;
  font-weight: 600;
  cursor: pointer;
  transition: all 0.2s;
}

.btn-primary {
  background: var(--accent);
  color: var(--bg-primary);
}

.btn-primary:hover {
  background: var(--accent-hover);
  box-shadow: 0 0 20px var(--accent-glow);
}

.btn-secondary {
  background: var(--bg-tertiary);
  color: var(--text-primary);
  border: 1px solid var(--border);
}

.btn-secondary:hover {
  background: var(--bg-secondary);
  border-color: var(--accent);
}

.btn-danger {
  background: var(--danger);
  color: white;
  width: 100%;
}

.btn-danger:hover {
  background: #ff4d4d;
  box-shadow: 0 0 20px rgba(255, 51, 51, 0.3);
}

.btn-warning {
  background: #ff9800;
  color: white;
  width: 100%;
  margin-bottom: 10px;
}

.btn-warning:hover {
  background: #ffa726;
  box-shadow: 0 0 20px rgba(255, 152, 0, 0.3);
}

.warning-box {
  display: flex;
  align-items: center;
  gap: 12px;
  padding: 14px 16px;
  background: rgba(255, 170, 0, 0.1);
  border: 1px solid rgba(255, 170, 0, 0.3);
  border-radius: 10px;
  margin-top: 20px;
}

.warning-icon {
  font-size: 1.2rem;
}

.warning-text {
  color: var(--warning);
  font-size: 0.85rem;
  font-weight: 500;
}

.info-list {
  display: flex;
  flex-direction: column;
  gap: 12px;
}

.info-row {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 12px 16px;
  background: var(--bg-tertiary);
  border-radius: 10px;
}

.info-row .info-label {
  margin: 0;
  font-size: 0.85rem;
}

.info-row .info-value {
  font-size: 0.95rem;
}

.toast {
  position: fixed;
  bottom: 24px;
  left: 50%;
  transform: translateX(-50%) translateY(100px);
  background: var(--bg-tertiary);
  color: var(--text-primary);
  padding: 14px 24px;
  border-radius: 12px;
  box-shadow: 0 4px 24px rgba(0,0,0,0.4);
  border: 1px solid var(--border);
  opacity: 0;
  transition: all 0.3s ease;
  z-index: 1000;
  max-width: 90%;
}

.toast.show {
  transform: translateX(-50%) translateY(0);
  opacity: 1;
}

.toast.success { border-color: var(--success); border-left: 4px solid var(--success); }
.toast.warning { border-color: var(--warning); border-left: 4px solid var(--warning); }
.toast.error { border-color: var(--danger); border-left: 4px solid var(--danger); }
.toast.info { border-color: var(--accent); border-left: 4px solid var(--accent); }

footer {
  text-align: center;
  padding: 24px 0;
  color: var(--text-muted);
  font-size: 0.8rem;
}

@media (max-width: 400px) {
  .container { padding: 12px; }
  .header-content { flex-direction: column; gap: 12px; text-align: center; }
  .logo-section { justify-content: center; }
  .button-group { flex-direction: column; }
  .info-grid { grid-template-columns: 1fr; }
}
)rawliteral";

inline const char SCRIPT_JS[] = R"rawliteral(
let pollTimer = null;
let state = {};

function fetchState() {
  fetch('/api/state')
    .then(r => r.json())
    .then(data => {
      state = data;
      updateUI(data);
      updateConnectionStatus(true);
    })
    .catch(err => {
      console.error('Fetch error:', err);
      updateConnectionStatus(false);
    });
}

function updateConnectionStatus(connected) {
  const dot = document.getElementById('connection-dot');
  const text = document.getElementById('connection-text');
  if (connected) {
    dot.className = 'dot connected';
    text.textContent = 'Online';
  } else {
    dot.className = 'dot disconnected';
    text.textContent = 'Offline';
  }
}

function updateUI(data) {
  if (data.ssid) {
    document.getElementById('current-ssid').textContent = data.ssid;
    document.getElementById('welcome-ssid').textContent = data.ssid;
  }
  if (data.ip) {
    document.getElementById('info-ip').textContent = data.ip;
  }
  document.getElementById('info-clients').textContent = data.clients || 0;
  
  if (data.uptime !== undefined) {
    document.getElementById('device-uptime').textContent = formatUptime(data.uptime);
  }
  if (data.freeHeap !== undefined) {
    document.getElementById('device-heap').textContent = Math.round(data.freeHeap / 1024) + ' KB';
  }
}

function formatUptime(seconds) {
  const h = Math.floor(seconds / 3600);
  const m = Math.floor((seconds % 3600) / 60);
  const s = Math.floor(seconds % 60);
  return pad(h) + ':' + pad(m) + ':' + pad(s);
}

function pad(n) {
  return n.toString().padStart(2, '0');
}

function sendCommand(cmd, data) {
  data = data || {};
  data.cmd = cmd;
  fetch('/api/command', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify(data)
  })
  .then(r => r.json())
  .then(res => {
    if (res.success) {
      showToast('Command sent', 'success');
    }
  })
  .catch(err => {
    showToast('Error: ' + err, 'error');
  });
}

function showToast(message, type) {
  type = type || 'info';
  const toast = document.getElementById('toast');
  toast.textContent = message;
  toast.className = 'toast ' + type + ' show';
  setTimeout(function() {
    toast.className = 'toast';
  }, 3000);
}

// Tab switching
document.querySelectorAll('.tab').forEach(function(tab) {
  tab.addEventListener('click', function() {
    document.querySelectorAll('.tab').forEach(function(t) { t.classList.remove('active'); });
    tab.classList.add('active');
    
    var tabId = tab.dataset.tab;
    document.querySelectorAll('.tab-content').forEach(function(c) { c.classList.remove('active'); });
    document.getElementById(tabId).classList.add('active');
  });
});

// Password visibility toggle
document.getElementById('toggle-password').addEventListener('click', function() {
  var input = document.getElementById('custom-password');
  var btn = document.getElementById('toggle-password');
  if (input.type === 'password') {
    input.type = 'text';
    btn.innerHTML = '&#128584;';
  } else {
    input.type = 'password';
    btn.innerHTML = '&#128065;';
  }
});

// Save WiFi credentials
document.getElementById('save-wifi-btn').addEventListener('click', function() {
  var ssid = document.getElementById('custom-ssid').value.trim();
  var password = document.getElementById('custom-password').value;
  
  if (!ssid) {
    showToast('Please enter an SSID', 'error');
    return;
  }
  
  if (password.length < 8 || password.length > 12) {
    showToast('Password must be 8-12 characters', 'error');
    return;
  }
  
  sendCommand('setWifiCredentials', { ssid: ssid, password: password });
  document.getElementById('restart-warning').style.display = 'flex';
});

// Reset WiFi to auto
document.getElementById('reset-wifi-btn').addEventListener('click', function() {
  if (confirm('Reset to auto-generated WiFi credentials?')) {
    sendCommand('resetWifiToAuto');
    document.getElementById('custom-ssid').value = '';
    document.getElementById('custom-password').value = '';
    document.getElementById('restart-warning').style.display = 'flex';
  }
});

// Restart device
document.getElementById('restart-btn').addEventListener('click', function() {
  if (confirm('Are you sure you want to restart the device?')) {
    sendCommand('restart');
    showToast('Restarting device...', 'warning');
  }
});

// Kick all other clients
document.getElementById('kick-clients-btn').addEventListener('click', function() {
  if (confirm('Disconnect all other devices from this network?')) {
    sendCommand('kickOtherClients');
    showToast('Kicking other clients...', 'warning');
  }
});

// Initialize - poll every 2 seconds
fetchState();
pollTimer = setInterval(fetchState, 2000);
)rawliteral";

} // namespace Web
} // namespace SystemAPI
