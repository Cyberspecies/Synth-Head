# GPU Pin Mapping

Source of truth: `env:GPU_Programmable` → `GPU_Programmable.cpp` (HUB75 pins from ARCOS `HUB75Config::getDefault()` / hardware pinout).

Board: ESP32-S3 (`esp32s3usbotg`), upload/monitor **COM5**.

---

## HUB75 (2× 64×32 → 128×32, dual OE)

Initialized with `begin(true, …)` (dual OE). Colour depth 5-bit, double buffer, gamma 2.2.

### Data

| Signal | GPIO |
|--------|------|
| R0 | 7 |
| G0 | 15 |
| B0 | 16 |
| R1 | 17 |
| G1 | 18 |
| B1 | 8 |

### Row address

| Signal | GPIO |
|--------|------|
| A | 41 |
| B | 40 |
| C | 39 |
| D | 38 |
| E | 42 |

### Control

| Signal | GPIO | Notes |
|--------|------|-------|
| CLK | 37 | |
| LAT | 36 | |
| OE | 35 | Left panel (X 0–63) |
| OE2 | 6 | Right panel (X 64–127) |

## I2C — OLED (SH1107 128×128)

| Signal | GPIO | Settings |
|--------|------|----------|
| SDA | 2 | 400 kHz, addr `0x3C` |
| SCL | 1 | Flip H + V on (upside-down mount); contrast `0xFF` |

## UART — CPU↔GPU (UART1, **921600**)

| Signal | GPIO | Direction |
|--------|------|-----------|
| TX | 12 | GPU → CPU RX (CPU GPIO 11) |
| RX | 13 | GPU ← CPU TX (CPU GPIO 12) |

## GPIO summary

| GPIO | Function |
|------|----------|
| 1 | OLED SCL |
| 2 | OLED SDA |
| 6 | HUB75 OE2 |
| 7 | HUB75 R0 |
| 8 | HUB75 B1 |
| 12 | UART TX → CPU |
| 13 | UART RX ← CPU |
| 15 | HUB75 G0 |
| 16 | HUB75 B0 |
| 17 | HUB75 R1 |
| 18 | HUB75 G1 |
| 35 | HUB75 OE |
| 36 | HUB75 LAT |
| 37 | HUB75 CLK |
| 38 | HUB75 D |
| 39 | HUB75 C |
| 40 | HUB75 B |
| 41 | HUB75 A |
| 42 | HUB75 E |
