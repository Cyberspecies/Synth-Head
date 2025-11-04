# UART LED Controller System

## Overview

This system receives LED data via UART to control 4 separate LED strips (49 RGBW LEDs total) and sends button state feedback back through the same UART connection.

## Hardware Configuration

### LED Strips (SK6812 RGBW)
- **Left Fin:** 13 LEDs on GPIO 18
- **Right Fin:** 13 LEDs on GPIO 38
- **Tongue:** 9 LEDs on GPIO 8
- **Scale:** 14 LEDs on GPIO 37

**Total:** 49 LEDs (13 + 13 + 9 + 14)

### Buttons (Active LOW with Pull-up)
- **Button A:** GPIO 5
- **Button B:** GPIO 6
- **Button C:** GPIO 7
- **Button D:** GPIO 10

### UART Connection (ESP32-to-ESP32)
- **RX:** GPIO 11
- **TX:** GPIO 12
- **Baud Rate:** 1,000,000 (1 Mbps)

---

## Data Protocol

### Receiving LED Data (from sender to this device)

The system expects a continuous stream of **196 bytes** per frame:

```
Total Data: (13 + 13 + 9 + 14) LEDs × 4 bytes/LED = 196 bytes
```

#### Data Structure (196 bytes total)

| LED Strip   | LED Count | Byte Range | Description        |
|-------------|-----------|------------|--------------------|
| Left Fin    | 13        | 0-51       | First 13 LEDs      |
| Right Fin   | 13        | 52-103     | Next 13 LEDs       |
| Tongue      | 9         | 104-139    | Next 9 LEDs        |
| Scale       | 14        | 140-195    | Last 14 LEDs       |

#### RGBW Format (4 bytes per LED)

Each LED uses 4 consecutive bytes in this order:
```
[R, G, B, W]
```

- **R:** Red value (0-255)
- **G:** Green value (0-255)
- **B:** Blue value (0-255)
- **W:** White value (0-255)

#### Example Frame

To set the first LED of each strip:
```
// Left Fin LED 0: Red (255, 0, 0, 0)
Bytes 0-3: [255, 0, 0, 0]

// Right Fin LED 0: Green (0, 255, 0, 0)
Bytes 52-55: [0, 255, 0, 0]

// Tongue LED 0: Blue (0, 0, 255, 0)
Bytes 104-107: [0, 0, 255, 0]

// Scale LED 0: White (0, 0, 0, 255)
Bytes 140-143: [0, 0, 0, 255]
```

### Sending Button Data (from this device to sender)

The system sends **4 bytes** of button state data:

```
[Button_A, Button_B, Button_C, Button_D]
```

- **0x00:** Button not pressed
- **0x01:** Button pressed

---

## Creating the Sender

### Python Example (PC or Raspberry Pi)

```python
import serial
import time

# Configure serial port
ser = serial.Serial(
    port='COM3',  # Change to your port (e.g., '/dev/ttyUSB0' on Linux)
    baudrate=1000000,
    bytesize=8,
    parity='N',
    stopbits=1,
    timeout=0.1
)

# LED configuration
LEFT_FIN_COUNT = 13
RIGHT_FIN_COUNT = 13
TONGUE_COUNT = 9
SCALE_COUNT = 14
TOTAL_LEDS = LEFT_FIN_COUNT + RIGHT_FIN_COUNT + TONGUE_COUNT + SCALE_COUNT
BYTES_PER_LED = 4
TOTAL_BYTES = TOTAL_LEDS * BYTES_PER_LED  # 196 bytes

def create_led_frame():
    """Create a frame of LED data (196 bytes)"""
    frame = bytearray(TOTAL_BYTES)
    
    # Example: Set all left fin LEDs to red
    for i in range(LEFT_FIN_COUNT):
        offset = i * BYTES_PER_LED
        frame[offset] = 255      # R
        frame[offset + 1] = 0    # G
        frame[offset + 2] = 0    # B
        frame[offset + 3] = 0    # W
    
    # Example: Set all right fin LEDs to green
    for i in range(RIGHT_FIN_COUNT):
        offset = (LEFT_FIN_COUNT + i) * BYTES_PER_LED
        frame[offset] = 0        # R
        frame[offset + 1] = 255  # G
        frame[offset + 2] = 0    # B
        frame[offset + 3] = 0    # W
    
    # Example: Set all tongue LEDs to blue
    for i in range(TONGUE_COUNT):
        offset = (LEFT_FIN_COUNT + RIGHT_FIN_COUNT + i) * BYTES_PER_LED
        frame[offset] = 0        # R
        frame[offset + 1] = 0    # G
        frame[offset + 2] = 255  # B
        frame[offset + 3] = 0    # W
    
    # Example: Set all scale LEDs to white
    for i in range(SCALE_COUNT):
        offset = (LEFT_FIN_COUNT + RIGHT_FIN_COUNT + TONGUE_COUNT + i) * BYTES_PER_LED
        frame[offset] = 0        # R
        frame[offset + 1] = 0    # G
        frame[offset + 2] = 0    # B
        frame[offset + 3] = 255  # W
    
    return frame

def read_button_state():
    """Read button state from receiver"""
    if ser.in_waiting >= 4:
        button_data = ser.read(4)
        return {
            'A': button_data[0] == 0x01,
            'B': button_data[1] == 0x01,
            'C': button_data[2] == 0x01,
            'D': button_data[3] == 0x01
        }
    return None

def main():
    print("UART LED Sender Starting...")
    print(f"Sending {TOTAL_BYTES} bytes per frame")
    
    try:
        while True:
            # Create and send LED frame
            frame = create_led_frame()
            ser.write(frame)
            
            # Read button state
            buttons = read_button_state()
            if buttons:
                print(f"Buttons: A={buttons['A']}, B={buttons['B']}, "
                      f"C={buttons['C']}, D={buttons['D']}")
            
            # Send at ~60 FPS
            time.sleep(1.0 / 60.0)
    
    except KeyboardInterrupt:
        print("\nStopping...")
    finally:
        ser.close()

if __name__ == '__main__':
    main()
```

### Arduino/ESP32 Sender Example

```cpp
#include <Arduino.h>

// UART configuration
#define UART_TX_PIN 12
#define UART_RX_PIN 11
#define UART_BAUD 1000000

// LED configuration
const int LEFT_FIN_COUNT = 13;
const int RIGHT_FIN_COUNT = 13;
const int TONGUE_COUNT = 9;
const int SCALE_COUNT = 14;
const int TOTAL_LEDS = LEFT_FIN_COUNT + RIGHT_FIN_COUNT + TONGUE_COUNT + SCALE_COUNT;
const int BYTES_PER_LED = 4;
const int TOTAL_BYTES = TOTAL_LEDS * BYTES_PER_LED; // 196 bytes

uint8_t led_frame[TOTAL_BYTES];
HardwareSerial UartSerial(1);

void setup(){
  Serial.begin(115200);
  
  // Initialize UART
  UartSerial.begin(UART_BAUD, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);
  
  Serial.println("UART LED Sender Ready");
}

void setLedRgbw(int led_index, uint8_t r, uint8_t g, uint8_t b, uint8_t w){
  if(led_index < 0 || led_index >= TOTAL_LEDS) return;
  
  int offset = led_index * BYTES_PER_LED;
  led_frame[offset] = r;
  led_frame[offset + 1] = g;
  led_frame[offset + 2] = b;
  led_frame[offset + 3] = w;
}

void sendFrame(){
  UartSerial.write(led_frame, TOTAL_BYTES);
}

void readButtons(){
  if(UartSerial.available() >= 4){
    uint8_t button_a = UartSerial.read();
    uint8_t button_b = UartSerial.read();
    uint8_t button_c = UartSerial.read();
    uint8_t button_d = UartSerial.read();
    
    Serial.printf("Buttons: A=%d B=%d C=%d D=%d\n", 
                  button_a, button_b, button_c, button_d);
  }
}

void loop(){
  // Example: Rainbow effect
  static uint8_t hue = 0;
  
  for(int i = 0; i < TOTAL_LEDS; i++){
    uint8_t pixel_hue = hue + (i * 256 / TOTAL_LEDS);
    // Simple HSV to RGB conversion (simplified)
    uint8_t r = (pixel_hue < 85) ? (255 - pixel_hue * 3) : 0;
    uint8_t g = (pixel_hue < 170) ? ((pixel_hue < 85) ? pixel_hue * 3 : 255 - (pixel_hue - 85) * 3) : 0;
    uint8_t b = (pixel_hue >= 170) ? ((pixel_hue - 170) * 3) : 0;
    
    setLedRgbw(i, r, g, b, 0);
  }
  
  hue++;
  
  // Send frame
  sendFrame();
  
  // Read button state
  readButtons();
  
  delay(16); // ~60 FPS
}
```

---

## Timing Recommendations

### Sending Rate
- **Recommended:** 30-60 FPS (frames per second)
- **Calculation:** `delay_ms = 1000 / fps`
- **Example:** 60 FPS = 16.67ms delay between frames

### Button Polling
- **Recommended:** 20-50 Hz
- The receiver sends button state every 50ms by default

---

## Troubleshooting

### No LED Response
1. Verify UART connections (RX ↔ TX crossover)
2. Check baud rate matches (1,000,000)
3. Ensure 196 bytes are sent per frame
4. Verify ground connection between devices

### Button Data Not Received
1. Check TX pin connection on receiver
2. Verify sender is reading from UART
3. Check for buffer overflow (clear buffer periodically)

### LEDs Show Wrong Colors
1. Verify RGBW byte order
2. Check if LEDs are RGBW (SK6812) vs RGB (WS2812B)
3. Ensure power supply is adequate (each LED can draw up to 80mA)

### Data Corruption
1. Use shorter cable runs (< 2 meters recommended at 1Mbps)
2. Add pull-up/pull-down resistors on UART lines if needed
3. Consider reducing baud rate to 115200 for longer distances

---

## Power Considerations

### Maximum Current Draw
- **Per LED:** ~80mA (all channels at full white)
- **Total (49 LEDs):** ~3.92A at full brightness

### Recommendations
1. Use a 5V power supply rated for at least 5A
2. Add bulk capacitor (1000µF) near LED strips
3. Add 100µF capacitor at each LED strip input
4. Use thick wires for power distribution (18-20 AWG)

---

## Code Files

### Receiver (This Device)
- `main_new.cpp` - Main application loop
- `include/UartController.h` - UART interface header
- `include/UartController.impl.hpp` - UART implementation
- `LedController_new.h` - LED driver header
- `LedController_new.impl.hpp` - LED driver implementation

### To Compile
Use PlatformIO with the provided `platformio.ini` configuration.

---

## License

See project license file.

## Author

ARCOS Team - 2025
