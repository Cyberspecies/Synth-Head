/**
 * @file CPU_OledWebDemo.cpp
 * @brief Simplified OLED + Web Captive Portal Demo
 * 
 * Demonstrates:
 * - OLED UI with SSD1327 128x128 display
 * - WiFi Captive Portal
 * - WebSocket bidirectional sync
 */

#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <DNSServer.h>
#include <ArduinoJson.h>

// ============================================================
// Pin Definitions
// ============================================================
#define I2C_SDA       11
#define I2C_SCL       12
#define OLED_CS       15
#define OLED_DC       16
#define OLED_RST      17
#define OLED_CLK      36
#define OLED_MOSI     35
#define ENC_A         40
#define ENC_B         38
#define ENC_BTN       39
#define LED_PIN       21

// ============================================================
// Display & Server Setup
// ============================================================
U8G2_SSD1327_WS_128X128_F_4W_HW_SPI u8g2(U8G2_R0, OLED_CS, OLED_DC, OLED_RST);

const char* AP_SSID = "SynthHead-AP";
const char* AP_PASS = "12345678";
const IPAddress AP_IP(192, 168, 4, 1);
const byte DNS_PORT = 53;

WebServer server(80);
WebSocketsServer webSocket(81);
DNSServer dnsServer;

// ============================================================
// Sync State
// ============================================================
struct SyncState {
  uint8_t mode = 0;          // 0=Normal, 1=Party, 2=Sleep
  uint8_t brightness = 128;
  uint8_t slider1 = 50;
  uint8_t slider2 = 50;
  bool toggle1 = false;
  bool toggle2 = true;
  float temperature = 0.0f;
  float humidity = 0.0f;
  uint32_t uptime = 0;
  uint8_t connectedClients = 0;
  bool dirty = false;
} state;

// ============================================================
// Encoder State
// ============================================================
volatile int encoderPos = 0;
volatile bool encoderPressed = false;
int menuIndex = 0;
const int MENU_ITEMS = 6;
const char* menuLabels[] = {"Mode", "Brightness", "Slider 1", "Slider 2", "Toggle 1", "Toggle 2"};

void IRAM_ATTR encoderISR() {
  static uint8_t lastState = 0;
  uint8_t a = digitalRead(ENC_A);
  uint8_t b = digitalRead(ENC_B);
  uint8_t currentState = (a << 1) | b;
  
  if (lastState == 0b00) {
    if (currentState == 0b01) encoderPos++;
    else if (currentState == 0b10) encoderPos--;
  }
  lastState = currentState;
}

void IRAM_ATTR buttonISR() {
  static uint32_t lastPress = 0;
  if (millis() - lastPress > 200) {
    encoderPressed = true;
    lastPress = millis();
  }
}

// ============================================================
// Web Page HTML
// ============================================================
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>SynthHead Control</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
      background: linear-gradient(135deg, #1a1a2e 0%, #16213e 100%);
      color: #e0e0e0;
      min-height: 100vh;
      padding: 20px;
    }
    .container { max-width: 600px; margin: 0 auto; }
    h1 {
      text-align: center;
      color: #00d9ff;
      margin-bottom: 30px;
      text-shadow: 0 0 10px rgba(0,217,255,0.5);
    }
    .card {
      background: rgba(255,255,255,0.05);
      border-radius: 15px;
      padding: 20px;
      margin-bottom: 20px;
      backdrop-filter: blur(10px);
      border: 1px solid rgba(255,255,255,0.1);
    }
    .card-title {
      color: #00d9ff;
      font-size: 14px;
      text-transform: uppercase;
      letter-spacing: 1px;
      margin-bottom: 15px;
    }
    .control-row {
      display: flex;
      align-items: center;
      justify-content: space-between;
      margin-bottom: 15px;
    }
    .control-row:last-child { margin-bottom: 0; }
    .label { font-size: 14px; }
    .slider-container { flex: 1; margin: 0 15px; }
    input[type="range"] {
      width: 100%;
      height: 8px;
      border-radius: 4px;
      background: #333;
      outline: none;
      -webkit-appearance: none;
    }
    input[type="range"]::-webkit-slider-thumb {
      -webkit-appearance: none;
      width: 20px;
      height: 20px;
      border-radius: 50%;
      background: #00d9ff;
      cursor: pointer;
      box-shadow: 0 0 10px rgba(0,217,255,0.5);
    }
    .value { min-width: 40px; text-align: right; font-weight: bold; }
    select {
      background: #333;
      color: #fff;
      border: none;
      padding: 10px 15px;
      border-radius: 8px;
      font-size: 14px;
    }
    .toggle {
      position: relative;
      width: 50px;
      height: 26px;
    }
    .toggle input {
      opacity: 0;
      width: 0;
      height: 0;
    }
    .toggle-slider {
      position: absolute;
      cursor: pointer;
      top: 0; left: 0; right: 0; bottom: 0;
      background: #333;
      border-radius: 26px;
      transition: 0.3s;
    }
    .toggle-slider:before {
      position: absolute;
      content: "";
      height: 20px;
      width: 20px;
      left: 3px;
      bottom: 3px;
      background: white;
      border-radius: 50%;
      transition: 0.3s;
    }
    .toggle input:checked + .toggle-slider {
      background: #00d9ff;
    }
    .toggle input:checked + .toggle-slider:before {
      transform: translateX(24px);
    }
    .sensor-grid {
      display: grid;
      grid-template-columns: repeat(2, 1fr);
      gap: 15px;
    }
    .sensor-item {
      background: rgba(0,0,0,0.2);
      border-radius: 10px;
      padding: 15px;
      text-align: center;
    }
    .sensor-value {
      font-size: 24px;
      font-weight: bold;
      color: #00d9ff;
    }
    .sensor-label {
      font-size: 12px;
      color: #888;
      margin-top: 5px;
    }
    .status {
      text-align: center;
      font-size: 12px;
      color: #666;
      margin-top: 20px;
    }
    .status.connected { color: #00ff88; }
  </style>
</head>
<body>
  <div class="container">
    <h1>🎛️ SynthHead</h1>
    
    <div class="card">
      <div class="card-title">Controls</div>
      <div class="control-row">
        <span class="label">Mode</span>
        <select id="mode" onchange="sendUpdate('mode', this.value)">
          <option value="0">Normal</option>
          <option value="1">Party</option>
          <option value="2">Sleep</option>
        </select>
      </div>
      <div class="control-row">
        <span class="label">Brightness</span>
        <div class="slider-container">
          <input type="range" id="brightness" min="0" max="255" value="128" oninput="sendUpdate('brightness', this.value)">
        </div>
        <span class="value" id="brightness-val">128</span>
      </div>
      <div class="control-row">
        <span class="label">Slider 1</span>
        <div class="slider-container">
          <input type="range" id="slider1" min="0" max="100" value="50" oninput="sendUpdate('slider1', this.value)">
        </div>
        <span class="value" id="slider1-val">50</span>
      </div>
      <div class="control-row">
        <span class="label">Slider 2</span>
        <div class="slider-container">
          <input type="range" id="slider2" min="0" max="100" value="50" oninput="sendUpdate('slider2', this.value)">
        </div>
        <span class="value" id="slider2-val">50</span>
      </div>
    </div>
    
    <div class="card">
      <div class="card-title">Toggles</div>
      <div class="control-row">
        <span class="label">Toggle 1</span>
        <label class="toggle">
          <input type="checkbox" id="toggle1" onchange="sendUpdate('toggle1', this.checked ? 1 : 0)">
          <span class="toggle-slider"></span>
        </label>
      </div>
      <div class="control-row">
        <span class="label">Toggle 2</span>
        <label class="toggle">
          <input type="checkbox" id="toggle2" checked onchange="sendUpdate('toggle2', this.checked ? 1 : 0)">
          <span class="toggle-slider"></span>
        </label>
      </div>
    </div>
    
    <div class="card">
      <div class="card-title">Sensors</div>
      <div class="sensor-grid">
        <div class="sensor-item">
          <div class="sensor-value" id="temp">--</div>
          <div class="sensor-label">Temperature °C</div>
        </div>
        <div class="sensor-item">
          <div class="sensor-value" id="humidity">--</div>
          <div class="sensor-label">Humidity %</div>
        </div>
        <div class="sensor-item">
          <div class="sensor-value" id="uptime">--</div>
          <div class="sensor-label">Uptime</div>
        </div>
        <div class="sensor-item">
          <div class="sensor-value" id="clients">0</div>
          <div class="sensor-label">Clients</div>
        </div>
      </div>
    </div>
    
    <div class="status" id="status">Connecting...</div>
  </div>
  
  <script>
    let ws;
    let reconnectTimer;
    
    function connect() {
      ws = new WebSocket('ws://' + location.hostname + ':81/');
      
      ws.onopen = function() {
        document.getElementById('status').textContent = '🟢 Connected';
        document.getElementById('status').className = 'status connected';
        clearTimeout(reconnectTimer);
      };
      
      ws.onclose = function() {
        document.getElementById('status').textContent = '🔴 Disconnected - Reconnecting...';
        document.getElementById('status').className = 'status';
        reconnectTimer = setTimeout(connect, 2000);
      };
      
      ws.onmessage = function(evt) {
        try {
          const data = JSON.parse(evt.data);
          updateUI(data);
        } catch(e) {}
      };
    }
    
    function updateUI(data) {
      if (data.mode !== undefined) document.getElementById('mode').value = data.mode;
      if (data.brightness !== undefined) {
        document.getElementById('brightness').value = data.brightness;
        document.getElementById('brightness-val').textContent = data.brightness;
      }
      if (data.slider1 !== undefined) {
        document.getElementById('slider1').value = data.slider1;
        document.getElementById('slider1-val').textContent = data.slider1;
      }
      if (data.slider2 !== undefined) {
        document.getElementById('slider2').value = data.slider2;
        document.getElementById('slider2-val').textContent = data.slider2;
      }
      if (data.toggle1 !== undefined) document.getElementById('toggle1').checked = data.toggle1;
      if (data.toggle2 !== undefined) document.getElementById('toggle2').checked = data.toggle2;
      if (data.temp !== undefined) document.getElementById('temp').textContent = data.temp.toFixed(1);
      if (data.humidity !== undefined) document.getElementById('humidity').textContent = data.humidity.toFixed(0);
      if (data.uptime !== undefined) {
        const h = Math.floor(data.uptime / 3600);
        const m = Math.floor((data.uptime % 3600) / 60);
        const s = data.uptime % 60;
        document.getElementById('uptime').textContent = 
          String(h).padStart(2,'0') + ':' + String(m).padStart(2,'0') + ':' + String(s).padStart(2,'0');
      }
      if (data.clients !== undefined) document.getElementById('clients').textContent = data.clients;
    }
    
    function sendUpdate(key, value) {
      if (ws && ws.readyState === WebSocket.OPEN) {
        const msg = {};
        msg[key] = parseInt(value);
        ws.send(JSON.stringify(msg));
      }
      // Update local display
      if (key === 'brightness') document.getElementById('brightness-val').textContent = value;
      if (key === 'slider1') document.getElementById('slider1-val').textContent = value;
      if (key === 'slider2') document.getElementById('slider2-val').textContent = value;
    }
    
    connect();
  </script>
</body>
</html>
)rawliteral";

// ============================================================
// WebSocket Handler
// ============================================================
void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED:
      state.connectedClients++;
      state.dirty = true;
      break;
      
    case WStype_DISCONNECTED:
      if (state.connectedClients > 0) state.connectedClients--;
      break;
      
    case WStype_TEXT: {
      JsonDocument doc;
      DeserializationError err = deserializeJson(doc, payload, length);
      if (err) return;
      
      if (doc.containsKey("mode")) state.mode = doc["mode"];
      if (doc.containsKey("brightness")) state.brightness = doc["brightness"];
      if (doc.containsKey("slider1")) state.slider1 = doc["slider1"];
      if (doc.containsKey("slider2")) state.slider2 = doc["slider2"];
      if (doc.containsKey("toggle1")) state.toggle1 = doc["toggle1"];
      if (doc.containsKey("toggle2")) state.toggle2 = doc["toggle2"];
      state.dirty = true;
      break;
    }
  }
}

void broadcastState() {
  JsonDocument doc;
  doc["mode"] = state.mode;
  doc["brightness"] = state.brightness;
  doc["slider1"] = state.slider1;
  doc["slider2"] = state.slider2;
  doc["toggle1"] = state.toggle1;
  doc["toggle2"] = state.toggle2;
  doc["temp"] = state.temperature;
  doc["humidity"] = state.humidity;
  doc["uptime"] = state.uptime;
  doc["clients"] = state.connectedClients;
  
  String json;
  serializeJson(doc, json);
  webSocket.broadcastTXT(json);
}

// ============================================================
// OLED Drawing
// ============================================================
void drawOLED() {
  u8g2.clearBuffer();
  
  // Title bar
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 10, "SynthHead");
  
  // WiFi indicator
  char clientStr[8];
  snprintf(clientStr, sizeof(clientStr), "C:%d", state.connectedClients);
  u8g2.drawStr(100, 10, clientStr);
  
  // Horizontal line
  u8g2.drawHLine(0, 14, 128);
  
  // Menu items
  const int startY = 26;
  const int itemHeight = 16;
  
  for (int i = 0; i < MENU_ITEMS; i++) {
    int y = startY + i * itemHeight;
    
    // Highlight selected item
    if (i == menuIndex) {
      u8g2.drawBox(0, y - 10, 128, itemHeight);
      u8g2.setDrawColor(0);
    }
    
    // Draw label
    u8g2.drawStr(4, y, menuLabels[i]);
    
    // Draw value
    char valStr[16];
    int valX = 70;
    
    switch (i) {
      case 0: // Mode
        snprintf(valStr, sizeof(valStr), "%s", 
          state.mode == 0 ? "Normal" : (state.mode == 1 ? "Party" : "Sleep"));
        break;
      case 1: // Brightness
        snprintf(valStr, sizeof(valStr), "%d", state.brightness);
        break;
      case 2: // Slider 1
        snprintf(valStr, sizeof(valStr), "%d%%", state.slider1);
        break;
      case 3: // Slider 2
        snprintf(valStr, sizeof(valStr), "%d%%", state.slider2);
        break;
      case 4: // Toggle 1
        snprintf(valStr, sizeof(valStr), "%s", state.toggle1 ? "ON" : "OFF");
        break;
      case 5: // Toggle 2
        snprintf(valStr, sizeof(valStr), "%s", state.toggle2 ? "ON" : "OFF");
        break;
    }
    u8g2.drawStr(valX, y, valStr);
    
    if (i == menuIndex) {
      u8g2.setDrawColor(1);
    }
  }
  
  // Status bar at bottom
  u8g2.drawHLine(0, 116, 128);
  
  // IP Address
  u8g2.setFont(u8g2_font_5x7_tf);
  u8g2.drawStr(0, 126, WiFi.softAPIP().toString().c_str());
  
  // Uptime
  char upStr[12];
  uint32_t secs = state.uptime;
  snprintf(upStr, sizeof(upStr), "%02lu:%02lu:%02lu", secs/3600, (secs%3600)/60, secs%60);
  u8g2.drawStr(85, 126, upStr);
  
  u8g2.sendBuffer();
}

// ============================================================
// Setup
// ============================================================
void setup() {
  Serial.begin(115200);
  Serial.println("\n=== SynthHead OLED + Web Demo ===");
  
  // LED
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);
  
  // Encoder
  pinMode(ENC_A, INPUT_PULLUP);
  pinMode(ENC_B, INPUT_PULLUP);
  pinMode(ENC_BTN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(ENC_A), encoderISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENC_BTN), buttonISR, FALLING);
  
  // OLED - U8g2 handles SPI internally via constructor pins
  u8g2.begin();
  u8g2.setContrast(255);
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(20, 60, "Starting...");
  u8g2.sendBuffer();
  
  // WiFi AP
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(AP_IP, AP_IP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(AP_SSID, AP_PASS);
  Serial.printf("AP Started: %s\n", AP_SSID);
  Serial.printf("IP: %s\n", WiFi.softAPIP().toString().c_str());
  
  // DNS - Captive Portal
  dnsServer.start(DNS_PORT, "*", AP_IP);
  
  // HTTP Server
  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", INDEX_HTML);
  });
  server.onNotFound([]() {
    server.sendHeader("Location", "http://" + WiFi.softAPIP().toString());
    server.send(302);
  });
  server.begin();
  
  // WebSocket
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  
  Serial.println("Setup complete!");
}

// ============================================================
// Main Loop
// ============================================================
void loop() {
  static uint32_t lastUpdate = 0;
  static uint32_t lastBroadcast = 0;
  static uint32_t lastSensor = 0;
  static int lastEncoderPos = 0;
  
  // Handle DNS
  dnsServer.processNextRequest();
  
  // Handle HTTP
  server.handleClient();
  
  // Handle WebSocket
  webSocket.loop();
  
  // Handle encoder navigation (every 10ms)
  if (millis() - lastUpdate > 10) {
    lastUpdate = millis();
    
    // Encoder rotation
    int diff = encoderPos - lastEncoderPos;
    if (diff != 0) {
      lastEncoderPos = encoderPos;
      
      if (encoderPressed) {
        // Adjust selected value
        switch (menuIndex) {
          case 0: // Mode
            state.mode = (state.mode + (diff > 0 ? 1 : 2)) % 3;
            break;
          case 1: // Brightness
            state.brightness = constrain(state.brightness + diff * 5, 0, 255);
            break;
          case 2: // Slider 1
            state.slider1 = constrain(state.slider1 + diff, 0, 100);
            break;
          case 3: // Slider 2
            state.slider2 = constrain(state.slider2 + diff, 0, 100);
            break;
          case 4: // Toggle 1
            state.toggle1 = !state.toggle1;
            break;
          case 5: // Toggle 2
            state.toggle2 = !state.toggle2;
            break;
        }
        state.dirty = true;
      } else {
        // Navigate menu
        menuIndex = (menuIndex + diff + MENU_ITEMS) % MENU_ITEMS;
      }
    }
    
    // Button release
    static bool wasPressed = false;
    bool isPressed = (digitalRead(ENC_BTN) == LOW);
    if (wasPressed && !isPressed) {
      encoderPressed = !encoderPressed;
    }
    wasPressed = isPressed;
  }
  
  // Update sensors (every 1 second)
  if (millis() - lastSensor > 1000) {
    lastSensor = millis();
    state.uptime = millis() / 1000;
    
    // Simulate sensor data
    state.temperature = 22.0f + (float)(random(-20, 20)) / 10.0f;
    state.humidity = 45.0f + (float)(random(-10, 10));
    
    state.dirty = true;
  }
  
  // Broadcast state to WebSocket clients (every 200ms or when dirty)
  if (millis() - lastBroadcast > 200 || state.dirty) {
    if (state.connectedClients > 0) {
      broadcastState();
    }
    state.dirty = false;
    lastBroadcast = millis();
  }
  
  // Update OLED (every 50ms = 20 FPS)
  static uint32_t lastDraw = 0;
  if (millis() - lastDraw > 50) {
    lastDraw = millis();
    drawOLED();
  }
  
  // LED heartbeat
  digitalWrite(LED_PIN, (millis() / 500) % 2);
}
