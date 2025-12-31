# LED Rainbow Test Suite

This directory contains LED test code for the ARCOS project, implementing a smooth rainbow hue cycle effect across all connected **WRGB LED strips** (LEDs with Red, Green, Blue, and White channels).

## Hardware Configuration

Based on `PIN_MAPPING_CPU.md`, the following **WRGB LED strips** are configured:

- **Left Fin (GPIO 18):** 13 WRGB LEDs in series
- **Tongue (GPIO 5):** 9 WRGB LEDs in series  
- **Right Fin (GPIO 38):** 13 WRGB LEDs in series
- **Scale LEDs (GPIO 37):** 14 WRGB LEDs in a row

**Note:** LED Strip 0 (GPIO 16) and LED Strip 3 (GPIO 39) are not used in the current hardware configuration.

## Files Overview

### Core Files
- `include/LedController.h` - LED controller class header
- `include/LedController.impl.hpp` - LED controller implementation
- `src/main.cpp` - Basic rainbow cycle demo
- `src/rainbow_test.cpp` - Focused rainbow hue cycle test
- `src/led_test_advanced.cpp` - Multiple effect demo suite

## Quick Start

### Option 1: Basic Rainbow Test
To run the simple rainbow hue cycle effect:

1. Rename `src/main.cpp` to `src/main.cpp.bak`
2. Rename `src/rainbow_test.cpp` to `src/main.cpp`
3. Build and upload with PlatformIO

### Option 2: Advanced Test Suite
To run multiple LED effects (rainbow, chase, breathing, solid colors):

1. Rename `src/main.cpp` to `src/main.cpp.bak`
2. Rename `src/led_test_advanced.cpp` to `src/main.cpp`
3. Build and upload with PlatformIO

## Building and Uploading

```bash
# Build the project
pio run

# Upload to ESP32S3
pio run --target upload

# Monitor serial output
pio device monitor
```

## Effect Parameters

### Rainbow Hue Cycle
- **Speed:** Adjustable via `setRainbowSpeed(float speed)`
- **Update Rate:** Configurable via `setUpdateInterval(unsigned long ms)`
- **Brightness:** Fixed at 80% for optimal visibility

### Advanced Effects
The advanced test suite cycles through:
1. **Rainbow Cycle** (10s) - Smooth hue transition
2. **Chase Effect** (10s) - Orange light chasing across strips
3. **Breathing Effect** (10s) - Blue pulsing/breathing
4. **Solid Colors** (10s) - Cycling through 8 solid colors

## Customization

### Changing Rainbow Speed
```cpp
led_controller.setRainbowSpeed(2.0f);  // Faster
led_controller.setRainbowSpeed(0.5f);  // Slower
```

### Changing Update Rate
```cpp
led_controller.setUpdateInterval(30);  // ~33 FPS (faster)
led_controller.setUpdateInterval(100); // 10 FPS (slower)
```

### Individual Strip Control
```cpp
led_controller.setLeftFinColor(0xFF0000);    // Red left fin
led_controller.setTongueColor(0x00FF00);     // Green tongue
led_controller.setRightFinColor(0x0000FF);   // Blue right fin
led_controller.setScaleColor(0xFFFF00);      // Yellow scale LEDs
```

## Troubleshooting

### No LEDs Lighting Up
1. Check power supply connections
2. Verify GPIO pin connections match `PIN_MAPPING_CPU.md`
3. Ensure LED strips are WS2812B/NeoPixel compatible
4. Check serial monitor for initialization errors

### Colors Appear Wrong
- Verify WRGB LED strip type and color ordering
- Check if strips require different WRGB channel ordering (some use RGBW, others WRGB)
- Try different NeoPixel color formats: NEO_WRGB, NEO_RGBW, NEO_GRBW

### Performance Issues
- Reduce update rate: `setUpdateInterval(100)` or higher
- Lower rainbow speed: `setRainbowSpeed(0.5f)` or lower

## Code Style

This code follows the ARCOS coding style guidelines:
- 2-space indentation
- Tight brace style (`{` on same line)
- snake_case for variables
- camelCase for methods
- ALL_CAPS for constants

## Dependencies

- Adafruit NeoPixel Library (already configured in `platformio.ini`)
- ESP32 Arduino Framework
- C++17 standard library

## Expected Behavior

When working correctly, you should see:
1. All LED strips light up simultaneously
2. Smooth color transitions cycling through the full rainbow spectrum
3. Each LED in a strip shows a slightly different hue, creating a rainbow gradient
4. The rainbow pattern continuously rotates/cycles across all strips

The effect creates a beautiful, smooth rainbow that flows across all your LED strips!