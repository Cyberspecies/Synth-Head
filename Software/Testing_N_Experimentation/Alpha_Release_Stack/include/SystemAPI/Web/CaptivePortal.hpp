/*****************************************************************
 * @file CaptivePortal.hpp
 * @brief WiFi Captive Portal with WebSocket sync to OLED UI
 * 
 * Creates a WiFi access point with a captive portal that serves
 * a web interface. Uses WebSocket for real-time bidirectional
 * synchronization with the OLED display UI.
 * 
 * @author ARCOS
 * @version 1.0
 *****************************************************************/

#pragma once

#include "SystemAPI/SyncState.hpp"

#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <DNSServer.h>
#include <ArduinoJson.h>

namespace SystemAPI {
namespace Web {

// Forward declaration of HTML content (defined at end of file)
extern const char INDEX_HTML[];
extern const char STYLE_CSS[];
extern const char SCRIPT_JS[];

/**
 * @brief Captive Portal Manager
 * 
 * Handles WiFi AP creation, DNS for captive portal,
 * HTTP server for static files, and WebSocket for real-time sync.
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
  bool init(const char* ssid = "SynthHead-AP", const char* password = "") {
    strncpy(ssid_, ssid, sizeof(ssid_) - 1);
    strncpy(password_, password, sizeof(password_) - 1);
    
    // Update sync state
    auto& state = SYNC_STATE.state();
    strncpy(state.ssid, ssid_, sizeof(state.ssid) - 1);
    
    // Start WiFi AP
    WiFi.mode(WIFI_AP);
    if (strlen(password_) > 0) {
      WiFi.softAP(ssid_, password_);
    } else {
      WiFi.softAP(ssid_);
    }
    
    IPAddress ip = WiFi.softAPIP();
    snprintf(state.ipAddress, sizeof(state.ipAddress), 
             "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
    
    // Start DNS server for captive portal
    dnsServer_.start(53, "*", ip);
    
    // Setup HTTP routes
    setupRoutes();
    httpServer_.begin();
    
    // Setup WebSocket
    wsServer_.begin();
    wsServer_.onEvent([this](uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
      handleWebSocketEvent(num, type, payload, length);
    });
    
    // Register sync state change callback
    SYNC_STATE.setOnChange([this](uint32_t flags) {
      broadcastState(flags);
    });
    
    initialized_ = true;
    return true;
  }
  
  /**
   * @brief Update the portal (call in loop)
   */
  void update() {
    if (!initialized_) return;
    
    dnsServer_.processNextRequest();
    httpServer_.handleClient();
    wsServer_.loop();
    
    // Update client count
    auto& state = SYNC_STATE.state();
    state.wifiClients = wsServer_.connectedClients();
    
    // Periodic full state broadcast
    broadcastTimer_ += 0.001f;  // Assume ~1ms per call
    if (broadcastTimer_ >= 1.0f) {  // Every second
      broadcastState(SyncState::FLAG_ALL);
      broadcastTimer_ = 0;
    }
  }
  
  /**
   * @brief Get number of connected WebSocket clients
   */
  uint8_t getClientCount() const {
    return wsServer_.connectedClients();
  }
  
  /**
   * @brief Send a notification to all web clients
   */
  void sendNotification(const char* title, const char* message, const char* type = "info") {
    StaticJsonDocument<256> doc;
    doc["type"] = "notification";
    doc["title"] = title;
    doc["message"] = message;
    doc["notifyType"] = type;
    
    char buffer[256];
    serializeJson(doc, buffer);
    wsServer_.broadcastTXT(buffer);
  }
  
private:
  CaptivePortal() : httpServer_(80), wsServer_(81) {}
  
  void setupRoutes() {
    // Main page
    httpServer_.on("/", HTTP_GET, [this]() {
      httpServer_.send(200, "text/html", INDEX_HTML);
    });
    
    // CSS
    httpServer_.on("/style.css", HTTP_GET, [this]() {
      httpServer_.send(200, "text/css", STYLE_CSS);
    });
    
    // JavaScript
    httpServer_.on("/script.js", HTTP_GET, [this]() {
      httpServer_.send(200, "application/javascript", SCRIPT_JS);
    });
    
    // API endpoint for initial state
    httpServer_.on("/api/state", HTTP_GET, [this]() {
      String json = serializeState();
      httpServer_.send(200, "application/json", json);
    });
    
    // Captive portal detection
    httpServer_.on("/generate_204", HTTP_GET, [this]() {
      httpServer_.sendHeader("Location", "/");
      httpServer_.send(302, "text/plain", "");
    });
    
    httpServer_.on("/fwlink", HTTP_GET, [this]() {
      httpServer_.sendHeader("Location", "/");
      httpServer_.send(302, "text/plain", "");
    });
    
    httpServer_.on("/hotspot-detect.html", HTTP_GET, [this]() {
      httpServer_.send(200, "text/html", INDEX_HTML);
    });
    
    // 404 handler - redirect to main page (captive portal behavior)
    httpServer_.onNotFound([this]() {
      httpServer_.sendHeader("Location", "/");
      httpServer_.send(302, "text/plain", "");
    });
  }
  
  void handleWebSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
    switch (type) {
      case WStype_CONNECTED: {
        // Send full state to new client
        String state = serializeState();
        wsServer_.sendTXT(num, state);
        break;
      }
      
      case WStype_DISCONNECTED:
        break;
        
      case WStype_TEXT: {
        // Parse incoming JSON command
        StaticJsonDocument<512> doc;
        DeserializationError error = deserializeJson(doc, payload, length);
        
        if (!error) {
          handleCommand(doc);
        }
        break;
      }
      
      default:
        break;
    }
  }
  
  void handleCommand(JsonDocument& doc) {
    const char* cmd = doc["cmd"];
    if (!cmd) return;
    
    auto& state = SYNC_STATE.state();
    
    if (strcmp(cmd, "setBrightness") == 0) {
      SYNC_STATE.setBrightness(doc["value"].as<int>());
    }
    else if (strcmp(cmd, "setFanSpeed") == 0) {
      SYNC_STATE.setFanSpeed(doc["value"].as<int>());
    }
    else if (strcmp(cmd, "setSlider1") == 0) {
      SYNC_STATE.setSlider1(doc["value"].as<int>());
    }
    else if (strcmp(cmd, "setSlider2") == 0) {
      SYNC_STATE.setSlider2(doc["value"].as<int>());
    }
    else if (strcmp(cmd, "setSlider3") == 0) {
      SYNC_STATE.setSlider3(doc["value"].as<int>());
    }
    else if (strcmp(cmd, "setToggle1") == 0) {
      SYNC_STATE.setToggle1(doc["value"].as<bool>());
      SYNC_STATE.setLedEnabled(doc["value"].as<bool>());
    }
    else if (strcmp(cmd, "setToggle2") == 0) {
      SYNC_STATE.setToggle2(doc["value"].as<bool>());
      state.displayEnabled = doc["value"].as<bool>();
    }
    else if (strcmp(cmd, "setToggle3") == 0) {
      SYNC_STATE.setToggle3(doc["value"].as<bool>());
    }
    else if (strcmp(cmd, "setLedColor") == 0) {
      SYNC_STATE.setLedColor(doc["value"].as<int>());
    }
    else if (strcmp(cmd, "setDropdown1") == 0) {
      SYNC_STATE.setDropdown1(doc["value"].as<int>());
    }
    else if (strcmp(cmd, "setDropdown2") == 0) {
      SYNC_STATE.setDropdown2(doc["value"].as<int>());
    }
    else if (strcmp(cmd, "setTab") == 0) {
      SYNC_STATE.setSelectedTab(doc["value"].as<int>());
    }
    else if (strcmp(cmd, "setMode") == 0) {
      SYNC_STATE.setMode(static_cast<SystemMode>(doc["value"].as<int>()));
    }
  }
  
  String serializeState() {
    auto& state = SYNC_STATE.state();
    
    StaticJsonDocument<1024> doc;
    doc["type"] = "state";
    doc["version"] = state.version;
    
    // System
    doc["mode"] = (int)state.mode;
    doc["status"] = state.statusText;
    
    // Controls
    doc["brightness"] = state.brightness;
    doc["displayEnabled"] = state.displayEnabled;
    doc["animationSpeed"] = state.animationSpeed;
    doc["ledEnabled"] = state.ledEnabled;
    doc["ledColor"] = state.ledColor;
    doc["fanSpeed"] = state.fanSpeed;
    
    // Sliders
    doc["slider1"] = state.slider1Value;
    doc["slider2"] = state.slider2Value;
    doc["slider3"] = state.slider3Value;
    
    // Toggles
    doc["toggle1"] = state.toggle1;
    doc["toggle2"] = state.toggle2;
    doc["toggle3"] = state.toggle3;
    
    // Dropdowns
    doc["dropdown1"] = state.dropdown1Selection;
    doc["dropdown2"] = state.dropdown2Selection;
    
    // Sensors
    doc["temperature"] = state.temperature;
    doc["humidity"] = state.humidity;
    doc["pressure"] = state.pressure;
    doc["accelX"] = state.accelX;
    doc["accelY"] = state.accelY;
    doc["accelZ"] = state.accelZ;
    doc["gyroX"] = state.gyroX;
    doc["gyroY"] = state.gyroY;
    doc["gyroZ"] = state.gyroZ;
    
    // GPS
    doc["latitude"] = state.latitude;
    doc["longitude"] = state.longitude;
    doc["altitude"] = state.altitude;
    doc["satellites"] = state.satellites;
    doc["gpsValid"] = state.gpsValid;
    
    // Network
    doc["ssid"] = state.ssid;
    doc["ip"] = state.ipAddress;
    doc["clients"] = state.wifiClients;
    
    // Stats
    doc["uptime"] = state.uptime;
    doc["freeHeap"] = state.freeHeap;
    doc["cpuUsage"] = state.cpuUsage;
    doc["fps"] = state.fps;
    
    // Selected tab
    doc["selectedTab"] = state.selectedTab;
    
    String output;
    serializeJson(doc, output);
    return output;
  }
  
  void broadcastState(uint32_t flags) {
    if (!initialized_ || wsServer_.connectedClients() == 0) return;
    
    String json = serializeState();
    wsServer_.broadcastTXT(json);
  }
  
  // State
  bool initialized_ = false;
  char ssid_[32] = "SynthHead-AP";
  char password_[32] = "";
  float broadcastTimer_ = 0;
  
  // Servers
  DNSServer dnsServer_;
  WebServer httpServer_;
  WebSocketsServer wsServer_;
};

// Convenience macro
#define CAPTIVE_PORTAL CaptivePortal::instance()

// ============================================================
// Embedded Web Content
// ============================================================

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
  <title>SynthHead Control Panel</title>
  <link rel="stylesheet" href="/style.css">
</head>
<body>
  <div class="container">
    <header>
      <h1><span class="logo">⚙</span> SynthHead</h1>
      <div class="status-bar">
        <span id="connection-status" class="status disconnected">⚫ Disconnected</span>
        <span id="wifi-clients">👥 0</span>
      </div>
    </header>
    
    <nav class="tabs">
      <button class="tab active" data-tab="status">📊 Status</button>
      <button class="tab" data-tab="controls">🎛 Controls</button>
      <button class="tab" data-tab="sensors">📡 Sensors</button>
      <button class="tab" data-tab="settings">⚙ Settings</button>
    </nav>
    
    <!-- Status Tab -->
    <section id="status" class="tab-content active">
      <div class="card">
        <h2>System Status</h2>
        <div class="status-grid">
          <div class="status-item">
            <label>Mode</label>
            <span id="mode" class="value mode-idle">IDLE</span>
          </div>
          <div class="status-item">
            <label>Uptime</label>
            <span id="uptime" class="value">00:00:00</span>
          </div>
          <div class="status-item">
            <label>CPU</label>
            <div class="progress-bar">
              <div id="cpu-bar" class="progress" style="width: 0%"></div>
            </div>
            <span id="cpu-value" class="value-small">0%</span>
          </div>
          <div class="status-item">
            <label>Free Heap</label>
            <span id="heap" class="value">0 KB</span>
          </div>
          <div class="status-item">
            <label>FPS</label>
            <span id="fps" class="value">0</span>
          </div>
        </div>
      </div>
      
      <div class="card">
        <h2>Network</h2>
        <div class="info-list">
          <div class="info-item">
            <span class="label">SSID:</span>
            <span id="ssid" class="value">SynthHead-AP</span>
          </div>
          <div class="info-item">
            <span class="label">IP:</span>
            <span id="ip" class="value">192.168.4.1</span>
          </div>
          <div class="info-item">
            <span class="label">Clients:</span>
            <span id="clients" class="value">0</span>
          </div>
        </div>
      </div>
    </section>
    
    <!-- Controls Tab -->
    <section id="controls" class="tab-content">
      <div class="card">
        <h2>Display</h2>
        <div class="control-group">
          <label>Brightness</label>
          <input type="range" id="brightness" min="0" max="255" value="128" class="slider">
          <span id="brightness-value" class="slider-value">128</span>
        </div>
      </div>
      
      <div class="card">
        <h2>Hardware</h2>
        <div class="control-group">
          <label>Fan Speed</label>
          <input type="range" id="fanSpeed" min="0" max="100" value="0" class="slider">
          <span id="fanSpeed-value" class="slider-value">0%</span>
        </div>
        
        <div class="control-group">
          <label>Slider 1</label>
          <input type="range" id="slider1" min="0" max="100" value="50" class="slider">
          <span id="slider1-value" class="slider-value">50</span>
        </div>
      </div>
      
      <div class="card">
        <h2>Toggles</h2>
        <div class="toggle-group">
          <label class="toggle-label">
            <span>LED Enable</span>
            <input type="checkbox" id="toggle1" class="toggle">
            <span class="toggle-slider"></span>
          </label>
        </div>
        <div class="toggle-group">
          <label class="toggle-label">
            <span>Display Enable</span>
            <input type="checkbox" id="toggle2" class="toggle" checked>
            <span class="toggle-slider"></span>
          </label>
        </div>
        <div class="toggle-group">
          <label class="toggle-label">
            <span>Auto Mode</span>
            <input type="checkbox" id="toggle3" class="toggle">
            <span class="toggle-slider"></span>
          </label>
        </div>
      </div>
    </section>
    
    <!-- Sensors Tab -->
    <section id="sensors" class="tab-content">
      <div class="card">
        <h2>🌡 Environment</h2>
        <div class="sensor-grid">
          <div class="sensor-item">
            <span class="sensor-label">Temperature</span>
            <span id="temperature" class="sensor-value">--.-°C</span>
          </div>
          <div class="sensor-item">
            <span class="sensor-label">Humidity</span>
            <span id="humidity" class="sensor-value">--.-%</span>
          </div>
          <div class="sensor-item">
            <span class="sensor-label">Pressure</span>
            <span id="pressure" class="sensor-value">---- hPa</span>
          </div>
        </div>
      </div>
      
      <div class="card">
        <h2>🎮 IMU</h2>
        <div class="imu-display">
          <div class="imu-row">
            <span class="imu-label">Accel</span>
            <span id="accel" class="imu-value">X:0 Y:0 Z:0</span>
          </div>
          <div class="imu-row">
            <span class="imu-label">Gyro</span>
            <span id="gyro" class="imu-value">X:0 Y:0 Z:0</span>
          </div>
        </div>
      </div>
      
      <div class="card">
        <h2>📍 GPS</h2>
        <div class="gps-status" id="gps-status">
          <span class="gps-indicator no-fix">●</span>
          <span id="gps-fix">No Fix</span>
        </div>
        <div class="gps-grid">
          <div class="gps-item">
            <span class="gps-label">Latitude</span>
            <span id="latitude" class="gps-value">--.------</span>
          </div>
          <div class="gps-item">
            <span class="gps-label">Longitude</span>
            <span id="longitude" class="gps-value">--.------</span>
          </div>
          <div class="gps-item">
            <span class="gps-label">Altitude</span>
            <span id="altitude" class="gps-value">--- m</span>
          </div>
          <div class="gps-item">
            <span class="gps-label">Satellites</span>
            <span id="satellites" class="gps-value">0</span>
          </div>
        </div>
      </div>
    </section>
    
    <!-- Settings Tab -->
    <section id="settings" class="tab-content">
      <div class="card">
        <h2>LED Settings</h2>
        <div class="control-group">
          <label>LED Color</label>
          <select id="ledColor" class="dropdown">
            <option value="0">Off</option>
            <option value="1">Red</option>
            <option value="2">Green</option>
            <option value="3">Blue</option>
            <option value="4">White</option>
          </select>
        </div>
      </div>
      
      <div class="card">
        <h2>Animation</h2>
        <div class="control-group">
          <label>Animation Speed</label>
          <input type="range" id="slider2" min="0" max="100" value="50" class="slider">
          <span id="slider2-value" class="slider-value">50</span>
        </div>
        
        <div class="control-group">
          <label>Misc Value</label>
          <input type="range" id="slider3" min="0" max="100" value="50" class="slider">
          <span id="slider3-value" class="slider-value">50</span>
        </div>
      </div>
      
      <div class="card danger">
        <h2>⚠ Danger Zone</h2>
        <button id="resetBtn" class="btn btn-danger">Factory Reset</button>
      </div>
    </section>
    
    <footer>
      <p>SynthHead v1.0 • ARCOS Framework</p>
    </footer>
  </div>
  
  <!-- Toast Notification -->
  <div id="toast" class="toast"></div>
  
  <script src="/script.js"></script>
</body>
</html>
)rawliteral";

const char STYLE_CSS[] PROGMEM = R"rawliteral(
:root {
  --bg-primary: #0d1117;
  --bg-secondary: #161b22;
  --bg-tertiary: #21262d;
  --text-primary: #f0f6fc;
  --text-secondary: #8b949e;
  --accent: #58a6ff;
  --accent-hover: #79b8ff;
  --success: #3fb950;
  --warning: #d29922;
  --danger: #f85149;
  --border: #30363d;
  --shadow: rgba(0,0,0,0.3);
}

* {
  margin: 0;
  padding: 0;
  box-sizing: border-box;
}

body {
  font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Oxygen, Ubuntu, sans-serif;
  background: var(--bg-primary);
  color: var(--text-primary);
  min-height: 100vh;
  line-height: 1.5;
}

.container {
  max-width: 600px;
  margin: 0 auto;
  padding: 16px;
}

header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 16px 0;
  border-bottom: 1px solid var(--border);
  margin-bottom: 16px;
}

header h1 {
  font-size: 1.5rem;
  font-weight: 600;
  display: flex;
  align-items: center;
  gap: 8px;
}

.logo {
  font-size: 1.8rem;
}

.status-bar {
  display: flex;
  gap: 16px;
  font-size: 0.85rem;
}

.status {
  padding: 4px 8px;
  border-radius: 12px;
  font-weight: 500;
}

.status.connected {
  background: rgba(63, 185, 80, 0.2);
  color: var(--success);
}

.status.disconnected {
  background: rgba(248, 81, 73, 0.2);
  color: var(--danger);
}

/* Tabs */
.tabs {
  display: flex;
  gap: 8px;
  margin-bottom: 16px;
  overflow-x: auto;
  padding-bottom: 8px;
}

.tab {
  background: var(--bg-secondary);
  border: 1px solid var(--border);
  color: var(--text-secondary);
  padding: 10px 16px;
  border-radius: 8px;
  cursor: pointer;
  font-size: 0.9rem;
  white-space: nowrap;
  transition: all 0.2s;
}

.tab:hover {
  background: var(--bg-tertiary);
  color: var(--text-primary);
}

.tab.active {
  background: var(--accent);
  color: var(--bg-primary);
  border-color: var(--accent);
}

/* Tab Content */
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

/* Cards */
.card {
  background: var(--bg-secondary);
  border: 1px solid var(--border);
  border-radius: 12px;
  padding: 16px;
  margin-bottom: 16px;
}

.card h2 {
  font-size: 1rem;
  color: var(--text-secondary);
  margin-bottom: 12px;
  padding-bottom: 8px;
  border-bottom: 1px solid var(--border);
}

.card.danger {
  border-color: var(--danger);
}

.card.danger h2 {
  color: var(--danger);
}

/* Status Grid */
.status-grid {
  display: grid;
  grid-template-columns: repeat(2, 1fr);
  gap: 12px;
}

.status-item {
  display: flex;
  flex-direction: column;
  gap: 4px;
}

.status-item label {
  font-size: 0.75rem;
  color: var(--text-secondary);
  text-transform: uppercase;
}

.status-item .value {
  font-size: 1.1rem;
  font-weight: 600;
  font-family: 'SF Mono', Monaco, monospace;
}

.value-small {
  font-size: 0.85rem;
  color: var(--text-secondary);
}

.mode-idle { color: var(--text-secondary); }
.mode-running { color: var(--success); }
.mode-paused { color: var(--warning); }
.mode-error { color: var(--danger); }

/* Progress Bar */
.progress-bar {
  width: 100%;
  height: 8px;
  background: var(--bg-tertiary);
  border-radius: 4px;
  overflow: hidden;
}

.progress {
  height: 100%;
  background: linear-gradient(90deg, var(--accent), var(--success));
  border-radius: 4px;
  transition: width 0.3s ease;
}

/* Info List */
.info-list {
  display: flex;
  flex-direction: column;
  gap: 8px;
}

.info-item {
  display: flex;
  justify-content: space-between;
}

.info-item .label {
  color: var(--text-secondary);
}

.info-item .value {
  font-family: 'SF Mono', Monaco, monospace;
}

/* Controls */
.control-group {
  margin-bottom: 16px;
}

.control-group:last-child {
  margin-bottom: 0;
}

.control-group label {
  display: block;
  font-size: 0.85rem;
  color: var(--text-secondary);
  margin-bottom: 8px;
}

/* Sliders */
.slider {
  -webkit-appearance: none;
  width: 100%;
  height: 8px;
  background: var(--bg-tertiary);
  border-radius: 4px;
  outline: none;
  margin-bottom: 4px;
}

.slider::-webkit-slider-thumb {
  -webkit-appearance: none;
  width: 20px;
  height: 20px;
  background: var(--accent);
  border-radius: 50%;
  cursor: pointer;
  transition: transform 0.2s;
}

.slider::-webkit-slider-thumb:hover {
  transform: scale(1.1);
}

.slider::-webkit-slider-thumb:active {
  transform: scale(0.95);
  background: var(--accent-hover);
}

.slider-value {
  font-size: 0.85rem;
  color: var(--text-secondary);
  font-family: 'SF Mono', Monaco, monospace;
}

/* Toggles */
.toggle-group {
  margin-bottom: 12px;
}

.toggle-label {
  display: flex;
  justify-content: space-between;
  align-items: center;
  cursor: pointer;
  padding: 8px 0;
}

.toggle-label span:first-child {
  color: var(--text-primary);
}

.toggle {
  display: none;
}

.toggle-slider {
  width: 48px;
  height: 24px;
  background: var(--bg-tertiary);
  border-radius: 12px;
  position: relative;
  transition: background 0.3s;
}

.toggle-slider::after {
  content: '';
  position: absolute;
  width: 20px;
  height: 20px;
  background: var(--text-secondary);
  border-radius: 50%;
  top: 2px;
  left: 2px;
  transition: all 0.3s;
}

.toggle:checked + .toggle-slider {
  background: var(--accent);
}

.toggle:checked + .toggle-slider::after {
  left: 26px;
  background: var(--text-primary);
}

/* Dropdown */
.dropdown {
  width: 100%;
  padding: 10px 12px;
  background: var(--bg-tertiary);
  border: 1px solid var(--border);
  border-radius: 8px;
  color: var(--text-primary);
  font-size: 1rem;
  cursor: pointer;
  outline: none;
}

.dropdown:focus {
  border-color: var(--accent);
}

/* Sensor Display */
.sensor-grid {
  display: grid;
  grid-template-columns: repeat(3, 1fr);
  gap: 12px;
}

.sensor-item {
  text-align: center;
  padding: 12px;
  background: var(--bg-tertiary);
  border-radius: 8px;
}

.sensor-label {
  display: block;
  font-size: 0.7rem;
  color: var(--text-secondary);
  text-transform: uppercase;
  margin-bottom: 4px;
}

.sensor-value {
  font-size: 1rem;
  font-weight: 600;
  font-family: 'SF Mono', Monaco, monospace;
}

/* IMU Display */
.imu-display {
  display: flex;
  flex-direction: column;
  gap: 8px;
}

.imu-row {
  display: flex;
  justify-content: space-between;
  padding: 8px 12px;
  background: var(--bg-tertiary);
  border-radius: 8px;
}

.imu-label {
  color: var(--text-secondary);
  font-weight: 500;
}

.imu-value {
  font-family: 'SF Mono', Monaco, monospace;
}

/* GPS Display */
.gps-status {
  display: flex;
  align-items: center;
  gap: 8px;
  margin-bottom: 12px;
  padding: 8px 12px;
  background: var(--bg-tertiary);
  border-radius: 8px;
}

.gps-indicator {
  font-size: 1.2rem;
}

.gps-indicator.no-fix { color: var(--danger); }
.gps-indicator.fix { color: var(--success); }

.gps-grid {
  display: grid;
  grid-template-columns: repeat(2, 1fr);
  gap: 12px;
}

.gps-item {
  padding: 8px;
  background: var(--bg-tertiary);
  border-radius: 8px;
}

.gps-label {
  display: block;
  font-size: 0.7rem;
  color: var(--text-secondary);
  text-transform: uppercase;
}

.gps-value {
  font-family: 'SF Mono', Monaco, monospace;
}

/* Buttons */
.btn {
  padding: 12px 24px;
  border: none;
  border-radius: 8px;
  font-size: 1rem;
  font-weight: 500;
  cursor: pointer;
  transition: all 0.2s;
}

.btn-danger {
  background: var(--danger);
  color: white;
}

.btn-danger:hover {
  background: #da3633;
  transform: translateY(-2px);
}

.btn-danger:active {
  transform: translateY(0);
}

/* Toast */
.toast {
  position: fixed;
  bottom: 24px;
  left: 50%;
  transform: translateX(-50%) translateY(100px);
  background: var(--bg-tertiary);
  color: var(--text-primary);
  padding: 12px 24px;
  border-radius: 8px;
  box-shadow: 0 4px 20px var(--shadow);
  opacity: 0;
  transition: all 0.3s ease;
  z-index: 1000;
}

.toast.show {
  transform: translateX(-50%) translateY(0);
  opacity: 1;
}

.toast.success { border-left: 4px solid var(--success); }
.toast.warning { border-left: 4px solid var(--warning); }
.toast.error { border-left: 4px solid var(--danger); }
.toast.info { border-left: 4px solid var(--accent); }

/* Footer */
footer {
  text-align: center;
  padding: 24px 0;
  color: var(--text-secondary);
  font-size: 0.85rem;
}

/* Responsive */
@media (max-width: 480px) {
  .container {
    padding: 12px;
  }
  
  header h1 {
    font-size: 1.2rem;
  }
  
  .status-bar {
    flex-direction: column;
    gap: 4px;
    font-size: 0.75rem;
  }
  
  .tabs {
    gap: 4px;
  }
  
  .tab {
    padding: 8px 12px;
    font-size: 0.8rem;
  }
  
  .sensor-grid {
    grid-template-columns: 1fr;
  }
  
  .gps-grid {
    grid-template-columns: 1fr;
  }
}
)rawliteral";

const char SCRIPT_JS[] PROGMEM = R"rawliteral(
// WebSocket connection
let ws = null;
let reconnectTimer = null;
let state = {};

// Connect to WebSocket
function connect() {
  const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
  const host = window.location.hostname || '192.168.4.1';
  ws = new WebSocket(`${protocol}//${host}:81/`);
  
  ws.onopen = () => {
    console.log('WebSocket connected');
    updateConnectionStatus(true);
    clearTimeout(reconnectTimer);
  };
  
  ws.onclose = () => {
    console.log('WebSocket disconnected');
    updateConnectionStatus(false);
    reconnectTimer = setTimeout(connect, 3000);
  };
  
  ws.onerror = (err) => {
    console.error('WebSocket error:', err);
    ws.close();
  };
  
  ws.onmessage = (event) => {
    try {
      const data = JSON.parse(event.data);
      handleMessage(data);
    } catch (e) {
      console.error('Parse error:', e);
    }
  };
}

function updateConnectionStatus(connected) {
  const status = document.getElementById('connection-status');
  if (connected) {
    status.textContent = '🟢 Connected';
    status.className = 'status connected';
  } else {
    status.textContent = '⚫ Disconnected';
    status.className = 'status disconnected';
  }
}

// Handle incoming messages
function handleMessage(data) {
  if (data.type === 'state') {
    state = data;
    updateUI(data);
  } else if (data.type === 'notification') {
    showToast(data.message, data.notifyType);
  }
}

// Update UI from state
function updateUI(data) {
  // Status tab
  updateElement('mode', getModeText(data.mode), `mode-${getModeClass(data.mode)}`);
  updateElement('uptime', formatUptime(data.uptime));
  updateElement('cpu-value', `${Math.round(data.cpuUsage)}%`);
  document.getElementById('cpu-bar').style.width = `${data.cpuUsage}%`;
  updateElement('heap', `${Math.round(data.freeHeap / 1024)} KB`);
  updateElement('fps', data.fps.toFixed(1));
  
  updateElement('ssid', data.ssid);
  updateElement('ip', data.ip);
  updateElement('clients', data.clients);
  document.getElementById('wifi-clients').textContent = `👥 ${data.clients}`;
  
  // Controls tab - update without triggering events
  updateSlider('brightness', data.brightness);
  updateSlider('fanSpeed', data.fanSpeed);
  updateSlider('slider1', data.slider1);
  updateSlider('slider2', data.slider2);
  updateSlider('slider3', data.slider3);
  
  updateToggle('toggle1', data.toggle1);
  updateToggle('toggle2', data.toggle2);
  updateToggle('toggle3', data.toggle3);
  
  // Sensors tab
  updateElement('temperature', `${data.temperature.toFixed(1)}°C`);
  updateElement('humidity', `${data.humidity.toFixed(1)}%`);
  updateElement('pressure', `${data.pressure.toFixed(0)} hPa`);
  
  updateElement('accel', `X:${data.accelX} Y:${data.accelY} Z:${data.accelZ}`);
  updateElement('gyro', `X:${data.gyroX} Y:${data.gyroY} Z:${data.gyroZ}`);
  
  const gpsIndicator = document.querySelector('.gps-indicator');
  const gpsFix = document.getElementById('gps-fix');
  if (data.gpsValid) {
    gpsIndicator.className = 'gps-indicator fix';
    gpsFix.textContent = `Fix (${data.satellites} sats)`;
  } else {
    gpsIndicator.className = 'gps-indicator no-fix';
    gpsFix.textContent = 'No Fix';
  }
  
  updateElement('latitude', data.latitude.toFixed(6));
  updateElement('longitude', data.longitude.toFixed(6));
  updateElement('altitude', `${data.altitude.toFixed(0)} m`);
  updateElement('satellites', data.satellites);
  
  // Settings tab
  document.getElementById('ledColor').value = data.ledColor;
}

function updateElement(id, text, className) {
  const el = document.getElementById(id);
  if (el) {
    el.textContent = text;
    if (className) {
      el.className = `value ${className}`;
    }
  }
}

function updateSlider(id, value) {
  const slider = document.getElementById(id);
  const valueEl = document.getElementById(`${id}-value`);
  if (slider && slider.value != value) {
    slider.value = value;
  }
  if (valueEl) {
    valueEl.textContent = id === 'fanSpeed' ? `${value}%` : value;
  }
}

function updateToggle(id, checked) {
  const toggle = document.getElementById(id);
  if (toggle && toggle.checked !== checked) {
    toggle.checked = checked;
  }
}

// Helper functions
function getModeText(mode) {
  const modes = ['IDLE', 'RUNNING', 'PAUSED', 'ERROR'];
  return modes[mode] || 'UNKNOWN';
}

function getModeClass(mode) {
  const classes = ['idle', 'running', 'paused', 'error'];
  return classes[mode] || 'idle';
}

function formatUptime(seconds) {
  const h = Math.floor(seconds / 3600);
  const m = Math.floor((seconds % 3600) / 60);
  const s = seconds % 60;
  return `${pad(h)}:${pad(m)}:${pad(s)}`;
}

function pad(n) {
  return n.toString().padStart(2, '0');
}

// Send command to device
function sendCommand(cmd, value) {
  if (ws && ws.readyState === WebSocket.OPEN) {
    ws.send(JSON.stringify({ cmd, value }));
  }
}

// Toast notification
function showToast(message, type = 'info') {
  const toast = document.getElementById('toast');
  toast.textContent = message;
  toast.className = `toast ${type} show`;
  
  setTimeout(() => {
    toast.className = 'toast';
  }, 3000);
}

// Tab switching
document.querySelectorAll('.tab').forEach(tab => {
  tab.addEventListener('click', () => {
    // Update active tab
    document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));
    tab.classList.add('active');
    
    // Update active content
    const tabId = tab.dataset.tab;
    document.querySelectorAll('.tab-content').forEach(c => c.classList.remove('active'));
    document.getElementById(tabId).classList.add('active');
    
    // Notify device
    const tabIndex = ['status', 'controls', 'sensors', 'settings'].indexOf(tabId);
    sendCommand('setTab', tabIndex);
  });
});

// Slider event handlers
document.querySelectorAll('.slider').forEach(slider => {
  slider.addEventListener('input', (e) => {
    const id = e.target.id;
    const value = parseInt(e.target.value);
    const valueEl = document.getElementById(`${id}-value`);
    if (valueEl) {
      valueEl.textContent = id === 'fanSpeed' ? `${value}%` : value;
    }
  });
  
  slider.addEventListener('change', (e) => {
    const id = e.target.id;
    const value = parseInt(e.target.value);
    
    const cmdMap = {
      'brightness': 'setBrightness',
      'fanSpeed': 'setFanSpeed',
      'slider1': 'setSlider1',
      'slider2': 'setSlider2',
      'slider3': 'setSlider3'
    };
    
    if (cmdMap[id]) {
      sendCommand(cmdMap[id], value);
    }
  });
});

// Toggle event handlers
document.querySelectorAll('.toggle').forEach(toggle => {
  toggle.addEventListener('change', (e) => {
    const id = e.target.id;
    const value = e.target.checked;
    
    const cmdMap = {
      'toggle1': 'setToggle1',
      'toggle2': 'setToggle2',
      'toggle3': 'setToggle3'
    };
    
    if (cmdMap[id]) {
      sendCommand(cmdMap[id], value);
    }
  });
});

// Dropdown event handlers
document.getElementById('ledColor').addEventListener('change', (e) => {
  sendCommand('setLedColor', parseInt(e.target.value));
});

// Reset button
document.getElementById('resetBtn').addEventListener('click', () => {
  if (confirm('Are you sure you want to factory reset?')) {
    sendCommand('factoryReset', true);
    showToast('Factory reset initiated...', 'warning');
  }
});

// Initialize
connect();
)rawliteral";

} // namespace Web
} // namespace SystemAPI
