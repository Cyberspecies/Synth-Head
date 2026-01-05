/*****************************************************************
 * File:      NetworkService.hpp
 * Category:  include/FrameworkAPI
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Manages WiFi connectivity, captive portal, mDNS, and web-based
 *    configuration. Provides a clean API for network operations.
 * 
 * Usage:
 *    NetworkService network;
 *    network.init();
 *    
 *    // Start captive portal for initial setup
 *    network.startCaptivePortal("SynthHead-Setup", "");
 *    
 *    // Or connect to saved network
 *    network.connect("MyNetwork", "password123");
 *    
 *    // Add web routes for config
 *    network.addRoute("/config", HTTP_GET, [](Request& req) {
 *      return "<html>Config page</html>";
 *    });
 *    
 *    // Register callbacks
 *    network.onStateChange([](WiFiState state) {
 *      printf("WiFi state: %d\n", state);
 *    });
 *****************************************************************/

#ifndef ARCOS_INCLUDE_FRAMEWORKAPI_NETWORK_SERVICE_HPP_
#define ARCOS_INCLUDE_FRAMEWORKAPI_NETWORK_SERVICE_HPP_

#include "FrameworkTypes.hpp"
#include <cstring>
#include <functional>

namespace arcos::framework {

/**
 * HTTP Method types
 */
enum class HttpMethod : uint8_t {
  GET,
  POST,
  PUT,
  DELETE,
  OPTIONS
};

/**
 * Simple HTTP request representation
 */
struct HttpRequest {
  HttpMethod method = HttpMethod::GET;
  const char* uri = nullptr;
  const char* body = nullptr;
  size_t body_length = 0;
  
  // Query parameter helpers (simplified)
  const char* getParam(const char* name) const {
    (void)name;
    return nullptr; // HAL would implement actual parsing
  }
};

/**
 * Simple HTTP response builder
 */
struct HttpResponse {
  int status_code = 200;
  const char* content_type = "text/html";
  char body[2048];
  size_t body_length = 0;
  
  HttpResponse& status(int code) {
    status_code = code;
    return *this;
  }
  
  HttpResponse& type(const char* ct) {
    content_type = ct;
    return *this;
  }
  
  HttpResponse& send(const char* content) {
    body_length = strlen(content);
    if (body_length > sizeof(body) - 1) {
      body_length = sizeof(body) - 1;
    }
    memcpy(body, content, body_length);
    body[body_length] = '\0';
    return *this;
  }
  
  HttpResponse& json(const char* content) {
    content_type = "application/json";
    return send(content);
  }
};

/**
 * Route handler type
 */
using RouteHandler = std::function<void(const HttpRequest&, HttpResponse&)>;

/**
 * Route entry
 */
struct Route {
  char path[64];
  HttpMethod method;
  RouteHandler handler;
  bool active;
};

/**
 * Network info structure
 */
struct NetworkInfo {
  char ssid[33];
  char ip[16];
  char gateway[16];
  char subnet[16];
  int8_t rssi;
  uint8_t channel;
  bool connected;
  bool has_ip;
};

/**
 * NetworkService - WiFi and web server management
 * 
 * Provides:
 * - Station mode (connect to existing network)
 * - AP mode (create access point)
 * - Captive portal (redirect all to config page)
 * - Simple HTTP server for configuration
 * - mDNS for easy discovery
 */
class NetworkService {
public:
  static constexpr size_t MAX_ROUTES = 16;
  static constexpr size_t MAX_SAVED_NETWORKS = 5;
  
  NetworkService() = default;
  
  /**
   * Initialize network service
   * @param hostname Device hostname for mDNS
   */
  Result init(const char* hostname = "synthhead") {
    strncpy(hostname_, hostname, sizeof(hostname_) - 1);
    hostname_[sizeof(hostname_) - 1] = '\0';
    
    route_count_ = 0;
    memset(routes_, 0, sizeof(routes_));
    memset(&info_, 0, sizeof(info_));
    
    state_ = WiFiState::DISCONNECTED;
    initialized_ = true;
    
    // Add default routes
    addDefaultRoutes();
    
    return Result::OK;
  }
  
  /**
   * Connect to a WiFi network
   * @param ssid Network name
   * @param password Network password (empty for open)
   * @param timeout_ms Connection timeout
   */
  Result connect(const char* ssid, const char* password = "", 
                 uint32_t timeout_ms = 10000) {
    if (!initialized_) return Result::NOT_INITIALIZED;
    
    strncpy(pending_ssid_, ssid, sizeof(pending_ssid_) - 1);
    strncpy(pending_password_, password, sizeof(pending_password_) - 1);
    connect_timeout_ = timeout_ms;
    connect_start_time_ = 0;  // Will be set on first update
    
    setState(WiFiState::CONNECTING);
    
    // Note: Actual connection handled by HAL layer
    // This just initiates the request
    
    return Result::OK;
  }
  
  /**
   * Disconnect from current network
   */
  Result disconnect() {
    if (!initialized_) return Result::NOT_INITIALIZED;
    
    setState(WiFiState::DISCONNECTED);
    memset(&info_, 0, sizeof(info_));
    
    return Result::OK;
  }
  
  /**
   * Start access point mode
   * @param ssid AP name
   * @param password AP password (empty for open)
   * @param channel WiFi channel
   */
  Result startAP(const char* ssid, const char* password = "", uint8_t channel = 1) {
    if (!initialized_) return Result::NOT_INITIALIZED;
    
    strncpy(ap_ssid_, ssid, sizeof(ap_ssid_) - 1);
    strncpy(ap_password_, password, sizeof(ap_password_) - 1);
    ap_channel_ = channel;
    
    setState(WiFiState::AP_MODE);
    
    // Default AP IP: 192.168.4.1
    strcpy(info_.ip, "192.168.4.1");
    strcpy(info_.gateway, "192.168.4.1");
    strcpy(info_.subnet, "255.255.255.0");
    info_.channel = channel;
    info_.has_ip = true;
    
    return Result::OK;
  }
  
  /**
   * Start captive portal mode
   * Creates AP and redirects all DNS queries to the config page
   * @param ssid Portal AP name
   * @param password Portal password (empty for open)
   */
  Result startCaptivePortal(const char* ssid = "SynthHead-Setup", 
                            const char* password = "") {
    Result res = startAP(ssid, password);
    if (res != Result::OK) return res;
    
    captive_portal_active_ = true;
    setState(WiFiState::PORTAL_MODE);
    
    return Result::OK;
  }
  
  /**
   * Stop captive portal
   */
  Result stopCaptivePortal() {
    captive_portal_active_ = false;
    return Result::OK;
  }
  
  /**
   * Add HTTP route
   * @param path URL path (e.g., "/config")
   * @param method HTTP method
   * @param handler Request handler function
   */
  Result addRoute(const char* path, HttpMethod method, RouteHandler handler) {
    if (!initialized_) return Result::NOT_INITIALIZED;
    if (route_count_ >= MAX_ROUTES) return Result::BUFFER_FULL;
    
    Route& r = routes_[route_count_++];
    strncpy(r.path, path, sizeof(r.path) - 1);
    r.path[sizeof(r.path) - 1] = '\0';
    r.method = method;
    r.handler = handler;
    r.active = true;
    
    return Result::OK;
  }
  
  /**
   * Register callback for network state changes
   */
  Result onStateChange(NetworkCallback callback) {
    state_callback_ = callback;
    return Result::OK;
  }
  
  /**
   * Register callback for network events
   */
  Result onEvent(std::function<void(NetworkEvent)> callback) {
    event_callback_ = callback;
    return Result::OK;
  }
  
  /**
   * Set hostname for mDNS
   */
  Result setHostname(const char* hostname) {
    strncpy(hostname_, hostname, sizeof(hostname_) - 1);
    hostname_[sizeof(hostname_) - 1] = '\0';
    return Result::OK;
  }
  
  /**
   * Get current network info
   */
  const NetworkInfo& getInfo() const { return info_; }
  
  /**
   * Get current WiFi state
   */
  WiFiState getState() const { return state_; }
  
  /**
   * Check if connected to a network
   */
  bool isConnected() const {
    return state_ == WiFiState::CONNECTED && info_.has_ip;
  }
  
  /**
   * Check if in AP mode
   */
  bool isAPActive() const {
    return state_ == WiFiState::AP_MODE || state_ == WiFiState::PORTAL_MODE;
  }
  
  /**
   * Get IP address string
   */
  const char* getIP() const { return info_.ip; }
  
  /**
   * Get hostname
   */
  const char* getHostname() const { return hostname_; }
  
  /**
   * Update network state - call regularly
   * @param dt_ms Time since last update
   */
  void update(uint32_t dt_ms) {
    if (!initialized_) return;
    
    current_time_ += dt_ms;
    
    // Handle connection timeout
    if (state_ == WiFiState::CONNECTING) {
      if (connect_start_time_ == 0) {
        connect_start_time_ = current_time_;
      }
      
      if (current_time_ - connect_start_time_ >= connect_timeout_) {
        setState(WiFiState::ERROR);
        fireEvent(NetworkEvent::CONNECT_FAILED);
      }
    }
    
    // Simulated connection success for testing
    // Real implementation would check actual WiFi stack
  }
  
  /**
   * Handle incoming HTTP request (called by HAL)
   */
  void handleRequest(const HttpRequest& request, HttpResponse& response) {
    // Check for captive portal redirect
    if (captive_portal_active_) {
      // Captive portal detection endpoints
      if (strcmp(request.uri, "/generate_204") == 0 ||
          strcmp(request.uri, "/gen_204") == 0 ||
          strcmp(request.uri, "/hotspot-detect.html") == 0 ||
          strcmp(request.uri, "/canonical.html") == 0) {
        response.status(302)
                .type("text/html")
                .send("<html><head><meta http-equiv='refresh' content='0; url=/'></head></html>");
        return;
      }
    }
    
    // Find matching route
    for (size_t i = 0; i < route_count_; i++) {
      Route& r = routes_[i];
      if (r.active && strcmp(r.path, request.uri) == 0 && r.method == request.method) {
        r.handler(request, response);
        return;
      }
    }
    
    // 404 Not Found
    response.status(404).type("text/plain").send("Not Found");
  }
  
  // HAL callbacks for actual WiFi events
  void onWiFiConnected(const char* ssid, const char* ip) {
    strncpy(info_.ssid, ssid, sizeof(info_.ssid) - 1);
    strncpy(info_.ip, ip, sizeof(info_.ip) - 1);
    info_.connected = true;
    info_.has_ip = true;
    
    setState(WiFiState::CONNECTED);
    fireEvent(NetworkEvent::CONNECTED);
  }
  
  void onWiFiDisconnected() {
    info_.connected = false;
    info_.has_ip = false;
    
    setState(WiFiState::DISCONNECTED);
    fireEvent(NetworkEvent::DISCONNECTED);
  }
  
  void onIPAcquired(const char* ip) {
    strncpy(info_.ip, ip, sizeof(info_.ip) - 1);
    info_.has_ip = true;
    fireEvent(NetworkEvent::IP_ACQUIRED);
  }
  
private:
  void setState(WiFiState new_state) {
    if (state_ != new_state) {
      state_ = new_state;
      if (state_callback_) {
        state_callback_(new_state);
      }
    }
  }
  
  void fireEvent(NetworkEvent event) {
    if (event_callback_) {
      event_callback_(event);
    }
  }
  
  void addDefaultRoutes() {
    // Root - Main config page
    addRoute("/", HttpMethod::GET, [this](const HttpRequest&, HttpResponse& res) {
      res.type("text/html").send(R"html(
<!DOCTYPE html>
<html>
<head>
  <title>SynthHead Config</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: sans-serif; padding: 20px; background: #1a1a2e; color: #fff; }
    h1 { color: #e94560; }
    input, button { padding: 10px; margin: 5px 0; width: 100%; box-sizing: border-box; }
    button { background: #e94560; color: white; border: none; cursor: pointer; }
    button:hover { background: #ff6b6b; }
    .card { background: #16213e; padding: 20px; border-radius: 10px; margin: 10px 0; }
  </style>
</head>
<body>
  <h1>SynthHead Setup</h1>
  <div class="card">
    <h2>WiFi Configuration</h2>
    <form action="/wifi" method="POST">
      <input type="text" name="ssid" placeholder="Network Name" required>
      <input type="password" name="password" placeholder="Password">
      <button type="submit">Connect</button>
    </form>
  </div>
  <div class="card">
    <h2>Device Info</h2>
    <p>Hostname: synthhead.local</p>
    <p>IP: 192.168.4.1</p>
  </div>
</body>
</html>
)html");
    });
    
    // WiFi config endpoint
    addRoute("/wifi", HttpMethod::POST, [this](const HttpRequest& req, HttpResponse& res) {
      // Parse form data from body (simplified)
      (void)req;
      res.type("text/html").send(R"html(
<!DOCTYPE html>
<html>
<head>
  <title>Connecting...</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: sans-serif; padding: 20px; background: #1a1a2e; color: #fff; text-align: center; }
    .spinner { border: 4px solid #16213e; border-top: 4px solid #e94560; border-radius: 50%; width: 40px; height: 40px; animation: spin 1s linear infinite; margin: 20px auto; }
    @keyframes spin { 0% { transform: rotate(0deg); } 100% { transform: rotate(360deg); } }
  </style>
</head>
<body>
  <h1>Connecting to WiFi...</h1>
  <div class="spinner"></div>
  <p>Please wait. The device will restart once connected.</p>
</body>
</html>
)html");
    });
    
    // Status API
    addRoute("/api/status", HttpMethod::GET, [this](const HttpRequest&, HttpResponse& res) {
      char json[256];
      snprintf(json, sizeof(json),
               R"({"state":%d,"connected":%s,"ip":"%s","ssid":"%s","rssi":%d})",
               static_cast<int>(state_),
               info_.connected ? "true" : "false",
               info_.ip,
               info_.ssid,
               info_.rssi);
      res.json(json);
    });
    
    // Scan API
    addRoute("/api/scan", HttpMethod::GET, [](const HttpRequest&, HttpResponse& res) {
      // Would trigger WiFi scan and return results
      res.json(R"({"networks":[]})");
    });
  }
  
  bool initialized_ = false;
  WiFiState state_ = WiFiState::DISCONNECTED;
  uint32_t current_time_ = 0;
  
  char hostname_[32] = "synthhead";
  char ap_ssid_[33] = "";
  char ap_password_[65] = "";
  uint8_t ap_channel_ = 1;
  
  char pending_ssid_[33] = "";
  char pending_password_[65] = "";
  uint32_t connect_timeout_ = 10000;
  uint32_t connect_start_time_ = 0;
  
  bool captive_portal_active_ = false;
  
  NetworkInfo info_{};
  Route routes_[MAX_ROUTES];
  size_t route_count_ = 0;
  
  NetworkCallback state_callback_;
  std::function<void(NetworkEvent)> event_callback_;
};

} // namespace arcos::framework

#endif // ARCOS_INCLUDE_FRAMEWORKAPI_NETWORK_SERVICE_HPP_
