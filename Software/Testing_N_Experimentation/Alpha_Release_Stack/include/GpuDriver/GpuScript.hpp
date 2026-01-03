/*****************************************************************
 * File:      GpuScript.hpp
 * Category:  GPU Driver / Script System
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Simple scripting system for GPU animations and sequences.
 *    Scripts are bytecode that the GPU interprets.
 * 
 * Script Format:
 *    Each instruction is:
 *      [OPCODE:1] [ARGS:variable]
 * 
 * Example Script (rainbow + text):
 *    EFFECT RAINBOW 5000
 *    DELAY 1000
 *    CLEAR
 *    TEXT 10 5 "Hello!"
 *    RECT 0 0 128 32
 *    LOOP
 *****************************************************************/

#ifndef GPU_SCRIPT_HPP_
#define GPU_SCRIPT_HPP_

#include <stdint.h>
#include "GpuBaseAPI.hpp"

namespace gpu {

// ============================================================
// Script Opcodes
// ============================================================

enum class ScriptOp : uint8_t {
  NOP         = 0x00,  // No operation
  END         = 0x01,  // End script
  
  // Flow control
  DELAY       = 0x10,  // DELAY <ms:2>
  LOOP        = 0x11,  // Loop to start
  JUMP        = 0x12,  // JUMP <offset:2>
  CALL        = 0x13,  // CALL <script_id:1>
  RETURN      = 0x14,  // Return from call
  
  // Variables
  SET_VAR     = 0x20,  // SET_VAR <var:1> <value:4>
  INC_VAR     = 0x21,  // INC_VAR <var:1>
  DEC_VAR     = 0x22,  // DEC_VAR <var:1>
  
  // Drawing (uses current display)
  CLEAR       = 0x30,  // CLEAR <r:1> <g:1> <b:1>
  PIXEL       = 0x31,  // PIXEL <x:2> <y:2> <r:1> <g:1> <b:1>
  LINE        = 0x32,  // LINE <x0:2> <y0:2> <x1:2> <y1:2> <r:1> <g:1> <b:1>
  RECT        = 0x33,  // RECT <x:2> <y:2> <w:2> <h:2> <r:1> <g:1> <b:1>
  FILL_RECT   = 0x34,  // FILL_RECT <x:2> <y:2> <w:2> <h:2> <r:1> <g:1> <b:1>
  CIRCLE      = 0x35,  // CIRCLE <cx:2> <cy:2> <r:2> <r:1> <g:1> <b:1>
  FILL_CIRCLE = 0x36,  // FILL_CIRCLE <cx:2> <cy:2> <r:2> <r:1> <g:1> <b:1>
  
  // Text
  TEXT        = 0x40,  // TEXT <x:2> <y:2> <len:1> <string:len>
  TEXT_COLOR  = 0x41,  // TEXT_COLOR <r:1> <g:1> <b:1>
  TEXT_SIZE   = 0x42,  // TEXT_SIZE <size:1>
  
  // Sprites
  SPRITE      = 0x50,  // SPRITE <id:1> <x:2> <y:2> <frame:1>
  
  // Effects
  EFFECT      = 0x60,  // EFFECT <type:1> <duration:2> <intensity:1>
  STOP_EFFECT = 0x61,  // Stop current effect
  
  // Buffer
  SWAP        = 0x70,  // Swap buffers
  SET_DISPLAY = 0x71,  // SET_DISPLAY <display:1>
  LOCK        = 0x72,  // Lock buffer
  UNLOCK      = 0x73,  // Unlock buffer
};

// ============================================================
// Script Builder (for CPU side)
// ============================================================

class ScriptBuilder {
public:
  ScriptBuilder() : pos_(0) {
    memset(buffer_, 0, sizeof(buffer_));
  }
  
  // Get compiled script
  const uint8_t* getData() const { return buffer_; }
  uint16_t getLength() const { return pos_; }
  
  // Reset builder
  void reset() { pos_ = 0; }
  
  // ========== Flow Control ==========
  
  ScriptBuilder& nop() {
    write8((uint8_t)ScriptOp::NOP);
    return *this;
  }
  
  ScriptBuilder& end() {
    write8((uint8_t)ScriptOp::END);
    return *this;
  }
  
  ScriptBuilder& delay(uint16_t ms) {
    write8((uint8_t)ScriptOp::DELAY);
    write16(ms);
    return *this;
  }
  
  ScriptBuilder& loop() {
    write8((uint8_t)ScriptOp::LOOP);
    return *this;
  }
  
  ScriptBuilder& jump(uint16_t offset) {
    write8((uint8_t)ScriptOp::JUMP);
    write16(offset);
    return *this;
  }
  
  // ========== Variables ==========
  
  ScriptBuilder& setVar(uint8_t var, int32_t value) {
    write8((uint8_t)ScriptOp::SET_VAR);
    write8(var);
    write32(value);
    return *this;
  }
  
  ScriptBuilder& incVar(uint8_t var) {
    write8((uint8_t)ScriptOp::INC_VAR);
    write8(var);
    return *this;
  }
  
  ScriptBuilder& decVar(uint8_t var) {
    write8((uint8_t)ScriptOp::DEC_VAR);
    write8(var);
    return *this;
  }
  
  // ========== Drawing ==========
  
  ScriptBuilder& clear(uint8_t r = 0, uint8_t g = 0, uint8_t b = 0) {
    write8((uint8_t)ScriptOp::CLEAR);
    write8(r); write8(g); write8(b);
    return *this;
  }
  
  ScriptBuilder& clear(ColorRGB color) {
    return clear(color.r, color.g, color.b);
  }
  
  ScriptBuilder& pixel(int16_t x, int16_t y, uint8_t r, uint8_t g, uint8_t b) {
    write8((uint8_t)ScriptOp::PIXEL);
    write16(x); write16(y);
    write8(r); write8(g); write8(b);
    return *this;
  }
  
  ScriptBuilder& pixel(int16_t x, int16_t y, ColorRGB color) {
    return pixel(x, y, color.r, color.g, color.b);
  }
  
  ScriptBuilder& line(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                      uint8_t r, uint8_t g, uint8_t b) {
    write8((uint8_t)ScriptOp::LINE);
    write16(x0); write16(y0);
    write16(x1); write16(y1);
    write8(r); write8(g); write8(b);
    return *this;
  }
  
  ScriptBuilder& line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, ColorRGB color) {
    return line(x0, y0, x1, y1, color.r, color.g, color.b);
  }
  
  ScriptBuilder& rect(int16_t x, int16_t y, uint16_t w, uint16_t h,
                      uint8_t r, uint8_t g, uint8_t b) {
    write8((uint8_t)ScriptOp::RECT);
    write16(x); write16(y);
    write16(w); write16(h);
    write8(r); write8(g); write8(b);
    return *this;
  }
  
  ScriptBuilder& rect(int16_t x, int16_t y, uint16_t w, uint16_t h, ColorRGB color) {
    return rect(x, y, w, h, color.r, color.g, color.b);
  }
  
  ScriptBuilder& fillRect(int16_t x, int16_t y, uint16_t w, uint16_t h,
                          uint8_t r, uint8_t g, uint8_t b) {
    write8((uint8_t)ScriptOp::FILL_RECT);
    write16(x); write16(y);
    write16(w); write16(h);
    write8(r); write8(g); write8(b);
    return *this;
  }
  
  ScriptBuilder& fillRect(int16_t x, int16_t y, uint16_t w, uint16_t h, ColorRGB color) {
    return fillRect(x, y, w, h, color.r, color.g, color.b);
  }
  
  ScriptBuilder& circle(int16_t cx, int16_t cy, uint16_t radius,
                        uint8_t r, uint8_t g, uint8_t b) {
    write8((uint8_t)ScriptOp::CIRCLE);
    write16(cx); write16(cy);
    write16(radius);
    write8(r); write8(g); write8(b);
    return *this;
  }
  
  ScriptBuilder& fillCircle(int16_t cx, int16_t cy, uint16_t radius,
                            uint8_t r, uint8_t g, uint8_t b) {
    write8((uint8_t)ScriptOp::FILL_CIRCLE);
    write16(cx); write16(cy);
    write16(radius);
    write8(r); write8(g); write8(b);
    return *this;
  }
  
  // ========== Text ==========
  
  ScriptBuilder& text(int16_t x, int16_t y, const char* str) {
    write8((uint8_t)ScriptOp::TEXT);
    write16(x); write16(y);
    uint8_t len = strlen(str);
    if (len > 64) len = 64;
    write8(len);
    for (uint8_t i = 0; i < len; i++) {
      write8(str[i]);
    }
    return *this;
  }
  
  ScriptBuilder& textColor(uint8_t r, uint8_t g, uint8_t b) {
    write8((uint8_t)ScriptOp::TEXT_COLOR);
    write8(r); write8(g); write8(b);
    return *this;
  }
  
  ScriptBuilder& textColor(ColorRGB color) {
    return textColor(color.r, color.g, color.b);
  }
  
  ScriptBuilder& textSize(uint8_t size) {
    write8((uint8_t)ScriptOp::TEXT_SIZE);
    write8(size);
    return *this;
  }
  
  // ========== Sprites ==========
  
  ScriptBuilder& sprite(uint8_t id, int16_t x, int16_t y, uint8_t frame = 0) {
    write8((uint8_t)ScriptOp::SPRITE);
    write8(id);
    write16(x); write16(y);
    write8(frame);
    return *this;
  }
  
  // ========== Effects ==========
  
  ScriptBuilder& effect(EffectCmd type, uint16_t duration_ms, uint8_t intensity = 128) {
    write8((uint8_t)ScriptOp::EFFECT);
    write8((uint8_t)type);
    write16(duration_ms);
    write8(intensity);
    return *this;
  }
  
  ScriptBuilder& rainbow(uint16_t cycle_ms = 5000) {
    return effect(EffectCmd::RAINBOW, cycle_ms);
  }
  
  ScriptBuilder& plasma() {
    return effect(EffectCmd::PLASMA, 0);
  }
  
  ScriptBuilder& fire() {
    return effect(EffectCmd::FIRE, 0);
  }
  
  ScriptBuilder& fade(uint16_t duration_ms, uint8_t target = 0) {
    return effect(EffectCmd::FADE, duration_ms, target);
  }
  
  ScriptBuilder& stopEffect() {
    write8((uint8_t)ScriptOp::STOP_EFFECT);
    return *this;
  }
  
  // ========== Buffer ==========
  
  ScriptBuilder& swap() {
    write8((uint8_t)ScriptOp::SWAP);
    return *this;
  }
  
  ScriptBuilder& setDisplay(Display display) {
    write8((uint8_t)ScriptOp::SET_DISPLAY);
    write8((uint8_t)display);
    return *this;
  }
  
  ScriptBuilder& lock() {
    write8((uint8_t)ScriptOp::LOCK);
    return *this;
  }
  
  ScriptBuilder& unlock() {
    write8((uint8_t)ScriptOp::UNLOCK);
    return *this;
  }
  
private:
  uint8_t buffer_[MAX_SCRIPT_SIZE];
  uint16_t pos_;
  
  void write8(uint8_t v) {
    if (pos_ < MAX_SCRIPT_SIZE) buffer_[pos_++] = v;
  }
  
  void write16(uint16_t v) {
    write8(v & 0xFF);
    write8((v >> 8) & 0xFF);
  }
  
  void write32(uint32_t v) {
    write8(v & 0xFF);
    write8((v >> 8) & 0xFF);
    write8((v >> 16) & 0xFF);
    write8((v >> 24) & 0xFF);
  }
};

// ============================================================
// Pre-built Scripts
// ============================================================

namespace Scripts {

// Rainbow cycle animation
inline void buildRainbow(ScriptBuilder& s, uint16_t cycle_ms = 5000) {
  s.reset()
   .rainbow(cycle_ms)
   .end();
}

// Scrolling text
inline void buildScrollingText(ScriptBuilder& s, const char* text, 
                               int16_t y = 12, uint16_t delay_ms = 50) {
  s.reset()
   .setVar(0, 128)  // Start x position (off right edge)
   .textColor(Colors::White)
   .clear(Colors::Black)
   .text(0, y, text)  // Will use variable for x... simplified for now
   .swap()
   .delay(delay_ms)
   .loop();
}

// Plasma effect
inline void buildPlasma(ScriptBuilder& s) {
  s.reset()
   .plasma()
   .end();
}

// Fire effect
inline void buildFire(ScriptBuilder& s) {
  s.reset()
   .fire()
   .end();
}

// Blink pattern
inline void buildBlink(ScriptBuilder& s, ColorRGB color, uint16_t on_ms, uint16_t off_ms) {
  s.reset()
   .clear(color)
   .swap()
   .delay(on_ms)
   .clear(Colors::Black)
   .swap()
   .delay(off_ms)
   .loop();
}

// Progress bar animation
inline void buildProgressBar(ScriptBuilder& s, uint16_t duration_ms) {
  // Simplified - would need variables in real implementation
  s.reset()
   .clear(Colors::Black)
   .rect(10, 12, 108, 8, Colors::White)
   .swap()
   .delay(duration_ms / 100)
   .loop();
}

// Boot animation
inline void buildBootAnimation(ScriptBuilder& s) {
  s.reset()
   .clear(Colors::Black)
   .swap()
   .delay(500)
   .textColor(Colors::Cyan)
   .text(20, 5, "ARCOS")
   .swap()
   .delay(1000)
   .text(10, 20, "Booting...")
   .swap()
   .delay(500)
   .rainbow(2000)
   .delay(2000)
   .stopEffect()
   .clear(Colors::Black)
   .text(30, 12, "Ready")
   .swap()
   .delay(1000)
   .end();
}

// Alert animation
inline void buildAlert(ScriptBuilder& s, const char* message) {
  s.reset()
   .clear(Colors::Red)
   .swap()
   .delay(200)
   .clear(Colors::Black)
   .textColor(Colors::Red)
   .text(10, 12, message)
   .swap()
   .delay(200)
   .loop();
}

} // namespace Scripts

} // namespace gpu

#endif // GPU_SCRIPT_HPP_
