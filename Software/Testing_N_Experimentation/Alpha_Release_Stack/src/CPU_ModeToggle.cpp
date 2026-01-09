/*****************************************************************
 * File:      CPU_ModeToggle.cpp
 * Category:  src
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Test program for system modes with mode-specific handlers.
 *    
 *    STARTUP MODES:
 *    - Hold A+D during power-on = System Test Loop Mode
 *      (Runs all tests in infinite loop with 5s gaps)
 *    - Hold A only during power-on = Debug Menu Mode
 *      (Interactive console menu for individual tests)
 *    
 *    RUNTIME BUTTONS:
 *    Button A (GPIO 5)  = Boot Mode
 *    Button B (GPIO 6)  = Running Mode
 *    Button C (GPIO 7)  = Debug Mode (runtime)
 *    Button D (GPIO 15) = System Test Mode (runs once, returns)
 *    
 *    DEBUG MENU CONTROLS:
 *    Button A = Previous (navigate up)
 *    Button B = Select/Set
 *    Button C = Next (navigate down)
 *    Button D = Cancel/Back
 *
 *    System Test mode runs comprehensive HAL tests:
 *    - Sensors: ICM20948, BME280, INMP441, Buttons, NEO-8M GPS
 *    - LED Strips: Left Fin, Right Fin, Scale, Tongue (RGBW)
 *    - GPU: Communication, HUB75 patterns, OLED patterns
 *    - Fans: On/Off test
 *****************************************************************/

#include <cstdio>
#include <cstring>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/i2s_std.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <cstdlib>
#include <cmath>

#include "SystemAPI/SystemMode.hpp"
#include "SystemAPI/HalTest.hpp"

static const char* TAG = "MODE_TOGGLE";

// ============================================================
// Button Pin Definitions (Active LOW)
// ============================================================
constexpr gpio_num_t BUTTON_A = GPIO_NUM_5;   // Prev / Boot mode
constexpr gpio_num_t BUTTON_B = GPIO_NUM_6;   // Select / Running mode
constexpr gpio_num_t BUTTON_C = GPIO_NUM_7;   // Next / Debug mode
constexpr gpio_num_t BUTTON_D = GPIO_NUM_15;  // Cancel / System Test mode

// ============================================================
// Button State Tracking
// ============================================================
struct ButtonState {
  bool lastState = true;  // Pull-up, so idle = HIGH
  bool pressed = false;
};

static ButtonState btnA, btnB, btnC, btnD;

// ============================================================
// Mode-specific state variables
// ============================================================
static float bootProgress = 0.0f;
static float runningTime = 0.0f;
static int debugFrameCount = 0;

// ============================================================
// Startup Mode Tracking (determines button behavior)
// ============================================================
enum class StartupMode {
  NORMAL,           // Normal boot - buttons control runtime modes
  DEBUG_MENU,       // A held at startup - buttons navigate debug menu
  SYSTEM_TEST_LOOP  // A+D held at startup - no button control, just loops
};
static StartupMode g_startupMode = StartupMode::NORMAL;

// ============================================================
// Initialize GPIO for buttons
// ============================================================
static void initButtons(){
  gpio_config_t io_conf = {};
  io_conf.intr_type = GPIO_INTR_DISABLE;
  io_conf.mode = GPIO_MODE_INPUT;
  io_conf.pin_bit_mask = (1ULL << BUTTON_A) | (1ULL << BUTTON_B) | 
                         (1ULL << BUTTON_C) | (1ULL << BUTTON_D);
  io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
  io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
  gpio_config(&io_conf);
  
  ESP_LOGI(TAG, "Buttons initialized: A=GPIO%d, B=GPIO%d, C=GPIO%d, D=GPIO%d",
           BUTTON_A, BUTTON_B, BUTTON_C, BUTTON_D);
}

// ============================================================
// Check button press (with debounce via edge detection)
// ============================================================
static bool checkButtonPress(gpio_num_t pin, ButtonState& state){
  bool currentState = gpio_get_level(pin);
  
  // Detect falling edge (button pressed, active LOW)
  if(!currentState && state.lastState){
    state.lastState = currentState;
    return true;
  }
  
  state.lastState = currentState;
  return false;
}

// ============================================================
// Wait for any button press (blocking) - Rising Edge Only
// Returns: 'A', 'B', 'C', 'D' or 0 if timeout
// Implements 100ms minimum between button actions
// ============================================================
static uint32_t lastButtonActionTime = 0;
constexpr uint32_t BUTTON_DEBOUNCE_MS = 100;  // 100ms between actions

static char waitForButton(uint32_t timeoutMs = 0){
  // Track previous states for edge detection
  bool prevA = true, prevB = true, prevC = true, prevD = true;
  
  // Initialize with current state to avoid false trigger on entry
  prevA = gpio_get_level(BUTTON_A);
  prevB = gpio_get_level(BUTTON_B);
  prevC = gpio_get_level(BUTTON_C);
  prevD = gpio_get_level(BUTTON_D);
  
  uint32_t startTime = esp_timer_get_time() / 1000;
  
  while(true){
    uint32_t now = esp_timer_get_time() / 1000;
    
    // Read current button states
    bool currA = gpio_get_level(BUTTON_A);
    bool currB = gpio_get_level(BUTTON_B);
    bool currC = gpio_get_level(BUTTON_C);
    bool currD = gpio_get_level(BUTTON_D);
    
    // Check debounce timing
    bool canTrigger = (now - lastButtonActionTime) >= BUTTON_DEBOUNCE_MS;
    
    // Detect falling edge (HIGH->LOW, button press) with debounce
    if(canTrigger){
      if(prevA && !currA){ lastButtonActionTime = now; return 'A'; }
      if(prevB && !currB){ lastButtonActionTime = now; return 'B'; }
      if(prevC && !currC){ lastButtonActionTime = now; return 'C'; }
      if(prevD && !currD){ lastButtonActionTime = now; return 'D'; }
    }
    
    // Update previous states
    prevA = currA;
    prevB = currB;
    prevC = currC;
    prevD = currD;
    
    // Check timeout
    if(timeoutMs > 0){
      uint32_t elapsed = now - startTime;
      if(elapsed >= timeoutMs) return 0;
    }
    
    vTaskDelay(pdMS_TO_TICKS(20));  // Poll every 20ms
  }
}

// ============================================================
// Debug Menu System
// ============================================================

// Menu item structure
struct MenuItem {
  const char* label;
  void (*action)();
  MenuItem* submenu;
  int submenuCount;
};

// Forward declarations for menu actions
static void actionShowAllSensors();
static void actionShowIMU();
static void actionShowBME280();
static void actionShowMicrophone();
static void actionShowGPS();
static void actionShowButtons();

static void actionHUB75HSLPattern();
static void actionHUB75GrayscalePattern();
static void actionHUB75OrientationArrows();
static void actionHUB75AllPatterns();

static void actionOLEDCheckerPattern();
static void actionOLEDStripesPattern();
static void actionOLEDOrientationArrows();
static void actionOLEDAllPatterns();

static void actionLEDLeftFin();
static void actionLEDRightFin();
static void actionLEDScale();
static void actionLEDTongue();
static void actionLEDAllStrips();

static void actionRunFullTest();

// Submenu: Sensors
static MenuItem sensorSubmenu[] = {
  {"Show All Sensors", actionShowAllSensors, nullptr, 0},
  {"IMU (ICM20948)", actionShowIMU, nullptr, 0},
  {"Environment (BME280)", actionShowBME280, nullptr, 0},
  {"Microphone (INMP441)", actionShowMicrophone, nullptr, 0},
  {"GPS (NEO-8M)", actionShowGPS, nullptr, 0},
  {"Buttons State", actionShowButtons, nullptr, 0},
};

// Submenu: HUB75 Display
static MenuItem hub75Submenu[] = {
  {"All Patterns (cycle)", actionHUB75AllPatterns, nullptr, 0},
  {"HSL + Grayscale", actionHUB75HSLPattern, nullptr, 0},
  {"Grayscale Only", actionHUB75GrayscalePattern, nullptr, 0},
  {"Orientation Arrows", actionHUB75OrientationArrows, nullptr, 0},
};

// Submenu: OLED Display
static MenuItem oledSubmenu[] = {
  {"All Patterns (cycle)", actionOLEDAllPatterns, nullptr, 0},
  {"Checker + H-Stripes", actionOLEDCheckerPattern, nullptr, 0},
  {"V-Stripes + Diagonal", actionOLEDStripesPattern, nullptr, 0},
  {"Orientation Arrows", actionOLEDOrientationArrows, nullptr, 0},
};

// Submenu: LED Strips
static MenuItem ledSubmenu[] = {
  {"All Strips (cycle)", actionLEDAllStrips, nullptr, 0},
  {"Left Fin", actionLEDLeftFin, nullptr, 0},
  {"Right Fin", actionLEDRightFin, nullptr, 0},
  {"Scale", actionLEDScale, nullptr, 0},
  {"Tongue", actionLEDTongue, nullptr, 0},
};

// Main menu
static MenuItem mainMenu[] = {
  {"Sensors", nullptr, sensorSubmenu, 6},
  {"HUB75 Display", nullptr, hub75Submenu, 4},
  {"OLED Display", nullptr, oledSubmenu, 4},
  {"LED Strips", nullptr, ledSubmenu, 5},
  {"Run Full Test", actionRunFullTest, nullptr, 0},
};
static const int mainMenuCount = 5;

// ============================================================
// Menu Display & Navigation
// ============================================================
static void printMenuHeader(){
  printf("\n");
  printf("╔══════════════════════════════════════════════════════════╗\n");
  printf("║           DEBUG MENU - Interactive Console               ║\n");
  printf("╠══════════════════════════════════════════════════════════╣\n");
  printf("║  A = Prev   B = Select   C = Next   D = Back/Exit        ║\n");
  printf("╚══════════════════════════════════════════════════════════╝\n");
}

static void printMenu(MenuItem* items, int count, int selected, const char* title){
  printf("\n");
  printf("┌──────────────────────────────────────────────────────────┐\n");
  printf("│  %-56s│\n", title);
  printf("├──────────────────────────────────────────────────────────┤\n");
  
  for(int i = 0; i < count; i++){
    if(i == selected){
      printf("│  > %-52s  │\n", items[i].label);
    }else{
      printf("│    %-52s  │\n", items[i].label);
    }
  }
  
  printf("└──────────────────────────────────────────────────────────┘\n");
  printf("  Selection: %d/%d\n", selected + 1, count);
}

// Navigate a menu (returns when D pressed or action completed)
// Supports nested submenus with proper back navigation
static void navigateMenu(MenuItem* items, int count, const char* title){
  int selected = 0;
  bool needRedraw = true;
  
  while(true){
    // Only redraw menu when needed (after action or navigation)
    if(needRedraw){
      printMenu(items, count, selected, title);
      needRedraw = false;
    }
    
    char btn = waitForButton();
    
    switch(btn){
      case 'A':  // Previous
        selected--;
        if(selected < 0) selected = count - 1;
        needRedraw = true;
        break;
        
      case 'C':  // Next
        selected++;
        if(selected >= count) selected = 0;
        needRedraw = true;
        break;
        
      case 'B':  // Select
        if(items[selected].submenu != nullptr){
          // Enter submenu (recursive call creates new layer)
          printf("\n  Entering: %s\n", items[selected].label);
          navigateMenu(items[selected].submenu, items[selected].submenuCount, 
                       items[selected].label);
          // When we return from submenu, redraw current menu
          needRedraw = true;
        }else if(items[selected].action != nullptr){
          // Execute action
          printf("\n  Executing: %s\n", items[selected].label);
          printf("  ────────────────────────────────────────\n");
          items[selected].action();
          printf("  ────────────────────────────────────────\n");
          printf("  Press any button to continue...\n");
          waitForButton();
          needRedraw = true;
        }
        break;
        
      case 'D':  // Back/Cancel - return to parent menu
        printf("\n  Back to parent menu...\n");
        return;
    }
  }
}

// ============================================================
// Menu Action Implementations
// ============================================================

// Helper to get HalTestRunner reference
static SystemAPI::HalTest::HalTestRunner& getTestRunner(){
  static SystemAPI::HalTest::HalTestRunner runner;
  return runner;
}

// Helper: Check if any button is pressed (non-blocking)
// Returns true if any button is currently held OR was just pressed
static bool anyButtonPressed(){
  bool currA = gpio_get_level(BUTTON_A);
  bool currB = gpio_get_level(BUTTON_B);
  bool currC = gpio_get_level(BUTTON_C);
  bool currD = gpio_get_level(BUTTON_D);
  
  // Active LOW - return true if any button is being held
  return (!currA || !currB || !currC || !currD);
}

// Helper: Initialize I2C if not done
static bool s_i2cInitialized = false;
static void ensureI2CInit(){
  if(s_i2cInitialized) return;
  
  i2c_config_t conf = {};
  conf.mode = I2C_MODE_MASTER;
  conf.sda_io_num = GPIO_NUM_9;   // I2C_SDA
  conf.scl_io_num = GPIO_NUM_10;  // I2C_SCL
  conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
  conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
  conf.master.clk_speed = 400000;
  
  if(i2c_param_config(I2C_NUM_0, &conf) == ESP_OK){
    if(i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0) == ESP_OK ||
       true){  // May already be installed
      s_i2cInitialized = true;
    }
  }
}

// Helper: Write a byte to I2C register
static esp_err_t i2cWriteByte(uint8_t addr, uint8_t reg, uint8_t data){
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
  i2c_master_write_byte(cmd, reg, true);
  i2c_master_write_byte(cmd, data, true);
  i2c_master_stop(cmd);
  esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(100));
  i2c_cmd_link_delete(cmd);
  return ret;
}

// Helper: Read a byte from I2C register
static uint8_t i2cRead(uint8_t addr, uint8_t reg){
  uint8_t data = 0;
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
  i2c_master_write_byte(cmd, reg, true);
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_READ, true);
  i2c_master_read_byte(cmd, &data, I2C_MASTER_LAST_NACK);
  i2c_master_stop(cmd);
  i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(100));
  i2c_cmd_link_delete(cmd);
  return data;
}

// Helper: Read multiple bytes from I2C
static esp_err_t i2cReadBytes(uint8_t addr, uint8_t reg, uint8_t* buf, size_t len){
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
  i2c_master_write_byte(cmd, reg, true);
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_READ, true);
  if(len > 1){
    i2c_master_read(cmd, buf, len - 1, I2C_MASTER_ACK);
  }
  i2c_master_read_byte(cmd, buf + len - 1, I2C_MASTER_LAST_NACK);
  i2c_master_stop(cmd);
  esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(100));
  i2c_cmd_link_delete(cmd);
  return ret;
}

// ICM20948 initialization
static bool s_imuInitialized = false;
static void initICM20948(){
  if(s_imuInitialized) return;
  
  printf("  Initializing ICM20948...\n");
  
  // Check WHO_AM_I (should be 0xEA)
  uint8_t whoami = i2cRead(0x68, 0x00);
  printf("  WHO_AM_I: 0x%02X (expect 0xEA)\n", whoami);
  
  // Reset device
  i2cWriteByte(0x68, 0x06, 0x80);  // PWR_MGMT_1 = reset
  vTaskDelay(pdMS_TO_TICKS(100));
  
  // Wake up device with auto clock select
  i2cWriteByte(0x68, 0x06, 0x01);  // PWR_MGMT_1 = auto clock
  vTaskDelay(pdMS_TO_TICKS(50));
  
  // Enable all accelerometer and gyroscope axes
  i2cWriteByte(0x68, 0x07, 0x00);  // PWR_MGMT_2 = all sensors enabled
  vTaskDelay(pdMS_TO_TICKS(10));
  
  s_imuInitialized = true;
  printf("  ICM20948 initialized.\n");
}

// BME280 initialization
static bool s_bmeInitialized = false;
static void initBME280(){
  if(s_bmeInitialized) return;
  
  printf("  Initializing BME280...\n");
  
  // Check chip ID (should be 0x60 for BME280, 0x58 for BMP280)
  uint8_t chipid = i2cRead(0x76, 0xD0);
  printf("  Chip ID: 0x%02X (expect 0x60 for BME280)\n", chipid);
  
  // Soft reset
  i2cWriteByte(0x76, 0xE0, 0xB6);
  vTaskDelay(pdMS_TO_TICKS(50));
  
  // Wait for reset to complete
  vTaskDelay(pdMS_TO_TICKS(50));
  
  s_bmeInitialized = true;
  printf("  BME280 initialized.\n");
}

// ----- Sensor Actions -----

static void actionShowAllSensors(){
  printf("  Reading all sensors (press any button to stop)...\n\n");
  ensureI2CInit();
  initICM20948();
  
  // Wait for button release from menu selection
  while(anyButtonPressed()) vTaskDelay(pdMS_TO_TICKS(50));
  vTaskDelay(pdMS_TO_TICKS(100));  // Debounce
  
  while(!anyButtonPressed()){
    // IMU
    uint8_t accelData[6];
    i2cReadBytes(0x68, 0x2D, accelData, 6);  // ICM20948 accel regs
    int16_t ax = (accelData[0] << 8) | accelData[1];
    int16_t ay = (accelData[2] << 8) | accelData[3];
    int16_t az = (accelData[4] << 8) | accelData[5];
    
    // BME280 - temp
    uint8_t tempData[3];
    i2cReadBytes(0x76, 0xFA, tempData, 3);
    int32_t tempRaw = (tempData[0] << 12) | (tempData[1] << 4) | (tempData[2] >> 4);
    
    // Buttons (show state, not used for exit in this line)
    bool btnA = !gpio_get_level(BUTTON_A);
    bool btnB = !gpio_get_level(BUTTON_B);
    bool btnC = !gpio_get_level(BUTTON_C);
    bool btnD = !gpio_get_level(BUTTON_D);
    
    printf("  IMU: ax=%6d ay=%6d az=%6d | Temp(raw)=%ld | Btns:%c%c%c%c\n",
           ax, ay, az, (long)tempRaw,
           btnA ? 'A' : '-', btnB ? 'B' : '-', btnC ? 'C' : '-', btnD ? 'D' : '-');
    
    // Wait ~1 second but check button every 50ms
    for(int i = 0; i < 20 && !anyButtonPressed(); i++){
      vTaskDelay(pdMS_TO_TICKS(50));
    }
  }
  while(anyButtonPressed()) vTaskDelay(pdMS_TO_TICKS(50));
  printf("  Stopped.\n");
}

static void actionShowIMU(){
  printf("  Reading ICM20948 IMU (press any button to stop)...\n");
  printf("  Format: Accel(X,Y,Z) Gyro(X,Y,Z)\n\n");
  ensureI2CInit();
  initICM20948();
  
  // Wait for button release from menu selection
  while(anyButtonPressed()) vTaskDelay(pdMS_TO_TICKS(50));
  vTaskDelay(pdMS_TO_TICKS(100));  // Debounce
  
  while(!anyButtonPressed()){
    // Read accelerometer (6 bytes starting at 0x2D)
    uint8_t accelData[6];
    i2cReadBytes(0x68, 0x2D, accelData, 6);
    int16_t ax = (accelData[0] << 8) | accelData[1];
    int16_t ay = (accelData[2] << 8) | accelData[3];
    int16_t az = (accelData[4] << 8) | accelData[5];
    
    // Read gyroscope (6 bytes starting at 0x33)
    uint8_t gyroData[6];
    i2cReadBytes(0x68, 0x33, gyroData, 6);
    int16_t gx = (gyroData[0] << 8) | gyroData[1];
    int16_t gy = (gyroData[2] << 8) | gyroData[3];
    int16_t gz = (gyroData[4] << 8) | gyroData[5];
    
    printf("  Accel: %6d %6d %6d | Gyro: %6d %6d %6d\n", ax, ay, az, gx, gy, gz);
    
    // Wait ~1 second but check button every 50ms
    for(int i = 0; i < 20 && !anyButtonPressed(); i++){
      vTaskDelay(pdMS_TO_TICKS(50));
    }
  }
  while(anyButtonPressed()) vTaskDelay(pdMS_TO_TICKS(50));
  printf("  Stopped.\n");
}

// BME280 calibration data structure
static struct {
  uint16_t dig_T1;
  int16_t  dig_T2, dig_T3;
  uint16_t dig_P1;
  int16_t  dig_P2, dig_P3, dig_P4, dig_P5, dig_P6, dig_P7, dig_P8, dig_P9;
  uint8_t  dig_H1, dig_H3;
  int16_t  dig_H2, dig_H4, dig_H5;
  int8_t   dig_H6;
  bool loaded;
} bmeCalib = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, false};

static void loadBME280Calibration(){
  if(bmeCalib.loaded) return;
  
  // Read temperature and pressure calibration (0x88-0x9F)
  uint8_t cal1[26];
  memset(cal1, 0, sizeof(cal1));
  i2cReadBytes(0x76, 0x88, cal1, 26);
  
  bmeCalib.dig_T1 = (uint16_t)((cal1[1] << 8) | cal1[0]);
  bmeCalib.dig_T2 = (int16_t)((cal1[3] << 8) | cal1[2]);
  bmeCalib.dig_T3 = (int16_t)((cal1[5] << 8) | cal1[4]);
  bmeCalib.dig_P1 = (uint16_t)((cal1[7] << 8) | cal1[6]);
  bmeCalib.dig_P2 = (int16_t)((cal1[9] << 8) | cal1[8]);
  bmeCalib.dig_P3 = (int16_t)((cal1[11] << 8) | cal1[10]);
  bmeCalib.dig_P4 = (int16_t)((cal1[13] << 8) | cal1[12]);
  bmeCalib.dig_P5 = (int16_t)((cal1[15] << 8) | cal1[14]);
  bmeCalib.dig_P6 = (int16_t)((cal1[17] << 8) | cal1[16]);
  bmeCalib.dig_P7 = (int16_t)((cal1[19] << 8) | cal1[18]);
  bmeCalib.dig_P8 = (int16_t)((cal1[21] << 8) | cal1[20]);
  bmeCalib.dig_P9 = (int16_t)((cal1[23] << 8) | cal1[22]);
  
  // H1 at 0xA1
  uint8_t h1 = 0;
  i2cReadBytes(0x76, 0xA1, &h1, 1);
  bmeCalib.dig_H1 = h1;
  
  // Humidity calibration (0xE1-0xE7)
  uint8_t cal2[7];
  memset(cal2, 0, sizeof(cal2));
  i2cReadBytes(0x76, 0xE1, cal2, 7);
  
  bmeCalib.dig_H2 = (int16_t)((cal2[1] << 8) | cal2[0]);
  bmeCalib.dig_H3 = cal2[2];
  bmeCalib.dig_H4 = (int16_t)((cal2[3] << 4) | (cal2[4] & 0x0F));
  bmeCalib.dig_H5 = (int16_t)((cal2[5] << 4) | (cal2[4] >> 4));
  bmeCalib.dig_H6 = (int8_t)cal2[6];
  
  // Verify calibration loaded (dig_T1 and dig_P1 should be non-zero)
  if(bmeCalib.dig_T1 == 0 || bmeCalib.dig_P1 == 0){
    printf("  WARNING: BME280 calibration may not have loaded correctly!\n");
    printf("  dig_T1=%u dig_P1=%u (should be non-zero)\n", bmeCalib.dig_T1, bmeCalib.dig_P1);
  }
  
  bmeCalib.loaded = true;
}

static int32_t g_t_fine = 0;  // Shared for pressure/humidity compensation

static float bme280CompensateTemp(int32_t adc_T){
  int32_t var1 = ((((adc_T >> 3) - ((int32_t)bmeCalib.dig_T1 << 1))) * ((int32_t)bmeCalib.dig_T2)) >> 11;
  int32_t var2 = (((((adc_T >> 4) - ((int32_t)bmeCalib.dig_T1)) * ((adc_T >> 4) - ((int32_t)bmeCalib.dig_T1))) >> 12) * ((int32_t)bmeCalib.dig_T3)) >> 14;
  g_t_fine = var1 + var2;
  return ((g_t_fine * 5 + 128) >> 8) / 100.0f;
}

static float bme280CompensatePressure(int32_t adc_P){
  int64_t var1 = ((int64_t)g_t_fine) - 128000;
  int64_t var2 = var1 * var1 * (int64_t)bmeCalib.dig_P6;
  var2 = var2 + ((var1 * (int64_t)bmeCalib.dig_P5) << 17);
  var2 = var2 + (((int64_t)bmeCalib.dig_P4) << 35);
  var1 = ((var1 * var1 * (int64_t)bmeCalib.dig_P3) >> 8) + ((var1 * (int64_t)bmeCalib.dig_P2) << 12);
  var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)bmeCalib.dig_P1) >> 33;
  if(var1 == 0) return 0;
  int64_t p = 1048576 - adc_P;
  p = (((p << 31) - var2) * 3125) / var1;
  var1 = (((int64_t)bmeCalib.dig_P9) * (p >> 13) * (p >> 13)) >> 25;
  var2 = (((int64_t)bmeCalib.dig_P8) * p) >> 19;
  p = ((p + var1 + var2) >> 8) + (((int64_t)bmeCalib.dig_P7) << 4);
  return (p / 256.0f) / 100.0f;  // Return hPa
}

static float bme280CompensateHumidity(int32_t adc_H){
  int32_t v_x1_u32r = g_t_fine - 76800;
  v_x1_u32r = (((((adc_H << 14) - (((int32_t)bmeCalib.dig_H4) << 20) - (((int32_t)bmeCalib.dig_H5) * v_x1_u32r)) +
               16384) >> 15) * (((((((v_x1_u32r * ((int32_t)bmeCalib.dig_H6)) >> 10) *
               (((v_x1_u32r * ((int32_t)bmeCalib.dig_H3)) >> 11) + 32768)) >> 10) + 2097152) *
               ((int32_t)bmeCalib.dig_H2) + 8192) >> 14));
  v_x1_u32r = v_x1_u32r - (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7) * ((int32_t)bmeCalib.dig_H1)) >> 4);
  v_x1_u32r = (v_x1_u32r < 0) ? 0 : v_x1_u32r;
  v_x1_u32r = (v_x1_u32r > 419430400) ? 419430400 : v_x1_u32r;
  return (v_x1_u32r >> 12) / 1024.0f;
}

static void actionShowBME280(){
  printf("  Reading BME280 (press any button to stop)...\n");
  printf("  Format: Raw -> Metric\n\n");
  ensureI2CInit();
  initBME280();
  
  // Force reload calibration after init
  bmeCalib.loaded = false;
  loadBME280Calibration();
  
  // Wait for button release from menu selection
  while(anyButtonPressed()) vTaskDelay(pdMS_TO_TICKS(50));
  vTaskDelay(pdMS_TO_TICKS(100));  // Debounce
  
  // Configure humidity oversampling (ctrl_hum 0xF2)
  i2cWriteByte(0x76, 0xF2, 0x01);  // humidity oversampling x1
  
  // Trigger measurement (forced mode with oversampling)
  i2cWriteByte(0x76, 0xF4, 0x25);  // temp x1, press x1, forced mode
  
  while(!anyButtonPressed()){
    vTaskDelay(pdMS_TO_TICKS(50));  // Wait for measurement
    
    // Read all data (press 0xF7-F9, temp 0xFA-FC, humid 0xFD-FE)
    uint8_t data[8];
    i2cReadBytes(0x76, 0xF7, data, 8);
    
    int32_t adc_P = (data[0] << 12) | (data[1] << 4) | (data[2] >> 4);
    int32_t adc_T = (data[3] << 12) | (data[4] << 4) | (data[5] >> 4);
    int32_t adc_H = (data[6] << 8) | data[7];
    
    // Compensate (must do temp first for t_fine)
    float tempC = bme280CompensateTemp(adc_T);
    float pressHpa = bme280CompensatePressure(adc_P);
    float humidPct = bme280CompensateHumidity(adc_H);
    
    printf("  T:%8ld->%6.2fC | P:%8ld->%7.2fhPa | H:%6ld->%5.1f%%\n", 
           (long)adc_T, tempC, (long)adc_P, pressHpa, (long)adc_H, humidPct);
    
    // Trigger next measurement
    i2cWriteByte(0x76, 0xF4, 0x25);
    
    // Wait ~1 second but check button every 50ms
    for(int i = 0; i < 19 && !anyButtonPressed(); i++){
      vTaskDelay(pdMS_TO_TICKS(50));
    }
  }
  // Wait for button release before returning
  while(anyButtonPressed()) vTaskDelay(pdMS_TO_TICKS(50));
  printf("  Stopped.\n");
}

// I2S handle for microphone
static i2s_chan_handle_t g_i2s_rx_handle = nullptr;
static bool g_i2s_initialized = false;

static void initINMP441(){
  if(g_i2s_initialized) return;
  
  // Channel configuration
  i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  chan_cfg.dma_desc_num = 4;
  chan_cfg.dma_frame_num = 256;
  
  esp_err_t err = i2s_new_channel(&chan_cfg, nullptr, &g_i2s_rx_handle);
  if(err != ESP_OK){
    printf("  Failed to create I2S channel: %d\n", err);
    return;
  }
  
  // Standard mode config for INMP441
  i2s_std_config_t std_cfg = {
    .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(16000),  // 16kHz sample rate
    .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO),
    .gpio_cfg = {
      .mclk = I2S_GPIO_UNUSED,
      .bclk = GPIO_NUM_40,  // CLK
      .ws = GPIO_NUM_42,    // WS (L/R clock)
      .dout = I2S_GPIO_UNUSED,
      .din = GPIO_NUM_2,    // DOUT (data in)
      .invert_flags = {
        .mclk_inv = false,
        .bclk_inv = false,
        .ws_inv = false,
      },
    },
  };
  
  err = i2s_channel_init_std_mode(g_i2s_rx_handle, &std_cfg);
  if(err != ESP_OK){
    printf("  Failed to init I2S std mode: %d\n", err);
    i2s_del_channel(g_i2s_rx_handle);
    g_i2s_rx_handle = nullptr;
    return;
  }
  
  err = i2s_channel_enable(g_i2s_rx_handle);
  if(err != ESP_OK){
    printf("  Failed to enable I2S channel: %d\n", err);
    i2s_del_channel(g_i2s_rx_handle);
    g_i2s_rx_handle = nullptr;
    return;
  }
  
  g_i2s_initialized = true;
  printf("  I2S initialized for INMP441 @ 16kHz\n");
}

static void actionShowMicrophone(){
  printf("  INMP441 Microphone (press any button to stop)...\n");
  
  initINMP441();
  if(!g_i2s_initialized){
    printf("  ERROR: Could not initialize I2S\n");
    return;
  }
  
  printf("  Format: Current | Average | Peak (per second)\n\n");
  
  // Wait for button release from menu selection
  while(anyButtonPressed()) vTaskDelay(pdMS_TO_TICKS(50));
  vTaskDelay(pdMS_TO_TICKS(100));  // Debounce
  
  // Sample buffer (16kHz sample rate, 1 second worth)
  const size_t SAMPLES_PER_READ = 256;
  int32_t samples[SAMPLES_PER_READ];
  
  while(!anyButtonPressed()){
    int64_t sumAbs = 0;
    int32_t peakVal = 0;
    int32_t currentVal = 0;
    int sampleCount = 0;
    
    // Read for ~1 second (16000 samples / 256 per read = ~62 reads)
    int64_t startTime = esp_timer_get_time();
    while((esp_timer_get_time() - startTime) < 1000000){  // 1 second
      size_t bytesRead = 0;
      esp_err_t err = i2s_channel_read(g_i2s_rx_handle, samples, sizeof(samples), &bytesRead, pdMS_TO_TICKS(100));
      
      if(err == ESP_OK && bytesRead > 0){
        int samplesReceived = bytesRead / sizeof(int32_t);
        for(int i = 0; i < samplesReceived; i++){
          // INMP441 data is 24-bit in upper bits of 32-bit word
          int32_t val = samples[i] >> 14;  // Shift more for reasonable numbers
          int32_t absVal = (val < 0) ? -val : val;
          if(absVal > currentVal) currentVal = absVal;  // Keep max sample as "current"
          sumAbs += absVal;
          if(absVal > peakVal) peakVal = absVal;
          sampleCount++;
        }
      }
      
      // Quick button check during sampling
      if(anyButtonPressed()) break;
    }
    
    if(sampleCount > 0){
      int32_t avgVal = (int32_t)(sumAbs / sampleCount);
      printf("  Cur:%8ld | Avg:%8ld | Peak:%8ld\n", 
             (long)currentVal, (long)avgVal, (long)peakVal);
    }else{
      printf("  (no samples received)\n");
    }
  }
  while(anyButtonPressed()) vTaskDelay(pdMS_TO_TICKS(50));
  printf("  Stopped.\n");
}

// GPS parsed data structure
static struct {
  char time[12];       // HHMMSS.SS
  char lat[16];        // Latitude
  char latDir;         // N/S
  char lon[16];        // Longitude
  char lonDir;         // E/W
  int fixQuality;      // 0=invalid, 1=GPS, 2=DGPS, 4=RTK, 5=Float RTK
  int numSats;         // Number of satellites
  char altitude[10];   // Altitude in meters
  char speed[10];      // Speed in knots
  char course[10];     // Course over ground
  char date[10];       // DDMMYY
  char hdop[8];        // Horizontal dilution of precision
  bool valid;
  // Last known good position (kept when fix lost)
  char lastLat[16];
  char lastLatDir;
  char lastLon[16];
  char lastLonDir;
  char lastAlt[10];
  int64_t lastFixTime; // Microseconds timestamp of last fix
} gpsData = {"", "", ' ', "", ' ', 0, 0, "", "", "", "", "", false, "", ' ', "", ' ', "", 0};

static void parseNMEAField(const char* sentence, int fieldNum, char* dest, size_t destSize){
  const char* p = sentence;
  int field = 0;
  
  // Skip to desired field
  while(*p && field < fieldNum){
    if(*p == ',') field++;
    p++;
  }
  
  // Copy until next comma or end
  size_t i = 0;
  while(*p && *p != ',' && *p != '*' && i < destSize - 1){
    dest[i++] = *p++;
  }
  dest[i] = '\0';
}

static void parseGGA(const char* sentence){
  char field[20];
  
  // Time (field 1)
  parseNMEAField(sentence, 1, gpsData.time, sizeof(gpsData.time));
  
  // Latitude (field 2) and direction (field 3)
  parseNMEAField(sentence, 2, gpsData.lat, sizeof(gpsData.lat));
  parseNMEAField(sentence, 3, field, sizeof(field));
  gpsData.latDir = field[0] ? field[0] : ' ';
  
  // Longitude (field 4) and direction (field 5)
  parseNMEAField(sentence, 4, gpsData.lon, sizeof(gpsData.lon));
  parseNMEAField(sentence, 5, field, sizeof(field));
  gpsData.lonDir = field[0] ? field[0] : ' ';
  
  // Fix quality (field 6)
  parseNMEAField(sentence, 6, field, sizeof(field));
  gpsData.fixQuality = atoi(field);
  
  // Number of satellites (field 7)
  parseNMEAField(sentence, 7, field, sizeof(field));
  gpsData.numSats = atoi(field);
  
  // HDOP (field 8) - horizontal dilution of precision
  parseNMEAField(sentence, 8, gpsData.hdop, sizeof(gpsData.hdop));
  
  // Altitude (field 9)
  parseNMEAField(sentence, 9, gpsData.altitude, sizeof(gpsData.altitude));
  
  gpsData.valid = (gpsData.fixQuality > 0);
  
  // Save last known good position when we have a fix
  if(gpsData.valid && strlen(gpsData.lat) > 0 && strlen(gpsData.lon) > 0){
    strncpy(gpsData.lastLat, gpsData.lat, sizeof(gpsData.lastLat));
    gpsData.lastLatDir = gpsData.latDir;
    strncpy(gpsData.lastLon, gpsData.lon, sizeof(gpsData.lastLon));
    gpsData.lastLonDir = gpsData.lonDir;
    strncpy(gpsData.lastAlt, gpsData.altitude, sizeof(gpsData.lastAlt));
    gpsData.lastFixTime = esp_timer_get_time();
  }
}

static void parseRMC(const char* sentence){
  char field[20];
  
  // Time (field 1)
  parseNMEAField(sentence, 1, gpsData.time, sizeof(gpsData.time));
  
  // Status (field 2) - A=active, V=void
  parseNMEAField(sentence, 2, field, sizeof(field));
  gpsData.valid = (field[0] == 'A');
  
  // Latitude (field 3) and direction (field 4)
  parseNMEAField(sentence, 3, gpsData.lat, sizeof(gpsData.lat));
  parseNMEAField(sentence, 4, field, sizeof(field));
  gpsData.latDir = field[0] ? field[0] : ' ';
  
  // Longitude (field 5) and direction (field 6)
  parseNMEAField(sentence, 5, gpsData.lon, sizeof(gpsData.lon));
  parseNMEAField(sentence, 6, field, sizeof(field));
  gpsData.lonDir = field[0] ? field[0] : ' ';
  
  // Speed in knots (field 7)
  parseNMEAField(sentence, 7, gpsData.speed, sizeof(gpsData.speed));
  
  // Course (field 8)
  parseNMEAField(sentence, 8, gpsData.course, sizeof(gpsData.course));
  
  // Date (field 9)
  parseNMEAField(sentence, 9, gpsData.date, sizeof(gpsData.date));
}

static void parseNMEA(const char* sentence){
  if(strncmp(sentence, "$GPGGA", 6) == 0 || strncmp(sentence, "$GNGGA", 6) == 0){
    parseGGA(sentence);
  }else if(strncmp(sentence, "$GPRMC", 6) == 0 || strncmp(sentence, "$GNRMC", 6) == 0){
    parseRMC(sentence);
  }
}

static void actionShowGPS(){
  printf("  Reading NEO-8M GPS (press any button to stop)...\n");
  printf("  Time     | Lat          | Lon           | Alt    | Speed   | Sats | HDOP\n\n");
  
  // Initialize GPS UART if needed (UART2)
  static bool gpsUartInit = false;
  if(!gpsUartInit){
    uart_config_t uart_config = {};
    uart_config.baud_rate = 9600;
    uart_config.data_bits = UART_DATA_8_BITS;
    uart_config.parity = UART_PARITY_DISABLE;
    uart_config.stop_bits = UART_STOP_BITS_1;
    uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    uart_config.source_clk = UART_SCLK_DEFAULT;
    
    uart_param_config(UART_NUM_2, &uart_config);
    uart_set_pin(UART_NUM_2, GPIO_NUM_43, GPIO_NUM_44, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(UART_NUM_2, 2048, 0, 0, nullptr, 0);  // Larger buffer
    gpsUartInit = true;
  }
  
  // Wait for button release from menu selection
  while(anyButtonPressed()) vTaskDelay(pdMS_TO_TICKS(50));
  vTaskDelay(pdMS_TO_TICKS(100));  // Debounce
  
  // Buffer for accumulating NMEA sentences
  char buf[512];
  char lineBuf[128];
  int linePos = 0;
  
  int64_t lastPrint = 0;
  
  while(!anyButtonPressed()){
    // Read available GPS data
    int len = uart_read_bytes(UART_NUM_2, buf, sizeof(buf) - 1, pdMS_TO_TICKS(100));
    
    if(len > 0){
      for(int i = 0; i < len; i++){
        char c = buf[i];
        if(c == '\n' || c == '\r'){
          if(linePos > 0){
            lineBuf[linePos] = '\0';
            if(lineBuf[0] == '$'){
              parseNMEA(lineBuf);
            }
            linePos = 0;
          }
        }else if(linePos < (int)sizeof(lineBuf) - 1){
          lineBuf[linePos++] = c;
        }
      }
    }
    
    // Print status every second
    int64_t now = esp_timer_get_time();
    if((now - lastPrint) >= 1000000){
      // Format time as HH:MM:SS
      char timeStr[12] = "--:--:--";
      if(strlen(gpsData.time) >= 6){
        snprintf(timeStr, sizeof(timeStr), "%.2s:%.2s:%.2s", 
                 gpsData.time, gpsData.time + 2, gpsData.time + 4);
      }
      
      // Determine if using current fix or last known position
      bool usingLastKnown = false;
      const char* lat = gpsData.lat;
      char latDir = gpsData.latDir;
      const char* lon = gpsData.lon;
      char lonDir = gpsData.lonDir;
      const char* alt = gpsData.altitude;
      
      // If no current fix but have last known, use that
      if(!gpsData.valid && gpsData.lastFixTime > 0 && strlen(gpsData.lastLat) > 0){
        lat = gpsData.lastLat;
        latDir = gpsData.lastLatDir;
        lon = gpsData.lastLon;
        lonDir = gpsData.lastLonDir;
        alt = gpsData.lastAlt;
        usingLastKnown = true;
      }
      
      // Format latitude
      char latStr[20] = "  --------";
      if(strlen(lat) >= 4){
        snprintf(latStr, sizeof(latStr), "%s%c", lat, latDir);
      }
      
      // Format longitude
      char lonStr[20] = "  ---------";
      if(strlen(lon) >= 5){
        snprintf(lonStr, sizeof(lonStr), "%s%c", lon, lonDir);
      }
      
      // Format altitude
      char altStr[16] = "  ----";
      if(strlen(alt) > 0){
        snprintf(altStr, sizeof(altStr), "%5.1fm", atof(alt));
      }
      
      // Format HDOP (lower is better: <1=ideal, 1-2=excellent, 2-5=good, 5-10=moderate, >10=poor)
      char hdopStr[12] = "----";
      float hdop = 99.9f;
      if(strlen(gpsData.hdop) > 0){
        hdop = atof(gpsData.hdop);
        if(hdop < 1.0f) snprintf(hdopStr, sizeof(hdopStr), "%.1f++", hdop);
        else if(hdop < 2.0f) snprintf(hdopStr, sizeof(hdopStr), "%.1f+ ", hdop);
        else if(hdop < 5.0f) snprintf(hdopStr, sizeof(hdopStr), "%.1f  ", hdop);
        else if(hdop < 10.0f) snprintf(hdopStr, sizeof(hdopStr), "%.1f- ", hdop);
        else snprintf(hdopStr, sizeof(hdopStr), "%.0f--", hdop);
      }
      
      // Format speed (convert knots to km/h: 1 knot = 1.852 km/h)
      // Apply stationary filter only when signal is weak (high HDOP or few sats)
      char spdStr[16] = "   ----";
      float kmh = 0;
      if(strlen(gpsData.speed) > 0 && gpsData.valid){
        float knots = atof(gpsData.speed);
        kmh = knots * 1.852f;
        
        // Good signal (HDOP < 2.0 and 5+ sats): show actual speed
        // Moderate signal (HDOP < 5.0 and 4+ sats): filter < 2 km/h as jitter
        // Weak signal: filter < 5 km/h as jitter
        float jitterThreshold = 5.0f;  // Default for weak signal
        if(hdop < 2.0f && gpsData.numSats >= 5){
          jitterThreshold = 0.5f;  // Good signal - show nearly everything
        }else if(hdop < 5.0f && gpsData.numSats >= 4){
          jitterThreshold = 2.0f;  // Moderate signal
        }
        
        if(kmh < jitterThreshold){
          snprintf(spdStr, sizeof(spdStr), "  0.0km/h");
        }else{
          snprintf(spdStr, sizeof(spdStr), "%5.1fkm/h", kmh);
        }
      }
      
      // Status string - include quality assessment
      const char* status;
      if(gpsData.valid){
        if(gpsData.fixQuality == 2) status = "[DGPS]";
        else if(gpsData.fixQuality == 4) status = "[RTK ]";
        else if(gpsData.fixQuality == 5) status = "[FRTK]";
        else if(hdop < 2.0f && gpsData.numSats >= 6) status = "[GOOD]";
        else if(hdop < 5.0f && gpsData.numSats >= 4) status = "[FIX ]";
        else status = "[WEAK]";
      }else if(usingLastKnown){
        int ageSec = (int)((now - gpsData.lastFixTime) / 1000000);
        if(ageSec < 60) status = "[OLD ]";
        else status = "[LOST]";
      }else{
        status = "[----]";
      }
      
      printf("  %8s | %12s | %13s | %6s | %9s | %2d %s %s\n",
             timeStr, latStr, lonStr, altStr, spdStr, gpsData.numSats, status, hdopStr);
      
      lastPrint = now;
    }
  }
  while(anyButtonPressed()) vTaskDelay(pdMS_TO_TICKS(50));
  printf("  Stopped.\n");
}

static void actionShowButtons(){
  printf("  Button states (press and hold any button to stop)...\n\n");
  
  // Wait for button release from menu selection
  while(anyButtonPressed()) vTaskDelay(pdMS_TO_TICKS(50));
  vTaskDelay(pdMS_TO_TICKS(100));  // Debounce
  
  while(!anyButtonPressed()){
    bool currA = gpio_get_level(BUTTON_A);
    bool currB = gpio_get_level(BUTTON_B);
    bool currC = gpio_get_level(BUTTON_C);
    bool currD = gpio_get_level(BUTTON_D);
    
    printf("  A:%s B:%s C:%s D:%s\n",
           currA ? "---" : "LOW",
           currB ? "---" : "LOW",
           currC ? "---" : "LOW",
           currD ? "---" : "LOW");
    
    // Wait ~1 second but check button every 50ms
    for(int i = 0; i < 20 && !anyButtonPressed(); i++){
      vTaskDelay(pdMS_TO_TICKS(50));
    }
  }
  while(anyButtonPressed()) vTaskDelay(pdMS_TO_TICKS(50));
  printf("  Stopped.\n");
}

// ----- HUB75 Display Actions -----
static void actionHUB75HSLPattern(){
  printf("  Displaying HSL + Grayscale on HUB75 (5s)...\n");
  auto& runner = getTestRunner();
  runner.testGpuHub75Pattern(0);
  printf("  Done.\n");
}

static void actionHUB75GrayscalePattern(){
  printf("  Displaying Grayscale Gradient on HUB75 (5s)...\n");
  auto& runner = getTestRunner();
  runner.testGpuHub75Pattern(1);
  printf("  Done.\n");
}

static void actionHUB75OrientationArrows(){
  printf("  Displaying Orientation Arrows on HUB75 (5s)...\n");
  auto& runner = getTestRunner();
  runner.testGpuHub75Pattern(2);
  printf("  Done.\n");
}

static void actionHUB75AllPatterns(){
  printf("  Cycling all HUB75 patterns...\n");
  auto& runner = getTestRunner();
  for(int i = 0; i < 3; i++){
    printf("  Pattern %d/3...\n", i+1);
    runner.testGpuHub75Pattern(i);
  }
  printf("  All patterns complete.\n");
}

// ----- OLED Display Actions -----
static void actionOLEDCheckerPattern(){
  printf("  Displaying Checker + H-Stripes on OLED (5s)...\n");
  auto& runner = getTestRunner();
  runner.testGpuOledPattern(0);
  printf("  Done.\n");
}

static void actionOLEDStripesPattern(){
  printf("  Displaying V-Stripes + Diagonal on OLED (5s)...\n");
  auto& runner = getTestRunner();
  runner.testGpuOledPattern(1);
  printf("  Done.\n");
}

static void actionOLEDOrientationArrows(){
  printf("  Displaying Orientation Arrows on OLED (5s)...\n");
  auto& runner = getTestRunner();
  runner.testGpuOledPattern(2);
  printf("  Done.\n");
}

static void actionOLEDAllPatterns(){
  printf("  Cycling all OLED patterns...\n");
  auto& runner = getTestRunner();
  for(int i = 0; i < 3; i++){
    printf("  Pattern %d/3...\n", i+1);
    runner.testGpuOledPattern(i);
  }
  printf("  All patterns complete.\n");
}

// ----- LED Strip Actions -----
static void actionLEDLeftFin(){
  printf("  Testing Left Fin LEDs (RGBW cycle)...\n");
  auto& runner = getTestRunner();
  runner.testLedStrip(0);
}

static void actionLEDRightFin(){
  printf("  Testing Right Fin LEDs (RGBW cycle)...\n");
  auto& runner = getTestRunner();
  runner.testLedStrip(1);
}

static void actionLEDScale(){
  printf("  Testing Scale LEDs (RGBW cycle)...\n");
  auto& runner = getTestRunner();
  runner.testLedStrip(2);
}

static void actionLEDTongue(){
  printf("  Testing Tongue LEDs (RGBW cycle)...\n");
  auto& runner = getTestRunner();
  runner.testLedStrip(3);
}

static void actionLEDAllStrips(){
  printf("  Testing all LED strips (RGBW cycle)...\n");
  auto& runner = getTestRunner();
  for(int i = 0; i < 4; i++){
    runner.testLedStrip(i);
  }
  printf("  All strips tested.\n");
}

// ----- Full Test Action -----
static void actionRunFullTest(){
  printf("  Running full HAL test suite...\n\n");
  SystemAPI::HalTest::HalTestRunner testRunner;
  testRunner.runWithConsoleOutput();
}

// ============================================================
// Debug Menu Entry Point (runs forever until reset)
// ============================================================
static void runDebugMenu(){
  printMenuHeader();
  
  // Reset button timing on entry
  lastButtonActionTime = 0;
  
  while(true){
    navigateMenu(mainMenu, mainMenuCount, "MAIN MENU");
    
    // If we exit main menu via D, confirm or restart
    printf("\n");
    printf("┌──────────────────────────────────────────────────────────┐\n");
    printf("│  You're at the top level. What would you like to do?    │\n");
    printf("│  B = Re-enter Main Menu                                 │\n");
    printf("│  (Press any button to continue, timeout in 3s)          │\n");
    printf("└──────────────────────────────────────────────────────────┘\n");
    
    waitForButton(3000);  // 3 second timeout, result doesn't matter
    printf("  Returning to Main Menu...\n");
    // Always loop back to main menu
  }
}

// ============================================================
// System Test Loop Entry Point (runs forever until reset)
// ============================================================
static void runSystemTestLoop(uint32_t loopDelayMs){
  printf("\n");
  printf("╔══════════════════════════════════════════════════════════╗\n");
  printf("║         SYSTEM TEST LOOP MODE (A+D STARTUP)              ║\n");
  printf("║         Running all tests in infinite loop               ║\n");
  printf("║         Gap between iterations: %lu ms                   ║\n", (unsigned long)loopDelayMs);
  printf("║         Reset/power-cycle to exit                        ║\n");
  printf("╚══════════════════════════════════════════════════════════╝\n");
  printf("\n");
  
  int iteration = 0;
  while(true){
    iteration++;
    printf("\n");
    printf("════════════════════════════════════════════════════════════\n");
    printf("  SYSTEM TEST LOOP - ITERATION #%d\n", iteration);
    printf("════════════════════════════════════════════════════════════\n");
    printf("\n");
    
    // Run comprehensive HAL tests
    SystemAPI::HalTest::HalTestRunner testRunner;
    testRunner.runWithConsoleOutput();
    
    printf("\n");
    printf("  [TEST] Iteration #%d complete. Waiting %lu ms...\n", 
           iteration, (unsigned long)loopDelayMs);
    printf("════════════════════════════════════════════════════════════\n");
    printf("\n");
    
    vTaskDelay(pdMS_TO_TICKS(loopDelayMs));
  }
}

// ============================================================
// Print mode banner
// ============================================================
static void printModeBanner(SystemAPI::Mode::SystemMode mode){
  const char* modeName = SystemAPI::Mode::getModeName(mode);
  
  printf("\n");
  printf("╔════════════════════════════════════════╗\n");
  printf("║  SYSTEM MODE: %-24s ║\n", modeName);
  printf("╚════════════════════════════════════════╝\n");
  printf("\n");
  
  ESP_LOGI(TAG, "Mode changed to: %s", modeName);
}

// ============================================================
// Print controls help
// ============================================================
static void printHelp(){
  printf("\n");
  printf("┌────────────────────────────────────────┐\n");
  printf("│       SYSTEM MODE TEST                 │\n");
  printf("├────────────────────────────────────────┤\n");
  printf("│  STARTUP: Hold A+D = Test Loop Mode    │\n");
  printf("│  STARTUP: Hold A   = Debug Menu        │\n");
  printf("├────────────────────────────────────────┤\n");
  printf("│  Button A (GPIO 5)  = Boot Mode        │\n");
  printf("│  Button B (GPIO 6)  = Running Mode     │\n");
  printf("│  Button C (GPIO 7)  = Debug Mode       │\n");
  printf("│  Button D (GPIO 15) = System Test Mode │\n");
  printf("├────────────────────────────────────────┤\n");
  printf("│  State Machine:                        │\n");
  printf("│  Debug → Boot → Running ↔ System Test  │\n");
  printf("│  Running/SystemTest → Debug            │\n");
  printf("└────────────────────────────────────────┘\n");
  printf("\n");
}

// ============================================================
// Register Mode Handlers
// ============================================================
static void registerModeHandlers(SystemAPI::Mode::Manager& mm){
  using namespace SystemAPI::Mode;
  
  // ----- BOOT MODE HANDLER -----
  ModeHandler bootHandler;
  bootHandler.name = "BootSequence";
  bootHandler.onEnter = [](){
    bootProgress = 0.0f;
    printf("  [BOOT] Initializing boot sequence...\n");
    printf("  [BOOT] Loading configuration...\n");
  };
  bootHandler.onUpdate = [](float dt){
    bootProgress += dt * 0.5f;  // 2 seconds to "complete"
    if(bootProgress >= 1.0f){
      bootProgress = 1.0f;
    }
  };
  bootHandler.onRender = [](){
    // Progress bar visualization
    int bars = static_cast<int>(bootProgress * 20);
    printf("\r  [BOOT] Progress: [");
    for(int i = 0; i < 20; i++){
      printf(i < bars ? "█" : "░");
    }
    printf("] %3.0f%%", bootProgress * 100);
    fflush(stdout);
  };
  bootHandler.onExit = [](){
    printf("\n  [BOOT] Boot sequence complete!\n");
    bootProgress = 0.0f;
  };
  mm.registerHandler(SystemMode::BOOT, bootHandler);
  
  // ----- RUNNING MODE HANDLER -----
  ModeHandler runningHandler;
  runningHandler.name = "MainRuntime";
  runningHandler.onEnter = [](){
    runningTime = 0.0f;
    printf("  [RUNNING] System now active!\n");
    printf("  [RUNNING] All subsystems operational.\n");
  };
  runningHandler.onUpdate = [](float dt){
    runningTime += dt;
  };
  runningHandler.onRender = [](){
    // Only print every ~1 second
    static float lastPrint = 0.0f;
    if(runningTime - lastPrint >= 1.0f){
      printf("  [RUNNING] Uptime: %.1f seconds\n", runningTime);
      lastPrint = runningTime;
    }
  };
  runningHandler.onExit = [](){
    printf("  [RUNNING] Pausing main runtime...\n");
  };
  mm.registerHandler(SystemMode::RUNNING, runningHandler);
  
  // ----- DEBUG MODE HANDLER (runtime - verbose overlay) -----
  ModeHandler debugHandler;
  debugHandler.name = "DebugOverlay";
  debugHandler.onEnter = [](){
    debugFrameCount = 0;
    printf("  [DEBUG] ════════════════════════════════\n");
    printf("  [DEBUG] Debug mode enabled\n");
    printf("  [DEBUG] Verbose logging: ON\n");
    printf("  [DEBUG] Performance overlay: ON\n");
    printf("  [DEBUG] ════════════════════════════════\n");
  };
  debugHandler.onUpdate = [](float dt){
    debugFrameCount++;
  };
  debugHandler.onRender = [](){
    // Only print every 20 frames
    if(debugFrameCount % 20 == 0){
      printf("  [DEBUG] Frame: %d | Heap: %lu bytes free\n", 
             debugFrameCount, 
             (unsigned long)esp_get_free_heap_size());
    }
  };
  debugHandler.onExit = [](){
    printf("  [DEBUG] Debug mode disabled\n");
    printf("  [DEBUG] Total frames in debug: %d\n", debugFrameCount);
  };
  mm.registerHandler(SystemMode::DEBUG, debugHandler);
  
  // ----- SYSTEM TEST MODE HANDLER (runtime - single run) -----
  ModeHandler testHandler;
  testHandler.name = "HalSystemTest";
  testHandler.onEnter = [](){
    printf("  [TEST] ┌─────────────────────────────────────────────────────────┐\n");
    printf("  [TEST] │     COMPREHENSIVE HAL SYSTEM TEST                       │\n");
    printf("  [TEST] │     Max Duration: 5 minutes                             │\n");
    printf("  [TEST] │     GPS Warning: 2 minutes (NEO-8M cold start)          │\n");
    printf("  [TEST] │     Auto-return to Running in 5s after completion       │\n");
    printf("  [TEST] └─────────────────────────────────────────────────────────┘\n");
    printf("  [TEST] Starting hardware diagnostics...\n\n");
    
    // Run comprehensive HAL tests
    SystemAPI::HalTest::HalTestRunner testRunner;
    testRunner.runWithConsoleOutput();
    
    // 5 second countdown before returning to Running
    printf("\n");
    printf("  [TEST] ───────────────────────────────────────────────────────────\n");
    printf("  [TEST] HAL test sequence complete.\n");
    printf("  [TEST] Returning to Running mode in 5 seconds...\n");
    for(int i = 5; i > 0; i--){
      printf("  [TEST] %d...\n", i);
      vTaskDelay(pdMS_TO_TICKS(1000));
    }
    printf("  [TEST] Switching to Running mode now.\n");
    printf("  [TEST] ───────────────────────────────────────────────────────────\n");
    
    // Auto-transition to Running mode
    SystemAPI::Mode::Manager::instance().exitSystemTest();
  };
  testHandler.onUpdate = [](float dt){
    // Test runs once on enter, no continuous update needed
  };
  testHandler.onRender = [](){
    // Test runs once on enter, no continuous render needed
  };
  testHandler.onExit = [](){
    // Exit message already printed in onEnter countdown
  };
  mm.registerHandler(SystemMode::SYSTEM_TEST, testHandler);
  
  printf("  Mode handlers registered: Boot=%zu, Running=%zu, Debug=%zu, Test=%zu\n",
         mm.getHandlerCount(SystemMode::BOOT),
         mm.getHandlerCount(SystemMode::RUNNING),
         mm.getHandlerCount(SystemMode::DEBUG),
         mm.getHandlerCount(SystemMode::SYSTEM_TEST));
}

// ============================================================
// Main Entry Point
// ============================================================
extern "C" void app_main(){
  ESP_LOGI(TAG, "=== CPU Mode Toggle Test ===");
  
  // Initialize buttons FIRST for startup check
  initButtons();
  
  // Longer delay to let GPIO stabilize and give user time to hold buttons
  printf("\n  Checking startup buttons...\n");
  vTaskDelay(pdMS_TO_TICKS(300));
  
  // Check startup button combinations (sample multiple times for reliability)
  int aCount = 0, dCount = 0;
  for(int i = 0; i < 5; i++){
    if(gpio_get_level(BUTTON_A) == 0) aCount++;  // Active LOW
    if(gpio_get_level(BUTTON_D) == 0) dCount++;
    vTaskDelay(pdMS_TO_TICKS(20));
  }
  
  bool btnAHeld = (aCount >= 3);  // At least 3/5 samples = held
  bool btnDHeld = (dCount >= 3);
  
  printf("  Button A: %s (%d/5 samples)\n", btnAHeld ? "HELD" : "not held", aCount);
  printf("  Button D: %s (%d/5 samples)\n", btnDHeld ? "HELD" : "not held", dCount);
  
  // Determine startup mode
  if(btnAHeld && btnDHeld){
    // A+D = System Test Loop Mode
    g_startupMode = StartupMode::SYSTEM_TEST_LOOP;
    ESP_LOGI(TAG, "*** A+D HELD AT STARTUP - ENTERING SYSTEM TEST LOOP MODE ***");
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════╗\n");
    printf("║     A + D DETECTED AT STARTUP                            ║\n");
    printf("║     ENTERING SYSTEM TEST LOOP MODE...                    ║\n");
    printf("║     Buttons DISABLED - Reset to exit                     ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    // Run system test loop forever (never returns)
    runSystemTestLoop(5000);
    return;
  }
  
  if(btnAHeld && !btnDHeld){
    // A only = Debug Menu Mode
    g_startupMode = StartupMode::DEBUG_MENU;
    ESP_LOGI(TAG, "*** A HELD AT STARTUP - ENTERING DEBUG MENU MODE ***");
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════╗\n");
    printf("║     A DETECTED AT STARTUP                                ║\n");
    printf("║     ENTERING DEBUG MENU MODE...                          ║\n");
    printf("║     A=Prev  B=Select  C=Next  D=Back                     ║\n");
    printf("║     Runtime mode switching DISABLED                      ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    // Run debug menu forever (never returns)
    runDebugMenu();
    return;
  }
  
  // Normal boot - g_startupMode is already NORMAL by default
  g_startupMode = StartupMode::NORMAL;
  ESP_LOGI(TAG, "Normal startup - entering runtime mode system");
  printf("\n");
  printf("╔══════════════════════════════════════════════════════════╗\n");
  printf("║     NORMAL STARTUP                                       ║\n");
  printf("║     Runtime mode switching ENABLED                       ║\n");
  printf("║     A=Boot  B=Running  C=Debug  D=SystemTest             ║\n");
  printf("╚══════════════════════════════════════════════════════════╝\n");
  printf("\n");
  
  // Initialize mode manager
  auto& modeManager = SystemAPI::Mode::Manager::instance();
  modeManager.initialize(SystemAPI::Mode::SystemMode::BOOT);
  
  // Register mode change callback (for banner display)
  modeManager.onModeChange([](const SystemAPI::Mode::ModeEventData& e){
    if(e.type == SystemAPI::Mode::ModeEvent::MODE_CHANGED){
      printModeBanner(e.currentMode);
    }
  });
  
  // Register mode-specific handlers
  registerModeHandlers(modeManager);
  
  // Print help
  printHelp();
  
  // Print initial mode
  printModeBanner(modeManager.getCurrentMode());
  
  // Timing for update loop
  int64_t lastTime = esp_timer_get_time();
  
  // Main loop - ONLY runs in NORMAL startup mode
  while(true){
    // Calculate delta time
    int64_t now = esp_timer_get_time();
    float deltaTime = (now - lastTime) / 1000000.0f;
    lastTime = now;
    
    // Button handling ONLY in normal mode
    // (Debug menu and System Test Loop have their own isolated button handling)
    
    // Check Button A - Boot mode
    if(checkButtonPress(BUTTON_A, btnA)){
      ESP_LOGI(TAG, "Button A pressed - requesting Boot mode");
      if(!modeManager.setMode(SystemAPI::Mode::SystemMode::BOOT)){
        ESP_LOGW(TAG, "Cannot transition to Boot from current mode");
        printf("  [!] Cannot enter Boot mode from %s\n", 
               SystemAPI::Mode::getModeName(modeManager.getCurrentMode()));
      }
    }
    
    // Check Button B - Running mode
    if(checkButtonPress(BUTTON_B, btnB)){
      ESP_LOGI(TAG, "Button B pressed - requesting Running mode");
      if(!modeManager.enterRunning()){
        ESP_LOGW(TAG, "Cannot transition to Running from current mode");
        printf("  [!] Cannot enter Running mode from %s\n",
               SystemAPI::Mode::getModeName(modeManager.getCurrentMode()));
      }
    }
    
    // Check Button C - Debug mode (runtime debug overlay, not debug menu)
    if(checkButtonPress(BUTTON_C, btnC)){
      ESP_LOGI(TAG, "Button C pressed - requesting Debug mode");
      if(!modeManager.enterDebugMode()){
        ESP_LOGW(TAG, "Cannot transition to Debug from current mode");
        printf("  [!] Cannot enter Debug mode from %s\n",
               SystemAPI::Mode::getModeName(modeManager.getCurrentMode()));
      }
    }
    
    // Check Button D - System Test mode (single run)
    if(checkButtonPress(BUTTON_D, btnD)){
      ESP_LOGI(TAG, "Button D pressed - requesting System Test mode");
      if(!modeManager.enterSystemTest()){
        ESP_LOGW(TAG, "Cannot transition to System Test from current mode");
        printf("  [!] Cannot enter System Test mode from %s\n",
               SystemAPI::Mode::getModeName(modeManager.getCurrentMode()));
      }
    }
    
    // Update and render current mode's handlers
    modeManager.update(deltaTime);
    modeManager.render();
    
    // Small delay for button debounce and frame pacing
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}
