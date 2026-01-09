/*****************************************************************
 * CPU_FrameworkAPI_SandSim.cpp - 2D Falling Sand Simulation
 * 
 * A cellular automaton sand simulation that demonstrates:
 * - GPU communication with optimized batch rendering
 * - Real-time particle physics simulation
 * - Multiple particle types (sand, water, stone, fire)
 * - Clean, composable GPU API wrapper
 * - IMU-based gravity control (tilt to change gravity direction!)
 * - Environmental sensor integration (BME280 temperature/humidity/pressure)
 * - GPS location tracking
 * - Microphone audio input for sound-reactive particles!
 * 
 * Controls:
 *   A = Cycle particle type
 *   B = Spawn particles
 *   C = Clear screen
 *   D = Toggle IMU gravity control
 *   TILT = Change gravity direction (when IMU enabled)
 * 
 * Hardware:
 *   - CPU: ESP32-S3 (ESP-IDF)
 *   - GPU: ESP32-S3 with HUB75 (128x32) + OLED (128x128)
 *   - UART: TX=GPIO12, RX=GPIO11 @ 10 Mbps
 *   - IMU: ICM20948 on I2C (SDA=9, SCL=10)
 *   - Environmental: BME280 on I2C (addr=0x76)
 *   - GPS: NEO-8M on UART2 (TX=43, RX=44)
 *   - Microphone: INMP441 on I2S
 *****************************************************************/

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <cstdlib>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/i2s_std.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char* TAG = "SAND_SIM";

// ============================================================
// Configuration
// ============================================================

// UART
#define GPU_UART_NUM    UART_NUM_1
#define GPU_UART_TX     GPIO_NUM_12
#define GPU_UART_RX     GPIO_NUM_11
#define GPU_BAUD        10000000

// Display dimensions
#define HUB75_W         128
#define HUB75_H         32
#define OLED_W          128
#define OLED_H          128

// Buttons (directly poll GPIO - faster than ADC)
#define BTN_A_PIN       GPIO_NUM_5
#define BTN_B_PIN       GPIO_NUM_6
#define BTN_C_PIN       GPIO_NUM_7
#define BTN_D_PIN       GPIO_NUM_15

// I2C for IMU
#define I2C_SDA_PIN     GPIO_NUM_9
#define I2C_SCL_PIN     GPIO_NUM_10
#define IMU_I2C_ADDR    0x68
#define BME280_I2C_ADDR 0x76

// GPS UART (UART2)
#define GPS_UART_NUM    UART_NUM_2
#define GPS_UART_TX     GPIO_NUM_43
#define GPS_UART_RX     GPIO_NUM_44
#define GPS_BAUD        9600

// I2S Microphone (INMP441)
#define MIC_I2S_NUM     I2S_NUM_0
#define MIC_WS_PIN      GPIO_NUM_42
#define MIC_BCK_PIN     GPIO_NUM_40
#define MIC_SD_PIN      GPIO_NUM_2
#define MIC_LR_SEL_PIN  GPIO_NUM_41

// Simulation grid (1:1 with HUB75 pixels)
#define GRID_W          HUB75_W
#define GRID_H          HUB75_H

// IMU gravity control thresholds
#define GRAVITY_TILT_THRESHOLD  0.3f   // Minimum tilt to change gravity direction
#define GRAVITY_DEAD_ZONE       0.1f   // Dead zone for "no gravity" state

// ============================================================
// GPU Command Protocol
// ============================================================

enum class Cmd : uint8_t {
  NOP             = 0x00,
  DRAW_PIXEL      = 0x40,
  DRAW_LINE       = 0x41,
  DRAW_RECT       = 0x42,
  DRAW_FILL       = 0x43,
  DRAW_CIRCLE     = 0x44,
  CLEAR           = 0x47,
  SET_TARGET      = 0x50,
  PRESENT         = 0x51,
  OLED_CLEAR      = 0x60,
  OLED_LINE       = 0x61,
  OLED_FILL       = 0x63,
  OLED_CIRCLE     = 0x64,
  OLED_PRESENT    = 0x65,
  PING            = 0xF0,
  RESET           = 0xFF,
};

// ============================================================
// Color Helper
// ============================================================

struct Color {
  uint8_t r, g, b;
  
  constexpr Color() : r(0), g(0), b(0) {}
  constexpr Color(uint8_t r, uint8_t g, uint8_t b) : r(r), g(g), b(b) {}
  
  // Predefined colors
  static constexpr Color black()   { return Color(0, 0, 0); }
  static constexpr Color white()   { return Color(255, 255, 255); }
  static constexpr Color red()     { return Color(255, 0, 0); }
  static constexpr Color green()   { return Color(0, 255, 0); }
  static constexpr Color blue()    { return Color(0, 0, 255); }
  static constexpr Color yellow()  { return Color(255, 255, 0); }
  static constexpr Color cyan()    { return Color(0, 255, 255); }
  static constexpr Color magenta() { return Color(255, 0, 255); }
  static constexpr Color orange()  { return Color(255, 128, 0); }
  
  // Create from HSV (h: 0-360, s: 0-100, v: 0-100)
  static Color fromHSV(int h, int s, int v) {
    h = h % 360;
    float fs = s / 100.0f;
    float fv = v / 100.0f;
    float c = fv * fs;
    float x = c * (1.0f - fabsf(fmodf(h / 60.0f, 2.0f) - 1.0f));
    float m = fv - c;
    float r, g, b;
    if (h < 60)      { r = c; g = x; b = 0; }
    else if (h < 120) { r = x; g = c; b = 0; }
    else if (h < 180) { r = 0; g = c; b = x; }
    else if (h < 240) { r = 0; g = x; b = c; }
    else if (h < 300) { r = x; g = 0; b = c; }
    else              { r = c; g = 0; b = x; }
    return Color((uint8_t)((r + m) * 255), (uint8_t)((g + m) * 255), (uint8_t)((b + m) * 255));
  }
  
  // Blend with another color
  Color blend(const Color& other, float t) const {
    return Color(
      (uint8_t)(r + (other.r - r) * t),
      (uint8_t)(g + (other.g - g) * t),
      (uint8_t)(b + (other.b - b) * t)
    );
  }
  
  // Darken/brighten
  Color darken(float factor) const {
    return Color((uint8_t)(r * factor), (uint8_t)(g * factor), (uint8_t)(b * factor));
  }
  
  bool operator==(const Color& o) const { return r == o.r && g == o.g && b == o.b; }
  bool operator!=(const Color& o) const { return !(*this == o); }
};

// ============================================================
// GPU Display Driver - Clean, Composable API
// ============================================================

class GpuDisplay {
private:
  bool initialized_ = false;
  
  void send(Cmd cmd, const uint8_t* data = nullptr, uint16_t len = 0) {
    uint8_t header[5] = {
      0xAA, 0x55,
      static_cast<uint8_t>(cmd),
      static_cast<uint8_t>(len & 0xFF),
      static_cast<uint8_t>((len >> 8) & 0xFF)
    };
    uart_write_bytes(GPU_UART_NUM, header, 5);
    if (len > 0 && data) {
      uart_write_bytes(GPU_UART_NUM, data, len);
    }
    uart_wait_tx_done(GPU_UART_NUM, pdMS_TO_TICKS(10));
  }
  
  // Pack int16 to bytes (little endian)
  static void pack16(uint8_t* buf, int16_t val) {
    buf[0] = val & 0xFF;
    buf[1] = (val >> 8) & 0xFF;
  }

public:
  // ---- Initialization ----
  bool init() {
    uart_config_t cfg = {
      .baud_rate = GPU_BAUD,
      .data_bits = UART_DATA_8_BITS,
      .parity = UART_PARITY_DISABLE,
      .stop_bits = UART_STOP_BITS_1,
      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
      .rx_flow_ctrl_thresh = 0,
      .source_clk = UART_SCLK_DEFAULT,
      .flags = { .allow_pd = 0, .backup_before_sleep = 0 }
    };
    
    // IMPORTANT: driver_install BEFORE param_config!
    esp_err_t err = uart_driver_install(GPU_UART_NUM, 4096, 4096, 0, nullptr, 0);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "UART driver install failed: %d", err);
      return false;
    }
    
    err = uart_param_config(GPU_UART_NUM, &cfg);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "UART param config failed: %d", err);
      return false;
    }
    
    err = uart_set_pin(GPU_UART_NUM, GPU_UART_TX, GPU_UART_RX, -1, -1);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "UART set pin failed: %d", err);
      return false;
    }
    
    ESP_LOGI(TAG, "GPU UART OK: TX=%d, RX=%d, %d baud", GPU_UART_TX, GPU_UART_RX, GPU_BAUD);
    initialized_ = true;
    
    // Handshake with GPU
    vTaskDelay(pdMS_TO_TICKS(100));
    ping();
    vTaskDelay(pdMS_TO_TICKS(50));
    reset();
    vTaskDelay(pdMS_TO_TICKS(100));
    
    return true;
  }
  
  // ---- System Commands ----
  void ping() { send(Cmd::PING); }
  void reset() { send(Cmd::RESET); }
  
  // ---- HUB75 Display (128x32 RGB) ----
  
  void hub75_clear(Color c = Color::black()) {
    uint8_t data[3] = {c.r, c.g, c.b};
    send(Cmd::SET_TARGET, (const uint8_t*)"\x00", 1);
    send(Cmd::CLEAR, data, 3);
  }
  
  void hub75_present() {
    send(Cmd::SET_TARGET, (const uint8_t*)"\x00", 1);
    send(Cmd::PRESENT);
  }
  
  void hub75_pixel(int16_t x, int16_t y, Color c) {
    if (x < 0 || x >= HUB75_W || y < 0 || y >= HUB75_H) return;
    uint8_t data[7];
    pack16(&data[0], x);
    pack16(&data[2], y);
    data[4] = c.r; data[5] = c.g; data[6] = c.b;
    send(Cmd::SET_TARGET, (const uint8_t*)"\x00", 1);
    send(Cmd::DRAW_PIXEL, data, 7);
  }
  
  void hub75_line(int16_t x1, int16_t y1, int16_t x2, int16_t y2, Color c) {
    uint8_t data[11];
    pack16(&data[0], x1);
    pack16(&data[2], y1);
    pack16(&data[4], x2);
    pack16(&data[6], y2);
    data[8] = c.r; data[9] = c.g; data[10] = c.b;
    send(Cmd::SET_TARGET, (const uint8_t*)"\x00", 1);
    send(Cmd::DRAW_LINE, data, 11);
  }
  
  void hub75_rect(int16_t x, int16_t y, int16_t w, int16_t h, Color c) {
    uint8_t data[11];
    pack16(&data[0], x);
    pack16(&data[2], y);
    pack16(&data[4], w);
    pack16(&data[6], h);
    data[8] = c.r; data[9] = c.g; data[10] = c.b;
    send(Cmd::SET_TARGET, (const uint8_t*)"\x00", 1);
    send(Cmd::DRAW_RECT, data, 11);
  }
  
  void hub75_fill(int16_t x, int16_t y, int16_t w, int16_t h, Color c) {
    uint8_t data[11];
    pack16(&data[0], x);
    pack16(&data[2], y);
    pack16(&data[4], w);
    pack16(&data[6], h);
    data[8] = c.r; data[9] = c.g; data[10] = c.b;
    send(Cmd::SET_TARGET, (const uint8_t*)"\x00", 1);
    send(Cmd::DRAW_FILL, data, 11);
  }
  
  void hub75_circle(int16_t cx, int16_t cy, int16_t r, Color c) {
    uint8_t data[9];
    pack16(&data[0], cx);
    pack16(&data[2], cy);
    pack16(&data[4], r);
    data[6] = c.r; data[7] = c.g; data[8] = c.b;
    send(Cmd::SET_TARGET, (const uint8_t*)"\x00", 1);
    send(Cmd::DRAW_CIRCLE, data, 9);
  }
  
  // ---- OLED Display (128x128 Monochrome) ----
  
  void oled_clear() {
    send(Cmd::OLED_CLEAR);
  }
  
  void oled_present() {
    send(Cmd::OLED_PRESENT);
  }
  
  void oled_line(int16_t x1, int16_t y1, int16_t x2, int16_t y2, bool on = true) {
    uint8_t data[9];
    pack16(&data[0], x1);
    pack16(&data[2], y1);
    pack16(&data[4], x2);
    pack16(&data[6], y2);
    data[8] = on ? 1 : 0;
    send(Cmd::OLED_LINE, data, 9);
  }
  
  void oled_fill(int16_t x, int16_t y, int16_t w, int16_t h, bool on = true) {
    uint8_t data[9];
    pack16(&data[0], x);
    pack16(&data[2], y);
    pack16(&data[4], w);
    pack16(&data[6], h);
    data[8] = on ? 1 : 0;
    send(Cmd::OLED_FILL, data, 9);
  }
  
  void oled_circle(int16_t cx, int16_t cy, int16_t r, bool on = true) {
    uint8_t data[7];
    pack16(&data[0], cx);
    pack16(&data[2], cy);
    pack16(&data[4], r);
    data[6] = on ? 1 : 0;
    send(Cmd::OLED_CIRCLE, data, 7);
  }
  
  // ---- Batch Pixel Drawing (optimized for many particles) ----
  // Note: Due to UART overhead, we'll send pixels in batches
  
  void hub75_pixels_begin() {
    send(Cmd::SET_TARGET, (const uint8_t*)"\x00", 1);
  }
  
  void hub75_pixel_raw(int16_t x, int16_t y, Color c) {
    if (x < 0 || x >= HUB75_W || y < 0 || y >= HUB75_H) return;
    uint8_t data[7];
    pack16(&data[0], x);
    pack16(&data[2], y);
    data[4] = c.r; data[5] = c.g; data[6] = c.b;
    send(Cmd::DRAW_PIXEL, data, 7);
  }
};

// Global GPU instance
static GpuDisplay gpu;

// ============================================================
// Particle Types
// ============================================================

enum class ParticleType : uint8_t {
  EMPTY = 0,
  SAND,
  WATER,
  STONE,
  FIRE,
  WOOD,
  OIL,
  COUNT
};

// Particle colors (with slight variation)
static Color getParticleColor(ParticleType type, int variation = 0) {
  switch (type) {
    case ParticleType::SAND:   return Color(220 - variation*3, 180 - variation*2, 80 + variation*2);
    case ParticleType::WATER:  return Color(30 + variation*2, 100 + variation*3, 200 + variation*2);
    case ParticleType::STONE:  return Color(100 + variation*2, 100 + variation*2, 110 + variation*2);
    case ParticleType::FIRE:   return Color(255, 100 + variation*10, variation*5);
    case ParticleType::WOOD:   return Color(139 - variation*3, 90 - variation*2, 43 + variation);
    case ParticleType::OIL:    return Color(60 + variation, 50 + variation, 30 + variation);
    default:                   return Color::black();
  }
}

static const char* getParticleName(ParticleType type) {
  switch (type) {
    case ParticleType::SAND:   return "SAND";
    case ParticleType::WATER:  return "WATER";
    case ParticleType::STONE:  return "STONE";
    case ParticleType::FIRE:   return "FIRE";
    case ParticleType::WOOD:   return "WOOD";
    case ParticleType::OIL:    return "OIL";
    default:                   return "EMPTY";
  }
}

// ============================================================
// Simulation Grid
// ============================================================

struct Particle {
  ParticleType type = ParticleType::EMPTY;
  uint8_t variation = 0;   // Color variation
  uint8_t lifetime = 255;  // For fire
  bool updated = false;    // Already processed this frame
};

// ============================================================
// Sensor Data Structures (defined early for use in SandSimulation)
// ============================================================

struct ImuData {
  float accelX, accelY, accelZ;
  float gyroX, gyroY, gyroZ;
  float temperature;
  bool valid;
};

struct EnvironmentalData {
  float temperature;    // °C
  float humidity;       // %RH
  float pressure;       // hPa
  float altitude;       // meters (calculated from pressure)
  bool valid;
};

struct GpsData {
  float latitude;       // degrees (positive = N, negative = S)
  float longitude;      // degrees (positive = E, negative = W)
  float altitude;       // meters above sea level
  float speed;          // km/h
  float course;         // degrees
  int satellites;       // number of satellites in view
  int hour, minute, second;  // UTC time
  int day, month, year;      // UTC date
  bool hasfix;          // true if GPS has a valid fix
  bool valid;           // true if data was read successfully
};

struct AudioData {
  int32_t level;      // Raw audio level (absolute amplitude)
  float levelDb;      // Level in dB (rough estimate)
  float peakLevel;    // Peak level (for VU meter)
  bool valid;
};

struct AllSensorData {
  ImuData imu;
  EnvironmentalData env;
  GpsData gps;
  AudioData audio;
};

class SandSimulation {
private:
  Particle grid_[GRID_W * GRID_H];
  int frameCount_ = 0;
  
  // Gravity can now be in any direction (from IMU)
  float gravityX_ = 0.0f;     // -1 to +1 (left to right)
  float gravityY_ = 1.0f;     // -1 to +1 (up to down, positive = down)
  bool imuGravityEnabled_ = true;  // Use IMU for gravity
  
  Particle& at(int x, int y) {
    return grid_[y * GRID_W + x];
  }
  
  bool inBounds(int x, int y) const {
    return x >= 0 && x < GRID_W && y >= 0 && y < GRID_H;
  }
  
  bool isEmpty(int x, int y) const {
    if (!inBounds(x, y)) return false;
    return grid_[y * GRID_W + x].type == ParticleType::EMPTY;
  }
  
  bool isLiquid(int x, int y) const {
    if (!inBounds(x, y)) return false;
    ParticleType t = grid_[y * GRID_W + x].type;
    return t == ParticleType::WATER || t == ParticleType::OIL;
  }
  
  void swap(int x1, int y1, int x2, int y2) {
    if (!inBounds(x1, y1) || !inBounds(x2, y2)) return;
    Particle temp = at(x1, y1);
    at(x1, y1) = at(x2, y2);
    at(x2, y2) = temp;
    at(x1, y1).updated = true;
    at(x2, y2).updated = true;
  }
  
  void move(int fromX, int fromY, int toX, int toY) {
    if (!inBounds(fromX, fromY) || !inBounds(toX, toY)) return;
    at(toX, toY) = at(fromX, fromY);
    at(fromX, fromY).type = ParticleType::EMPTY;
    at(toX, toY).updated = true;
  }
  
  // Get primary gravity direction as discrete step
  void getGravityStep(int& dx, int& dy) const {
    // Determine dominant gravity axis
    float absX = fabsf(gravityX_);
    float absY = fabsf(gravityY_);
    
    if (absX < GRAVITY_DEAD_ZONE && absY < GRAVITY_DEAD_ZONE) {
      // Very little gravity - particles float
      dx = 0;
      dy = 0;
      return;
    }
    
    // Primary direction based on stronger axis
    if (absY >= absX) {
      dx = 0;
      dy = (gravityY_ > 0) ? 1 : -1;
    } else {
      dx = (gravityX_ > 0) ? 1 : -1;
      dy = 0;
    }
  }
  
  // Get diagonal fall directions based on gravity
  void getDiagonalDirs(int& diag1X, int& diag1Y, int& diag2X, int& diag2Y) const {
    int dx, dy;
    getGravityStep(dx, dy);
    
    if (dy != 0) {
      // Vertical gravity - diagonal means left/right + down/up
      diag1X = -1; diag1Y = dy;
      diag2X = 1;  diag2Y = dy;
    } else if (dx != 0) {
      // Horizontal gravity - diagonal means up/down + left/right  
      diag1X = dx; diag1Y = -1;
      diag2X = dx; diag2Y = 1;
    } else {
      // No gravity - no diagonal preference
      diag1X = 0; diag1Y = 0;
      diag2X = 0; diag2Y = 0;
    }
  }
  
  // Update a single sand particle with 2D gravity
  void updateSand(int x, int y) {
    int dx, dy;
    getGravityStep(dx, dy);
    
    // No gravity - sand floats
    if (dx == 0 && dy == 0) return;
    
    int nx = x + dx;
    int ny = y + dy;
    
    // Try to fall in gravity direction
    if (isEmpty(nx, ny)) {
      move(x, y, nx, ny);
      return;
    }
    
    // Try diagonal falls
    int d1x, d1y, d2x, d2y;
    getDiagonalDirs(d1x, d1y, d2x, d2y);
    
    int dir = (rand() % 2) ? 0 : 1;  // Randomize which diagonal first
    int tryX1 = x + (dir ? d1x : d2x);
    int tryY1 = y + (dir ? d1y : d2y);
    int tryX2 = x + (dir ? d2x : d1x);
    int tryY2 = y + (dir ? d2y : d1y);
    
    if (isEmpty(tryX1, tryY1)) {
      move(x, y, tryX1, tryY1);
      return;
    }
    if (isEmpty(tryX2, tryY2)) {
      move(x, y, tryX2, tryY2);
      return;
    }
    
    // Try to displace liquids
    if (isLiquid(nx, ny)) {
      swap(x, y, nx, ny);
      return;
    }
    if (isLiquid(tryX1, tryY1)) {
      swap(x, y, tryX1, tryY1);
      return;
    }
    if (isLiquid(tryX2, tryY2)) {
      swap(x, y, tryX2, tryY2);
      return;
    }
  }
  
  // Update a water particle with 2D gravity
  void updateWater(int x, int y) {
    int dx, dy;
    getGravityStep(dx, dy);
    
    // No gravity - water floats
    if (dx == 0 && dy == 0) return;
    
    int nx = x + dx;
    int ny = y + dy;
    
    // Try to fall in gravity direction
    if (isEmpty(nx, ny)) {
      move(x, y, nx, ny);
      return;
    }
    
    // Try diagonal falls
    int d1x, d1y, d2x, d2y;
    getDiagonalDirs(d1x, d1y, d2x, d2y);
    
    int dir = (rand() % 2) ? 0 : 1;
    if (isEmpty(x + (dir ? d1x : d2x), y + (dir ? d1y : d2y))) {
      move(x, y, x + (dir ? d1x : d2x), y + (dir ? d1y : d2y));
      return;
    }
    if (isEmpty(x + (dir ? d2x : d1x), y + (dir ? d2y : d1y))) {
      move(x, y, x + (dir ? d2x : d1x), y + (dir ? d2y : d1y));
      return;
    }
    
    // Spread perpendicular to gravity (flow)
    int spreadDirs[2][2];
    if (dy != 0) {
      // Vertical gravity - spread horizontally
      spreadDirs[0][0] = 1;  spreadDirs[0][1] = 0;
      spreadDirs[1][0] = -1; spreadDirs[1][1] = 0;
    } else {
      // Horizontal gravity - spread vertically
      spreadDirs[0][0] = 0;  spreadDirs[0][1] = 1;
      spreadDirs[1][0] = 0;  spreadDirs[1][1] = -1;
    }
    
    dir = rand() % 2;
    if (isEmpty(x + spreadDirs[dir][0], y + spreadDirs[dir][1])) {
      move(x, y, x + spreadDirs[dir][0], y + spreadDirs[dir][1]);
      return;
    }
    if (isEmpty(x + spreadDirs[1-dir][0], y + spreadDirs[1-dir][1])) {
      move(x, y, x + spreadDirs[1-dir][0], y + spreadDirs[1-dir][1]);
      return;
    }
  }
  
  // Update a fire particle with 2D gravity
  void updateFire(int x, int y) {
    Particle& p = at(x, y);
    
    // Fire rises (opposite of gravity)
    int dx, dy;
    getGravityStep(dx, dy);
    // Invert for fire (rises against gravity)
    dx = -dx;
    dy = -dy;
    
    // Decrease lifetime
    if (p.lifetime > 0) {
      p.lifetime -= (rand() % 15) + 5;
      if (p.lifetime < 10 || p.lifetime > 250) {  // Underflow or nearly dead
        p.type = ParticleType::EMPTY;
        return;
      }
    }
    
    // Try to rise with some randomness
    int randDir = (rand() % 3) - 1;  // -1, 0, or 1
    int nx = x + dx + (dy != 0 ? randDir : 0);
    int ny = y + dy + (dx != 0 ? randDir : 0);
    
    if (isEmpty(nx, ny)) {
      move(x, y, nx, ny);
      return;
    }
    if (isEmpty(x + dx, y + dy)) {
      move(x, y, x + dx, y + dy);
      return;
    }
    
    // Spread fire to adjacent wood/oil
    for (int ddx = -1; ddx <= 1; ddx++) {
      for (int ddy = -1; ddy <= 1; ddy++) {
        if (ddx == 0 && ddy == 0) continue;
        int adjX = x + ddx;
        int adjY = y + ddy;
        if (inBounds(adjX, adjY)) {
          ParticleType adjType = at(adjX, adjY).type;
          if ((adjType == ParticleType::WOOD || adjType == ParticleType::OIL) && (rand() % 10) < 2) {
            at(adjX, adjY).type = ParticleType::FIRE;
            at(adjX, adjY).lifetime = 200 + (rand() % 55);
            at(adjX, adjY).variation = rand() % 10;
          }
        }
      }
    }
    
    // Flicker variation
    p.variation = rand() % 10;
  }
  
  // Update oil particle with 2D gravity
  void updateOil(int x, int y) {
    int dx, dy;
    getGravityStep(dx, dy);
    
    // No gravity - oil floats
    if (dx == 0 && dy == 0) return;
    
    int nx = x + dx;
    int ny = y + dy;
    
    // Oil floats on water - check if water in gravity direction
    if (inBounds(nx, ny) && at(nx, ny).type == ParticleType::WATER) {
      swap(x, y, nx, ny);
      return;
    }
    
    // Fall like water otherwise
    if (isEmpty(nx, ny)) {
      move(x, y, nx, ny);
      return;
    }
    
    // Try diagonal falls
    int d1x, d1y, d2x, d2y;
    getDiagonalDirs(d1x, d1y, d2x, d2y);
    
    int dir = rand() % 2;
    if (isEmpty(x + (dir ? d1x : d2x), y + (dir ? d1y : d2y))) {
      move(x, y, x + (dir ? d1x : d2x), y + (dir ? d1y : d2y));
      return;
    }
    if (isEmpty(x + (dir ? d2x : d1x), y + (dir ? d2y : d1y))) {
      move(x, y, x + (dir ? d2x : d1x), y + (dir ? d2y : d1y));
      return;
    }
    
    // Spread perpendicular to gravity
    int spreadDirs[2][2];
    if (dy != 0) {
      spreadDirs[0][0] = 1;  spreadDirs[0][1] = 0;
      spreadDirs[1][0] = -1; spreadDirs[1][1] = 0;
    } else {
      spreadDirs[0][0] = 0;  spreadDirs[0][1] = 1;
      spreadDirs[1][0] = 0;  spreadDirs[1][1] = -1;
    }
    
    dir = rand() % 2;
    if (isEmpty(x + spreadDirs[dir][0], y + spreadDirs[dir][1])) {
      move(x, y, x + spreadDirs[dir][0], y + spreadDirs[dir][1]);
      return;
    }
    if (isEmpty(x + spreadDirs[1-dir][0], y + spreadDirs[1-dir][1])) {
      move(x, y, x + spreadDirs[1-dir][0], y + spreadDirs[1-dir][1]);
      return;
    }
  }

public:
  int particleCount = 0;
  ParticleType selectedType = ParticleType::SAND;
  int brushSize = 3;
  
  SandSimulation() {
    clear();
  }
  
  void clear() {
    for (int i = 0; i < GRID_W * GRID_H; i++) {
      grid_[i].type = ParticleType::EMPTY;
      grid_[i].updated = false;
    }
    particleCount = 0;
  }
  
  // Toggle IMU-based gravity control
  void toggleImuGravity() {
    imuGravityEnabled_ = !imuGravityEnabled_;
    ESP_LOGI(TAG, "IMU Gravity: %s", imuGravityEnabled_ ? "ENABLED" : "DISABLED");
    if (!imuGravityEnabled_) {
      // Reset to default down gravity
      gravityX_ = 0.0f;
      gravityY_ = 1.0f;
    }
  }
  
  bool isImuGravityEnabled() const { return imuGravityEnabled_; }
  
  // Set gravity from IMU accelerometer readings
  // accelX: positive = tilt right, negative = tilt left
  // accelY: positive = tilt forward, negative = tilt back  
  // Note: When device is flat, accelZ ~= 1g (pointing down)
  void setGravityFromIMU(float accelX, float accelY, float accelZ) {
    if (!imuGravityEnabled_) return;
    
    // Map accelerometer to gravity direction
    // accelX affects horizontal gravity
    // accelY affects vertical gravity (forward/back tilt of device)
    // We're mapping the device orientation to screen gravity
    
    // Normalize and apply threshold
    float magnitude = sqrtf(accelX*accelX + accelY*accelY + accelZ*accelZ);
    if (magnitude < 0.1f) magnitude = 1.0f;  // Avoid division by zero
    
    // Use X and Y accelerometer to determine gravity direction
    // Scale factor to make gravity responsive but not too sensitive
    gravityX_ = accelX / magnitude * 2.0f;
    gravityY_ = accelY / magnitude * 2.0f;
    
    // Clamp values
    if (gravityX_ > 1.0f) gravityX_ = 1.0f;
    if (gravityX_ < -1.0f) gravityX_ = -1.0f;
    if (gravityY_ > 1.0f) gravityY_ = 1.0f;
    if (gravityY_ < -1.0f) gravityY_ = -1.0f;
  }
  
  // Get current gravity for display
  float getGravityX() const { return gravityX_; }
  float getGravityY() const { return gravityY_; }
  
  void cycleParticleType() {
    int t = (int)selectedType + 1;
    if (t >= (int)ParticleType::COUNT || t == 0) t = 1;  // Skip EMPTY
    selectedType = (ParticleType)t;
    ESP_LOGI(TAG, "Selected: %s", getParticleName(selectedType));
  }
  
  // Spawn particles at position with brush
  void spawn(int cx, int cy) {
    for (int dy = -brushSize; dy <= brushSize; dy++) {
      for (int dx = -brushSize; dx <= brushSize; dx++) {
        // Circular brush
        if (dx*dx + dy*dy > brushSize*brushSize) continue;
        
        int x = cx + dx;
        int y = cy + dy;
        
        if (!inBounds(x, y)) continue;
        if (at(x, y).type != ParticleType::EMPTY) continue;
        
        // Some randomness to brush density
        if (rand() % 3 == 0) continue;
        
        at(x, y).type = selectedType;
        at(x, y).variation = rand() % 10;
        at(x, y).lifetime = 255;
        particleCount++;
      }
    }
  }
  
  // Update simulation one step
  void update() {
    frameCount_++;
    
    // Clear update flags
    for (int i = 0; i < GRID_W * GRID_H; i++) {
      grid_[i].updated = false;
    }
    
    // Determine scan direction based on gravity
    // Process in direction opposite to gravity so particles can fall
    int dx, dy;
    getGravityStep(dx, dy);
    
    // Vertical scan order
    int startY, endY, stepY;
    if (dy >= 0) {
      // Gravity down or horizontal: scan bottom to top
      startY = GRID_H - 1;
      endY = -1;
      stepY = -1;
    } else {
      // Gravity up: scan top to bottom
      startY = 0;
      endY = GRID_H;
      stepY = 1;
    }
    
    // Horizontal scan order
    int startX, endX, stepX;
    if (dx >= 0) {
      // Gravity right or vertical: scan right to left
      startX = GRID_W - 1;
      endX = -1;
      stepX = -1;
    } else {
      // Gravity left: scan left to right
      startX = 0;
      endX = GRID_W;
      stepX = 1;
    }
    
    for (int y = startY; y != endY; y += stepY) {
      // Alternate horizontal scan for natural look
      int actualStartX = ((frameCount_ + y) % 2) ? startX : (endX - stepX);
      int actualEndX = ((frameCount_ + y) % 2) ? endX : (startX - stepX);
      int actualStepX = ((frameCount_ + y) % 2) ? stepX : -stepX;
      
      for (int x = actualStartX; x != actualEndX; x += actualStepX) {
        Particle& p = at(x, y);
        if (p.updated) continue;
        
        switch (p.type) {
          case ParticleType::SAND:
            updateSand(x, y);
            break;
          case ParticleType::WATER:
            updateWater(x, y);
            break;
          case ParticleType::FIRE:
            updateFire(x, y);
            break;
          case ParticleType::OIL:
            updateOil(x, y);
            break;
          case ParticleType::STONE:
          case ParticleType::WOOD:
            // Static particles don't move
            break;
          default:
            break;
        }
      }
    }
    
    // Recount particles
    particleCount = 0;
    for (int i = 0; i < GRID_W * GRID_H; i++) {
      if (grid_[i].type != ParticleType::EMPTY) {
        particleCount++;
      }
    }
  }
  
  // Render to HUB75 display
  void render() {
    gpu.hub75_clear(Color::black());
    
    // Draw all non-empty particles
    gpu.hub75_pixels_begin();
    for (int y = 0; y < GRID_H; y++) {
      for (int x = 0; x < GRID_W; x++) {
        const Particle& p = grid_[y * GRID_W + x];
        if (p.type != ParticleType::EMPTY) {
          Color c = getParticleColor(p.type, p.variation);
          
          // Fire brightness varies with lifetime
          if (p.type == ParticleType::FIRE) {
            float brightness = p.lifetime / 255.0f;
            c = c.darken(0.3f + brightness * 0.7f);
          }
          
          gpu.hub75_pixel_raw(x, y, c);
        }
      }
    }
    
    gpu.hub75_present();
  }
  
  // Render stats to OLED with all sensor data
  void renderOLED(const AllSensorData& sensors) {
    gpu.oled_clear();
    
    // Draw border
    gpu.oled_line(0, 0, 127, 0, true);
    gpu.oled_line(127, 0, 127, 127, true);
    gpu.oled_line(127, 127, 0, 127, true);
    gpu.oled_line(0, 127, 0, 0, true);
    
    // Title area
    gpu.oled_fill(4, 4, 120, 10, true);
    gpu.oled_fill(6, 6, 116, 6, false);
    
    // Particle count bar graph (row 1)
    int barWidth = (particleCount * 100) / (GRID_W * GRID_H);
    if (barWidth > 100) barWidth = 100;
    gpu.oled_fill(10, 18, barWidth, 6, true);
    gpu.oled_line(10, 24, 110, 24, true);
    
    // Selected particle type indicator (row 2)
    int typeY = 28;
    for (int i = 1; i < (int)ParticleType::COUNT; i++) {
      bool selected = (i == (int)selectedType);
      int boxX = 10 + (i - 1) * 18;
      
      if (selected) {
        gpu.oled_fill(boxX - 1, typeY - 1, 12, 12, true);
        gpu.oled_fill(boxX + 1, typeY + 1, 8, 8, false);
      } else {
        gpu.oled_fill(boxX, typeY, 10, 10, true);
      }
    }
    
    // Gravity direction indicator (with IMU status)
    int gravCenterX = 100;
    int gravCenterY = 33;
    int gravRadius = 10;
    gpu.oled_circle(gravCenterX, gravCenterY, gravRadius, true);
    
    int arrowEndX = gravCenterX + (int)(gravityX_ * gravRadius * 0.8f);
    int arrowEndY = gravCenterY + (int)(gravityY_ * gravRadius * 0.8f);
    gpu.oled_line(gravCenterX, gravCenterY, arrowEndX, arrowEndY, true);
    
    // ---- Environmental Data (row 3) ----
    // Temperature bar
    int envY = 46;
    gpu.oled_line(5, envY, 5, envY + 18, true);  // Thermometer outline
    gpu.oled_line(10, envY, 10, envY + 18, true);
    gpu.oled_line(5, envY, 10, envY, true);
    gpu.oled_line(5, envY + 18, 10, envY + 18, true);
    
    if (sensors.env.valid) {
      // Fill thermometer based on temp (0-40°C range)
      int tempFill = (int)((sensors.env.temperature / 40.0f) * 16);
      if (tempFill < 0) tempFill = 0;
      if (tempFill > 16) tempFill = 16;
      gpu.oled_fill(6, envY + 17 - tempFill, 4, tempFill + 1, true);
    }
    
    // Humidity bar
    gpu.oled_line(20, envY, 20, envY + 18, true);
    gpu.oled_line(25, envY, 25, envY + 18, true);
    gpu.oled_line(20, envY, 25, envY, true);
    gpu.oled_line(20, envY + 18, 25, envY + 18, true);
    
    if (sensors.env.valid) {
      int humFill = (int)((sensors.env.humidity / 100.0f) * 16);
      if (humFill < 0) humFill = 0;
      if (humFill > 16) humFill = 16;
      gpu.oled_fill(21, envY + 17 - humFill, 4, humFill + 1, true);
    }
    
    // Pressure indicator (simple bar)
    gpu.oled_line(35, envY, 35, envY + 18, true);
    gpu.oled_line(40, envY, 40, envY + 18, true);
    gpu.oled_line(35, envY, 40, envY, true);
    gpu.oled_line(35, envY + 18, 40, envY + 18, true);
    
    if (sensors.env.valid) {
      // Map 950-1050 hPa to bar
      int presFill = (int)(((sensors.env.pressure - 950.0f) / 100.0f) * 16);
      if (presFill < 0) presFill = 0;
      if (presFill > 16) presFill = 16;
      gpu.oled_fill(36, envY + 17 - presFill, 4, presFill + 1, true);
    }
    
    // ---- Audio Level (VU meter) ----
    int audioY = envY;
    int audioX = 50;
    
    // VU meter outline
    gpu.oled_line(audioX, audioY, audioX + 60, audioY, true);
    gpu.oled_line(audioX, audioY + 18, audioX + 60, audioY + 18, true);
    gpu.oled_line(audioX, audioY, audioX, audioY + 18, true);
    gpu.oled_line(audioX + 60, audioY, audioX + 60, audioY + 18, true);
    
    if (sensors.audio.valid) {
      // Map audio level to bar (log scale approximation)
      int levelBar = (int)((sensors.audio.levelDb + 60.0f) / 60.0f * 56);
      if (levelBar < 0) levelBar = 0;
      if (levelBar > 56) levelBar = 56;
      gpu.oled_fill(audioX + 2, audioY + 4, levelBar, 10, true);
      
      // Peak indicator
      int peakBar = (int)(log10f(sensors.audio.peakLevel + 1) / 5.0f * 56);
      if (peakBar > 56) peakBar = 56;
      if (peakBar > 0) {
        gpu.oled_line(audioX + 2 + peakBar, audioY + 2, audioX + 2 + peakBar, audioY + 16, true);
      }
    }
    
    // ---- GPS Status (row 4) ----
    int gpsY = 70;
    
    // Satellite icon (simple)
    gpu.oled_circle(15, gpsY + 6, 5, true);
    
    // Satellite count indicator
    if (sensors.gps.valid) {
      int satBars = sensors.gps.satellites;
      if (satBars > 8) satBars = 8;
      for (int i = 0; i < satBars; i++) {
        gpu.oled_fill(25 + i * 6, gpsY + 10 - i, 4, 2 + i, true);
      }
      
      // Fix indicator
      if (sensors.gps.hasfix) {
        gpu.oled_fill(75, gpsY, 10, 10, true);  // Solid = fix
      } else {
        // Hollow = no fix
        gpu.oled_line(75, gpsY, 85, gpsY, true);
        gpu.oled_line(85, gpsY, 85, gpsY + 10, true);
        gpu.oled_line(85, gpsY + 10, 75, gpsY + 10, true);
        gpu.oled_line(75, gpsY + 10, 75, gpsY, true);
      }
      
      // Speed indicator bar (0-100 km/h range)
      int speedBar = (int)(sensors.gps.speed * 0.9f);
      if (speedBar > 90) speedBar = 90;
      if (speedBar > 0) {
        gpu.oled_fill(25, gpsY + 14, speedBar, 4, true);
      }
    }
    
    // ---- IMU Data (row 5) ----
    int imuY = 92;
    
    if (sensors.imu.valid) {
      // Accelerometer XYZ bars
      int accelX = (int)(sensors.imu.accelX * 15);
      int accelY = (int)(sensors.imu.accelY * 15);
      int accelZ = (int)((sensors.imu.accelZ - 1.0f) * 15);  // Subtract 1g for flat
      
      // X axis bar (centered)
      int barCenter = 30;
      if (accelX >= 0) {
        gpu.oled_fill(barCenter, imuY, accelX, 4, true);
      } else {
        gpu.oled_fill(barCenter + accelX, imuY, -accelX, 4, true);
      }
      
      // Y axis bar
      if (accelY >= 0) {
        gpu.oled_fill(barCenter, imuY + 6, accelY, 4, true);
      } else {
        gpu.oled_fill(barCenter + accelY, imuY + 6, -accelY, 4, true);
      }
      
      // Z axis bar
      if (accelZ >= 0) {
        gpu.oled_fill(barCenter, imuY + 12, accelZ, 4, true);
      } else {
        gpu.oled_fill(barCenter + accelZ, imuY + 12, -accelZ, 4, true);
      }
      
      // Center line
      gpu.oled_line(barCenter, imuY - 2, barCenter, imuY + 18, true);
      
      // Gyro rotation indicators (circles with direction)
      int gyroX = 80;
      gpu.oled_circle(gyroX, imuY + 4, 4, true);
      gpu.oled_circle(gyroX + 16, imuY + 4, 4, true);
      gpu.oled_circle(gyroX + 32, imuY + 4, 4, true);
      
      // Rotation arrows (simplified - show direction)
      if (fabsf(sensors.imu.gyroX) > 5) {
        int dir = sensors.imu.gyroX > 0 ? 1 : -1;
        gpu.oled_line(gyroX, imuY + 4, gyroX + dir * 3, imuY + 4 - 2, true);
      }
      if (fabsf(sensors.imu.gyroY) > 5) {
        int dir = sensors.imu.gyroY > 0 ? 1 : -1;
        gpu.oled_line(gyroX + 16, imuY + 4, gyroX + 16 + dir * 3, imuY + 4 - 2, true);
      }
      if (fabsf(sensors.imu.gyroZ) > 5) {
        int dir = sensors.imu.gyroZ > 0 ? 1 : -1;
        gpu.oled_line(gyroX + 32, imuY + 4, gyroX + 32, imuY + 4 + dir * 3, true);
      }
    }
    
    // IMU enabled indicator
    if (imuGravityEnabled_) {
      gpu.oled_fill(115, imuY, 10, 10, true);
    } else {
      gpu.oled_line(115, imuY, 125, imuY, true);
      gpu.oled_line(125, imuY, 125, imuY + 10, true);
      gpu.oled_line(125, imuY + 10, 115, imuY + 10, true);
      gpu.oled_line(115, imuY + 10, 115, imuY, true);
    }
    
    // ---- Status bar (bottom) ----
    gpu.oled_line(5, 115, 122, 115, true);
    
    // Sensor status indicators
    int statusY = 118;
    // IMU status
    if (sensors.imu.valid) gpu.oled_fill(10, statusY, 6, 6, true);
    else gpu.oled_line(10, statusY, 16, statusY + 6, true);
    
    // ENV status
    if (sensors.env.valid) gpu.oled_fill(25, statusY, 6, 6, true);
    else gpu.oled_line(25, statusY, 31, statusY + 6, true);
    
    // GPS status  
    if (sensors.gps.hasfix) gpu.oled_fill(40, statusY, 6, 6, true);
    else if (sensors.gps.valid) gpu.oled_circle(43, statusY + 3, 3, true);
    else gpu.oled_line(40, statusY, 46, statusY + 6, true);
    
    // MIC status
    if (sensors.audio.valid) gpu.oled_fill(55, statusY, 6, 6, true);
    else gpu.oled_line(55, statusY, 61, statusY + 6, true);
    
    gpu.oled_present();
  }
};

// Global simulation
static SandSimulation sim;

// ============================================================
// Input Handling
// ============================================================

struct InputState {
  bool btnA = false;
  bool btnB = false;
  bool btnC = false;
  bool btnD = false;
  bool prevA = false;
  bool prevB = false;
  bool prevC = false;
  bool prevD = false;
  int cursorX = GRID_W / 2;
  int cursorY = GRID_H / 2;
};

static InputState input;

static void initButtons() {
  gpio_config_t io_conf = {
    .pin_bit_mask = (1ULL << BTN_A_PIN) | (1ULL << BTN_B_PIN) | 
                    (1ULL << BTN_C_PIN) | (1ULL << BTN_D_PIN),
    .mode = GPIO_MODE_INPUT,
    .pull_up_en = GPIO_PULLUP_ENABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type = GPIO_INTR_DISABLE
  };
  gpio_config(&io_conf);
}

static void readInput() {
  input.prevA = input.btnA;
  input.prevB = input.btnB;
  input.prevC = input.btnC;
  input.prevD = input.btnD;
  
  input.btnA = !gpio_get_level(BTN_A_PIN);  // Active low
  input.btnB = !gpio_get_level(BTN_B_PIN);
  input.btnC = !gpio_get_level(BTN_C_PIN);
  input.btnD = !gpio_get_level(BTN_D_PIN);
}

static bool justPressed(bool curr, bool prev) {
  return curr && !prev;
}

// ============================================================
// ICM20948 IMU Driver (Direct ESP-IDF I2C)
// ============================================================

// ICM20948 Register addresses
#define ICM20948_WHO_AM_I       0x00
#define ICM20948_USER_CTRL      0x03
#define ICM20948_PWR_MGMT_1     0x06
#define ICM20948_PWR_MGMT_2     0x07
#define ICM20948_ACCEL_XOUT_H   0x2D
#define ICM20948_GYRO_XOUT_H    0x33
#define ICM20948_WHO_AM_I_VAL   0xEA

static i2c_port_t i2c_port = I2C_NUM_0;
static bool imu_initialized = false;
static float accel_scale = 1.0f / 8192.0f;  // ±4g
static float gyro_scale = 1.0f / 65.5f;      // ±500 dps

// ImuData struct is defined earlier in the file

static esp_err_t i2c_write_byte(uint8_t addr, uint8_t reg, uint8_t data) {
  uint8_t buf[2] = {reg, data};
  return i2c_master_write_to_device(i2c_port, addr, buf, 2, pdMS_TO_TICKS(100));
}

static esp_err_t i2c_read_bytes(uint8_t addr, uint8_t reg, uint8_t* data, size_t len) {
  return i2c_master_write_read_device(i2c_port, addr, &reg, 1, data, len, pdMS_TO_TICKS(100));
}

static bool initI2C() {
  i2c_config_t conf = {
    .mode = I2C_MODE_MASTER,
    .sda_io_num = I2C_SDA_PIN,
    .scl_io_num = I2C_SCL_PIN,
    .sda_pullup_en = GPIO_PULLUP_ENABLE,
    .scl_pullup_en = GPIO_PULLUP_ENABLE,
    .master = {
      .clk_speed = 400000
    },
    .clk_flags = 0
  };
  
  esp_err_t err = i2c_param_config(i2c_port, &conf);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "I2C param config failed: %d", err);
    return false;
  }
  
  err = i2c_driver_install(i2c_port, I2C_MODE_MASTER, 0, 0, 0);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "I2C driver install failed: %d", err);
    return false;
  }
  
  ESP_LOGI(TAG, "I2C initialized: SDA=%d, SCL=%d @ 400kHz", I2C_SDA_PIN, I2C_SCL_PIN);
  return true;
}

static bool initIMU() {
  ESP_LOGI(TAG, "Initializing ICM20948 IMU...");
  
  // Initialize I2C bus
  if (!initI2C()) {
    return false;
  }
  
  // Check WHO_AM_I register
  uint8_t who_am_i = 0;
  esp_err_t err = i2c_read_bytes(IMU_I2C_ADDR, ICM20948_WHO_AM_I, &who_am_i, 1);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to read WHO_AM_I: %d", err);
    return false;
  }
  
  if (who_am_i != ICM20948_WHO_AM_I_VAL) {
    ESP_LOGE(TAG, "Wrong WHO_AM_I: 0x%02X (expected 0x%02X)", who_am_i, ICM20948_WHO_AM_I_VAL);
    return false;
  }
  
  ESP_LOGI(TAG, "ICM20948 detected (WHO_AM_I=0x%02X)", who_am_i);
  
  // Reset device
  i2c_write_byte(IMU_I2C_ADDR, ICM20948_PWR_MGMT_1, 0x80);
  vTaskDelay(pdMS_TO_TICKS(100));
  
  // Wake up device, auto select clock
  i2c_write_byte(IMU_I2C_ADDR, ICM20948_PWR_MGMT_1, 0x01);
  vTaskDelay(pdMS_TO_TICKS(50));
  
  // Enable accel and gyro
  i2c_write_byte(IMU_I2C_ADDR, ICM20948_PWR_MGMT_2, 0x00);
  
  imu_initialized = true;
  ESP_LOGI(TAG, "IMU initialized successfully!");
  return true;
}

static ImuData readIMU() {
  ImuData data = {0, 0, 0, 0, 0, 0, 0, false};
  
  if (!imu_initialized) return data;
  
  // Read accel + gyro + temp (14 bytes)
  uint8_t buffer[14];
  esp_err_t err = i2c_read_bytes(IMU_I2C_ADDR, ICM20948_ACCEL_XOUT_H, buffer, 14);
  if (err != ESP_OK) {
    return data;
  }
  
  // Parse accelerometer (bytes 0-5)
  int16_t ax = (buffer[0] << 8) | buffer[1];
  int16_t ay = (buffer[2] << 8) | buffer[3];
  int16_t az = (buffer[4] << 8) | buffer[5];
  
  // Parse gyroscope (bytes 6-11)
  int16_t gx = (buffer[6] << 8) | buffer[7];
  int16_t gy = (buffer[8] << 8) | buffer[9];
  int16_t gz = (buffer[10] << 8) | buffer[11];
  
  // Parse temperature (bytes 12-13)
  int16_t temp_raw = (buffer[12] << 8) | buffer[13];
  
  // Apply scale factors
  data.accelX = ax * accel_scale;
  data.accelY = ay * accel_scale;
  data.accelZ = az * accel_scale;
  
  data.gyroX = gx * gyro_scale;
  data.gyroY = gy * gyro_scale;
  data.gyroZ = gz * gyro_scale;
  
  data.temperature = (temp_raw / 333.87f) + 21.0f;
  data.valid = true;
  
  return data;
}

// Update gravity from IMU readings
static void updateGravityFromIMU() {
  if (!imu_initialized || !sim.isImuGravityEnabled()) return;
  
  ImuData imu = readIMU();
  if (imu.valid) {
    // Map accelerometer to gravity
    // When device is tilted, accelerometer measures gravity component
    // accelX: positive = tilt right, negative = tilt left  
    // accelY: positive = tilt forward (away from user)
    // accelZ: ~1g when flat face up
    
    sim.setGravityFromIMU(imu.accelX, imu.accelY, imu.accelZ);
  }
}

// ============================================================
// BME280 Environmental Sensor Driver
// ============================================================

// BME280 Registers
#define BME280_REG_ID           0xD0
#define BME280_REG_RESET        0xE0
#define BME280_REG_CTRL_HUM     0xF2
#define BME280_REG_STATUS       0xF3
#define BME280_REG_CTRL_MEAS    0xF4
#define BME280_REG_CONFIG       0xF5
#define BME280_REG_DATA         0xF7
#define BME280_REG_CALIB00      0x88
#define BME280_REG_CALIB26      0xE1
#define BME280_CHIP_ID          0x60

static bool bme280_initialized = false;

// BME280 calibration data
struct BME280_Calib {
  uint16_t dig_T1;
  int16_t  dig_T2, dig_T3;
  uint16_t dig_P1;
  int16_t  dig_P2, dig_P3, dig_P4, dig_P5, dig_P6, dig_P7, dig_P8, dig_P9;
  uint8_t  dig_H1, dig_H3;
  int16_t  dig_H2, dig_H4, dig_H5;
  int8_t   dig_H6;
  int32_t  t_fine;
};

static BME280_Calib bme_calib = {};

// EnvironmentalData struct is defined earlier in the file

static bool initBME280() {
  ESP_LOGI(TAG, "Initializing BME280 environmental sensor...");
  
  // Check chip ID
  uint8_t chip_id = 0;
  esp_err_t err = i2c_read_bytes(BME280_I2C_ADDR, BME280_REG_ID, &chip_id, 1);
  if (err != ESP_OK || chip_id != BME280_CHIP_ID) {
    ESP_LOGW(TAG, "BME280 not found (ID=0x%02X, expected 0x%02X)", chip_id, BME280_CHIP_ID);
    return false;
  }
  
  ESP_LOGI(TAG, "BME280 detected (ID=0x%02X)", chip_id);
  
  // Soft reset
  i2c_write_byte(BME280_I2C_ADDR, BME280_REG_RESET, 0xB6);
  vTaskDelay(pdMS_TO_TICKS(10));
  
  // Read calibration data (26 bytes from 0x88, 7 bytes from 0xE1)
  uint8_t calib[26];
  err = i2c_read_bytes(BME280_I2C_ADDR, BME280_REG_CALIB00, calib, 26);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to read BME280 calibration");
    return false;
  }
  
  // Parse temperature calibration
  bme_calib.dig_T1 = (calib[1] << 8) | calib[0];
  bme_calib.dig_T2 = (calib[3] << 8) | calib[2];
  bme_calib.dig_T3 = (calib[5] << 8) | calib[4];
  
  // Parse pressure calibration
  bme_calib.dig_P1 = (calib[7] << 8) | calib[6];
  bme_calib.dig_P2 = (calib[9] << 8) | calib[8];
  bme_calib.dig_P3 = (calib[11] << 8) | calib[10];
  bme_calib.dig_P4 = (calib[13] << 8) | calib[12];
  bme_calib.dig_P5 = (calib[15] << 8) | calib[14];
  bme_calib.dig_P6 = (calib[17] << 8) | calib[16];
  bme_calib.dig_P7 = (calib[19] << 8) | calib[18];
  bme_calib.dig_P8 = (calib[21] << 8) | calib[20];
  bme_calib.dig_P9 = (calib[23] << 8) | calib[22];
  
  // Humidity calibration (separate register block)
  bme_calib.dig_H1 = calib[25];
  
  uint8_t hum_calib[7];
  err = i2c_read_bytes(BME280_I2C_ADDR, BME280_REG_CALIB26, hum_calib, 7);
  if (err == ESP_OK) {
    bme_calib.dig_H2 = (hum_calib[1] << 8) | hum_calib[0];
    bme_calib.dig_H3 = hum_calib[2];
    bme_calib.dig_H4 = (hum_calib[3] << 4) | (hum_calib[4] & 0x0F);
    bme_calib.dig_H5 = (hum_calib[5] << 4) | ((hum_calib[4] >> 4) & 0x0F);
    bme_calib.dig_H6 = (int8_t)hum_calib[6];
  }
  
  // Configure: humidity oversampling x1
  i2c_write_byte(BME280_I2C_ADDR, BME280_REG_CTRL_HUM, 0x01);
  
  // Configure: normal mode, temp x1, pressure x1
  i2c_write_byte(BME280_I2C_ADDR, BME280_REG_CTRL_MEAS, 0x27);
  
  // Config: standby 1000ms, filter off
  i2c_write_byte(BME280_I2C_ADDR, BME280_REG_CONFIG, 0xA0);
  
  bme280_initialized = true;
  ESP_LOGI(TAG, "BME280 initialized successfully!");
  return true;
}

static EnvironmentalData readBME280() {
  EnvironmentalData data = {0, 0, 0, 0, false};
  
  if (!bme280_initialized) return data;
  
  // Read raw data (8 bytes: pressure[3], temp[3], humidity[2])
  uint8_t raw[8];
  esp_err_t err = i2c_read_bytes(BME280_I2C_ADDR, BME280_REG_DATA, raw, 8);
  if (err != ESP_OK) return data;
  
  int32_t adc_P = ((uint32_t)raw[0] << 12) | ((uint32_t)raw[1] << 4) | ((raw[2] >> 4) & 0x0F);
  int32_t adc_T = ((uint32_t)raw[3] << 12) | ((uint32_t)raw[4] << 4) | ((raw[5] >> 4) & 0x0F);
  int32_t adc_H = ((uint32_t)raw[6] << 8) | raw[7];
  
  // Temperature compensation
  int32_t var1 = ((((adc_T >> 3) - ((int32_t)bme_calib.dig_T1 << 1))) * ((int32_t)bme_calib.dig_T2)) >> 11;
  int32_t var2 = (((((adc_T >> 4) - ((int32_t)bme_calib.dig_T1)) * ((adc_T >> 4) - ((int32_t)bme_calib.dig_T1))) >> 12) * ((int32_t)bme_calib.dig_T3)) >> 14;
  bme_calib.t_fine = var1 + var2;
  int32_t T = (bme_calib.t_fine * 5 + 128) >> 8;
  data.temperature = T / 100.0f;
  
  // Pressure compensation
  int64_t var1_p = ((int64_t)bme_calib.t_fine) - 128000;
  int64_t var2_p = var1_p * var1_p * (int64_t)bme_calib.dig_P6;
  var2_p = var2_p + ((var1_p * (int64_t)bme_calib.dig_P5) << 17);
  var2_p = var2_p + (((int64_t)bme_calib.dig_P4) << 35);
  var1_p = ((var1_p * var1_p * (int64_t)bme_calib.dig_P3) >> 8) + ((var1_p * (int64_t)bme_calib.dig_P2) << 12);
  var1_p = (((((int64_t)1) << 47) + var1_p)) * ((int64_t)bme_calib.dig_P1) >> 33;
  if (var1_p != 0) {
    int64_t p = 1048576 - adc_P;
    p = (((p << 31) - var2_p) * 3125) / var1_p;
    var1_p = (((int64_t)bme_calib.dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    var2_p = (((int64_t)bme_calib.dig_P8) * p) >> 19;
    p = ((p + var1_p + var2_p) >> 8) + (((int64_t)bme_calib.dig_P7) << 4);
    data.pressure = (float)((uint32_t)p) / 25600.0f;  // hPa
  }
  
  // Humidity compensation
  int32_t h = bme_calib.t_fine - 76800;
  h = (((((adc_H << 14) - (((int32_t)bme_calib.dig_H4) << 20) - (((int32_t)bme_calib.dig_H5) * h)) + 16384) >> 15) *
       (((((((h * ((int32_t)bme_calib.dig_H6)) >> 10) * (((h * ((int32_t)bme_calib.dig_H3)) >> 11) + 32768)) >> 10) + 2097152) *
         ((int32_t)bme_calib.dig_H2) + 8192) >> 14));
  h = h - (((((h >> 15) * (h >> 15)) >> 7) * ((int32_t)bme_calib.dig_H1)) >> 4);
  h = (h < 0) ? 0 : h;
  h = (h > 419430400) ? 419430400 : h;
  data.humidity = (float)(h >> 12) / 1024.0f;
  
  // Altitude from pressure (using sea level pressure 1013.25 hPa)
  data.altitude = 44330.0f * (1.0f - powf(data.pressure / 1013.25f, 0.1903f));
  
  data.valid = true;
  return data;
}

// ============================================================
// GPS Driver (NEO-8M via UART)
// ============================================================

// GpsData struct is defined earlier in the file

static bool gps_initialized = false;
static GpsData gps_data = {};
static char gps_buffer[256] = {};
static int gps_buf_idx = 0;

static bool initGPS() {
  ESP_LOGI(TAG, "Initializing GPS (NEO-8M)...");
  
  uart_config_t cfg = {
    .baud_rate = GPS_BAUD,
    .data_bits = UART_DATA_8_BITS,
    .parity = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    .rx_flow_ctrl_thresh = 0,
    .source_clk = UART_SCLK_DEFAULT,
    .flags = { .allow_pd = 0, .backup_before_sleep = 0 }
  };
  
  // Install driver first
  esp_err_t err = uart_driver_install(GPS_UART_NUM, 1024, 0, 0, nullptr, 0);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "GPS UART driver install failed: %d", err);
    return false;
  }
  
  err = uart_param_config(GPS_UART_NUM, &cfg);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "GPS UART param config failed: %d", err);
    return false;
  }
  
  err = uart_set_pin(GPS_UART_NUM, GPS_UART_TX, GPS_UART_RX, -1, -1);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "GPS UART set pin failed: %d", err);
    return false;
  }
  
  gps_initialized = true;
  ESP_LOGI(TAG, "GPS UART initialized: TX=%d, RX=%d @ %d baud", GPS_UART_TX, GPS_UART_RX, GPS_BAUD);
  return true;
}

// Parse NMEA coordinate (DDDMM.MMMM format to decimal degrees)
static float parseNmeaCoord(const char* str, char dir) {
  float raw = atof(str);
  int degrees = (int)(raw / 100);
  float minutes = raw - (degrees * 100);
  float decimal = degrees + (minutes / 60.0f);
  if (dir == 'S' || dir == 'W') decimal = -decimal;
  return decimal;
}

// Parse a single NMEA sentence
static void parseNmeaSentence(const char* sentence) {
  char buf[128];
  strncpy(buf, sentence, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';
  
  // Check for GPGGA (fix data)
  if (strncmp(buf, "$GPGGA", 6) == 0 || strncmp(buf, "$GNGGA", 6) == 0) {
    char* token = strtok(buf, ",");
    int field = 0;
    char lat_str[20] = "", lon_str[20] = "";
    char lat_dir = 'N', lon_dir = 'E';
    
    while (token != nullptr && field < 15) {
      switch (field) {
        case 1:  // Time
          if (strlen(token) >= 6) {
            gps_data.hour = (token[0] - '0') * 10 + (token[1] - '0');
            gps_data.minute = (token[2] - '0') * 10 + (token[3] - '0');
            gps_data.second = (token[4] - '0') * 10 + (token[5] - '0');
          }
          break;
        case 2: strncpy(lat_str, token, sizeof(lat_str) - 1); break;
        case 3: lat_dir = token[0]; break;
        case 4: strncpy(lon_str, token, sizeof(lon_str) - 1); break;
        case 5: lon_dir = token[0]; break;
        case 6: gps_data.hasfix = (atoi(token) > 0); break;
        case 7: gps_data.satellites = atoi(token); break;
        case 9: gps_data.altitude = atof(token); break;
      }
      token = strtok(nullptr, ",");
      field++;
    }
    
    if (strlen(lat_str) > 0 && strlen(lon_str) > 0) {
      gps_data.latitude = parseNmeaCoord(lat_str, lat_dir);
      gps_data.longitude = parseNmeaCoord(lon_str, lon_dir);
      gps_data.valid = true;
    }
  }
  // Check for GPRMC (speed/course)
  else if (strncmp(buf, "$GPRMC", 6) == 0 || strncmp(buf, "$GNRMC", 6) == 0) {
    char* token = strtok(buf, ",");
    int field = 0;
    
    while (token != nullptr && field < 12) {
      switch (field) {
        case 7: gps_data.speed = atof(token) * 1.852f; break;  // knots to km/h
        case 8: gps_data.course = atof(token); break;
        case 9:  // Date DDMMYY
          if (strlen(token) >= 6) {
            gps_data.day = (token[0] - '0') * 10 + (token[1] - '0');
            gps_data.month = (token[2] - '0') * 10 + (token[3] - '0');
            gps_data.year = 2000 + (token[4] - '0') * 10 + (token[5] - '0');
          }
          break;
      }
      token = strtok(nullptr, ",");
      field++;
    }
  }
}

// Read and parse any available GPS data (non-blocking)
static void updateGPS() {
  if (!gps_initialized) return;
  
  // Read available bytes
  int len = uart_read_bytes(GPS_UART_NUM, (uint8_t*)&gps_buffer[gps_buf_idx], 
                            sizeof(gps_buffer) - gps_buf_idx - 1, 0);
  if (len <= 0) return;
  
  gps_buf_idx += len;
  gps_buffer[gps_buf_idx] = '\0';
  
  // Process complete sentences
  char* start = gps_buffer;
  char* newline;
  while ((newline = strchr(start, '\n')) != nullptr) {
    *newline = '\0';
    // Remove \r if present
    if (newline > start && *(newline-1) == '\r') {
      *(newline-1) = '\0';
    }
    
    if (start[0] == '$') {
      parseNmeaSentence(start);
    }
    start = newline + 1;
  }
  
  // Move remaining incomplete data to start of buffer
  if (start > gps_buffer) {
    int remaining = gps_buf_idx - (start - gps_buffer);
    if (remaining > 0) {
      memmove(gps_buffer, start, remaining);
    }
    gps_buf_idx = remaining;
  }
  
  // Prevent buffer overflow
  if (gps_buf_idx >= (int)sizeof(gps_buffer) - 16) {
    gps_buf_idx = 0;
  }
}

// ============================================================
// INMP441 Microphone Driver (I2S)
// ============================================================

static i2s_chan_handle_t mic_chan = nullptr;
static bool mic_initialized = false;

// AudioData struct is defined earlier in the file

static float mic_peak_level = 0;
static float mic_peak_decay = 0.95f;  // Peak meter decay rate

static bool initMicrophone() {
  ESP_LOGI(TAG, "Initializing INMP441 microphone...");
  
  // Configure I2S channel
  i2s_chan_config_t chan_cfg = {
    .id = I2S_NUM_0,
    .role = I2S_ROLE_MASTER,
    .dma_desc_num = 6,
    .dma_frame_num = 240,
    .auto_clear_after_cb = false,
    .auto_clear_before_cb = false,
    .intr_priority = 0,
  };
  
  esp_err_t err = i2s_new_channel(&chan_cfg, nullptr, &mic_chan);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to create I2S channel: %d", err);
    return false;
  }
  
  // Configure I2S standard mode for INMP441
  i2s_std_config_t std_cfg = {
    .clk_cfg = {
      .sample_rate_hz = 16000,
      .clk_src = I2S_CLK_SRC_DEFAULT,
      .ext_clk_freq_hz = 0,
      .mclk_multiple = I2S_MCLK_MULTIPLE_256,
    },
    .slot_cfg = {
      .data_bit_width = I2S_DATA_BIT_WIDTH_32BIT,
      .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,
      .slot_mode = I2S_SLOT_MODE_MONO,
      .slot_mask = I2S_STD_SLOT_LEFT,
      .ws_width = I2S_DATA_BIT_WIDTH_32BIT,
      .ws_pol = false,
      .bit_shift = true,
      .left_align = true,
      .big_endian = false,
      .bit_order_lsb = false,
    },
    .gpio_cfg = {
      .mclk = I2S_GPIO_UNUSED,
      .bclk = MIC_BCK_PIN,
      .ws = MIC_WS_PIN,
      .dout = I2S_GPIO_UNUSED,
      .din = MIC_SD_PIN,
      .invert_flags = {
        .mclk_inv = false,
        .bclk_inv = false,
        .ws_inv = false,
      },
    },
  };
  
  err = i2s_channel_init_std_mode(mic_chan, &std_cfg);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to init I2S std mode: %d", err);
    i2s_del_channel(mic_chan);
    mic_chan = nullptr;
    return false;
  }
  
  err = i2s_channel_enable(mic_chan);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to enable I2S channel: %d", err);
    i2s_del_channel(mic_chan);
    mic_chan = nullptr;
    return false;
  }
  
  mic_initialized = true;
  ESP_LOGI(TAG, "Microphone initialized: WS=%d, BCK=%d, SD=%d", MIC_WS_PIN, MIC_BCK_PIN, MIC_SD_PIN);
  return true;
}

static AudioData readMicrophone() {
  AudioData data = {0, -60.0f, 0, false};
  
  if (!mic_initialized || mic_chan == nullptr) return data;
  
  // Read samples
  int32_t samples[64];
  size_t bytes_read = 0;
  esp_err_t err = i2s_channel_read(mic_chan, samples, sizeof(samples), &bytes_read, 0);
  
  if (err != ESP_OK || bytes_read == 0) return data;
  
  // Calculate RMS level
  int64_t sum = 0;
  int num_samples = bytes_read / sizeof(int32_t);
  for (int i = 0; i < num_samples; i++) {
    int32_t sample = samples[i] >> 8;  // Shift down from 24-bit to manageable range
    sum += (int64_t)sample * sample;
  }
  
  int32_t rms = (int32_t)sqrtf((float)(sum / num_samples));
  data.level = rms;
  
  // Convert to dB (rough estimate)
  if (rms > 0) {
    data.levelDb = 20.0f * log10f((float)rms / 32768.0f);
  } else {
    data.levelDb = -60.0f;
  }
  
  // Peak level with decay
  if (rms > mic_peak_level) {
    mic_peak_level = rms;
  } else {
    mic_peak_level *= mic_peak_decay;
  }
  data.peakLevel = mic_peak_level;
  
  data.valid = true;
  return data;
}

// ============================================================
// Sensor Data Aggregator
// ============================================================

// AllSensorData struct is defined earlier in the file

static AllSensorData allSensors = {};

static void updateAllSensors() {
  // Update IMU
  allSensors.imu = readIMU();
  if (allSensors.imu.valid && sim.isImuGravityEnabled()) {
    sim.setGravityFromIMU(allSensors.imu.accelX, allSensors.imu.accelY, allSensors.imu.accelZ);
  }
  
  // Update environmental (less frequently - sensor is slow)
  static int envCounter = 0;
  if (++envCounter >= 30) {  // Every ~1 second at 30fps
    allSensors.env = readBME280();
    envCounter = 0;
  }
  
  // Update GPS (non-blocking, parses available data)
  updateGPS();
  allSensors.gps = gps_data;
  
  // Update microphone
  allSensors.audio = readMicrophone();
}

// ============================================================
// Main
// ============================================================

extern "C" void app_main() {
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "╔══════════════════════════════════════════════════════╗");
  ESP_LOGI(TAG, "║   2D Sand Simulation - Full Sensor Suite Edition     ║");
  ESP_LOGI(TAG, "║   Tilt to change gravity, make noise to spawn!       ║");
  ESP_LOGI(TAG, "╚══════════════════════════════════════════════════════╝");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "Controls:");
  ESP_LOGI(TAG, "  A = Cycle particle type");
  ESP_LOGI(TAG, "  B = Spawn particles (hold)");
  ESP_LOGI(TAG, "  C = Clear screen");
  ESP_LOGI(TAG, "  D = Toggle IMU gravity control");
  ESP_LOGI(TAG, "  TILT = Change gravity direction");
  ESP_LOGI(TAG, "  SOUND = Spawns particles on loud sounds!");
  ESP_LOGI(TAG, "");
  
  // Wait for GPU to boot
  vTaskDelay(pdMS_TO_TICKS(2000));
  
  // Initialize GPU
  if (!gpu.init()) {
    ESP_LOGE(TAG, "GPU init failed!");
    return;
  }
  
  // Initialize all sensors
  ESP_LOGI(TAG, "=== Initializing Sensors ===");
  
  // IMU (also initializes I2C bus)
  bool imuOk = initIMU();
  if (imuOk) {
    ESP_LOGI(TAG, "[OK] IMU (ICM20948) - tilt device to control gravity!");
  } else {
    ESP_LOGW(TAG, "[--] IMU not available");
  }
  
  // BME280 Environmental (uses same I2C bus)
  bool envOk = initBME280();
  if (envOk) {
    ESP_LOGI(TAG, "[OK] Environmental (BME280) - temp/humidity/pressure");
  } else {
    ESP_LOGW(TAG, "[--] Environmental sensor not available");
  }
  
  // GPS
  bool gpsOk = initGPS();
  if (gpsOk) {
    ESP_LOGI(TAG, "[OK] GPS (NEO-8M) - location tracking");
  } else {
    ESP_LOGW(TAG, "[--] GPS not available");
  }
  
  // Microphone
  bool micOk = initMicrophone();
  if (micOk) {
    ESP_LOGI(TAG, "[OK] Microphone (INMP441) - sound-reactive particles!");
  } else {
    ESP_LOGW(TAG, "[--] Microphone not available");
  }
  
  ESP_LOGI(TAG, "============================");
  
  initButtons();
  srand((unsigned int)esp_timer_get_time());
  
  ESP_LOGI(TAG, "Starting simulation...");
  
  // Create some initial particles
  sim.selectedType = ParticleType::STONE;
  // Draw floor
  for (int x = 0; x < GRID_W; x++) {
    sim.spawn(x, GRID_H - 2);
  }
  // Draw walls
  for (int y = 0; y < GRID_H; y++) {
    sim.spawn(0, y);
    sim.spawn(GRID_W - 1, y);
  }
  sim.selectedType = ParticleType::SAND;
  
  // Main loop
  uint64_t lastTime = esp_timer_get_time();
  uint32_t frameCount = 0;
  uint64_t fpsTimer = lastTime;
  int spawnX = GRID_W / 2;
  int spawnY = 2;
  float spawnAngle = 0;
  
  // Sound reactivity settings
  float soundThreshold = 500.0f;  // Minimum level to trigger spawn
  int soundCooldown = 0;          // Prevent too rapid spawning
  
  while (true) {
    uint64_t now = esp_timer_get_time();
    float dt = (now - lastTime) / 1000000.0f;
    lastTime = now;
    
    // Read buttons
    readInput();
    
    // Handle input
    if (justPressed(input.btnA, input.prevA)) {
      sim.cycleParticleType();
    }
    
    if (justPressed(input.btnC, input.prevC)) {
      sim.clear();
      // Redraw boundaries
      sim.selectedType = ParticleType::STONE;
      for (int x = 0; x < GRID_W; x++) {
        sim.spawn(x, GRID_H - 2);
      }
      for (int y = 0; y < GRID_H; y++) {
        sim.spawn(0, y);
        sim.spawn(GRID_W - 1, y);
      }
      sim.selectedType = ParticleType::SAND;
      ESP_LOGI(TAG, "Cleared!");
    }
    
    if (justPressed(input.btnD, input.prevD)) {
      sim.toggleImuGravity();
    }
    
    // Update all sensors
    updateAllSensors();
    
    // B = spawn particles at moving position
    if (input.btnB) {
      // Move spawn point in a pattern
      spawnAngle += dt * 2.0f;
      spawnX = GRID_W / 2 + (int)(sinf(spawnAngle) * 40);
      spawnX = (spawnX < 5) ? 5 : (spawnX > GRID_W - 5) ? GRID_W - 5 : spawnX;
      sim.spawn(spawnX, spawnY);
    }
    
    // Sound-reactive particle spawning!
    if (soundCooldown > 0) soundCooldown--;
    if (allSensors.audio.valid && allSensors.audio.level > soundThreshold && soundCooldown == 0) {
      // Spawn particles based on sound level
      int numParticles = (int)(allSensors.audio.level / 1000.0f);
      if (numParticles > 10) numParticles = 10;
      
      for (int i = 0; i < numParticles; i++) {
        int x = 10 + (rand() % (GRID_W - 20));
        sim.spawn(x, 1);
      }
      soundCooldown = 5;  // Wait a few frames before next sound spawn
    }
    
    // Auto-spawn some sand for demo (less if sound is active)
    if (frameCount % 5 == 0 && sim.particleCount < (GRID_W * GRID_H / 3)) {
      int x = 20 + (rand() % (GRID_W - 40));
      sim.spawn(x, 1);
    }
    
    // Temperature affects fire - hot temps make fire spread faster
    // (Environmental sensor integration example)
    // This could be expanded to affect particle behavior
    
    // Update physics
    sim.update();
    
    // Render
    sim.render();
    
    // Update OLED less frequently (with all sensor data)
    if (frameCount % 10 == 0) {
      sim.renderOLED(allSensors);
    }
    
    // FPS and sensor status counter
    frameCount++;
    if (now - fpsTimer >= 1000000) {
      ESP_LOGI(TAG, "FPS: %lu | Particles: %d | Gravity: (%.2f, %.2f)", 
               frameCount, sim.particleCount, 
               sim.getGravityX(), sim.getGravityY());
      
      // Log sensor data periodically
      if (allSensors.env.valid) {
        ESP_LOGI(TAG, "  ENV: %.1f°C, %.1f%% RH, %.1f hPa, %.1fm alt",
                 allSensors.env.temperature, allSensors.env.humidity,
                 allSensors.env.pressure, allSensors.env.altitude);
      }
      if (allSensors.gps.valid && allSensors.gps.hasfix) {
        ESP_LOGI(TAG, "  GPS: %.6f, %.6f | %d sats | %.1f km/h",
                 allSensors.gps.latitude, allSensors.gps.longitude,
                 allSensors.gps.satellites, allSensors.gps.speed);
      } else if (allSensors.gps.valid) {
        ESP_LOGI(TAG, "  GPS: Searching... (%d sats)", allSensors.gps.satellites);
      }
      if (allSensors.audio.valid) {
        ESP_LOGI(TAG, "  MIC: level=%ld (%.1f dB)", allSensors.audio.level, allSensors.audio.levelDb);
      }
      
      frameCount = 0;
      fpsTimer = now;
    }
    
    // Target ~30 FPS (simulation is CPU intensive)
    vTaskDelay(pdMS_TO_TICKS(33));
  }
}
