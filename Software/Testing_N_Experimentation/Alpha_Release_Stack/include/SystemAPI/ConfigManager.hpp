/*****************************************************************
 * @file ConfigManager.hpp
 * @brief Configuration Management - Hardware config abstraction
 * 
 * Manages hardware and system configuration so the application layer
 * doesn't need to know about pins, addresses, or hardware specifics.
 * 
 * Features:
 * - Hardware pin configuration (CPU and GPU)
 * - Device addresses (I2C, SPI, UART)
 * - Display configurations
 * - Communication settings
 * - Persistent configuration storage
 * - Runtime configuration changes
 * - Configuration profiles
 * 
 * @author ARCOS
 * @version 1.0
 *****************************************************************/

#pragma once

#include <cstdint>
#include <cstring>
#include <functional>

namespace SystemAPI {
namespace Config {

// ============================================================
// Pin Configuration Types
// ============================================================

/**
 * @brief GPIO pin type (use -1 for unused)
 */
using Pin = int8_t;
constexpr Pin PIN_UNUSED = -1;

/**
 * @brief I2C device configuration
 */
struct I2CConfig {
  Pin sda = PIN_UNUSED;
  Pin scl = PIN_UNUSED;
  uint32_t frequency = 400000;  ///< Hz
  uint8_t port = 0;             ///< I2C port number
};

/**
 * @brief SPI device configuration
 */
struct SPIConfig {
  Pin mosi = PIN_UNUSED;
  Pin miso = PIN_UNUSED;
  Pin sck = PIN_UNUSED;
  Pin cs = PIN_UNUSED;
  uint32_t frequency = 10000000; ///< Hz
  uint8_t mode = 0;              ///< SPI mode 0-3
  uint8_t port = 0;              ///< SPI port number
};

/**
 * @brief UART configuration
 */
struct UARTConfig {
  Pin tx = PIN_UNUSED;
  Pin rx = PIN_UNUSED;
  uint32_t baud = 115200;
  uint8_t port = 0;              ///< UART port number
  uint8_t dataBits = 8;
  uint8_t stopBits = 1;
  uint8_t parity = 0;            ///< 0=none, 1=odd, 2=even
};

/**
 * @brief I2S (audio) configuration
 */
struct I2SConfig {
  Pin ws = PIN_UNUSED;           ///< Word select (LRCLK)
  Pin bck = PIN_UNUSED;          ///< Bit clock
  Pin dataOut = PIN_UNUSED;      ///< Data output
  Pin dataIn = PIN_UNUSED;       ///< Data input
  Pin mclk = PIN_UNUSED;         ///< Master clock (optional)
  uint32_t sampleRate = 44100;
  uint8_t bitsPerSample = 16;
  uint8_t port = 0;
};

/**
 * @brief PWM output configuration
 */
struct PWMConfig {
  Pin pin = PIN_UNUSED;
  uint32_t frequency = 25000;    ///< Hz
  uint8_t resolution = 8;        ///< Bits
  uint8_t channel = 0;
};

/**
 * @brief Button/input configuration
 */
struct ButtonConfig {
  Pin pin = PIN_UNUSED;
  bool activeLow = true;         ///< Active low (pull-up)
  bool enablePullup = true;
  uint16_t debounceMs = 20;
};

// ============================================================
// Device Configurations
// ============================================================

/**
 * @brief IMU (ICM20948) configuration
 */
struct IMUConfig {
  uint8_t i2cAddress = 0x68;
  uint8_t i2cPort = 0;
  float accelScale = 4.0f;       ///< ±2, ±4, ±8, ±16 g
  float gyroScale = 500.0f;      ///< ±250, ±500, ±1000, ±2000 dps
  uint16_t sampleRate = 100;     ///< Hz
  bool enableMag = true;         ///< Enable magnetometer
};

/**
 * @brief Environmental sensor (BME280) configuration
 */
struct EnvironmentalConfig {
  uint8_t i2cAddress = 0x76;     ///< 0x76 or 0x77
  uint8_t i2cPort = 0;
  uint8_t oversampleTemp = 1;    ///< 1, 2, 4, 8, 16
  uint8_t oversamplePressure = 1;
  uint8_t oversampleHumidity = 1;
  float seaLevelPressure = 1013.25f; ///< hPa for altitude calc
};

/**
 * @brief GPS configuration
 */
struct GPSConfig {
  UARTConfig uart;
  uint16_t updateRate = 1;       ///< Hz
  bool enableGLONASS = true;
  bool enableGalileo = false;
};

/**
 * @brief Microphone configuration
 */
struct MicrophoneConfig {
  I2SConfig i2s;
  uint8_t gain = 24;             ///< dB
  bool agcEnabled = false;       ///< Auto gain control
};

/**
 * @brief Display configuration
 */
struct DisplayConfig {
  uint16_t width = 128;
  uint16_t height = 32;
  uint8_t brightness = 100;      ///< 0-100%
  uint8_t rotation = 0;          ///< 0, 90, 180, 270
  bool flipHorizontal = false;
  bool flipVertical = false;
};

/**
 * @brief HUB75 matrix configuration
 */
struct HUB75Config : public DisplayConfig {
  Pin r1 = PIN_UNUSED, g1 = PIN_UNUSED, b1 = PIN_UNUSED;
  Pin r2 = PIN_UNUSED, g2 = PIN_UNUSED, b2 = PIN_UNUSED;
  Pin a = PIN_UNUSED, b = PIN_UNUSED, c = PIN_UNUSED, d = PIN_UNUSED, e = PIN_UNUSED;
  Pin clk = PIN_UNUSED, lat = PIN_UNUSED, oe = PIN_UNUSED;
  uint8_t scanRate = 16;         ///< 1/16, 1/32, etc.
  uint8_t colorDepth = 8;        ///< Bits per color
};

/**
 * @brief OLED display configuration
 */
struct OLEDConfig : public DisplayConfig {
  uint8_t i2cAddress = 0x3C;
  uint8_t i2cPort = 0;
  uint8_t contrast = 255;
  bool invertDisplay = false;
};

// ============================================================
// Full Hardware Configuration
// ============================================================

/**
 * @brief CPU hardware configuration
 */
struct CPUHardwareConfig {
  // Communication
  I2CConfig i2c;
  UARTConfig gpuUart;            ///< CPU-GPU link
  UARTConfig debugUart;          ///< Debug output
  UARTConfig gpsUart;            ///< GPS module
  I2SConfig microphone;
  
  // Buttons
  ButtonConfig buttonA;
  ButtonConfig buttonB;
  ButtonConfig buttonC;
  ButtonConfig buttonD;
  
  // Sensors
  IMUConfig imu;
  EnvironmentalConfig environmental;
  GPSConfig gps;
  MicrophoneConfig mic;
  
  // PWM outputs
  PWMConfig fan1;
  PWMConfig fan2;
  
  // Storage
  SPIConfig sdCard;
};

/**
 * @brief GPU hardware configuration
 */
struct GPUHardwareConfig {
  // Displays
  HUB75Config hub75;
  OLEDConfig oled;
  
  // Communication
  UARTConfig cpuUart;            ///< CPU-GPU link
  I2CConfig i2c;                 ///< For OLED
};

/**
 * @brief System configuration
 */
struct SystemConfig {
  char deviceName[32] = "ARCOS-Device";
  char firmwareVersion[16] = "1.0.0";
  
  // Network
  char wifiSSID[32] = "";
  char wifiPassword[64] = "";
  char apSSID[32] = "ARCOS-AP";
  char apPassword[64] = "arcos123";
  uint16_t webServerPort = 80;
  
  // Bluetooth
  char btName[32] = "ARCOS-BT";
  bool btEnabled = false;
  
  // Logging
  uint8_t logLevel = 2;          ///< 0=off, 1=error, 2=warn, 3=info, 4=debug
  bool logToSerial = true;
  bool logToFile = false;
  
  // Performance
  uint8_t targetFps = 30;
  bool vsyncEnabled = true;
};

// ============================================================
// Configuration Profiles
// ============================================================

/**
 * @brief Configuration profile ID
 */
enum class Profile : uint8_t {
  DEFAULT,
  LOW_POWER,
  PERFORMANCE,
  DEBUG,
  CUSTOM_1,
  CUSTOM_2,
  CUSTOM_3,
  PROFILE_COUNT
};

inline const char* getProfileName(Profile profile) {
  switch (profile) {
    case Profile::DEFAULT:     return "Default";
    case Profile::LOW_POWER:   return "Low Power";
    case Profile::PERFORMANCE: return "Performance";
    case Profile::DEBUG:       return "Debug";
    case Profile::CUSTOM_1:    return "Custom 1";
    case Profile::CUSTOM_2:    return "Custom 2";
    case Profile::CUSTOM_3:    return "Custom 3";
    default:                   return "Unknown";
  }
}

// ============================================================
// Configuration Manager
// ============================================================

/**
 * @brief Singleton configuration manager
 * 
 * @example
 * ```cpp
 * auto& config = Config::Manager::instance();
 * 
 * // Get pin for button A (app doesn't need to know the actual pin)
 * auto buttonPin = config.getCPU().buttonA.pin;
 * 
 * // Get display dimensions
 * auto width = config.getGPU().hub75.width;
 * auto height = config.getGPU().hub75.height;
 * 
 * // Change settings
 * config.getSystem().targetFps = 60;
 * config.save();
 * ```
 */
class Manager {
public:
  static Manager& instance() {
    static Manager inst;
    return inst;
  }
  
  // ---- Initialization ----
  
  bool initialize(const char* configPath = nullptr) {
    // Set up default configurations
    setDefaultConfig();
    
    // Load from file if path provided
    if (configPath && strlen(configPath) > 0) {
      load(configPath);
    }
    
    initialized_ = true;
    return true;
  }
  
  void shutdown() {
    initialized_ = false;
  }
  
  // ---- Configuration Access ----
  
  CPUHardwareConfig& getCPU() { return cpuConfig_; }
  const CPUHardwareConfig& getCPU() const { return cpuConfig_; }
  
  GPUHardwareConfig& getGPU() { return gpuConfig_; }
  const GPUHardwareConfig& getGPU() const { return gpuConfig_; }
  
  SystemConfig& getSystem() { return systemConfig_; }
  const SystemConfig& getSystem() const { return systemConfig_; }
  
  // ---- Profile Management ----
  
  Profile getCurrentProfile() const { return currentProfile_; }
  
  bool loadProfile(Profile profile) {
    currentProfile_ = profile;
    applyProfile(profile);
    return true;
  }
  
  // ---- Persistence ----
  
  bool load(const char* path) {
    // Implementation would load from file/NVS
    (void)path;
    return true;
  }
  
  bool save(const char* path = nullptr) {
    // Implementation would save to file/NVS
    (void)path;
    return true;
  }
  
  // ---- Validation ----
  
  bool validate() const {
    // Check for obvious configuration errors
    if (cpuConfig_.gpuUart.tx == PIN_UNUSED || cpuConfig_.gpuUart.rx == PIN_UNUSED) {
      return false;  // GPU link required
    }
    return true;
  }
  
  bool isInitialized() const { return initialized_; }
  
  // ---- Hardware Queries (convenience) ----
  
  // These let the app layer query capabilities without knowing hardware
  
  bool hasIMU() const { return cpuConfig_.imu.i2cAddress != 0; }
  bool hasGPS() const { return cpuConfig_.gpsUart.tx != PIN_UNUSED; }
  bool hasEnvironmental() const { return cpuConfig_.environmental.i2cAddress != 0; }
  bool hasMicrophone() const { return cpuConfig_.microphone.i2s.dataIn != PIN_UNUSED; }
  bool hasWiFi() const { return strlen(systemConfig_.wifiSSID) > 0; }
  bool hasBluetooth() const { return systemConfig_.btEnabled; }
  bool hasSDCard() const { return cpuConfig_.sdCard.cs != PIN_UNUSED; }
  
  int getButtonCount() const {
    int count = 0;
    if (cpuConfig_.buttonA.pin != PIN_UNUSED) count++;
    if (cpuConfig_.buttonB.pin != PIN_UNUSED) count++;
    if (cpuConfig_.buttonC.pin != PIN_UNUSED) count++;
    if (cpuConfig_.buttonD.pin != PIN_UNUSED) count++;
    return count;
  }
  
  int getDisplayWidth() const { return gpuConfig_.hub75.width; }
  int getDisplayHeight() const { return gpuConfig_.hub75.height; }
  int getOLEDWidth() const { return gpuConfig_.oled.width; }
  int getOLEDHeight() const { return gpuConfig_.oled.height; }
  
private:
  Manager() { setDefaultConfig(); }
  Manager(const Manager&) = delete;
  Manager& operator=(const Manager&) = delete;
  
  bool initialized_ = false;
  Profile currentProfile_ = Profile::DEFAULT;
  
  CPUHardwareConfig cpuConfig_;
  GPUHardwareConfig gpuConfig_;
  SystemConfig systemConfig_;
  
  void setDefaultConfig() {
    // ---- CPU Configuration (based on HAL definitions) ----
    
    // I2C
    cpuConfig_.i2c.sda = 9;
    cpuConfig_.i2c.scl = 10;
    cpuConfig_.i2c.frequency = 400000;
    cpuConfig_.i2c.port = 0;
    
    // GPU UART
    cpuConfig_.gpuUart.tx = 12;
    cpuConfig_.gpuUart.rx = 11;
    cpuConfig_.gpuUart.baud = 10000000;  // 10 Mbps
    cpuConfig_.gpuUart.port = 1;
    
    // GPS UART
    cpuConfig_.gpsUart.tx = 43;
    cpuConfig_.gpsUart.rx = 44;
    cpuConfig_.gpsUart.baud = 9600;
    cpuConfig_.gpsUart.port = 2;
    
    // Buttons
    cpuConfig_.buttonA = {5, true, true, 20};
    cpuConfig_.buttonB = {6, true, true, 20};
    cpuConfig_.buttonC = {7, true, true, 20};
    cpuConfig_.buttonD = {15, true, true, 20};
    
    // IMU
    cpuConfig_.imu.i2cAddress = 0x68;
    cpuConfig_.imu.i2cPort = 0;
    cpuConfig_.imu.accelScale = 4.0f;
    cpuConfig_.imu.gyroScale = 500.0f;
    
    // Environmental
    cpuConfig_.environmental.i2cAddress = 0x76;
    cpuConfig_.environmental.i2cPort = 0;
    
    // Microphone
    cpuConfig_.microphone.i2s.ws = 42;
    cpuConfig_.microphone.i2s.bck = 40;
    cpuConfig_.microphone.i2s.dataIn = 2;
    cpuConfig_.microphone.i2s.sampleRate = 16000;
    
    // Fans
    cpuConfig_.fan1 = {17, 25000, 8, 0};
    cpuConfig_.fan2 = {36, 25000, 8, 1};
    
    // SD Card
    cpuConfig_.sdCard.mosi = 47;
    cpuConfig_.sdCard.miso = 14;
    cpuConfig_.sdCard.sck = 21;
    cpuConfig_.sdCard.cs = 48;
    
    // ---- GPU Configuration ----
    
    // HUB75 display
    gpuConfig_.hub75.width = 128;
    gpuConfig_.hub75.height = 32;
    gpuConfig_.hub75.brightness = 100;
    gpuConfig_.hub75.colorDepth = 8;
    
    // OLED display
    gpuConfig_.oled.width = 128;
    gpuConfig_.oled.height = 128;
    gpuConfig_.oled.i2cAddress = 0x3C;
    gpuConfig_.oled.contrast = 255;
    
    // CPU UART on GPU side
    gpuConfig_.cpuUart.tx = 12;
    gpuConfig_.cpuUart.rx = 13;
    gpuConfig_.cpuUart.baud = 10000000;
  }
  
  void applyProfile(Profile profile) {
    switch (profile) {
      case Profile::LOW_POWER:
        systemConfig_.targetFps = 15;
        cpuConfig_.imu.sampleRate = 50;
        break;
        
      case Profile::PERFORMANCE:
        systemConfig_.targetFps = 60;
        cpuConfig_.imu.sampleRate = 200;
        gpuConfig_.hub75.brightness = 100;
        break;
        
      case Profile::DEBUG:
        systemConfig_.logLevel = 4;  // Debug
        systemConfig_.logToSerial = true;
        break;
        
      default:
        // Use defaults
        break;
    }
  }
};

// ============================================================
// Convenience Functions
// ============================================================

/**
 * @brief Get a reference to the config manager
 */
inline Manager& get() { return Manager::instance(); }

/**
 * @brief Quick access to CPU config
 */
inline const CPUHardwareConfig& cpu() { return Manager::instance().getCPU(); }

/**
 * @brief Quick access to GPU config
 */
inline const GPUHardwareConfig& gpu() { return Manager::instance().getGPU(); }

/**
 * @brief Quick access to system config
 */
inline SystemConfig& system() { return Manager::instance().getSystem(); }

} // namespace Config
} // namespace SystemAPI
