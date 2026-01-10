# Lucidus/ARCOS API Documentation Index

## Overview

This document serves as the master index for all API documentation in the Lucidus project (powered by ARCOS - Autonomous Robotic Control & Operations System). Lucidus is an ESP32-S3 based animatronic eye display system with WiFi configuration, LED matrix displays, and a comprehensive sensor suite.

## System Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                     APPLICATION LAYER                           │
│  ┌───────────────┬──────────────────┬───────────────────────┐  │
│  │ BootMode      │ CurrentMode      │ CaptivePortalManager  │  │
│  │ (startup)     │ (runtime)        │ (web configuration)   │  │
│  └───────────────┴──────────────────┴───────────────────────┘  │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │            EyeDisplayController (animations)              │  │
│  └───────────────────────────────────────────────────────────┘  │
├─────────────────────────────────────────────────────────────────┤
│                      SYSTEM API LAYER                           │
│  ┌─────────────┬─────────────┬──────────────┬───────────────┐  │
│  │   Network   │   Config    │   Security   │      UI       │  │
│  │  WiFiDriver │ EyeConfig   │ SecurityDrv  │  (future)     │  │
│  │  Portal     │ Storage     │ TRNG/Creds   │               │  │
│  └─────────────┴─────────────┴──────────────┴───────────────┘  │
├─────────────────────────────────────────────────────────────────┤
│                  MIDDLEWARE / GPU DRIVER                        │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │  GpuDriver │ GpuRenderer │ GpuCommands │ GpuAnimations   │  │
│  └───────────────────────────────────────────────────────────┘  │
├─────────────────────────────────────────────────────────────────┤
│                   HAL (Hardware Abstraction)                    │
│  ┌─────────────┬─────────────┬─────────────┬─────────────────┐ │
│  │   Timer     │    GPIO     │    I2C      │    UART         │ │
│  │   I2S       │    SPI      │   IMU       │    GPS          │ │
│  │   Display   │  LEDStrip   │  Storage    │   Microphone    │ │
│  └─────────────┴─────────────┴─────────────┴─────────────────┘ │
├─────────────────────────────────────────────────────────────────┤
│              ESP32-S3 Platform (ESP-IDF 5.5.0)                  │
└─────────────────────────────────────────────────────────────────┘
```

---

## Quick Start

### Connecting to Lucidus

1. **Power on** the device
2. **Connect to WiFi**: Look for `Lucidus_XXXX` network
3. **Open browser**: Navigate to `http://10.0.0.1`
4. **Configure**: Use the web interface to customize eyes, animations, and settings

### Building & Uploading

```bash
# Build and upload to CPU (main processor)
pio run -e CPU_Main -t upload

# Build and upload to GPU (display processor)
pio run -e GPU_Programmable -t upload

# Monitor serial output
pio device monitor -p COM15 -b 115200
```

---

## API Layers

### 1. Application Layer

High-level application logic including modes and managers.

| File | Location | Description |
|------|----------|-------------|
| `CPU_Main.cpp` | [src/CPU_Main.cpp](src/CPU_Main.cpp) | Main entry point |
| `BootMode` | [src/Modes/BootMode.hpp](src/Modes/BootMode.hpp) | Startup sequence, WiFi init |
| `CurrentMode` | [src/Modes/CurrentMode.hpp](src/Modes/CurrentMode.hpp) | Runtime eye display mode |
| `CaptivePortalManager` | [src/Application/CaptivePortalManager.hpp](src/Application/CaptivePortalManager.hpp) | Web portal orchestration |
| `EyeDisplayController` | [src/Application/EyeDisplayController.hpp](src/Application/EyeDisplayController.hpp) | Eye rendering & animation |

---

### 2. System API Layer

Platform services for networking, configuration, and security.

#### Network (`include/SystemAPI/Network/`)

| File | Description |
|------|-------------|
| [WiFiDriver.hpp](include/SystemAPI/Network/WiFiDriver.hpp) | WiFi AP/STA modes, DHCP, custom IP (10.0.0.1) |
| [CaptivePortalServer.hpp](include/SystemAPI/Network/CaptivePortalServer.hpp) | DNS + HTTP server for captive portal |

**WiFiDriver API:**
```cpp
namespace arcos::wifi {
    class WiFiDriver {
        static bool startAP(const char* ssid, const char* password);
        static bool startSTA(const char* ssid, const char* password);
        static void stop();
        static bool isConnected();
        static const char* getIP();
    };
}
```

**CaptivePortalServer API:**
```cpp
namespace arcos::portal {
    class CaptivePortalServer {
        static bool start();   // Start DNS (port 53) + HTTP (port 80)
        static void stop();
        static bool isRunning();
    };
}
```

#### Configuration (`include/SystemAPI/Config/`)

| File | Description |
|------|-------------|
| [EyeConfig.hpp](include/SystemAPI/Config/EyeConfig.hpp) | JSON-based eye configuration system |
| [ConfigStorage.hpp](include/SystemAPI/Config/ConfigStorage.hpp) | NVS persistent storage |

**EyeConfig Structures:**
```cpp
namespace arcos::config {
    // Shader types: SOLID, RAINBOW, GRADIENT, PULSE, FIRE
    struct ShaderConfig {
        ShaderType type;        // rainbow by default
        uint8_t speed;          // 0-100
        uint8_t saturation;     // 0-100
        uint8_t brightness;     // 0-100
        uint8_t color1_r/g/b;   // Primary color
        uint8_t color2_r/g/b;   // Secondary color
    };
    
    // Eye shape defined by polygon vertices
    struct EyeShape {
        char name[24];
        Point points[32];       // Polygon vertices
        uint8_t pointCount;
        int16_t offsetX, offsetY;
        float scale;
    };
    
    // Eye set (expression) using shapes
    struct EyeSet {
        char name[24];
        uint8_t leftOpenShape, leftClosedShape, leftHalfShape;
        uint8_t rightOpenShape, rightClosedShape, rightHalfShape;
    };
    
    // Complete configuration
    struct EyeConfiguration {
        DisplaySettings display;
        ShaderConfig shader;
        EyeShape shapes[8];
        EyeSet sets[16];
        Animation animations[16];
        Transition transitions[16];
    };
    
    // Initialize with defaults (user's eye polygon + rainbow)
    void initDefaultConfig();
}
```

**Default Eye Shape (Polygon):**
```cpp
// Coordinates for default eye:
{6, 8}, {14, 8}, {20, 11}, {26, 17}, {27, 19}, {28, 22},
{23, 22}, {21, 19}, {19, 17}, {17, 17}, {16, 19}, {18, 22},
{7, 22}, {4, 20}, {2, 17}, {2, 12}
```

#### Security (`include/SystemAPI/Security/`)

| File | Description |
|------|-------------|
| [SecurityDriver.hpp](include/SystemAPI/Security/SecurityDriver.hpp) | Hardware TRNG, credential generation |

**Security API:**
```cpp
namespace arcos::security {
    class TRNG {
        static bool getRandomBytes(uint8_t* buf, size_t len);
        static uint32_t getRandomU32();
        static bool generateAlphanumeric(char* buf, size_t len);
    };
    
    class CredentialManager {
        static void generateSSID(char* buf);      // "Lucidus_XXXX"
        static void generatePassword(char* buf); // 12-char random
        static bool loadFromNVS();
        static bool saveToNVS();
    };
}
```

---

### 3. HAL Layer (Hardware Abstraction)

Platform-independent interfaces for all hardware peripherals.

#### Interface Files (`include/HAL/`)

| Interface | File | Description |
|-----------|------|-------------|
| Master Header | [hal.hpp](include/HAL/hal.hpp) | Includes all HAL interfaces |
| Common Types | [HalTypes.hpp](include/HAL/HalTypes.hpp) | RGB, Vec3f, HalResult |
| Logging | [IHalLog.hpp](include/HAL/IHalLog.hpp) | Log levels, error handling |
| Timer | [IHalTimer.hpp](include/HAL/IHalTimer.hpp) | System timing, delays |
| GPIO | [IHalGpio.hpp](include/HAL/IHalGpio.hpp) | Digital I/O, buttons |
| UART | [IHalUart.hpp](include/HAL/IHalUart.hpp) | Serial communication |
| I2C | [IHalI2c.hpp](include/HAL/IHalI2c.hpp) | I2C bus operations |
| SPI | [IHalSpi.hpp](include/HAL/IHalSpi.hpp) | SPI bus operations |
| I2S | [IHalI2s.hpp](include/HAL/IHalI2s.hpp) | Audio I2S interface |
| IMU | [IHalImu.hpp](include/HAL/IHalImu.hpp) | Accelerometer, gyroscope |
| Environmental | [IHalEnvironmental.hpp](include/HAL/IHalEnvironmental.hpp) | Temperature, humidity |
| GPS | [IHalGps.hpp](include/HAL/IHalGps.hpp) | Position, velocity, time |
| Microphone | [IHalMicrophone.hpp](include/HAL/IHalMicrophone.hpp) | Audio input |
| LED Strip | [IHalLedStrip.hpp](include/HAL/IHalLedStrip.hpp) | Addressable LEDs |
| Display | [IHalDisplay.hpp](include/HAL/IHalDisplay.hpp) | HUB75, OLED displays |
| Storage | [IHalStorage.hpp](include/HAL/IHalStorage.hpp) | SD card, file ops |

#### ESP32 Implementations (`src/HAL/ESP32/`)

| Implementation | File | Description |
|----------------|------|-------------|
| Factory | `Esp32HalFactory` | Creates all HAL instances |
| Logging | `Esp32HalLog` | Serial logging |
| Timer | `Esp32HalTimer` | millis/micros/delay |
| GPIO | `Esp32HalGpio` | GPIO, PWM, Button |
| UART | `Esp32HalUart` | HardwareSerial |
| I2C | `Esp32HalI2c` | Wire library |
| Sensors | `Esp32HalSensors` | ICM20948, BME280 |
| GPS | `Esp32HalGps` | NEO-8M NMEA |
| Microphone | `Esp32HalMicrophone` | INMP441 I2S |
| LED Strip | `Esp32HalLedStrip` | SK6812/WS2812 |
| Display | `Esp32HalDisplay` | HUB75, OLED |
| Storage | `Esp32HalStorage` | SD card |

---

### 4. GPU Driver Layer

Graphics rendering and display management for the GPU processor.

| File | Location | Description |
|------|----------|-------------|
| `GpuDriver.hpp` | [include/GpuDriver/](include/GpuDriver/) | Main GPU interface |
| `GpuRenderer.hpp` | [include/GpuDriver/](include/GpuDriver/) | Polygon/primitive rendering |
| `GpuCommands.hpp` | [include/GpuDriver/](include/GpuDriver/) | Command buffer system |
| `GpuAnimationSystem.hpp` | [include/GpuDriver/](include/GpuDriver/) | Animation playback |
| `GpuSDF.hpp` | [include/GpuDriver/](include/GpuDriver/) | Signed distance field rendering |
| `GpuAntialiasing.hpp` | [include/GpuDriver/](include/GpuDriver/) | Edge smoothing |
| `GpuCompositor.hpp` | [include/GpuDriver/](include/GpuDriver/) | Layer compositing |

---

## Hardware Reference

### CPU Hardware (COM 15)

| Peripheral | Interface | Pins | Notes |
|------------|-----------|------|-------|
| ICM20948 IMU | I2C (0x68) | SDA:9, SCL:10 | Accel + Gyro + Mag |
| BME280 | I2C (0x76) | SDA:9, SCL:10 | Temp + Humidity + Pressure |
| NEO-8M GPS | UART | TX:43, RX:44 | Position + Time |
| INMP441 Mic | I2S | WS:42, BCK:40, SD:2 | Audio input |
| LED Left Fin | GPIO 18 | 13 LEDs | SK6812 |
| LED Right Fin | GPIO 38 | 13 LEDs | SK6812 |
| LED Tongue | GPIO 8 | 9 LEDs | SK6812 |
| LED Scale | GPIO 37 | 14 LEDs | SK6812 |
| Buttons A-D | GPIO | 5, 6, 7, 15 | User input |
| Fans | PWM | 17, 36 | Cooling |
| SD Card | SPI | MISO:14, MOSI:47, CLK:21, CS:48 | Storage |
| GPU UART | UART | TX:12, RX:11 | CPU-GPU link |

### GPU Hardware (COM 16)

| Peripheral | Interface | Notes |
|------------|-----------|-------|
| HUB75 Matrix | Parallel GPIO | 64x32 or 64x64 LED matrix |
| OLED SH1107 | I2C | Status display |
| CPU UART | UART | CPU-GPU link |

---

## Configuration JSON Format

The eye system uses human-readable JSON for configuration:

```json
{
  "version": 1,
  "display": {
    "brightness": 80,
    "mirrorMode": true,
    "width": 64,
    "height": 32
  },
  "shader": {
    "type": "rainbow",
    "speed": 50,
    "saturation": 100,
    "brightness": 80,
    "color1": [255, 255, 255],
    "color2": [0, 0, 255]
  },
  "shapes": [
    {
      "name": "Default Eye",
      "points": [[6,8],[14,8],[20,11],[26,17],[27,19],[28,22],
                 [23,22],[21,19],[19,17],[17,17],[16,19],[18,22],
                 [7,22],[4,20],[2,17],[2,12]],
      "offset": [0, 0],
      "scale": 1.0
    }
  ],
  "eyeSets": [
    {
      "name": "Normal",
      "left": {"open": 0, "closed": 1, "half": 2},
      "right": {"open": 0, "closed": 1, "half": 2}
    }
  ],
  "animations": [
    {"name": "Blink", "type": "blink", "duration": 150}
  ],
  "activeSet": 0,
  "lookX": 0,
  "lookY": 0
}
```

---

## Web Interface

The captive portal provides a complete eye management system:

### Eyes Tab
- Live preview with left/right eye canvases
- Look X/Y position sliders
- Manual blink button
- Eye set selector
- Create/edit/delete eye sets

### Sprites Tab
- Upload custom sprites (auto-scaled to 64x32)
- Sprite library management
- Rename/delete sprites

### Animate Tab
- Animation list (Blink, Look, Squint, Wink)
- Create custom animations
- Transition rules (Timer, Accelerometer, Sound, Button)
- Animation settings (blink timing, transition speed)

### Settings Tab
- Brightness control
- Mirror mode toggle
- **WiFi Connection** - Connect to home network for laptop control
- Device info, Reboot, Factory Reset

---

## WiFi Modes

### AP Mode (Default)
- SSID: `Lucidus_XXXX` (random suffix)
- Password: 12-char random (shown on OLED)
- IP: `10.0.0.1`
- Use: Initial setup, portable operation

### STA Mode (Home Network)
- Connect to your home WiFi via Settings tab
- Device gets IP from router (e.g., 192.168.1.xxx)
- Control from any device on network
- AP mode remains active for fallback

---

## Build Environments

| Environment | Board | Description |
|-------------|-------|-------------|
| `CPU_Main` | esp32s3usbotg | Main application |
| `CPU_PolygonDemo` | esp32s3usbotg | Polygon rendering test |
| `GPU_Programmable` | esp32s3box | GPU with script support |
| `GPU_Command_Renderer` | esp32s3box | Command-based rendering |

---

## Memory Usage (CPU_Main)

```
Flash: [========  ]  78.8% (826KB / 1MB)
RAM:   [=         ]  14.1% (46KB / 320KB)
```

---

## Development Notes

### Adding New Eye Shapes
1. Define polygon points (x, y coordinates)
2. Add to `shapes` array in config
3. Reference by index in eye sets

### Creating Animations
1. Choose animation type (blink, look, squint, wink, custom)
2. Set duration in milliseconds
3. Configure easing (linear, ease-in, ease-out)

### Shader Customization
- **Solid**: Single color fill
- **Rainbow**: HSV cycling with position + time
- **Gradient**: Two-color vertical blend
- **Pulse**: Brightness oscillation

---

## File Structure

```
Alpha_Release_Stack/
├── include/
│   ├── HAL/                    # Hardware abstraction interfaces
│   ├── SystemAPI/
│   │   ├── Config/             # EyeConfig, ConfigStorage
│   │   ├── Network/            # WiFiDriver, CaptivePortalServer
│   │   └── Security/           # SecurityDriver (TRNG)
│   ├── GpuDriver/              # GPU rendering system
│   ├── Comms/                  # CPU-GPU communication
│   ├── BaseAPI/                # Base system types
│   └── FrameworkAPI/           # High-level framework
├── src/
│   ├── Application/            # Managers and controllers
│   ├── Modes/                  # Boot and runtime modes
│   ├── HAL/ESP32/              # ESP32 HAL implementations
│   └── CPU_Main.cpp            # Entry point
├── platformio.ini              # Build configuration
├── API_INDEX.md                # This file
├── PIN_MAPPING_CPU.md          # CPU pin assignments
└── PIN_MAPPING_GPU.md          # GPU pin assignments
```

---

*Lucidus - Animatronic Eye Display System*
*Powered by ARCOS (Autonomous Robotic Control & Operations System)*
*Author: XCR1793 (Feather Forge)*
