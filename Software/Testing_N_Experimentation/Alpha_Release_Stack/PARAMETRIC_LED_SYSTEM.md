# Parametric LED Control System

## 🎯 Concept: Bandwidth Optimization Through Procedural Generation

Instead of sending **196 bytes @ 60 FPS** (11,760 bytes/sec), we send **16 bytes only when parameters change** (~0.3 - 5 bytes/sec average).

### Key Innovation
- **GPU**: Sends animation *parameters* (type, hue, speed, brightness)
- **CPU**: Generates LED frames *locally* at 60 FPS from parameters
- **Result**: 99%+ bandwidth reduction while maintaining 60 FPS smooth animation

---

## 📦 Protocol Structure

### Animation Parameter Packet (16 bytes)
```cpp
struct AnimationParams {
    uint16_t magic;           // 0xAA55 sync marker
    uint8_t animation_type;   // 0-8 (off, solid, rainbow, gradient, wave, breathing, etc.)
    uint8_t frame_counter;    // For skip detection
    float param1;             // Animation-specific (hue, offset, position)
    float param2;             // Animation-specific (speed, saturation, rate)
    float param3;             // Animation-specific (brightness, width, min)
    uint8_t crc8;             // Data integrity check
};
```

### Transmission Strategy
- **Send only when parameters change** (not every frame!)
- **Keepalive every 1 second** (ensure connection alive)
- **CRC validation** (detect corruption)
- **Frame counter** (detect packet loss)

---

## 🎨 Supported Animations

### 1. **ANIM_RAINBOW** (Current Implementation)
- `param1`: Global hue offset (0-360°) - rotates entire rainbow
- `param2`: Hue speed (degrees/frame) - local animation speed on CPU
- `param3`: Brightness (0-1)
- **Bandwidth**: ~3 updates/sec (as hue slowly changes)

### 2. **ANIM_SOLID**
- `param1`: Hue (0-360°)
- `param2`: Saturation (0-1)
- `param3`: Brightness (0-1)
- **Bandwidth**: Only when color changes

### 3. **ANIM_GRADIENT**
- `param1`: Start hue (0-360°)
- `param2`: End hue (0-360°)
- `param3`: Brightness (0-1)
- **Bandwidth**: Only when gradient changes

### 4. **ANIM_WAVE**
- `param1`: Wave position (0-1, wraps)
- `param2`: Wave speed (units/frame)
- `param3`: Wave width (0-1)
- **Bandwidth**: ~60 updates/sec (if wave_position sent), but can be CPU-animated

### 5. **ANIM_BREATHING**
- `param1`: Hue (0-360°)
- `param2`: Breath rate (cycles/second)
- `param3`: Minimum brightness (0-1)
- **Bandwidth**: Only when color/rate changes

---

## 📊 Bandwidth Comparison

### Old System (Raw Pixel Data)
```
196 bytes/frame × 60 FPS = 11,760 bytes/sec
```

### New System (Parametric)
```
RAINBOW animation:
  - 16 bytes × ~3 updates/sec = 48 bytes/sec
  - Reduction: 99.6% less bandwidth!

SOLID color (static):
  - 16 bytes × 1 update = 16 bytes total
  - Then 0 bytes/sec until color changes

WAVE animation:
  - If GPU sends position: 16 bytes × 60 FPS = 960 bytes/sec
  - If CPU animates locally: 16 bytes × 1 update = 16 bytes total
  - Reduction: 92% - 99.9% less bandwidth!
```

---

## 🔧 Implementation Files

### CPU Side (Arduino)
1. **`ParametricLedProtocol.h`** - Protocol definitions
2. **`ParametricAnimator.h`** - Animation generator interface
3. **`ParametricAnimator.impl.hpp`** - Animation algorithms (HSV, rainbow, gradient, etc.)
4. **`parametric_led_cpu_receiver.cpp`** - Main receiver loop

### GPU Side (ESP-IDF)
1. **`ParametricLedProtocol.h`** - Same protocol (shared)
2. **`parametric_led_gpu_sender.cpp`** - Parameter sender

---

## 🚀 How It Works

### CPU Flow (60 FPS)
```cpp
loop() {
    // Receive parameter updates (when GPU sends them)
    if (udp_has_packet) {
        animator.updateParams(received_params);
    }
    
    // Generate LED data at 60 FPS (always!)
    if (time_for_frame) {
        animator.generateFrame(led_data, 49);
        led_controller.update(led_data);
    }
}
```

### GPU Flow (~3 FPS for rainbow, 0 FPS for static)
```cpp
animation_task() {
    // Calculate parameters
    params.type = ANIM_RAINBOW;
    params.hue_offset = current_time * 0.1;  // Slow rotation
    params.hue_speed = 0.6;                  // CPU animation speed
    params.brightness = 1.0;
    
    // Send only if changed significantly
    if (params_changed(params, last_params)) {
        send_params(params);
    }
    
    // Or send keepalive every 1 second
    if (time_since_last_send > 1000ms) {
        send_params(params);
    }
}
```

---

## 🎯 Benefits

### 1. **Massive Bandwidth Reduction**
- 99%+ less data transmitted
- Works reliably even on slow/unstable networks
- Frees bandwidth for other data (audio, sensors, etc.)

### 2. **Perfect Synchronization**
- CPU generates frames at exactly 60 FPS locally
- No network jitter affects animation smoothness
- Frame rate independent of network speed

### 3. **Scalability**
- Can support hundreds of LEDs without bandwidth increase
- Multiple animation layers (background + foreground)
- Complex effects with simple parameters

### 4. **Power Efficiency**
- Less WiFi transmission = less power consumption
- GPU can sleep between parameter updates
- CPU does lightweight math (HSV conversion)

---

## 🔧 Configuration

### 1. Update WiFi Credentials

**CPU** (`parametric_led_cpu_receiver.cpp` line 20-21):
```cpp
const char* WIFI_SSID = "YourNetworkName";
const char* WIFI_PASSWORD = "YourPassword";
```

**GPU** (`parametric_led_gpu_sender.cpp` line 51-52):
```cpp
#define WIFI_SSID "YourNetworkName"
#define WIFI_PASSWORD "YourPassword"
```

### 2. Set CPU IP Address

**GPU** (`parametric_led_gpu_sender.cpp` line 55):
```cpp
#define CPU_IP_ADDR "192.168.1.XXX"  // Check CPU's WiFi IP from serial monitor
```

### 3. Compile and Upload

**CPU**:
```bash
pio run --target upload --upload-port COM15
```

**GPU**:
```bash
pio run --target upload --upload-port COM16
```

---

## 📈 Expected Performance

### Serial Monitor Output (CPU)
```
=====================================
PARAM UPDATE FPS: 3.2 | Generated FPS: 59.8
Received: 16 | Skipped: 0 (0.0%) | Corrupted: 0
WiFi RSSI: -45 dBm
=====================================
```

### Serial Monitor Output (GPU)
```
===== STATS =====
Params sent: 48 total (3.1/sec)
Animation: Type=2 P1=127.3 P2=0.60 P3=1.00
Buttons: A=0 B=0 C=0 D=0
================
```

---

## 🎨 Adding New Animations

### 1. Define Animation Type
Add to `ParametricLedProtocol.h`:
```cpp
enum AnimationType : uint8_t {
    // ...existing...
    ANIM_COMET = 9,    // NEW: Comet tail effect
};
```

### 2. Implement Generator
Add to `ParametricAnimator.impl.hpp`:
```cpp
void ParametricAnimator::generateComet(uint8_t* led_data, uint32_t num_leds) {
    float comet_pos = param1;  // Position (0-1)
    float tail_length = param2; // Tail length (0-1)
    float brightness = param3;  // Brightness (0-1)
    
    // Render comet with tail...
}
```

### 3. Add to Switch Statement
In `ParametricAnimator::generateFrame()`:
```cpp
case ANIM_COMET:
    generateComet(led_data, num_leds);
    break;
```

### 4. Send from GPU
In `parametric_led_gpu_sender.cpp`:
```cpp
current_params.animation_type = ANIM_COMET;
current_params.param1 = comet_position;  // Update each frame or calculate locally
current_params.param2 = 0.3f;            // 30% tail length
current_params.param3 = 1.0f;            // Full brightness
```

---

## 🐛 Troubleshooting

### LEDs Not Updating
1. Check WiFi connection on both devices
2. Verify CPU IP address matches GPU's target
3. Check serial monitors for packet reception
4. Ensure firewall allows UDP ports 8888/8889

### Stuttering Animation
1. Check "Generated FPS" - should be 59-60
2. Verify WiFi signal strength (RSSI > -70 dBm)
3. Move devices closer to router
4. Check CPU isn't overloaded (add timing prints)

### Parameters Not Received
1. Check "PARAM UPDATE FPS" - should be 0.5-10 FPS
2. Verify magic number (0xAA55) in packets
3. Check CRC corruption count
4. Try ping between devices to test connectivity

---

## 💡 Future Enhancements

### 1. **Multi-Zone Control**
Send different parameters for different LED strips:
```cpp
struct MultiZoneParams {
    AnimationParams left_fin;
    AnimationParams right_fin;
    AnimationParams tongue;
    AnimationParams scale;
};
```

### 2. **Reactive Audio**
Add audio parameters:
```cpp
params.param1 = bass_level;    // React to music
params.param2 = beat_detected;
params.param3 = frequency_peak;
```

### 3. **Smooth Transitions**
Interpolate between parameter changes:
```cpp
animator.setTransitionTime(500);  // 500ms smooth fade
animator.updateParams(new_params);
```

### 4. **Preset Library**
Store animation presets:
```cpp
presets[0] = {ANIM_RAINBOW, 0, 0.6, 1.0};     // Fast rainbow
presets[1] = {ANIM_BREATHING, 240, 0.5, 0.3}; // Slow blue breathing
```

---

## 📝 Summary

This parametric system is like **streaming video codecs** but for LEDs:
- H.264 doesn't send every pixel - it sends motion vectors and key frames
- We don't send every LED - we send animation parameters and let CPU reconstruct

**Result**: Smooth 60 FPS animation with 1% of the bandwidth! 🎉
