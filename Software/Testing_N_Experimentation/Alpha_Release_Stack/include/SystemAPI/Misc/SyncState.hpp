/*****************************************************************
 * @file SyncState.hpp
 * @brief Shared state for bidirectional OLED/Web UI synchronization
 * 
 * This defines the shared data model that both the OLED UI and
 * the web captive portal display and modify. Changes from either
 * side are synchronized via WebSocket.
 * 
 * @author ARCOS
 * @version 1.0
 *****************************************************************/

#pragma once

#include <cstdint>
#include <cstring>
#include <functional>

namespace SystemAPI {

/**
 * @brief System mode enumeration
 */
enum class SystemMode : uint8_t {
  IDLE = 0,
  RUNNING,
  PAUSED,
  ERROR
};

/**
 * @brief Shared synchronized state between OLED and Web UI
 */
struct SyncState {
  // System
  SystemMode mode = SystemMode::IDLE;
  char statusText[32] = "Ready";
  
  // Display settings
  uint8_t brightness = 128;
  bool displayEnabled = true;
  uint8_t animationSpeed = 50;
  
  // Sensor readings (read-only from web, display only)
  float temperature = 0.0f;
  float humidity = 0.0f;
  float pressure = 0.0f;
  int16_t accelX = 0, accelY = 0, accelZ = 0;
  int16_t gyroX = 0, gyroY = 0, gyroZ = 0;
  
  // GPS data
  float latitude = 0.0f;
  float longitude = 0.0f;
  float altitude = 0.0f;
  float gpsSpeed = 0.0f;       // Speed in km/h
  float gpsHeading = 0.0f;     // Course over ground (degrees)
  float gpsHdop = 99.9f;       // Horizontal dilution of precision
  uint8_t satellites = 0;
  bool gpsValid = false;
  uint8_t gpsHour = 0;
  uint8_t gpsMinute = 0;
  uint8_t gpsSecond = 0;
  uint8_t gpsDay = 0;
  uint8_t gpsMonth = 0;
  uint16_t gpsYear = 0;
  
  // Connection status
  bool gpuConnected = false;
  
  // GPU stats
  float gpuFps = 0.0f;           // GPU frames per second
  uint32_t gpuFreeHeap = 0;      // GPU free heap bytes
  uint32_t gpuMinHeap = 0;       // GPU minimum free heap
  uint8_t gpuLoad = 0;           // GPU load percent (0-100)
  uint32_t gpuTotalFrames = 0;   // Total GPU frames since boot
  uint32_t gpuUptime = 0;        // GPU uptime in ms
  bool gpuHub75Ok = false;       // HUB75 display status
  bool gpuOledOk = false;        // OLED display status
  
  // Microphone
  bool micConnected = false;
  uint8_t micLevel = 0;
  float micDb = -60.0f;  // dB level (-60 to 0)
  
  // User controls (bidirectional)
  bool ledEnabled = false;
  uint8_t ledColor = 0;  // 0=off, 1=red, 2=green, 3=blue, 4=white
  int16_t fanSpeed = 0;  // 0-100%
  
  // Menu selection
  uint8_t selectedMenuItem = 0;
  uint8_t selectedTab = 0;
  
  // Sliders/values
  int16_t slider1Value = 50;
  int16_t slider2Value = 50;
  int16_t slider3Value = 50;
  
  // Checkboxes/toggles
  bool toggle1 = false;
  bool toggle2 = false;
  bool toggle3 = false;
  
  // Text input
  char inputText[64] = "";
  
  // Dropdown selection
  uint8_t dropdown1Selection = 0;
  uint8_t dropdown2Selection = 0;
  
  // Network info
  char ssid[32] = "SynthHead-AP";
  char ipAddress[16] = "192.168.4.1";
  uint8_t wifiClients = 0;
  
  // External WiFi (Station Mode) Settings
  bool extWifiEnabled = false;       // System is enabled (persisted)
  bool extWifiConnected = false;     // Kill switch - actually connect
  bool extWifiIsConnected = false;   // Connection status
  char extWifiSSID[32] = "";         // Target network SSID
  char extWifiPassword[64] = "";     // Target network password
  char extWifiIP[16] = "";           // IP assigned when connected
  int8_t extWifiRSSI = -100;         // Signal strength
  
  // Authentication for external WiFi mode
  bool authEnabled = false;          // Require login when on external WiFi
  char authUsername[32] = "admin";   // Site-wide username
  char authPassword[32] = "";        // Site-wide password (hashed in practice)
  char authSessionToken[65] = "";    // Current valid session token
  
  // Stats
  uint32_t uptime = 0;
  uint32_t freeHeap = 0;
  float cpuUsage = 0.0f;
  float fps = 0.0f;
  
  // Dirty flags for change detection
  uint32_t changeFlags = 0;
  uint32_t version = 0;  // Incremented on each change
  
  // Change flag bits
  static constexpr uint32_t FLAG_MODE = (1 << 0);
  static constexpr uint32_t FLAG_BRIGHTNESS = (1 << 1);
  static constexpr uint32_t FLAG_LED = (1 << 2);
  static constexpr uint32_t FLAG_FAN = (1 << 3);
  static constexpr uint32_t FLAG_SLIDERS = (1 << 4);
  static constexpr uint32_t FLAG_TOGGLES = (1 << 5);
  static constexpr uint32_t FLAG_MENU = (1 << 6);
  static constexpr uint32_t FLAG_TEXT = (1 << 7);
  static constexpr uint32_t FLAG_DROPDOWN = (1 << 8);
  static constexpr uint32_t FLAG_SENSORS = (1 << 9);
  static constexpr uint32_t FLAG_GPS = (1 << 10);
  static constexpr uint32_t FLAG_STATS = (1 << 11);
  static constexpr uint32_t FLAG_ALL = 0xFFFFFFFF;
  
  void markChanged(uint32_t flags) {
    changeFlags |= flags;
    version++;
  }
  
  void clearChanged() {
    changeFlags = 0;
  }
  
  bool hasChanges() const {
    return changeFlags != 0;
  }
};

/**
 * @brief Sync State Manager - singleton for shared state
 */
class SyncStateManager {
public:
  static SyncStateManager& instance() {
    static SyncStateManager inst;
    return inst;
  }
  
  SyncState& state() { return state_; }
  const SyncState& state() const { return state_; }
  
  // Callbacks for state changes
  using ChangeCallback = std::function<void(uint32_t flags)>;
  
  void setOnChange(ChangeCallback cb) { onChange_ = cb; }
  
  void notifyChange(uint32_t flags) {
    state_.markChanged(flags);
    if (onChange_) {
      onChange_(flags);
    }
  }
  
  // Convenience setters with change notification
  void setMode(SystemMode mode) {
    state_.mode = mode;
    notifyChange(SyncState::FLAG_MODE);
  }
  
  void setBrightness(uint8_t val) {
    state_.brightness = val;
    notifyChange(SyncState::FLAG_BRIGHTNESS);
  }
  
  void setLedEnabled(bool val) {
    state_.ledEnabled = val;
    notifyChange(SyncState::FLAG_LED);
  }
  
  void setLedColor(uint8_t val) {
    state_.ledColor = val;
    notifyChange(SyncState::FLAG_LED);
  }
  
  void setFanSpeed(int16_t val) {
    state_.fanSpeed = val;
    notifyChange(SyncState::FLAG_FAN);
  }
  
  void setSlider1(int16_t val) {
    state_.slider1Value = val;
    notifyChange(SyncState::FLAG_SLIDERS);
  }
  
  void setSlider2(int16_t val) {
    state_.slider2Value = val;
    notifyChange(SyncState::FLAG_SLIDERS);
  }
  
  void setSlider3(int16_t val) {
    state_.slider3Value = val;
    notifyChange(SyncState::FLAG_SLIDERS);
  }
  
  void setToggle1(bool val) {
    state_.toggle1 = val;
    notifyChange(SyncState::FLAG_TOGGLES);
  }
  
  void setToggle2(bool val) {
    state_.toggle2 = val;
    notifyChange(SyncState::FLAG_TOGGLES);
  }
  
  void setToggle3(bool val) {
    state_.toggle3 = val;
    notifyChange(SyncState::FLAG_TOGGLES);
  }
  
  void setSelectedTab(uint8_t tab) {
    state_.selectedTab = tab;
    notifyChange(SyncState::FLAG_MENU);
  }
  
  void setDropdown1(uint8_t val) {
    state_.dropdown1Selection = val;
    notifyChange(SyncState::FLAG_DROPDOWN);
  }
  
  void setDropdown2(uint8_t val) {
    state_.dropdown2Selection = val;
    notifyChange(SyncState::FLAG_DROPDOWN);
  }
  
  // Update sensor data (typically from sensors)
  void updateSensors(float temp, float hum, float pres) {
    state_.temperature = temp;
    state_.humidity = hum;
    state_.pressure = pres;
    notifyChange(SyncState::FLAG_SENSORS);
  }
  
  void updateIMU(int16_t ax, int16_t ay, int16_t az, int16_t gx, int16_t gy, int16_t gz) {
    state_.accelX = ax; state_.accelY = ay; state_.accelZ = az;
    state_.gyroX = gx; state_.gyroY = gy; state_.gyroZ = gz;
    notifyChange(SyncState::FLAG_SENSORS);
  }
  
  void updateGPS(float lat, float lon, float alt, uint8_t sats, bool valid,
                 float speed = 0.0f, float heading = 0.0f, float hdop = 99.9f,
                 uint8_t hour = 0, uint8_t minute = 0, uint8_t second = 0,
                 uint8_t day = 0, uint8_t month = 0, uint16_t year = 0) {
    state_.latitude = lat;
    state_.longitude = lon;
    state_.altitude = alt;
    state_.satellites = sats;
    state_.gpsValid = valid;
    state_.gpsSpeed = speed;
    state_.gpsHeading = heading;
    state_.gpsHdop = hdop;
    state_.gpsHour = hour;
    state_.gpsMinute = minute;
    state_.gpsSecond = second;
    state_.gpsDay = day;
    state_.gpsMonth = month;
    state_.gpsYear = year;
    notifyChange(SyncState::FLAG_GPS);
  }
  
  void updateStats(uint32_t uptime, uint32_t heap, float cpu, float fps) {
    state_.uptime = uptime;
    state_.freeHeap = heap;
    state_.cpuUsage = cpu;
    state_.fps = fps;
    notifyChange(SyncState::FLAG_STATS);
  }
  
private:
  SyncStateManager() = default;
  SyncState state_;
  ChangeCallback onChange_;
};

// Convenience macro
#define SYNC_STATE SyncStateManager::instance()

} // namespace SystemAPI
