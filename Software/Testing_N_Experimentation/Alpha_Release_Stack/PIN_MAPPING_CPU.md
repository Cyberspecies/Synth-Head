
# CPU Pin Mapping Documentation

This document describes the pin assignments and connected peripherals for the CPU in the current hardware setup.

---

## CPU Pin Mapping Overview

### I2C Bus
- **SDA:** GPIO 9
- **SCL:** GPIO 10
- **Connected Devices:** ICM20948 (IMU), BME280 (Environmental Sensor)

### LED Strips
- **LED Strip 0:** GPIO 16
- **LED Strip 1 (Left Fin):** GPIO 18 — 13 LEDs in series
- **LED Strip 2:** GPIO 8
- **LED Strip 3:** GPIO 39
- **LED Strip 4 (Right Fin):** GPIO 38 — 13 LEDs in series
- **LED Strip 5 (Scale LEDs):** GPIO 37 — 14 LEDs in a row

### Buttons (A/B/C/D)
- **Button 0 (A):** GPIO 5
- **Button 1 (B):** GPIO 6
- **Button 2 (C):** GPIO 7
- **Button 3 (D):** GPIO 10

### Fans (PWM Controlled)
- **Fan 1:** GPIO 17
- **Fan 2:** GPIO 36

### MicroSD Card (SPI)
- **MISO:** GPIO 14
- **MOSI:** GPIO 47
- **CLK:** GPIO 21
- **CS:** GPIO 48

### UART Connections
- **NEO M8 (GPS):** TX: GPIO 43, RX: GPIO 44
- **ESP-to-ESP Connection:** RX: GPIO 11, TX: GPIO 12
  - **Baud Rate:** 1,000,000 (1M) by default

### I2S Microphone (INMP441)
- **DOUT:** GPIO 2
- **CLK:** GPIO 40
- **L/R Select:** GPIO 41
- **WS:** GPIO 42

---

## CPU Features Used

All features are used except:
- Fan 1
- LED Strip 0
- LED Strip 3

#### LED Strip Details
- **Strip 1 (Left Fin):** 13 LEDs in series
- **Strip 2 (Tongue):** 9 LEDs in series
- **Strip 4 (Right Fin):** 13 LEDs in series
- **Strip 5 (Scale LEDs):** 14 LEDs in a row

#### Button Mapping
- **Button 0:** A
- **Button 1:** B
- **Button 2:** C
- **Button 3:** D

#### UART Settings
- **ESP-to-ESP UART Baud Rate:** 1,000,000 (1M) by default

---

**Note:** Please verify all connections before powering the system. Update this document if hardware changes are made.

## I2C Bus
- **SDA:** GPIO 9
- **SCL:** GPIO 10
- **Connected Devices:**
  - ICM20948 (IMU)
  - BME280 (Environmental Sensor)

## LED Strips
- **LED Strip 0:** GPIO 16
- **LED Strip 1:** GPIO 18
- **LED Strip 2:** GPIO 5
- **LED Strip 3:** GPIO 39
- **LED Strip 4:** GPIO 38
- **LED Strip 5:** GPIO 37

## Buttons
- **Button 1:** GPIO 5
- **Button 2:** GPIO 6
- **Button 3:** GPIO 7
- **Button 4:** GPIO 10

## Fans (PWM Controlled)
- **Fan 1:** GPIO 17
- **Fan 2:** GPIO 36

## MicroSD Card (SPI)
- **MISO:** GPIO 14
- **MOSI:** GPIO 47
- **CLK:** GPIO 21
- **CS:** GPIO 48

## UART Connections
- **NEO M8 (GPS):**
  - TX: GPIO 43
  - RX: GPIO 44
- **ESP-to-ESP Connection:**
  - RX: GPIO 11
  - TX: GPIO 12

## I2S Microphone (INMP441)
- **DOUT:** GPIO 2
- **CLK:** GPIO 40
- **L/R Select:** GPIO 41
- **WS:** GPIO 42



---

## Features Used

All features are used except:
- Fan 1
- LED Strip 0
- LED Strip 3

### LED Strip Details
- **Strip 1 (Left Fin):** 13 LEDs in series
- **Strip 4 (Right Fin):** 13 LEDs in series
- **Strip 5 (Scale LEDs):** 14 LEDs in a row

### Button Mapping
- **Button 0:** A
- **Button 1:** B
- **Button 2:** C
- **Button 3:** D

### UART Settings
- **ESP-to-ESP UART Baud Rate:** 1,000,000 (1M) by default