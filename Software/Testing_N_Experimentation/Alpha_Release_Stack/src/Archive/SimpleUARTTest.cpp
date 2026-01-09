/*****************************************************************
 * SimpleUARTTest.cpp - UART Byte Test
 * 
 * CPU sends 0x55, expects 0xAA from GPU
 * GPU sends 0xAA, expects 0x55 from CPU
 *****************************************************************/

#ifdef CPU_BUILD
// ============================================================
// CPU Build (Arduino Framework)
// ============================================================
#include <Arduino.h>

constexpr uint8_t UART_TX_PIN = 12;
constexpr uint8_t UART_RX_PIN = 11;
constexpr uint32_t BAUD_RATE = 2000000;

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("\n========================================");
  Serial.println("  UART Byte Test - CPU");
  Serial.printf("  TX=GPIO%d  RX=GPIO%d  Baud=%d\n", UART_TX_PIN, UART_RX_PIN, BAUD_RATE);
  Serial.println("========================================\n");
  
  // Initialize UART on Serial1
  Serial1.begin(BAUD_RATE, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);
  Serial.println("Serial1 initialized!");
  Serial.println("Starting TX/RX test...");
}

int cycle = 0;

void loop() {
  cycle++;
  Serial.printf("\n=== Cycle %d ===\n", cycle);
  
  // Send byte to GPU
  uint8_t tx_byte = 0x55;  // CPU sends 0x55
  Serial1.write(tx_byte);
  Serial.printf("CPU TX: Sent 0x%02X\n", tx_byte);
  
  // Wait a bit for response
  delay(50);
  
  // Check for received data from GPU
  int available = Serial1.available();
  if (available > 0) {
    Serial.printf("CPU RX: Received %d bytes:\n", available);
    while (Serial1.available()) {
      uint8_t rx_byte = Serial1.read();
      Serial.printf("  0x%02X\n", rx_byte);
    }
  } else {
    Serial.println("CPU RX: No data from GPU");
  }
  
  delay(950);  // ~1 second cycle
}

#endif // CPU_BUILD


