# GPU Pin Mapping Documentation

This document describes the pin assignments and connected peripherals for the GPU (ESP32-S3) in the current hardware setup.

---

## GPU Pin Mapping Overview

### HUB75 LED Matrix Display (128x32 Dual Panel)

The GPU drives two 64x32 HUB75 LED matrix panels for a combined resolution of 128x32 pixels.

#### Data Lines (RGB)
| Signal | GPIO | Description |
|--------|------|-------------|
| R0 | GPIO 7 | Upper half red data |
| G0 | GPIO 15 | Upper half green data |
| B0 | GPIO 16 | Upper half blue data |
| R1 | GPIO 17 | Lower half red data |
| G1 | GPIO 18 | Lower half green data |
| B1 | GPIO 8 | Lower half blue data |

#### Row Address Lines
| Signal | GPIO | Description |
|--------|------|-------------|
| A | GPIO 41 | Row address bit 0 |
| B | GPIO 40 | Row address bit 1 |
| C | GPIO 39 | Row address bit 2 |
| D | GPIO 38 | Row address bit 3 |
| E | GPIO 42 | Row address bit 4 (for 64-row panels) |

#### Control Lines
| Signal | GPIO | Description |
|--------|------|-------------|
| CLK | GPIO 37 | Pixel clock signal |
| LAT | GPIO 36 | Latch signal (load row data) |
| OE | GPIO 35 | Output Enable (primary panel) |
| OE2 | GPIO 6 | Output Enable (secondary panel, dual mode) |

---

### I2C Bus (OLED Display)
| Signal | GPIO | Description |
|--------|------|-------------|
| SDA | GPIO 2 | I2C Data |
| SCL | GPIO 1 | I2C Clock |

**Connected Devices:**
- SH1107 OLED Display (128x128, Address: 0x3C)

**I2C Settings:**
- Frequency: 400 kHz

---

### UART Communication (CPU-GPU)
| Signal | GPIO | Description |
|--------|------|-------------|
| TX | GPIO 12 | GPU TX → CPU RX (GPIO 11) |
| RX | GPIO 13 | GPU RX ← CPU TX (GPIO 12) |

**UART Settings:**
- Baud Rate: 10,000,000 (10 Mbps)
- Data Bits: 8
- Parity: None
- Stop Bits: 1
- Flow Control: Disabled

---

## Complete GPIO Summary

| GPIO | Function | Peripheral |
|------|----------|------------|
| 1 | SDA | I2C (OLED) |
| 2 | SCL | I2C (OLED) |
| 6 | OE2 | HUB75 (Secondary Output Enable) |
| 7 | R0 | HUB75 (Upper Red) |
| 8 | B1 | HUB75 (Lower Blue) |
| 12 | TX | UART (to CPU) |
| 13 | RX | UART (from CPU) |
| 15 | G0 | HUB75 (Upper Green) |
| 16 | B0 | HUB75 (Upper Blue) |
| 17 | R1 | HUB75 (Lower Red) |
| 18 | G1 | HUB75 (Lower Green) |
| 35 | OE | HUB75 (Primary Output Enable) |
| 36 | LAT | HUB75 (Latch) |
| 37 | CLK | HUB75 (Clock) |
| 38 | D | HUB75 (Row Address D) |
| 39 | C | HUB75 (Row Address C) |
| 40 | B | HUB75 (Row Address B) |
| 41 | A | HUB75 (Row Address A) |
| 42 | E | HUB75 (Row Address E) |

---

## Display Configuration

### HUB75 Settings
- **Panel Size:** 64x32 per panel
- **Total Resolution:** 128x32 (dual panel mode)
- **Colour Depth:** 5-bit per channel (32 levels)
- **Double Buffering:** Enabled
- **Gamma Correction:** Enabled (γ = 2.2)
- **Clock Frequency:** 10 MHz

### OLED Settings
- **Display:** SH1107
- **Resolution:** 128x128 monochrome
- **I2C Address:** 0x3C
- **Contrast:** 0xCF (default)
- **Orientation:** Flipped horizontal and vertical (upside-down mounting)

---

## Hardware Notes

### HUB75 Dual Panel Mode
The GPU operates in dual Output Enable (OE) mode where:
- **OE (GPIO 35):** Controls the left panel (X: 0-63)
- **OE2 (GPIO 6):** Controls the right panel (X: 64-127)

Both panels share the same data and address lines but are enabled separately for parallel addressing.

### Panel Inversion Settings
- **Panel 0 (Left):** Flip vertical = true
- **Panel 1 (Right):** Flip vertical = false

### Alternative Pin Mapping (ESP32-HUB75-MatrixPanel-I2S-DMA)
For reference, the legacy HAL uses different pins:
| Signal | GPIO |
|--------|------|
| R1 | 25 |
| G1 | 26 |
| B1 | 27 |
| R2 | 14 |
| G2 | 12 |
| B2 | 13 |
| A | 23 |
| B | 19 |
| C | 5 |
| D | 17 |
| E | -1 (unused) |
| LAT | 4 |
| OE | 15 |
| CLK | 16 |

**Note:** The current ARCOS driver uses the pin mapping in the main table above.

---

**Note:** Please verify all connections before powering the system. Update this document if hardware changes are made.
