/*****************************************************************
 * File:      GpuRenderer.hpp
 * Category:  GPU Driver / GPU Side
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    GPU-side renderer that receives commands from CPU and
 *    renders to HUB75 and OLED displays.
 * 
 * Features:
 *    - Command parsing and execution
 *    - Double-buffered rendering
 *    - Sprite management
 *    - Animation system
 *    - Script interpreter
 *    - Effect engine
 * 
 * Usage (in GPU main):
 *    GpuRenderer renderer;
 *    renderer.init();
 *    
 *    while(1) {
 *      renderer.processCommands();
 *      renderer.update();
 *      renderer.render();
 *      vTaskDelay(1);
 *    }
 *****************************************************************/

#ifndef GPU_RENDERER_HPP_
#define GPU_RENDERER_HPP_

// ESP-IDF includes
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "GpuBaseAPI.hpp"

namespace gpu {

static const char* TAG = "GpuRenderer";

// ============================================================
// Sprite Structure
// ============================================================

struct Sprite {
  bool loaded;
  uint8_t* data;
  uint16_t width;
  uint16_t height;
  uint8_t frames;
  ColorFormat format;
  uint32_t data_size;
};

// ============================================================
// Animation Structure
// ============================================================

struct Animation {
  bool active;
  uint8_t sprite_id;
  int16_t x, y;
  uint8_t current_frame;
  uint8_t start_frame;
  uint8_t end_frame;
  uint16_t frame_delay_ms;
  uint32_t last_frame_time;
  LoopMode loop_mode;
  bool paused;
  bool forward;  // For ping-pong
};

// ============================================================
// Effect State
// ============================================================

struct EffectState {
  bool active;
  EffectCmd type;
  uint32_t start_time;
  uint16_t duration_ms;
  uint8_t intensity;
  uint8_t param1, param2;
  float progress;
};

// ============================================================
// Script State
// ============================================================

struct Script {
  bool loaded;
  uint8_t* code;
  uint16_t code_len;
  bool running;
  uint16_t pc;  // Program counter
};

// ============================================================
// GpuRenderer Class
// ============================================================

class GpuRenderer {
public:
  struct Config {
    uint32_t baud_rate;
    uint8_t rx_pin;
    uint8_t tx_pin;
    uint16_t rx_buffer_size;
    
    Config() : baud_rate(GPU_BAUD_RATE), rx_pin(13), tx_pin(12), 
               rx_buffer_size(8192) {}
  };
  
  GpuRenderer() : initialized_(false), uart_num_(UART_NUM_1) {
    // Initialize sprite array
    memset(sprites_, 0, sizeof(sprites_));
    memset(animations_, 0, sizeof(animations_));
    memset(scripts_, 0, sizeof(scripts_));
    memset(&hub75_effect_, 0, sizeof(hub75_effect_));
    memset(&oled_effect_, 0, sizeof(oled_effect_));
    memset(&stats_, 0, sizeof(stats_));
  }
  
  ~GpuRenderer() {
    // Free sprite data
    for (int i = 0; i < MAX_SPRITES; i++) {
      if (sprites_[i].data) free(sprites_[i].data);
    }
    for (int i = 0; i < 8; i++) {
      if (scripts_[i].code) free(scripts_[i].code);
    }
    // Free frame buffers
    if (hub75_buffer_[0]) free(hub75_buffer_[0]);
    if (hub75_buffer_[1]) free(hub75_buffer_[1]);
    if (oled_buffer_[0]) free(oled_buffer_[0]);
    if (oled_buffer_[1]) free(oled_buffer_[1]);
  }
  
  // ============================================================
  // Initialization
  // ============================================================
  
  bool init(const Config& config = Config()) {
    config_ = config;
    
    // Configure UART
    uart_config_t uart_config = {};
    uart_config.baud_rate = config.baud_rate;
    uart_config.data_bits = UART_DATA_8_BITS;
    uart_config.parity = UART_PARITY_DISABLE;
    uart_config.stop_bits = UART_STOP_BITS_1;
    uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    uart_config.source_clk = UART_SCLK_DEFAULT;
    
    esp_err_t err = uart_driver_install(uart_num_, config.rx_buffer_size, 1024, 0, NULL, 0);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "uart_driver_install failed: %d", err);
      return false;
    }
    
    err = uart_param_config(uart_num_, &uart_config);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "uart_param_config failed: %d", err);
      return false;
    }
    
    err = uart_set_pin(uart_num_, config.tx_pin, config.rx_pin, 
                       UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "uart_set_pin failed: %d", err);
      return false;
    }
    
    // Allocate frame buffers (double-buffered)
    hub75_buffer_[0] = (uint8_t*)malloc(HUB75_WIDTH * HUB75_HEIGHT * 3);
    hub75_buffer_[1] = (uint8_t*)malloc(HUB75_WIDTH * HUB75_HEIGHT * 3);
    oled_buffer_[0] = (uint8_t*)malloc(OLED_WIDTH * OLED_HEIGHT / 8);
    oled_buffer_[1] = (uint8_t*)malloc(OLED_WIDTH * OLED_HEIGHT / 8);
    
    if (!hub75_buffer_[0] || !hub75_buffer_[1] || 
        !oled_buffer_[0] || !oled_buffer_[1]) {
      ESP_LOGE(TAG, "Failed to allocate frame buffers");
      return false;
    }
    
    // Clear buffers
    memset(hub75_buffer_[0], 0, HUB75_WIDTH * HUB75_HEIGHT * 3);
    memset(hub75_buffer_[1], 0, HUB75_WIDTH * HUB75_HEIGHT * 3);
    memset(oled_buffer_[0], 0, OLED_WIDTH * OLED_HEIGHT / 8);
    memset(oled_buffer_[1], 0, OLED_WIDTH * OLED_HEIGHT / 8);
    
    initialized_ = true;
    ESP_LOGI(TAG, "GpuRenderer initialized");
    return true;
  }
  
  // ============================================================
  // Main Loop Functions
  // ============================================================
  
  // Process incoming commands from CPU
  void processCommands(size_t max_commands = 100) {
    if (!initialized_) return;
    
    size_t commands_processed = 0;
    size_t available = 0;
    uart_get_buffered_data_len(uart_num_, &available);
    
    while (commands_processed < max_commands && 
           available >= sizeof(PacketHeader) + sizeof(PacketFooter)) {
      
      // Look for sync pattern
      uint8_t sync;
      if (uart_read_bytes(uart_num_, &sync, 1, 0) != 1) break;
      
      if (sync != SYNC_BYTE_1) {
        stats_.sync_errors++;
        uart_get_buffered_data_len(uart_num_, &available);
        continue;
      }
      
      // Read rest of sync
      uint8_t sync_rest[2];
      if (uart_read_bytes(uart_num_, sync_rest, 2, pdMS_TO_TICKS(5)) != 2) {
        stats_.sync_errors++;
        uart_get_buffered_data_len(uart_num_, &available);
        continue;
      }
      
      if (sync_rest[0] != SYNC_BYTE_2 || sync_rest[1] != SYNC_BYTE_3) {
        stats_.sync_errors++;
        uart_get_buffered_data_len(uart_num_, &available);
        continue;
      }
      
      // Read header
      PacketHeader hdr;
      hdr.sync1 = sync;
      hdr.sync2 = sync_rest[0];
      hdr.sync3 = sync_rest[1];
      
      int read = uart_read_bytes(uart_num_, ((uint8_t*)&hdr) + 3, 
                                 sizeof(hdr) - 3, pdMS_TO_TICKS(10));
      if (read != (int)(sizeof(hdr) - 3)) {
        stats_.sync_errors++;
        uart_get_buffered_data_len(uart_num_, &available);
        continue;
      }
      
      // Validate header
      if (hdr.version != PROTOCOL_VERSION) {
        ESP_LOGW(TAG, "Invalid protocol version: %d", hdr.version);
        flushBytes(hdr.payload_len + sizeof(PacketFooter));
        uart_get_buffered_data_len(uart_num_, &available);
        continue;
      }
      
      // Validate payload length
      if (hdr.payload_len > MAX_PACKET_SIZE) {
        ESP_LOGW(TAG, "Payload too large: %d", hdr.payload_len);
        flushBytes(hdr.payload_len + sizeof(PacketFooter));
        uart_get_buffered_data_len(uart_num_, &available);
        continue;
      }
      
      // Read payload
      uint8_t payload[MAX_PACKET_SIZE];
      if (hdr.payload_len > 0) {
        read = uart_read_bytes(uart_num_, payload, hdr.payload_len, pdMS_TO_TICKS(50));
        if (read != (int)hdr.payload_len) {
          stats_.checksum_errors++;
          uart_get_buffered_data_len(uart_num_, &available);
          continue;
        }
      }
      
      // Read footer
      PacketFooter ftr;
      uart_read_bytes(uart_num_, (uint8_t*)&ftr, sizeof(ftr), pdMS_TO_TICKS(5));
      
      // Validate checksum
      uint16_t calc_checksum = calculateChecksum((uint8_t*)&hdr, sizeof(hdr));
      if (hdr.payload_len > 0) {
        calc_checksum += calculateChecksum(payload, hdr.payload_len);
      }
      
      if (calc_checksum != ftr.checksum) {
        stats_.checksum_errors++;
        sendNack(hdr.seq_num);
        uart_get_buffered_data_len(uart_num_, &available);
        continue;
      }
      
      // Execute command
      executeCommand(hdr, payload);
      stats_.commands_received++;
      commands_processed++;
      
      // Send ACK
      sendAck(hdr.seq_num);
      
      uart_get_buffered_data_len(uart_num_, &available);
    }
  }
  
  // Update animations and effects
  void update() {
    if (!initialized_) return;
    
    uint32_t now = esp_timer_get_time() / 1000;
    
    // Update animations
    for (int i = 0; i < MAX_ANIMATIONS; i++) {
      if (animations_[i].active && !animations_[i].paused) {
        updateAnimation(animations_[i], now);
      }
    }
    
    // Update effects
    if (hub75_effect_.active) {
      updateEffect(hub75_effect_, now);
    }
    if (oled_effect_.active) {
      updateEffect(oled_effect_, now);
    }
    
    // Update scripts
    for (int i = 0; i < 8; i++) {
      if (scripts_[i].running) {
        executeScriptStep(scripts_[i]);
      }
    }
    
    stats_.frames_rendered++;
  }
  
  // Render frame (call after all updates)
  void render() {
    if (!initialized_) return;
    
    // Render animations to back buffer
    for (int i = 0; i < MAX_ANIMATIONS; i++) {
      if (animations_[i].active) {
        renderAnimation(animations_[i]);
      }
    }
    
    // Apply effects
    if (hub75_effect_.active) {
      applyEffect(hub75_effect_, Display::HUB75);
    }
    if (oled_effect_.active) {
      applyEffect(oled_effect_, Display::OLED);
    }
  }
  
  // ============================================================
  // Frame Buffer Access
  // ============================================================
  
  uint8_t* getHub75Buffer() { 
    return hub75_buffer_[hub75_read_idx_]; 
  }
  
  uint8_t* getOledBuffer() { 
    return oled_buffer_[oled_read_idx_]; 
  }
  
  bool isHub75Ready() const { return hub75_ready_; }
  bool isOledReady() const { return oled_ready_; }
  
  void consumeHub75() { hub75_ready_ = false; }
  void consumeOled() { oled_ready_ = false; }
  
  // ============================================================
  // Statistics
  // ============================================================
  
  struct Stats {
    uint32_t commands_received;
    uint32_t sync_errors;
    uint32_t checksum_errors;
    uint32_t frames_rendered;
    uint32_t sprites_loaded;
    uint32_t animations_active;
  };
  
  const Stats& getStats() const { return stats_; }
  
private:
  Config config_;
  bool initialized_;
  uart_port_t uart_num_;
  
  // Frame buffers (double-buffered)
  uint8_t* hub75_buffer_[2] = {nullptr, nullptr};
  uint8_t* oled_buffer_[2] = {nullptr, nullptr};
  uint8_t hub75_write_idx_ = 0;
  uint8_t hub75_read_idx_ = 0;
  uint8_t oled_write_idx_ = 0;
  uint8_t oled_read_idx_ = 0;
  bool hub75_ready_ = false;
  bool oled_ready_ = false;
  bool hub75_locked_ = false;
  bool oled_locked_ = false;
  
  // Resources
  Sprite sprites_[MAX_SPRITES];
  Animation animations_[MAX_ANIMATIONS];
  Script scripts_[8];
  EffectState hub75_effect_;
  EffectState oled_effect_;
  
  // Current drawing state
  uint8_t current_layer_ = 0;
  Rect hub75_clip_ = {0, 0, HUB75_WIDTH, HUB75_HEIGHT};
  Rect oled_clip_ = {0, 0, OLED_WIDTH, OLED_HEIGHT};
  uint8_t hub75_brightness_ = 255;
  uint8_t oled_brightness_ = 255;
  
  Stats stats_;
  
  // ============================================================
  // Command Execution
  // ============================================================
  
  void executeCommand(const PacketHeader& hdr, const uint8_t* payload) {
    Display display = static_cast<Display>(hdr.display);
    CmdCategory category = static_cast<CmdCategory>(hdr.category);
    
    switch (category) {
      case CmdCategory::SYSTEM:
        executeSystemCmd(hdr.command, display, payload, hdr.payload_len);
        break;
      case CmdCategory::DRAW:
        executeDrawCmd(hdr.command, display, payload, hdr.payload_len);
        break;
      case CmdCategory::TEXT:
        executeTextCmd(hdr.command, display, payload, hdr.payload_len);
        break;
      case CmdCategory::IMAGE:
        executeImageCmd(hdr.command, display, payload, hdr.payload_len);
        break;
      case CmdCategory::ANIMATION:
        executeAnimCmd(hdr.command, display, payload, hdr.payload_len);
        break;
      case CmdCategory::SCRIPT:
        executeScriptCmd(hdr.command, display, payload, hdr.payload_len);
        break;
      case CmdCategory::BUFFER:
        executeBufferCmd(hdr.command, display, payload, hdr.payload_len);
        break;
      case CmdCategory::EFFECT:
        executeEffectCmd(hdr.command, display, payload, hdr.payload_len);
        break;
      default:
        ESP_LOGW(TAG, "Unknown command category: 0x%02X", (uint8_t)category);
        break;
    }
  }
  
  // System commands
  void executeSystemCmd(uint8_t cmd, Display display, 
                        const uint8_t* payload, uint16_t len) {
    SysCmd sys_cmd = static_cast<SysCmd>(cmd);
    
    switch (sys_cmd) {
      case SysCmd::INIT:
        ESP_LOGI(TAG, "Init command received");
        break;
        
      case SysCmd::RESET:
        // Clear all state
        clearAllSprites();
        clearAllAnimations();
        memset(hub75_buffer_[0], 0, HUB75_WIDTH * HUB75_HEIGHT * 3);
        memset(hub75_buffer_[1], 0, HUB75_WIDTH * HUB75_HEIGHT * 3);
        memset(oled_buffer_[0], 0, OLED_WIDTH * OLED_HEIGHT / 8);
        memset(oled_buffer_[1], 0, OLED_WIDTH * OLED_HEIGHT / 8);
        break;
        
      case SysCmd::SET_BRIGHTNESS:
        if (len >= 1) {
          if (display == Display::HUB75 || display == Display::BOTH) {
            hub75_brightness_ = payload[0];
          }
          if (display == Display::OLED || display == Display::BOTH) {
            oled_brightness_ = payload[0];
          }
        }
        break;
        
      case SysCmd::PING:
        sendPong();
        break;
        
      case SysCmd::STATUS:
        sendStatus();
        break;
        
      case SysCmd::CAPABILITIES:
        sendCapabilities();
        break;
        
      default:
        break;
    }
  }
  
  // Drawing commands
  void executeDrawCmd(uint8_t cmd, Display display,
                      const uint8_t* payload, uint16_t len) {
    DrawCmd draw_cmd = static_cast<DrawCmd>(cmd);
    
    switch (draw_cmd) {
      case DrawCmd::PIXEL:
        if (len >= sizeof(CmdPixel)) {
          const CmdPixel* p = (const CmdPixel*)payload;
          drawPixel(display, p->x, p->y, p->color);
        }
        break;
        
      case DrawCmd::LINE:
        if (len >= sizeof(CmdLine)) {
          const CmdLine* p = (const CmdLine*)payload;
          drawLine(display, p->x0, p->y0, p->x1, p->y1, p->color, p->thickness);
        }
        break;
        
      case DrawCmd::RECT:
        if (len >= sizeof(CmdRect)) {
          const CmdRect* p = (const CmdRect*)payload;
          drawRect(display, p->x, p->y, p->w, p->h, p->color, p->thickness);
        }
        break;
        
      case DrawCmd::RECT_FILL:
        if (len >= sizeof(CmdRect)) {
          const CmdRect* p = (const CmdRect*)payload;
          fillRect(display, p->x, p->y, p->w, p->h, p->color);
        }
        break;
        
      case DrawCmd::CIRCLE:
        if (len >= sizeof(CmdCircle)) {
          const CmdCircle* p = (const CmdCircle*)payload;
          drawCircle(display, p->cx, p->cy, p->radius, p->color, p->thickness);
        }
        break;
        
      case DrawCmd::CIRCLE_FILL:
        if (len >= sizeof(CmdCircle)) {
          const CmdCircle* p = (const CmdCircle*)payload;
          fillCircle(display, p->cx, p->cy, p->radius, p->color);
        }
        break;
        
      default:
        ESP_LOGW(TAG, "Unhandled draw command: 0x%02X", cmd);
        break;
    }
  }
  
  // Text commands
  void executeTextCmd(uint8_t cmd, Display display,
                      const uint8_t* payload, uint16_t len) {
    TextCmd text_cmd = static_cast<TextCmd>(cmd);
    
    switch (text_cmd) {
      case TextCmd::DRAW_STRING:
        if (len >= sizeof(CmdText)) {
          const CmdText* t = (const CmdText*)payload;
          const char* str = (const char*)(payload + sizeof(CmdText));
          drawText(display, t->x, t->y, str, t->str_len, t->color, t->scale);
        }
        break;
        
      case TextCmd::DRAW_CHAR:
        if (len >= 8) {
          int16_t x = *(int16_t*)payload;
          int16_t y = *(int16_t*)(payload + 2);
          char c = payload[4];
          ColorRGB color = *(ColorRGB*)(payload + 5);
          drawChar(display, x, y, c, color, 1);
        }
        break;
        
      default:
        break;
    }
  }
  
  // Image commands
  void executeImageCmd(uint8_t cmd, Display display,
                       const uint8_t* payload, uint16_t len) {
    ImageCmd img_cmd = static_cast<ImageCmd>(cmd);
    
    switch (img_cmd) {
      case ImageCmd::LOAD_SPRITE:
        if (len >= sizeof(CmdLoadSprite)) {
          const CmdLoadSprite* s = (const CmdLoadSprite*)payload;
          const uint8_t* data = payload + sizeof(CmdLoadSprite);
          loadSprite(s->sprite_id, data, s->width, s->height, 
                     s->frames, static_cast<ColorFormat>(s->format));
        }
        break;
        
      case ImageCmd::UNLOAD_SPRITE:
        if (len >= 1) {
          unloadSprite(payload[0]);
        }
        break;
        
      case ImageCmd::DRAW_SPRITE:
        if (len >= sizeof(CmdSprite)) {
          const CmdSprite* s = (const CmdSprite*)payload;
          drawSprite(display, s->sprite_id, s->x, s->y, s->frame);
        }
        break;
        
      default:
        break;
    }
  }
  
  // Animation commands
  void executeAnimCmd(uint8_t cmd, Display display,
                      const uint8_t* payload, uint16_t len) {
    AnimCmd anim_cmd = static_cast<AnimCmd>(cmd);
    
    switch (anim_cmd) {
      case AnimCmd::CREATE:
        if (len >= sizeof(CmdAnimCreate)) {
          const CmdAnimCreate* a = (const CmdAnimCreate*)payload;
          createAnimation(a->anim_id, a->sprite_id, a->start_frame,
                          a->end_frame, a->frame_delay_ms,
                          static_cast<LoopMode>(a->loop_mode));
        }
        break;
        
      case AnimCmd::START:
        if (len >= 5) {
          uint8_t id = payload[0];
          int16_t x = *(int16_t*)(payload + 1);
          int16_t y = *(int16_t*)(payload + 3);
          startAnimation(id, display, x, y);
        }
        break;
        
      case AnimCmd::STOP:
        if (len >= 1) stopAnimation(payload[0]);
        break;
        
      case AnimCmd::PAUSE:
        if (len >= 1) pauseAnimation(payload[0]);
        break;
        
      case AnimCmd::RESUME:
        if (len >= 1) resumeAnimation(payload[0]);
        break;
        
      case AnimCmd::DESTROY:
        if (len >= 1) destroyAnimation(payload[0]);
        break;
        
      default:
        break;
    }
  }
  
  // Script commands
  void executeScriptCmd(uint8_t cmd, Display display,
                        const uint8_t* payload, uint16_t len) {
    ScriptCmd script_cmd = static_cast<ScriptCmd>(cmd);
    
    switch (script_cmd) {
      case ScriptCmd::UPLOAD:
        if (len >= sizeof(CmdScriptUpload)) {
          const CmdScriptUpload* s = (const CmdScriptUpload*)payload;
          const uint8_t* code = payload + sizeof(CmdScriptUpload);
          uploadScript(s->script_id, code, s->script_len);
        }
        break;
        
      case ScriptCmd::EXECUTE:
        if (len >= 1) executeScript(payload[0]);
        break;
        
      case ScriptCmd::STOP:
        if (len >= 1) stopScript(payload[0]);
        break;
        
      case ScriptCmd::DELETE:
        if (len >= 1) deleteScript(payload[0]);
        break;
        
      default:
        break;
    }
  }
  
  // Buffer commands
  void executeBufferCmd(uint8_t cmd, Display display,
                        const uint8_t* payload, uint16_t len) {
    BufferCmd buf_cmd = static_cast<BufferCmd>(cmd);
    
    switch (buf_cmd) {
      case BufferCmd::CLEAR:
        if (len >= sizeof(CmdBufferClear)) {
          const CmdBufferClear* c = (const CmdBufferClear*)payload;
          clearBuffer(display, c->color);
        }
        break;
        
      case BufferCmd::SWAP:
        swapBuffer(display);
        break;
        
      case BufferCmd::FILL:
        if (len >= 3) {
          const ColorRGB* c = (const ColorRGB*)payload;
          fillBuffer(display, *c);
        }
        break;
        
      case BufferCmd::LOCK:
        if (display == Display::HUB75 || display == Display::BOTH) hub75_locked_ = true;
        if (display == Display::OLED || display == Display::BOTH) oled_locked_ = true;
        break;
        
      case BufferCmd::UNLOCK:
        if (display == Display::HUB75 || display == Display::BOTH) {
          hub75_locked_ = false;
          swapBuffer(Display::HUB75);
        }
        if (display == Display::OLED || display == Display::BOTH) {
          oled_locked_ = false;
          swapBuffer(Display::OLED);
        }
        break;
        
      case BufferCmd::SET_CLIP:
        if (len >= sizeof(Rect)) {
          const Rect* r = (const Rect*)payload;
          if (display == Display::HUB75 || display == Display::BOTH) hub75_clip_ = *r;
          if (display == Display::OLED || display == Display::BOTH) oled_clip_ = *r;
        }
        break;
        
      case BufferCmd::CLEAR_CLIP:
        if (display == Display::HUB75 || display == Display::BOTH) {
          hub75_clip_ = {0, 0, HUB75_WIDTH, HUB75_HEIGHT};
        }
        if (display == Display::OLED || display == Display::BOTH) {
          oled_clip_ = {0, 0, OLED_WIDTH, OLED_HEIGHT};
        }
        break;
        
      default:
        break;
    }
  }
  
  // Effect commands
  void executeEffectCmd(uint8_t cmd, Display display,
                        const uint8_t* payload, uint16_t len) {
    if (len < sizeof(CmdEffect)) return;
    
    const CmdEffect* e = (const CmdEffect*)payload;
    EffectState effect;
    effect.active = true;
    effect.type = static_cast<EffectCmd>(e->effect_type);
    effect.start_time = esp_timer_get_time() / 1000;
    effect.duration_ms = e->duration_ms;
    effect.intensity = e->intensity;
    effect.param1 = e->param1;
    effect.param2 = e->param2;
    effect.progress = 0.0f;
    
    if (display == Display::HUB75 || display == Display::BOTH) {
      hub75_effect_ = effect;
    }
    if (display == Display::OLED || display == Display::BOTH) {
      oled_effect_ = effect;
    }
  }
  
  // ============================================================
  // Drawing Primitives
  // ============================================================
  
  void drawPixel(Display display, int16_t x, int16_t y, ColorRGB color) {
    if (display == Display::HUB75 || display == Display::BOTH) {
      drawPixelHub75(x, y, color);
    }
    if (display == Display::OLED || display == Display::BOTH) {
      // Convert to mono
      uint8_t mono = (color.r + color.g + color.b) > 384 ? 1 : 0;
      drawPixelOled(x, y, mono);
    }
  }
  
  void drawPixelHub75(int16_t x, int16_t y, ColorRGB color) {
    if (x < 0 || x >= HUB75_WIDTH || y < 0 || y >= HUB75_HEIGHT) return;
    
    uint8_t* buf = hub75_buffer_[1 - hub75_read_idx_];  // Write to back buffer
    uint32_t idx = (y * HUB75_WIDTH + x) * 3;
    buf[idx] = color.r;
    buf[idx + 1] = color.g;
    buf[idx + 2] = color.b;
  }
  
  void drawPixelOled(int16_t x, int16_t y, uint8_t on) {
    if (x < 0 || x >= OLED_WIDTH || y < 0 || y >= OLED_HEIGHT) return;
    
    uint8_t* buf = oled_buffer_[1 - oled_read_idx_];
    uint32_t byte_idx = (y * OLED_WIDTH + x) / 8;
    uint8_t bit_idx = x % 8;
    
    if (on) {
      buf[byte_idx] |= (1 << bit_idx);
    } else {
      buf[byte_idx] &= ~(1 << bit_idx);
    }
  }
  
  void drawLine(Display display, int16_t x0, int16_t y0, 
                int16_t x1, int16_t y1, ColorRGB color, uint8_t thickness) {
    // Bresenham's line algorithm
    int16_t dx = abs(x1 - x0);
    int16_t dy = abs(y1 - y0);
    int16_t sx = x0 < x1 ? 1 : -1;
    int16_t sy = y0 < y1 ? 1 : -1;
    int16_t err = dx - dy;
    
    while (true) {
      if (thickness <= 1) {
        drawPixel(display, x0, y0, color);
      } else {
        fillCircle(display, x0, y0, thickness / 2, color);
      }
      
      if (x0 == x1 && y0 == y1) break;
      
      int16_t e2 = 2 * err;
      if (e2 > -dy) {
        err -= dy;
        x0 += sx;
      }
      if (e2 < dx) {
        err += dx;
        y0 += sy;
      }
    }
  }
  
  void drawRect(Display display, int16_t x, int16_t y,
                uint16_t w, uint16_t h, ColorRGB color, uint8_t thickness) {
    // Top
    drawLine(display, x, y, x + w - 1, y, color, thickness);
    // Bottom  
    drawLine(display, x, y + h - 1, x + w - 1, y + h - 1, color, thickness);
    // Left
    drawLine(display, x, y, x, y + h - 1, color, thickness);
    // Right
    drawLine(display, x + w - 1, y, x + w - 1, y + h - 1, color, thickness);
  }
  
  void fillRect(Display display, int16_t x, int16_t y,
                uint16_t w, uint16_t h, ColorRGB color) {
    for (int16_t j = y; j < y + (int16_t)h; j++) {
      for (int16_t i = x; i < x + (int16_t)w; i++) {
        drawPixel(display, i, j, color);
      }
    }
  }
  
  void drawCircle(Display display, int16_t cx, int16_t cy,
                  uint16_t r, ColorRGB color, uint8_t thickness) {
    // Midpoint circle algorithm
    int16_t x = r;
    int16_t y = 0;
    int16_t err = 0;
    
    while (x >= y) {
      drawPixel(display, cx + x, cy + y, color);
      drawPixel(display, cx + y, cy + x, color);
      drawPixel(display, cx - y, cy + x, color);
      drawPixel(display, cx - x, cy + y, color);
      drawPixel(display, cx - x, cy - y, color);
      drawPixel(display, cx - y, cy - x, color);
      drawPixel(display, cx + y, cy - x, color);
      drawPixel(display, cx + x, cy - y, color);
      
      y++;
      if (err <= 0) {
        err += 2 * y + 1;
      }
      if (err > 0) {
        x--;
        err -= 2 * x + 1;
      }
    }
  }
  
  void fillCircle(Display display, int16_t cx, int16_t cy,
                  uint16_t r, ColorRGB color) {
    for (int16_t y = -r; y <= r; y++) {
      for (int16_t x = -r; x <= r; x++) {
        if (x * x + y * y <= r * r) {
          drawPixel(display, cx + x, cy + y, color);
        }
      }
    }
  }
  
  // Simple 5x7 font
  void drawChar(Display display, int16_t x, int16_t y, 
                char c, ColorRGB color, uint8_t scale) {
    // Simplified - just draw a placeholder rectangle
    // In production, use a proper font
    if (c >= 32 && c < 127) {
      drawRect(display, x, y, 5 * scale, 7 * scale, color, 1);
    }
  }
  
  void drawText(Display display, int16_t x, int16_t y,
                const char* str, uint8_t len, ColorRGB color, uint8_t scale) {
    int16_t cursor_x = x;
    for (uint8_t i = 0; i < len; i++) {
      drawChar(display, cursor_x, y, str[i], color, scale);
      cursor_x += 6 * scale;  // 5 pixels + 1 space
    }
  }
  
  // ============================================================
  // Sprite Management
  // ============================================================
  
  void loadSprite(uint8_t id, const uint8_t* data, uint16_t w, uint16_t h,
                  uint8_t frames, ColorFormat format) {
    if (id >= MAX_SPRITES) return;
    
    // Free existing sprite
    if (sprites_[id].data) {
      free(sprites_[id].data);
    }
    
    uint32_t bytes_per_pixel = (format == ColorFormat::RGB888) ? 3 :
                               (format == ColorFormat::RGB565) ? 2 : 1;
    uint32_t size = w * h * frames * bytes_per_pixel;
    
    sprites_[id].data = (uint8_t*)malloc(size);
    if (!sprites_[id].data) {
      ESP_LOGE(TAG, "Failed to allocate sprite %d", id);
      return;
    }
    
    memcpy(sprites_[id].data, data, size);
    sprites_[id].width = w;
    sprites_[id].height = h;
    sprites_[id].frames = frames;
    sprites_[id].format = format;
    sprites_[id].data_size = size;
    sprites_[id].loaded = true;
    
    stats_.sprites_loaded++;
    ESP_LOGI(TAG, "Loaded sprite %d: %dx%d, %d frames", id, w, h, frames);
  }
  
  void unloadSprite(uint8_t id) {
    if (id >= MAX_SPRITES || !sprites_[id].loaded) return;
    
    free(sprites_[id].data);
    sprites_[id].data = nullptr;
    sprites_[id].loaded = false;
    stats_.sprites_loaded--;
  }
  
  void drawSprite(Display display, uint8_t id, int16_t x, int16_t y, uint8_t frame) {
    if (id >= MAX_SPRITES || !sprites_[id].loaded) return;
    
    Sprite& s = sprites_[id];
    if (frame >= s.frames) frame = 0;
    
    uint32_t bytes_per_pixel = (s.format == ColorFormat::RGB888) ? 3 : 1;
    uint32_t frame_size = s.width * s.height * bytes_per_pixel;
    uint8_t* frame_data = s.data + frame * frame_size;
    
    for (uint16_t py = 0; py < s.height; py++) {
      for (uint16_t px = 0; px < s.width; px++) {
        uint32_t idx = (py * s.width + px) * bytes_per_pixel;
        
        ColorRGB color;
        if (s.format == ColorFormat::RGB888) {
          color.r = frame_data[idx];
          color.g = frame_data[idx + 1];
          color.b = frame_data[idx + 2];
        } else {
          // Grayscale or mono
          uint8_t v = frame_data[idx];
          color = {v, v, v};
        }
        
        drawPixel(display, x + px, y + py, color);
      }
    }
  }
  
  void clearAllSprites() {
    for (int i = 0; i < MAX_SPRITES; i++) {
      if (sprites_[i].loaded) unloadSprite(i);
    }
  }
  
  // ============================================================
  // Animation System
  // ============================================================
  
  void createAnimation(uint8_t id, uint8_t sprite_id, uint8_t start, uint8_t end,
                       uint16_t delay_ms, LoopMode loop) {
    if (id >= MAX_ANIMATIONS) return;
    
    animations_[id].sprite_id = sprite_id;
    animations_[id].start_frame = start;
    animations_[id].end_frame = end;
    animations_[id].current_frame = start;
    animations_[id].frame_delay_ms = delay_ms;
    animations_[id].loop_mode = loop;
    animations_[id].forward = true;
    animations_[id].active = false;
    animations_[id].paused = false;
  }
  
  void startAnimation(uint8_t id, Display display, int16_t x, int16_t y) {
    if (id >= MAX_ANIMATIONS) return;
    
    animations_[id].x = x;
    animations_[id].y = y;
    animations_[id].active = true;
    animations_[id].paused = false;
    animations_[id].current_frame = animations_[id].start_frame;
    animations_[id].last_frame_time = esp_timer_get_time() / 1000;
    stats_.animations_active++;
  }
  
  void stopAnimation(uint8_t id) {
    if (id >= MAX_ANIMATIONS || !animations_[id].active) return;
    animations_[id].active = false;
    stats_.animations_active--;
  }
  
  void pauseAnimation(uint8_t id) {
    if (id >= MAX_ANIMATIONS) return;
    animations_[id].paused = true;
  }
  
  void resumeAnimation(uint8_t id) {
    if (id >= MAX_ANIMATIONS) return;
    animations_[id].paused = false;
    animations_[id].last_frame_time = esp_timer_get_time() / 1000;
  }
  
  void destroyAnimation(uint8_t id) {
    if (id >= MAX_ANIMATIONS) return;
    if (animations_[id].active) stats_.animations_active--;
    animations_[id].active = false;
  }
  
  void clearAllAnimations() {
    for (int i = 0; i < MAX_ANIMATIONS; i++) {
      animations_[i].active = false;
    }
    stats_.animations_active = 0;
  }
  
  void updateAnimation(Animation& anim, uint32_t now) {
    if (now - anim.last_frame_time < anim.frame_delay_ms) return;
    
    anim.last_frame_time = now;
    
    switch (anim.loop_mode) {
      case LoopMode::ONCE:
        if (anim.current_frame < anim.end_frame) {
          anim.current_frame++;
        } else {
          anim.active = false;
          stats_.animations_active--;
        }
        break;
        
      case LoopMode::LOOP:
        anim.current_frame++;
        if (anim.current_frame > anim.end_frame) {
          anim.current_frame = anim.start_frame;
        }
        break;
        
      case LoopMode::PING_PONG:
        if (anim.forward) {
          anim.current_frame++;
          if (anim.current_frame >= anim.end_frame) {
            anim.forward = false;
          }
        } else {
          anim.current_frame--;
          if (anim.current_frame <= anim.start_frame) {
            anim.forward = true;
          }
        }
        break;
        
      case LoopMode::REVERSE:
        if (anim.current_frame > anim.start_frame) {
          anim.current_frame--;
        } else {
          anim.current_frame = anim.end_frame;
        }
        break;
    }
  }
  
  void renderAnimation(const Animation& anim) {
    // Render to both displays for now (could track per-display)
    drawSprite(Display::HUB75, anim.sprite_id, anim.x, anim.y, anim.current_frame);
  }
  
  // ============================================================
  // Script System (Simple)
  // ============================================================
  
  void uploadScript(uint8_t id, const uint8_t* code, uint16_t len) {
    if (id >= 8) return;
    
    if (scripts_[id].code) {
      free(scripts_[id].code);
    }
    
    scripts_[id].code = (uint8_t*)malloc(len);
    if (!scripts_[id].code) return;
    
    memcpy(scripts_[id].code, code, len);
    scripts_[id].code_len = len;
    scripts_[id].loaded = true;
    scripts_[id].running = false;
    scripts_[id].pc = 0;
  }
  
  void executeScript(uint8_t id) {
    if (id >= 8 || !scripts_[id].loaded) return;
    scripts_[id].running = true;
    scripts_[id].pc = 0;
  }
  
  void stopScript(uint8_t id) {
    if (id >= 8) return;
    scripts_[id].running = false;
  }
  
  void deleteScript(uint8_t id) {
    if (id >= 8) return;
    if (scripts_[id].code) {
      free(scripts_[id].code);
      scripts_[id].code = nullptr;
    }
    scripts_[id].loaded = false;
    scripts_[id].running = false;
  }
  
  void executeScriptStep(Script& script) {
    // Simple bytecode interpreter - placeholder
    // In production, implement a proper scripting engine
    script.running = false;  // Finish after one step for now
  }
  
  // ============================================================
  // Effects
  // ============================================================
  
  void updateEffect(EffectState& effect, uint32_t now) {
    if (effect.duration_ms > 0) {
      uint32_t elapsed = now - effect.start_time;
      effect.progress = (float)elapsed / effect.duration_ms;
      
      if (effect.progress >= 1.0f) {
        effect.progress = 1.0f;
        effect.active = false;
      }
    }
  }
  
  void applyEffect(const EffectState& effect, Display display) {
    switch (effect.type) {
      case EffectCmd::RAINBOW:
        applyRainbowEffect(display, effect);
        break;
      case EffectCmd::FADE:
        applyFadeEffect(display, effect);
        break;
      case EffectCmd::PLASMA:
        applyPlasmaEffect(display, effect);
        break;
      default:
        break;
    }
  }
  
  void applyRainbowEffect(Display display, const EffectState& effect) {
    // Rainbow cycle effect
    uint32_t now = esp_timer_get_time() / 1000;
    uint8_t offset = (now / 10) % 256;
    
    if (display == Display::HUB75) {
      uint8_t* buf = hub75_buffer_[1 - hub75_read_idx_];
      for (int y = 0; y < HUB75_HEIGHT; y++) {
        for (int x = 0; x < HUB75_WIDTH; x++) {
          uint8_t hue = (x * 2 + offset) % 256;
          ColorRGB c = hueToRGB(hue);
          
          uint32_t idx = (y * HUB75_WIDTH + x) * 3;
          buf[idx] = c.r;
          buf[idx + 1] = c.g;
          buf[idx + 2] = c.b;
        }
      }
    }
  }
  
  void applyFadeEffect(Display display, const EffectState& effect) {
    uint8_t brightness = (effect.intensity * (1.0f - effect.progress));
    
    if (display == Display::HUB75) {
      uint8_t* buf = hub75_buffer_[1 - hub75_read_idx_];
      for (int i = 0; i < HUB75_WIDTH * HUB75_HEIGHT * 3; i++) {
        buf[i] = (buf[i] * brightness) / 255;
      }
    }
  }
  
  void applyPlasmaEffect(Display display, const EffectState& effect) {
    uint32_t now = esp_timer_get_time() / 10000;
    
    if (display == Display::HUB75) {
      uint8_t* buf = hub75_buffer_[1 - hub75_read_idx_];
      for (int y = 0; y < HUB75_HEIGHT; y++) {
        for (int x = 0; x < HUB75_WIDTH; x++) {
          // Simple plasma
          float v = sinf(x / 8.0f + now / 10.0f) + 
                    sinf(y / 8.0f + now / 15.0f) +
                    sinf((x + y) / 16.0f + now / 20.0f);
          v = (v + 3) / 6 * 255;
          
          uint8_t hue = (uint8_t)v;
          ColorRGB c = hueToRGB(hue);
          
          uint32_t idx = (y * HUB75_WIDTH + x) * 3;
          buf[idx] = c.r;
          buf[idx + 1] = c.g;
          buf[idx + 2] = c.b;
        }
      }
    }
  }
  
  ColorRGB hueToRGB(uint8_t hue) {
    uint8_t region = hue / 43;
    uint8_t remainder = (hue - region * 43) * 6;
    
    uint8_t r, g, b;
    switch (region) {
      case 0: r = 255; g = remainder; b = 0; break;
      case 1: r = 255 - remainder; g = 255; b = 0; break;
      case 2: r = 0; g = 255; b = remainder; break;
      case 3: r = 0; g = 255 - remainder; b = 255; break;
      case 4: r = remainder; g = 0; b = 255; break;
      default: r = 255; g = 0; b = 255 - remainder; break;
    }
    return {r, g, b};
  }
  
  // ============================================================
  // Buffer Management
  // ============================================================
  
  void clearBuffer(Display display, ColorRGB color) {
    if (display == Display::HUB75 || display == Display::BOTH) {
      uint8_t* buf = hub75_buffer_[1 - hub75_read_idx_];
      for (int i = 0; i < HUB75_WIDTH * HUB75_HEIGHT; i++) {
        buf[i * 3] = color.r;
        buf[i * 3 + 1] = color.g;
        buf[i * 3 + 2] = color.b;
      }
    }
    if (display == Display::OLED || display == Display::BOTH) {
      uint8_t* buf = oled_buffer_[1 - oled_read_idx_];
      uint8_t fill = (color.r + color.g + color.b) > 384 ? 0xFF : 0x00;
      memset(buf, fill, OLED_WIDTH * OLED_HEIGHT / 8);
    }
  }
  
  void fillBuffer(Display display, ColorRGB color) {
    clearBuffer(display, color);
  }
  
  void swapBuffer(Display display) {
    if (display == Display::HUB75 || display == Display::BOTH) {
      if (!hub75_locked_) {
        hub75_read_idx_ = 1 - hub75_read_idx_;
        hub75_ready_ = true;
      }
    }
    if (display == Display::OLED || display == Display::BOTH) {
      if (!oled_locked_) {
        oled_read_idx_ = 1 - oled_read_idx_;
        oled_ready_ = true;
      }
    }
  }
  
  // ============================================================
  // Communication Helpers
  // ============================================================
  
  void sendAck(uint16_t seq_num) {
    sendResponse(SysCmd::ACK, seq_num, nullptr, 0);
  }
  
  void sendNack(uint16_t seq_num) {
    sendResponse(SysCmd::NACK, seq_num, nullptr, 0);
  }
  
  void sendPong() {
    sendResponse(SysCmd::PONG, 0, nullptr, 0);
  }
  
  void sendStatus() {
    GpuStatus status;
    status.uptime_ms = esp_timer_get_time() / 1000;
    status.hub75_fps = 60;  // TODO: measure actual
    status.oled_fps = 30;
    status.cpu_usage = 0;
    status.memory_usage = 0;
    status.frames_rendered = stats_.frames_rendered;
    status.errors = stats_.sync_errors + stats_.checksum_errors;
    status.sprites_loaded = stats_.sprites_loaded;
    status.animations_active = stats_.animations_active;
    
    sendResponse(SysCmd::STATUS, 0, (uint8_t*)&status, sizeof(status));
  }
  
  void sendCapabilities() {
    GpuCapabilities caps;
    caps.protocol_version = PROTOCOL_VERSION;
    caps.hub75_width = HUB75_WIDTH;
    caps.hub75_height = HUB75_HEIGHT;
    caps.oled_width = OLED_WIDTH;
    caps.oled_height = OLED_HEIGHT;
    caps.max_sprites = MAX_SPRITES;
    caps.max_animations = MAX_ANIMATIONS;
    caps.max_layers = MAX_LAYERS;
    caps.free_memory = esp_get_free_heap_size();
    caps.storage_size = 0;  // No storage
    
    sendResponse(SysCmd::CAPABILITIES, 0, (uint8_t*)&caps, sizeof(caps));
  }
  
  void sendResponse(SysCmd cmd, uint16_t seq_num, 
                    const uint8_t* data, uint16_t len) {
    PacketHeader hdr;
    hdr.sync1 = SYNC_BYTE_1;
    hdr.sync2 = SYNC_BYTE_2;
    hdr.sync3 = SYNC_BYTE_3;
    hdr.version = PROTOCOL_VERSION;
    hdr.category = (uint8_t)CmdCategory::SYSTEM;
    hdr.command = (uint8_t)cmd;
    hdr.display = (uint8_t)Display::BOTH;
    hdr.flags = 0;
    hdr.payload_len = len;
    hdr.seq_num = seq_num;
    
    uint16_t checksum = calculateChecksum((uint8_t*)&hdr, sizeof(hdr));
    if (data && len > 0) {
      checksum += calculateChecksum(data, len);
    }
    
    PacketFooter ftr;
    ftr.checksum = checksum;
    ftr.end_byte = SYNC_BYTE_2;
    
    uart_write_bytes(uart_num_, (const char*)&hdr, sizeof(hdr));
    if (data && len > 0) {
      uart_write_bytes(uart_num_, (const char*)data, len);
    }
    uart_write_bytes(uart_num_, (const char*)&ftr, sizeof(ftr));
  }
  
  void flushBytes(size_t count) {
    uint8_t discard[64];
    while (count > 0) {
      size_t to_read = count > sizeof(discard) ? sizeof(discard) : count;
      int read = uart_read_bytes(uart_num_, discard, to_read, pdMS_TO_TICKS(10));
      if (read <= 0) break;
      count -= read;
    }
  }
};

} // namespace gpu

#endif // GPU_RENDERER_HPP_
