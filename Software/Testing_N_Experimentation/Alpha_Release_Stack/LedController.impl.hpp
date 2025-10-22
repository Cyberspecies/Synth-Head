#ifndef LED_CONTROLLER_IMPL_HPP
#define LED_CONTROLLER_IMPL_HPP

#include "LedController.h"

LedController::LedController() :
  left_fin_strip(nullptr),
  tongue_strip(nullptr),
  right_fin_strip(nullptr),
  scale_strip(nullptr),
  scale_led_count(DEFAULT_SCALE_LED_COUNT),
  hue_offset(0.0f),
  hue_speed(1.0f),
  last_update_time(0),
  update_interval_ms(50){
}

LedController::~LedController(){
  if(left_fin_strip){
    delete left_fin_strip;
  }
  if(tongue_strip){
    delete tongue_strip;
  }
  if(right_fin_strip){
    delete right_fin_strip;
  }
  if(scale_strip){
    delete scale_strip;
  }
}

bool LedController::initialize(){
  // Create NeoPixel strip objects for WRGB LEDs
  left_fin_strip = new Adafruit_NeoPixel(LEFT_FIN_LED_COUNT, LED_STRIP_1_PIN, NEO_WRGB + NEO_KHZ800);
  tongue_strip = new Adafruit_NeoPixel(TONGUE_LED_COUNT, LED_STRIP_2_PIN, NEO_WRGB + NEO_KHZ800);
  right_fin_strip = new Adafruit_NeoPixel(RIGHT_FIN_LED_COUNT, LED_STRIP_4_PIN, NEO_WRGB + NEO_KHZ800);
  scale_strip = new Adafruit_NeoPixel(scale_led_count, LED_STRIP_5_PIN, NEO_WRGB + NEO_KHZ800);

  // Check if allocation was successful
  if(!left_fin_strip || !tongue_strip || !right_fin_strip || !scale_strip){
    return false;
  }

  // Initialize all strips
  left_fin_strip->begin();
  tongue_strip->begin();
  right_fin_strip->begin();
  scale_strip->begin();

  // Clear all LEDs
  clearAllStrips();
  showAllStrips();

  last_update_time = millis();
  return true;
}

void LedController::update(){
  unsigned long current_time = millis();
  
  if(current_time - last_update_time >= update_interval_ms){
    updateRainbowEffect();
    last_update_time = current_time;
  }
}

void LedController::updateRainbowEffect(){
  // Update hue offset for smooth rainbow cycling
  hue_offset += hue_speed;
  if(hue_offset >= 360.0f){
    hue_offset -= 360.0f;
  }

  // Apply rainbow effect - RGB only for fins/scales, WRGB for tongue
  for(int i = 0; i < LEFT_FIN_LED_COUNT; i++){
    float hue = fmod(hue_offset + (i * 360.0f / LEFT_FIN_LED_COUNT), 360.0f);
    uint32_t color = hsvToWrgbNoWhite(hue, 1.0f, 0.8f); // No white channel
    left_fin_strip->setPixelColor(i, color);
  }

  for(int i = 0; i < TONGUE_LED_COUNT; i++){
    float hue = fmod(hue_offset + (i * 360.0f / TONGUE_LED_COUNT), 360.0f);
    uint32_t color = hsvToWrgb(hue, 1.0f, 0.8f); // Use white channel for tongue
    tongue_strip->setPixelColor(i, color);
  }

  for(int i = 0; i < RIGHT_FIN_LED_COUNT; i++){
    float hue = fmod(hue_offset + (i * 360.0f / RIGHT_FIN_LED_COUNT), 360.0f);
    uint32_t color = hsvToWrgbNoWhite(hue, 1.0f, 0.8f); // No white channel
    right_fin_strip->setPixelColor(i, color);
  }

  for(int i = 0; i < scale_led_count; i++){
    float hue = fmod(hue_offset + (i * 360.0f / scale_led_count), 360.0f);
    uint32_t color = hsvToWrgbNoWhite(hue, 1.0f, 0.8f); // No white channel
    scale_strip->setPixelColor(i, color);
  }

  showAllStrips();
}

uint32_t LedController::hsvToRgb(float hue, float saturation, float value){
  float c = value * saturation;
  float x = c * (1.0f - fabs(fmod(hue / 60.0f, 2.0f) - 1.0f));
  float m = value - c;

  float r_prime, g_prime, b_prime;

  if(hue >= 0 && hue < 60){
    r_prime = c; g_prime = x; b_prime = 0;
  }else if(hue >= 60 && hue < 120){
    r_prime = x; g_prime = c; b_prime = 0;
  }else if(hue >= 120 && hue < 180){
    r_prime = 0; g_prime = c; b_prime = x;
  }else if(hue >= 180 && hue < 240){
    r_prime = 0; g_prime = x; b_prime = c;
  }else if(hue >= 240 && hue < 300){
    r_prime = x; g_prime = 0; b_prime = c;
  }else{
    r_prime = c; g_prime = 0; b_prime = x;
  }

  uint8_t r = (uint8_t)((r_prime + m) * 255);
  uint8_t g = (uint8_t)((g_prime + m) * 255);
  uint8_t b = (uint8_t)((b_prime + m) * 255);

  return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

uint32_t LedController::hsvToWrgb(float hue, float saturation, float value){
  float c = value * saturation;
  float x = c * (1.0f - fabs(fmod(hue / 60.0f, 2.0f) - 1.0f));
  float m = value - c;

  float r_prime, g_prime, b_prime;

  if(hue >= 0 && hue < 60){
    r_prime = c; g_prime = x; b_prime = 0;
  }else if(hue >= 60 && hue < 120){
    r_prime = x; g_prime = c; b_prime = 0;
  }else if(hue >= 120 && hue < 180){
    r_prime = 0; g_prime = c; b_prime = x;
  }else if(hue >= 180 && hue < 240){
    r_prime = 0; g_prime = x; b_prime = c;
  }else if(hue >= 240 && hue < 300){
    r_prime = x; g_prime = 0; b_prime = c;
  }else{
    r_prime = c; g_prime = 0; b_prime = x;
  }

  uint8_t r = (uint8_t)((r_prime + m) * 255);
  uint8_t g = (uint8_t)((g_prime + m) * 255);
  uint8_t b = (uint8_t)((b_prime + m) * 255);
  
  // For WRGB LEDs, calculate white channel for better color mixing
  // Use the minimum of RGB values as white component for more vivid colors
  uint8_t w = (uint8_t)(m * 255 * 0.3f); // Reduced white for more saturated colors
  
  // WRGB format: White, Red, Green, Blue (may vary by LED type)
  return ((uint32_t)w << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

uint32_t LedController::hsvToWrgbNoWhite(float hue, float saturation, float value){
  float c = value * saturation;
  float x = c * (1.0f - fabs(fmod(hue / 60.0f, 2.0f) - 1.0f));
  float m = value - c;

  float r_prime, g_prime, b_prime;

  if(hue >= 0 && hue < 60){
    r_prime = c; g_prime = x; b_prime = 0;
  }else if(hue >= 60 && hue < 120){
    r_prime = x; g_prime = c; b_prime = 0;
  }else if(hue >= 120 && hue < 180){
    r_prime = 0; g_prime = c; b_prime = x;
  }else if(hue >= 180 && hue < 240){
    r_prime = 0; g_prime = x; b_prime = c;
  }else if(hue >= 240 && hue < 300){
    r_prime = x; g_prime = 0; b_prime = c;
  }else{
    r_prime = c; g_prime = 0; b_prime = x;
  }

  uint8_t r = (uint8_t)((r_prime + m) * 255);
  uint8_t g = (uint8_t)((g_prime + m) * 255);
  uint8_t b = (uint8_t)((b_prime + m) * 255);
  
  // WRGB format but with white channel disabled (set to 0)
  uint8_t w = 0; // No white channel for pure RGB colors
  
  // WRGB format: White, Red, Green, Blue
  return ((uint32_t)w << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

void LedController::setRainbowSpeed(float speed){
  hue_speed = speed;
}

void LedController::setUpdateInterval(unsigned long interval_ms){
  update_interval_ms = interval_ms;
}

void LedController::setScaleLedCount(int count){
  if(count > 0){
    scale_led_count = count;
    
    // If scale strip is already initialized, recreate it with new count
    if(scale_strip){
      delete scale_strip;
      scale_strip = new Adafruit_NeoPixel(scale_led_count, LED_STRIP_5_PIN, NEO_WRGB + NEO_KHZ800);
      scale_strip->begin();
      scale_strip->clear();
      scale_strip->show();
    }
  }
}

void LedController::setLeftFinColor(uint32_t color){
  for(int i = 0; i < LEFT_FIN_LED_COUNT; i++){
    left_fin_strip->setPixelColor(i, color);
  }
  left_fin_strip->show();
}

void LedController::setTongueColor(uint32_t color){
  for(int i = 0; i < TONGUE_LED_COUNT; i++){
    tongue_strip->setPixelColor(i, color);
  }
  tongue_strip->show();
}

void LedController::setRightFinColor(uint32_t color){
  for(int i = 0; i < RIGHT_FIN_LED_COUNT; i++){
    right_fin_strip->setPixelColor(i, color);
  }
  right_fin_strip->show();
}

void LedController::setScaleColor(uint32_t color){
  for(int i = 0; i < scale_led_count; i++){
    scale_strip->setPixelColor(i, color);
  }
  scale_strip->show();
}

void LedController::setAllStripsColor(uint32_t color){
  setLeftFinColor(color);
  setTongueColor(color);
  setRightFinColor(color);
  setScaleColor(color);
}

void LedController::clearAllStrips(){
  left_fin_strip->clear();
  tongue_strip->clear();
  right_fin_strip->clear();
  scale_strip->clear();
}

void LedController::showAllStrips(){
  left_fin_strip->show();
  tongue_strip->show();
  right_fin_strip->show();
  scale_strip->show();
}

void LedController::runRainbowCycle(){
  static float cycle_hue = 0.0f;
  
  for(int i = 0; i < LEFT_FIN_LED_COUNT; i++){
    float hue = fmod(cycle_hue + (i * 360.0f / LEFT_FIN_LED_COUNT), 360.0f);
    left_fin_strip->setPixelColor(i, hsvToWrgbNoWhite(hue, 1.0f, 0.8f)); // No white
  }
  
  for(int i = 0; i < TONGUE_LED_COUNT; i++){
    float hue = fmod(cycle_hue + (i * 360.0f / TONGUE_LED_COUNT), 360.0f);
    tongue_strip->setPixelColor(i, hsvToWrgb(hue, 1.0f, 0.8f)); // Use white for tongue
  }
  
  for(int i = 0; i < RIGHT_FIN_LED_COUNT; i++){
    float hue = fmod(cycle_hue + (i * 360.0f / RIGHT_FIN_LED_COUNT), 360.0f);
    right_fin_strip->setPixelColor(i, hsvToWrgbNoWhite(hue, 1.0f, 0.8f)); // No white
  }
  
  for(int i = 0; i < scale_led_count; i++){
    float hue = fmod(cycle_hue + (i * 360.0f / scale_led_count), 360.0f);
    scale_strip->setPixelColor(i, hsvToWrgbNoWhite(hue, 1.0f, 0.8f)); // No white
  }
  
  showAllStrips();
  
  cycle_hue += 2.0f;
  if(cycle_hue >= 360.0f){
    cycle_hue = 0.0f;
  }
}

void LedController::runChaseEffect(uint32_t color, int delay_ms){
  static int chase_position = 0;
  static unsigned long last_chase_time = 0;
  
  if(millis() - last_chase_time >= delay_ms){
    clearAllStrips();
    
    // Chase effect on each strip
    left_fin_strip->setPixelColor(chase_position % LEFT_FIN_LED_COUNT, color);
    tongue_strip->setPixelColor(chase_position % TONGUE_LED_COUNT, color);
    right_fin_strip->setPixelColor(chase_position % RIGHT_FIN_LED_COUNT, color);
    scale_strip->setPixelColor(chase_position % scale_led_count, color);
    
    showAllStrips();
    
    chase_position++;
    last_chase_time = millis();
  }
}

void LedController::runBreathingEffect(uint32_t color, float breathing_speed){
  static float breath_phase = 0.0f;
  
  float brightness = (sin(breath_phase) + 1.0f) / 2.0f; // 0.0 to 1.0
  
  // Extract RGB components from color
  uint8_t r = (color >> 16) & 0xFF;
  uint8_t g = (color >> 8) & 0xFF;
  uint8_t b = color & 0xFF;
  
  // Apply brightness
  uint32_t dimmed_color = ((uint32_t)(r * brightness) << 16) |
                          ((uint32_t)(g * brightness) << 8) |
                          (uint32_t)(b * brightness);
  
  setAllStripsColor(dimmed_color);
  
  breath_phase += breathing_speed;
  if(breath_phase >= 2.0f * PI){
    breath_phase = 0.0f;
  }
}

#endif // LED_CONTROLLER_IMPL_HPP