/*****************************************************************
 * @file HttpServer.hpp
 * @brief HTTP Server for Captive Portal
 * 
 * Implements the HTTP server with URI routing and handlers.
 * 
 * @author ARCOS
 * @version 1.0
 *****************************************************************/

#pragma once

#include "SystemAPI/Web/WebTypes.hpp"
#include "SystemAPI/Web/Interfaces/ICommandHandler.hpp"
#include "SystemAPI/Web/Content/WebContent.hpp"
#include "SystemAPI/Misc/SyncState.hpp"
#include "SystemAPI/Security/SecurityDriver.hpp"

#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_random.h"
#include "cJSON.h"

// Socket includes for getpeername
#include "lwip/sockets.h"
#include "lwip/inet.h"

#include <cstring>
#include <vector>
#include <functional>

namespace SystemAPI {
namespace Web {

static const char* HTTP_TAG = "HttpServer";

/**
 * @brief HTTP Server for Web Portal
 * 
 * Handles all HTTP requests including API endpoints,
 * static content, and captive portal detection.
 */
class HttpServer {
public:
    using CommandCallback = std::function<void(CommandType, cJSON*)>;
    
    /**
     * @brief Get singleton instance
     */
    static HttpServer& instance() {
        static HttpServer inst;
        return inst;
    }
    
    /**
     * @brief Start the HTTP server
     * @return true on success
     */
    bool start() {
        if (server_) return true;
        
        httpd_config_t config = HTTPD_DEFAULT_CONFIG();
        config.max_uri_handlers = 30;
        config.stack_size = 8192;
        config.lru_purge_enable = true;
        config.uri_match_fn = httpd_uri_match_wildcard;
        
        if (httpd_start(&server_, &config) != ESP_OK) {
            ESP_LOGE(HTTP_TAG, "Failed to start HTTP server");
            return false;
        }
        
        registerHandlers();
        
        ESP_LOGI(HTTP_TAG, "HTTP server started on port %d", HTTP_PORT);
        return true;
    }
    
    /**
     * @brief Stop the HTTP server
     */
    void stop() {
        if (server_) {
            httpd_stop(server_);
            server_ = nullptr;
            ESP_LOGI(HTTP_TAG, "HTTP server stopped");
        }
    }
    
    /**
     * @brief Check if server is running
     */
    bool isRunning() const { return server_ != nullptr; }
    
    /**
     * @brief Set command callback
     */
    void setCommandCallback(CommandCallback callback) {
        command_callback_ = callback;
    }
    
    /**
     * @brief Get the httpd handle (for advanced use)
     */
    httpd_handle_t getHandle() const { return server_; }

private:
    HttpServer() = default;
    ~HttpServer() { stop(); }
    
    // Prevent copying
    HttpServer(const HttpServer&) = delete;
    HttpServer& operator=(const HttpServer&) = delete;
    
    /**
     * @brief Register all URI handlers
     */
    void registerHandlers() {
        // Login page (always accessible)
        registerHandler("/login", HTTP_GET, handleLoginPage);
        registerHandler("/api/login", HTTP_POST, handleApiLogin);
        registerHandler("/api/logout", HTTP_POST, handleApiLogout);
        
        // Page routes - each tab is a separate page
        registerHandler("/", HTTP_GET, handlePageBasic);
        registerHandler("/system", HTTP_GET, handlePageSystem);
        registerHandler("/advanced", HTTP_GET, handlePageAdvanced);
        registerHandler("/settings", HTTP_GET, handlePageSettings);
        
        // Static content handlers
        registerHandler("/style.css", HTTP_GET, handleCss);
        
        // API endpoints
        registerHandler("/api/state", HTTP_GET, handleApiState);
        registerHandler("/api/command", HTTP_POST, handleApiCommand);
        registerHandler("/api/scan", HTTP_GET, handleApiScan);
        
        // Captive portal detection endpoints
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
            registerHandler(path, HTTP_GET, handleRedirect);
        }
        
        // Wildcard catch-all (must be last) - handle all HTTP methods
        registerHandler("/*", HTTP_GET, handleCatchAll);
        registerHandler("/*", HTTP_POST, handleCatchAll);
        registerHandler("/*", HTTP_PUT, handleCatchAll);
        registerHandler("/*", HTTP_DELETE, handleCatchAll);
        registerHandler("/*", HTTP_HEAD, handleCatchAll);
    }
    
    /**
     * @brief Helper to register a handler
     */
    void registerHandler(const char* uri, httpd_method_t method, esp_err_t (*handler)(httpd_req_t*)) {
        httpd_uri_t uri_handler = {
            .uri = uri,
            .method = method,
            .handler = handler,
            .user_ctx = this
        };
        httpd_register_uri_handler(server_, &uri_handler);
    }
    
    // ========== Authentication Helpers ==========
    
    /**
     * @brief Check if request is coming from external network (not direct AP)
     * 
     * If the device is connected to an external network (APSTA mode),
     * requests can come either from:
     * - AP clients (192.168.4.x) - these are direct device connections
     * - STA network (the external network IP range) - these need auth
     */
    static bool isExternalNetworkRequest(httpd_req_t* req) {
        auto& state = SYNC_STATE.state();
        
        // If external WiFi is not connected, all requests are from AP
        if (!state.extWifiIsConnected) {
            return false;
        }
        
        // Get client IP from the request
        int sockfd = httpd_req_to_sockfd(req);
        struct sockaddr_in6 addr;
        socklen_t addr_len = sizeof(addr);
        
        if (getpeername(sockfd, (struct sockaddr*)&addr, &addr_len) != 0) {
            return false;  // Can't determine, assume safe
        }
        
        // Convert to IPv4 if mapped
        uint32_t client_ip = 0;
        if (addr.sin6_family == AF_INET) {
            client_ip = ((struct sockaddr_in*)&addr)->sin_addr.s_addr;
        } else if (addr.sin6_family == AF_INET6) {
            // Check for IPv4-mapped address
            if (IN6_IS_ADDR_V4MAPPED(&addr.sin6_addr)) {
                memcpy(&client_ip, &addr.sin6_addr.s6_addr[12], 4);
            }
        }
        
        // AP subnet is 192.168.4.0/24 = 0xC0A80400
        // Mask: 255.255.255.0 = 0xFFFFFF00
        uint32_t ap_network = 0x0404A8C0;  // 192.168.4.0 in little-endian
        uint32_t ap_mask = 0x00FFFFFF;     // 255.255.255.0 in little-endian
        
        // If client is on AP subnet, it's a direct connection
        if ((client_ip & ap_mask) == (ap_network & ap_mask)) {
            ESP_LOGD(HTTP_TAG, "Request from AP client (direct connection)");
            return false;
        }
        
        // Otherwise, it's from the external network
        ESP_LOGI(HTTP_TAG, "Request from external network client");
        return true;
    }
    
    /**
     * @brief Check if request has valid authentication token (cookie)
     */
    static bool isAuthenticated(httpd_req_t* req) {
        auto& state = SYNC_STATE.state();
        
        // If auth not enabled, everyone is authenticated
        if (!state.authEnabled || strlen(state.authPassword) == 0) {
            return true;
        }
        
        // Get cookie header
        char cookie[128] = {0};
        if (httpd_req_get_hdr_value_str(req, "Cookie", cookie, sizeof(cookie)) != ESP_OK) {
            return false;
        }
        
        // Look for auth token in cookie
        // Format: auth_token=<token>
        char* token_start = strstr(cookie, "auth_token=");
        if (!token_start) {
            return false;
        }
        
        token_start += 11;  // Skip "auth_token="
        char* token_end = strchr(token_start, ';');
        
        char token[65] = {0};
        size_t token_len = token_end ? (token_end - token_start) : strlen(token_start);
        if (token_len >= sizeof(token)) token_len = sizeof(token) - 1;
        strncpy(token, token_start, token_len);
        
        // Compare with stored session token
        return strcmp(token, state.authSessionToken) == 0 && strlen(token) > 0;
    }
    
    /**
     * @brief Check if auth is required and redirect to login if needed
     * @return true if request should be blocked (redirected to login)
     */
    static bool requiresAuthRedirect(httpd_req_t* req) {
        auto& state = SYNC_STATE.state();
        
        // Auth only required when:
        // 1. External WiFi is connected
        // 2. Auth is enabled
        // 3. Request is from external network
        // 4. User is not authenticated
        
        if (!state.extWifiIsConnected) return false;
        if (!state.authEnabled) return false;
        if (strlen(state.authPassword) == 0) return false;
        if (!isExternalNetworkRequest(req)) return false;
        if (isAuthenticated(req)) return false;
        
        return true;
    }
    
    /**
     * @brief Redirect to login page
     */
    static esp_err_t redirectToLogin(httpd_req_t* req) {
        httpd_resp_set_status(req, "302 Found");
        httpd_resp_set_hdr(req, "Location", "/login");
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    }
    
    // ========== Login Page ==========
    
    static const char* getLoginPage() {
        return R"rawliteral(<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Login - Lucidius</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body { background: #0a0a0a; color: #fff; font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; min-height: 100vh; display: flex; align-items: center; justify-content: center; }
    .login-container { background: #141414; border-radius: 16px; padding: 40px; width: 100%; max-width: 400px; margin: 20px; border: 1px solid #222; }
    h1 { text-align: center; margin-bottom: 8px; color: #ff6b00; }
    .subtitle { text-align: center; color: #888; margin-bottom: 32px; font-size: 14px; }
    .warning { background: rgba(255, 59, 48, 0.1); border: 1px solid rgba(255, 59, 48, 0.3); border-radius: 8px; padding: 12px 16px; margin-bottom: 24px; color: #ff6b6b; font-size: 13px; text-align: center; }
    .form-group { margin-bottom: 20px; }
    label { display: block; color: #888; font-size: 13px; margin-bottom: 8px; }
    input { width: 100%; padding: 14px 16px; background: #1a1a1a; border: 1px solid #333; border-radius: 8px; color: #fff; font-size: 16px; transition: border-color 0.2s; }
    input:focus { outline: none; border-color: #ff6b00; }
    .btn { width: 100%; padding: 14px; background: linear-gradient(135deg, #ff6b00, #ff8533); color: #fff; border: none; border-radius: 8px; font-size: 16px; font-weight: 600; cursor: pointer; transition: transform 0.2s, box-shadow 0.2s; }
    .btn:hover { transform: translateY(-2px); box-shadow: 0 4px 20px rgba(255, 107, 0, 0.3); }
    .btn:active { transform: translateY(0); }
    .error { color: #ff6b6b; font-size: 13px; margin-top: 16px; text-align: center; display: none; }
    .error.show { display: block; }
  </style>
</head>
<body>
  <div class="login-container">
    <h1>Lucidius</h1>
    <p class="subtitle">External Network Access</p>
    <div class="warning">
      You are connecting via an external network.<br>
      Authentication is required for security.
    </div>
    <form id="login-form">
      <div class="form-group">
        <label for="username">Username</label>
        <input type="text" id="username" name="username" autocomplete="username" required>
      </div>
      <div class="form-group">
        <label for="password">Password</label>
        <input type="password" id="password" name="password" autocomplete="current-password" required>
      </div>
      <button type="submit" class="btn">Log In</button>
      <p class="error" id="error-msg">Invalid username or password</p>
    </form>
  </div>
  <script>
    document.getElementById('login-form').addEventListener('submit', function(e) {
      e.preventDefault();
      var username = document.getElementById('username').value;
      var password = document.getElementById('password').value;
      
      fetch('/api/login', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ username: username, password: password })
      })
      .then(r => r.json())
      .then(data => {
        if (data.success) {
          window.location.href = '/';
        } else {
          document.getElementById('error-msg').classList.add('show');
        }
      })
      .catch(err => {
        document.getElementById('error-msg').textContent = 'Connection error';
        document.getElementById('error-msg').classList.add('show');
      });
    });
  </script>
</body>
</html>)rawliteral";
    }
    
    static esp_err_t handleLoginPage(httpd_req_t* req) {
        // If already authenticated or not from external network, redirect to home
        auto& state = SYNC_STATE.state();
        if (!state.extWifiIsConnected || !state.authEnabled || 
            !isExternalNetworkRequest(req) || isAuthenticated(req)) {
            httpd_resp_set_status(req, "302 Found");
            httpd_resp_set_hdr(req, "Location", "/");
            httpd_resp_send(req, NULL, 0);
            return ESP_OK;
        }
        
        httpd_resp_set_type(req, "text/html");
        httpd_resp_send(req, getLoginPage(), HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    static esp_err_t handleApiLogin(httpd_req_t* req) {
        char buf[256];
        int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
        if (ret <= 0) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No body");
            return ESP_FAIL;
        }
        buf[ret] = '\0';
        
        cJSON* root = cJSON_Parse(buf);
        if (!root) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
            return ESP_FAIL;
        }
        
        cJSON* username = cJSON_GetObjectItem(root, "username");
        cJSON* password = cJSON_GetObjectItem(root, "password");
        
        auto& state = SYNC_STATE.state();
        bool success = false;
        
        if (username && password && username->valuestring && password->valuestring) {
            // Check credentials
            if (strcmp(username->valuestring, state.authUsername) == 0 &&
                strcmp(password->valuestring, state.authPassword) == 0) {
                
                // Generate session token
                uint32_t r1 = esp_random();
                uint32_t r2 = esp_random();
                uint32_t r3 = esp_random();
                uint32_t r4 = esp_random();
                snprintf(state.authSessionToken, sizeof(state.authSessionToken),
                        "%08lx%08lx%08lx%08lx", 
                        (unsigned long)r1, (unsigned long)r2, 
                        (unsigned long)r3, (unsigned long)r4);
                
                success = true;
                ESP_LOGI(HTTP_TAG, "Login successful for user: %s", state.authUsername);
            } else {
                ESP_LOGW(HTTP_TAG, "Login failed for user: %s", username->valuestring);
            }
        }
        
        cJSON_Delete(root);
        
        httpd_resp_set_type(req, "application/json");
        
        if (success) {
            // Set auth cookie
            char cookie[128];
            snprintf(cookie, sizeof(cookie), "auth_token=%s; Path=/; HttpOnly; SameSite=Strict",
                    state.authSessionToken);
            httpd_resp_set_hdr(req, "Set-Cookie", cookie);
            httpd_resp_send(req, "{\"success\":true}", HTTPD_RESP_USE_STRLEN);
        } else {
            httpd_resp_send(req, "{\"success\":false,\"error\":\"Invalid credentials\"}", HTTPD_RESP_USE_STRLEN);
        }
        
        return ESP_OK;
    }
    
    static esp_err_t handleApiLogout(httpd_req_t* req) {
        auto& state = SYNC_STATE.state();
        
        // Clear session token
        memset(state.authSessionToken, 0, sizeof(state.authSessionToken));
        
        // Clear cookie
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_hdr(req, "Set-Cookie", "auth_token=; Path=/; Expires=Thu, 01 Jan 1970 00:00:00 GMT");
        httpd_resp_send(req, "{\"success\":true}", HTTPD_RESP_USE_STRLEN);
        
        ESP_LOGI(HTTP_TAG, "User logged out");
        return ESP_OK;
    }
    
    // ========== Page Handlers ==========
    
    static esp_err_t handlePageBasic(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return redirectToLogin(req);
        
        ESP_LOGI(HTTP_TAG, "Serving Basic page");
        httpd_resp_set_type(req, "text/html");
        httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
        httpd_resp_send(req, Content::PAGE_BASIC, HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    static esp_err_t handlePageSystem(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return redirectToLogin(req);
        
        ESP_LOGI(HTTP_TAG, "Serving System page");
        httpd_resp_set_type(req, "text/html");
        httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
        httpd_resp_send(req, Content::PAGE_SYSTEM, HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    static esp_err_t handlePageAdvanced(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return redirectToLogin(req);
        
        ESP_LOGI(HTTP_TAG, "Serving Advanced page");
        httpd_resp_set_type(req, "text/html");
        httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
        httpd_resp_send(req, Content::PAGE_ADVANCED, HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    static esp_err_t handlePageSettings(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return redirectToLogin(req);
        
        ESP_LOGI(HTTP_TAG, "Serving Settings page");
        httpd_resp_set_type(req, "text/html");
        httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
        httpd_resp_send(req, Content::PAGE_SETTINGS, HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    // ========== Static Content Handlers ==========
    
    static esp_err_t handleCss(httpd_req_t* req) {
        httpd_resp_set_type(req, "text/css");
        httpd_resp_send(req, Content::STYLE_CSS, HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    // ========== API Handlers ==========
    
    /**
     * @brief Return 401 Unauthorized for API requests
     */
    static esp_err_t sendUnauthorized(httpd_req_t* req) {
        httpd_resp_set_status(req, "401 Unauthorized");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"error\":\"Unauthorized\",\"login_required\":true}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    static esp_err_t handleApiState(httpd_req_t* req) {
        // Check auth for external network requests
        if (requiresAuthRedirect(req)) return sendUnauthorized(req);
        
        auto& state = SYNC_STATE.state();
        
        cJSON* root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "ssid", state.ssid);
        cJSON_AddStringToObject(root, "ip", state.ipAddress);
        cJSON_AddNumberToObject(root, "clients", state.wifiClients);
        cJSON_AddNumberToObject(root, "uptime", state.uptime);
        cJSON_AddNumberToObject(root, "freeHeap", state.freeHeap);
        cJSON_AddNumberToObject(root, "brightness", state.brightness);
        cJSON_AddNumberToObject(root, "cpuUsage", state.cpuUsage);
        cJSON_AddNumberToObject(root, "fps", state.fps);
        
        // Sensor data
        cJSON* sensors = cJSON_CreateObject();
        cJSON_AddNumberToObject(sensors, "temperature", state.temperature);
        cJSON_AddNumberToObject(sensors, "humidity", state.humidity);
        cJSON_AddNumberToObject(sensors, "pressure", state.pressure);
        cJSON_AddItemToObject(root, "sensors", sensors);
        
        // IMU data
        cJSON* imu = cJSON_CreateObject();
        cJSON_AddNumberToObject(imu, "accelX", state.accelX);
        cJSON_AddNumberToObject(imu, "accelY", state.accelY);
        cJSON_AddNumberToObject(imu, "accelZ", state.accelZ);
        cJSON_AddNumberToObject(imu, "gyroX", state.gyroX);
        cJSON_AddNumberToObject(imu, "gyroY", state.gyroY);
        cJSON_AddNumberToObject(imu, "gyroZ", state.gyroZ);
        cJSON_AddItemToObject(root, "imu", imu);
        
        // GPS data
        cJSON* gps = cJSON_CreateObject();
        cJSON_AddNumberToObject(gps, "latitude", state.latitude);
        cJSON_AddNumberToObject(gps, "longitude", state.longitude);
        cJSON_AddNumberToObject(gps, "altitude", state.altitude);
        cJSON_AddNumberToObject(gps, "satellites", state.satellites);
        cJSON_AddBoolToObject(gps, "valid", state.gpsValid);
        cJSON_AddNumberToObject(gps, "speed", state.gpsSpeed);
        cJSON_AddNumberToObject(gps, "heading", state.gpsHeading);
        cJSON_AddNumberToObject(gps, "hdop", state.gpsHdop);
        
        // GPS Time as formatted string
        char timeStr[20];
        snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d", 
                 state.gpsHour, state.gpsMinute, state.gpsSecond);
        cJSON_AddStringToObject(gps, "time", timeStr);
        
        char dateStr[16];
        snprintf(dateStr, sizeof(dateStr), "%04d-%02d-%02d", 
                 state.gpsYear, state.gpsMonth, state.gpsDay);
        cJSON_AddStringToObject(gps, "date", dateStr);
        cJSON_AddItemToObject(root, "gps", gps);
        
        // Connection status
        cJSON_AddBoolToObject(root, "gpuConnected", state.gpuConnected);
        
        // Microphone
        cJSON_AddNumberToObject(root, "mic", state.micLevel);
        cJSON_AddBoolToObject(root, "micConnected", state.micConnected);
        cJSON_AddNumberToObject(root, "micDb", state.micDb);
        
        // System mode
        const char* modeStr = "idle";
        switch (state.mode) {
            case SystemMode::RUNNING: modeStr = "running"; break;
            case SystemMode::PAUSED: modeStr = "paused"; break;
            case SystemMode::ERROR: modeStr = "error"; break;
            default: modeStr = "idle"; break;
        }
        cJSON_AddStringToObject(root, "mode", modeStr);
        cJSON_AddStringToObject(root, "statusText", state.statusText);
        
        // External WiFi state
        cJSON_AddBoolToObject(root, "extWifiEnabled", state.extWifiEnabled);
        cJSON_AddBoolToObject(root, "extWifiConnected", state.extWifiConnected);
        cJSON_AddBoolToObject(root, "extWifiIsConnected", state.extWifiIsConnected);
        cJSON_AddStringToObject(root, "extWifiSSID", state.extWifiSSID);
        cJSON_AddStringToObject(root, "extWifiIP", state.extWifiIP);
        cJSON_AddNumberToObject(root, "extWifiRSSI", state.extWifiRSSI);
        
        // Authentication state (don't send password!)
        cJSON_AddBoolToObject(root, "authEnabled", state.authEnabled);
        cJSON_AddStringToObject(root, "authUsername", state.authUsername);
        
        char* json = cJSON_PrintUnformatted(root);
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, json, strlen(json));
        
        free(json);
        cJSON_Delete(root);
        
        return ESP_OK;
    }
    
    static esp_err_t handleApiCommand(httpd_req_t* req) {
        // Check auth for external network requests
        if (requiresAuthRedirect(req)) return sendUnauthorized(req);
        
        HttpServer* self = static_cast<HttpServer*>(req->user_ctx);
        
        // Read body
        char buf[HTTP_BUFFER_SIZE];
        int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
        if (ret <= 0) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No body");
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
            CommandType type = stringToCommand(cmd->valuestring);
            self->processCommand(type, root);
        }
        
        cJSON_Delete(root);
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"success\":true}", HTTPD_RESP_USE_STRLEN);
        
        return ESP_OK;
    }
    
    /**
     * @brief Handle WiFi network scanning
     */
    static esp_err_t handleApiScan(httpd_req_t* req) {
        // Check auth for external network requests
        if (requiresAuthRedirect(req)) return sendUnauthorized(req);
        
        ESP_LOGI(HTTP_TAG, "Starting WiFi scan...");
        
        // Get current WiFi mode
        wifi_mode_t mode;
        esp_wifi_get_mode(&mode);
        
        // Ensure STA interface exists for scanning
        bool wasAPOnly = (mode == WIFI_MODE_AP);
        if (wasAPOnly) {
            ESP_LOGI(HTTP_TAG, "Switching to APSTA mode for scan");
            
            // Create STA netif if not exists
            esp_netif_t* sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
            if (!sta_netif) {
                sta_netif = esp_netif_create_default_wifi_sta();
                ESP_LOGI(HTTP_TAG, "Created STA netif for scanning");
            }
            
            esp_wifi_set_mode(WIFI_MODE_APSTA);
            vTaskDelay(pdMS_TO_TICKS(200)); // Longer delay for mode switch
        }
        
        // Configure scan - use passive scan which works better in APSTA mode
        wifi_scan_config_t scan_config = {
            .ssid = nullptr,
            .bssid = nullptr,
            .channel = 0,
            .show_hidden = false,
            .scan_type = WIFI_SCAN_TYPE_PASSIVE,
            .scan_time = {
                .passive = 200
            }
        };
        
        // Start scan (blocking)
        esp_err_t err = esp_wifi_scan_start(&scan_config, true);
        if (err != ESP_OK) {
            ESP_LOGE(HTTP_TAG, "WiFi scan failed: %s", esp_err_to_name(err));
            // Restore AP-only mode if we switched
            if (wasAPOnly) {
                esp_wifi_set_mode(WIFI_MODE_AP);
            }
            httpd_resp_set_type(req, "application/json");
            char errJson[100];
            snprintf(errJson, sizeof(errJson), "{\"networks\":[], \"error\":\"Scan failed: %s\"}", esp_err_to_name(err));
            httpd_resp_send(req, errJson, HTTPD_RESP_USE_STRLEN);
            return ESP_OK;
        }
        
        // Get scan results
        uint16_t ap_count = 0;
        esp_wifi_scan_get_ap_num(&ap_count);
        
        if (ap_count == 0) {
            httpd_resp_set_type(req, "application/json");
            httpd_resp_send(req, "{\"networks\":[]}", HTTPD_RESP_USE_STRLEN);
            return ESP_OK;
        }
        
        // Limit to reasonable number
        if (ap_count > 20) ap_count = 20;
        
        wifi_ap_record_t* ap_records = (wifi_ap_record_t*)malloc(sizeof(wifi_ap_record_t) * ap_count);
        if (!ap_records) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Memory allocation failed");
            return ESP_FAIL;
        }
        
        esp_wifi_scan_get_ap_records(&ap_count, ap_records);
        
        // Build JSON response
        cJSON* root = cJSON_CreateObject();
        cJSON* networks = cJSON_CreateArray();
        
        for (int i = 0; i < ap_count; i++) {
            // Skip networks with empty SSID
            if (strlen((char*)ap_records[i].ssid) == 0) continue;
            
            cJSON* net = cJSON_CreateObject();
            cJSON_AddStringToObject(net, "ssid", (char*)ap_records[i].ssid);
            cJSON_AddNumberToObject(net, "rssi", ap_records[i].rssi);
            cJSON_AddNumberToObject(net, "channel", ap_records[i].primary);
            cJSON_AddBoolToObject(net, "secure", ap_records[i].authmode != WIFI_AUTH_OPEN);
            
            // Auth mode string
            const char* authStr = "Unknown";
            switch (ap_records[i].authmode) {
                case WIFI_AUTH_OPEN: authStr = "Open"; break;
                case WIFI_AUTH_WEP: authStr = "WEP"; break;
                case WIFI_AUTH_WPA_PSK: authStr = "WPA"; break;
                case WIFI_AUTH_WPA2_PSK: authStr = "WPA2"; break;
                case WIFI_AUTH_WPA_WPA2_PSK: authStr = "WPA/WPA2"; break;
                case WIFI_AUTH_WPA3_PSK: authStr = "WPA3"; break;
                case WIFI_AUTH_WPA2_WPA3_PSK: authStr = "WPA2/WPA3"; break;
                default: authStr = "Enterprise"; break;
            }
            cJSON_AddStringToObject(net, "auth", authStr);
            
            cJSON_AddItemToArray(networks, net);
        }
        
        cJSON_AddItemToObject(root, "networks", networks);
        
        free(ap_records);
        
        char* json = cJSON_PrintUnformatted(root);
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, json, strlen(json));
        
        free(json);
        cJSON_Delete(root);
        
        ESP_LOGI(HTTP_TAG, "WiFi scan complete, found %d networks", ap_count);
        return ESP_OK;
    }
    
    // ========== Captive Portal Handlers ==========
    
    static esp_err_t handleRedirect(httpd_req_t* req) {
        httpd_resp_set_status(req, "302 Found");
        httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
        httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
        httpd_resp_send(req, nullptr, 0);
        return ESP_OK;
    }
    
    static esp_err_t handleCatchAll(httpd_req_t* req) {
        char host_header[MAX_HOST_HEADER_LENGTH] = {0};
        httpd_req_get_hdr_value_str(req, "Host", host_header, sizeof(host_header));
        ESP_LOGI(HTTP_TAG, "Catch-all request: Host=%s URI=%s Method=%d", host_header, req->uri, req->method);
        
        // For non-GET requests, respond with redirect
        if (req->method != HTTP_GET) {
            httpd_resp_set_status(req, "302 Found");
            httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
            httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
            httpd_resp_send(req, nullptr, 0);
            return ESP_OK;
        }
        
        // Serve Basic page for any unmatched GET requests
        httpd_resp_set_type(req, "text/html");
        httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
        httpd_resp_set_hdr(req, "Pragma", "no-cache");
        httpd_resp_set_hdr(req, "Expires", "0");
        httpd_resp_send(req, Content::PAGE_BASIC, HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    // ========== Command Processing ==========
    
    void processCommand(CommandType type, cJSON* params) {
        // Invoke callback if set
        if (command_callback_) {
            command_callback_(type, params);
        }
        
        // Default handling
        switch (type) {
            case CommandType::SET_BRIGHTNESS: {
                cJSON* val = cJSON_GetObjectItem(params, "value");
                if (val) SYNC_STATE.setBrightness(val->valueint);
                break;
            }
            
            case CommandType::SET_WIFI_CREDENTIALS: {
                cJSON* ssid = cJSON_GetObjectItem(params, "ssid");
                cJSON* password = cJSON_GetObjectItem(params, "password");
                if (ssid && password && ssid->valuestring && password->valuestring) {
                    ESP_LOGI(HTTP_TAG, "WiFi credentials update: %s", ssid->valuestring);
                    
                    auto& security = arcos::security::SecurityDriver::instance();
                    if (security.setCustomCredentials(ssid->valuestring, password->valuestring)) {
                        ESP_LOGI(HTTP_TAG, "Custom credentials saved successfully");
                        vTaskDelay(pdMS_TO_TICKS(1000));
                        esp_restart();
                    } else {
                        ESP_LOGE(HTTP_TAG, "Failed to save credentials");
                    }
                }
                break;
            }
            
            case CommandType::RESET_WIFI_TO_AUTO: {
                ESP_LOGI(HTTP_TAG, "WiFi reset to auto requested");
                auto& security = arcos::security::SecurityDriver::instance();
                if (security.resetToAuto()) {
                    ESP_LOGI(HTTP_TAG, "Reset to auto credentials successful");
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    esp_restart();
                }
                break;
            }
            
            case CommandType::RESTART:
                ESP_LOGI(HTTP_TAG, "Restart requested");
                vTaskDelay(pdMS_TO_TICKS(500));
                esp_restart();
                break;
                
            case CommandType::KICK_CLIENTS: {
                ESP_LOGI(HTTP_TAG, "Kick clients requested");
                wifi_sta_list_t sta_list;
                esp_wifi_ap_get_sta_list(&sta_list);
                
                ESP_LOGI(HTTP_TAG, "Found %d connected clients", sta_list.num);
                
                int kicked = 0;
                for (int i = 0; i < sta_list.num; i++) {
                    uint16_t aid = i + 1;
                    if (esp_wifi_deauth_sta(aid) == ESP_OK) {
                        kicked++;
                        ESP_LOGI(HTTP_TAG, "Kicked client AID=%d", aid);
                    }
                }
                ESP_LOGI(HTTP_TAG, "Kicked %d clients total", kicked);
                break;
            }
            
            case CommandType::SET_EXT_WIFI: {
                cJSON* enabled = cJSON_GetObjectItem(params, "enabled");
                cJSON* ssid = cJSON_GetObjectItem(params, "ssid");
                cJSON* password = cJSON_GetObjectItem(params, "password");
                
                auto& state = SYNC_STATE.state();
                
                if (enabled) {
                    state.extWifiEnabled = cJSON_IsTrue(enabled);
                }
                if (ssid && ssid->valuestring) {
                    strncpy(state.extWifiSSID, ssid->valuestring, sizeof(state.extWifiSSID) - 1);
                    state.extWifiSSID[sizeof(state.extWifiSSID) - 1] = '\0';
                }
                if (password && password->valuestring) {
                    strncpy(state.extWifiPassword, password->valuestring, sizeof(state.extWifiPassword) - 1);
                    state.extWifiPassword[sizeof(state.extWifiPassword) - 1] = '\0';
                }
                
                ESP_LOGI(HTTP_TAG, "External WiFi config: enabled=%d, ssid=%s", 
                         state.extWifiEnabled, state.extWifiSSID);
                
                // Save to NVS for persistence
                auto& security = arcos::security::SecurityDriver::instance();
                security.saveExtWifiSettings(state.extWifiEnabled, state.extWifiConnected, state.extWifiSSID, 
                                            state.extWifiPassword, state.authEnabled,
                                            state.authUsername, state.authPassword);
                break;
            }
            
            case CommandType::EXT_WIFI_CONNECT: {
                cJSON* connect = cJSON_GetObjectItem(params, "connect");
                auto& state = SYNC_STATE.state();
                
                if (connect) {
                    bool shouldConnect = cJSON_IsTrue(connect);
                    state.extWifiConnected = shouldConnect;
                    
                    ESP_LOGI(HTTP_TAG, "External WiFi connect: %s", shouldConnect ? "true" : "false");
                    
                    // Save connect state to NVS for persistence across boots
                    auto& security = arcos::security::SecurityDriver::instance();
                    security.saveExtWifiSettings(state.extWifiEnabled, state.extWifiConnected, state.extWifiSSID, 
                                                state.extWifiPassword, state.authEnabled,
                                                state.authUsername, state.authPassword);
                    
                    if (shouldConnect && state.extWifiEnabled && strlen(state.extWifiSSID) > 0) {
                        // Ensure STA netif exists
                        esp_netif_t* sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
                        if (!sta_netif) {
                            sta_netif = esp_netif_create_default_wifi_sta();
                            ESP_LOGI(HTTP_TAG, "Created STA netif for connection");
                        }
                        
                        // Initiate station connection
                        wifi_config_t sta_config = {};
                        strncpy((char*)sta_config.sta.ssid, state.extWifiSSID, sizeof(sta_config.sta.ssid));
                        strncpy((char*)sta_config.sta.password, state.extWifiPassword, sizeof(sta_config.sta.password));
                        sta_config.sta.threshold.authmode = WIFI_AUTH_WPA_PSK;  // More permissive
                        sta_config.sta.pmf_cfg.capable = true;
                        sta_config.sta.pmf_cfg.required = false;
                        
                        // Switch to AP+STA mode
                        esp_wifi_set_mode(WIFI_MODE_APSTA);
                        esp_wifi_set_config(WIFI_IF_STA, &sta_config);
                        esp_wifi_connect();
                        
                        ESP_LOGI(HTTP_TAG, "Connecting to external network: %s", state.extWifiSSID);
                    } else if (!shouldConnect) {
                        // Disconnect and switch back to AP-only
                        esp_wifi_disconnect();
                        esp_wifi_set_mode(WIFI_MODE_AP);
                        state.extWifiIsConnected = false;
                        memset(state.extWifiIP, 0, sizeof(state.extWifiIP));
                        state.extWifiRSSI = -100;
                        
                        ESP_LOGI(HTTP_TAG, "Disconnected from external network");
                    }
                }
                break;
            }
            
            case CommandType::SET_AUTH: {
                cJSON* enabled = cJSON_GetObjectItem(params, "enabled");
                cJSON* username = cJSON_GetObjectItem(params, "username");
                cJSON* password = cJSON_GetObjectItem(params, "password");
                
                auto& state = SYNC_STATE.state();
                
                if (enabled) {
                    state.authEnabled = cJSON_IsTrue(enabled);
                }
                if (username && username->valuestring) {
                    strncpy(state.authUsername, username->valuestring, sizeof(state.authUsername) - 1);
                    state.authUsername[sizeof(state.authUsername) - 1] = '\0';
                }
                if (password && password->valuestring && strlen(password->valuestring) > 0) {
                    // In production, this should be hashed!
                    strncpy(state.authPassword, password->valuestring, sizeof(state.authPassword) - 1);
                    state.authPassword[sizeof(state.authPassword) - 1] = '\0';
                }
                
                ESP_LOGI(HTTP_TAG, "Auth config: enabled=%d, username=%s", 
                         state.authEnabled, state.authUsername);
                
                // Save to NVS for persistence
                auto& security = arcos::security::SecurityDriver::instance();
                security.saveExtWifiSettings(state.extWifiEnabled, state.extWifiConnected, state.extWifiSSID, 
                                            state.extWifiPassword, state.authEnabled,
                                            state.authUsername, state.authPassword);
                break;
            }
            
            default:
                ESP_LOGW(HTTP_TAG, "Unknown command type");
                break;
        }
    }
    
    // State
    httpd_handle_t server_ = nullptr;
    CommandCallback command_callback_ = nullptr;
};

// Convenience macro
#define HTTP_SERVER HttpServer::instance()

} // namespace Web
} // namespace SystemAPI
