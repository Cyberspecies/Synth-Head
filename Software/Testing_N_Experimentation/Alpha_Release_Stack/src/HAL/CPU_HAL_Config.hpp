/**
 * @file CPU_HAL_Config.hpp
 * @brief Hardware Abstraction Layer configuration for CPU
 * 
 * Defines all pin mappings and hardware configuration structures.
 * This file should be the single source of truth for hardware pins.
 */

#pragma once

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/uart.h"
#include "driver/i2s_std.h"

namespace HAL {

//=============================================================================
// Pin Definitions
//=============================================================================

// I2C Bus
constexpr gpio_num_t PIN_I2C_SDA = GPIO_NUM_9;
constexpr gpio_num_t PIN_I2C_SCL = GPIO_NUM_10;
constexpr uint32_t I2C_FREQ_HZ = 400000;

// Buttons (active LOW)
constexpr gpio_num_t PIN_BUTTON_A = GPIO_NUM_5;
constexpr gpio_num_t PIN_BUTTON_B = GPIO_NUM_6;
constexpr gpio_num_t PIN_BUTTON_C = GPIO_NUM_7;
constexpr gpio_num_t PIN_BUTTON_D = GPIO_NUM_15;

// LED Strips
constexpr gpio_num_t PIN_LED_STRIP_0 = GPIO_NUM_16;
constexpr gpio_num_t PIN_LED_STRIP_1 = GPIO_NUM_18;  // Left Fin - 13 LEDs
constexpr gpio_num_t PIN_LED_STRIP_2 = GPIO_NUM_8;   // Tongue - 9 LEDs
constexpr gpio_num_t PIN_LED_STRIP_3 = GPIO_NUM_39;
constexpr gpio_num_t PIN_LED_STRIP_4 = GPIO_NUM_38;  // Right Fin - 13 LEDs
constexpr gpio_num_t PIN_LED_STRIP_5 = GPIO_NUM_37;  // Scale LEDs - 14 LEDs

// Fans (PWM)
constexpr gpio_num_t PIN_FAN_1 = GPIO_NUM_17;
constexpr gpio_num_t PIN_FAN_2 = GPIO_NUM_36;

// INMP441 Microphone (I2S)
constexpr gpio_num_t PIN_MIC_DOUT = GPIO_NUM_2;
constexpr gpio_num_t PIN_MIC_CLK = GPIO_NUM_40;
constexpr gpio_num_t PIN_MIC_LR = GPIO_NUM_41;
constexpr gpio_num_t PIN_MIC_WS = GPIO_NUM_42;

// GPS (UART2)
constexpr gpio_num_t PIN_GPS_TX = GPIO_NUM_43;
constexpr gpio_num_t PIN_GPS_RX = GPIO_NUM_44;
constexpr uint32_t GPS_BAUD_RATE = 9600;

// ESP-to-ESP UART
constexpr gpio_num_t PIN_ESP_UART_RX = GPIO_NUM_11;
constexpr gpio_num_t PIN_ESP_UART_TX = GPIO_NUM_12;
constexpr uint32_t ESP_UART_BAUD_RATE = 1000000;

// MicroSD (SPI)
constexpr gpio_num_t PIN_SD_MISO = GPIO_NUM_14;
constexpr gpio_num_t PIN_SD_MOSI = GPIO_NUM_47;
constexpr gpio_num_t PIN_SD_CLK = GPIO_NUM_21;
constexpr gpio_num_t PIN_SD_CS = GPIO_NUM_48;

//=============================================================================
// I2C Device Addresses
//=============================================================================

constexpr uint8_t I2C_ADDR_ICM20948 = 0x68;
constexpr uint8_t I2C_ADDR_BME280 = 0x76;

//=============================================================================
// Configuration Structures
//=============================================================================

/**
 * @brief Complete HAL configuration passed to lifecycle
 */
struct Config {
    // I2C
    struct {
        gpio_num_t sda = PIN_I2C_SDA;
        gpio_num_t scl = PIN_I2C_SCL;
        uint32_t freq = I2C_FREQ_HZ;
        i2c_port_t port = I2C_NUM_0;
    } i2c;
    
    // Buttons
    struct {
        gpio_num_t a = PIN_BUTTON_A;
        gpio_num_t b = PIN_BUTTON_B;
        gpio_num_t c = PIN_BUTTON_C;
        gpio_num_t d = PIN_BUTTON_D;
    } buttons;
    
    // Microphone
    struct {
        gpio_num_t dout = PIN_MIC_DOUT;
        gpio_num_t clk = PIN_MIC_CLK;
        gpio_num_t ws = PIN_MIC_WS;
        gpio_num_t lr = PIN_MIC_LR;
        i2s_port_t port = I2S_NUM_0;
        uint32_t sampleRate = 16000;
    } mic;
    
    // GPS
    struct {
        gpio_num_t tx = PIN_GPS_TX;
        gpio_num_t rx = PIN_GPS_RX;
        uint32_t baud = GPS_BAUD_RATE;
        uart_port_t port = UART_NUM_2;
    } gps;
    
    // ESP-to-ESP UART
    struct {
        gpio_num_t tx = PIN_ESP_UART_TX;
        gpio_num_t rx = PIN_ESP_UART_RX;
        uint32_t baud = ESP_UART_BAUD_RATE;
        uart_port_t port = UART_NUM_1;
    } espUart;
    
    // LED Strips
    struct {
        gpio_num_t pins[6] = {
            PIN_LED_STRIP_0, PIN_LED_STRIP_1, PIN_LED_STRIP_2,
            PIN_LED_STRIP_3, PIN_LED_STRIP_4, PIN_LED_STRIP_5
        };
        uint8_t ledCounts[6] = {0, 13, 9, 0, 13, 14};
    } leds;
    
    // Fans
    struct {
        gpio_num_t fan1 = PIN_FAN_1;
        gpio_num_t fan2 = PIN_FAN_2;
    } fans;
};

/**
 * @brief Get default HAL configuration
 */
inline Config getDefaultConfig() {
    return Config{};
}

} // namespace HAL
