/*****************************************************************
 * File:      CPU_BaudTest.cpp
 * Purpose:   Test UART baud rates with smaller 1KB packets
 * 
 * Tests each baud rate by sending test packets and waiting for ACK
 * Packet sizes: 512B, 1KB, 2KB
 *****************************************************************/

#include <Arduino.h>

// Test baud rates (in order)
const uint32_t BAUD_RATES[] = {
  2000000,   // 2 Mbps
  3000000,   // 3 Mbps
  4000000,   // 4 Mbps
  5000000,   // 5 Mbps
  6000000,   // 6 Mbps
  8000000,   // 8 Mbps
  10000000,  // 10 Mbps
  12000000,  // 12 Mbps
  15000000,  // 15 Mbps
  20000000,  // 20 Mbps
};
const int NUM_BAUDS = sizeof(BAUD_RATES) / sizeof(BAUD_RATES[0]);

// Test packet structure
const uint8_t SYNC_PATTERN[] = {0xAA, 0x55, 0xCC, 0x33};
const uint8_t TEST_512B_CMD = 0x01;   // 512B test command
const uint8_t TEST_1KB_CMD = 0x02;    // 1KB test command
const uint8_t TEST_2KB_CMD = 0x03;    // 2KB test command
const uint8_t TEST_4KB_CMD = 0x04;    // 4KB test command
const uint8_t ACK_CMD = 0x05;         // ACK response

// Packet sizes
const int PACKET_512B = 512;
const int PACKET_1KB = 1024;
const int PACKET_2KB = 2048;
const int PACKET_4KB = 4096;

// Test parameters
const int PACKETS_PER_SIZE = 30;  // 30 packets per size per baud
const int ACK_TIMEOUT_MS = 50;    // Timeout for ACK

// Pin configuration
const int UART_RX = 11;
const int UART_TX = 12;

// Results storage per packet size
struct SizeResult {
  int sent;
  int acked;
  uint32_t tx_time_sum;
};

struct BaudResult {
  uint32_t baud;
  SizeResult p512;
  SizeResult p1k;
  SizeResult p2k;
  SizeResult p4k;
};
BaudResult results[NUM_BAUDS];

// Current test state
int current_baud_idx = 0;
int current_size_idx = 0;  // 0=512B, 1=1KB, 2=2KB
int packets_sent = 0;
int packets_acked = 0;
uint32_t tx_time_sum = 0;
uint32_t send_time = 0;
bool waiting_for_ack = false;
uint8_t current_seq = 0;

// Packet buffer (max 4KB + header)
uint8_t tx_packet[PACKET_4KB + 16];
uint8_t rx_buffer[16];
int rx_idx = 0;

int getCurrentPacketSize() {
  switch(current_size_idx) {
    case 0: return PACKET_512B;
    case 1: return PACKET_1KB;
    case 2: return PACKET_2KB;
    case 3: return PACKET_4KB;
    default: return PACKET_1KB;
  }
}

uint8_t getCurrentCmd() {
  switch(current_size_idx) {
    case 0: return TEST_512B_CMD;
    case 1: return TEST_1KB_CMD;
    case 2: return TEST_2KB_CMD;
    case 3: return TEST_4KB_CMD;
    default: return TEST_1KB_CMD;
  }
}

const char* getSizeName() {
  switch(current_size_idx) {
    case 0: return "512B";
    case 1: return "1KB";
    case 2: return "2KB";
    case 3: return "4KB";
    default: return "???";
  }
}

void initPacket() {
  int size = getCurrentPacketSize();
  memcpy(tx_packet, SYNC_PATTERN, 4);
  tx_packet[4] = getCurrentCmd();
  tx_packet[5] = current_seq;
  // Fill with pattern
  for (int i = 6; i < size + 6; i++) {
    tx_packet[i] = (i + current_seq) & 0xFF;
  }
}

void switchBaud(uint32_t baud) {
  Serial1.end();
  delay(10);
  Serial1.begin(baud, SERIAL_8N1, UART_RX, UART_TX);
  delay(50);  // Let baud settle
}

void printResults() {
  Serial.println("\n");
  Serial.println("╔═══════════════════════════════════════════════════════════════════════════════════════════════════════════════════════╗");
  Serial.println("║                              UART BAUD RATE TEST RESULTS (Packet Size Comparison)                                     ║");
  Serial.println("╠══════════════╦══════════════════════╦══════════════════════╦══════════════════════╦══════════════════════╦═══════════╣");
  Serial.println("║   Baud Rate  ║     512B Packet      ║      1KB Packet      ║      2KB Packet      ║      4KB Packet      ║  Max FPS  ║");
  Serial.println("║              ║  Success |  TX Time  ║  Success |  TX Time  ║  Success |  TX Time  ║  Success |  TX Time  ║   @ 4KB   ║");
  Serial.println("╠══════════════╬══════════╬═══════════╬══════════╬═══════════╬══════════╬═══════════╬══════════╬═══════════╬═══════════╣");
  
  for (int i = 0; i < NUM_BAUDS; i++) {
    char line[200];
    
    float p512_pct = results[i].p512.sent > 0 ? 100.0f * results[i].p512.acked / results[i].p512.sent : 0;
    float p1k_pct = results[i].p1k.sent > 0 ? 100.0f * results[i].p1k.acked / results[i].p1k.sent : 0;
    float p2k_pct = results[i].p2k.sent > 0 ? 100.0f * results[i].p2k.acked / results[i].p2k.sent : 0;
    float p4k_pct = results[i].p4k.sent > 0 ? 100.0f * results[i].p4k.acked / results[i].p4k.sent : 0;
    
    uint32_t p512_avg = results[i].p512.acked > 0 ? results[i].p512.tx_time_sum / results[i].p512.sent : 0;
    uint32_t p1k_avg = results[i].p1k.acked > 0 ? results[i].p1k.tx_time_sum / results[i].p1k.sent : 0;
    uint32_t p2k_avg = results[i].p2k.acked > 0 ? results[i].p2k.tx_time_sum / results[i].p2k.sent : 0;
    uint32_t p4k_avg = results[i].p4k.acked > 0 ? results[i].p4k.tx_time_sum / results[i].p4k.sent : 0;
    
    // Calculate max FPS using 4KB packets (closer to real frame sizes)
    uint32_t max_fps = p4k_avg > 0 ? 1000000 / p4k_avg : 0;
    
    snprintf(line, sizeof(line), 
             "║ %4lu Mbps    ║  %5.1f%%  │ %5lu us  ║  %5.1f%%  │ %5lu us  ║  %5.1f%%  │ %5lu us  ║  %5.1f%%  │ %5lu us  ║   %4lu    ║",
             results[i].baud / 1000000,
             p512_pct, p512_avg,
             p1k_pct, p1k_avg,
             p2k_pct, p2k_avg,
             p4k_pct, p4k_avg,
             max_fps);
    Serial.println(line);
  }
  
  Serial.println("╚══════════════╩══════════╩═══════════╩══════════╩═══════════╩══════════╩═══════════╩══════════╩═══════════╩═══════════╝");
  
  // Find best configuration
  Serial.println("\n═══════════════════════════════════════════════════════════════════════");
  Serial.println("                    BEST CONFIGURATION RECOMMENDATION");
  Serial.println("═══════════════════════════════════════════════════════════════════════");
  
  int best_baud_idx = -1;
  int best_size_idx = -1;
  uint32_t best_fps = 0;
  
  for (int i = 0; i < NUM_BAUDS; i++) {
    // Check 4KB at 100%
    if (results[i].p4k.sent > 0 && results[i].p4k.acked == results[i].p4k.sent) {
      uint32_t fps = results[i].p4k.tx_time_sum > 0 ? 1000000 / (results[i].p4k.tx_time_sum / results[i].p4k.sent) : 0;
      if (fps > best_fps) { best_fps = fps; best_baud_idx = i; best_size_idx = 3; }
    }
    // Check 2KB at 100%
    if (results[i].p2k.sent > 0 && results[i].p2k.acked == results[i].p2k.sent) {
      uint32_t fps = results[i].p2k.tx_time_sum > 0 ? 1000000 / (results[i].p2k.tx_time_sum / results[i].p2k.sent) : 0;
      if (fps > best_fps) { best_fps = fps; best_baud_idx = i; best_size_idx = 2; }
    }
    // Check 1KB at 100%
    if (results[i].p1k.sent > 0 && results[i].p1k.acked == results[i].p1k.sent) {
      uint32_t fps = results[i].p1k.tx_time_sum > 0 ? 1000000 / (results[i].p1k.tx_time_sum / results[i].p1k.sent) : 0;
      if (fps > best_fps) { best_fps = fps; best_baud_idx = i; best_size_idx = 1; }
    }
  }
  
  if (best_baud_idx >= 0) {
    const char* sizes[] = {"512B", "1KB", "2KB", "4KB"};
    uint32_t pkt_sizes[] = {512, 1024, 2048, 4096};
    Serial.printf("  Best: %lu Mbps with %s packets = %lu max FPS\n", 
                  results[best_baud_idx].baud / 1000000, sizes[best_size_idx], best_fps);
    Serial.printf("  HUB75 (12KB): %d fragments x %s = ~%lu FPS\n",
                  12288 / pkt_sizes[best_size_idx], sizes[best_size_idx],
                  best_fps / (12288 / pkt_sizes[best_size_idx]));
    Serial.printf("  OLED (2KB):  %d fragments x %s = ~%lu FPS\n",
                  2048 / pkt_sizes[best_size_idx], sizes[best_size_idx],
                  best_fps / ((2048 + pkt_sizes[best_size_idx] - 1) / pkt_sizes[best_size_idx]));
  } else {
    Serial.println("  No 100% reliable configuration found!");
  }
  Serial.println("═══════════════════════════════════════════════════════════════════════\n");
}

void saveCurrentResults() {
  SizeResult* sr;
  switch(current_size_idx) {
    case 0: sr = &results[current_baud_idx].p512; break;
    case 1: sr = &results[current_baud_idx].p1k; break;
    case 2: sr = &results[current_baud_idx].p2k; break;
    case 3: sr = &results[current_baud_idx].p4k; break;
    default: return;
  }
  sr->sent = packets_sent;
  sr->acked = packets_acked;
  sr->tx_time_sum = tx_time_sum;
}

void startNextTest() {
  // Save previous results
  if (packets_sent > 0) {
    saveCurrentResults();
    float pct = 100.0f * packets_acked / packets_sent;
    Serial.printf("  %s: %d/%d (%.1f%%)\n", getSizeName(), packets_acked, packets_sent, pct);
  }
  
  // Move to next size or baud
  current_size_idx++;
  if (current_size_idx > 3) {  // Now 4 sizes: 512B, 1KB, 2KB, 4KB
    current_size_idx = 0;
    current_baud_idx++;
    
    if (current_baud_idx >= NUM_BAUDS) {
      // All tests complete
      printResults();
      Serial.println("Test complete! Restarting in 10 seconds...");
      delay(10000);
      current_baud_idx = 0;
      
      // Reset all results
      for (int i = 0; i < NUM_BAUDS; i++) {
        results[i].p512 = {0, 0, 0};
        results[i].p1k = {0, 0, 0};
        results[i].p2k = {0, 0, 0};
        results[i].p4k = {0, 0, 0};
      }
    }
    
    // Switch baud rate
    uint32_t baud = BAUD_RATES[current_baud_idx];
    results[current_baud_idx].baud = baud;
    Serial.printf("\n[TEST] %lu Mbps\n", baud / 1000000);
    Serial.println("════════════════════════════════════════");
    switchBaud(baud);
    delay(100);
  }
  
  // Reset counters
  packets_sent = 0;
  packets_acked = 0;
  tx_time_sum = 0;
  current_seq = 0;
  waiting_for_ack = false;
  rx_idx = 0;
}

void sendPacket() {
  initPacket();
  current_seq++;
  tx_packet[5] = current_seq;
  
  int size = getCurrentPacketSize();
  
  uint32_t tx_start = micros();
  Serial1.write(tx_packet, size + 6);
  Serial1.flush();
  uint32_t tx_time = micros() - tx_start;
  
  send_time = millis();
  waiting_for_ack = true;
  packets_sent++;
  tx_time_sum += tx_time;
}

bool checkForAck() {
  while (Serial1.available()) {
    uint8_t b = Serial1.read();
    rx_buffer[rx_idx++] = b;
    
    // Check for sync pattern at start
    if (rx_idx == 4) {
      if (memcmp(rx_buffer, SYNC_PATTERN, 4) != 0) {
        memmove(rx_buffer, rx_buffer + 1, 3);
        rx_idx = 3;
      }
    }
    
    // ACK packet: SYNC(4) + CMD(1) + SEQ(1) = 6 bytes
    if (rx_idx >= 6) {
      if (rx_buffer[4] == ACK_CMD && rx_buffer[5] == current_seq) {
        rx_idx = 0;
        return true;
      }
      memmove(rx_buffer, rx_buffer + 1, rx_idx - 1);
      rx_idx--;
    }
  }
  return false;
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println();
  Serial.println("╔═══════════════════════════════════════════════════════════════════════════════════════════════╗");
  Serial.println("║              CPU UART BAUD RATE TEST (Small Packets: 512B, 1KB, 2KB)                          ║");
  Serial.println("╠═══════════════════════════════════════════════════════════════════════════════════════════════╣");
  Serial.println("║  TX: GPIO12  ->  GPU RX: GPIO13                                                               ║");
  Serial.println("║  RX: GPIO11  <-  GPU TX: GPIO12                                                               ║");
  Serial.println("║  30 packets per size per baud rate                                                            ║");
  Serial.println("╚═══════════════════════════════════════════════════════════════════════════════════════════════╝");
  Serial.println();
  Serial.println(">>> Make sure GPU is running GPU_BaudTest firmware! <<<");
  Serial.println();
  
  // Initialize results
  for (int i = 0; i < NUM_BAUDS; i++) {
    results[i].baud = BAUD_RATES[i];
    results[i].p512 = {0, 0, 0};
    results[i].p1k = {0, 0, 0};
    results[i].p2k = {0, 0, 0};
  }
  
  // Start first test
  uint32_t baud = BAUD_RATES[0];
  results[0].baud = baud;
  Serial.printf("\n[TEST] %lu Mbps\n", baud / 1000000);
  Serial.println("════════════════════════════════════════");
  switchBaud(baud);
  delay(100);
}

void loop() {
  static uint32_t last_send = 0;
  uint32_t now = millis();
  
  // Waiting for ACK
  if (waiting_for_ack) {
    if (checkForAck()) {
      waiting_for_ack = false;
      packets_acked++;
      if (packets_acked % 10 == 0) {
        Serial.printf("  %s: %d/%d\n", getSizeName(), packets_acked, packets_sent);
      }
    } else if (now - send_time > ACK_TIMEOUT_MS) {
      // Timeout
      waiting_for_ack = false;
      rx_idx = 0;
      Serial.print("x");
    }
    return;
  }
  
  // Check if current size test is complete
  if (packets_sent >= PACKETS_PER_SIZE) {
    startNextTest();
    return;
  }
  
  // Send next packet
  if (now - last_send >= 5) {
    sendPacket();
    last_send = now;
  }
}
