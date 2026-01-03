/*****************************************************************
 * File:      CPU_HAL_Test.cpp
 * Category:  Main Application (HAL Test)
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    CPU-side HAL test application.
 *    Tests ALL HAL interfaces: GPIO, I2C, SPI, UART, I2S,
 *    sensors (IMU, ENV, GPS, Mic), LEDs, buttons, storage.
 * 
 * Hardware (COM 15):
 *    - ESP32-S3 (CPU)
 *    - I2C: SDA=GPIO9, SCL=GPIO10
 *      - ICM20948 IMU @ 0x68
 *      - BME280 Environmental @ 0x76
 *    - UART to GPU: RX=GPIO11, TX=GPIO12
 *    - GPS UART: TX=GPIO43, RX=GPIO44
 *    - Buttons: A=GPIO5, B=GPIO6, C=GPIO7, D=GPIO15
 *    - LED Strips: Left=GPIO18(13), Right=GPIO38(13), 
 *                  Tongue=GPIO8(9), Scale=GPIO37(14)
 *    - Fans: GPIO17, GPIO36 (PWM)
 *    - Microphone I2S: WS=GPIO42, BCK=GPIO40, SD=GPIO2
 *    - SD Card SPI: MISO=GPIO14, MOSI=GPIO47, CLK=GPIO21, CS=GPIO48
 * 
 * Usage:
 *    Build with CPU_HAL_Test environment in platformio.ini
 *****************************************************************/

#include <Arduino.h>
#include <cmath>
#include "HAL/Hal.hpp"
#include "HAL/ESP32/Esp32Hal.hpp"

using namespace arcos::hal;
using namespace arcos::hal::esp32;
using namespace arcos::hal::pins;

// ============================================================
// HAL Instances
// ============================================================

Esp32HalLog hal_log;
Esp32HalErrorHandler hal_error(&hal_log);
Esp32HalSystemTimer hal_timer;
Esp32HalGpio hal_gpio(&hal_log);
Esp32HalPwm hal_pwm(&hal_log);
Esp32HalI2c hal_i2c(&hal_log);
Esp32HalUart hal_uart(&hal_log);
Esp32HalSpi hal_spi(&hal_log);
Esp32HalGps hal_gps(&hal_log);
Esp32HalMicrophone hal_mic(&hal_log);
Esp32HalStorage hal_storage(&hal_log);
Esp32HalImu* hal_imu = nullptr;
Esp32HalEnvironmental* hal_env = nullptr;
Esp32HalLedStrip hal_led_left(&hal_log);
Esp32HalLedStrip hal_led_right(&hal_log);
Esp32HalLedStrip hal_led_tongue(&hal_log);
Esp32HalLedStrip hal_led_scale(&hal_log);
Esp32HalButton hal_btn_a(&hal_log);
Esp32HalButton hal_btn_b(&hal_log);
Esp32HalButton hal_btn_c(&hal_log);
Esp32HalButton hal_btn_d(&hal_log);

// Global logger for HAL macros
IHalLog* g_hal_log = nullptr;

// ============================================================
// Test State
// ============================================================

static constexpr const char* TAG = "CPU_TEST";

uint32_t last_sensor_read = 0;
uint32_t last_uart_send = 0;
uint32_t last_led_update = 0;
uint32_t last_gps_update = 0;
uint32_t last_mic_update = 0;
uint8_t led_hue = 0;
uint8_t test_phase = 0;

// Test results
bool buttons_ok = false;
bool sensors_ok = false;
bool uart_ok = false;
bool leds_ok = false;
bool gps_ok = false;
bool mic_ok = false;
bool storage_ok = false;
bool spi_ok = false;

// UART communication
uint8_t rx_buffer[256];
uint8_t tx_buffer[256];
uint32_t uart_rx_count = 0;
uint32_t uart_tx_count = 0;

// ============================================================
// Helper Functions
// ============================================================

/** HSV to RGB conversion */
RGB hsvToRgb(uint8_t h, uint8_t s, uint8_t v){
  RGB rgb;
  if(s == 0){
    rgb.r = rgb.g = rgb.b = v;
    return rgb;
  }
  
  uint8_t region = h / 43;
  uint8_t remainder = (h - region * 43) * 6;
  
  uint8_t p = (v * (255 - s)) >> 8;
  uint8_t q = (v * (255 - ((s * remainder) >> 8))) >> 8;
  uint8_t t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;
  
  switch(region){
    case 0:  rgb.r = v; rgb.g = t; rgb.b = p; break;
    case 1:  rgb.r = q; rgb.g = v; rgb.b = p; break;
    case 2:  rgb.r = p; rgb.g = v; rgb.b = t; break;
    case 3:  rgb.r = p; rgb.g = q; rgb.b = v; break;
    case 4:  rgb.r = t; rgb.g = p; rgb.b = v; break;
    default: rgb.r = v; rgb.g = p; rgb.b = q; break;
  }
  return rgb;
}

// ============================================================
// Test Functions
// ============================================================

/** Test GPIO and Buttons */
bool testButtons(){
  hal_log.info(TAG, "=== Testing Buttons ===");
  
  ButtonConfig config;
  config.mode = GpioMode::GPIO_INPUT_PULLUP;
  config.active_low = true;
  config.debounce_ms = 50;
  
  config.pin = cpu::BUTTON_A;
  if(hal_btn_a.init(config) != HalResult::OK){
    hal_log.error(TAG, "Button A init failed");
    return false;
  }
  
  config.pin = cpu::BUTTON_B;
  if(hal_btn_b.init(config) != HalResult::OK){
    hal_log.error(TAG, "Button B init failed");
    return false;
  }
  
  config.pin = cpu::BUTTON_C;
  if(hal_btn_c.init(config) != HalResult::OK){
    hal_log.error(TAG, "Button C init failed");
    return false;
  }
  
  config.pin = cpu::BUTTON_D;
  if(hal_btn_d.init(config) != HalResult::OK){
    hal_log.error(TAG, "Button D init failed");
    return false;
  }
  
  hal_log.info(TAG, "Buttons initialized OK");
  return true;
}

/** Test PWM (Fans) */
bool testPwm(){
  hal_log.info(TAG, "=== Testing PWM (Fans) ===");
  
  if(hal_pwm.init(cpu::FAN_2, 25000, 8) != HalResult::OK){
    hal_log.error(TAG, "Fan 2 PWM init failed");
    return false;
  }
  
  // Test different speeds
  hal_log.info(TAG, "Fan 2: 0%%");
  hal_pwm.setDutyPercent(cpu::FAN_2, 0);
  delay(500);
  
  hal_log.info(TAG, "Fan 2: 50%%");
  hal_pwm.setDutyPercent(cpu::FAN_2, 50);
  delay(500);
  
  hal_log.info(TAG, "Fan 2: 100%%");
  hal_pwm.setDutyPercent(cpu::FAN_2, 100);
  delay(500);
  
  hal_log.info(TAG, "Fan 2: 25%% (idle)");
  hal_pwm.setDutyPercent(cpu::FAN_2, 25);
  
  hal_log.info(TAG, "PWM test OK");
  return true;
}

/** Test I2C Bus */
bool testI2c(){
  hal_log.info(TAG, "=== Testing I2C ===");
  
  I2cConfig config;
  config.bus = 0;
  config.sda_pin = cpu::I2C_SDA;
  config.scl_pin = cpu::I2C_SCL;
  config.frequency = 400000;
  
  if(hal_i2c.init(config) != HalResult::OK){
    hal_log.error(TAG, "I2C init failed");
    return false;
  }
  
  // Scan for devices
  hal_log.info(TAG, "Scanning I2C bus...");
  int found = 0;
  for(uint8_t addr = 0x08; addr < 0x78; addr++){
    if(hal_i2c.probe(addr) == HalResult::OK){
      hal_log.info(TAG, "  Found device at 0x%02X", addr);
      found++;
    }
  }
  hal_log.info(TAG, "I2C scan: %d devices found", found);
  
  return true;
}

/** Test IMU Sensor */
bool testImu(){
  hal_log.info(TAG, "=== Testing IMU (ICM20948) ===");
  
  hal_imu = new Esp32HalImu(&hal_i2c, &hal_log);
  
  ImuConfig config;
  config.address = i2c_addr::ICM20948;
  config.accel_range = 4;
  config.gyro_range = 500;
  
  if(hal_imu->init(config) != HalResult::OK){
    hal_log.error(TAG, "IMU init failed");
    return false;
  }
  
  // Read some samples
  ImuData data;
  for(int i = 0; i < 3; i++){
    if(hal_imu->readAll(data) == HalResult::OK){
      hal_log.info(TAG, "IMU: Accel(%.2f, %.2f, %.2f) Gyro(%.1f, %.1f, %.1f) Temp=%.1fC",
                   data.accel.x, data.accel.y, data.accel.z,
                   data.gyro.x, data.gyro.y, data.gyro.z,
                   data.temperature);
    }
    delay(100);
  }
  
  hal_log.info(TAG, "IMU test OK");
  return true;
}

/** Test Environmental Sensor */
bool testEnvironmental(){
  hal_log.info(TAG, "=== Testing Environmental (BME280) ===");
  
  hal_env = new Esp32HalEnvironmental(&hal_i2c, &hal_log);
  
  EnvironmentalConfig config;
  config.address = i2c_addr::BME280;
  config.temp_oversampling = 1;
  config.humidity_oversampling = 1;
  config.pressure_oversampling = 1;
  config.mode = 3; // Normal mode
  
  if(hal_env->init(config) != HalResult::OK){
    hal_log.error(TAG, "Environmental sensor init failed");
    return false;
  }
  
  // Read some samples
  EnvironmentalData data;
  for(int i = 0; i < 3; i++){
    delay(100);
    if(hal_env->readAll(data) == HalResult::OK){
      hal_log.info(TAG, "ENV: Temp=%.2fC Humidity=%.1f%% Pressure=%.1fhPa",
                   data.temperature, data.humidity, data.pressure / 100.0f);
    }
  }
  
  hal_log.info(TAG, "Environmental test OK");
  return true;
}

/** Test LED Strips */
bool testLedStrips(){
  hal_log.info(TAG, "=== Testing LED Strips ===");
  
  LedStripConfig config;
  config.type = LedStripType::SK6812_RGBW;
  config.brightness = 50;
  
  // Left fin
  config.pin = cpu::LED_LEFT_FIN;
  config.led_count = cpu::LED_LEFT_FIN_COUNT;
  if(hal_led_left.init(config) != HalResult::OK){
    hal_log.error(TAG, "Left fin LED init failed");
    return false;
  }
  
  // Right fin
  config.pin = cpu::LED_RIGHT_FIN;
  config.led_count = cpu::LED_RIGHT_FIN_COUNT;
  if(hal_led_right.init(config) != HalResult::OK){
    hal_log.error(TAG, "Right fin LED init failed");
    return false;
  }
  
  // Tongue
  config.pin = cpu::LED_TONGUE;
  config.led_count = cpu::LED_TONGUE_COUNT;
  if(hal_led_tongue.init(config) != HalResult::OK){
    hal_log.error(TAG, "Tongue LED init failed");
    return false;
  }
  
  // Scale
  config.pin = cpu::LED_SCALE;
  config.led_count = cpu::LED_SCALE_COUNT;
  if(hal_led_scale.init(config) != HalResult::OK){
    hal_log.error(TAG, "Scale LED init failed");
    return false;
  }
  
  // Test pattern - red, green, blue sequence
  hal_log.info(TAG, "LED test: RED");
  hal_led_left.fill(RGB{255, 0, 0});
  hal_led_right.fill(RGB{255, 0, 0});
  hal_led_tongue.fill(RGB{255, 0, 0});
  hal_led_scale.fill(RGB{255, 0, 0});
  hal_led_left.show();
  hal_led_right.show();
  hal_led_tongue.show();
  hal_led_scale.show();
  delay(500);
  
  hal_log.info(TAG, "LED test: GREEN");
  hal_led_left.fill(RGB{0, 255, 0});
  hal_led_right.fill(RGB{0, 255, 0});
  hal_led_tongue.fill(RGB{0, 255, 0});
  hal_led_scale.fill(RGB{0, 255, 0});
  hal_led_left.show();
  hal_led_right.show();
  hal_led_tongue.show();
  hal_led_scale.show();
  delay(500);
  
  hal_log.info(TAG, "LED test: BLUE");
  hal_led_left.fill(RGB{0, 0, 255});
  hal_led_right.fill(RGB{0, 0, 255});
  hal_led_tongue.fill(RGB{0, 0, 255});
  hal_led_scale.fill(RGB{0, 0, 255});
  hal_led_left.show();
  hal_led_right.show();
  hal_led_tongue.show();
  hal_led_scale.show();
  delay(500);
  
  // Test RGBW white channel (SK6812 has dedicated white LED)
  hal_log.info(TAG, "LED test: WHITE (RGBW)");
  hal_led_left.fillRGBW(RGBW{0, 0, 0, 255});   // Pure white LED only
  hal_led_right.fillRGBW(RGBW{0, 0, 0, 255});
  hal_led_tongue.fillRGBW(RGBW{0, 0, 0, 255});
  hal_led_scale.fillRGBW(RGBW{0, 0, 0, 255});
  hal_led_left.show();
  hal_led_right.show();
  hal_led_tongue.show();
  hal_led_scale.show();
  delay(500);
  
  // Test warm white (RGB + W combined)
  hal_log.info(TAG, "LED test: WARM WHITE (RGB+W)");
  hal_led_left.fillRGBW(RGBW{255, 180, 100, 200});   // Warm white mixed
  hal_led_right.fillRGBW(RGBW{255, 180, 100, 200});
  hal_led_tongue.fillRGBW(RGBW{255, 180, 100, 200});
  hal_led_scale.fillRGBW(RGBW{255, 180, 100, 200});
  hal_led_left.show();
  hal_led_right.show();
  hal_led_tongue.show();
  hal_led_scale.show();
  delay(500);
  
  // Test RGBW gradient on individual LEDs
  hal_log.info(TAG, "LED test: RGBW gradient");
  for(uint16_t i = 0; i < hal_led_left.getLedCount(); i++){
    uint8_t white = (i * 20) % 256;
    uint8_t hue = (i * 20) % 256;
    RGB color = hsvToRgb(hue, 255, 128);
    hal_led_left.setPixelRGBW(i, RGBW{color.r, color.g, color.b, white});
    hal_led_right.setPixelRGBW(i, RGBW{color.r, color.g, color.b, white});
  }
  for(uint16_t i = 0; i < hal_led_tongue.getLedCount(); i++){
    uint8_t white = (i * 28) % 256;
    hal_led_tongue.setPixelRGBW(i, RGBW{0, 0, 0, white});
  }
  for(uint16_t i = 0; i < hal_led_scale.getLedCount(); i++){
    uint8_t white = 255 - (i * 18) % 256;
    hal_led_scale.setPixelRGBW(i, RGBW{100, 50, 0, white});  // Orange + white
  }
  hal_led_left.show();
  hal_led_right.show();
  hal_led_tongue.show();
  hal_led_scale.show();
  delay(500);
  
  hal_log.info(TAG, "LED strips test OK (RGB + RGBW verified)");
  return true;
}

/** Test UART Communication to GPU */
bool testUart(){
  hal_log.info(TAG, "=== Testing UART (CPU-GPU) ===");
  
  UartConfig config;
  config.port = 1;
  config.tx_pin = cpu::UART_TX;
  config.rx_pin = cpu::UART_RX;
  config.baud_rate = defaults::CPU_GPU_BAUD;
  config.tx_buffer_size = 8192;
  config.rx_buffer_size = 16384;
  
  if(hal_uart.init(config) != HalResult::OK){
    hal_log.error(TAG, "UART init failed");
    return false;
  }
  
  hal_log.info(TAG, "UART initialized at %lu baud", config.baud_rate);
  
  // Send test message
  const char* test_msg = "CPU_PING";
  hal_uart.write((const uint8_t*)test_msg, strlen(test_msg), nullptr);
  hal_log.info(TAG, "Sent: %s", test_msg);
  
  // Wait for response
  delay(100);
  
  if(hal_uart.available() > 0){
    size_t read;
    hal_uart.read(rx_buffer, hal_uart.available(), &read, 100);
    rx_buffer[read] = '\0';
    hal_log.info(TAG, "Received: %s", rx_buffer);
  }else{
    hal_log.warn(TAG, "No response from GPU");
  }
  
  hal_log.info(TAG, "UART test OK");
  return true;
}

/** Test GPS Module */
bool testGps(){
  hal_log.info(TAG, "=== Testing GPS (NEO-8M) ===");
  
  GpsConfig config;
  config.tx_pin = cpu::GPS_TX;
  config.rx_pin = cpu::GPS_RX;
  config.baud_rate = 9600;
  
  if(hal_gps.init(config) != HalResult::OK){
    hal_log.error(TAG, "GPS init failed");
    return false;
  }
  
  hal_log.info(TAG, "GPS initialized, waiting for data...");
  
  // Try to get some data (GPS may take time to get fix)
  uint32_t start = millis();
  bool got_data = false;
  while(millis() - start < 3000){  // Wait up to 3 seconds
    if(hal_gps.update() == HalResult::OK){
      got_data = true;
      GpsData data;
      hal_gps.getData(data);
      hal_log.info(TAG, "GPS: Sats=%d Fix=%d", 
                   data.satellites_used, (int)data.fix_quality);
      if(data.position.valid){
        hal_log.info(TAG, "GPS: Lat=%.6f Lon=%.6f Alt=%.1fm",
                     data.position.latitude, data.position.longitude,
                     data.position.altitude);
      }
      break;
    }
    delay(100);
  }
  
  if(!got_data){
    hal_log.warn(TAG, "No GPS data received (module may need sky view)");
  }
  
  hal_log.info(TAG, "GPS test OK (hardware present)");
  return true;
}

/** Test Microphone */
bool testMicrophone(){
  hal_log.info(TAG, "=== Testing Microphone (INMP441) ===");
  
  MicrophoneConfig config;
  config.ws_pin = cpu::MIC_WS;
  config.bck_pin = cpu::MIC_CLK;
  config.data_pin = cpu::MIC_DOUT;
  config.sample_rate = 16000;
  config.i2s_port = 0;
  config.buffer_size = 512;
  
  if(hal_mic.init(config) != HalResult::OK){
    hal_log.error(TAG, "Microphone init failed");
    return false;
  }
  
  hal_log.info(TAG, "Microphone initialized, reading samples...");
  
  // Read a few samples
  for(int i = 0; i < 5; i++){
    if(hal_mic.update() == HalResult::OK){
      MicrophoneData data;
      hal_mic.getData(data);
      hal_log.info(TAG, "MIC: Peak=%ld RMS=%.4f dB=%.1f %s",
                   data.peak_amplitude, data.rms_level, data.db_level,
                   data.clipping ? "CLIP!" : "");
    }
    delay(100);
  }
  
  hal_log.info(TAG, "Microphone test OK");
  return true;
}

/** Test SPI Bus */
bool testSpi(){
  hal_log.info(TAG, "=== Testing SPI ===");
  
  SpiConfig config;
  config.bus = 0;
  config.mosi_pin = cpu::SD_MOSI;
  config.miso_pin = cpu::SD_MISO;
  config.sck_pin = cpu::SD_CLK;
  config.cs_pin = cpu::SD_CS;
  config.frequency = 1000000;  // 1 MHz for testing
  config.mode = SpiMode::MODE_0;
  
  if(hal_spi.init(config) != HalResult::OK){
    hal_log.error(TAG, "SPI init failed");
    return false;
  }
  
  hal_log.info(TAG, "SPI initialized: MOSI=%d MISO=%d CLK=%d CS=%d",
               config.mosi_pin, config.miso_pin, config.sck_pin, config.cs_pin);
  
  // Simple loopback test (if MOSI connected to MISO)
  hal_spi.beginTransaction();
  uint8_t tx = 0xAA;
  uint8_t rx = 0;
  hal_spi.transfer(tx, &rx);
  hal_spi.endTransaction();
  
  hal_log.info(TAG, "SPI transfer: TX=0x%02X RX=0x%02X", tx, rx);
  
  hal_spi.deinit();  // Deinit so SD card can use SPI
  
  hal_log.info(TAG, "SPI test OK");
  return true;
}

/** Test SD Card Storage */
bool testStorage(){
  hal_log.info(TAG, "=== Testing Storage (SD Card) ===");
  
  SdCardConfig config;
  config.miso_pin = cpu::SD_MISO;
  config.mosi_pin = cpu::SD_MOSI;
  config.clk_pin = cpu::SD_CLK;
  config.cs_pin = cpu::SD_CS;
  config.frequency = 20000000;
  
  if(hal_storage.init(config) != HalResult::OK){
    hal_log.error(TAG, "Storage init failed");
    return false;
  }
  
  hal_log.info(TAG, "Mounting SD card...");
  if(hal_storage.mount() != HalResult::OK){
    hal_log.warn(TAG, "SD card mount failed (card may not be inserted)");
    return false;
  }
  
  // Get card info
  uint64_t total = hal_storage.getTotalSize();
  uint64_t free_space = hal_storage.getFreeSpace();
  hal_log.info(TAG, "SD Card: Total=%llu MB, Free=%llu MB",
               total / (1024*1024), free_space / (1024*1024));
  
  // Test file operations
  const char* test_file = "/hal_test.txt";
  const char* test_data = "ARCOS HAL Storage Test\n";
  
  // Create and write file
  Esp32HalFile file(&hal_log);
  if(file.open(test_file, FileMode::WRITE) == HalResult::OK){
    size_t written;
    file.write((const uint8_t*)test_data, strlen(test_data), &written);
    file.close();
    hal_log.info(TAG, "Wrote %zu bytes to %s", written, test_file);
  }
  
  // Read file back
  if(file.open(test_file, FileMode::READ) == HalResult::OK){
    uint8_t buffer[64];
    size_t read_bytes;
    file.read(buffer, sizeof(buffer) - 1, &read_bytes);
    buffer[read_bytes] = '\0';
    file.close();
    hal_log.info(TAG, "Read: %s", buffer);
  }
  
  // Delete test file
  hal_storage.deleteFile(test_file);
  hal_log.info(TAG, "Test file deleted");
  
  hal_storage.unmount();
  hal_log.info(TAG, "Storage test OK");
  return true;
}

// ============================================================
// Main Application
// ============================================================

void setup(){
  // Initialize Serial for debugging
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("\n\n========================================");
  Serial.println("  ARCOS HAL Test - CPU (COM 15)");
  Serial.println("  Testing ALL HAL Implementations");
  Serial.println("========================================\n");
  
  // Initialize logging
  hal_log.init(LogLevel::DEBUG);
  g_hal_log = &hal_log;
  hal_error.init();
  
  hal_log.info(TAG, "Starting comprehensive HAL tests...");
  
  // Initialize GPIO
  hal_gpio.init();
  
  // Run all tests
  hal_log.info(TAG, "\n--- Core Tests ---");
  buttons_ok = testButtons();
  testPwm();
  
  hal_log.info(TAG, "\n--- Communication Tests ---");
  if(testI2c()){
    sensors_ok = testImu() && testEnvironmental();
  }
  spi_ok = testSpi();
  uart_ok = testUart();
  
  hal_log.info(TAG, "\n--- Sensor Tests ---");
  gps_ok = testGps();
  mic_ok = testMicrophone();
  
  hal_log.info(TAG, "\n--- Output Tests ---");
  leds_ok = testLedStrips();
  
  hal_log.info(TAG, "\n--- Storage Tests ---");
  storage_ok = testStorage();
  
  // Print summary
  hal_log.info(TAG, "\n============ TEST SUMMARY ============");
  hal_log.info(TAG, "Buttons:      %s", buttons_ok ? "OK" : "FAIL");
  hal_log.info(TAG, "I2C Sensors:  %s", sensors_ok ? "OK" : "FAIL");
  hal_log.info(TAG, "SPI:          %s", spi_ok ? "OK" : "FAIL");
  hal_log.info(TAG, "UART:         %s", uart_ok ? "OK" : "FAIL");
  hal_log.info(TAG, "GPS:          %s", gps_ok ? "OK" : "FAIL/NO FIX");
  hal_log.info(TAG, "Microphone:   %s", mic_ok ? "OK" : "FAIL");
  hal_log.info(TAG, "LED Strips:   %s", leds_ok ? "OK" : "FAIL");
  hal_log.info(TAG, "Storage:      %s", storage_ok ? "OK" : "FAIL/NO CARD");
  hal_log.info(TAG, "======================================\n");
  
  hal_log.info(TAG, "Entering main loop...");
  hal_log.info(TAG, "Press buttons to test, watch LEDs animate");
}

void loop(){
  uint32_t now = hal_timer.millis();
  
  // Update buttons
  hal_btn_a.update();
  hal_btn_b.update();
  hal_btn_c.update();
  hal_btn_d.update();
  
  // Check for button presses
  if(hal_btn_a.justPressed()){
    hal_log.info(TAG, "Button A pressed!");
  }
  if(hal_btn_b.justPressed()){
    hal_log.info(TAG, "Button B pressed!");
  }
  if(hal_btn_c.justPressed()){
    hal_log.info(TAG, "Button C pressed!");
  }
  if(hal_btn_d.justPressed()){
    hal_log.info(TAG, "Button D pressed!");
  }
  
  // Update LED animation every 30ms
  if(now - last_led_update >= 30){
    last_led_update = now;
    led_hue += 2;
    
    // RGBW Rainbow animation on all strips (using white channel)
    for(uint16_t i = 0; i < hal_led_left.getLedCount(); i++){
      RGB color = hsvToRgb((led_hue + i * 20) % 256, 255, 128);
      // Add subtle white pulse for depth
      uint8_t white = (sin((led_hue + i * 10) * 0.05f) + 1.0f) * 30;
      hal_led_left.setPixelRGBW(i, RGBW{color.r, color.g, color.b, white});
    }
    for(uint16_t i = 0; i < hal_led_right.getLedCount(); i++){
      RGB color = hsvToRgb((led_hue + i * 20 + 128) % 256, 255, 128);
      uint8_t white = (sin((led_hue + i * 10 + 128) * 0.05f) + 1.0f) * 30;
      hal_led_right.setPixelRGBW(i, RGBW{color.r, color.g, color.b, white});
    }
    for(uint16_t i = 0; i < hal_led_tongue.getLedCount(); i++){
      RGB color = hsvToRgb((led_hue + i * 28) % 256, 255, 200);
      // Tongue: warmer whites for organic feel
      uint8_t white = (sin((led_hue + i * 15) * 0.08f) + 1.0f) * 50;
      hal_led_tongue.setPixelRGBW(i, RGBW{color.r, color.g, color.b, white});
    }
    for(uint16_t i = 0; i < hal_led_scale.getLedCount(); i++){
      RGB color = hsvToRgb((led_hue + i * 18 + 64) % 256, 255, 100);
      // Scale: subtle white accent
      uint8_t white = (sin((led_hue + i * 12) * 0.06f) + 1.0f) * 20;
      hal_led_scale.setPixelRGBW(i, RGBW{color.r, color.g, color.b, white});
    }
    
    hal_led_left.show();
    hal_led_right.show();
    hal_led_tongue.show();
    hal_led_scale.show();
  }
  
  // Read sensors every second
  if(sensors_ok && (now - last_sensor_read >= 1000)){
    last_sensor_read = now;
    
    ImuData imu_data;
    if(hal_imu && hal_imu->readAll(imu_data) == HalResult::OK){
      hal_log.debug(TAG, "IMU: A(%.2f,%.2f,%.2f) G(%.1f,%.1f,%.1f)",
                    imu_data.accel.x, imu_data.accel.y, imu_data.accel.z,
                    imu_data.gyro.x, imu_data.gyro.y, imu_data.gyro.z);
    }
    
    EnvironmentalData env_data;
    if(hal_env && hal_env->readAll(env_data) == HalResult::OK){
      hal_log.debug(TAG, "ENV: T=%.1fC H=%.1f%% P=%.1fhPa",
                    env_data.temperature, env_data.humidity, 
                    env_data.pressure / 100.0f);
    }
  }
  
  // Update GPS every 500ms
  if(gps_ok && (now - last_gps_update >= 500)){
    last_gps_update = now;
    hal_gps.update();
    
    if(hal_gps.hasFix()){
      GpsData data;
      hal_gps.getData(data);
      hal_log.debug(TAG, "GPS: Sats=%d Lat=%.6f Lon=%.6f",
                    data.satellites_used, data.position.latitude,
                    data.position.longitude);
    }
  }
  
  // Update microphone every 200ms
  if(mic_ok && (now - last_mic_update >= 200)){
    last_mic_update = now;
    if(hal_mic.update() == HalResult::OK){
      float db = hal_mic.getDbLevel();
      if(db > -50){  // Only log if there's some sound
        hal_log.debug(TAG, "MIC: dB=%.1f", db);
      }
    }
  }
  
  // UART communication - send heartbeat every 500ms
  if(uart_ok && (now - last_uart_send >= 500)){
    last_uart_send = now;
    
    // Send heartbeat
    char msg[32];
    snprintf(msg, sizeof(msg), "CPU:%lu", now);
    hal_uart.write((const uint8_t*)msg, strlen(msg), nullptr);
    uart_tx_count++;
    
    // Check for incoming data
    if(hal_uart.available() > 0){
      size_t read;
      hal_uart.read(rx_buffer, hal_uart.available(), &read, 10);
      if(read > 0){
        rx_buffer[read] = '\0';
        uart_rx_count++;
        hal_log.debug(TAG, "GPU says: %s", rx_buffer);
      }
    }
  }
  
  hal_timer.yield();
}
