#ifndef UART_CONTROLLER_H
#define UART_CONTROLLER_H

#include <Arduino.h>
#include <HardwareSerial.h>

class UartController{
private:
  // UART pin definitions from PIN_MAPPING_CPU.md
  static constexpr int UART_RX_PIN = 11;
  static constexpr int UART_TX_PIN = 12;
  static constexpr int UART_BAUD_RATE = 921600; // 921.6 kbps - standard high-speed rate, more reliable than 1 Mbps
  static constexpr int UART_RX_BUFFER_SIZE = 4096; // Large RX buffer for high-speed reception

  // LED configuration
  static constexpr int LEFT_FIN_LED_COUNT = 13;
  static constexpr int RIGHT_FIN_LED_COUNT = 13;
  static constexpr int TONGUE_LED_COUNT = 9;
  static constexpr int SCALE_LED_COUNT = 14;
  static constexpr int TOTAL_LED_COUNT = LEFT_FIN_LED_COUNT + RIGHT_FIN_LED_COUNT + TONGUE_LED_COUNT + SCALE_LED_COUNT;
  
  // RGBW bytes per LED
  static constexpr int BYTES_PER_LED = 4;
  static constexpr int LED_DATA_BYTES = TOTAL_LED_COUNT * BYTES_PER_LED; // 196 bytes
  
  // Frame protocol with sync markers and CRC
  static constexpr uint8_t SYNC_BYTE_1 = 0xAA;
  static constexpr uint8_t SYNC_BYTE_2 = 0x55;
  static constexpr int SYNC_BYTES = 2;
  static constexpr int FRAME_COUNTER_BYTES = 1;
  static constexpr int CRC_BYTES = 1;
  static constexpr int TOTAL_BUFFER_SIZE = SYNC_BYTES + LED_DATA_BYTES + FRAME_COUNTER_BYTES + CRC_BYTES; // 200 bytes

  // Button pin definitions from PIN_MAPPING_CPU.md
  static constexpr int BUTTON_A_PIN = 5;
  static constexpr int BUTTON_B_PIN = 6;
  static constexpr int BUTTON_C_PIN = 7;
  static constexpr int BUTTON_D_PIN = 15;

  HardwareSerial* uart_serial;
  
  // Receive buffer for LED data
  uint8_t receive_buffer[TOTAL_BUFFER_SIZE];
  int buffer_index;
  
  // Button state tracking
  bool button_state[4];
  bool last_button_state[4];
  
  // Frame counter tracking
  uint8_t last_frame_counter;
  uint32_t total_frames_received;
  uint32_t frames_skipped;
  
  // Packet loss diagnostics
  uint32_t frames_corrupted;      // Failed CRC
  uint32_t sync_failures;         // Failed to find sync marker
  uint32_t total_bytes_received;  // Total UART bytes processed
  
  // Helper methods
  bool findSyncMarker();
  uint8_t calculateCRC8(const uint8_t* data, size_t length);
  bool validateReceivedData();

public:
  UartController();
  ~UartController();

  // Initialization
  bool initialize();

  // Main update loop
  void update();

  // Receive LED data
  bool receiveData();
  bool hasNewData() const;
  
  // Send button state
  void sendButtonState();
  
  // Get LED data by section
  const uint8_t* getLeftFinData() const;
  const uint8_t* getRightFinData() const;
  const uint8_t* getTongueData() const;
  const uint8_t* getScaleData() const;
  
  // Get individual LED RGBW values
  void getLedRgbw(int led_index, uint8_t& r, uint8_t& g, uint8_t& b, uint8_t& w) const;
  
  // Frame counter and skip detection
  uint8_t getFrameCounter() const;
  uint32_t getTotalFramesReceived() const;
  uint32_t getFramesSkipped() const;
  uint32_t getFramesCorrupted() const;
  uint32_t getSyncFailures() const;
  
  // Get button states
  bool getButtonState(int button_index) const;
  bool getButtonPressed(int button_index) const; // Returns true on button press event
  
  // Clear buffer
  void clearBuffer();
  
  // Debug methods
  void printReceivedData() const;
  void printButtonStates() const;
  
private:
  void initializeButtons();
  void readButtons();
};

#endif // UART_CONTROLLER_H
