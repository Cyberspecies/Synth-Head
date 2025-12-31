# Quick Start Guide: Parametric LED System

## 🚀 Get Your LEDs Running in 5 Minutes!

### Step 1: Update WiFi Credentials

#### CPU (Line 20-21 in `src/parametric_led_cpu_receiver.cpp`):
```cpp
const char* WIFI_SSID = "YourNetworkName";
const char* WIFI_PASSWORD = "YourPassword";
```

#### GPU (Line 51-52 in `src/parametric_led_gpu_sender.cpp`):
```cpp
#define WIFI_SSID "YourNetworkName"
#define WIFI_PASSWORD "YourPassword"
```

---

### Step 2: Upload CPU Code

```powershell
# Default environment is CPU (parametric)
pio run --target upload --upload-port COM15

# Monitor to get IP address
pio device monitor --port COM15 --baud 115200
```

**Look for this in the serial monitor:**
```
✓ WiFi Connected!
CPU IP: 192.168.1.105    <-- COPY THIS IP ADDRESS
```

Press `Ctrl+C` to exit monitor.

---

### Step 3: Update GPU Target IP

Edit `src/parametric_led_gpu_sender.cpp` (line 55):
```cpp
#define CPU_IP_ADDR "192.168.1.105"  // Paste CPU IP here
```

---

### Step 4: Upload GPU Code

```powershell
# Upload GPU code
pio run -e GPU --target upload --upload-port COM16

# Monitor GPU output
pio device monitor --port COM16 --baud 115200
```

**GPU should show:**
```
WiFi connected! IP: 192.168.1.106
UDP sockets initialized. Target: 192.168.1.105:8888
Animation task started
===== STATS =====
Params sent: 15 total (3.0/sec)
Animation: Type=2 P1=45.2 P2=0.60 P3=1.00
```

---

### Step 5: Verify LEDs! 🎉

Your LEDs should now display a smooth **60 FPS rainbow animation**!

---

## 📊 What You Should See

### CPU Serial Output (Every 1 second):
```
=====================================
PARAM UPDATE FPS: 3.1 | Generated FPS: 59.8
Received: 15 | Skipped: 0 (0.0%) | Corrupted: 0
WiFi RSSI: -42 dBm
=====================================
```

**Key Metrics:**
- **PARAM UPDATE FPS**: 0.5-10 Hz (parameters sent only when changed)
- **Generated FPS**: 59-60 Hz (CPU generates frames locally)
- **Skipped**: Should be 0 (no packet loss)
- **RSSI**: > -70 dBm (good signal)

### GPU Serial Output (Every 5 seconds):
```
===== STATS =====
Params sent: 46 total (3.1/sec)
Animation: Type=2 P1=127.3 P2=0.60 P3=1.00
Buttons: A=0 B=0 C=0 D=0
================
```

---

## 🐛 Troubleshooting

### ❌ CPU Won't Connect to WiFi
```
✗ WiFi Connection Failed!
Check your SSID and password
```

**Solutions:**
1. Double-check SSID and password (case-sensitive!)
2. Make sure WiFi is 2.4 GHz (ESP32 doesn't support 5 GHz)
3. Move closer to router
4. Restart router if necessary

### ❌ CPU Gets IP But No LEDs Update
```
CPU IP: 192.168.1.105
PARAM UPDATE FPS: 0.0 | Generated FPS: 59.8
Received: 0
```

**Solutions:**
1. Check GPU has correct CPU IP address in code
2. Verify GPU is connected to WiFi (check GPU serial monitor)
3. Check router firewall (allow UDP port 8888)
4. Ping CPU from GPU to test connectivity

### ❌ LEDs Stuttering
```
Generated FPS: 45.2   <-- Should be 59-60!
```

**Solutions:**
1. CPU is overloaded - check for long delays in code
2. Add timing debug: `Serial.println(millis() - last_time);`
3. Reduce serial print frequency (already 1 Hz)

### ❌ Parameters Not Received
```
PARAM UPDATE FPS: 0.0
Received: 0 | Corrupted: 5
```

**Solutions:**
1. Check magic number validation (0xAA55)
2. Check CRC corruption - WiFi interference
3. Try static IP instead of DHCP
4. Move devices closer to router (improve RSSI)

---

## 🎨 Changing Animation

### Quick Test: Change to Solid Color

In `src/parametric_led_gpu_sender.cpp`, find the animation task (line ~230):

```cpp
// Change from RAINBOW (2) to SOLID (1)
current_params.animation_type = ANIM_SOLID;  // Change this line
current_params.param1 = 240.0f;  // Hue: Blue
current_params.param2 = 1.0f;    // Saturation: Full
current_params.param3 = 1.0f;    // Brightness: Full
```

Re-upload GPU code, LEDs should now be solid blue!

### Change to Breathing Effect

```cpp
current_params.animation_type = ANIM_BREATHING;
current_params.param1 = 0.0f;    // Hue: Red
current_params.param2 = 1.0f;    // Breath rate: 1 cycle/sec
current_params.param3 = 0.2f;    // Min brightness: 20%
```

### Available Animations

| Type | Value | param1 | param2 | param3 |
|------|-------|--------|--------|--------|
| OFF | 0 | - | - | - |
| SOLID | 1 | hue (0-360) | saturation (0-1) | brightness (0-1) |
| RAINBOW | 2 | hue offset (0-360) | speed (deg/frame) | brightness (0-1) |
| GRADIENT | 3 | start hue | end hue | brightness |
| WAVE | 4 | position (0-1) | speed | width (0-1) |
| BREATHING | 5 | hue | rate (Hz) | min brightness |

---

## 📈 Expected Performance

### Bandwidth Usage
- **Old system**: 11,760 bytes/sec (196 bytes × 60 FPS)
- **New system**: ~48 bytes/sec (16 bytes × 3 updates/sec)
- **Reduction**: **245x less bandwidth!** (99.6% savings)

### Frame Rate
- **Old UART**: 6-12 FPS (stuttering, packet loss)
- **New Parametric**: 60 FPS (smooth, CPU-generated)

### Network Load
- **WiFi Raw**: 94 Kbps continuous
- **WiFi Parametric**: 0.4 Kbps average (burst up to 1.3 Kbps)

---

## 🔄 Switching Between Implementations

### Use Parametric (Default, Recommended):
```powershell
pio run -e CPU --target upload --upload-port COM15
pio run -e GPU --target upload --upload-port COM16
```

### Use WiFi Raw Pixels (High Bandwidth):
```powershell
pio run -e CPU_WiFi_Raw --target upload --upload-port COM15
pio run -e GPU_WiFi_Raw --target upload --upload-port COM16
```

### Use UART (Legacy, Has Issues):
```powershell
pio run -e CPU_UART --target upload --upload-port COM15
pio run -e GPU_UART --target upload --upload-port COM16
```

---

## 📚 Further Reading

- **Full Documentation**: [`PARAMETRIC_LED_SYSTEM.md`](PARAMETRIC_LED_SYSTEM.md)
- **Comparison Guide**: [`IMPLEMENTATION_COMPARISON.md`](IMPLEMENTATION_COMPARISON.md)
- **Adding Animations**: See "Adding New Animations" in `PARAMETRIC_LED_SYSTEM.md`

---

## ✅ Success Checklist

- [ ] WiFi credentials updated in both CPU and GPU
- [ ] CPU uploaded and shows WiFi IP
- [ ] GPU updated with CPU IP address
- [ ] GPU uploaded successfully
- [ ] LEDs show smooth rainbow animation
- [ ] CPU shows "Generated FPS: 59-60"
- [ ] GPU shows "Params sent: X total (3.0/sec)"
- [ ] Button presses detected (check GPU serial)

---

## 🎉 You're Done!

Your LEDs should now be running with:
- ✅ Smooth 60 FPS animation
- ✅ 99% less bandwidth usage
- ✅ No stuttering or frame drops
- ✅ Scalable to hundreds of LEDs

Enjoy your optimized LED system! 🌈
