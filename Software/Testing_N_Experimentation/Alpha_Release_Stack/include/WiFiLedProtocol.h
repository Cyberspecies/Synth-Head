#ifndef WIFI_LED_PROTOCOL_H
#define WIFI_LED_PROTOCOL_H

#include <stdint.h>

/**
 * @file WiFiLedProtocol.h
 * @brief WiFi-based LED control protocol definitions
 * 
 * Replaces UART with WiFi for high-speed LED data transmission
 */

// WiFi Configuration Exchange (via UART)
#define WIFI_CONFIG_SYNC_1 0xCC
#define WIFI_CONFIG_SYNC_2 0xDD
#define WIFI_SSID_MAX_LEN 32
#define WIFI_PASSWORD_MAX_LEN 64

struct WiFiConfig {
  uint8_t sync1;              // 0xCC
  uint8_t sync2;              // 0xDD
  char ssid[WIFI_SSID_MAX_LEN];
  char password[WIFI_PASSWORD_MAX_LEN];
  uint32_t cpu_ip;            // CPU's IP address
  uint16_t led_port;          // Port for LED data
  uint16_t button_port;       // Port for button data
  uint8_t crc;                // CRC8 checksum
} __attribute__((packed));

// LED Data Packet (via WiFi UDP)
#define LED_PACKET_MAGIC 0xAA55
#define LEFT_FIN_COUNT 13
#define RIGHT_FIN_COUNT 13
#define TONGUE_COUNT 9
#define SCALE_COUNT 14
#define TOTAL_LEDS (LEFT_FIN_COUNT + RIGHT_FIN_COUNT + TONGUE_COUNT + SCALE_COUNT)
#define LED_DATA_BYTES (TOTAL_LEDS * 4) // 196 bytes RGBW

struct LedDataPacket {
  uint16_t magic;             // 0xAA55
  uint8_t frame_counter;      // 1-60 for frame skip detection
  uint8_t reserved;           // Padding
  uint8_t led_data[LED_DATA_BYTES]; // 196 bytes RGBW
  uint8_t crc;                // CRC8 checksum
} __attribute__((packed));

// Button Data Packet (via WiFi UDP)
#define BUTTON_PACKET_MAGIC 0x5AA5

struct ButtonDataPacket {
  uint16_t magic;             // 0x5AA5
  uint8_t button_a;           // 0x00 or 0x01
  uint8_t button_b;
  uint8_t button_c;
  uint8_t button_d;
  uint8_t crc;                // CRC8 checksum
} __attribute__((packed));

// Protocol constants
#define DEFAULT_LED_PORT 8888
#define DEFAULT_BUTTON_PORT 8889

// Helper function declarations
uint8_t calculateCRC8(const uint8_t* data, size_t length);

#endif // WIFI_LED_PROTOCOL_H
