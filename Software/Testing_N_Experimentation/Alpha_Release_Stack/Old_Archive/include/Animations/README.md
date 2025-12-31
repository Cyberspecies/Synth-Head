# Animation Library

This folder contains animation functions for HUB75, OLED, and LED displays. All animations use **function caching** rather than pixel caching for memory efficiency.

## Structure

```
Animations/
├── HUB75BootAnimations.hpp    - Boot animations for HUB75 matrix
├── HUB75TestAnimations.hpp    - Runtime test animations for HUB75
├── OLEDBootAnimations.hpp     - Boot animations for OLED display
├── LEDBootAnimations.hpp      - Boot animations for LED strips
└── LEDTestAnimations.hpp      - Runtime test animations for LEDs
```

## Usage Pattern

### 1. Include the animation header
```cpp
#include "Animations/HUB75BootAnimations.hpp"
```

### 2. Register animations with manager
```cpp
HUB75DisplayManager hub75;
hub75.initialize(true);

// Register all boot animations
arcos::animations::hub75::registerBootAnimations(hub75);

// Or register individual animations
hub75.registerAnimation("my_custom_anim", [&hub75](uint32_t time_ms){
  // Animation code here
  hub75.clear({0, 0, 0});
  // ... draw operations ...
  hub75.show();
});
```

### 3. Execute animations
```cpp
// By name
hub75.executeAnimation("boot_spinning_circles", millis());

// By index
hub75.executeAnimation(0, millis());
```

## Animation Function Signatures

### HUB75 & OLED Animations
```cpp
void animationFunc(uint32_t time_ms);
```
- `time_ms`: Animation time in milliseconds (for time-based effects)

### LED Animations
```cpp
void animationFunc(LedDataPayload& led_data, uint32_t time_ms);
```
- `led_data`: Reference to LED data payload (modify directly)
- `time_ms`: Animation time in milliseconds

## Creating Custom Animations

### Example: Custom HUB75 Animation
```cpp
inline void customPulse(arcos::manager::HUB75DisplayManager& manager, uint32_t time_ms){
  manager.clear({0, 0, 0});
  
  // Pulsing brightness
  float brightness = 127.5f + 127.5f * sinf(time_ms / 500.0f);
  RGB color = {static_cast<uint8_t>(brightness), 0, 0};
  
  // Draw expanding circle
  int radius = static_cast<int>(16.0f * (1.0f + sinf(time_ms / 300.0f)));
  manager.fillCircle(64, 16, radius, color);
  
  manager.show();
}

// Register it
hub75.registerAnimation("custom_pulse", [&hub75](uint32_t time_ms){
  customPulse(hub75, time_ms);
});
```

### Example: Custom LED Animation
```cpp
inline void customSparkle(LedDataPayload& led_data, uint32_t time_ms){
  // Clear all
  led_data.setAllColor(RgbwColor(0, 0, 0, 0));
  
  // Random sparkles
  for(int i = 0; i < 10; i++){
    uint16_t led_idx = (time_ms + i * 7) % LED_COUNT_TOTAL;
    uint8_t brightness = 255 - (i * 25);
    led_data.leds[led_idx] = RgbwColor(brightness, brightness, brightness, 0);
  }
}

// Register it
led_manager.registerAnimation("custom_sparkle", customSparkle);
```

## Boot Animations

### HUB75
- **boot_spinning_circles**: Dual counter-rotating patterns (good for showing dual display)
- **boot_progress_bar**: Gradient progress bar (visual feedback)
- **boot_ripple**: Expanding circles (smooth startup effect)

### OLED
- **boot_system_init**: Text-based initialization (shows subsystem status)
- **boot_circle_waves**: Expanding waves (visual appeal)
- **boot_logo**: Animated logo/brand (customizable branding)
- **boot_scan_lines**: Scanning effect (retro/technical look)

### LED
- **boot_rainbow_startup**: Rainbow fill (colorful startup)
- **boot_sequential_activation**: Strip-by-strip activation (hardware verification)
- **boot_pulse_wave**: Pulsing brightness (smooth transition)
- **boot_chase_effect**: Running lights (dynamic movement)
- **boot_color_wipe**: Progressive color fill (clear visual progress)

## Test/Runtime Animations

### HUB75
- **test_rainbow_wave**: Scrolling rainbow (color test)
- **test_plasma**: Plasma effect (complex math visualization)
- **test_bouncing_ball**: Physics simulation (motion test)
- **test_starfield**: Moving stars (depth effect)

### LED
- **test_rainbow**: Full spectrum wave
- **test_breathing**: Synchronized breathing per strip
- **test_wave**: Traveling wave effect
- **test_alternating**: Strip switching patterns
- **test_fire**: Flame simulation
- **test_theater_chase**: Classic marquee effect
- **test_color_fade**: Smooth color transitions

## Performance Notes

- All animations are computed in real-time (no pre-rendering)
- Typical animation execution: < 5ms per frame @ 60fps
- Memory usage: ~200 bytes per registered animation (function pointer + name)
- HUB75 antialiased lines: ~2-3x slower than normal lines (still fast enough)
- OLED antialiased lines: Similar performance (monochrome dithering)

## Tips for Animation Development

1. **Use time_ms for smooth animations**: Base effects on time rather than frame count
2. **Normalize values**: Keep brightness/position calculations in 0-1 range, then scale
3. **Avoid memory allocation**: Use stack variables or pre-allocated buffers
4. **Test on hardware**: Simulator doesn't show timing issues
5. **Profile performance**: Use `ESP_LOGI` with timestamps for debugging
6. **Consider display refresh**: HUB75 updates take ~1-2ms, OLED ~10-15ms

## Contributing

When adding new animations:
1. Follow CODING_STYLE.md conventions
2. Use `inline` keyword for header-only functions
3. Document parameters and behavior
4. Add to appropriate `register*Animations()` function
5. Test on actual hardware before committing
