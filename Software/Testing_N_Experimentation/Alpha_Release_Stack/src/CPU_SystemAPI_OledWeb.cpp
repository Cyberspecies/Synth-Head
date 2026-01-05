/*****************************************************************
 * @file CPU_SystemAPI_OledWeb.cpp
 * @brief OLED UI + Captive Portal Web Interface with Bidirectional Sync
 * 
 * This example demonstrates the complete SystemAPI UI framework:
 * - OLED 128x128 display with multi-scene UI
 * - WiFi captive portal with web interface
 * - Real-time bidirectional WebSocket synchronization
 * - Sensor integration (BME280, IMU, GPS)
 * 
 * Hardware:
 * - CPU ESP32-S3 with OLED display (SSD1327 or similar)
 * - BME280 environmental sensor
 * - MPU6050/ICM-20948 IMU
 * - GPS module (optional)
 * - Rotary encoder for navigation
 * 
 * @author ARCOS
 * @version 1.0
 *****************************************************************/

#include <Arduino.h>
#include <Wire.h>

// SystemAPI includes
#include "SystemAPI/SyncState.hpp"
#include "SystemAPI/UI/OledUI.hpp"
#include "SystemAPI/Web/CaptivePortal.hpp"

// Display driver (using U8g2 for OLED)
#include <U8g2lib.h>

using namespace SystemAPI;
using namespace SystemAPI::UI;
using namespace SystemAPI::Web;

// ============================================================
// Pin Definitions
// ============================================================
#define PIN_I2C_SDA     8
#define PIN_I2C_SCL     9

#define PIN_OLED_CS     10
#define PIN_OLED_DC     11
#define PIN_OLED_RST    12

// Rotary encoder
#define PIN_ENC_A       5
#define PIN_ENC_B       6
#define PIN_ENC_BTN     7

// LED
#define PIN_LED         48

// ============================================================
// Display Configuration
// ============================================================
// Using U8g2 for OLED display (128x128 SSD1327)
U8G2_SSD1327_EA_W128128_F_4W_HW_SPI oled(U8G2_R0, PIN_OLED_CS, PIN_OLED_DC, PIN_OLED_RST);

// Alternative: SSD1306 128x64
// U8G2_SSD1306_128X64_NONAME_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE);

// ============================================================
// Sensor Variables
// ============================================================
// Simulated sensor data (replace with real sensors)
float simTemp = 25.0f;
float simHumidity = 50.0f;
float simPressure = 1013.25f;
int16_t simAccelX = 0, simAccelY = 0, simAccelZ = 1000;
int16_t simGyroX = 0, simGyroY = 0, simGyroZ = 0;
float simLat = 0.0f, simLon = 0.0f;

// ============================================================
// Encoder State
// ============================================================
volatile int encoderPos = 0;
volatile bool encoderBtn = false;
int lastEncoderPos = 0;

// Encoder ISR
void IRAM_ATTR encoderISR() {
  static int8_t lookup[] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};
  static uint8_t encVal = 0;
  
  encVal = encVal << 2;
  encVal |= (digitalRead(PIN_ENC_A) << 1) | digitalRead(PIN_ENC_B);
  encoderPos += lookup[encVal & 0x0F];
}

// ============================================================
// Timing
// ============================================================
unsigned long lastUpdate = 0;
unsigned long lastSensorUpdate = 0;
unsigned long startTime = 0;
float fps = 0;
int frameCount = 0;
unsigned long lastFpsTime = 0;

// ============================================================
// Setup
// ============================================================
void setup() {
  Serial.begin(115200);
  Serial.println("\n\n=== SynthHead OLED + Web UI ===\n");
  
  // Initialize I2C
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.setClock(400000);
  
  // Initialize OLED display
  Serial.println("[OLED] Initializing...");
  oled.begin();
  oled.setContrast(200);
  oled.clearBuffer();
  oled.setFont(u8g2_font_6x10_tr);
  oled.drawStr(10, 30, "SynthHead");
  oled.drawStr(10, 45, "Initializing...");
  oled.sendBuffer();
  
  // Initialize encoder pins
  pinMode(PIN_ENC_A, INPUT_PULLUP);
  pinMode(PIN_ENC_B, INPUT_PULLUP);
  pinMode(PIN_ENC_BTN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_A), encoderISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_B), encoderISR, CHANGE);
  
  // Initialize LED
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);
  
  // Initialize OLED UI
  Serial.println("[UI] Initializing UI framework...");
  if (!OLED_UI.init(128, 128)) {
    Serial.println("[UI] ERROR: Failed to initialize UI!");
    while (1) { delay(100); }
  }
  Serial.println("[UI] UI initialized");
  
  // Initialize WiFi Captive Portal
  Serial.println("[WiFi] Starting captive portal...");
  oled.clearBuffer();
  oled.drawStr(10, 30, "Starting WiFi...");
  oled.sendBuffer();
  
  if (!CAPTIVE_PORTAL.init("SynthHead-AP", "")) {
    Serial.println("[WiFi] ERROR: Failed to start portal!");
  } else {
    auto& state = SYNC_STATE.state();
    Serial.printf("[WiFi] AP Started: %s\n", state.ssid);
    Serial.printf("[WiFi] IP: %s\n", state.ipAddress);
    Serial.println("[WiFi] Connect to WiFi and open browser for control panel");
  }
  
  // Set initial sync state
  auto& state = SYNC_STATE.state();
  state.mode = SystemMode::RUNNING;
  strncpy(state.statusText, "Running", sizeof(state.statusText));
  state.brightness = 128;
  state.displayEnabled = true;
  
  startTime = millis();
  lastFpsTime = millis();
  
  Serial.println("\n[Ready] System initialized!\n");
  Serial.println("=== Controls ===");
  Serial.println("Encoder: Navigate menus");
  Serial.println("Encoder Button: Select/Back");
  Serial.println("Web UI: http://192.168.4.1");
  Serial.println("================\n");
}

// ============================================================
// Sensor Update (Simulated)
// ============================================================
void updateSensors() {
  // Simulate sensor drift
  simTemp += (random(-10, 11) / 100.0f);
  simTemp = constrain(simTemp, 20.0f, 30.0f);
  
  simHumidity += (random(-20, 21) / 100.0f);
  simHumidity = constrain(simHumidity, 40.0f, 60.0f);
  
  simPressure += (random(-10, 11) / 10.0f);
  simPressure = constrain(simPressure, 1000.0f, 1030.0f);
  
  // Simulate IMU
  simAccelX = random(-100, 101);
  simAccelY = random(-100, 101);
  simAccelZ = 1000 + random(-50, 51);
  
  simGyroX = random(-50, 51);
  simGyroY = random(-50, 51);
  simGyroZ = random(-50, 51);
  
  // Update sync state
  SYNC_STATE.updateSensors(simTemp, simHumidity, simPressure);
  SYNC_STATE.updateIMU(simAccelX, simAccelY, simAccelZ, simGyroX, simGyroY, simGyroZ);
  
  // Simulated GPS
  bool gpsValid = random(0, 10) > 3;
  if (gpsValid) {
    simLat = 37.7749f + (random(-100, 101) / 10000.0f);
    simLon = -122.4194f + (random(-100, 101) / 10000.0f);
    SYNC_STATE.updateGPS(simLat, simLon, 10.0f, random(4, 12), true);
  } else {
    SYNC_STATE.updateGPS(0, 0, 0, 0, false);
  }
}

// ============================================================
// Update Stats
// ============================================================
void updateStats() {
  // Uptime
  uint32_t uptime = (millis() - startTime) / 1000;
  
  // Free heap
  uint32_t freeHeap = ESP.getFreeHeap();
  
  // CPU usage (estimated)
  float cpuUsage = 30.0f + (random(0, 30) / 10.0f);  // Simulated
  
  SYNC_STATE.updateStats(uptime, freeHeap, cpuUsage, fps);
}

// ============================================================
// Handle Encoder Input
// ============================================================
void handleEncoder() {
  // Check encoder rotation
  int delta = encoderPos - lastEncoderPos;
  if (delta != 0) {
    lastEncoderPos = encoderPos;
    
    if (delta > 0) {
      OLED_UI.navigateDown();
    } else {
      OLED_UI.navigateUp();
    }
    
    // Also send to UI manager for slider/value adjustment
    OLED_UI.encoderRotate(delta);
  }
  
  // Check button
  static bool lastBtn = false;
  bool btn = !digitalRead(PIN_ENC_BTN);  // Active low
  
  if (btn && !lastBtn) {
    // Button pressed
    OLED_UI.select();
  }
  lastBtn = btn;
}

// ============================================================
// Apply LED state from sync
// ============================================================
void applyLedState() {
  auto& state = SYNC_STATE.state();
  
  if (state.ledEnabled && state.ledColor > 0) {
    // Simple on/off for single LED
    digitalWrite(PIN_LED, HIGH);
  } else {
    digitalWrite(PIN_LED, LOW);
  }
}

// ============================================================
// Render OLED Display
// ============================================================
void renderDisplay() {
  // Get UI framebuffer
  const uint8_t* buffer = OLED_UI.getBuffer();
  
  // Clear OLED buffer
  oled.clearBuffer();
  
  // Copy UI buffer to OLED
  // The UI uses MONO_1BPP format - need to convert to U8g2 format
  for (int y = 0; y < 128; y++) {
    for (int x = 0; x < 128; x++) {
      // UI buffer format: 1 bit per pixel, MSB first
      int byteIdx = (y * 128 + x) / 8;
      int bitIdx = 7 - (x % 8);
      
      if (buffer[byteIdx] & (1 << bitIdx)) {
        oled.setDrawColor(1);
        oled.drawPixel(x, y);
      }
    }
  }
  
  // Send to display
  oled.sendBuffer();
}

// ============================================================
// Alternative: Direct U8g2 Rendering (More Efficient)
// ============================================================
void renderDisplayDirect() {
  oled.clearBuffer();
  
  auto& state = SYNC_STATE.state();
  int y = 0;
  
  // Header
  oled.setDrawColor(1);
  oled.drawBox(0, 0, 128, 14);
  oled.setDrawColor(0);
  oled.setFont(u8g2_font_6x10_tr);
  oled.drawStr(4, 10, "SynthHead");
  
  // WiFi icon (if connected)
  oled.setDrawColor(0);
  if (CAPTIVE_PORTAL.getClientCount() > 0) {
    oled.drawStr(100, 10, "WiFi");
  }
  
  oled.setDrawColor(1);
  y = 20;
  
  // Status
  oled.setFont(u8g2_font_5x7_tr);
  char buf[32];
  
  snprintf(buf, sizeof(buf), "Mode: %s", 
           state.mode == SystemMode::RUNNING ? "RUN" : "IDLE");
  oled.drawStr(4, y, buf);
  y += 10;
  
  snprintf(buf, sizeof(buf), "Temp: %.1fC", state.temperature);
  oled.drawStr(4, y, buf);
  y += 10;
  
  snprintf(buf, sizeof(buf), "Hum: %.0f%%", state.humidity);
  oled.drawStr(4, y, buf);
  y += 10;
  
  snprintf(buf, sizeof(buf), "Clients: %d", state.wifiClients);
  oled.drawStr(4, y, buf);
  y += 12;
  
  // Sliders visualization
  oled.drawStr(4, y, "Brightness:");
  y += 8;
  oled.drawFrame(4, y, 100, 8);
  oled.drawBox(4, y, (state.brightness * 100) / 255, 8);
  y += 12;
  
  oled.drawStr(4, y, "Slider1:");
  y += 8;
  oled.drawFrame(4, y, 100, 8);
  oled.drawBox(4, y, state.slider1Value, 8);
  y += 12;
  
  // Toggles
  snprintf(buf, sizeof(buf), "[%c] LED  [%c] Display  [%c] Auto",
           state.toggle1 ? 'X' : ' ',
           state.toggle2 ? 'X' : ' ',
           state.toggle3 ? 'X' : ' ');
  oled.drawStr(4, y, buf);
  y += 12;
  
  // GPS status
  if (state.gpsValid) {
    snprintf(buf, sizeof(buf), "GPS: %.4f, %.4f", state.latitude, state.longitude);
  } else {
    strcpy(buf, "GPS: No Fix");
  }
  oled.drawStr(4, y, buf);
  y += 10;
  
  // Footer
  snprintf(buf, sizeof(buf), "FPS:%.0f Heap:%dK", fps, state.freeHeap / 1024);
  oled.drawStr(4, 124, buf);
  
  oled.sendBuffer();
}

// ============================================================
// Main Loop
// ============================================================
void loop() {
  unsigned long now = millis();
  
  // Handle captive portal (high priority)
  CAPTIVE_PORTAL.update();
  
  // Handle encoder input
  handleEncoder();
  
  // UI update at 60 FPS
  if (now - lastUpdate >= 16) {  // ~60 FPS
    float dt = (now - lastUpdate) / 1000.0f;
    lastUpdate = now;
    
    // Update UI
    OLED_UI.update(dt);
    
    // Render to display
    // Option 1: Using UI framework buffer
    // OLED_UI.render();
    // renderDisplay();
    
    // Option 2: Direct rendering (more efficient for now)
    renderDisplayDirect();
    
    // Apply hardware state
    applyLedState();
    
    // FPS calculation
    frameCount++;
    if (now - lastFpsTime >= 1000) {
      fps = frameCount * 1000.0f / (now - lastFpsTime);
      frameCount = 0;
      lastFpsTime = now;
    }
  }
  
  // Update sensors at 10 Hz
  if (now - lastSensorUpdate >= 100) {
    lastSensorUpdate = now;
    updateSensors();
    updateStats();
  }
  
  // Small yield for WiFi/system
  yield();
}
