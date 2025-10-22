#ifndef LED_CONTROLLER_NEW_IMPL_HPP
#define LED_CONTROLLER_NEW_IMPL_HPP

#include "LedController_new.h"

// Constructor
LedController::LedController()
  : left_fin_strip(nullptr),
    right_fin_strip(nullptr),
    tongue_strip(nullptr),
    scale_strip(nullptr),
    is_initialized(false){
}

// Destructor
LedController::~LedController(){
  if(left_fin_strip != nullptr){
    delete left_fin_strip;
  }
  if(right_fin_strip != nullptr){
    delete right_fin_strip;
  }
  if(tongue_strip != nullptr){
    delete tongue_strip;
  }
  if(scale_strip != nullptr){
    delete scale_strip;
  }
}

// Initialize all LED strips
bool LedController::initialize(){
  Serial.println("Initializing LED Controller...");

  // Create NeoPixel objects with RGBW format
  // NEO_GRBW + NEO_KHZ800 for SK6812 RGBW LEDs
  left_fin_strip = new Adafruit_NeoPixel(LEFT_FIN_LED_COUNT, LEFT_FIN_PIN, 
                                          NEO_GRBW + NEO_KHZ800);
  right_fin_strip = new Adafruit_NeoPixel(RIGHT_FIN_LED_COUNT, RIGHT_FIN_PIN, 
                                           NEO_GRBW + NEO_KHZ800);
  tongue_strip = new Adafruit_NeoPixel(TONGUE_LED_COUNT, TONGUE_PIN, 
                                        NEO_GRBW + NEO_KHZ800);
  scale_strip = new Adafruit_NeoPixel(SCALE_LED_COUNT, SCALE_PIN, 
                                       NEO_GRBW + NEO_KHZ800);

  // Check allocation
  if(!left_fin_strip || !right_fin_strip || !tongue_strip || !scale_strip){
    Serial.println("Error: Failed to allocate NeoPixel objects");
    return false;
  }

  // Initialize strips
  left_fin_strip->begin();
  right_fin_strip->begin();
  tongue_strip->begin();
  scale_strip->begin();

  // Clear all strips
  clearAll();
  showAll();

  is_initialized = true;
  
  Serial.println("LED Controller initialized successfully");
  Serial.printf("  Left Fin:  %d LEDs on GPIO %d\n", LEFT_FIN_LED_COUNT, LEFT_FIN_PIN);
  Serial.printf("  Right Fin: %d LEDs on GPIO %d\n", RIGHT_FIN_LED_COUNT, RIGHT_FIN_PIN);
  Serial.printf("  Tongue:    %d LEDs on GPIO %d\n", TONGUE_LED_COUNT, TONGUE_PIN);
  Serial.printf("  Scale:     %d LEDs on GPIO %d\n", SCALE_LED_COUNT, SCALE_PIN);

  return true;
}

// Update all strips from UART data
void LedController::updateFromUartData(const uint8_t* left_fin_data,
                                        const uint8_t* right_fin_data,
                                        const uint8_t* tongue_data,
                                        const uint8_t* scale_data){
  if(!is_initialized){
    Serial.println("ERROR: LED Controller not initialized!");
    return;
  }

  updateLeftFin(left_fin_data);
  updateRightFin(right_fin_data);
  updateTongue(tongue_data);
  updateScale(scale_data);
  
  showAll();
  
  // Debug: Print first LED value to verify update
  static int update_count = 0;
  if(++update_count % 10 == 0){
    Serial.printf("\n[LED Update #%d] First LED: R=%d G=%d B=%d W=%d\n", 
                  update_count, left_fin_data[0], left_fin_data[1], 
                  left_fin_data[2], left_fin_data[3]);
  }
}

// Update left fin from RGBW data
void LedController::updateLeftFin(const uint8_t* rgbw_data){
  if(!is_initialized || !rgbw_data){
    return;
  }

  for(int i = 0; i < LEFT_FIN_LED_COUNT; i++){
    int base = i * 4;
    uint8_t r = rgbw_data[base];
    uint8_t g = rgbw_data[base + 1];
    uint8_t b = rgbw_data[base + 2];
    uint8_t w = rgbw_data[base + 3];
    left_fin_strip->setPixelColor(i, r, g, b, w);
  }
}

// Update right fin from RGBW data
void LedController::updateRightFin(const uint8_t* rgbw_data){
  if(!is_initialized || !rgbw_data){
    return;
  }

  for(int i = 0; i < RIGHT_FIN_LED_COUNT; i++){
    int base = i * 4;
    uint8_t r = rgbw_data[base];
    uint8_t g = rgbw_data[base + 1];
    uint8_t b = rgbw_data[base + 2];
    uint8_t w = rgbw_data[base + 3];
    right_fin_strip->setPixelColor(i, r, g, b, w);
  }
}

// Update tongue from RGBW data
void LedController::updateTongue(const uint8_t* rgbw_data){
  if(!is_initialized || !rgbw_data){
    return;
  }

  for(int i = 0; i < TONGUE_LED_COUNT; i++){
    int base = i * 4;
    uint8_t r = rgbw_data[base];
    uint8_t g = rgbw_data[base + 1];
    uint8_t b = rgbw_data[base + 2];
    uint8_t w = rgbw_data[base + 3];
    tongue_strip->setPixelColor(i, r, g, b, w);
  }
}

// Update scale from RGBW data
void LedController::updateScale(const uint8_t* rgbw_data){
  if(!is_initialized || !rgbw_data){
    return;
  }

  for(int i = 0; i < SCALE_LED_COUNT; i++){
    int base = i * 4;
    uint8_t r = rgbw_data[base];
    uint8_t g = rgbw_data[base + 1];
    uint8_t b = rgbw_data[base + 2];
    uint8_t w = rgbw_data[base + 3];
    scale_strip->setPixelColor(i, r, g, b, w);
  }
}

// Set individual LED on left fin
void LedController::setLeftFinLed(int index, uint8_t r, uint8_t g, uint8_t b, uint8_t w){
  if(!is_initialized || index < 0 || index >= LEFT_FIN_LED_COUNT){
    return;
  }
  left_fin_strip->setPixelColor(index, r, g, b, w);
}

// Set individual LED on right fin
void LedController::setRightFinLed(int index, uint8_t r, uint8_t g, uint8_t b, uint8_t w){
  if(!is_initialized || index < 0 || index >= RIGHT_FIN_LED_COUNT){
    return;
  }
  right_fin_strip->setPixelColor(index, r, g, b, w);
}

// Set individual LED on tongue
void LedController::setTongueLed(int index, uint8_t r, uint8_t g, uint8_t b, uint8_t w){
  if(!is_initialized || index < 0 || index >= TONGUE_LED_COUNT){
    return;
  }
  tongue_strip->setPixelColor(index, r, g, b, w);
}

// Set individual LED on scale
void LedController::setScaleLed(int index, uint8_t r, uint8_t g, uint8_t b, uint8_t w){
  if(!is_initialized || index < 0 || index >= SCALE_LED_COUNT){
    return;
  }
  scale_strip->setPixelColor(index, r, g, b, w);
}

// Set entire left fin to single color
void LedController::setLeftFinColor(uint8_t r, uint8_t g, uint8_t b, uint8_t w){
  if(!is_initialized){
    return;
  }
  for(int i = 0; i < LEFT_FIN_LED_COUNT; i++){
    left_fin_strip->setPixelColor(i, r, g, b, w);
  }
}

// Set entire right fin to single color
void LedController::setRightFinColor(uint8_t r, uint8_t g, uint8_t b, uint8_t w){
  if(!is_initialized){
    return;
  }
  for(int i = 0; i < RIGHT_FIN_LED_COUNT; i++){
    right_fin_strip->setPixelColor(i, r, g, b, w);
  }
}

// Set entire tongue to single color
void LedController::setTongueColor(uint8_t r, uint8_t g, uint8_t b, uint8_t w){
  if(!is_initialized){
    return;
  }
  for(int i = 0; i < TONGUE_LED_COUNT; i++){
    tongue_strip->setPixelColor(i, r, g, b, w);
  }
}

// Set entire scale to single color
void LedController::setScaleColor(uint8_t r, uint8_t g, uint8_t b, uint8_t w){
  if(!is_initialized){
    return;
  }
  for(int i = 0; i < SCALE_LED_COUNT; i++){
    scale_strip->setPixelColor(i, r, g, b, w);
  }
}

// Show all strips
void LedController::showAll(){
  if(!is_initialized){
    return;
  }
  left_fin_strip->show();
  right_fin_strip->show();
  tongue_strip->show();
  scale_strip->show();
}

// Clear all strips
void LedController::clearAll(){
  if(!is_initialized){
    return;
  }
  left_fin_strip->clear();
  right_fin_strip->clear();
  tongue_strip->clear();
  scale_strip->clear();
}

// Test pattern
void LedController::testPattern(){
  if(!is_initialized){
    return;
  }

  Serial.println("Running LED test pattern...");

  // Test left fin - Red
  setLeftFinColor(255, 0, 0, 0);
  showAll();
  delay(500);

  // Test right fin - Green
  setRightFinColor(0, 255, 0, 0);
  showAll();
  delay(500);

  // Test tongue - Blue
  setTongueColor(0, 0, 255, 0);
  showAll();
  delay(500);

  // Test scale - White
  setScaleColor(0, 0, 0, 255);
  showAll();
  delay(500);

  // All off
  clearAll();
  showAll();
}

// Rainbow cycle effect
void LedController::rainbowCycle(uint8_t wait){
  if(!is_initialized){
    return;
  }

  uint16_t i, j;

  for(j = 0; j < 256; j++){
    // Left fin
    for(i = 0; i < LEFT_FIN_LED_COUNT; i++){
      uint32_t c = left_fin_strip->ColorHSV(((i * 256 / LEFT_FIN_LED_COUNT) + j) & 255);
      left_fin_strip->setPixelColor(i, c);
    }
    // Right fin
    for(i = 0; i < RIGHT_FIN_LED_COUNT; i++){
      uint32_t c = right_fin_strip->ColorHSV(((i * 256 / RIGHT_FIN_LED_COUNT) + j) & 255);
      right_fin_strip->setPixelColor(i, c);
    }
    // Tongue
    for(i = 0; i < TONGUE_LED_COUNT; i++){
      uint32_t c = tongue_strip->ColorHSV(((i * 256 / TONGUE_LED_COUNT) + j) & 255);
      tongue_strip->setPixelColor(i, c);
    }
    // Scale
    for(i = 0; i < SCALE_LED_COUNT; i++){
      uint32_t c = scale_strip->ColorHSV(((i * 256 / SCALE_LED_COUNT) + j) & 255);
      scale_strip->setPixelColor(i, c);
    }
    showAll();
    delay(wait);
  }
}

#endif // LED_CONTROLLER_NEW_IMPL_HPP
