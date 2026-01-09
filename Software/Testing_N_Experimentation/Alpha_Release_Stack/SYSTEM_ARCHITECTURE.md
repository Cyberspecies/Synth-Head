# Synth-Head System Architecture & Reference

> **For AI Agents / New Chat Sessions**  
> This document contains all critical information about the dual-ESP32 Synth-Head project including pin mappings, communication protocols, HAL layer, GPU commands, and example code.

---

## Table of Contents

1. [Hardware Overview](#hardware-overview)
2. [Pin Mappings](#pin-mappings)
3. [CPU-GPU Communication Protocol](#cpu-gpu-communication-protocol)
4. [GPU Command Reference](#gpu-command-reference)
5. [HAL Layer Architecture](#hal-layer-architecture)
6. [HAL Interface Reference](#hal-interface-reference)
7. [Display Systems](#display-systems)
8. [Example: Polygon Demo with Cycling Hue](#example-polygon-demo-with-cycling-hue)
9. [PlatformIO Build Configuration](#platformio-build-configuration)
10. [File Structure](#file-structure)

---

## Hardware Overview

The Synth-Head project uses a **dual ESP32-S3 architecture**:

| Component | Description | COM Port |
|-----------|-------------|----------|
| **CPU** | Main processor - runs application logic, sensors, WiFi | COM15 |
| **GPU** | Display processor - drives HUB75 + OLED, receives draw commands | COM5 |

### Displays

| Display | Resolution | Type | Location | Interface |
|---------|------------|------|----------|-----------|
| **HUB75** | 128×32 | RGB LED Matrix (2× 64×32 panels) | Both eyes | GPU direct |
| **OLED** | 128×128 | Monochrome SH1107 | GPU board | I2C (0x3C) |

### Communication

- **CPU ↔ GPU**: UART @ **10 Mbps** (10,000,000 baud)
- **Protocol**: `[0xAA][0x55][CmdType:1][Length:2 LE][Payload:N]`

---

## Pin Mappings

### CPU Pins (ESP32-S3 on COM15)

#### I2C Bus
| Pin | GPIO | Function | Connected Devices |
|-----|------|----------|-------------------|
| SDA | **9** | I2C Data | ICM20948 (0x68), BME280 (0x76) |
| SCL | **10** | I2C Clock | ICM20948 (0x68), BME280 (0x76) |

#### LED Strips (WS2812B / SK6812)
| Strip | GPIO | LED Count | Purpose |
|-------|------|-----------|---------|
| Strip 0 | 16 | - | *Unused* |
| Left Fin | **18** | 13 | Left fin animation |
| Tongue | **8** | 9 | Tongue animation |
| Strip 3 | 39 | - | *Unused* |
| Right Fin | **38** | 13 | Right fin animation |
| Scale LEDs | **37** | 14 | Scale row |

#### Buttons (Active LOW)
| Button | GPIO | Label |
|--------|------|-------|
| A | **5** | Button A |
| B | **6** | Button B |
| C | **7** | Button C |
| D | **15** | Button D |

#### Fans (PWM)
| Fan | GPIO | Status |
|-----|------|--------|
| Fan 1 | 17 | *Unused* |
| Fan 2 | **36** | Active |

#### SD Card (SPI)
| Signal | GPIO |
|--------|------|
| MISO | 14 |
| MOSI | 47 |
| CLK | 21 |
| CS | 48 |

#### GPS (NEO-M8, UART2)
| Signal | GPIO | Baud |
|--------|------|------|
| TX | 43 | 9600 |
| RX | 44 | 9600 |

#### CPU-GPU UART (UART1)
| Signal | GPIO | Note |
|--------|------|------|
| TX | **12** | → GPU RX (GPIO 13) |
| RX | **11** | ← GPU TX (GPIO 12) |
| **Baud** | - | **10,000,000** (10 Mbps) |

#### I2S Microphone (INMP441)
| Signal | GPIO |
|--------|------|
| DOUT (SD) | 2 |
| CLK (BCK) | 40 |
| L/R Select | 41 |
| WS | 42 |

### GPU Pins (ESP32-S3 on COM5)

#### CPU-GPU UART (UART1)
| Signal | GPIO |
|--------|------|
| TX | 12 |
| RX | **13** |

#### HUB75 (Directly driven by GPU)
```
Configured in ARCOS HUB75 driver - see driver_hub75_simple.hpp
```

#### OLED (I2C)
| Signal | Address |
|--------|---------|
| SH1107 | **0x3C** |

---

## CPU-GPU Communication Protocol

### Packet Format

```
┌──────┬──────┬─────────┬───────────────┬─────────────┐
│ 0xAA │ 0x55 │ CmdType │ Length (2 LE) │ Payload (N) │
└──────┴──────┴─────────┴───────────────┴─────────────┘
   1B     1B      1B          2B            0-N B
```

- **Sync bytes**: `0xAA 0x55` (must be present)
- **CmdType**: Single byte command identifier
- **Length**: 16-bit little-endian payload length
- **Payload**: Command-specific data

### UART Configuration

```cpp
// CPU side (sending to GPU)
constexpr uart_port_t UART_PORT = UART_NUM_1;
constexpr int UART_TX_PIN = 12;  // CPU TX -> GPU RX (GPIO 13)
constexpr int UART_RX_PIN = 11;  // CPU RX <- GPU TX (GPIO 12)
constexpr int UART_BAUD = 10000000;  // 10 Mbps

uart_config_t cfg = {
    .baud_rate = UART_BAUD,
    .data_bits = UART_DATA_8_BITS,
    .parity = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    .source_clk = UART_SCLK_DEFAULT,
};
uart_param_config(UART_PORT, &cfg);
uart_set_pin(UART_PORT, UART_TX_PIN, UART_RX_PIN, -1, -1);
uart_driver_install(UART_PORT, 1024, 1024, 0, nullptr, 0);
```

---

## GPU Command Reference

### Command Types (CmdType values)

#### System Commands
| Cmd | Hex | Payload | Description |
|-----|-----|---------|-------------|
| NOP | 0x00 | - | No operation |
| PING | 0xF0 | - | Ping GPU |
| RESET | 0xFF | - | Reset GPU state |

#### Target Selection
| Cmd | Hex | Payload | Description |
|-----|-----|---------|-------------|
| SET_TARGET | 0x50 | `[target:1]` | 0=HUB75, 1=OLED |
| PRESENT | 0x51 | - | Swap framebuffer to display |

#### HUB75 Drawing (Target 0)
| Cmd | Hex | Payload | Description |
|-----|-----|---------|-------------|
| DRAW_PIXEL | 0x40 | `[x:2][y:2][r:1][g:1][b:1]` | Draw single pixel |
| DRAW_LINE | 0x41 | `[x1:2][y1:2][x2:2][y2:2][r:1][g:1][b:1]` | Draw line |
| DRAW_RECT | 0x42 | `[x:2][y:2][w:2][h:2][r:1][g:1][b:1]` | Rectangle outline |
| DRAW_FILL | 0x43 | `[x:2][y:2][w:2][h:2][r:1][g:1][b:1]` | Filled rectangle |
| DRAW_CIRCLE | 0x44 | `[cx:2][cy:2][radius:2][r:1][g:1][b:1]` | Circle outline |
| DRAW_POLY | 0x45 | `[nVerts:1][varStart:1][r:1][g:1][b:1]` | Polygon from vars |
| BLIT_SPRITE | 0x46 | `[id:1][x:2][y:2]` | Blit uploaded sprite |
| CLEAR | 0x47 | `[r:1][g:1][b:1]` | Clear to color |

#### Float Coordinate Versions (Anti-aliased)
| Cmd | Hex | Payload | Description |
|-----|-----|---------|-------------|
| DRAW_LINE_F | 0x48 | `[x1:4f][y1:4f][x2:4f][y2:4f][r:1][g:1][b:1]` | AA line |
| DRAW_CIRCLE_F | 0x49 | `[cx:4f][cy:4f][radius:4f][r:1][g:1][b:1]` | AA circle |
| DRAW_RECT_F | 0x4A | `[x:4f][y:4f][w:4f][h:4f][r:1][g:1][b:1]` | AA rect |

#### OLED Drawing (128×128 Mono)
| Cmd | Hex | Payload | Description |
|-----|-----|---------|-------------|
| OLED_CLEAR | 0x60 | - | Clear OLED |
| OLED_LINE | 0x61 | `[x1:2][y1:2][x2:2][y2:2][on:1]` | Draw line |
| OLED_RECT | 0x62 | `[x:2][y:2][w:2][h:2][on:1]` | Rectangle outline |
| OLED_FILL | 0x63 | `[x:2][y:2][w:2][h:2][on:1]` | Filled rectangle |
| OLED_CIRCLE | 0x64 | `[cx:2][cy:2][r:2][on:1]` | Circle outline |
| OLED_PRESENT | 0x65 | - | Update OLED display |
| OLED_PIXEL | 0x66 | `[x:2][y:2][on:1]` | Single pixel |
| OLED_VLINE | 0x67 | `[x:2][y:2][len:2][on:1]` | Vertical line |
| OLED_HLINE | 0x68 | `[x:2][y:2][len:2][on:1]` | Horizontal line |
| OLED_FILL_CIRCLE | 0x69 | `[cx:2][cy:2][r:2][on:1]` | Filled circle |

#### Shader System
| Cmd | Hex | Payload | Description |
|-----|-----|---------|-------------|
| UPLOAD_SHADER | 0x10 | `[slot:1][bytecode:N]` | Upload shader |
| DELETE_SHADER | 0x11 | `[slot:1]` | Delete shader |
| EXEC_SHADER | 0x12 | `[slot:1]` | Execute shader |
| SET_VAR | 0x30 | `[id:1][value:2]` | Set variable |
| SET_VARS | 0x31 | `[startId:1][count:1][values:2×N]` | Set multiple vars |

#### Sprite System
| Cmd | Hex | Payload | Description |
|-----|-----|---------|-------------|
| UPLOAD_SPRITE | 0x20 | `[id:1][w:1][h:1][fmt:1][data:N]` | Upload sprite |
| DELETE_SPRITE | 0x21 | `[id:1]` | Delete sprite |

### Payload Encoding

All multi-byte integers are **little-endian (LE)**:
```cpp
// Encode int16_t to payload
void encodeI16(uint8_t* buf, int idx, int16_t val) {
    buf[idx] = val & 0xFF;           // Low byte first
    buf[idx + 1] = (val >> 8) & 0xFF; // High byte second
}
```

---

## HAL Layer Architecture

### Namespace Structure

```
arcos::hal
├── HalResult          // Result codes (OK, ERROR, TIMEOUT, etc.)
├── HalTypes           // RGB, RGBW, Vec3f, Vec3i
├── pins::cpu          // CPU pin definitions
├── pins::gpu          // GPU pin definitions  
├── pins::i2c_addr     // I2C device addresses
├── pins::defaults     // Default settings
│
├── IHalGpio           // Digital GPIO interface
├── IHalPwm            // PWM output interface
├── IHalButton         // Button input interface
├── IHalUart           // UART interface
├── IHalI2c            // I2C interface
├── IHalSpi            // SPI interface
├── IHalI2s            // I2S audio interface
├── IHalTimer          // Timer/delay interface
├── IHalImu            // IMU sensor interface
├── IHalEnvironmental  // Temperature/humidity/pressure
├── IHalGps            // GPS interface
├── IHalMicrophone     // Microphone input
├── IHalLedStrip       // Addressable LED strips
├── IHalDisplay        // Display interface
└── IHalStorage        // SD card/flash storage
```

### Pin Constants (from `HAL/hal.hpp`)

```cpp
namespace arcos::hal::pins {

namespace cpu {
  // I2C
  constexpr gpio_pin_t I2C_SDA = 9;
  constexpr gpio_pin_t I2C_SCL = 10;
  
  // LED Strips
  constexpr gpio_pin_t LED_LEFT_FIN = 18;    // 13 LEDs
  constexpr gpio_pin_t LED_RIGHT_FIN = 38;   // 13 LEDs
  constexpr gpio_pin_t LED_TONGUE = 8;       // 9 LEDs
  constexpr gpio_pin_t LED_SCALE = 37;       // 14 LEDs
  
  // Buttons
  constexpr gpio_pin_t BUTTON_A = 5;
  constexpr gpio_pin_t BUTTON_B = 6;
  constexpr gpio_pin_t BUTTON_C = 7;
  constexpr gpio_pin_t BUTTON_D = 15;
  
  // CPU-GPU UART
  constexpr gpio_pin_t UART_TX = 12;
  constexpr gpio_pin_t UART_RX = 11;
  
  // I2S Microphone
  constexpr gpio_pin_t MIC_DOUT = 2;
  constexpr gpio_pin_t MIC_CLK = 40;
  constexpr gpio_pin_t MIC_LR_SEL = 41;
  constexpr gpio_pin_t MIC_WS = 42;
  
  // GPS
  constexpr gpio_pin_t GPS_TX = 43;
  constexpr gpio_pin_t GPS_RX = 44;
}

namespace i2c_addr {
  constexpr i2c_addr_t ICM20948 = 0x68;
  constexpr i2c_addr_t BME280 = 0x76;
  constexpr i2c_addr_t OLED_SH1107 = 0x3C;
}

namespace defaults {
  constexpr uint32_t CPU_GPU_BAUD = 10000000;  // 10 Mbps
  constexpr uint32_t GPS_BAUD = 9600;
  constexpr uint32_t I2C_FREQ = 400000;
}

}
```

---

## HAL Interface Reference

### IHalImu (ICM20948)

```cpp
struct ImuData {
  Vec3f accel;        // Accelerometer (g)
  Vec3f gyro;         // Gyroscope (°/s)
  Vec3f mag;          // Magnetometer (μT)
  float temperature;  // Temperature (°C)
  timestamp_ms_t timestamp;
  bool accel_valid, gyro_valid, mag_valid;
};

struct ImuConfig {
  i2c_addr_t address = 0x68;
  uint8_t accel_range = 4;      // ±2, ±4, ±8, ±16 g
  uint16_t accel_rate = 100;    // Hz
  uint16_t gyro_range = 500;    // ±250, ±500, ±1000, ±2000 dps
  uint16_t gyro_rate = 100;     // Hz
  bool mag_enabled = true;
  uint8_t mag_rate = 100;       // Hz
};

class IHalImu {
  virtual HalResult init(const ImuConfig& config) = 0;
  virtual HalResult deinit() = 0;
  virtual bool isInitialized() const = 0;
  virtual HalResult readAll(ImuData& data) = 0;
  virtual HalResult readAccel(Vec3f& accel) = 0;
  virtual HalResult readGyro(Vec3f& gyro) = 0;
  virtual HalResult readMag(Vec3f& mag) = 0;
};
```

### IHalEnvironmental (BME280)

```cpp
struct EnvironmentalData {
  float temperature;     // °C
  float humidity;        // %
  float pressure;        // Pa
  timestamp_ms_t timestamp;
  bool temperature_valid, humidity_valid, pressure_valid;
};

struct EnvironmentalConfig {
  i2c_addr_t address = 0x76;
  uint8_t temp_oversampling = 1;     // 1, 2, 4, 8, 16
  uint8_t humidity_oversampling = 1;
  uint8_t pressure_oversampling = 1;
  uint8_t mode = 3;                   // 0=sleep, 1=forced, 3=normal
  uint16_t standby_ms = 1000;
};

class IHalEnvironmental {
  virtual HalResult init(const EnvironmentalConfig& config) = 0;
  virtual HalResult readAll(EnvironmentalData& data) = 0;
  virtual HalResult readTemperature(float& temp) = 0;
  virtual HalResult readHumidity(float& humidity) = 0;
  virtual HalResult readPressure(float& pressure) = 0;
};
```

### IHalGpio

```cpp
enum class GpioMode { INPUT, OUTPUT, INPUT_PULLUP, INPUT_PULLDOWN };
enum class GpioState { LOW, HIGH };

class IHalGpio {
  virtual HalResult init() = 0;
  virtual HalResult pinMode(gpio_pin_t pin, GpioMode mode) = 0;
  virtual GpioState digitalRead(gpio_pin_t pin) = 0;
  virtual HalResult digitalWrite(gpio_pin_t pin, GpioState state) = 0;
};
```

### Common Types

```cpp
enum class HalResult : uint8_t {
  OK = 0, ERROR, TIMEOUT, BUSY, INVALID_PARAM,
  NOT_INITIALIZED, NOT_SUPPORTED, BUFFER_FULL,
  BUFFER_EMPTY, NO_DATA, HARDWARE_FAULT,
  ALREADY_INITIALIZED, INVALID_STATE, NO_MEMORY,
  DEVICE_NOT_FOUND, READ_FAILED, WRITE_FAILED
};

struct RGB { uint8_t r, g, b; };
struct RGBW { uint8_t r, g, b, w; };
struct Vec3f { float x, y, z; };
struct Vec3i { int32_t x, y, z; };
```

---

## Display Systems

### HUB75 (128×32 RGB)

- **Configuration**: 2× 64×32 panels side-by-side
- **Coordinate System**: (0,0) = top-left, X increases right, Y increases down
- **Color Depth**: 24-bit RGB (8 bits per channel)
- **Frame Rate**: 60 FPS typical, 30 FPS minimum
- **Notes**: GPU automatically mirrors X-axis for correct orientation

### OLED (128×128 Mono)

- **Controller**: SH1107
- **Interface**: I2C @ 0x3C
- **Color Depth**: 1-bit (on/off)
- **Frame Rate**: 15 FPS typical
- **Notes**: GPU mirrors both X and Y for correct orientation

### Display Workflow

```cpp
// HUB75 rendering
gpu.setTarget(0);           // Target HUB75
gpu.hub75Clear(0, 0, 0);    // Clear to black
gpu.hub75Line(0, 0, 127, 31, 255, 0, 0);  // Draw red diagonal
gpu.hub75Present();         // Swap to display

// OLED rendering
gpu.oledClear();            // Clear OLED
gpu.oledText(10, 10, "Hello");  // Draw text
gpu.oledPresent();          // Update display
```

---

## Example: Polygon Demo with Cycling Hue

This example demonstrates:
- UART setup for CPU→GPU communication
- HSV to RGB color conversion
- Scanline polygon fill algorithm
- Animated hue cycling

### Full Source Code (`CPU_PolygonDemo.cpp`)

```cpp
/*****************************************************************
 * CPU_PolygonDemo.cpp - Simple filled polygon with RGB effect
 * 
 * Displays a filled polygon on both HUB75 panels with animated
 * RGB color cycling. OLED stays black.
 *****************************************************************/

#include <cstdio>
#include <cstring>
#include <cmath>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char* TAG = "POLY_DEMO";

// ============================================================
// UART Configuration
// ============================================================
constexpr uart_port_t UART_PORT = UART_NUM_1;
constexpr int UART_TX_PIN = 12;
constexpr int UART_RX_PIN = 11;
constexpr int UART_BAUD = 10000000;

// ============================================================
// Protocol Constants
// ============================================================
constexpr uint8_t SYNC0 = 0xAA;
constexpr uint8_t SYNC1 = 0x55;

enum CmdType : uint8_t {
    DRAW_PIXEL = 0x40,
    CLEAR = 0x47,
    SET_TARGET = 0x50,
    PRESENT = 0x51,
    OLED_CLEAR = 0x60,
    OLED_PRESENT = 0x65,
};

// ============================================================
// Polygon Vertices (YOUR SHAPE)
// ============================================================
// Original: {6,8},{14,8},{20,11},{26,17},{27,19},{28,22},{23,22},
//           {21,19},{19,17},{17,17},{16,19},{18,22},{7,22},{4,20},{2,17},{2,12}
static const int16_t POLY_X[] = {6, 14, 20, 26, 27, 28, 23, 21, 19, 17, 16, 18, 7, 4, 2, 2};
static const int16_t POLY_Y[] = {8, 8, 11, 17, 19, 22, 22, 19, 17, 17, 19, 22, 22, 20, 17, 12};
static const int NUM_VERTS = 16;

// ============================================================
// Send Command to GPU
// ============================================================
static void sendCmd(CmdType type, const uint8_t* payload, uint16_t len) {
    uint8_t header[5] = {SYNC0, SYNC1, type, (uint8_t)(len & 0xFF), (uint8_t)(len >> 8)};
    uart_write_bytes(UART_PORT, header, 5);
    if (len > 0 && payload) {
        uart_write_bytes(UART_PORT, payload, len);
    }
    uart_wait_tx_done(UART_PORT, pdMS_TO_TICKS(50));
}

static void setTarget(uint8_t t) { sendCmd(SET_TARGET, &t, 1); }
static void present() { sendCmd(PRESENT, nullptr, 0); }
static void oledClear() { sendCmd(OLED_CLEAR, nullptr, 0); }
static void oledPresent() { sendCmd(OLED_PRESENT, nullptr, 0); }

static void clear(uint8_t r, uint8_t g, uint8_t b) {
    uint8_t p[3] = {r, g, b};
    sendCmd(CLEAR, p, 3);
}

// ============================================================
// HSV to RGB Conversion
// ============================================================
static void hsvToRgb(float h, float s, float v, uint8_t* r, uint8_t* g, uint8_t* b) {
    float c = v * s;
    float x = c * (1 - fabsf(fmodf(h / 60.0f, 2) - 1));
    float m = v - c;
    
    float rf, gf, bf;
    if (h < 60)       { rf = c; gf = x; bf = 0; }
    else if (h < 120) { rf = x; gf = c; bf = 0; }
    else if (h < 180) { rf = 0; gf = c; bf = x; }
    else if (h < 240) { rf = 0; gf = x; bf = c; }
    else if (h < 300) { rf = x; gf = 0; bf = c; }
    else              { rf = c; gf = 0; bf = x; }
    
    *r = (uint8_t)((rf + m) * 255);
    *g = (uint8_t)((gf + m) * 255);
    *b = (uint8_t)((bf + m) * 255);
}

// ============================================================
// Scanline Polygon Fill with Gradient
// ============================================================
static void fillPolygonGradient(int16_t* px, int16_t* py, int n, float baseHue) {
    // Find bounding box
    int16_t minX = px[0], maxX = px[0];
    int16_t minY = py[0], maxY = py[0];
    for (int i = 1; i < n; i++) {
        if (px[i] < minX) minX = px[i];
        if (px[i] > maxX) maxX = px[i];
        if (py[i] < minY) minY = py[i];
        if (py[i] > maxY) maxY = py[i];
    }
    
    float width = maxX - minX;
    if (width < 1) width = 1;
    
    // Scanline fill
    for (int16_t y = minY; y <= maxY; y++) {
        int16_t nodeX[32];
        int nodes = 0;
        
        // Find edge intersections
        int j = n - 1;
        for (int i = 0; i < n; i++) {
            if ((py[i] < y && py[j] >= y) || (py[j] < y && py[i] >= y)) {
                nodeX[nodes++] = px[i] + (y - py[i]) * (px[j] - px[i]) / (py[j] - py[i]);
            }
            j = i;
        }
        
        // Sort intersections
        for (int i = 0; i < nodes - 1; i++) {
            for (int k = i + 1; k < nodes; k++) {
                if (nodeX[i] > nodeX[k]) {
                    int16_t tmp = nodeX[i];
                    nodeX[i] = nodeX[k];
                    nodeX[k] = tmp;
                }
            }
        }
        
        // Fill between pairs with gradient
        for (int i = 0; i < nodes; i += 2) {
            if (i + 1 < nodes) {
                for (int16_t x = nodeX[i]; x <= nodeX[i+1]; x++) {
                    // Gradient: position-based hue (0° to 240° = red to blue)
                    float t = (float)(x - minX) / width;
                    float hue = baseHue + t * 240.0f;  // Red→Blue spectrum
                    while (hue >= 360.0f) hue -= 360.0f;
                    
                    uint8_t r, g, b;
                    hsvToRgb(hue, 1.0f, 1.0f, &r, &g, &b);
                    
                    // Send DRAW_PIXEL command
                    uint8_t p[7];
                    p[0] = x & 0xFF; p[1] = x >> 8;
                    p[2] = y & 0xFF; p[3] = y >> 8;
                    p[4] = r; p[5] = g; p[6] = b;
                    uint8_t hdr[5] = {SYNC0, SYNC1, DRAW_PIXEL, 7, 0};
                    uart_write_bytes(UART_PORT, hdr, 5);
                    uart_write_bytes(UART_PORT, p, 7);
                }
            }
        }
    }
}

// ============================================================
// UART Initialization
// ============================================================
static bool initUart() {
    uart_config_t cfg = {
        .baud_rate = UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,
        .source_clk = UART_SCLK_DEFAULT,
    };
    uart_param_config(UART_PORT, &cfg);
    uart_set_pin(UART_PORT, UART_TX_PIN, UART_RX_PIN, -1, -1);
    uart_driver_install(UART_PORT, 1024, 1024, 0, nullptr, 0);
    return true;
}

// ============================================================
// Main Entry Point
// ============================================================
extern "C" void app_main() {
    ESP_LOGI(TAG, "=== Polygon Demo ===");
    
    initUart();
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // Clear OLED once
    oledClear();
    oledPresent();
    
    // Prepare polygon vertices - FLIPPED horizontally for both eyes
    int16_t leftX[NUM_VERTS], leftY[NUM_VERTS];
    int16_t rightX[NUM_VERTS], rightY[NUM_VERTS];
    
    for (int i = 0; i < NUM_VERTS; i++) {
        // Left eye: mirror around x=32
        leftX[i] = 64 - POLY_X[i];
        leftY[i] = POLY_Y[i];
        
        // Right eye: mirror around x=96
        rightX[i] = 128 - POLY_X[i];
        rightY[i] = POLY_Y[i];
    }
    
    // Animation loop
    float hueOffset = 0.0f;
    
    while (true) {
        setTarget(0);
        clear(0, 0, 0);
        
        // Draw both eyes with gradient
        fillPolygonGradient(leftX, leftY, NUM_VERTS, hueOffset);
        fillPolygonGradient(rightX, rightY, NUM_VERTS, hueOffset);
        
        uart_wait_tx_done(UART_PORT, pdMS_TO_TICKS(100));
        present();
        
        // Cycle hue for animation (remove this line for static gradient)
        hueOffset += 2.0f;
        if (hueOffset >= 360.0f) hueOffset -= 360.0f;
        
        vTaskDelay(pdMS_TO_TICKS(33));  // ~30 FPS
    }
}
```

### Static vs Animated Gradient

**For static gradient (red→blue):**
```cpp
// Set hueOffset = 0 and don't increment it
fillPolygonGradient(leftX, leftY, NUM_VERTS, 0.0f);
```

**For cycling animation:**
```cpp
hueOffset += 2.0f;  // Speed of color cycling
if (hueOffset >= 360.0f) hueOffset -= 360.0f;
```

---

## PlatformIO Build Configuration

### For CPU Polygon Demo

Add to `platformio.ini`:

```ini
[env:CPU_PolygonDemo]
platform = espressif32
board = esp32s3usbotg
framework = espidf
upload_port = COM15
monitor_port = COM15
monitor_speed = 115200
board_build.flash_size = 8MB
board_build.cmake_extra_args = -DCPU_POLYGON_DEMO_MODE=1
build_flags = 
    -DARDUINO_USB_CDC_ON_BOOT
    -DCPU_BUILD
    -O3
    -std=c++17
```

Add to `src/CMakeLists.txt`:

```cmake
option(CPU_POLYGON_DEMO_MODE "Build CPU Polygon Demo" OFF)

if(CPU_POLYGON_DEMO_MODE)
    idf_component_register(
        SRCS "CPU_PolygonDemo.cpp"
        INCLUDE_DIRS "../include"
    )
endif()
```

### For GPU Programmable

```ini
[env:GPU_Programmable]
platform = espressif32
board = esp32s3usbotg
framework = espidf
upload_port = COM5
monitor_port = COM5
monitor_speed = 115200
board_build.flash_size = 8MB
board_build.sdkconfig_defaults = sdkconfig.defaults.GPU
board_build.cmake_extra_args = -DGPU_PROGRAMMABLE_MODE=1
build_flags = 
    -DARDUINO_USB_CDC_ON_BOOT
    -DGPU_BUILD
    -DGPU_PROGRAMMABLE
    -O3
    -std=c++17
    -I ../include
```

### Build Commands

```bash
# Build and upload GPU firmware first
pio run -e GPU_Programmable -t upload

# Build and upload CPU demo
pio run -e CPU_PolygonDemo -t upload
```

---

## File Structure

```
Alpha_Release_Stack/
├── platformio.ini              # Build configurations
├── CMakeLists.txt              # Main CMake config
├── PIN_MAPPING_CPU.md          # CPU pin reference
├── SYSTEM_ARCHITECTURE.md      # THIS FILE
│
├── include/
│   ├── HAL/
│   │   ├── hal.hpp             # Master HAL include
│   │   ├── HalTypes.hpp        # Common types
│   │   ├── IHalGpio.hpp        # GPIO interface
│   │   ├── IHalI2c.hpp         # I2C interface
│   │   ├── IHalUart.hpp        # UART interface
│   │   ├── IHalImu.hpp         # IMU interface
│   │   ├── IHalEnvironmental.hpp
│   │   ├── IHalGps.hpp
│   │   ├── IHalMicrophone.hpp
│   │   └── ...
│   │
│   ├── GpuDriver/
│   │   ├── GpuCommands.hpp     # Stable GPU command wrapper
│   │   ├── GpuBaseAPI.hpp
│   │   └── ...
│   │
│   ├── FrameworkAPI/
│   │   ├── Physics2D.hpp       # 2D physics engine
│   │   ├── InputManager.hpp
│   │   └── ...
│   │
│   └── SystemAPI/
│       ├── SystemAPI.hpp
│       ├── WebServer.hpp
│       └── UI/
│
├── src/
│   ├── CMakeLists.txt          # Source selection
│   ├── CPU.cpp                 # Default CPU main
│   ├── GPU.cpp                 # Default GPU main
│   ├── CPU_PolygonDemo.cpp     # Polygon demo
│   ├── GPU_Programmable.cpp    # Programmable GPU firmware
│   └── ...
│
└── ARCOS/ARCOS_library/        # Core ARCOS library
    └── include/
        └── abstraction/
            └── drivers/
                └── components/
                    ├── HUB75/
                    └── OLED/
```

---

## Quick Reference Card

### Essential Commands

```cpp
// Setup
GpuCommands gpu;
gpu.init();  // UART @ 10Mbps on GPIO 12/11

// HUB75 (128x32 RGB)
gpu.hub75Clear(0, 0, 0);
gpu.hub75Pixel(x, y, r, g, b);
gpu.hub75Line(x1, y1, x2, y2, r, g, b);
gpu.hub75Fill(x, y, w, h, r, g, b);
gpu.hub75Present();

// OLED (128x128 Mono)
gpu.oledClear();
gpu.oledText(x, y, "Hello", scale, on);
gpu.oledFill(x, y, w, h, on);
gpu.oledPresent();
```

### Key Constants

| Constant | Value |
|----------|-------|
| CPU→GPU Baud | 10,000,000 |
| CPU TX Pin | GPIO 12 |
| CPU RX Pin | GPIO 11 |
| GPU RX Pin | GPIO 13 |
| HUB75 Size | 128×32 |
| OLED Size | 128×128 |
| IMU Address | 0x68 |
| BME280 Address | 0x76 |
| OLED Address | 0x3C |

---

*Last updated: January 2026*
