# CPU Pin Mapping

Source of truth: `env:CPU_Main` → `CPU_Main.cpp` → `LifecycleController` / `CurrentMode` + `Drivers/*`.

Board: ESP32-S3 (`esp32s3_n16`), upload/monitor **COM6**.

---

## I2C (port 0, 400 kHz)

| Signal | GPIO | Devices |
|--------|------|---------|
| SDA | 9 | ICM20948 (`0x68`), BME280 (`0x76`) |
| SCL | 10 | |

## Buttons (active LOW)

| Button | GPIO | Notes |
|--------|------|-------|
| A | 5 | Boot: hold for debug menu; with D = system test |
| B | 6 | Select; hold 15s at boot = factory reset |
| C | 7 | Next / down |
| D | 15 | Back / cancel; with A at boot = system test |

## LED strips (WS2812)

| Strip | GPIO | LEDs | Role |
|-------|------|------|------|
| 0 | 16 | 0 | Unused |
| 1 | 18 | 13 | Left fin |
| 2 | 8 | 9 | Tongue |
| 3 | 39 | 0 | Unused |
| 4 | 38 | 13 | Right fin |
| 5 | 37 | 14 | Scale LEDs |

## Fans

| Fan | GPIO |
|-----|------|
| 1 | 17 |
| 2 | 36 |

## MicroSD (SPI)

| Signal | GPIO |
|--------|------|
| MISO | 14 |
| MOSI | 47 |
| CLK | 21 |
| CS | 48 |

## UART — GPS (UART2, 9600)

| Signal | GPIO | Direction |
|--------|------|-----------|
| TX | 43 | CPU → GPS RX |
| RX | 44 | CPU ← GPS TX |

## UART — CPU↔GPU (UART1, **921600**)

| Signal | GPIO | Direction |
|--------|------|-----------|
| TX | 12 | CPU → GPU RX (GPU GPIO 13) |
| RX | 11 | CPU ← GPU TX (GPU GPIO 12) |

## I2S mic (INMP441)

| Signal | GPIO |
|--------|------|
| DOUT / DATA | 2 |
| CLK / BCK | 40 |
| L/R select | 41 |
| WS / LRCLK | 42 |
