# UI System Documentation

## Overview
Hierarchical menu system with button management for OLED interface.

## Directory Structure
```
include/UI/
├── ButtonManager.hpp          # Button state tracking with press/hold detection
├── Menu/
│   ├── MenuSystem.hpp         # Core menu hierarchy and navigation
│   └── MenuRenderer.hpp       # Rendering implementation
└── OLED/
    └── DebugPages.hpp         # Sensor data display pages
```

## Button Mapping
- **Button 1 (A / SET)**: Select/Confirm *(used in mode selector)*
- **Button 2 (B / UP)**: Navigate Up / Previous
- **Button 3 (C / DOWN)**: Navigate Down / Next
- **Button 4 (D / MODE)**: Mode selector (hold 2 seconds)

## Menu Hierarchy

### Top Level (Mode Selector)
Access by **holding MODE button (2 seconds)** from any screen:
1. **Debug Mode** - Sensor data pages
2. **Standard Mode** - Gyroscope circular visualization
3. **Screen Saver** - Animated bouncing text

**Mode Selector Controls:**
- **UP/DOWN**: Navigate between modes
- **SET**: Confirm selection and enter chosen mode
- **MODE (short press)**: Cancel and return to current mode

### Debug Mode Pages
Navigate with UP/DOWN buttons:
1. **IMU Data** - Accelerometer, gyroscope, magnetometer
2. **Environmental** - Temperature, humidity, pressure
3. **GPS Data** - Position, navigation, time
4. **Microphone** - Audio level, peak, bar graph
5. **System Info** - Frame rates, sensors, buttons

### Standard Mode
Gyroscope visualization with circular indicator:
- Center crosshair shows zero rotation
- Moving dot shows current gyro X/Y rotation rate (±500 dps)
- Line from center to dot shows direction of rotation
- Numeric values (X/Y/Z) displayed at bottom in dps
- Live pulse indicator shows active updates

## ButtonManager Features

### Detection Types
- **Short Press**: Released before 2 second threshold
- **Hold**: Triggered after 2 seconds
- **Hold Repeat**: Continuous events every 150ms while held
- **Debounce**: 50ms minimum

### API
```cpp
ButtonManager btnMgr;
btnMgr.update(sensor_data, current_time_ms);

if (btnMgr.wasPressed(ButtonID::UP)) { }      // Short press
if (btnMgr.wasHeld(ButtonID::MODE)) { }       // Hold started
if (btnMgr.isHeld(ButtonID::MODE)) { }        // Currently held
if (btnMgr.shouldRepeat(ButtonID::DOWN)) { }  // Repeat event

btnMgr.clearFlags();  // Call after processing
```

## MenuSystem Usage

```cpp
#include "UI/ButtonManager.hpp"
#include "UI/Menu/MenuSystem.hpp"
#include "UI/Menu/MenuRenderer.hpp"

// In your task:
ButtonManager btn_mgr;
MenuSystem menu;

// Update loop:
btn_mgr.update(sensor_data, current_time_ms);
menu.update(btn_mgr, sensor_data, current_time_ms);
menu.render(oled_manager);
```

## Integration with GPU.cpp

Replace existing OLED page code with menu system:
1. Include new headers
2. Create ButtonManager and MenuSystem instances
3. Update `oledUpdateTask()` to use menu system
4. Remove old `display*Page()` functions
5. Export stats as globals for system info page

## Global Variables Needed
```cpp
namespace arcos::ui::menu {
  uint32_t g_sensor_fps = 0;
  uint32_t g_led_fps = 0;
  uint8_t g_fan_speed = 0;
}
```

## Benefits
- ✅ **Organized**: UI code separated into logical modules
- ✅ **Hierarchical**: Clean menu tree structure
- ✅ **Extensible**: Easy to add new modes and pages
- ✅ **Robust**: Proper button debouncing and hold detection
- ✅ **Smaller GPU.cpp**: ~235 lines removed from main file

## File Sizes
- ButtonManager.hpp: ~180 lines
- MenuSystem.hpp: ~180 lines
- MenuRenderer.hpp: ~130 lines
- DebugPages.hpp: ~180 lines
- **Total**: ~670 lines (organized vs monolithic)

## Next Steps
To integrate into GPU.cpp:
1. Add includes for new UI system
2. Replace page navigation with menu system
3. Remove old display functions
4. Test button behavior and menu navigation
