# GPU Command Protocol Reference

**Version:** 1.0  
**Last Updated:** January 2026  
**Author:** XCR1793 (Feather Forge)

---

## Table of Contents

1. [Overview](#overview)
2. [Hardware Configuration](#hardware-configuration)
3. [Protocol Format](#protocol-format)
4. [Display Coordinate Systems](#display-coordinate-systems)
5. [Command Reference](#command-reference)
6. [Usage Examples](#usage-examples)
7. [Best Practices](#best-practices)

---

## Overview

The GPU is an ESP32-S3 that drives two displays:
- **HUB75**: 128x32 RGB LED matrix (dual 64x32 panels)
- **OLED**: SH1107 128x128 monochrome display

Communication is via UART at **10 Mbps** (10,000,000 baud).

### Key Features
- Vector drawing primitives (lines, rectangles, circles, polygons)
- Sub-pixel precision with fixed-point float commands
- Sprite upload and blitting
- Programmable shader support (bytecode execution)
- Dual-target rendering (HUB75 or OLED)
- Boot animation until CPU connection

---

## Hardware Configuration

### UART Pins
| Signal | CPU GPIO | GPU GPIO | Description |
|--------|----------|----------|-------------|
| TX     | GPIO12   | GPIO13 (RX) | CPU transmits to GPU |
| RX     | GPIO11   | GPIO12 (TX) | GPU transmits to CPU |

### I2C Bus (GPU side - for OLED)
| Signal | GPIO |
|--------|------|
| SDA    | GPIO2 |
| SCL    | GPIO1 |

### Display Connections
- **HUB75**: Standard HUB75 pinout (directly connected)
- **OLED**: I2C address `0x3C`

---

## Protocol Format

All commands use the following packet structure:

```
┌──────────┬──────────┬──────────┬───────────────┬─────────────────┐
│ SYNC0    │ SYNC1    │ CMD_TYPE │ LENGTH (16b)  │ PAYLOAD         │
│ 0xAA     │ 0x55     │ 1 byte   │ 2 bytes (LE)  │ 0-2043 bytes    │
└──────────┴──────────┴──────────┴───────────────┴─────────────────┘
```

- **SYNC0**: Always `0xAA`
- **SYNC1**: Always `0x55`
- **CMD_TYPE**: Command opcode (see table below)
- **LENGTH**: Payload length in bytes (little-endian uint16)
- **PAYLOAD**: Command-specific data

### Example Packet Construction (C++)
```cpp
void sendCommand(CmdType cmd, const uint8_t* payload, uint16_t len) {
    uint8_t header[5] = {
        0xAA,           // SYNC0
        0x55,           // SYNC1
        (uint8_t)cmd,   // Command type
        (uint8_t)(len & 0xFF),      // Length low byte
        (uint8_t)((len >> 8) & 0xFF) // Length high byte
    };
    uart_write_bytes(UART_PORT, header, 5);
    if (len > 0 && payload) {
        uart_write_bytes(UART_PORT, payload, len);
    }
}
```

---

## Display Coordinate Systems

### HUB75 (128x32 RGB)
```
Origin (0,0) ─────────────────────────────────► X (127,0)
      │                                              
      │         ┌────────────────────────────┐      
      │         │                            │      
      │         │     Panel 0     Panel 1    │      
      │         │     (64x32)     (64x32)    │      
      │         │                            │      
      │         └────────────────────────────┘      
      ▼                                              
      Y (0,31)                                        
```
- Width: 128 pixels
- Height: 32 pixels
- Color: RGB888 (3 bytes per pixel: R, G, B)

### OLED (128x128 Monochrome)

**⚠️ IMPORTANT: OLED Orientation Correction**

The OLED display requires transformation:
1. **Flip horizontally** (mirror back-to-front)
2. **Rotate 180 degrees**

The GPU firmware handles this automatically. When drawing to OLED:
```
Physical Screen      →    Your Coordinates
┌─────────────┐           ┌─────────────┐
│             │           │ (0,0)       │
│ (mirrored)  │    →      │     ───►X   │
│             │           │     │       │
└─────────────┘           │     ▼Y      │
                          └─────────────┘
```

- Width: 128 pixels
- Height: 128 pixels
- Color: 1-bit (on/off per pixel)
- Buffer format: Column-major, 8 pixels per byte (LSB = top)

---

## Command Reference

### Command Opcodes

| Opcode | Name | Description |
|--------|------|-------------|
| `0x00` | NOP | No operation |
| **Shaders** |||
| `0x10` | UPLOAD_SHADER | Upload bytecode shader |
| `0x11` | DELETE_SHADER | Remove shader from slot |
| `0x12` | EXEC_SHADER | Execute shader |
| **Sprites** |||
| `0x20` | UPLOAD_SPRITE | Upload sprite data |
| `0x21` | DELETE_SPRITE | Remove sprite from slot |
| **Variables** |||
| `0x30` | SET_VAR | Set single variable |
| `0x31` | SET_VARS | Set multiple variables |
| **Drawing (Integer)** |||
| `0x40` | DRAW_PIXEL | Draw single pixel |
| `0x41` | DRAW_LINE | Draw line |
| `0x42` | DRAW_RECT | Draw rectangle outline |
| `0x43` | DRAW_FILL | Draw filled rectangle |
| `0x44` | DRAW_CIRCLE | Draw circle outline |
| `0x45` | DRAW_POLY | Draw filled polygon |
| `0x46` | BLIT_SPRITE | Blit sprite to buffer |
| `0x47` | CLEAR | Clear buffer with color |
| **Drawing (Float - 8.8 Fixed Point)** |||
| `0x48` | DRAW_LINE_F | Line with sub-pixel precision |
| `0x49` | DRAW_CIRCLE_F | Circle with sub-pixel precision |
| `0x4A` | DRAW_RECT_F | Rectangle with sub-pixel precision |
| **Target Control** |||
| `0x50` | SET_TARGET | Select render target (0=HUB75, 1=OLED) |
| `0x51` | PRESENT | Push buffer to display |
| **OLED-Specific** |||
| `0x60` | OLED_CLEAR | Clear OLED buffer |
| `0x61` | OLED_LINE | Draw line on OLED |
| `0x62` | OLED_RECT | Draw rectangle outline on OLED |
| `0x63` | OLED_FILL | Draw filled rectangle on OLED |
| `0x64` | OLED_CIRCLE | Draw circle on OLED |
| `0x65` | OLED_PRESENT | Push OLED buffer to display |
| **System** |||
| `0xF0` | PING | Connectivity check |
| `0xFF` | RESET | Reset GPU state |

---

### Detailed Command Specifications

#### `0x00` - NOP
No operation. Can be used for timing or padding.

**Payload:** None (length = 0)

---

#### `0x10` - UPLOAD_SHADER
Upload shader bytecode to a slot (max 16 shaders, max 1024 bytes each).

**Payload:**
| Offset | Size | Description |
|--------|------|-------------|
| 0 | 1 | Slot ID (0-15) |
| 1-2 | 2 | Bytecode length (LE) |
| 3+ | N | Bytecode data |

---

#### `0x11` - DELETE_SHADER
Remove shader from slot.

**Payload:**
| Offset | Size | Description |
|--------|------|-------------|
| 0 | 1 | Slot ID (0-15) |

---

#### `0x12` - EXEC_SHADER
Execute shader in slot.

**Payload:**
| Offset | Size | Description |
|--------|------|-------------|
| 0 | 1 | Slot ID (0-15) |

---

#### `0x20` - UPLOAD_SPRITE
Upload sprite data (max 64 sprites).

**Payload:**
| Offset | Size | Description |
|--------|------|-------------|
| 0 | 1 | Sprite ID (0-63) |
| 1 | 1 | Width |
| 2 | 1 | Height |
| 3 | 1 | Format: 0=RGB888, 1=1bpp mono |
| 4+ | N | Pixel data |

**Data Size:**
- RGB888: `width × height × 3` bytes
- 1bpp: `ceil(width/8) × height` bytes

---

#### `0x21` - DELETE_SPRITE
Remove sprite from slot.

**Payload:**
| Offset | Size | Description |
|--------|------|-------------|
| 0 | 1 | Sprite ID (0-63) |

---

#### `0x30` - SET_VAR
Set a single variable (256 variables available, int16).

**Payload:**
| Offset | Size | Description |
|--------|------|-------------|
| 0 | 1 | Variable index (0-255) |
| 1-2 | 2 | Value (int16 LE) |

---

#### `0x31` - SET_VARS
Set multiple consecutive variables.

**Payload:**
| Offset | Size | Description |
|--------|------|-------------|
| 0 | 1 | Start index |
| 1 | 1 | Count |
| 2+ | 2×N | Values (int16 LE each) |

---

#### `0x40` - DRAW_PIXEL
Draw a single pixel on current target.

**Payload (7 bytes):**
| Offset | Size | Description |
|--------|------|-------------|
| 0-1 | 2 | X position (int16 LE) |
| 2-3 | 2 | Y position (int16 LE) |
| 4 | 1 | Red (0-255) |
| 5 | 1 | Green (0-255) |
| 6 | 1 | Blue (0-255) |

---

#### `0x41` - DRAW_LINE
Draw a line between two points.

**Payload (11 bytes):**
| Offset | Size | Description |
|--------|------|-------------|
| 0-1 | 2 | X1 (int16 LE) |
| 2-3 | 2 | Y1 (int16 LE) |
| 4-5 | 2 | X2 (int16 LE) |
| 6-7 | 2 | Y2 (int16 LE) |
| 8 | 1 | Red |
| 9 | 1 | Green |
| 10 | 1 | Blue |

---

#### `0x42` - DRAW_RECT
Draw rectangle outline.

**Payload (11 bytes):**
| Offset | Size | Description |
|--------|------|-------------|
| 0-1 | 2 | X (int16 LE) |
| 2-3 | 2 | Y (int16 LE) |
| 4-5 | 2 | Width (int16 LE) |
| 6-7 | 2 | Height (int16 LE) |
| 8 | 1 | Red |
| 9 | 1 | Green |
| 10 | 1 | Blue |

---

#### `0x43` - DRAW_FILL
Draw filled rectangle.

**Payload (11 bytes):** Same as DRAW_RECT

---

#### `0x44` - DRAW_CIRCLE
Draw circle outline.

**Payload (9 bytes):**
| Offset | Size | Description |
|--------|------|-------------|
| 0-1 | 2 | Center X (int16 LE) |
| 2-3 | 2 | Center Y (int16 LE) |
| 4-5 | 2 | Radius (int16 LE) |
| 6 | 1 | Red |
| 7 | 1 | Green |
| 8 | 1 | Blue |

---

#### `0x45` - DRAW_POLY
Draw filled polygon (max 16 vertices).

**Payload:**
| Offset | Size | Description |
|--------|------|-------------|
| 0 | 1 | Vertex count N (max 16) |
| 1 | 1 | Red |
| 2 | 1 | Green |
| 3 | 1 | Blue |
| 4+ | 4×N | Vertex data (X,Y pairs as int16 LE) |

---

#### `0x46` - BLIT_SPRITE
Draw sprite at position.

**Payload (5 bytes):**
| Offset | Size | Description |
|--------|------|-------------|
| 0 | 1 | Sprite ID |
| 1-2 | 2 | X position (int16 LE) |
| 3-4 | 2 | Y position (int16 LE) |

---

#### `0x47` - CLEAR
Clear current target buffer with color.

**Payload (3 bytes):**
| Offset | Size | Description |
|--------|------|-------------|
| 0 | 1 | Red |
| 1 | 1 | Green |
| 2 | 1 | Blue |

---

#### `0x48` - DRAW_LINE_F
Draw line with sub-pixel precision using 8.8 fixed-point coordinates.

**8.8 Format:** High byte = integer part (signed), Low byte = fractional part (0-255 → 0.0-0.996)

**Payload (11 bytes):**
| Offset | Size | Description |
|--------|------|-------------|
| 0 | 1 | X1 fraction |
| 1 | 1 | X1 integer (signed) |
| 2 | 1 | Y1 fraction |
| 3 | 1 | Y1 integer (signed) |
| 4 | 1 | X2 fraction |
| 5 | 1 | X2 integer (signed) |
| 6 | 1 | Y2 fraction |
| 7 | 1 | Y2 integer (signed) |
| 8 | 1 | Red |
| 9 | 1 | Green |
| 10 | 1 | Blue |

---

#### `0x49` - DRAW_CIRCLE_F
Draw circle with sub-pixel precision.

**Payload (9 bytes):**
| Offset | Size | Description |
|--------|------|-------------|
| 0-1 | 2 | Center X (8.8 fixed) |
| 2-3 | 2 | Center Y (8.8 fixed) |
| 4-5 | 2 | Radius (8.8 fixed) |
| 6 | 1 | Red |
| 7 | 1 | Green |
| 8 | 1 | Blue |

---

#### `0x4A` - DRAW_RECT_F
Draw rectangle with sub-pixel precision.

**Payload (11 bytes):**
| Offset | Size | Description |
|--------|------|-------------|
| 0-1 | 2 | X (8.8 fixed) |
| 2-3 | 2 | Y (8.8 fixed) |
| 4-5 | 2 | Width (8.8 fixed) |
| 6-7 | 2 | Height (8.8 fixed) |
| 8 | 1 | Red |
| 9 | 1 | Green |
| 10 | 1 | Blue |

---

#### `0x50` - SET_TARGET
Select which display to render to.

**Payload (1 byte):**
| Value | Target |
|-------|--------|
| 0 | HUB75 (128x32 RGB) |
| 1 | OLED (128x128 mono) |

---

#### `0x51` - PRESENT
Push current buffer to display hardware.

**Payload:** None (length = 0)

---

#### `0x60` - OLED_CLEAR
Clear OLED buffer (always targets OLED regardless of SET_TARGET).

**Payload:** None (length = 0)

---

#### `0x61` - OLED_LINE
Draw line directly on OLED buffer.

**Payload (9 bytes):**
| Offset | Size | Description |
|--------|------|-------------|
| 0-1 | 2 | X1 (int16 LE) |
| 2-3 | 2 | Y1 (int16 LE) |
| 4-5 | 2 | X2 (int16 LE) |
| 6-7 | 2 | Y2 (int16 LE) |
| 8 | 1 | On/Off (0=off, >0=on) |

---

#### `0x62` - OLED_RECT
Draw rectangle outline on OLED.

**Payload (9 bytes):**
| Offset | Size | Description |
|--------|------|-------------|
| 0-1 | 2 | X (int16 LE) |
| 2-3 | 2 | Y (int16 LE) |
| 4-5 | 2 | Width (int16 LE) |
| 6-7 | 2 | Height (int16 LE) |
| 8 | 1 | On/Off |

---

#### `0x63` - OLED_FILL
Draw filled rectangle on OLED.

**Payload (9 bytes):** Same as OLED_RECT

---

#### `0x64` - OLED_CIRCLE
Draw circle outline on OLED.

**Payload (7 bytes):**
| Offset | Size | Description |
|--------|------|-------------|
| 0-1 | 2 | Center X (int16 LE) |
| 2-3 | 2 | Center Y (int16 LE) |
| 4-5 | 2 | Radius (int16 LE) |
| 6 | 1 | On/Off |

---

#### `0x65` - OLED_PRESENT
Push OLED buffer to display and clear buffer for next frame.

**Payload:** None (length = 0)

---

#### `0xF0` - PING
Connection check. GPU logs receipt.

**Payload:** None

---

#### `0xFF` - RESET
Reset all GPU state (shaders, sprites, variables, buffers).

**Payload:** None

---

## Usage Examples

### Example 1: Clear Screen and Draw Rectangle (HUB75)

```cpp
// Clear HUB75 to black
uint8_t clearPayload[] = {0, 0, 0};  // R, G, B
sendCommand(CmdType::SET_TARGET, (uint8_t[]){0}, 1);  // Target HUB75
sendCommand(CmdType::CLEAR, clearPayload, 3);

// Draw red filled rectangle at (10,5) size 50x20
uint8_t rectPayload[] = {
    10, 0,   // X = 10 (little-endian)
    5, 0,    // Y = 5
    50, 0,   // Width = 50
    20, 0,   // Height = 20
    255, 0, 0 // Red color
};
sendCommand(CmdType::DRAW_FILL, rectPayload, 11);

// Present to display
sendCommand(CmdType::PRESENT, nullptr, 0);
```

### Example 2: Draw Circle on OLED

```cpp
// Clear OLED
sendCommand(CmdType::OLED_CLEAR, nullptr, 0);

// Draw circle at center (64, 64) with radius 30
uint8_t circlePayload[] = {
    64, 0,   // CX = 64
    64, 0,   // CY = 64
    30, 0,   // Radius = 30
    1        // On (white)
};
sendCommand(CmdType::OLED_CIRCLE, circlePayload, 7);

// Push to display
sendCommand(CmdType::OLED_PRESENT, nullptr, 0);
```

### Example 3: Upload and Blit 1bpp Sprite

```cpp
// Define 8x8 smiley face sprite (1bpp)
const uint8_t smiley[8] = {
    0b00111100,  // Row 0
    0b01000010,  // Row 1
    0b10100101,  // Row 2 (eyes)
    0b10000001,  // Row 3
    0b10100101,  // Row 4 (smile corners)
    0b10011001,  // Row 5 (smile)
    0b01000010,  // Row 6
    0b00111100   // Row 7
};

// Upload sprite to slot 0
uint8_t uploadPayload[4 + 8] = {
    0,       // Sprite ID = 0
    8,       // Width = 8
    8,       // Height = 8
    1,       // Format = 1bpp
    // ... sprite data follows
};
memcpy(uploadPayload + 4, smiley, 8);
sendCommand(CmdType::UPLOAD_SPRITE, uploadPayload, 12);

// Blit sprite at position (20, 10)
uint8_t blitPayload[] = {
    0,       // Sprite ID = 0
    20, 0,   // X = 20
    10, 0    // Y = 10
};
sendCommand(CmdType::BLIT_SPRITE, blitPayload, 5);
sendCommand(CmdType::PRESENT, nullptr, 0);
```

### Example 4: Smooth Animation with Float Commands

```cpp
// Animate a circle smoothly across screen
for (float x = 0; x < 128; x += 0.5f) {
    sendCommand(CmdType::CLEAR, (uint8_t[]){0,0,0}, 3);
    
    // Convert float to 8.8 fixed point
    uint8_t circleF[9];
    circleF[0] = (uint8_t)((x - (int8_t)x) * 256);  // X fraction
    circleF[1] = (int8_t)x;                          // X integer
    circleF[2] = 0;                                   // Y fraction
    circleF[3] = 16;                                  // Y = 16
    circleF[4] = 0;                                   // R fraction
    circleF[5] = 8;                                   // R = 8
    circleF[6] = 0; circleF[7] = 255; circleF[8] = 0; // Green
    
    sendCommand(CmdType::DRAW_CIRCLE_F, circleF, 9);
    sendCommand(CmdType::PRESENT, nullptr, 0);
    
    vTaskDelay(pdMS_TO_TICKS(16));  // ~60 fps
}
```

### Example 5: Complete Frame Loop

```cpp
void renderLoop() {
    while (true) {
        // === HUB75 Frame ===
        sendCommand(CmdType::SET_TARGET, (uint8_t[]){0}, 1);
        sendCommand(CmdType::CLEAR, (uint8_t[]){0,0,32}, 3);  // Dark blue
        
        // Draw some content...
        drawSensorBars();  // Your function
        
        sendCommand(CmdType::PRESENT, nullptr, 0);
        
        // === OLED Frame ===
        sendCommand(CmdType::OLED_CLEAR, nullptr, 0);
        
        // Draw OLED content...
        drawStatusInfo();  // Your function
        
        sendCommand(CmdType::OLED_PRESENT, nullptr, 0);
        
        vTaskDelay(pdMS_TO_TICKS(16));  // 60 fps target
    }
}
```

---

## Best Practices

### 1. Batch Commands Before Present
Draw all primitives, then call PRESENT once:
```cpp
// Good: Batch drawing
CLEAR → DRAW_RECT → DRAW_LINE → DRAW_CIRCLE → PRESENT

// Bad: Present after each draw (slow, causes flicker)
CLEAR → PRESENT → DRAW_RECT → PRESENT → ...
```

### 2. Use OLED-Specific Commands for OLED
The `OLED_*` commands (`0x60`-`0x65`) always target the OLED regardless of `SET_TARGET`. Use them when you want to update OLED independently:

```cpp
// Update both displays in one loop iteration
SET_TARGET(0) → [HUB75 draws] → PRESENT
OLED_CLEAR → [OLED draws] → OLED_PRESENT
```

### 3. Pre-Upload Sprites
Upload sprites once at startup, then just blit them:
```cpp
// Startup
UPLOAD_SPRITE(0, dino_sprite)
UPLOAD_SPRITE(1, cactus_sprite)

// Game loop (fast!)
BLIT_SPRITE(0, x, y)
BLIT_SPRITE(1, obstacleX, obstacleY)
```

### 4. Use Float Commands for Smooth Animation
For animations where objects move less than 1 pixel per frame, use `DRAW_*_F` commands to avoid jitter.

### 5. Mind the UART Buffer
At 10 Mbps, the GPU can receive ~1.25 MB/s. If sending full frame data:
- HUB75: 12KB × 60fps = 720 KB/s ✓
- OLED: 2KB × 15fps = 30 KB/s ✓

For command-based rendering, bandwidth is rarely an issue.

### 6. Handle Boot Animation
The GPU shows a boot animation until it receives a command from the CPU. Send a PING or any command to start rendering:
```cpp
// On CPU startup
sendCommand(CmdType::PING, nullptr, 0);  // Wake up GPU
```

---

## Appendix: CmdType Enum (C++)

```cpp
enum class CmdType : uint8_t {
    NOP = 0x00,
    
    // Shaders
    UPLOAD_SHADER = 0x10,
    DELETE_SHADER = 0x11,
    EXEC_SHADER = 0x12,
    
    // Sprites
    UPLOAD_SPRITE = 0x20,
    DELETE_SPRITE = 0x21,
    
    // Variables
    SET_VAR = 0x30,
    SET_VARS = 0x31,
    
    // Drawing (integer coordinates)
    DRAW_PIXEL = 0x40,
    DRAW_LINE = 0x41,
    DRAW_RECT = 0x42,
    DRAW_FILL = 0x43,
    DRAW_CIRCLE = 0x44,
    DRAW_POLY = 0x45,
    BLIT_SPRITE = 0x46,
    CLEAR = 0x47,
    
    // Drawing (8.8 fixed-point coordinates)
    DRAW_LINE_F = 0x48,
    DRAW_CIRCLE_F = 0x49,
    DRAW_RECT_F = 0x4A,
    
    // Target control
    SET_TARGET = 0x50,  // 0=HUB75, 1=OLED
    PRESENT = 0x51,
    
    // OLED-specific (always target OLED)
    OLED_CLEAR = 0x60,
    OLED_LINE = 0x61,
    OLED_RECT = 0x62,
    OLED_FILL = 0x63,
    OLED_CIRCLE = 0x64,
    OLED_PRESENT = 0x65,
    
    // System
    PING = 0xF0,
    RESET = 0xFF,
};
```

---

## Appendix: Helper Functions

```cpp
// Send command helper
void sendCommand(CmdType cmd, const uint8_t* payload, uint16_t len) {
    uint8_t header[5] = {0xAA, 0x55, (uint8_t)cmd, 
                         (uint8_t)(len & 0xFF), (uint8_t)(len >> 8)};
    uart_write_bytes(UART_PORT, header, 5);
    if (len > 0 && payload) {
        uart_write_bytes(UART_PORT, payload, len);
    }
}

// Convert int16 to little-endian bytes
inline void int16ToLE(int16_t val, uint8_t* buf) {
    buf[0] = val & 0xFF;
    buf[1] = (val >> 8) & 0xFF;
}

// Convert float to 8.8 fixed point
inline void floatTo88(float val, uint8_t* buf) {
    int8_t intPart = (int8_t)val;
    uint8_t fracPart = (uint8_t)((val - intPart) * 256);
    buf[0] = fracPart;
    buf[1] = (uint8_t)intPart;
}

// Draw line helper
void drawLine(int16_t x1, int16_t y1, int16_t x2, int16_t y2, 
              uint8_t r, uint8_t g, uint8_t b) {
    uint8_t payload[11];
    int16ToLE(x1, payload + 0);
    int16ToLE(y1, payload + 2);
    int16ToLE(x2, payload + 4);
    int16ToLE(y2, payload + 6);
    payload[8] = r;
    payload[9] = g;
    payload[10] = b;
    sendCommand(CmdType::DRAW_LINE, payload, 11);
}

// Draw filled rect helper
void fillRect(int16_t x, int16_t y, int16_t w, int16_t h,
              uint8_t r, uint8_t g, uint8_t b) {
    uint8_t payload[11];
    int16ToLE(x, payload + 0);
    int16ToLE(y, payload + 2);
    int16ToLE(w, payload + 4);
    int16ToLE(h, payload + 6);
    payload[8] = r;
    payload[9] = g;
    payload[10] = b;
    sendCommand(CmdType::DRAW_FILL, payload, 11);
}

// Clear HUB75 to color
void clearHUB75(uint8_t r, uint8_t g, uint8_t b) {
    uint8_t target[] = {0};
    sendCommand(CmdType::SET_TARGET, target, 1);
    uint8_t color[] = {r, g, b};
    sendCommand(CmdType::CLEAR, color, 3);
}

// OLED pixel helper (direct buffer manipulation)
void oledPixel(int16_t x, int16_t y, bool on) {
    uint8_t payload[9];
    int16ToLE(x, payload + 0);
    int16ToLE(y, payload + 2);
    int16ToLE(1, payload + 4);  // w=1
    int16ToLE(1, payload + 6);  // h=1
    payload[8] = on ? 1 : 0;
    sendCommand(CmdType::OLED_FILL, payload, 9);
}
```

---

*Document generated for ARCOS GPU Protocol v1.0*
