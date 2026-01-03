/*****************************************************************
 * File:      GpuDriver.hpp
 * Category:  GPU Driver / CPU Side
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    CPU-side GPU driver for sending commands to the GPU.
 *    Provides a high-level API for graphics operations.
 * 
 * Usage:
 *    GpuDriver gpu;
 *    gpu.init();
 *    
 *    // Drawing
 *    gpu.clear(Display::HUB75, Colors::Black);
 *    gpu.drawRect(Display::HUB75, 10, 5, 20, 10, Colors::Red);
 *    gpu.drawText(Display::HUB75, 0, 0, "Hello!");
 *    gpu.swap(Display::HUB75);
 *    
 *    // Sprites
 *    gpu.loadSprite(0, sprite_data, 16, 16, 4);
 *    gpu.drawSprite(Display::HUB75, 0, 50, 10);
 *    
 *    // Effects
 *    gpu.startEffect(Display::HUB75, EffectCmd::RAINBOW, 5000);
 *    
 *    // Scripts
 *    gpu.uploadScript(0, script_data, script_len);
 *    gpu.executeScript(0);
 *****************************************************************/

#ifndef GPU_DRIVER_HPP_
#define GPU_DRIVER_HPP_

#include <Arduino.h>
#include "GpuBaseAPI.hpp"

namespace gpu {

class GpuDriver {
public:
  // Configuration
  struct Config {
    uint32_t baud_rate;
    uint8_t tx_pin;
    uint8_t rx_pin;
    bool auto_swap;      // Auto swap buffer after draw commands
    bool wait_for_ack;   // Wait for ACK after each command
    
    Config() : baud_rate(GPU_BAUD_RATE), tx_pin(12), rx_pin(11),
               auto_swap(false), wait_for_ack(false) {}
  };
  
  // Statistics
  struct Stats {
    uint32_t commands_sent;
    uint32_t bytes_sent;
    uint32_t acks_received;
    uint32_t nacks_received;
    uint32_t timeouts;
    uint32_t errors;
    uint32_t last_rtt_us;
  };
  
  GpuDriver() : initialized_(false), seq_num_(0) {
    memset(&stats_, 0, sizeof(stats_));
  }
  
  // ============================================================
  // Initialization
  // ============================================================
  
  bool init(const Config& config = Config()) {
    config_ = config;
    
    Serial1.begin(config.baud_rate, SERIAL_8N1, config.rx_pin, config.tx_pin);
    Serial1.setRxBufferSize(2048);
    
    initialized_ = true;
    
    // Send init command
    return sendSystemCmd(SysCmd::INIT);
  }
  
  // ============================================================
  // System Commands
  // ============================================================
  
  bool reset() {
    return sendSystemCmd(SysCmd::RESET);
  }
  
  bool ping() {
    uint32_t start = micros();
    if (sendSystemCmd(SysCmd::PING)) {
      // Wait for PONG
      if (waitForResponse(SysCmd::PONG, 100)) {
        stats_.last_rtt_us = micros() - start;
        return true;
      }
    }
    return false;
  }
  
  bool setBrightness(Display display, uint8_t brightness) {
    uint8_t payload[1] = {brightness};
    return sendCommand(CmdCategory::SYSTEM, (uint8_t)SysCmd::SET_BRIGHTNESS, 
                       display, payload, 1);
  }
  
  bool setFps(Display display, uint8_t fps) {
    uint8_t payload[1] = {fps};
    return sendCommand(CmdCategory::SYSTEM, (uint8_t)SysCmd::SET_FPS,
                       display, payload, 1);
  }
  
  GpuStatus getStatus() {
    GpuStatus status = {};
    sendSystemCmd(SysCmd::STATUS);
    // Read response...
    return status;
  }
  
  GpuCapabilities getCapabilities() {
    GpuCapabilities caps = {};
    sendSystemCmd(SysCmd::CAPABILITIES);
    // Read response...
    return caps;
  }
  
  // ============================================================
  // Drawing Commands
  // ============================================================
  
  bool drawPixel(Display display, int16_t x, int16_t y, ColorRGB color) {
    CmdPixel cmd = {x, y, color};
    return sendCommand(CmdCategory::DRAW, (uint8_t)DrawCmd::PIXEL,
                       display, (uint8_t*)&cmd, sizeof(cmd));
  }
  
  bool drawLine(Display display, int16_t x0, int16_t y0, 
                int16_t x1, int16_t y1, ColorRGB color, uint8_t thickness = 1) {
    CmdLine cmd = {x0, y0, x1, y1, color, thickness};
    return sendCommand(CmdCategory::DRAW, (uint8_t)DrawCmd::LINE,
                       display, (uint8_t*)&cmd, sizeof(cmd));
  }
  
  bool drawRect(Display display, int16_t x, int16_t y, 
                uint16_t w, uint16_t h, ColorRGB color, uint8_t thickness = 1) {
    CmdRect cmd = {x, y, w, h, color, thickness};
    return sendCommand(CmdCategory::DRAW, (uint8_t)DrawCmd::RECT,
                       display, (uint8_t*)&cmd, sizeof(cmd));
  }
  
  bool fillRect(Display display, int16_t x, int16_t y,
                uint16_t w, uint16_t h, ColorRGB color) {
    CmdRect cmd = {x, y, w, h, color, 0};  // thickness 0 = filled
    return sendCommand(CmdCategory::DRAW, (uint8_t)DrawCmd::RECT_FILL,
                       display, (uint8_t*)&cmd, sizeof(cmd));
  }
  
  bool drawCircle(Display display, int16_t cx, int16_t cy,
                  uint16_t radius, ColorRGB color, uint8_t thickness = 1) {
    CmdCircle cmd = {cx, cy, radius, color, thickness};
    return sendCommand(CmdCategory::DRAW, (uint8_t)DrawCmd::CIRCLE,
                       display, (uint8_t*)&cmd, sizeof(cmd));
  }
  
  bool fillCircle(Display display, int16_t cx, int16_t cy,
                  uint16_t radius, ColorRGB color) {
    CmdCircle cmd = {cx, cy, radius, color, 0};
    return sendCommand(CmdCategory::DRAW, (uint8_t)DrawCmd::CIRCLE_FILL,
                       display, (uint8_t*)&cmd, sizeof(cmd));
  }
  
  bool drawTriangle(Display display, int16_t x0, int16_t y0,
                    int16_t x1, int16_t y1, int16_t x2, int16_t y2,
                    ColorRGB color, uint8_t thickness = 1) {
    struct {
      int16_t x0, y0, x1, y1, x2, y2;
      ColorRGB color;
      uint8_t thickness;
    } cmd = {x0, y0, x1, y1, x2, y2, color, thickness};
    return sendCommand(CmdCategory::DRAW, (uint8_t)DrawCmd::TRIANGLE,
                       display, (uint8_t*)&cmd, sizeof(cmd));
  }
  
  // Draw polygon from array of points
  bool drawPolygon(Display display, const Point* points, uint8_t count,
                   ColorRGB color, uint8_t thickness = 1) {
    if (count < 3 || count > 32) return false;
    
    uint8_t buffer[256];
    buffer[0] = count;
    buffer[1] = thickness;
    memcpy(&buffer[2], &color, 3);
    memcpy(&buffer[5], points, count * sizeof(Point));
    
    return sendCommand(CmdCategory::DRAW, (uint8_t)DrawCmd::POLYGON,
                       display, buffer, 5 + count * sizeof(Point));
  }
  
  // Draw rounded rectangle
  bool drawRoundedRect(Display display, int16_t x, int16_t y,
                       uint16_t w, uint16_t h, uint8_t radius,
                       ColorRGB color, uint8_t thickness = 1) {
    struct {
      int16_t x, y;
      uint16_t w, h;
      uint8_t radius;
      ColorRGB color;
      uint8_t thickness;
    } cmd = {x, y, w, h, radius, color, thickness};
    return sendCommand(CmdCategory::DRAW, (uint8_t)DrawCmd::ROUNDED_RECT,
                       display, (uint8_t*)&cmd, sizeof(cmd));
  }
  
  // Draw gradient rectangle
  bool drawGradientRect(Display display, int16_t x, int16_t y,
                        uint16_t w, uint16_t h,
                        ColorRGB color1, ColorRGB color2, bool horizontal = true) {
    struct {
      int16_t x, y;
      uint16_t w, h;
      ColorRGB color1, color2;
      uint8_t horizontal;
    } cmd = {x, y, w, h, color1, color2, horizontal ? 1 : 0};
    return sendCommand(CmdCategory::DRAW, (uint8_t)DrawCmd::GRADIENT_RECT,
                       display, (uint8_t*)&cmd, sizeof(cmd));
  }
  
  // ============================================================
  // Text Commands
  // ============================================================
  
  bool setFont(Display display, uint8_t font_id) {
    current_font_ = font_id;
    uint8_t payload[1] = {font_id};
    return sendCommand(CmdCategory::TEXT, (uint8_t)TextCmd::SET_FONT,
                       display, payload, 1);
  }
  
  bool setTextSize(Display display, uint8_t size) {
    current_text_size_ = size;
    uint8_t payload[1] = {size};
    return sendCommand(CmdCategory::TEXT, (uint8_t)TextCmd::SET_SIZE,
                       display, payload, 1);
  }
  
  bool setTextColor(Display display, ColorRGB color) {
    current_text_color_ = color;
    return sendCommand(CmdCategory::TEXT, (uint8_t)TextCmd::SET_COLOR,
                       display, (uint8_t*)&color, 3);
  }
  
  bool setTextAlign(Display display, TextAlign align) {
    uint8_t payload[1] = {(uint8_t)align};
    return sendCommand(CmdCategory::TEXT, (uint8_t)TextCmd::SET_ALIGN,
                       display, payload, 1);
  }
  
  bool drawText(Display display, int16_t x, int16_t y, const char* text) {
    size_t len = strlen(text);
    if (len > 200) len = 200;  // Max string length
    
    uint8_t buffer[256];
    CmdText* cmd = (CmdText*)buffer;
    cmd->x = x;
    cmd->y = y;
    cmd->font_id = current_font_;
    cmd->scale = current_text_size_;
    cmd->color = current_text_color_;
    cmd->align = (uint8_t)TextAlign::LEFT;
    cmd->str_len = len;
    memcpy(buffer + sizeof(CmdText), text, len);
    
    return sendCommand(CmdCategory::TEXT, (uint8_t)TextCmd::DRAW_STRING,
                       display, buffer, sizeof(CmdText) + len);
  }
  
  bool drawTextFormatted(Display display, int16_t x, int16_t y, 
                         const char* format, ...) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    return drawText(display, x, y, buffer);
  }
  
  bool drawChar(Display display, int16_t x, int16_t y, char c, ColorRGB color) {
    struct {
      int16_t x, y;
      char c;
      ColorRGB color;
    } cmd = {x, y, c, color};
    return sendCommand(CmdCategory::TEXT, (uint8_t)TextCmd::DRAW_CHAR,
                       display, (uint8_t*)&cmd, sizeof(cmd));
  }
  
  // ============================================================
  // Sprite / Image Commands
  // ============================================================
  
  bool loadSprite(uint8_t sprite_id, const uint8_t* data, 
                  uint16_t width, uint16_t height, uint8_t frames = 1,
                  ColorFormat format = ColorFormat::RGB888) {
    // Calculate data size
    uint32_t bytes_per_pixel = 3;  // RGB888
    if (format == ColorFormat::RGB565) bytes_per_pixel = 2;
    else if (format == ColorFormat::MONO) bytes_per_pixel = 1;
    uint32_t data_size = width * height * frames * bytes_per_pixel;
    
    // Send header
    CmdLoadSprite hdr;
    hdr.sprite_id = sprite_id;
    hdr.width = width;
    hdr.height = height;
    hdr.frames = frames;
    hdr.format = (uint8_t)format;
    hdr.data_size = data_size;
    
    // For large sprites, we need to send in chunks
    // First, send the header
    uint8_t buffer[MAX_PACKET_SIZE];
    memcpy(buffer, &hdr, sizeof(hdr));
    
    if (data_size <= MAX_PACKET_SIZE - sizeof(hdr)) {
      // Small sprite - send all at once
      memcpy(buffer + sizeof(hdr), data, data_size);
      return sendCommand(CmdCategory::IMAGE, (uint8_t)ImageCmd::LOAD_SPRITE,
                         Display::BOTH, buffer, sizeof(hdr) + data_size);
    } else {
      // Large sprite - send in chunks (use file upload)
      return uploadSpriteChunked(sprite_id, data, width, height, frames, format);
    }
  }
  
  bool unloadSprite(uint8_t sprite_id) {
    uint8_t payload[1] = {sprite_id};
    return sendCommand(CmdCategory::IMAGE, (uint8_t)ImageCmd::UNLOAD_SPRITE,
                       Display::BOTH, payload, 1);
  }
  
  bool drawSprite(Display display, uint8_t sprite_id, int16_t x, int16_t y,
                  uint8_t frame = 0, uint8_t flags = 0) {
    CmdSprite cmd = {sprite_id, x, y, frame, flags};
    return sendCommand(CmdCategory::IMAGE, (uint8_t)ImageCmd::DRAW_SPRITE,
                       display, (uint8_t*)&cmd, sizeof(cmd));
  }
  
  // Draw raw bitmap data directly
  bool drawBitmap(Display display, int16_t x, int16_t y,
                  uint16_t width, uint16_t height,
                  const uint8_t* data, ColorFormat format = ColorFormat::RGB888) {
    struct {
      int16_t x, y;
      uint16_t w, h;
      uint8_t format;
    } header = {x, y, width, height, (uint8_t)format};
    
    uint32_t bytes_per_pixel = (format == ColorFormat::RGB888) ? 3 : 
                               (format == ColorFormat::RGB565) ? 2 : 1;
    uint32_t data_size = width * height * bytes_per_pixel;
    
    if (sizeof(header) + data_size > MAX_PACKET_SIZE) {
      // Too large - need chunked transfer
      return false;
    }
    
    uint8_t buffer[MAX_PACKET_SIZE];
    memcpy(buffer, &header, sizeof(header));
    memcpy(buffer + sizeof(header), data, data_size);
    
    return sendCommand(CmdCategory::IMAGE, (uint8_t)ImageCmd::DRAW_BITMAP,
                       display, buffer, sizeof(header) + data_size);
  }
  
  // ============================================================
  // Animation Commands
  // ============================================================
  
  bool createAnimation(uint8_t anim_id, uint8_t sprite_id,
                       uint8_t start_frame, uint8_t end_frame,
                       uint16_t frame_delay_ms, LoopMode loop = LoopMode::LOOP) {
    CmdAnimCreate cmd;
    cmd.anim_id = anim_id;
    cmd.sprite_id = sprite_id;
    cmd.start_frame = start_frame;
    cmd.end_frame = end_frame;
    cmd.frame_delay_ms = frame_delay_ms;
    cmd.loop_mode = (uint8_t)loop;
    
    return sendCommand(CmdCategory::ANIMATION, (uint8_t)AnimCmd::CREATE,
                       Display::BOTH, (uint8_t*)&cmd, sizeof(cmd));
  }
  
  bool startAnimation(uint8_t anim_id, Display display, int16_t x, int16_t y) {
    struct {
      uint8_t anim_id;
      int16_t x, y;
    } cmd = {anim_id, x, y};
    return sendCommand(CmdCategory::ANIMATION, (uint8_t)AnimCmd::START,
                       display, (uint8_t*)&cmd, sizeof(cmd));
  }
  
  bool stopAnimation(uint8_t anim_id) {
    uint8_t payload[1] = {anim_id};
    return sendCommand(CmdCategory::ANIMATION, (uint8_t)AnimCmd::STOP,
                       Display::BOTH, payload, 1);
  }
  
  bool pauseAnimation(uint8_t anim_id) {
    uint8_t payload[1] = {anim_id};
    return sendCommand(CmdCategory::ANIMATION, (uint8_t)AnimCmd::PAUSE,
                       Display::BOTH, payload, 1);
  }
  
  bool resumeAnimation(uint8_t anim_id) {
    uint8_t payload[1] = {anim_id};
    return sendCommand(CmdCategory::ANIMATION, (uint8_t)AnimCmd::RESUME,
                       Display::BOTH, payload, 1);
  }
  
  bool setAnimationSpeed(uint8_t anim_id, uint16_t frame_delay_ms) {
    struct {
      uint8_t anim_id;
      uint16_t frame_delay_ms;
    } cmd = {anim_id, frame_delay_ms};
    return sendCommand(CmdCategory::ANIMATION, (uint8_t)AnimCmd::SET_SPEED,
                       Display::BOTH, (uint8_t*)&cmd, sizeof(cmd));
  }
  
  bool destroyAnimation(uint8_t anim_id) {
    uint8_t payload[1] = {anim_id};
    return sendCommand(CmdCategory::ANIMATION, (uint8_t)AnimCmd::DESTROY,
                       Display::BOTH, payload, 1);
  }
  
  // Screen transition effects
  bool transition(Display display, uint8_t effect, uint16_t duration_ms) {
    struct {
      uint8_t effect;
      uint16_t duration_ms;
    } cmd = {effect, duration_ms};
    return sendCommand(CmdCategory::ANIMATION, (uint8_t)AnimCmd::TRANSITION,
                       display, (uint8_t*)&cmd, sizeof(cmd));
  }
  
  // ============================================================
  // Script Commands
  // ============================================================
  
  bool uploadScript(uint8_t script_id, const uint8_t* script, uint16_t len) {
    if (len > MAX_SCRIPT_SIZE) return false;
    
    uint8_t buffer[MAX_SCRIPT_SIZE + 8];
    CmdScriptUpload* cmd = (CmdScriptUpload*)buffer;
    cmd->script_id = script_id;
    cmd->script_len = len;
    memcpy(buffer + sizeof(CmdScriptUpload), script, len);
    
    return sendCommand(CmdCategory::SCRIPT, (uint8_t)ScriptCmd::UPLOAD,
                       Display::BOTH, buffer, sizeof(CmdScriptUpload) + len);
  }
  
  bool uploadScript(uint8_t script_id, const char* script) {
    return uploadScript(script_id, (const uint8_t*)script, strlen(script));
  }
  
  bool executeScript(uint8_t script_id) {
    uint8_t payload[1] = {script_id};
    return sendCommand(CmdCategory::SCRIPT, (uint8_t)ScriptCmd::EXECUTE,
                       Display::BOTH, payload, 1);
  }
  
  bool stopScript(uint8_t script_id) {
    uint8_t payload[1] = {script_id};
    return sendCommand(CmdCategory::SCRIPT, (uint8_t)ScriptCmd::STOP,
                       Display::BOTH, payload, 1);
  }
  
  bool deleteScript(uint8_t script_id) {
    uint8_t payload[1] = {script_id};
    return sendCommand(CmdCategory::SCRIPT, (uint8_t)ScriptCmd::DELETE,
                       Display::BOTH, payload, 1);
  }
  
  // Execute inline script (not stored)
  bool executeInline(const char* script) {
    uint16_t len = strlen(script);
    if (len > MAX_SCRIPT_SIZE) return false;
    
    uint8_t buffer[MAX_SCRIPT_SIZE + 2];
    buffer[0] = len & 0xFF;
    buffer[1] = (len >> 8) & 0xFF;
    memcpy(buffer + 2, script, len);
    
    return sendCommand(CmdCategory::SCRIPT, (uint8_t)ScriptCmd::INLINE,
                       Display::BOTH, buffer, 2 + len);
  }
  
  bool setScriptVar(uint8_t script_id, const char* name, int32_t value) {
    uint8_t name_len = strlen(name);
    if (name_len > 32) return false;
    
    uint8_t buffer[64];
    buffer[0] = script_id;
    buffer[1] = name_len;
    memcpy(buffer + 2, name, name_len);
    memcpy(buffer + 2 + name_len, &value, 4);
    
    return sendCommand(CmdCategory::SCRIPT, (uint8_t)ScriptCmd::SET_VAR,
                       Display::BOTH, buffer, 2 + name_len + 4);
  }
  
  // ============================================================
  // File Commands
  // ============================================================
  
  bool uploadFile(const char* filename, const uint8_t* data, uint32_t size,
                  uint8_t file_type = 0) {
    // Start upload
    uint16_t name_len = strlen(filename);
    uint8_t start_buf[256];
    CmdFileStart* start = (CmdFileStart*)start_buf;
    start->file_type = file_type;
    start->file_size = size;
    start->name_len = name_len;
    memcpy(start_buf + sizeof(CmdFileStart), filename, name_len);
    
    if (!sendCommand(CmdCategory::FILE, (uint8_t)FileCmd::UPLOAD_START,
                     Display::BOTH, start_buf, sizeof(CmdFileStart) + name_len)) {
      return false;
    }
    
    // Send data in chunks
    uint32_t offset = 0;
    const uint16_t CHUNK_SIZE = MAX_PACKET_SIZE - sizeof(CmdFileData) - 16;
    
    while (offset < size) {
      uint16_t chunk_len = min((uint32_t)CHUNK_SIZE, size - offset);
      
      uint8_t data_buf[MAX_PACKET_SIZE];
      CmdFileData* chunk = (CmdFileData*)data_buf;
      chunk->offset = offset;
      chunk->chunk_len = chunk_len;
      memcpy(data_buf + sizeof(CmdFileData), data + offset, chunk_len);
      
      if (!sendCommand(CmdCategory::FILE, (uint8_t)FileCmd::UPLOAD_DATA,
                       Display::BOTH, data_buf, sizeof(CmdFileData) + chunk_len)) {
        return false;
      }
      
      offset += chunk_len;
    }
    
    // End upload
    return sendCommand(CmdCategory::FILE, (uint8_t)FileCmd::UPLOAD_END,
                       Display::BOTH, nullptr, 0);
  }
  
  bool deleteFile(const char* filename) {
    uint16_t len = strlen(filename);
    uint8_t buffer[256];
    buffer[0] = len & 0xFF;
    buffer[1] = (len >> 8) & 0xFF;
    memcpy(buffer + 2, filename, len);
    
    return sendCommand(CmdCategory::FILE, (uint8_t)FileCmd::DELETE,
                       Display::BOTH, buffer, 2 + len);
  }
  
  // ============================================================
  // Buffer Commands
  // ============================================================
  
  bool clear(Display display, ColorRGB color = Colors::Black) {
    CmdBufferClear cmd = {color, 0};
    return sendCommand(CmdCategory::BUFFER, (uint8_t)BufferCmd::CLEAR,
                       display, (uint8_t*)&cmd, sizeof(cmd));
  }
  
  bool swap(Display display) {
    return sendCommand(CmdCategory::BUFFER, (uint8_t)BufferCmd::SWAP,
                       display, nullptr, 0);
  }
  
  bool setLayer(Display display, uint8_t layer) {
    uint8_t payload[1] = {layer};
    return sendCommand(CmdCategory::BUFFER, (uint8_t)BufferCmd::SET_LAYER,
                       display, payload, 1);
  }
  
  bool blendLayers(Display display, BlendMode mode = BlendMode::NORMAL) {
    uint8_t payload[1] = {(uint8_t)mode};
    return sendCommand(CmdCategory::BUFFER, (uint8_t)BufferCmd::BLEND_LAYERS,
                       display, payload, 1);
  }
  
  bool fill(Display display, ColorRGB color) {
    return sendCommand(CmdCategory::BUFFER, (uint8_t)BufferCmd::FILL,
                       display, (uint8_t*)&color, 3);
  }
  
  bool setClip(Display display, int16_t x, int16_t y, uint16_t w, uint16_t h) {
    Rect clip = {x, y, w, h};
    return sendCommand(CmdCategory::BUFFER, (uint8_t)BufferCmd::SET_CLIP,
                       display, (uint8_t*)&clip, sizeof(clip));
  }
  
  bool clearClip(Display display) {
    return sendCommand(CmdCategory::BUFFER, (uint8_t)BufferCmd::CLEAR_CLIP,
                       display, nullptr, 0);
  }
  
  // Lock buffer for batch operations (reduces flicker)
  bool lock(Display display) {
    return sendCommand(CmdCategory::BUFFER, (uint8_t)BufferCmd::LOCK,
                       display, nullptr, 0);
  }
  
  bool unlock(Display display) {
    return sendCommand(CmdCategory::BUFFER, (uint8_t)BufferCmd::UNLOCK,
                       display, nullptr, 0);
  }
  
  // ============================================================
  // Effect Commands
  // ============================================================
  
  bool startEffect(Display display, EffectCmd effect, uint16_t duration_ms,
                   uint8_t intensity = 128, uint8_t param1 = 0, uint8_t param2 = 0) {
    CmdEffect cmd;
    cmd.effect_type = (uint8_t)effect;
    cmd.duration_ms = duration_ms;
    cmd.intensity = intensity;
    cmd.param1 = param1;
    cmd.param2 = param2;
    
    return sendCommand(CmdCategory::EFFECT, (uint8_t)effect,
                       display, (uint8_t*)&cmd, sizeof(cmd));
  }
  
  bool fadeIn(Display display, uint16_t duration_ms) {
    return startEffect(display, EffectCmd::FADE, duration_ms, 255);
  }
  
  bool fadeOut(Display display, uint16_t duration_ms) {
    return startEffect(display, EffectCmd::FADE, duration_ms, 0);
  }
  
  bool scroll(Display display, int16_t dx, int16_t dy, uint16_t duration_ms) {
    CmdEffect cmd;
    cmd.effect_type = (uint8_t)EffectCmd::SCROLL;
    cmd.duration_ms = duration_ms;
    cmd.intensity = 128;
    cmd.param1 = dx;
    cmd.param2 = dy;
    return sendCommand(CmdCategory::EFFECT, (uint8_t)EffectCmd::SCROLL,
                       display, (uint8_t*)&cmd, sizeof(cmd));
  }
  
  bool rainbow(Display display, uint16_t cycle_ms = 5000) {
    return startEffect(display, EffectCmd::RAINBOW, cycle_ms);
  }
  
  bool plasma(Display display) {
    return startEffect(display, EffectCmd::PLASMA, 0);  // Continuous
  }
  
  bool fire(Display display) {
    return startEffect(display, EffectCmd::FIRE, 0);
  }
  
  bool matrixRain(Display display, uint8_t density = 128) {
    return startEffect(display, EffectCmd::MATRIX, 0, density);
  }
  
  bool particles(Display display, uint8_t count = 50, uint8_t speed = 128) {
    return startEffect(display, EffectCmd::PARTICLES, 0, count, speed);
  }
  
  // ============================================================
  // Convenience Methods
  // ============================================================
  
  // Draw progress bar
  bool drawProgressBar(Display display, int16_t x, int16_t y,
                       uint16_t w, uint16_t h, uint8_t percent,
                       ColorRGB fg = Colors::Green, ColorRGB bg = Colors::Gray) {
    // Draw background
    fillRect(display, x, y, w, h, bg);
    // Draw progress
    uint16_t progress_w = (w * percent) / 100;
    if (progress_w > 0) {
      fillRect(display, x, y, progress_w, h, fg);
    }
    // Draw border
    drawRect(display, x, y, w, h, Colors::White);
    return true;
  }
  
  // Draw icon at position (built-in icons)
  bool drawIcon(Display display, uint8_t icon_id, int16_t x, int16_t y,
                ColorRGB color = Colors::White) {
    struct {
      uint8_t icon_id;
      int16_t x, y;
      ColorRGB color;
    } cmd = {icon_id, x, y, color};
    return sendCommand(CmdCategory::IMAGE, (uint8_t)ImageCmd::DRAW_ICON,
                       display, (uint8_t*)&cmd, sizeof(cmd));
  }
  
  // Begin batch drawing (lock + clear)
  bool beginDraw(Display display, ColorRGB bg = Colors::Black) {
    lock(display);
    return clear(display, bg);
  }
  
  // End batch drawing (unlock + swap)
  bool endDraw(Display display) {
    unlock(display);
    return swap(display);
  }
  
  // ============================================================
  // Statistics
  // ============================================================
  
  const Stats& getStats() const { return stats_; }
  
  void resetStats() { memset(&stats_, 0, sizeof(stats_)); }
  
  void printStats() {
    Serial.println("\n═══ GPU Driver Stats ═══");
    Serial.printf("  Commands: %lu\n", stats_.commands_sent);
    Serial.printf("  Bytes TX: %lu\n", stats_.bytes_sent);
    Serial.printf("  ACKs: %lu, NACKs: %lu\n", stats_.acks_received, stats_.nacks_received);
    Serial.printf("  Timeouts: %lu, Errors: %lu\n", stats_.timeouts, stats_.errors);
    Serial.printf("  RTT: %lu us\n", stats_.last_rtt_us);
    Serial.println("═════════════════════════\n");
  }
  
  // ============================================================
  // Low-level Communication
  // ============================================================
  
  void process() {
    // Process incoming responses
    while (Serial1.available() >= (int)sizeof(PacketHeader)) {
      if (Serial1.peek() != SYNC_BYTE_1) {
        Serial1.read();
        continue;
      }
      
      PacketHeader hdr;
      Serial1.readBytes((uint8_t*)&hdr, sizeof(hdr));
      
      if (!validatePacketHeader(hdr)) {
        stats_.errors++;
        continue;
      }
      
      // Read payload
      uint8_t payload[256];
      if (hdr.payload_len > 0 && hdr.payload_len <= sizeof(payload)) {
        Serial1.readBytes(payload, hdr.payload_len);
      }
      
      // Read footer
      PacketFooter ftr;
      Serial1.readBytes((uint8_t*)&ftr, sizeof(ftr));
      
      // Handle response
      SysCmd cmd = static_cast<SysCmd>(hdr.command);
      if (cmd == SysCmd::ACK) {
        stats_.acks_received++;
      } else if (cmd == SysCmd::NACK) {
        stats_.nacks_received++;
      }
    }
  }
  
private:
  Config config_;
  bool initialized_;
  uint16_t seq_num_;
  Stats stats_;
  
  // Current text state
  uint8_t current_font_ = 0;
  uint8_t current_text_size_ = 1;
  ColorRGB current_text_color_ = Colors::White;
  
  bool sendSystemCmd(SysCmd cmd) {
    return sendCommand(CmdCategory::SYSTEM, (uint8_t)cmd, Display::BOTH, nullptr, 0);
  }
  
  bool sendCommand(CmdCategory category, uint8_t command, Display display,
                   const uint8_t* payload, uint16_t len) {
    if (!initialized_) return false;
    
    // Build header
    PacketHeader hdr;
    hdr.sync1 = SYNC_BYTE_1;
    hdr.sync2 = SYNC_BYTE_2;
    hdr.sync3 = SYNC_BYTE_3;
    hdr.version = PROTOCOL_VERSION;
    hdr.category = (uint8_t)category;
    hdr.command = command;
    hdr.display = (uint8_t)display;
    hdr.flags = 0;
    hdr.payload_len = len;
    hdr.seq_num = seq_num_++;
    
    // Calculate checksum
    uint16_t checksum = calculateChecksum((uint8_t*)&hdr, sizeof(hdr));
    if (payload && len > 0) {
      checksum += calculateChecksum(payload, len);
    }
    
    PacketFooter ftr;
    ftr.checksum = checksum;
    ftr.end_byte = SYNC_BYTE_2;
    
    // Send packet
    Serial1.write((uint8_t*)&hdr, sizeof(hdr));
    if (payload && len > 0) {
      Serial1.write(payload, len);
    }
    Serial1.write((uint8_t*)&ftr, sizeof(ftr));
    
    stats_.commands_sent++;
    stats_.bytes_sent += sizeof(hdr) + len + sizeof(ftr);
    
    // Wait for ACK if configured
    if (config_.wait_for_ack) {
      return waitForResponse(SysCmd::ACK, ACK_TIMEOUT_MS);
    }
    
    return true;
  }
  
  bool waitForResponse(SysCmd expected, uint32_t timeout_ms) {
    uint32_t start = millis();
    
    while (millis() - start < timeout_ms) {
      process();
      
      // Check if we got the expected response
      // (simplified - in production would track per-sequence)
      if (stats_.acks_received > 0 && expected == SysCmd::ACK) {
        return true;
      }
      
      delay(1);
    }
    
    stats_.timeouts++;
    return false;
  }
  
  bool uploadSpriteChunked(uint8_t sprite_id, const uint8_t* data,
                           uint16_t width, uint16_t height, uint8_t frames,
                           ColorFormat format) {
    // Use file upload mechanism for large sprites
    char filename[32];
    snprintf(filename, sizeof(filename), "sprite_%d.bin", sprite_id);
    
    uint32_t bytes_per_pixel = (format == ColorFormat::RGB888) ? 3 : 
                               (format == ColorFormat::RGB565) ? 2 : 1;
    uint32_t data_size = width * height * frames * bytes_per_pixel;
    
    return uploadFile(filename, data, data_size, 0);  // type 0 = sprite
  }
};

} // namespace gpu

#endif // GPU_DRIVER_HPP_
