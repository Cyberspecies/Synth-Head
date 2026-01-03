# HAL (Hardware Abstraction Layer) API

## Overview

The HAL API provides **platform-independent interfaces** for all hardware access in the ARCOS system. This layer sits at the bottom of the software stack and abstracts all platform-specific code, registers, and direct hardware access.

```
┌────────────────────────────────────────────┐
│           Application Layer                │
│    (User code, animations, logic)          │
├────────────────────────────────────────────┤
│           Middleware Layer                 │
│  (Odometry, Telemetry, Graphics Pipeline)  │
│  ⚠️ NO platform-specific code allowed!     │
│  ⚠️ NO direct register/sensor access!      │
├────────────────────────────────────────────┤
│              HAL Layer                     │  ◄── YOU ARE HERE
│  (This API - Platform-independent          │
│   interfaces for all hardware)             │
├────────────────────────────────────────────┤
│       Platform Implementation              │
│  (ESP32-S3, etc. - implements HAL          │
│   interfaces with platform-specific code)  │
└────────────────────────────────────────────┘
```

## Design Principles

1. **Platform Independence**: All interfaces use only standard C++ types and custom ARCOS types. No platform-specific headers or types are exposed.

2. **Interface Segregation**: Each hardware category has its own interface file. Include only what you need.

3. **Dependency Injection**: Middleware receives HAL implementations through constructor injection, not global singletons.

4. **No Register Access**: Middleware MUST NOT access hardware registers directly. All hardware interaction goes through HAL interfaces.

5. **Sensor Abstraction**: Raw sensors (IMU, GPS, etc.) are wrapped in HAL interfaces. Middleware builds higher-level services (odometry, telemetry) on top.

## File Structure

```
include/HAL/
├── Hal.hpp                 # Master header - includes all interfaces
├── HalTypes.hpp            # Common types (RGB, Vec3f, HalResult, etc.)
├── README.md               # This file
│
├── Communication Interfaces
│   ├── IHalGpio.hpp        # GPIO, PWM, Button interfaces
│   ├── IHalUart.hpp        # UART serial communication
│   ├── IHalI2c.hpp         # I2C master communication
│   ├── IHalSpi.hpp         # SPI master communication
│   └── IHalI2s.hpp         # I2S audio interface
│
├── System Interfaces
│   └── IHalTimer.hpp       # System timers, delays, timestamps
│
├── Sensor Interfaces
│   ├── IHalImu.hpp         # 9-axis IMU (ICM20948)
│   ├── IHalEnvironmental.hpp # Temp/Humidity/Pressure (BME280)
│   ├── IHalGps.hpp         # GPS positioning (NEO-8M)
│   └── IHalMicrophone.hpp  # I2S microphone (INMP441)
│
├── Output Interfaces
│   ├── IHalLedStrip.hpp    # Addressable LED strips (NeoPixel/SK6812)
│   └── IHalDisplay.hpp     # Displays (HUB75, OLED)
│
└── Storage Interfaces
    └── IHalStorage.hpp     # SD card, file operations
```

## Quick Start

### Include the Master Header

```cpp
#include "HAL/Hal.hpp"

// All interfaces are now available under arcos::hal namespace
```

### Or Include Individual Interfaces

```cpp
#include "HAL/IHalI2c.hpp"
#include "HAL/IHalImu.hpp"

// Only I2C and IMU interfaces available
```

## Interface Categories

### Communication Interfaces

| Interface | File | Purpose |
|-----------|------|---------|
| `IHalGpio` | [IHalGpio.hpp](IHalGpio.hpp) | Digital GPIO pin control |
| `IHalPwm` | [IHalGpio.hpp](IHalGpio.hpp) | PWM output (fans, dimmers) |
| `IHalButton` | [IHalGpio.hpp](IHalGpio.hpp) | Button input with debouncing |
| `IHalUart` | [IHalUart.hpp](IHalUart.hpp) | UART serial communication |
| `IHalI2c` | [IHalI2c.hpp](IHalI2c.hpp) | I2C master communication |
| `IHalSpi` | [IHalSpi.hpp](IHalSpi.hpp) | SPI master communication |
| `IHalI2s` | [IHalI2s.hpp](IHalI2s.hpp) | I2S audio streaming |

### System Interfaces

| Interface | File | Purpose |
|-----------|------|---------|
| `IHalSystemTimer` | [IHalTimer.hpp](IHalTimer.hpp) | Timestamps, delays |
| `IHalHardwareTimer` | [IHalTimer.hpp](IHalTimer.hpp) | Periodic interrupts |

### Sensor Interfaces

| Interface | File | Hardware | Middleware Use |
|-----------|------|----------|----------------|
| `IHalImu` | [IHalImu.hpp](IHalImu.hpp) | ICM20948 | → Odometry |
| `IHalEnvironmental` | [IHalEnvironmental.hpp](IHalEnvironmental.hpp) | BME280 | → Telemetry |
| `IHalGps` | [IHalGps.hpp](IHalGps.hpp) | NEO-8M | → Telemetry/Navigation |
| `IHalMicrophone` | [IHalMicrophone.hpp](IHalMicrophone.hpp) | INMP441 | → Audio Processing |

### Output Interfaces

| Interface | File | Hardware |
|-----------|------|----------|
| `IHalLedStrip` | [IHalLedStrip.hpp](IHalLedStrip.hpp) | NeoPixel/SK6812 RGBW |
| `IHalHub75Display` | [IHalDisplay.hpp](IHalDisplay.hpp) | HUB75 LED Matrix |
| `IHalOledDisplay` | [IHalDisplay.hpp](IHalDisplay.hpp) | OLED SH1107 |

### Storage Interfaces

| Interface | File | Hardware |
|-----------|------|----------|
| `IHalStorage` | [IHalStorage.hpp](IHalStorage.hpp) | SD Card |
| `IHalFile` | [IHalStorage.hpp](IHalStorage.hpp) | File operations |

## Pin Mapping

Pin definitions are available in `arcos::hal::pins` namespace:

```cpp
#include "HAL/Hal.hpp"
using namespace arcos::hal::pins;

// CPU pins (COM 15)
auto sda_pin = cpu::I2C_SDA;       // GPIO 9
auto button_a = cpu::BUTTON_A;     // GPIO 5

// GPU pins (COM 16)
auto gpu_tx = gpu::UART_TX;        // GPIO 12

// I2C addresses
auto imu_addr = i2c_addr::ICM20948; // 0x68

// Default settings
auto baud = defaults::CPU_GPU_BAUD; // 10 Mbps
```

## Usage Example

### Middleware Using HAL (Correct)

```cpp
// OdometryService.hpp
#include "HAL/IHalImu.hpp"

namespace arcos::middleware{

class OdometryService{
public:
  // Dependency injection - receives HAL implementation
  OdometryService(arcos::hal::IHalImu* imu)
    : imu_(imu){}
  
  bool init(){
    arcos::hal::ImuConfig config;
    config.accel_range = 4;
    config.gyro_range = 500;
    return imu_->init(config) == arcos::hal::HalResult::OK;
  }
  
  void update(){
    arcos::hal::ImuData data;
    if(imu_->readAll(data) == arcos::hal::HalResult::OK){
      // Process IMU data for odometry
      updateOrientation(data.accel, data.gyro);
    }
  }

private:
  arcos::hal::IHalImu* imu_;
  
  void updateOrientation(const arcos::hal::Vec3f& accel, 
                         const arcos::hal::Vec3f& gyro);
};

} // namespace arcos::middleware
```

### What NOT to Do in Middleware

```cpp
// ❌ WRONG - Direct hardware access in middleware
#include <driver/i2c.h>  // Platform-specific!

class BadOdometry{
  void readImu(){
    // ❌ Direct register access
    i2c_master_write_read_device(I2C_NUM_0, 0x68, ...);
  }
};
```

## Common Types

### HalResult

```cpp
enum class HalResult : uint8_t{
  OK = 0,           // Success
  ERROR,            // Generic error
  TIMEOUT,          // Operation timed out
  BUSY,             // Resource busy
  INVALID_PARAM,    // Invalid parameter
  NOT_INITIALIZED,  // Not initialized
  NOT_SUPPORTED,    // Not supported
  BUFFER_FULL,      // Buffer full
  BUFFER_EMPTY,     // Buffer empty
  NO_DATA,          // No data available
  HARDWARE_FAULT    // Hardware fault
};
```

### Color Types

```cpp
struct RGB{
  uint8_t r, g, b;
};

struct RGBW{
  uint8_t r, g, b, w;
  RGB toRGB() const;
};
```

### Vector Types

```cpp
struct Vec3f{
  float x, y, z;
};

struct Vec3i{
  int32_t x, y, z;
};
```

## Hardware Summary

### CPU (COM 15) Hardware

| Category | Hardware | HAL Interface |
|----------|----------|---------------|
| IMU | ICM20948 (I2C: 0x68) | `IHalImu` |
| Environmental | BME280 (I2C: 0x76) | `IHalEnvironmental` |
| GPS | NEO-8M (UART) | `IHalGps` |
| Microphone | INMP441 (I2S) | `IHalMicrophone` |
| LED Strips | SK6812 RGBW | `IHalLedStrip` |
| Buttons | A, B, C, D | `IHalButton` |
| Fans | PWM x2 | `IHalPwm` |
| SD Card | SPI | `IHalStorage` |
| GPU Comm | UART 10Mbps | `IHalUart` |

### GPU (COM 16) Hardware

| Category | Hardware | HAL Interface |
|----------|----------|---------------|
| Display | HUB75 128x32 | `IHalHub75Display` |
| OLED | SH1107 128x128 | `IHalOledDisplay` |
| CPU Comm | UART 10Mbps | `IHalUart` |

## See Also

- [CODING_STYLE.md](../../CODING_STYLE.md) - Project coding standards
- [PIN_MAPPING_CPU.md](../../PIN_MAPPING_CPU.md) - Hardware pin assignments
- [API_INDEX.md](../API_INDEX.md) - All API documentation index

---

*Part of the ARCOS (Autonomous Robotic Control & Operations System) project*
*Author: XCR1793 (Feather Forge)*
