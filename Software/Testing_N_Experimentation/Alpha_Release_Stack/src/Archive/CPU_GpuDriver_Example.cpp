/*****************************************************************
 * File:      CPU_GpuDriver_Example.cpp
 * Category:  Example / CPU Side
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Example demonstrating the GpuDriver API from the CPU side.
 *    Shows how to send commands, vectors, sprites, and scripts.
 * 
 * Usage:
 *    1. Upload GPU_GpuDriver_Example.cpp to GPU (COM5)
 *    2. Upload this file to CPU (COM15)
 *    3. Watch the displays for graphics demo
 *****************************************************************/

#include <Arduino.h>
#include <math.h>
#include "GpuDriver/GpuDriver.hpp"
#include "GpuDriver/GpuScript.hpp"

using namespace gpu;

// ============================================================
// GPU Driver Instance
// ============================================================

GpuDriver gpu;

// Demo state
enum class DemoMode {
  SHAPES,
  TEXT,
  ANIMATION,
  EFFECTS,
  SCRIPT
};

DemoMode current_demo = DemoMode::SHAPES;
uint32_t demo_start_time = 0;
uint32_t demo_duration = 5000;  // 5 seconds per demo
uint32_t frame_count = 0;

// ============================================================
// Demo Functions
// ============================================================

void demoShapes() {
  static uint8_t phase = 0;
  phase++;
  
  // Clear and draw shapes
  gpu.beginDraw(Display::HUB75, Colors::Black);
  
  // Moving rectangle
  int16_t rect_x = (phase * 2) % 100;
  gpu.fillRect(Display::HUB75, rect_x, 5, 20, 10, Colors::Red);
  
  // Pulsing circle
  uint8_t radius = 5 + (phase % 10);
  gpu.fillCircle(Display::HUB75, 64, 16, radius, Colors::Green);
  
  // Diagonal line
  gpu.drawLine(Display::HUB75, 0, 0, 127, 31, Colors::Blue, 1);
  
  // Border
  gpu.drawRect(Display::HUB75, 0, 0, 128, 32, Colors::White, 1);
  
  gpu.endDraw(Display::HUB75);
  
  // OLED - different pattern
  gpu.beginDraw(Display::OLED, Colors::Black);
  gpu.drawRect(Display::OLED, 10, 10, 108, 108, Colors::White, 1);
  gpu.fillCircle(Display::OLED, 64, 64, 30 + (phase % 20), Colors::White);
  gpu.endDraw(Display::OLED);
}

void demoText() {
  static uint8_t phase = 0;
  phase++;
  
  gpu.beginDraw(Display::HUB75, Colors::Black);
  
  // Title
  gpu.setTextColor(Display::HUB75, Colors::Cyan);
  gpu.drawText(Display::HUB75, 30, 2, "ARCOS");
  
  // Scrolling text simulation
  int16_t scroll_x = 128 - (phase * 2) % 256;
  gpu.setTextColor(Display::HUB75, Colors::Yellow);
  gpu.drawText(Display::HUB75, scroll_x, 14, "GPU Driver Demo");
  
  // Frame counter
  gpu.setTextColor(Display::HUB75, Colors::Green);
  gpu.drawTextFormatted(Display::HUB75, 0, 24, "F:%lu", frame_count);
  
  gpu.endDraw(Display::HUB75);
  
  // OLED
  gpu.beginDraw(Display::OLED, Colors::Black);
  gpu.setTextColor(Display::OLED, Colors::White);
  gpu.drawText(Display::OLED, 20, 30, "OLED Display");
  gpu.drawText(Display::OLED, 20, 50, "128x128 Mono");
  gpu.drawTextFormatted(Display::OLED, 20, 80, "Frame: %lu", frame_count);
  gpu.endDraw(Display::OLED);
}

void demoAnimation() {
  // This would use pre-loaded sprites
  // For now, simulate with moving shapes
  
  static uint8_t phase = 0;
  phase += 2;
  
  gpu.beginDraw(Display::HUB75, Colors::Black);
  
  // Bouncing ball simulation
  int16_t ball_x = 64 + (int16_t)(30 * sin(phase * 0.1));
  int16_t ball_y = 16 + (int16_t)(10 * sin(phase * 0.15));
  
  gpu.fillCircle(Display::HUB75, ball_x, ball_y, 5, Colors::Red);
  
  // Trail
  for (int i = 1; i <= 5; i++) {
    int16_t trail_x = 64 + (int16_t)(30 * sin((phase - i * 3) * 0.1));
    int16_t trail_y = 16 + (int16_t)(10 * sin((phase - i * 3) * 0.15));
    uint8_t brightness = 255 - i * 40;
    gpu.fillCircle(Display::HUB75, trail_x, trail_y, 3, 
                   ColorRGB(brightness, 0, 0));
  }
  
  gpu.endDraw(Display::HUB75);
}

void demoEffects() {
  static uint8_t effect_phase = 0;
  static uint8_t current_effect = 0;
  
  if (effect_phase == 0) {
    // Start a new effect
    switch (current_effect) {
      case 0:
        gpu.rainbow(Display::HUB75, 2000);
        Serial.println("[Demo] Rainbow effect");
        break;
      case 1:
        gpu.plasma(Display::HUB75);
        Serial.println("[Demo] Plasma effect");
        break;
      case 2:
        gpu.fire(Display::HUB75);
        Serial.println("[Demo] Fire effect");
        break;
    }
    effect_phase = 1;
  }
  
  // Effects run on GPU side, CPU just needs to wait
  static uint32_t effect_start = 0;
  if (effect_phase == 1) {
    effect_start = millis();
    effect_phase = 2;
  }
  
  if (effect_phase == 2 && millis() - effect_start > 3000) {
    // Switch to next effect
    current_effect = (current_effect + 1) % 3;
    effect_phase = 0;
  }
}

void demoScript() {
  static bool script_uploaded = false;
  
  if (!script_uploaded) {
    // Build and upload a script
    ScriptBuilder script;
    Scripts::buildBootAnimation(script);
    
    gpu.uploadScript(0, script.getData(), script.getLength());
    gpu.executeScript(0);
    
    script_uploaded = true;
    Serial.println("[Demo] Script uploaded and started");
  }
  
  // Script runs on GPU
}

// ============================================================
// Setup and Loop
// ============================================================

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("\n");
  Serial.println("╔════════════════════════════════════════════════════════════╗");
  Serial.println("║           GPU Driver Demo - CPU Side                       ║");
  Serial.println("║           Command-based graphics API                       ║");
  Serial.println("╚════════════════════════════════════════════════════════════╝");
  Serial.println();
  
  // Initialize GPU driver
  GpuDriver::Config config;
  config.baud_rate = GPU_BAUD_RATE;
  config.tx_pin = 12;
  config.rx_pin = 11;
  config.wait_for_ack = false;  // Fire-and-forget for speed
  
  if (!gpu.init(config)) {
    Serial.println("[ERROR] Failed to initialize GPU driver!");
    while (1) delay(1000);
  }
  
  Serial.printf("[CPU] GPU driver initialized at %lu baud\n", GPU_BAUD_RATE);
  
  // Ping GPU to verify connection
  if (gpu.ping()) {
    Serial.printf("[CPU] GPU responded, RTT: %lu us\n", gpu.getStats().last_rtt_us);
  } else {
    Serial.println("[WARN] GPU did not respond to ping");
  }
  
  // Set initial brightness
  gpu.setBrightness(Display::HUB75, 128);
  gpu.setBrightness(Display::OLED, 255);
  
  // Clear displays
  gpu.clear(Display::HUB75, Colors::Black);
  gpu.clear(Display::OLED, Colors::Black);
  gpu.swap(Display::BOTH);
  
  demo_start_time = millis();
  Serial.println("\n[CPU] Starting demo sequence...\n");
}

void loop() {
  uint32_t now = millis();
  
  // Process any GPU responses
  gpu.process();
  
  // Run current demo
  switch (current_demo) {
    case DemoMode::SHAPES:
      demoShapes();
      break;
    case DemoMode::TEXT:
      demoText();
      break;
    case DemoMode::ANIMATION:
      demoAnimation();
      break;
    case DemoMode::EFFECTS:
      demoEffects();
      break;
    case DemoMode::SCRIPT:
      demoScript();
      break;
  }
  
  frame_count++;
  
  // Switch demo mode every demo_duration
  if (now - demo_start_time > demo_duration) {
    demo_start_time = now;
    
    // Stop any running effects/scripts
    gpu.clear(Display::BOTH, Colors::Black);
    gpu.swap(Display::BOTH);
    
    // Next demo
    switch (current_demo) {
      case DemoMode::SHAPES:
        current_demo = DemoMode::TEXT;
        Serial.println("\n[Demo] Switching to TEXT demo");
        break;
      case DemoMode::TEXT:
        current_demo = DemoMode::ANIMATION;
        Serial.println("\n[Demo] Switching to ANIMATION demo");
        break;
      case DemoMode::ANIMATION:
        current_demo = DemoMode::EFFECTS;
        Serial.println("\n[Demo] Switching to EFFECTS demo");
        break;
      case DemoMode::EFFECTS:
        current_demo = DemoMode::SCRIPT;
        Serial.println("\n[Demo] Switching to SCRIPT demo");
        break;
      case DemoMode::SCRIPT:
        current_demo = DemoMode::SHAPES;
        Serial.println("\n[Demo] Switching to SHAPES demo");
        break;
    }
  }
  
  // Print stats every 5 seconds
  static uint32_t last_stats = 0;
  if (now - last_stats > 5000) {
    last_stats = now;
    const auto& stats = gpu.getStats();
    Serial.printf("[CPU] Commands: %lu | Bytes: %lu | ACKs: %lu | Errors: %lu\n",
                  stats.commands_sent, stats.bytes_sent, 
                  stats.acks_received, stats.errors);
  }
  
  // Frame rate limiting (~60fps)
  delay(16);
}
