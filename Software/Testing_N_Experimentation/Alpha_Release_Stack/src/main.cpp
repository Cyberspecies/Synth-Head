/*****************************************************************
 * File:      main.cpp
 * Category:  examples/hub75_hsl_demo
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    HSL color cycling demo application for HUB75 LED matrix
 *    displays demonstrating the ARCOS abstraction framework.
 *****************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_task_wdt.h"

// ESP32 HUB75 LED Matrix Panel DMA Library
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

static const char* TAG = "HSL_DEMO";

/** Platform implementations (injected into protocol) */
static HAL_PARALLEL_DEFAULT hardware;
static ParallelBuffer bufferManager;

/** HUB75 I2S protocol implementation */
static HUB75_I2S_Protocol i2sProtocol;

/** HUB75 display driver with dual OE support */
static HUB75Driver display;

/** BCM brightness control (0-255, affects entire screen via BCM bit depth) */
static uint8_t global_brightness = 255;  // Animated brightness
static uint32_t brightness_timer = 0;    // Timer for brightness animation
static float hue_offset = 0.0f;          // Hue rotation offset for animation

/** RGB colour structure */
struct CRGB{
  uint8_t r, g, b;
  CRGB() : r(0), g(0), b(0) {}
  CRGB(uint8_t red, uint8_t green, uint8_t blue) : r(red), g(green), b(blue) {}
};

/** HSL to RGB conversion
 * H: 0-360 degrees (hue)
 * S: 0-100 percent (saturation)
 * L: 0-100 percent (lightness/luminosity)
 * Returns RGB with 0-255 values
 */
CRGB hslToRgb(float h, float s, float l){
  h = fmodf(h, 360.0f);
  if(h < 0) h += 360.0f;
  s = fminf(fmaxf(s, 0.0f), 100.0f) / 100.0f;
  l = fminf(fmaxf(l, 0.0f), 100.0f) / 100.0f;
  
  float c = (1.0f - fabsf(2.0f * l - 1.0f)) * s;
  float h_prime = h / 60.0f;
  float x = c * (1.0f - fabsf(fmodf(h_prime, 2.0f) - 1.0f));
  float m = l - c / 2.0f;
  
  float r1, g1, b1;
  if(h_prime >= 0 && h_prime < 1){
    r1 = c; g1 = x; b1 = 0;
  }else if(h_prime >= 1 && h_prime < 2){
    r1 = x; g1 = c; b1 = 0;
  }else if(h_prime >= 2 && h_prime < 3){
    r1 = 0; g1 = c; b1 = x;
  }else if(h_prime >= 3 && h_prime < 4){
    r1 = 0; g1 = x; b1 = c;
  }else if(h_prime >= 4 && h_prime < 5){
    r1 = x; g1 = 0; b1 = c;
  }else{
    r1 = c; g1 = 0; b1 = x;
  }
  
  uint8_t r = (uint8_t)roundf((r1 + m) * 255.0f);
  uint8_t g = (uint8_t)roundf((g1 + m) * 255.0f);
  uint8_t b = (uint8_t)roundf((b1 + m) * 255.0f);
  
  return CRGB(r, g, b);
}

/** Draw full-panel HSL gradient on a single 64x32 panel
 * Panel layout (64x32):
 * - X axis (left to right): Hue gradient (0-360°) with cycling animation
 * - Y axis (top to middle): Saturation gradient (100% to 0%)
 * - Y axis (middle to bottom): Luminosity gradient (0% to 100%)
 * 
 * Creates a beautiful 2D color space visualization with animated hue cycling
 */
void drawHSLPattern(int panel_index){
  const int panel_width = 64;
  const int panel_height = 32;
  int panel_x_offset = panel_index * panel_width;
  
  for(int y = 0; y < panel_height; y++){
    for(int x = 0; x < panel_width; x++){
      float hue = fmodf((x / (float)(panel_width - 1)) * 360.0f + hue_offset, 360.0f);
      
      float saturation;
      if(y < panel_height / 2){
        saturation = 100.0f * (1.0f - (y / (float)(panel_height / 2)));
      }else{
        saturation = 100.0f;
      }
      
      float lightness;
      if(y < panel_height / 2){
        lightness = 50.0f;
      }else{
        float progress = (y - panel_height / 2) / (float)(panel_height / 2);
        lightness = progress * 100.0f;
      }
      
      CRGB color = hslToRgb(hue, saturation, lightness);
      display.setPixel(panel_x_offset + x, y, RGB(color.r, color.g, color.b));
    }
  }
}

/** Render HSL patterns to all panels */
void renderHSLPatterns(){
  drawHSLPattern(0);
  drawHSLPattern(1);
  display.show();
}

/** Update hue offset for color wheel cycling animation */
void updateHueCycle(){
  hue_offset += 2.0f;
  if(hue_offset >= 360.0f){
    hue_offset -= 360.0f;
  }
}

/** Update brightness with linear fade animation */
void updateBrightness(){
  brightness_timer++;
  const uint32_t cycle_length = 20;
  const uint8_t max_brightness = 255;
  
  uint32_t position = brightness_timer % cycle_length;
  if(position < cycle_length / 2){
    global_brightness = (uint8_t)((position * max_brightness) / (cycle_length / 2));
  }else{
    global_brightness = (uint8_t)(((cycle_length - position) * max_brightness) / (cycle_length / 2));
  }
  
  display.setBrightness(global_brightness);
  
  if(brightness_timer % 5 == 0){
    ESP_LOGI(TAG, "BCM Brightness: %d/255 (%.1f%%)", 
             global_brightness, (global_brightness / 255.0f) * 100.0f);
  }
}

extern "C" void app_main(){
  // VERY FIRST THING - print something
  vTaskDelay(pdMS_TO_TICKS(2000));
  printf("\n\n\n*** ESP32 BOOTED - APP STARTING ***\n\n\n");
  vTaskDelay(pdMS_TO_TICKS(100)); // Give time for serial to flush
  
  ESP_LOGI(TAG, "=== HSL Color Scale Demo ===");
  ESP_LOGI(TAG, "Demonstrating HSL color space with BCM brightness control");
  ESP_LOGI(TAG, "");
  
  /** Disable watchdog timer */
  esp_task_wdt_deinit();
  
  /** Configure display with dual OE pins */
  HUB75Config config = HUB75Config::getDefault();
  config.enable_gamma_correction = true;  // Enable built-in gamma correction
  config.gamma_value = 2.2f;
  config.dual_display_mode = true;        // Enable dual display spillover
  config.effective_width = 128;           // 64x2 = 128 pixels wide
  
  /** Panel inversion: Flip panel 0 vertically (required for hardware orientation) */
  config.panel_inversions[0].flip_vertical = true;   // Panel 0: flip upside down
  config.panel_inversions[1].flip_vertical = false;  // Panel 1: normal orientation
  
  /** Use the correct working pin configuration */
  config.pins.r0_pin = 7;   // Red 0
  config.pins.g0_pin = 15;  // Green 0  
  config.pins.b0_pin = 16;  // Blue 0
  config.pins.r1_pin = 17;  // Red 1
  config.pins.g1_pin = 18;  // Green 1
  config.pins.b1_pin = 8;   // Blue 1
  config.pins.a_pin = 41;   // Address A
  config.pins.b_pin = 40;   // Address B
  config.pins.c_pin = 39;   // Address C
  config.pins.d_pin = 38;   // Address D
  config.pins.e_pin = 42;   // Address E
  config.pins.lat_pin = 36; // Latch
  config.pins.oe_pin = 35;  // Primary Output Enable
  config.pins.oe_pin2 = 6;  // Secondary Output Enable
  config.pins.clock_pin = 37; // Clock
  
  /** Calculate buffer size using driver helper method */
  int buffer_size = HUB75Driver::calculateBufferSize(config);
  
  ESP_LOGI(TAG, "Calculated buffer size: %d samples (%d KB)", 
           buffer_size, (buffer_size * 2) / 1024);
  
  /** Initialize I2S protocol with hardware dependencies */
  if(!i2sProtocol.init(config, buffer_size, &hardware, &bufferManager)){
    ESP_LOGE(TAG, "Failed to initialise I2S protocol");
    return;
  }
  
  /** Initialise display with I2S protocol */
  if(!display.init(config, &i2sProtocol)){
    ESP_LOGE(TAG, "Failed to initialise HUB75 display");
    return;
  }
  
  /** Start display */
  if(!display.start()){
    ESP_LOGE(TAG, "Failed to start display");
    return;
  }

  ESP_LOGI(TAG, "Display initialized: %dx%d pixels (2x 64x32 panels)",
           display.getWidth(), display.getHeight());
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "Display Layout (each panel):");
  ESP_LOGI(TAG, "  - Top 10 rows: HUE gradient (0-360°)");
  ESP_LOGI(TAG, "  - Middle 11 rows: SATURATION gradient (0-100%%)");
  ESP_LOGI(TAG, "  - Bottom 11 rows: LIGHTNESS gradient (0-100%%)");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "Animation: HUE cycling + Brightness fade");
  ESP_LOGI(TAG, "  - All gradients cycle through rainbow colors");
  ESP_LOGI(TAG, "  - Brightness fades linearly 0-100%% over 1 second");
  ESP_LOGI(TAG, "  - Note: Brightness scales pixel values (may reduce apparent bit depth)");
  ESP_LOGI(TAG, "");
  
  /** Main loop: Display HSL patterns and update BCM brightness */
  uint32_t last_status_time = 0;
  
  while(true){
    uint64_t current_time_us = esp_timer_get_time();
    
    // Update hue offset for color cycling animation
    updateHueCycle();
    
    // Update brightness animation
    updateBrightness();
    
    // Render HSL patterns with animated hue offset
    renderHSLPatterns();
    
    // Log status every 5 seconds
    if(current_time_us - last_status_time >= 5000000){
      size_t free_heap = esp_get_free_heap_size();
      size_t total_heap = heap_caps_get_total_size(MALLOC_CAP_8BIT);
      
      ESP_LOGI(TAG, "=== HSL DEMO STATUS ===");
      ESP_LOGI(TAG, "  Hue Offset: %.1f° (cycling through rainbow)", hue_offset);
      ESP_LOGI(TAG, "  Brightness: %d/255 (%.1f%%)", 
               global_brightness, (global_brightness / 255.0f) * 100.0f);
      ESP_LOGI(TAG, "  Free RAM: %d KB / %d KB (%.1f%%)", 
               free_heap / 1024, total_heap / 1024, 
               (free_heap * 100.0f) / total_heap);
      ESP_LOGI(TAG, "  Display: %dx%d pixels", 
               display.getWidth(), display.getHeight());
      
      last_status_time = current_time_us;
    }
    
    // Delay to control update rate (~20 FPS for breathing effect)
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}