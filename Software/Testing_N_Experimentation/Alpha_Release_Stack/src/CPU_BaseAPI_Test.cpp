/*****************************************************************
 * File:      CPU_BaseAPI_Test.cpp
 * Category:  Main Application (Base API Test)
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    CPU-side Base System API test application.
 *    Tests:
 *    - Telemetry processor (sensor fusion)
 *    - Communication protocol (packet build/parse)
 *    - LED manager
 *    - System state management
 * 
 * Hardware (COM 15):
 *    - ESP32-S3 (CPU)
 *    - I2C: SDA=GPIO9, SCL=GPIO10
 *      - ICM20948 IMU @ 0x68
 *      - BME280 Environmental @ 0x76
 *    - UART to GPU: RX=GPIO11, TX=GPIO12 @ 10Mbps
 *    - LED Strips: SK6812 RGBW
 * 
 * Usage:
 *    Build with CPU_BaseAPI_Test environment in platformio.ini
 *****************************************************************/

#include <Arduino.h>
#include <cmath>

// HAL Layer
#include "HAL/Hal.hpp"
#include "HAL/ESP32/Esp32Hal.hpp"

// Base System API
#include "BaseAPI/BaseSystemAPI.hpp"

using namespace arcos::hal;
using namespace arcos::hal::esp32;
using namespace arcos::hal::pins;
using namespace arcos::base;

// ============================================================
// Configuration
// ============================================================

static constexpr const char* TAG = "CPU_BASE_TEST";

// ============================================================
// HAL Instances (from previous test)
// ============================================================

Esp32HalLog hal_log;
Esp32HalErrorHandler hal_error(&hal_log);
Esp32HalSystemTimer hal_timer;
Esp32HalGpio hal_gpio(&hal_log);
Esp32HalI2c hal_i2c(&hal_log);
Esp32HalUart hal_uart(&hal_log);
Esp32HalImu* hal_imu = nullptr;
Esp32HalEnvironmental* hal_env = nullptr;
Esp32HalLedStrip hal_led_left(&hal_log);
Esp32HalLedStrip hal_led_right(&hal_log);
Esp32HalLedStrip hal_led_tongue(&hal_log);
Esp32HalLedStrip hal_led_scale(&hal_log);

// Global logger
IHalLog* g_hal_log = nullptr;

// ============================================================
// Base API Components
// ============================================================

// Telemetry processor
TelemetryProcessor telemetry;

// System manager (with Arduino timing)
class ArduinoSystemManager : public SystemManager{
protected:
  Timestamp getCurrentTime() const override{
    return millis();
  }
};
ArduinoSystemManager system_mgr;

// LED buffers
LedBuffer led_left_buf;
LedBuffer led_right_buf;
LedBuffer led_tongue_buf;
LedBuffer led_scale_buf;

// Communication
uint8_t tx_packet_buffer[512];
uint8_t rx_packet_buffer[512];
PacketBuilder packet_builder(tx_packet_buffer, sizeof(tx_packet_buffer));
PacketParser packet_parser(rx_packet_buffer, sizeof(rx_packet_buffer));

// ============================================================
// State
// ============================================================

uint32_t last_sensor_update = 0;
uint32_t last_telemetry_send = 0;
uint32_t last_led_update = 0;
uint32_t last_status_print = 0;
uint32_t packets_sent = 0;
uint32_t packets_received = 0;
uint8_t animation_hue = 0;

// Sensor data storage
ImuData imu_data;
EnvironmentalData env_data;

// Test results
bool telemetry_ok = false;
bool protocol_ok = false;
bool leds_ok = false;
bool uart_ok = false;

// ============================================================
// Initialization Functions
// ============================================================

/** Initialize HAL (I2C, sensors) */
bool initHAL(){
  hal_log.info(TAG, "=== Initializing HAL ===");
  
  hal_gpio.init();
  
  // Initialize I2C
  I2cConfig i2c_config;
  i2c_config.bus = 0;
  i2c_config.sda_pin = cpu::I2C_SDA;
  i2c_config.scl_pin = cpu::I2C_SCL;
  i2c_config.frequency = 400000;
  
  if(hal_i2c.init(i2c_config) != HalResult::OK){
    hal_log.error(TAG, "I2C init failed");
    return false;
  }
  
  // Initialize IMU
  hal_imu = new Esp32HalImu(&hal_i2c, &hal_log);
  ImuConfig imu_config;
  imu_config.address = i2c_addr::ICM20948;
  imu_config.accel_range = 4;
  imu_config.gyro_range = 500;
  
  if(hal_imu->init(imu_config) != HalResult::OK){
    hal_log.warn(TAG, "IMU init failed");
  }else{
    hal_log.info(TAG, "IMU initialized");
  }
  
  // Initialize Environmental sensor
  hal_env = new Esp32HalEnvironmental(&hal_i2c, &hal_log);
  EnvironmentalConfig env_config;
  env_config.address = i2c_addr::BME280;
  env_config.mode = 3;
  
  if(hal_env->init(env_config) != HalResult::OK){
    hal_log.warn(TAG, "Environmental sensor init failed");
  }else{
    hal_log.info(TAG, "Environmental sensor initialized");
  }
  
  return true;
}

/** Initialize UART for GPU communication */
bool initUART(){
  hal_log.info(TAG, "=== Initializing UART ===");
  
  UartConfig config;
  config.port = 1;
  config.tx_pin = cpu::UART_TX;
  config.rx_pin = cpu::UART_RX;
  config.baud_rate = 2000000;  // 2 Mbps - matches old working code
  config.tx_buffer_size = 8192;
  config.rx_buffer_size = 16384;
  
  if(hal_uart.init(config) != HalResult::OK){
    hal_log.error(TAG, "UART init failed");
    return false;
  }
  
  hal_log.info(TAG, "UART initialized at %lu baud", config.baud_rate);
  return true;
}

/** Initialize LED strips */
bool initLEDs(){
  hal_log.info(TAG, "=== Initializing LED Strips ===");
  
  arcos::hal::LedStripConfig config;
  config.type = LedStripType::SK6812_RGBW;
  config.brightness = 50;
  
  // Initialize HAL LED strips
  config.pin = cpu::LED_LEFT_FIN;
  config.led_count = cpu::LED_LEFT_FIN_COUNT;
  if(hal_led_left.init(config) != HalResult::OK){
    hal_log.error(TAG, "Left LED init failed");
    return false;
  }
  
  config.pin = cpu::LED_RIGHT_FIN;
  config.led_count = cpu::LED_RIGHT_FIN_COUNT;
  if(hal_led_right.init(config) != HalResult::OK){
    hal_log.error(TAG, "Right LED init failed");
    return false;
  }
  
  config.pin = cpu::LED_TONGUE;
  config.led_count = cpu::LED_TONGUE_COUNT;
  if(hal_led_tongue.init(config) != HalResult::OK){
    hal_log.error(TAG, "Tongue LED init failed");
    return false;
  }
  
  config.pin = cpu::LED_SCALE;
  config.led_count = cpu::LED_SCALE_COUNT;
  if(hal_led_scale.init(config) != HalResult::OK){
    hal_log.error(TAG, "Scale LED init failed");
    return false;
  }
  
  // Initialize Base API LED buffers
  led_left_buf.init(cpu::LED_LEFT_FIN_COUNT);
  led_right_buf.init(cpu::LED_RIGHT_FIN_COUNT);
  led_tongue_buf.init(cpu::LED_TONGUE_COUNT);
  led_scale_buf.init(cpu::LED_SCALE_COUNT);
  
  hal_log.info(TAG, "LED strips initialized");
  return true;
}

// ============================================================
// Test Functions
// ============================================================

/** Test Telemetry Processor */
bool testTelemetry(){
  hal_log.info(TAG, "=== Testing Telemetry Processor ===");
  
  // Initialize telemetry with fusion config
  FusionConfig config;
  config.gyro_weight = 0.98f;
  config.accel_weight = 0.02f;
  config.sample_rate_hz = 100.0f;
  
  Result res = telemetry.init(config);
  if(res != Result::OK){
    hal_log.error(TAG, "Telemetry init failed");
    return false;
  }
  
  hal_log.info(TAG, "Telemetry processor initialized");
  
  // Run calibration sequence (device should be stationary)
  hal_log.info(TAG, "Starting calibration (keep device still)...");
  
  uint32_t cal_start = millis();
  while(millis() - cal_start < 3000){  // 3 seconds of samples
    // Read IMU
    if(hal_imu && hal_imu->readAll(imu_data) == HalResult::OK){
      Vec3 accel(imu_data.accel.x, imu_data.accel.y, imu_data.accel.z);
      Vec3 gyro(imu_data.gyro.x * math::DEG_TO_RAD,
                imu_data.gyro.y * math::DEG_TO_RAD,
                imu_data.gyro.z * math::DEG_TO_RAD);
      
      telemetry.updateIMU(accel, gyro, Vec3(), 0.01f);
    }
    delay(10);
  }
  
  // Complete calibration
  res = telemetry.calibrate();
  if(res == Result::OK){
    hal_log.info(TAG, "Calibration complete!");
  }else{
    hal_log.warn(TAG, "Calibration incomplete (may need more samples)");
  }
  
  // Test orientation output
  hal_log.info(TAG, "Testing orientation...");
  for(int i = 0; i < 10; i++){
    if(hal_imu && hal_imu->readAll(imu_data) == HalResult::OK){
      Vec3 accel(imu_data.accel.x, imu_data.accel.y, imu_data.accel.z);
      Vec3 gyro(imu_data.gyro.x * math::DEG_TO_RAD,
                imu_data.gyro.y * math::DEG_TO_RAD,
                imu_data.gyro.z * math::DEG_TO_RAD);
      
      telemetry.updateIMU(accel, gyro, Vec3(), 0.01f);
      
      const TelemetryData& telem = telemetry.getTelemetry();
      hal_log.info(TAG, "Orientation: Roll=%.1f Pitch=%.1f Yaw=%.1f",
                   telem.motion.euler.x * math::RAD_TO_DEG,
                   telem.motion.euler.y * math::RAD_TO_DEG,
                   telem.motion.euler.z * math::RAD_TO_DEG);
    }
    delay(100);
  }
  
  hal_log.info(TAG, "Telemetry test OK");
  return true;
}

/** Test Communication Protocol */
bool testProtocol(){
  hal_log.info(TAG, "=== Testing Communication Protocol ===");
  
  // Test packet building
  packet_builder.begin(PacketType::TELEMETRY);
  
  // Add telemetry data
  const TelemetryData& telem = telemetry.getTelemetry();
  if(!packet_builder.addTelemetry(telem)){
    hal_log.error(TAG, "Failed to add telemetry to packet");
    return false;
  }
  
  size_t packet_size = packet_builder.finalize();
  hal_log.info(TAG, "Built telemetry packet: %d bytes", packet_size);
  
  // Test packet parsing (simulate receiving our own packet)
  packet_parser.reset();
  
  const uint8_t* data = packet_builder.data();
  bool packet_complete = false;
  
  for(size_t i = 0; i < packet_size; i++){
    if(packet_parser.feed(data[i])){
      packet_complete = true;
      break;
    }
  }
  
  if(!packet_complete){
    hal_log.error(TAG, "Packet parsing failed");
    return false;
  }
  
  // Verify packet type
  if(packet_parser.getType() != PacketType::TELEMETRY){
    hal_log.error(TAG, "Wrong packet type");
    return false;
  }
  
  // Parse telemetry back
  TelemetryData parsed_telem;
  if(!packet_parser.parseTelemetry(parsed_telem)){
    hal_log.error(TAG, "Failed to parse telemetry");
    return false;
  }
  
  hal_log.info(TAG, "Parsed telemetry: Frame=%lu Roll=%.1f",
               parsed_telem.frame_number,
               parsed_telem.motion.euler.x * math::RAD_TO_DEG);
  
  // Test other packet types
  packet_builder.begin(PacketType::HEARTBEAT);
  packet_builder.addU32(millis());
  packet_builder.finalize();
  hal_log.info(TAG, "Heartbeat packet: %d bytes", packet_builder.size());
  
  packet_builder.begin(PacketType::PING);
  packet_builder.finalize();
  hal_log.info(TAG, "Ping packet: %d bytes", packet_builder.size());
  
  hal_log.info(TAG, "Protocol test OK");
  return true;
}

/** Test LED Buffer/Manager */
bool testLedManager(){
  hal_log.info(TAG, "=== Testing LED Manager ===");
  
  // Test fill
  hal_log.info(TAG, "Testing fill...");
  led_left_buf.fill(ColorW(255, 0, 0, 0));  // Red
  led_right_buf.fill(ColorW(0, 255, 0, 0)); // Green
  led_tongue_buf.fill(ColorW(0, 0, 255, 0)); // Blue
  led_scale_buf.fill(ColorW(0, 0, 0, 255));  // White
  
  // Copy to HAL and show
  for(uint8_t i = 0; i < led_left_buf.count(); i++){
    const ColorW& c = led_left_buf.getPixel(i);
    hal_led_left.setPixelRGBW(i, RGBW{c.r, c.g, c.b, c.w});
  }
  for(uint8_t i = 0; i < led_right_buf.count(); i++){
    const ColorW& c = led_right_buf.getPixel(i);
    hal_led_right.setPixelRGBW(i, RGBW{c.r, c.g, c.b, c.w});
  }
  for(uint8_t i = 0; i < led_tongue_buf.count(); i++){
    const ColorW& c = led_tongue_buf.getPixel(i);
    hal_led_tongue.setPixelRGBW(i, RGBW{c.r, c.g, c.b, c.w});
  }
  for(uint8_t i = 0; i < led_scale_buf.count(); i++){
    const ColorW& c = led_scale_buf.getPixel(i);
    hal_led_scale.setPixelRGBW(i, RGBW{c.r, c.g, c.b, c.w});
  }
  hal_led_left.show();
  hal_led_right.show();
  hal_led_tongue.show();
  hal_led_scale.show();
  delay(500);
  
  // Test gradient
  hal_log.info(TAG, "Testing gradient...");
  led_left_buf.gradient(ColorW(255, 0, 0, 0), ColorW(0, 0, 255, 128));
  led_right_buf.gradient(ColorW(0, 255, 0, 0), ColorW(255, 255, 0, 64));
  
  for(uint8_t i = 0; i < led_left_buf.count(); i++){
    const ColorW& c = led_left_buf.getPixel(i);
    hal_led_left.setPixelRGBW(i, RGBW{c.r, c.g, c.b, c.w});
  }
  for(uint8_t i = 0; i < led_right_buf.count(); i++){
    const ColorW& c = led_right_buf.getPixel(i);
    hal_led_right.setPixelRGBW(i, RGBW{c.r, c.g, c.b, c.w});
  }
  hal_led_left.show();
  hal_led_right.show();
  delay(500);
  
  // Test rainbow
  hal_log.info(TAG, "Testing rainbow...");
  led_left_buf.rainbow(0, 20);
  led_right_buf.rainbow(128, 20);
  led_tongue_buf.rainbow(64, 28);
  led_scale_buf.rainbow(192, 18);
  
  for(uint8_t i = 0; i < led_left_buf.count(); i++){
    const ColorW& c = led_left_buf.getPixel(i);
    hal_led_left.setPixelRGBW(i, RGBW{c.r, c.g, c.b, c.w});
  }
  for(uint8_t i = 0; i < led_right_buf.count(); i++){
    const ColorW& c = led_right_buf.getPixel(i);
    hal_led_right.setPixelRGBW(i, RGBW{c.r, c.g, c.b, c.w});
  }
  for(uint8_t i = 0; i < led_tongue_buf.count(); i++){
    const ColorW& c = led_tongue_buf.getPixel(i);
    hal_led_tongue.setPixelRGBW(i, RGBW{c.r, c.g, c.b, c.w});
  }
  for(uint8_t i = 0; i < led_scale_buf.count(); i++){
    const ColorW& c = led_scale_buf.getPixel(i);
    hal_led_scale.setPixelRGBW(i, RGBW{c.r, c.g, c.b, c.w});
  }
  hal_led_left.show();
  hal_led_right.show();
  hal_led_tongue.show();
  hal_led_scale.show();
  delay(500);
  
  // Test effects
  hal_log.info(TAG, "Testing chase effect...");
  for(int frame = 0; frame < 30; frame++){
    effects::chase(led_left_buf, ColorW(0, 255, 255, 50), 
                   frame * 100, 100);
    
    for(uint8_t i = 0; i < led_left_buf.count(); i++){
      const ColorW& c = led_left_buf.getPixel(i);
      hal_led_left.setPixelRGBW(i, RGBW{c.r, c.g, c.b, c.w});
    }
    hal_led_left.show();
    delay(50);
  }
  
  hal_log.info(TAG, "Testing comet effect...");
  for(int frame = 0; frame < 50; frame++){
    effects::comet(led_right_buf, ColorW(255, 128, 0, 100),
                   frame * 50, 50, 6);
    
    for(uint8_t i = 0; i < led_right_buf.count(); i++){
      const ColorW& c = led_right_buf.getPixel(i);
      hal_led_right.setPixelRGBW(i, RGBW{c.r, c.g, c.b, c.w});
    }
    hal_led_right.show();
    delay(30);
  }
  
  hal_log.info(TAG, "LED Manager test OK");
  return true;
}

/** Test System State */
bool testSystemState(){
  hal_log.info(TAG, "=== Testing System State ===");
  
  // Initialize system manager
  Result res = system_mgr.init(DeviceRole::CPU, "ARCOS_CPU");
  if(res != Result::OK){
    hal_log.error(TAG, "System manager init failed");
    return false;
  }
  
  // Set status flags
  system_mgr.setSensorsReady(true);
  system_mgr.setLedsReady(true);
  system_mgr.setCommReady(uart_ok);
  
  // Set mode
  system_mgr.setMode(SystemMode::ACTIVE);
  
  // Print state
  const SystemState& state = system_mgr.getState();
  hal_log.info(TAG, "Device: %s", state.device_name);
  hal_log.info(TAG, "Role: %s", state.role == DeviceRole::CPU ? "CPU" : "GPU");
  hal_log.info(TAG, "Mode: %s", modeToString(state.mode));
  hal_log.info(TAG, "Uptime: %lu ms", system_mgr.getUptime());
  
  // Test error reporting
  system_mgr.reportError(ErrorCode::NONE, ErrorSeverity::INFO, "Test info");
  system_mgr.reportError(ErrorCode::GPS_NO_FIX, ErrorSeverity::WARNING, "No GPS fix");
  
  const PerformanceMetrics& metrics = system_mgr.getMetrics();
  hal_log.info(TAG, "Warnings: %lu, Errors: %lu",
               metrics.warning_count, metrics.error_count);
  
  system_mgr.clearError();
  
  hal_log.info(TAG, "System State test OK");
  return true;
}

// ============================================================
// Helper to copy buffer to HAL
// ============================================================

void updateLedStrip(const LedBuffer& buffer, Esp32HalLedStrip& strip){
  for(uint8_t i = 0; i < buffer.count(); i++){
    const ColorW& c = buffer.getPixel(i);
    strip.setPixelRGBW(i, RGBW{c.r, c.g, c.b, c.w});
  }
  strip.show();
}

// ============================================================
// Main Application
// ============================================================

void setup(){
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("\n\n========================================");
  Serial.println("  ARCOS Base System API Test - CPU");
  Serial.println("  Testing Middleware Layer");
  Serial.println("========================================\n");
  
  // Initialize logging
  hal_log.init(LogLevel::DEBUG);
  g_hal_log = &hal_log;
  hal_error.init();
  
  hal_log.info(TAG, "Starting Base System API tests...");
  hal_log.info(TAG, "API Version: %s", version::STRING);
  
  // Initialize HAL
  if(!initHAL()){
    hal_log.error(TAG, "HAL initialization failed!");
  }
  
  // Initialize UART
  uart_ok = initUART();
  
  // Initialize LEDs
  leds_ok = initLEDs();
  
  // Run tests
  hal_log.info(TAG, "\n--- Telemetry Test ---");
  telemetry_ok = testTelemetry();
  
  hal_log.info(TAG, "\n--- Protocol Test ---");
  protocol_ok = testProtocol();
  
  hal_log.info(TAG, "\n--- LED Manager Test ---");
  leds_ok = leds_ok && testLedManager();
  
  hal_log.info(TAG, "\n--- System State Test ---");
  testSystemState();
  
  // Print summary
  hal_log.info(TAG, "\n============ TEST SUMMARY ============");
  hal_log.info(TAG, "Telemetry:    %s", telemetry_ok ? "OK" : "FAIL");
  hal_log.info(TAG, "Protocol:     %s", protocol_ok ? "OK" : "FAIL");
  hal_log.info(TAG, "LED Manager:  %s", leds_ok ? "OK" : "FAIL");
  hal_log.info(TAG, "UART:         %s", uart_ok ? "OK" : "FAIL");
  hal_log.info(TAG, "======================================\n");
  
  hal_log.info(TAG, "Entering main loop...");
  hal_log.info(TAG, "Sending telemetry to GPU, animating LEDs");
}

void loop(){
  uint32_t now = hal_timer.millis();
  
  // Update sensors and telemetry (100 Hz)
  if(now - last_sensor_update >= 10){
    last_sensor_update = now;
    
    // Read IMU
    if(hal_imu && hal_imu->readAll(imu_data) == HalResult::OK){
      Vec3 accel(imu_data.accel.x, imu_data.accel.y, imu_data.accel.z);
      Vec3 gyro(imu_data.gyro.x * math::DEG_TO_RAD,
                imu_data.gyro.y * math::DEG_TO_RAD,
                imu_data.gyro.z * math::DEG_TO_RAD);
      
      telemetry.updateIMU(accel, gyro, Vec3(), 0.01f);
    }
    
    // Read environmental (slower)
    static uint32_t last_env = 0;
    if(now - last_env >= 500){
      last_env = now;
      if(hal_env && hal_env->readAll(env_data) == HalResult::OK){
        telemetry.updateEnvironment(
          env_data.temperature,
          env_data.humidity,
          env_data.pressure
        );
      }
    }
  }
  
  // Send telemetry to GPU (50 Hz)
  if(uart_ok && (now - last_telemetry_send >= 20)){
    last_telemetry_send = now;
    
    const TelemetryData& telem = telemetry.getTelemetry();
    
    packet_builder.begin(PacketType::TELEMETRY);
    packet_builder.addTelemetry(telem);
    size_t size = packet_builder.finalize();
    
    hal_uart.write(packet_builder.data(), size, nullptr);
    packets_sent++;
    system_mgr.addPacketSent(size);
  }
  
  // Check for incoming packets from GPU
  if(uart_ok && hal_uart.available() > 0){
    uint8_t byte;
    size_t read;
    while(hal_uart.read(&byte, 1, &read, 0) == HalResult::OK && read > 0){
      if(packet_parser.feed(byte)){
        packets_received++;
        system_mgr.addPacketReceived(packet_parser.getPayloadLength());
        
        PacketType type = packet_parser.getType();
        if(type == PacketType::PONG || type == PacketType::HEARTBEAT){
          hal_log.debug(TAG, "Received packet: type=0x%02X", (int)type);
        }
      }
    }
  }
  
  // Update LED animations (30 Hz)
  if(now - last_led_update >= 33){
    last_led_update = now;
    animation_hue += 2;
    
    // Get telemetry for orientation-based effects
    const TelemetryData& telem = telemetry.getTelemetry();
    float roll = telem.motion.euler.x * math::RAD_TO_DEG;
    float pitch = telem.motion.euler.y * math::RAD_TO_DEG;
    
    // Map orientation to colors (cool effect!)
    uint8_t roll_hue = (uint8_t)((roll + 90) * 255 / 180);
    uint8_t pitch_brightness = (uint8_t)(128 + pitch * 2);
    
    // Left fin: rainbow shifted by roll
    led_left_buf.rainbow(animation_hue + roll_hue, 20, 255, pitch_brightness);
    
    // Right fin: complementary
    led_right_buf.rainbow(animation_hue + roll_hue + 128, 20, 255, pitch_brightness);
    
    // Tongue: pulse based on stability
    if(telem.motion.is_stable){
      effects::pulse(led_tongue_buf, ColorW(0, 255, 0, 100), now, 1000);
    }else{
      effects::pulse(led_tongue_buf, ColorW(255, 100, 0, 50), now, 250);
    }
    
    // Scale: comet effect
    effects::comet(led_scale_buf, ColorW(100, 50, 200, 80), now, 60, 5);
    
    // Update hardware
    updateLedStrip(led_left_buf, hal_led_left);
    updateLedStrip(led_right_buf, hal_led_right);
    updateLedStrip(led_tongue_buf, hal_led_tongue);
    updateLedStrip(led_scale_buf, hal_led_scale);
  }
  
  // Print status (every 5 seconds)
  if(now - last_status_print >= 5000){
    last_status_print = now;
    
    const TelemetryData& telem = telemetry.getTelemetry();
    const PerformanceMetrics& metrics = system_mgr.getMetrics();
    
    hal_log.info(TAG, "=== Status ===");
    hal_log.info(TAG, "Orientation: R=%.1f P=%.1f Y=%.1f",
                 telem.motion.euler.x * math::RAD_TO_DEG,
                 telem.motion.euler.y * math::RAD_TO_DEG,
                 telem.motion.euler.z * math::RAD_TO_DEG);
    hal_log.info(TAG, "Env: T=%.1fC H=%.0f%% P=%.0fhPa",
                 telem.environment.temperature,
                 telem.environment.humidity,
                 telem.environment.pressure / 100.0f);
    hal_log.info(TAG, "Comm: TX=%lu RX=%lu",
                 packets_sent, packets_received);
    hal_log.info(TAG, "Uptime: %lu s", system_mgr.getUptime() / 1000);
  }
  
  // Update system metrics
  system_mgr.updateMetrics();
  
  hal_timer.yield();
}
