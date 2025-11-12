# Display and LED Managers

This folder contains manager classes for HUB75, OLED displays, and LED animations. Managers provide a high-level interface with animation function caching.

## Architecture

```
Manager/
├── HUB75DisplayManager.hpp    - HUB75 LED matrix manager
├── OLEDDisplayManager.hpp     - OLED display manager
└── LEDAnimationManager.hpp    - LED strip animation manager
```

## Design Philosophy

### Function Caching vs Pixel Caching
- **Function Caching**: Store animation functions, compute pixels on-demand
- **Pixel Caching**: Pre-render frames, store pixel data in memory

We use **function caching** because:
- Lower memory footprint (store function pointers, not pixel arrays)
- More flexible (animations can react to parameters)
- Easier to modify (change function, not data)
- Suitable for real-time rendering on ESP32-S3

## Manager Classes

### HUB75DisplayManager

Manages the dual HUB75 LED matrix (128x32 pixels, RGB).

#### Features
- Display initialization with dual OE pin support
- Line drawing (Bresenham and Xiaolin Wu antialiased)
- Shape primitives (circles, rectangles, filled variants)
- Alpha blending for antialiasing
- Animation registration and execution
- Direct pixel access

#### Example Usage
```cpp
#include "Manager/HUB75DisplayManager.hpp"

HUB75DisplayManager hub75;

void setup(){
  // Initialize display
  hub75.initialize(true);  // true = dual OE mode
  
  // Register animation
  hub75.registerAnimation("test", [&hub75](uint32_t time_ms){
    hub75.clear({0, 0, 0});
    
    // Draw antialiased line
    float angle = time_ms / 1000.0f;
    float x1 = 64.0f + 50.0f * cosf(angle);
    float y1 = 16.0f + 12.0f * sinf(angle);
    hub75.drawLineAntialiased(64.0f, 16.0f, x1, y1, {255, 0, 0});
    
    hub75.show();
  });
  
  // Execute animation
  hub75.executeAnimation("test", millis());
}
```

#### API Reference

**Initialization**
```cpp
bool initialize(bool dual_oe_mode = true);
```

**Drawing Functions**
```cpp
void clear(const RGB& color = {0, 0, 0});
void show();
void setPixel(int x, int y, const RGB& color);
void setPixelAlpha(int x, int y, const RGB& color, float alpha);
void drawLine(int x0, int y0, int x1, int y1, const RGB& color);
void drawLineAntialiased(float x0, float y0, float x1, float y1, const RGB& color);
void drawRect(int x, int y, int w, int h, const RGB& color);
void fillRect(int x, int y, int w, int h, const RGB& color);
void drawCircle(int cx, int cy, int radius, const RGB& color);
void fillCircle(int cx, int cy, int radius, const RGB& color);
```

**Animation Management**
```cpp
void registerAnimation(const char* name, HUB75AnimationFunc func);
void executeAnimation(size_t index, uint32_t time_ms);
bool executeAnimation(const char* name, uint32_t time_ms);
size_t getAnimationCount() const;
const char* getAnimationName(size_t index) const;
void clearAnimations();
```

---

### OLEDDisplayManager

Manages the OLED SH1107 display (128x128 pixels, monochrome).

#### Features
- I2C initialization with configurable pins
- Line drawing (normal and antialiased with dithering)
- Text rendering
- Shape primitives
- Animation registration and execution
- Display flip/rotation

#### Example Usage
```cpp
#include "Manager/OLEDDisplayManager.hpp"

OLEDDisplayManager oled;

void setup(){
  // Initialize display
  // i2c_bus, sda_pin, scl_pin, freq_hz, flip_h, flip_v, contrast
  oled.initialize(0, 2, 1, 400000, true, true, 0xCF);
  
  // Register animation
  oled.registerAnimation("clock", [&oled](uint32_t time_ms){
    oled.clear();
    
    // Draw clock face
    oled.drawCircle(64, 64, 50, true);
    
    // Draw hour hand
    float hour_angle = (time_ms / 1000.0f / 3600.0f) * 6.28318f;
    int hour_x = 64 + static_cast<int>(30.0f * sinf(hour_angle));
    int hour_y = 64 - static_cast<int>(30.0f * cosf(hour_angle));
    oled.drawLine(64, 64, hour_x, hour_y, true);
    
    oled.show();
  });
  
  // Execute animation
  oled.executeAnimation("clock", millis());
}
```

#### API Reference

**Initialization**
```cpp
bool initialize(int i2c_bus = 0, int sda_pin = 2, int scl_pin = 1, 
                uint32_t freq_hz = 400000, bool flip_horizontal = true,
                bool flip_vertical = true, uint8_t contrast = 0xCF);
```

**Drawing Functions**
```cpp
void clear();
void show();
void setPixel(int x, int y, bool on = true);
void drawText(int x, int y, const char* text, bool on = true);
void drawLine(int x0, int y0, int x1, int y1, bool on = true);
void drawLineAntialiased(float x0, float y0, float x1, float y1);
void drawRect(int x, int y, int w, int h, bool fill = false, bool on = true);
void fillRect(int x, int y, int w, int h, bool on = true);
void drawCircle(int cx, int cy, int radius, bool on = true);
void fillCircle(int cx, int cy, int radius, bool on = true);
```

**Animation Management**
```cpp
void registerAnimation(const char* name, OLEDAnimationFunc func);
void executeAnimation(size_t index, uint32_t time_ms);
bool executeAnimation(const char* name, uint32_t time_ms);
size_t getAnimationCount() const;
const char* getAnimationName(size_t index) const;
void clearAnimations();
```

---

### LEDAnimationManager

Manages NeoPixel RGBW LED strips (4 strips: left fin, tongue, right fin, scale).

#### Features
- Animation registration and execution
- Per-strip color control
- Fan speed management
- Animation cycling (next/previous)
- Current animation tracking
- Direct access to LED data payload for UART transmission

#### Example Usage
```cpp
#include "Manager/LEDAnimationManager.hpp"

LEDAnimationManager leds;

void setup(){
  // Initialize LED manager
  leds.initialize();
  leds.setFanSpeed(128);  // 50% fan speed
  
  // Register animation
  leds.registerAnimation("rainbow", [](LedDataPayload& data, uint32_t time_ms){
    float time_sec = time_ms / 1000.0f;
    
    for(uint16_t i = 0; i < LED_COUNT_TOTAL; i++){
      float hue = fmodf((i / (float)LED_COUNT_TOTAL) + time_sec * 0.1f, 1.0f);
      // Convert hue to RGB...
      data.leds[i] = RgbwColor(r, g, b, 0);
    }
  });
  
  // Set and update animation
  leds.setCurrentAnimation("rainbow");
  leds.updateCurrentAnimation(millis());
  
  // Get LED data for UART transmission
  LedDataPayload& data = leds.getLedData();
  uart.sendPacket(MessageType::LED_DATA, 
                  reinterpret_cast<uint8_t*>(&data), 
                  sizeof(LedDataPayload));
}
```

#### API Reference

**Initialization**
```cpp
void initialize();
```

**LED Control**
```cpp
LedDataPayload& getLedData();
void setFanSpeed(uint8_t speed);
uint8_t getFanSpeed() const;
void setAllColor(const RgbwColor& color);
void setLeftFinColor(const RgbwColor& color);
void setTongueColor(const RgbwColor& color);
void setRightFinColor(const RgbwColor& color);
void setScaleColor(const RgbwColor& color);
```

**Animation Management**
```cpp
void registerAnimation(const char* name, LEDAnimationFunc func);
void setCurrentAnimation(size_t index, bool reset_time = true);
bool setCurrentAnimation(const char* name, bool reset_time = true);
void updateCurrentAnimation(uint32_t time_ms);
void executeAnimation(size_t index, uint32_t time_ms);
bool executeAnimation(const char* name, uint32_t time_ms);
void nextAnimation();
void previousAnimation();
size_t getAnimationCount() const;
size_t getCurrentAnimationIndex() const;
const char* getAnimationName(size_t index) const;
void clearAnimations();
```

## Antialiasing

### HUB75 (RGB Color Display)
Uses **Xiaolin Wu's line algorithm** with alpha blending:
```cpp
hub75.drawLineAntialiased(0.5f, 0.5f, 127.5f, 31.5f, {255, 0, 0});
```
- Sub-pixel precision (float coordinates)
- Alpha blending based on pixel coverage
- Smooth, high-quality lines

### OLED (Monochrome Display)
Uses **dithering** to simulate antialiasing:
```cpp
oled.drawLineAntialiased(0.5f, 0.5f, 127.5f, 127.5f);
```
- Sub-pixel calculations
- Threshold-based pixel activation
- Best-effort smoothing for 1-bit display

## Thread Safety

### Shared Data Protection
When using managers across multiple tasks:

```cpp
// Create mutex
SemaphoreHandle_t display_mutex = xSemaphoreCreateMutex();

// Task 1: Update animation
void animationTask(void* param){
  while(true){
    if(xSemaphoreTake(display_mutex, portMAX_DELAY) == pdTRUE){
      hub75.executeAnimation("plasma", millis());
      xSemaphoreGive(display_mutex);
    }
    vTaskDelay(pdMS_TO_TICKS(16));
  }
}

// Task 2: Change animation
void controlTask(void* param){
  while(true){
    if(button_pressed){
      if(xSemaphoreTake(display_mutex, portMAX_DELAY) == pdTRUE){
        hub75.clearAnimations();
        hub75.registerAnimation("new_anim", newAnimFunc);
        xSemaphoreGive(display_mutex);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}
```

## Performance Considerations

### Frame Rate
- **Target**: 60 FPS (16.67ms per frame)
- **HUB75**: ~1-2ms update time
- **OLED**: ~10-15ms update time (I2C bottleneck)
- **LED Data**: <1ms preparation, UART transmission separate

### Optimization Tips
1. **Minimize `show()` calls**: Only call after all drawing complete
2. **Batch operations**: Group similar drawing operations
3. **Use integer math**: Faster than floating point where possible
4. **Profile animations**: Use ESP_LOGI with timestamps
5. **Avoid memory allocation**: Use stack or pre-allocated buffers

### Memory Usage
- **Manager overhead**: ~100-200 bytes per manager
- **Animation storage**: ~16 bytes per registered animation
- **Display buffers**: Managed by underlying drivers
- **No pixel caching**: Animations computed on-demand

## Best Practices

1. **RAII Pattern**: Managers handle initialization and cleanup
2. **Lambda Captures**: Capture manager by reference in lambdas
3. **Time-based Animation**: Use `time_ms` for smooth, frame-rate-independent effects
4. **Error Checking**: Always check `initialize()` return value
5. **Const Correctness**: Use const methods where appropriate

## Troubleshooting

### Display Not Initializing
```cpp
if(!hub75.initialize(true)){
  ESP_LOGE("APP", "HUB75 init failed!");
  // Check wiring, power supply, GPIO conflicts
}
```

### Animation Not Showing
```cpp
// Make sure to call show()
hub75.executeAnimation("test", millis());
hub75.show();  // <- Don't forget this!
```

### Flickering Display
```cpp
// Ensure single task updates display
// Use mutex if multiple tasks access display
```

### Slow Frame Rate
```cpp
// Profile animation execution time
uint32_t start = micros();
hub75.executeAnimation("plasma", millis());
uint32_t duration = micros() - start;
ESP_LOGI("PERF", "Animation took %luµs", duration);
```

## Future Enhancements

Potential improvements:
- [ ] Animation blending/transitions
- [ ] Sprite system for HUB75
- [ ] Font rendering for OLED
- [ ] Animation parameters/configuration
- [ ] Save/load animations from flash
- [ ] Animation scripting language
