/*****************************************************************
 * File:      CPU_ModeToggle.cpp
 * Category:  src
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Test program for system modes with mode-specific handlers.
 *    Maps 4 buttons to each mode:
 *    Button A (GPIO 5)  = Boot
 *    Button B (GPIO 6)  = Running
 *    Button C (GPIO 7)  = Debug
 *    Button D (GPIO 15) = System Test
 *    
 *    Demonstrates registering handlers for:
 *    - onEnter:  Called when entering a mode
 *    - onExit:   Called when leaving a mode
 *    - onUpdate: Called each frame (with deltaTime)
 *    - onRender: Called each frame for rendering
 *****************************************************************/

#include <cstdio>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "SystemAPI/SystemMode.hpp"

static const char* TAG = "MODE_TOGGLE";

// ============================================================
// Button Pin Definitions (Active LOW)
// ============================================================
constexpr gpio_num_t BUTTON_A = GPIO_NUM_5;   // Boot mode
constexpr gpio_num_t BUTTON_B = GPIO_NUM_6;   // Running mode
constexpr gpio_num_t BUTTON_C = GPIO_NUM_7;   // Debug mode
constexpr gpio_num_t BUTTON_D = GPIO_NUM_15;  // System Test mode

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
static int testStepCount = 0;

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
  
  // ----- DEBUG MODE HANDLER -----
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
  
  // ----- SYSTEM TEST MODE HANDLER -----
  ModeHandler testHandler;
  testHandler.name = "SystemTest";
  testHandler.onEnter = [](){
    testStepCount = 0;
    printf("  [TEST] ┌─────────────────────────────────┐\n");
    printf("  [TEST] │     SYSTEM TEST SEQUENCE        │\n");
    printf("  [TEST] └─────────────────────────────────┘\n");
    printf("  [TEST] Starting hardware diagnostics...\n");
  };
  testHandler.onUpdate = [](float dt){
    testStepCount++;
  };
  testHandler.onRender = [](){
    // Simulate test progress
    static const char* testItems[] = {
      "GPU Communication",
      "IMU Sensor",
      "Environmental Sensor",
      "HUB75 Display",
      "OLED Display",
      "LED Strips",
      "WiFi Module",
      "SD Card",
      "GPS Module",
      "Microphone"
    };
    
    if(testStepCount <= 10 && testStepCount > 0){
      if(testStepCount % 10 == 1){  // Print every ~0.5 seconds
        int idx = (testStepCount / 10) % 10;
        printf("  [TEST] Testing: %-20s [PASS]\n", testItems[idx]);
      }
    }
  };
  testHandler.onExit = [](){
    printf("  [TEST] ───────────────────────────────────\n");
    printf("  [TEST] All tests completed!\n");
    printf("  [TEST] Total steps: %d\n", testStepCount);
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
  
  // Initialize buttons
  initButtons();
  
  // Print help
  printHelp();
  
  // Print initial mode
  printModeBanner(modeManager.getCurrentMode());
  
  // Timing for update loop
  int64_t lastTime = esp_timer_get_time();
  
  // Main loop
  while(true){
    // Calculate delta time
    int64_t now = esp_timer_get_time();
    float deltaTime = (now - lastTime) / 1000000.0f;  // microseconds to seconds
    lastTime = now;
    
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
    
    // Check Button C - Debug mode
    if(checkButtonPress(BUTTON_C, btnC)){
      ESP_LOGI(TAG, "Button C pressed - requesting Debug mode");
      if(!modeManager.enterDebugMode()){
        ESP_LOGW(TAG, "Cannot transition to Debug from current mode");
        printf("  [!] Cannot enter Debug mode from %s\n",
               SystemAPI::Mode::getModeName(modeManager.getCurrentMode()));
      }
    }
    
    // Check Button D - System Test mode
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
