#include <Arduino.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <SPI.h>
#include <SD.h>

// --- HUB75 pin definitions ---
#define HUB75_R0 7
#define HUB75_G0 15
#define HUB75_B0 16
#define HUB75_R1 17
#define HUB75_G1 18
#define HUB75_B1 8
#define HUB75_A  41
#define HUB75_B  40
#define HUB75_C  39
#define HUB75_D  38
#define HUB75_E  42
#define HUB75_LAT 36
#define HUB75_OE0 35 // Panel 0
#define HUB75_OE1 6  // Panel 1
#define HUB75_CLK 37

// --- SD Card SPI pin definitions ---
#define SD_MOSI 21
#define SD_MISO 48
#define SD_SCK  47
#define SD_CS   14

// --- Panel configuration ---
#define PANEL_RES_X 64
#define PANEL_RES_Y 32
#define PANEL_CHAIN 1

// HUB75 configuration for shared data pins
struct HUB75Config{
  uint8_t r0_pin;
  uint8_t g0_pin;
  uint8_t b0_pin;
  uint8_t r1_pin;
  uint8_t g1_pin;
  uint8_t b1_pin;
  uint8_t a_pin;
  uint8_t b_pin;
  uint8_t c_pin;
  uint8_t d_pin;
  uint8_t e_pin;
  uint8_t lat_pin;
  uint8_t oe_pin;
  uint8_t clk_pin;
};

// Panel instances
MatrixPanel_I2S_DMA* panel0 = nullptr;
MatrixPanel_I2S_DMA* panel1 = nullptr;

// Function declarations
bool initializePanels();
bool initializeSDCard();
void setupPanelConfig(HUB75_I2S_CFG::i2s_pins& pins, uint8_t oe_pin);
void testPanels();
void testSDCard();

void setup(){
  Serial.begin(115200);
  Serial.println("Starting HUB75 dual panel and SD card initialization...");
  
  // Initialize panels
  if(!initializePanels()){
    Serial.println("ERROR: Failed to initialize HUB75 panels");
    return;
  }
  
  // Initialize SD card
  if(!initializeSDCard()){
    Serial.println("ERROR: Failed to initialize SD card");
    return;
  }
  
  Serial.println("Initialization complete!");
  
  // Run tests
  testPanels();
  testSDCard();
}

void loop(){
  // Main loop - add your application logic here
  delay(1000);
}

bool initializePanels(){
  Serial.println("Initializing HUB75 panels...");
  
  // Configure panel 0 (OE pin 35)
  HUB75_I2S_CFG::i2s_pins pins0;
  setupPanelConfig(pins0, HUB75_OE0);
  
  HUB75_I2S_CFG config0(
    PANEL_RES_X,
    PANEL_RES_Y,
    PANEL_CHAIN,
    pins0
  );
  
  panel0 = new MatrixPanel_I2S_DMA(config0);
  if(!panel0->begin()){
    Serial.println("ERROR: Panel 0 initialization failed");
    return false;
  }
  
  // Configure panel 1 (OE pin 6)
  HUB75_I2S_CFG::i2s_pins pins1;
  setupPanelConfig(pins1, HUB75_OE1);
  
  HUB75_I2S_CFG config1(
    PANEL_RES_X,
    PANEL_RES_Y,
    PANEL_CHAIN,
    pins1
  );
  
  panel1 = new MatrixPanel_I2S_DMA(config1);
  if(!panel1->begin()){
    Serial.println("ERROR: Panel 1 initialization failed");
    return false;
  }
  
  Serial.println("HUB75 panels initialized successfully");
  return true;
}

void setupPanelConfig(HUB75_I2S_CFG::i2s_pins& pins, uint8_t oe_pin){
  pins.r1 = HUB75_R0;
  pins.g1 = HUB75_G0;
  pins.b1 = HUB75_B0;
  pins.r2 = HUB75_R1;
  pins.g2 = HUB75_G1;
  pins.b2 = HUB75_B1;
  pins.a = HUB75_A;
  pins.b = HUB75_B;
  pins.c = HUB75_C;
  pins.d = HUB75_D;
  pins.e = HUB75_E;
  pins.lat = HUB75_LAT;
  pins.oe = oe_pin;  // Different OE pin for each panel
  pins.clk = HUB75_CLK;
}

bool initializeSDCard(){
  Serial.println("Initializing SD card...");
  
  // Configure SPI for SD card
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  
  if(!SD.begin(SD_CS)){
    Serial.println("ERROR: SD card mount failed");
    return false;
  }
  
  uint8_t card_type = SD.cardType();
  if(card_type == CARD_NONE){
    Serial.println("ERROR: No SD card attached");
    return false;
  }
  
  Serial.print("SD card type: ");
  if(card_type == CARD_MMC){
    Serial.println("MMC");
  }else if(card_type == CARD_SD){
    Serial.println("SDSC");
  }else if(card_type == CARD_SDHC){
    Serial.println("SDHC");
  }else{
    Serial.println("UNKNOWN");
  }
  
  uint64_t card_size = SD.cardSize() / (1024 * 1024);
  Serial.printf("SD card size: %lluMB\n", card_size);
  
  Serial.println("SD card initialized successfully");
  return true;
}

void testPanels(){
  Serial.println("Testing HUB75 panels...");
  
  if(panel0 && panel1){
    // Clear both panels
    panel0->fillScreen(0);
    panel1->fillScreen(0);
    
    // Test panel 0 - Red
    panel0->fillScreen(panel0->color565(255, 0, 0));
    panel0->setCursor(5, 5);
    panel0->setTextColor(panel0->color565(255, 255, 255));
    panel0->print("P0");
    
    // Test panel 1 - Blue  
    panel1->fillScreen(panel1->color565(0, 0, 255));
    panel1->setCursor(5, 5);
    panel1->setTextColor(panel1->color565(255, 255, 255));
    panel1->print("P1");
    
    Serial.println("Panel test complete - Panel 0 should be red, Panel 1 should be blue");
  }
}

void testSDCard(){
  Serial.println("Testing SD card...");
  
  File test_file = SD.open("/test.txt", FILE_WRITE);
  if(test_file){
    test_file.println("HUB75 and SD card test successful!");
    test_file.close();
    Serial.println("Test file written to SD card");
    
    // Read back the file
    test_file = SD.open("/test.txt");
    if(test_file){
      Serial.println("Test file content:");
      while(test_file.available()){
        Serial.write(test_file.read());
      }
      test_file.close();
    }
  }else{
    Serial.println("ERROR: Failed to create test file");
  }
}