# ARCOS API Documentation Index

## Overview

This document serves as the master index for all API documentation in the ARCOS project. Use this to navigate to specific API documentation for each layer of the system.

## System Architecture

```
┌─────────────────────────────────────────────────────────┐
│                  Application Layer                       │
│          (User code, animations, behaviors)              │
├─────────────────────────────────────────────────────────┤
│                  Middleware Layer                        │
│   ┌─────────────┬─────────────┬─────────────────────┐   │
│   │  Odometry   │  Telemetry  │  Graphics Pipeline  │   │
│   │  (motion)   │ (sensors)   │    (rendering)      │   │
│   └─────────────┴─────────────┴─────────────────────┘   │
│   ⚠️  Platform-independent only!                         │
│   ⚠️  No register/direct sensor access!                  │
├─────────────────────────────────────────────────────────┤
│                Base System API (HAL)                     │
│   (Hardware Abstraction - platform-independent APIs)     │
├─────────────────────────────────────────────────────────┤
│              Platform Implementation                     │
│         (ESP32-S3 specific implementations)              │
└─────────────────────────────────────────────────────────┘
```

## API Documentation

### HAL (Hardware Abstraction Layer) - Base System API

The HAL provides the "Base System API" - a complete abstraction layer for all hardware peripherals.

| Document | Location | Description |
|----------|----------|-------------|
| **HAL API** | [include/HAL/README.md](include/HAL/README.md) | Hardware abstraction interfaces for all peripherals |

**Interface Files (include/HAL/):**
| Interface | File | Description |
|-----------|------|-------------|
| Master Header | [Hal.hpp](include/HAL/Hal.hpp) | Includes all HAL interfaces |
| Common Types | [HalTypes.hpp](include/HAL/HalTypes.hpp) | RGB, Vec3f, HalResult, etc. |
| Logging | [IHalLog.hpp](include/HAL/IHalLog.hpp) | Log levels, error handling |
| Timer | [IHalTimer.hpp](include/HAL/IHalTimer.hpp) | System timing, delays |
| GPIO | [IHalGpio.hpp](include/HAL/IHalGpio.hpp) | Digital I/O, buttons |
| UART | [IHalUart.hpp](include/HAL/IHalUart.hpp) | Serial communication |
| I2C | [IHalI2c.hpp](include/HAL/IHalI2c.hpp) | I2C bus operations |
| SPI | [IHalSpi.hpp](include/HAL/IHalSpi.hpp) | SPI bus operations |
| I2S | [IHalI2s.hpp](include/HAL/IHalI2s.hpp) | Audio I2S interface |
| IMU | [IHalImu.hpp](include/HAL/IHalImu.hpp) | Accelerometer, gyroscope |
| Environmental | [IHalEnvironmental.hpp](include/HAL/IHalEnvironmental.hpp) | Temperature, humidity, pressure |
| GPS | [IHalGps.hpp](include/HAL/IHalGps.hpp) | Position, velocity, time |
| Microphone | [IHalMicrophone.hpp](include/HAL/IHalMicrophone.hpp) | Audio input, levels |
| LED Strip | [IHalLedStrip.hpp](include/HAL/IHalLedStrip.hpp) | Addressable LEDs |
| Display | [IHalDisplay.hpp](include/HAL/IHalDisplay.hpp) | HUB75, OLED displays |
| Storage | [IHalStorage.hpp](include/HAL/IHalStorage.hpp) | SD card, file operations |

**ESP32 Implementation Files (src/HAL/ESP32/):**
| Implementation | File | Description |
|----------------|------|-------------|
| Master Header | [Esp32Hal.hpp](src/HAL/ESP32/Esp32Hal.hpp) | Includes all ESP32 implementations |
| Factory | Esp32HalFactory | Creates and initializes all HAL instances |
| Logging | [Esp32HalLog.hpp](src/HAL/ESP32/Esp32HalLog.hpp) | Serial logging implementation |
| Timer | [Esp32HalTimer.hpp](src/HAL/ESP32/Esp32HalTimer.hpp) | millis/micros/delay |
| GPIO | [Esp32HalGpio.hpp](src/HAL/ESP32/Esp32HalGpio.hpp) | GPIO, PWM, Button |
| UART | [Esp32HalUart.hpp](src/HAL/ESP32/Esp32HalUart.hpp) | HardwareSerial wrapper |
| I2C | [Esp32HalI2c.hpp](src/HAL/ESP32/Esp32HalI2c.hpp) | Wire library wrapper |
| SPI | [Esp32HalSpi.hpp](src/HAL/ESP32/Esp32HalSpi.hpp) | SPI library wrapper |
| I2S | [Esp32HalI2s.hpp](src/HAL/ESP32/Esp32HalI2s.hpp) | ESP-IDF I2S driver |
| Sensors | [Esp32HalSensors.hpp](src/HAL/ESP32/Esp32HalSensors.hpp) | ICM20948, BME280 |
| GPS | [Esp32HalGps.hpp](src/HAL/ESP32/Esp32HalGps.hpp) | NEO-8M NMEA parser |
| Microphone | [Esp32HalMicrophone.hpp](src/HAL/ESP32/Esp32HalMicrophone.hpp) | INMP441 I2S mic |
| LED Strip | [Esp32HalLedStrip.hpp](src/HAL/ESP32/Esp32HalLedStrip.hpp) | NeoPixel wrapper |
| Display | [Esp32HalDisplay.hpp](src/HAL/ESP32/Esp32HalDisplay.hpp) | HUB75, OLED displays |
| Storage | [Esp32HalStorage.hpp](src/HAL/ESP32/Esp32HalStorage.hpp) | SD card file system |

---

### Communications

| Document | Location | Description |
|----------|----------|-------------|
| **UART Protocol** | [include/Comms/UartProtocol.hpp](include/Comms/UartProtocol.hpp) | CPU-GPU communication protocol |

**Key Files:**
- [UartProtocol.hpp](include/Comms/UartProtocol.hpp) - Message types, frame format
- [CpuUartHandler.hpp](include/Comms/CpuUartHandler.hpp) - CPU-side UART handler
- [GpuUartHandler.hpp](include/Comms/GpuUartHandler.hpp) - GPU-side UART handler

---

### Middleware (To Be Implemented)

| Document | Location | Description |
|----------|----------|-------------|
| Odometry API | `include/Middleware/Odometry/README.md` | Motion tracking, orientation |
| Telemetry API | `include/Middleware/Telemetry/README.md` | Sensor data aggregation |
| Graphics API | `include/Middleware/Graphics/README.md` | Display rendering pipeline |

---

### Managers

| Document | Location | Description |
|----------|----------|-------------|
| HUB75 Display | [include/Manager/HUB75DisplayManager.hpp](include/Manager/HUB75DisplayManager.hpp) | HUB75 LED matrix management |

---

## Hardware Reference

| Document | Location | Description |
|----------|----------|-------------|
| **CPU Pin Mapping** | [PIN_MAPPING_CPU.md](PIN_MAPPING_CPU.md) | CPU (COM 15) pin assignments |
| **Coding Style** | [CODING_STYLE.md](CODING_STYLE.md) | Project coding standards |

---

## Quick Reference

### Hardware Configuration

| Device | Port | Notes |
|--------|------|-------|
| **CPU** | COM 15 | Main processor - sensors, buttons, LEDs |
| **GPU** | COM 16 | Display processor - HUB75, OLED |
| **CPU-GPU Link** | 10 Mbps UART | High-speed serial connection |

### CPU Hardware (COM 15)

| Peripheral | Interface | Pins | HAL Interface | ESP32 Implementation |
|------------|-----------|------|---------------|----------------------|
| ICM20948 IMU | I2C (0x68) | SDA:9, SCL:10 | `IHalImu` | `Esp32HalImu` |
| BME280 | I2C (0x76) | SDA:9, SCL:10 | `IHalEnvironmental` | `Esp32HalEnvironmental` |
| NEO-8M GPS | UART | TX:43, RX:44 | `IHalGps` | `Esp32HalGps` |
| INMP441 Mic | I2S | WS:42, BCK:40, SD:2 | `IHalMicrophone` | `Esp32HalMicrophone` |
| LED Left Fin | GPIO 18 | 13 LEDs | `IHalLedStrip` | `Esp32HalLedStrip` |
| LED Right Fin | GPIO 38 | 13 LEDs | `IHalLedStrip` | `Esp32HalLedStrip` |
| LED Tongue | GPIO 8 | 9 LEDs | `IHalLedStrip` | `Esp32HalLedStrip` |
| LED Scale | GPIO 37 | 14 LEDs | `IHalLedStrip` | `Esp32HalLedStrip` |
| Buttons A-D | GPIO | 5, 6, 7, 15 | `IHalButton` | `Esp32HalButton` |
| Fans | PWM | 17, 36 | `IHalPwm` | `Esp32HalPwm` |
| SD Card | SPI | MISO:14, MOSI:47, CLK:21, CS:48 | `IHalStorage` | `Esp32HalStorage` |
| GPU UART | UART | TX:12, RX:11 | `IHalUart` | `Esp32HalUart` |

### GPU Hardware (COM 16)

| Peripheral | Interface | HAL Interface | ESP32 Implementation |
|------------|-----------|---------------|----------------------|
| HUB75 Matrix | Parallel | `IHalHub75Display` | `Esp32HalHub75Display` |
| OLED SH1107 | I2C | `IHalOledDisplay` | `Esp32HalOledDisplay` |
| CPU UART | UART | `IHalUart` | `Esp32HalUart` |

---

## HAL Implementation Status

### ✅ Fully Implemented
| Interface | Implementation | Status |
|-----------|----------------|--------|
| `IHalLog` | `Esp32HalLog` | ✅ Complete |
| `IHalTimer` | `Esp32HalSystemTimer` | ✅ Complete |
| `IHalGpio` | `Esp32HalGpio` | ✅ Complete |
| `IHalPwm` | `Esp32HalPwm` | ✅ Complete |
| `IHalButton` | `Esp32HalButton` | ✅ Complete |
| `IHalUart` | `Esp32HalUart` | ✅ Complete |
| `IHalI2c` | `Esp32HalI2c` | ✅ Complete |
| `IHalSpi` | `Esp32HalSpi` | ✅ Complete |
| `IHalI2s` | `Esp32HalI2s` | ✅ Complete |
| `IHalImu` | `Esp32HalImu` | ✅ Complete (ICM20948) |
| `IHalEnvironmental` | `Esp32HalEnvironmental` | ✅ Complete (BME280) |
| `IHalGps` | `Esp32HalGps` | ✅ Complete (NMEA) |
| `IHalMicrophone` | `Esp32HalMicrophone` | ✅ Complete (INMP441) |
| `IHalLedStrip` | `Esp32HalLedStrip` | ✅ Complete (SK6812) |
| `IHalStorage` | `Esp32HalStorage` | ✅ Complete (SD Card) |
| `IHalFile` | `Esp32HalFile` | ✅ Complete |
| `IHalOledDisplay` | `Esp32HalOledDisplay` | ✅ Complete (SH1107/SSD1306) |
| `IHalHub75Display` | `Esp32HalHub75Display` | ✅ Complete (with DMA library) |

---

## Layer Rules

### HAL Layer (Base System API)
- ✅ Platform-specific implementations allowed
- ✅ Direct register access allowed
- ✅ Hardware-specific includes allowed
- 📤 Exposes platform-independent interfaces

### Middleware Layer
- ❌ NO platform-specific code
- ❌ NO direct register access
- ❌ NO raw sensor access
- ✅ Uses only HAL interfaces
- ✅ Builds odometry, telemetry, graphics services

### Application Layer
- ❌ NO platform-specific code
- ❌ NO HAL direct access (use middleware)
- ✅ High-level behavior and logic
- ✅ Animation and UI code

---

## Usage Examples

### Using the HAL Factory

```cpp
#include "HAL/ESP32/Esp32Hal.hpp"
using namespace arcos::hal::esp32;

Esp32HalFactory hal;

void setup(){
  // Initialize core components
  hal.initCore();
  
  // Initialize I2C
  I2cConfig i2c_cfg = {.sda_pin = 9, .scl_pin = 10, .frequency = 400000};
  hal.initI2c(i2c_cfg);
  
  // Initialize sensors
  ImuConfig imu_cfg = {.address = 0x68};
  EnvironmentalConfig env_cfg = {.address = 0x76};
  hal.initSensors(imu_cfg, env_cfg);
}

void loop(){
  ImuData imu;
  hal.imu.readAll(imu);
  
  EnvironmentalData env;
  hal.env.readAll(env);
}
```

### Using Dependency Injection

```cpp
class TelemetryService{
public:
  TelemetryService(IHalImu* imu, IHalEnvironmental* env, IHalGps* gps)
    : imu_(imu), env_(env), gps_(gps){}
  
  void update(){
    imu_->readAll(imu_data_);
    env_->readAll(env_data_);
    gps_->update();
  }
  
private:
  IHalImu* imu_;
  IHalEnvironmental* env_;
  IHalGps* gps_;
  ImuData imu_data_;
  EnvironmentalData env_data_;
};
```

---

*Part of the ARCOS (Autonomous Robotic Control & Operations System) project*
*Author: XCR1793 (Feather Forge)*
