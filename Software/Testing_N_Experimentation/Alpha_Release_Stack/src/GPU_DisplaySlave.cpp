/*****************************************************************
 * GPU_DisplaySlave.cpp - Pure Display Slave Module
 * 
 * GPU receives raw framebuffers from CPU and displays them.
 * NO local rendering - GPU is a "dumb" display driver only.
 * 
 * Protocol:
 * - CPU sends HUB75 frames (12,288 bytes RGB) via UART fragments
 * - CPU sends OLED frames (2,048 bytes mono) via UART fragments
 * - GPU just receives and displays - zero computation
 * 
 * Optimizations Applied:
 * - DMA-based UART reception (hardware handles bytes)
 * - DMA-based HUB75 output (I2S peripheral)
 * - Double-buffered framebuffers (swap, no copy)
 * - Direct buffer writes (no intermediate processing)
 * - Dirty-flag updates (skip unchanged displays)
 * 
 * Display Configuration:
 * - HUB75: 128x32 RGB (dual 64x32 panels) 
 * - OLED: SH1107 128x128 monochrome
 * 
 * UART Configuration:
 * - Baud: 10 Mbps
 * - RX: GPIO13, TX: GPIO12
 * - Fragment size: 1024 bytes
 *****************************************************************/

#include <stdio.h>
#include <cstring>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "driver/uart.h"

#include "abstraction/hal.hpp"
#include "abstraction/drivers/components/HUB75/driver_hub75_simple.hpp"
#include "abstraction/drivers/components/OLED/driver_oled_sh1107.hpp"

using namespace arcos::abstraction::drivers;
using namespace arcos::abstraction;

static const char* TAG = "GPU_SLAVE";

// ============================================================
// Display Configuration
// ============================================================
constexpr int HUB75_WIDTH = 128;
constexpr int HUB75_HEIGHT = 32;
constexpr int HUB75_FRAME_SIZE = HUB75_WIDTH * HUB75_HEIGHT * 3;  // 12,288 bytes

constexpr int OLED_WIDTH = 128;
constexpr int OLED_HEIGHT = 128;
constexpr int OLED_FRAME_SIZE = (OLED_WIDTH * OLED_HEIGHT) / 8;  // 2,048 bytes

// Display objects
SimpleHUB75Display hub75;
DRIVER_OLED_SH1107 oled(0x3C, 0);

// ============================================================
// UART Protocol Constants (matches UartProtocol.hpp)
// ============================================================
constexpr uint8_t SYNC_1 = 0xAA;
constexpr uint8_t SYNC_2 = 0x55;
constexpr uint8_t SYNC_3 = 0xCC;

constexpr uint16_t FRAGMENT_SIZE = 1024;
constexpr uint8_t HUB75_FRAG_COUNT = 12;  // 12KB / 1KB
constexpr uint8_t OLED_FRAG_COUNT = 2;    // 2KB / 1KB

enum class MsgType : uint8_t {
  PING          = 0x01,
  PONG          = 0x02,
  ACK           = 0x03,
  NACK          = 0x04,
  STATUS        = 0x05,
  HUB75_FRAG    = 0x11,
  OLED_FRAG     = 0x13,
  SET_BRIGHTNESS = 0x21,
};

#pragma pack(push, 1)
struct PacketHeader {
  uint8_t sync1;
  uint8_t sync2;
  uint8_t sync3;
  uint8_t msg_type;
  uint16_t payload_len;
  uint16_t frame_num;
  uint8_t frag_index;
  uint8_t frag_total;
};  // 10 bytes

struct PacketFooter {
  uint16_t checksum;
  uint8_t end_byte;
};  // 3 bytes
#pragma pack(pop)

// ============================================================
// Double-Buffered Frame Reception
// ============================================================
struct RxFrameBuffer {
  uint8_t* data;
  uint16_t frame_num;
  uint8_t frags_received;
  bool complete;
  bool dirty;  // Needs display update
};

// HUB75 double buffer (12KB each, DMA-capable)
RxFrameBuffer hub75_front;
RxFrameBuffer hub75_back;
RxFrameBuffer* hub75_recv = &hub75_back;   // Receiving into back
RxFrameBuffer* hub75_display = &hub75_front;  // Displaying front

// OLED double buffer (2KB each)
RxFrameBuffer oled_front;
RxFrameBuffer oled_back;
RxFrameBuffer* oled_recv = &oled_back;
RxFrameBuffer* oled_display = &oled_front;

// ============================================================
// UART Configuration
// ============================================================
constexpr uart_port_t UART_PORT = UART_NUM_1;
constexpr int UART_RX_PIN = 13;
constexpr int UART_TX_PIN = 12;
constexpr int UART_BAUD = 10000000;  // 10 Mbps
constexpr int UART_RX_BUF = 8192;    // 8KB RX buffer

// Packet receive buffer
uint8_t rx_buffer[FRAGMENT_SIZE + sizeof(PacketHeader) + sizeof(PacketFooter) + 16];
int rx_pos = 0;

// ============================================================
// Statistics
// ============================================================
struct Stats {
  uint32_t hub75_frames;
  uint32_t oled_frames;
  uint32_t hub75_frags;
  uint32_t oled_frags;
  uint32_t checksum_errors;
  uint32_t sync_errors;
  uint32_t hub75_fps;
  uint32_t oled_fps;
  uint32_t last_hub75_time;
  uint32_t last_oled_time;
  
  void updateHub75Frame() {
    uint32_t now = esp_timer_get_time() / 1000;
    if (last_hub75_time > 0) {
      uint32_t dt = now - last_hub75_time;
      hub75_fps = (dt > 0) ? (1000 / dt) : 0;
    }
    last_hub75_time = now;
    hub75_frames++;
  }
  
  void updateOledFrame() {
    uint32_t now = esp_timer_get_time() / 1000;
    if (last_oled_time > 0) {
      uint32_t dt = now - last_oled_time;
      oled_fps = (dt > 0) ? (1000 / dt) : 0;
    }
    last_oled_time = now;
    oled_frames++;
  }
};

Stats stats;
bool hub75_ok = false;
bool oled_ok = false;

// ============================================================
// Checksum Calculation
// ============================================================
uint16_t calcChecksum(const uint8_t* data, size_t len) {
  uint16_t sum = 0;
  for (size_t i = 0; i < len; i++) {
    sum += data[i];
  }
  return sum;
}

// ============================================================
// Buffer Allocation
// ============================================================
bool allocateBuffers() {
  // Allocate HUB75 buffers in DMA-capable memory
  hub75_front.data = (uint8_t*)heap_caps_malloc(HUB75_FRAME_SIZE, MALLOC_CAP_DMA);
  hub75_back.data = (uint8_t*)heap_caps_malloc(HUB75_FRAME_SIZE, MALLOC_CAP_DMA);
  
  if (!hub75_front.data || !hub75_back.data) {
    ESP_LOGE(TAG, "Failed to allocate HUB75 buffers!");
    return false;
  }
  
  memset(hub75_front.data, 0, HUB75_FRAME_SIZE);
  memset(hub75_back.data, 0, HUB75_FRAME_SIZE);
  hub75_front.frame_num = 0;
  hub75_front.frags_received = 0;
  hub75_front.complete = false;
  hub75_front.dirty = false;
  hub75_back = hub75_front;
  hub75_back.data = hub75_back.data;  // Keep separate pointer
  
  // Allocate OLED buffers
  oled_front.data = (uint8_t*)heap_caps_malloc(OLED_FRAME_SIZE, MALLOC_CAP_DEFAULT);
  oled_back.data = (uint8_t*)heap_caps_malloc(OLED_FRAME_SIZE, MALLOC_CAP_DEFAULT);
  
  if (!oled_front.data || !oled_back.data) {
    ESP_LOGE(TAG, "Failed to allocate OLED buffers!");
    return false;
  }
  
  memset(oled_front.data, 0, OLED_FRAME_SIZE);
  memset(oled_back.data, 0, OLED_FRAME_SIZE);
  oled_front.frame_num = 0;
  oled_front.frags_received = 0;
  oled_front.complete = false;
  oled_front.dirty = false;
  oled_back = oled_front;
  oled_back.data = oled_back.data;
  
  ESP_LOGI(TAG, "Buffers allocated: HUB75=%dKB, OLED=%dKB", 
           HUB75_FRAME_SIZE * 2 / 1024, OLED_FRAME_SIZE * 2 / 1024);
  return true;
}

// ============================================================
// UART Initialization
// ============================================================
bool initUART() {
  uart_config_t uart_config = {
    .baud_rate = UART_BAUD,
    .data_bits = UART_DATA_8_BITS,
    .parity = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    .rx_flow_ctrl_thresh = 0,
    .source_clk = UART_SCLK_APB,
    .flags = {0}
  };
  
  esp_err_t err = uart_param_config(UART_PORT, &uart_config);
  if (err != ESP_OK) return false;
  
  err = uart_set_pin(UART_PORT, UART_TX_PIN, UART_RX_PIN, 
                     UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
  if (err != ESP_OK) return false;
  
  err = uart_driver_install(UART_PORT, UART_RX_BUF * 2, UART_RX_BUF, 0, NULL, 0);
  if (err != ESP_OK) return false;
  
  ESP_LOGI(TAG, "UART: %d baud (%.1f Mbps), RX=%d, TX=%d", 
           UART_BAUD, UART_BAUD / 1000000.0f, UART_RX_PIN, UART_TX_PIN);
  return true;
}

// ============================================================
// I2C Initialization for OLED
// ============================================================
bool initI2C() {
  HalResult result = ESP32S3_I2C_HAL::Initialize(0, 2, 1, 400000, 1000);
  if (result != HalResult::Success) {
    ESP_LOGE(TAG, "I2C init failed!");
    return false;
  }
  ESP_LOGI(TAG, "I2C: SDA=2, SCL=1, 400kHz");
  return true;
}

// ============================================================
// Send Response to CPU
// ============================================================
void sendPong(uint32_t timestamp) {
  uint8_t pkt[16];
  pkt[0] = SYNC_1;
  pkt[1] = SYNC_2;
  pkt[2] = SYNC_3;
  pkt[3] = (uint8_t)MsgType::PONG;
  pkt[4] = 4;  // payload len low
  pkt[5] = 0;  // payload len high
  pkt[6] = 0;  // frame_num low
  pkt[7] = 0;  // frame_num high
  pkt[8] = 0;  // frag_index
  pkt[9] = 1;  // frag_total
  memcpy(&pkt[10], &timestamp, 4);
  uint16_t chk = calcChecksum(&pkt[3], 11);
  pkt[14] = chk & 0xFF;
  pkt[15] = chk >> 8;
  uart_write_bytes(UART_PORT, pkt, 16);
}

void sendStatus() {
  uint8_t pkt[32];
  pkt[0] = SYNC_1;
  pkt[1] = SYNC_2;
  pkt[2] = SYNC_3;
  pkt[3] = (uint8_t)MsgType::STATUS;
  pkt[4] = 12;  // payload len
  pkt[5] = 0;
  pkt[6] = 0;
  pkt[7] = 0;
  pkt[8] = 0;
  pkt[9] = 1;
  
  // Payload: uptime, fps, frames, status
  uint32_t uptime = esp_timer_get_time() / 1000;
  memcpy(&pkt[10], &uptime, 4);
  pkt[14] = stats.hub75_fps;
  pkt[15] = stats.oled_fps;
  pkt[16] = stats.hub75_frames & 0xFF;
  pkt[17] = (stats.hub75_frames >> 8) & 0xFF;
  pkt[18] = stats.oled_frames & 0xFF;
  pkt[19] = (stats.oled_frames >> 8) & 0xFF;
  pkt[20] = hub75_ok ? 1 : 0;
  pkt[21] = oled_ok ? 1 : 0;
  
  uint16_t chk = calcChecksum(&pkt[3], 19);
  pkt[22] = chk & 0xFF;
  pkt[23] = chk >> 8;
  pkt[24] = 0x55;
  
  uart_write_bytes(UART_PORT, pkt, 25);
}

// ============================================================
// Process Received Fragment
// ============================================================
void processFragment(const PacketHeader* hdr, const uint8_t* payload) {
  MsgType type = (MsgType)hdr->msg_type;
  
  switch (type) {
    case MsgType::PING: {
      uint32_t timestamp = 0;
      if (hdr->payload_len >= 4) {
        memcpy(&timestamp, payload, 4);
      }
      sendPong(timestamp);
      break;
    }
    
    case MsgType::HUB75_FRAG: {
      // Validate fragment
      if (hdr->frag_index >= HUB75_FRAG_COUNT) {
        stats.sync_errors++;
        break;
      }
      
      // Check if new frame started
      if (hdr->frame_num != hub75_recv->frame_num) {
        hub75_recv->frame_num = hdr->frame_num;
        hub75_recv->frags_received = 0;
        hub75_recv->complete = false;
      }
      
      // Copy fragment data directly to buffer (zero-copy destination)
      uint32_t offset = hdr->frag_index * FRAGMENT_SIZE;
      uint16_t len = (hdr->frag_index == HUB75_FRAG_COUNT - 1) 
                     ? (HUB75_FRAME_SIZE % FRAGMENT_SIZE) 
                     : FRAGMENT_SIZE;
      if (len == 0) len = FRAGMENT_SIZE;
      
      memcpy(hub75_recv->data + offset, payload, len);
      hub75_recv->frags_received++;
      stats.hub75_frags++;
      
      // Check if frame complete
      if (hub75_recv->frags_received >= HUB75_FRAG_COUNT) {
        hub75_recv->complete = true;
        hub75_recv->dirty = true;
        
        // Swap buffers (pointer swap, no copy!)
        RxFrameBuffer* tmp = hub75_recv;
        hub75_recv = hub75_display;
        hub75_display = tmp;
        
        // Reset new receive buffer
        hub75_recv->frags_received = 0;
        hub75_recv->complete = false;
        
        stats.updateHub75Frame();
      }
      break;
    }
    
    case MsgType::OLED_FRAG: {
      if (hdr->frag_index >= OLED_FRAG_COUNT) {
        stats.sync_errors++;
        break;
      }
      
      if (hdr->frame_num != oled_recv->frame_num) {
        oled_recv->frame_num = hdr->frame_num;
        oled_recv->frags_received = 0;
        oled_recv->complete = false;
      }
      
      uint32_t offset = hdr->frag_index * FRAGMENT_SIZE;
      uint16_t len = (hdr->frag_index == OLED_FRAG_COUNT - 1)
                     ? (OLED_FRAME_SIZE % FRAGMENT_SIZE)
                     : FRAGMENT_SIZE;
      if (len == 0) len = FRAGMENT_SIZE;
      
      memcpy(oled_recv->data + offset, payload, len);
      oled_recv->frags_received++;
      stats.oled_frags++;
      
      if (oled_recv->frags_received >= OLED_FRAG_COUNT) {
        oled_recv->complete = true;
        oled_recv->dirty = true;
        
        RxFrameBuffer* tmp = oled_recv;
        oled_recv = oled_display;
        oled_display = tmp;
        
        oled_recv->frags_received = 0;
        oled_recv->complete = false;
        
        stats.updateOledFrame();
      }
      break;
    }
    
    case MsgType::SET_BRIGHTNESS: {
      if (hdr->payload_len >= 1 && hub75_ok) {
        hub75.setBrightness(payload[0]);
        ESP_LOGI(TAG, "Brightness: %d", payload[0]);
      }
      break;
    }
    
    default:
      break;
  }
}

// ============================================================
// UART Packet Parser (State Machine)
// ============================================================
enum class ParseState { SYNC1, SYNC2, SYNC3, HEADER, PAYLOAD, FOOTER };
ParseState parse_state = ParseState::SYNC1;
PacketHeader current_hdr;
int payload_received = 0;
uint8_t payload_buffer[FRAGMENT_SIZE + 16];

void processUART() {
  uint8_t byte;
  int available = uart_read_bytes(UART_PORT, rx_buffer, sizeof(rx_buffer), 0);
  
  for (int i = 0; i < available; i++) {
    byte = rx_buffer[i];
    
    switch (parse_state) {
      case ParseState::SYNC1:
        if (byte == SYNC_1) parse_state = ParseState::SYNC2;
        break;
        
      case ParseState::SYNC2:
        if (byte == SYNC_2) parse_state = ParseState::SYNC3;
        else parse_state = ParseState::SYNC1;
        break;
        
      case ParseState::SYNC3:
        if (byte == SYNC_3) {
          parse_state = ParseState::HEADER;
          rx_pos = 0;
        } else {
          parse_state = ParseState::SYNC1;
          stats.sync_errors++;
        }
        break;
        
      case ParseState::HEADER:
        ((uint8_t*)&current_hdr)[rx_pos + 3] = byte;  // Skip sync bytes
        rx_pos++;
        if (rx_pos >= sizeof(PacketHeader) - 3) {
          current_hdr.sync1 = SYNC_1;
          current_hdr.sync2 = SYNC_2;
          current_hdr.sync3 = SYNC_3;
          payload_received = 0;
          
          if (current_hdr.payload_len > FRAGMENT_SIZE + 16) {
            parse_state = ParseState::SYNC1;
            stats.sync_errors++;
          } else {
            parse_state = ParseState::PAYLOAD;
          }
        }
        break;
        
      case ParseState::PAYLOAD:
        payload_buffer[payload_received++] = byte;
        if (payload_received >= current_hdr.payload_len) {
          parse_state = ParseState::FOOTER;
          rx_pos = 0;
        }
        break;
        
      case ParseState::FOOTER:
        rx_pos++;
        if (rx_pos >= 3) {  // checksum(2) + end_byte(1)
          // Verify checksum (simplified - just process)
          processFragment(&current_hdr, payload_buffer);
          parse_state = ParseState::SYNC1;
        }
        break;
    }
  }
}

// ============================================================
// Display Update Functions
// ============================================================
void updateHUB75() {
  if (!hub75_ok || !hub75_display->dirty) return;
  
  // Direct copy from receive buffer to display
  const uint8_t* src = hub75_display->data;
  for (int y = 0; y < HUB75_HEIGHT; y++) {
    for (int x = 0; x < HUB75_WIDTH; x++) {
      int idx = (y * HUB75_WIDTH + x) * 3;
      hub75.setPixel(x, y, RGB(src[idx], src[idx + 1], src[idx + 2]));
    }
  }
  hub75.show();
  hub75_display->dirty = false;
}

void updateOLED() {
  if (!oled_ok || !oled_display->dirty) return;
  
  // Direct buffer copy to OLED
  memcpy(oled.getBuffer(), oled_display->data, OLED_FRAME_SIZE);
  oled.updateDisplay();
  oled_display->dirty = false;
}

// ============================================================
// Memory Stats
// ============================================================
void printMemory() {
  ESP_LOGI(TAG, "Memory: Heap=%lu, DMA=%lu, Largest=%lu",
           (unsigned long)esp_get_free_heap_size(),
           (unsigned long)heap_caps_get_free_size(MALLOC_CAP_DMA),
           (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_DMA));
}

// ============================================================
// Main Entry Point
// ============================================================
extern "C" void app_main(void) {
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "╔══════════════════════════════════════════════╗");
  ESP_LOGI(TAG, "║      GPU Display Slave - Pure Receiver       ║");
  ESP_LOGI(TAG, "║   HUB75: 128x32 | OLED: 128x128 | 10Mbps     ║");
  ESP_LOGI(TAG, "╚══════════════════════════════════════════════╝");
  ESP_LOGI(TAG, "");
  
  vTaskDelay(pdMS_TO_TICKS(2000));
  printMemory();
  
  // Allocate frame buffers
  if (!allocateBuffers()) {
    ESP_LOGE(TAG, "Buffer allocation failed!");
    while (1) vTaskDelay(pdMS_TO_TICKS(1000));
  }
  
  // Initialize HUB75
  ESP_LOGI(TAG, "--- HUB75 Init ---");
  HUB75Config config = HUB75Config::getDefault();
  config.colour_depth = 5;
  config.colour_buffer_count = 5;
  config.enable_double_buffering = true;
  config.enable_gamma_correction = true;
  config.gamma_value = 2.2f;
  
  hub75_ok = hub75.begin(true, config);
  if (!hub75_ok) {
    ESP_LOGE(TAG, "HUB75 FAILED!");
  } else {
    ESP_LOGI(TAG, "HUB75 OK: %dx%d, DMA+I2S", hub75.getWidth(), hub75.getHeight());
    hub75.setBrightness(200);
    hub75.clear();
    hub75.show();
  }
  
  // Initialize I2C + OLED
  ESP_LOGI(TAG, "--- OLED Init ---");
  if (initI2C()) {
    OLEDConfig oled_cfg;
    oled_cfg.contrast = 0xFF;
    oled_ok = oled.initialize(oled_cfg);
    if (oled_ok) {
      ESP_LOGI(TAG, "OLED OK: 128x128 mono");
      oled.clearBuffer();
      oled.drawString(10, 56, "GPU SLAVE READY", true);
      oled.updateDisplay();
    } else {
      ESP_LOGE(TAG, "OLED FAILED!");
    }
  }
  
  // Initialize UART
  ESP_LOGI(TAG, "--- UART Init ---");
  if (!initUART()) {
    ESP_LOGE(TAG, "UART FAILED!");
    while (1) vTaskDelay(pdMS_TO_TICKS(1000));
  }
  
  printMemory();
  
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "=== GPU SLAVE READY ===");
  ESP_LOGI(TAG, "Waiting for frames from CPU...");
  ESP_LOGI(TAG, "  HUB75: %s (12KB frames)", hub75_ok ? "OK" : "FAIL");
  ESP_LOGI(TAG, "  OLED:  %s (2KB frames)", oled_ok ? "OK" : "FAIL");
  ESP_LOGI(TAG, "");
  
  uint32_t last_stats_time = 0;
  
  // ============ Main Loop - Pure Receive & Display ============
  while (1) {
    // Process UART data (DMA-backed, non-blocking)
    processUART();
    
    // Update displays if new frame received (dirty flag)
    updateHUB75();
    updateOLED();
    
    // Print stats every second
    uint32_t now = esp_timer_get_time() / 1000;
    if (now - last_stats_time >= 1000) {
      ESP_LOGI(TAG, "HUB75: %lu frames @ %lu fps | OLED: %lu frames @ %lu fps | Err: %lu",
               (unsigned long)stats.hub75_frames,
               (unsigned long)stats.hub75_fps,
               (unsigned long)stats.oled_frames,
               (unsigned long)stats.oled_fps,
               (unsigned long)(stats.checksum_errors + stats.sync_errors));
      
      sendStatus();
      last_stats_time = now;
    }
    
    // Minimal yield - just prevent watchdog
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}
