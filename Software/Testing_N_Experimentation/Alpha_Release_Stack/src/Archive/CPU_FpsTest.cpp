/*****************************************************************
 * File:      CPU_FpsTest.cpp
 * Purpose:   Test maximum achievable FPS for HUB75 at various configs
 * 
 * Tests:
 *   - Different baud rates (4-15 Mbps)
 *   - Different fragment sizes (1KB, 2KB, 4KB, full 12KB)
 *   - Streaming mode (no ACK wait) vs ACK mode
 *   - Measures actual frames received by GPU
 *****************************************************************/

#include <Arduino.h>
#include "Comms/UartProtocol.hpp"

using namespace arcos::comms;

// Test configuration
constexpr uint32_t TEST_DURATION_MS = 5000;  // 5 seconds per test
constexpr uint32_t BAUD_RATES[] = {4000000, 6000000, 8000000, 10000000, 12000000, 15000000};
constexpr uint32_t NUM_BAUD_RATES = sizeof(BAUD_RATES) / sizeof(BAUD_RATES[0]);

// Fragment sizes to test
constexpr uint16_t FRAG_SIZES[] = {1024, 2048, 4096, 12288};  // 1KB, 2KB, 4KB, full frame
constexpr uint32_t NUM_FRAG_SIZES = sizeof(FRAG_SIZES) / sizeof(FRAG_SIZES[0]);

// Frame buffer
uint8_t frame_buffer[HUB75_RGB_SIZE];

// Statistics
struct TestResult {
  uint32_t baud_rate;
  uint16_t frag_size;
  uint32_t frames_sent;
  uint32_t frames_acked;
  uint32_t checksum_errors;
  uint32_t timeouts;
  float actual_fps;
  float success_rate;
};

TestResult results[NUM_BAUD_RATES * NUM_FRAG_SIZES];
uint32_t result_count = 0;

// Current test state
uint32_t current_baud = 0;
uint16_t current_frag_size = 0;
uint32_t frames_sent = 0;
uint32_t frames_acked = 0;
uint32_t checksum_errors = 0;
uint32_t timeouts = 0;
uint32_t test_start_time = 0;
uint16_t frame_num = 0;

// Generate test pattern
void generateTestPattern(uint16_t frame) {
  for (int i = 0; i < HUB75_RGB_SIZE; i++) {
    frame_buffer[i] = (i + frame) & 0xFF;
  }
}

// Calculate checksum
uint16_t calcChecksum(const uint8_t* data, size_t len) {
  uint16_t sum = 0;
  for (size_t i = 0; i < len; i++) {
    sum += data[i];
  }
  return sum;
}

// Send a frame with given fragment size (streaming mode - no ACK wait)
bool sendFrameFragmented(uint16_t frag_size) {
  uint8_t frag_count = (HUB75_RGB_SIZE + frag_size - 1) / frag_size;
  
  for (uint8_t frag = 0; frag < frag_count; frag++) {
    uint32_t offset = frag * frag_size;
    uint16_t len = min((uint32_t)frag_size, HUB75_RGB_SIZE - offset);
    
    // Build header
    PacketHeader hdr;
    hdr.sync1 = SYNC_BYTE_1;
    hdr.sync2 = SYNC_BYTE_2;
    hdr.sync3 = SYNC_BYTE_3;
    hdr.msg_type = static_cast<uint8_t>(MsgType::HUB75_FRAG);
    hdr.payload_len = len;
    hdr.frame_num = frame_num;
    hdr.frag_index = frag;
    hdr.frag_total = frag_count;
    
    // Calculate checksum
    uint16_t checksum = calcChecksum((uint8_t*)&hdr, sizeof(hdr));
    checksum += calcChecksum(frame_buffer + offset, len);
    
    PacketFooter ftr;
    ftr.checksum = checksum;
    ftr.end_byte = SYNC_BYTE_2;
    
    // Send
    Serial1.write((uint8_t*)&hdr, sizeof(hdr));
    Serial1.write(frame_buffer + offset, len);
    Serial1.write((uint8_t*)&ftr, sizeof(ftr));
  }
  
  Serial1.flush();
  return true;
}

// Send full frame (no fragmentation)
bool sendFrameFull() {
  PacketHeader hdr;
  hdr.sync1 = SYNC_BYTE_1;
  hdr.sync2 = SYNC_BYTE_2;
  hdr.sync3 = SYNC_BYTE_3;
  hdr.msg_type = static_cast<uint8_t>(MsgType::HUB75_FRAME);
  hdr.payload_len = HUB75_RGB_SIZE;
  hdr.frame_num = frame_num;
  hdr.frag_index = 0;
  hdr.frag_total = 1;
  
  uint16_t checksum = calcChecksum((uint8_t*)&hdr, sizeof(hdr));
  checksum += calcChecksum(frame_buffer, HUB75_RGB_SIZE);
  
  PacketFooter ftr;
  ftr.checksum = checksum;
  ftr.end_byte = SYNC_BYTE_2;
  
  Serial1.write((uint8_t*)&hdr, sizeof(hdr));
  Serial1.write(frame_buffer, HUB75_RGB_SIZE);
  Serial1.write((uint8_t*)&ftr, sizeof(ftr));
  Serial1.flush();
  
  return true;
}

// Process incoming ACKs
void processResponses() {
  while (Serial1.available() >= (int)sizeof(PacketHeader)) {
    if (Serial1.peek() != SYNC_BYTE_1) {
      Serial1.read();
      continue;
    }
    
    PacketHeader hdr;
    Serial1.readBytes((uint8_t*)&hdr, sizeof(hdr));
    
    if (hdr.sync1 != SYNC_BYTE_1 || hdr.sync2 != SYNC_BYTE_2 || hdr.sync3 != SYNC_BYTE_3) {
      continue;
    }
    
    // Read payload
    uint8_t payload[64];
    if (hdr.payload_len > 0 && hdr.payload_len <= sizeof(payload)) {
      Serial1.readBytes(payload, hdr.payload_len);
    }
    
    // Read footer
    PacketFooter ftr;
    Serial1.readBytes((uint8_t*)&ftr, sizeof(ftr));
    
    MsgType type = static_cast<MsgType>(hdr.msg_type);
    if (type == MsgType::ACK) {
      frames_acked++;
    } else if (type == MsgType::NACK) {
      checksum_errors++;
    }
  }
}

// Run single test
void runTest(uint32_t baud, uint16_t frag_size) {
  Serial.printf("\n[TEST] %lu bps, %u byte fragments\n", baud, frag_size);
  
  // Reinitialize UART
  Serial1.end();
  delay(50);
  Serial1.begin(baud, SERIAL_8N1, 11, 12);
  Serial1.setRxBufferSize(4096);
  delay(100);
  
  // Clear buffers
  while (Serial1.available()) Serial1.read();
  
  // Reset stats
  current_baud = baud;
  current_frag_size = frag_size;
  frames_sent = 0;
  frames_acked = 0;
  checksum_errors = 0;
  timeouts = 0;
  frame_num = 0;
  
  test_start_time = millis();
  uint32_t last_print = test_start_time;
  
  // Run test for TEST_DURATION_MS
  while (millis() - test_start_time < TEST_DURATION_MS) {
    // Generate and send frame
    generateTestPattern(frame_num);
    
    if (frag_size >= HUB75_RGB_SIZE) {
      sendFrameFull();
    } else {
      sendFrameFragmented(frag_size);
    }
    
    frames_sent++;
    frame_num++;
    
    // Process any responses
    processResponses();
    
    // Print progress every second
    if (millis() - last_print >= 1000) {
      uint32_t elapsed = millis() - test_start_time;
      float fps = (float)frames_sent * 1000.0f / elapsed;
      Serial.printf("  %lu frames sent, %.1f fps\n", frames_sent, fps);
      last_print = millis();
    }
    
    // Small delay to not overwhelm GPU
    // At 60fps we need 16.67ms per frame
    // At 10 Mbps, 12KB takes ~10ms to transmit
    // So we need ~7ms margin
    delayMicroseconds(500);  // 0.5ms gap between frames
  }
  
  // Final response check
  delay(100);
  processResponses();
  
  // Calculate results
  uint32_t elapsed = millis() - test_start_time;
  float actual_fps = (float)frames_sent * 1000.0f / elapsed;
  float success_rate = (frames_sent > 0) ? ((float)frames_acked / frames_sent * 100.0f) : 0;
  
  // Store result
  TestResult& r = results[result_count++];
  r.baud_rate = baud;
  r.frag_size = frag_size;
  r.frames_sent = frames_sent;
  r.frames_acked = frames_acked;
  r.checksum_errors = checksum_errors;
  r.timeouts = timeouts;
  r.actual_fps = actual_fps;
  r.success_rate = success_rate;
  
  Serial.printf("  Result: %lu sent, %lu acked (%.1f%%), %.1f fps\n",
                frames_sent, frames_acked, success_rate, actual_fps);
}

void printResults() {
  Serial.println("\n");
  Serial.println("╔════════════════════════════════════════════════════════════════════════════════╗");
  Serial.println("║                    HUB75 FPS TEST RESULTS (Streaming Mode)                     ║");
  Serial.println("╠══════════════╦════════════╦════════════╦════════════╦═══════════╦═════════════╣");
  Serial.println("║   Baud Rate  ║ Frag Size  ║ Frames TX  ║ Frames OK  ║    FPS    ║  Success %  ║");
  Serial.println("╠══════════════╬════════════╬════════════╬════════════╬═══════════╬═════════════╣");
  
  for (uint32_t i = 0; i < result_count; i++) {
    TestResult& r = results[i];
    const char* frag_str;
    if (r.frag_size == 1024) frag_str = "1KB";
    else if (r.frag_size == 2048) frag_str = "2KB";
    else if (r.frag_size == 4096) frag_str = "4KB";
    else frag_str = "FULL";
    
    Serial.printf("║  %3lu Mbps    ║    %4s    ║   %6lu   ║   %6lu   ║   %5.1f   ║    %5.1f%%   ║\n",
                  r.baud_rate / 1000000, frag_str, r.frames_sent, r.frames_acked, r.actual_fps, r.success_rate);
  }
  
  Serial.println("╚══════════════╩════════════╩════════════╩════════════╩═══════════╩═════════════╝");
  
  // Find best config for 60fps
  Serial.println("\n═══════════════════════════════════════════════════════════════");
  Serial.println("                BEST CONFIGURATIONS FOR 60+ FPS");
  Serial.println("═══════════════════════════════════════════════════════════════");
  
  for (uint32_t i = 0; i < result_count; i++) {
    TestResult& r = results[i];
    if (r.actual_fps >= 60.0f && r.success_rate >= 95.0f) {
      const char* frag_str;
      if (r.frag_size == 1024) frag_str = "1KB";
      else if (r.frag_size == 2048) frag_str = "2KB";
      else if (r.frag_size == 4096) frag_str = "4KB";
      else frag_str = "FULL";
      
      Serial.printf("  ✓ %lu Mbps + %s = %.1f fps (%.1f%% success)\n",
                    r.baud_rate / 1000000, frag_str, r.actual_fps, r.success_rate);
    }
  }
  Serial.println("═══════════════════════════════════════════════════════════════\n");
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("\n\n");
  Serial.println("╔════════════════════════════════════════════════════════════╗");
  Serial.println("║     HUB75 Maximum FPS Test - CPU Side (Streaming Mode)     ║");
  Serial.println("║     Testing various baud rates and fragment sizes          ║");
  Serial.println("╚════════════════════════════════════════════════════════════╝");
  Serial.println();
  
  // Initialize frame buffer with test pattern
  generateTestPattern(0);
  
  // Run all tests
  for (uint32_t b = 0; b < NUM_BAUD_RATES; b++) {
    for (uint32_t f = 0; f < NUM_FRAG_SIZES; f++) {
      runTest(BAUD_RATES[b], FRAG_SIZES[f]);
    }
  }
  
  // Print summary
  printResults();
  
  Serial.println("\nTest complete!");
}

void loop() {
  delay(10000);
}
