/*****************************************************************
 * File:      SystemMode.hpp
 * Category:  SystemAPI
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Defines system operational modes for the CPU including Boot,
 *    Running, Debug, and SystemTest modes. Provides mode transition
 *    management with callbacks and validation.
 *****************************************************************/

#ifndef SYSTEM_API_SYSTEM_MODE_HPP_
#define SYSTEM_API_SYSTEM_MODE_HPP_

#include <cstdint>
#include <functional>
#include <vector>
#include <algorithm>

namespace SystemAPI {
namespace Mode {

// ============================================================
// System Mode Definitions
// ============================================================

/**
 * @brief System operational modes
 * 
 * BOOT        - Initial startup, hardware initialization
 * RUNNING     - Normal operation mode
 * DEBUG       - Debug mode with verbose logging and diagnostics
 * SYSTEM_TEST - Self-test mode for hardware validation
 */
enum class SystemMode : uint8_t {
  BOOT = 0,       ///< System is booting and initializing
  RUNNING,        ///< Normal operation
  DEBUG,          ///< Debug mode with enhanced logging
  SYSTEM_TEST,    ///< Hardware self-test mode
};

/**
 * @brief Get string name for system mode
 */
inline const char* getModeName(SystemMode mode){
  switch(mode){
    case SystemMode::BOOT:        return "Boot";
    case SystemMode::RUNNING:     return "Running";
    case SystemMode::DEBUG:       return "Debug";
    case SystemMode::SYSTEM_TEST: return "System Test";
    default:                      return "Unknown";
  }
}

// ============================================================
// Mode Event Types
// ============================================================

/**
 * @brief Mode transition event types
 */
enum class ModeEvent : uint8_t {
  MODE_ENTER,     ///< Entering a new mode
  MODE_EXIT,      ///< Exiting current mode
  MODE_CHANGED,   ///< Mode has changed (post-transition)
  TEST_STARTED,   ///< System test started
  TEST_COMPLETED, ///< System test completed
  TEST_FAILED,    ///< System test failed
};

/**
 * @brief Mode event data structure
 */
struct ModeEventData {
  ModeEvent type;
  SystemMode previousMode;
  SystemMode currentMode;
  uint32_t timestamp;
  int testResult;         ///< Test result code (0 = pass)
  const char* message;    ///< Optional message
};

/**
 * @brief Mode event callback type
 */
using ModeEventCallback = std::function<void(const ModeEventData&)>;

// ============================================================
// Mode Handler - Attach code to specific modes
// ============================================================

/**
 * @brief Handler functions for a specific mode
 * 
 * Allows attaching initialization, update, render, and cleanup
 * code to specific system modes.
 * 
 * @example
 * ```cpp
 * ModeHandler debugHandler;
 * debugHandler.onEnter = []() { 
 *   ESP_LOGI("DEBUG", "Entering debug mode");
 *   initDebugOverlay();
 * };
 * debugHandler.onUpdate = [](float dt) {
 *   updateDebugMetrics(dt);
 * };
 * debugHandler.onRender = []() {
 *   renderDebugOverlay();
 * };
 * debugHandler.onExit = []() {
 *   cleanupDebugOverlay();
 * };
 * 
 * modeManager.registerHandler(SystemMode::DEBUG, debugHandler);
 * ```
 */
struct ModeHandler {
  std::function<void()> onEnter = nullptr;           ///< Called when entering this mode
  std::function<void()> onExit = nullptr;            ///< Called when exiting this mode
  std::function<void(float)> onUpdate = nullptr;     ///< Called each frame (deltaTime in seconds)
  std::function<void()> onRender = nullptr;          ///< Called each frame for rendering
  std::function<void()> onRenderHUB75 = nullptr;     ///< Called for HUB75 display rendering
  std::function<void()> onRenderOLED = nullptr;      ///< Called for OLED display rendering
  const char* name = nullptr;                        ///< Optional handler name for debugging
};

/**
 * @brief Number of system modes
 */
constexpr int NUM_MODES = 4;

// ============================================================
// System Test Result
// ============================================================

/**
 * @brief Self-test result codes
 */
enum class TestResult : uint8_t {
  PASS = 0,
  FAIL_GPU_COMM,      ///< GPU communication failed
  FAIL_SENSOR_IMU,    ///< IMU sensor failed
  FAIL_SENSOR_ENV,    ///< Environmental sensor failed
  FAIL_DISPLAY_HUB75, ///< HUB75 display failed
  FAIL_DISPLAY_OLED,  ///< OLED display failed
  FAIL_LED_STRIP,     ///< LED strip failed
  FAIL_WIFI,          ///< WiFi failed
  FAIL_SD_CARD,       ///< SD card failed
  FAIL_GPS,           ///< GPS failed
  FAIL_MICROPHONE,    ///< Microphone failed
  FAIL_UNKNOWN,       ///< Unknown failure
};

/**
 * @brief Get test result name
 */
inline const char* getTestResultName(TestResult result){
  switch(result){
    case TestResult::PASS:             return "PASS";
    case TestResult::FAIL_GPU_COMM:    return "GPU Comm Failed";
    case TestResult::FAIL_SENSOR_IMU:  return "IMU Failed";
    case TestResult::FAIL_SENSOR_ENV:  return "Env Sensor Failed";
    case TestResult::FAIL_DISPLAY_HUB75: return "HUB75 Failed";
    case TestResult::FAIL_DISPLAY_OLED:  return "OLED Failed";
    case TestResult::FAIL_LED_STRIP:   return "LED Strip Failed";
    case TestResult::FAIL_WIFI:        return "WiFi Failed";
    case TestResult::FAIL_SD_CARD:     return "SD Card Failed";
    case TestResult::FAIL_GPS:         return "GPS Failed";
    case TestResult::FAIL_MICROPHONE:  return "Microphone Failed";
    case TestResult::FAIL_UNKNOWN:     return "Unknown Failure";
    default:                           return "Unknown";
  }
}

/**
 * @brief Self-test status for individual components
 */
struct TestStatus {
  bool gpuComm = false;
  bool imu = false;
  bool environmental = false;
  bool hub75 = false;
  bool oled = false;
  bool ledStrips = false;
  bool wifi = false;
  bool sdCard = false;
  bool gps = false;
  bool microphone = false;
  
  /** Check if all tests passed */
  bool allPassed() const {
    return gpuComm && imu && environmental && hub75 && 
           oled && ledStrips && wifi && sdCard && gps && microphone;
  }
  
  /** Get count of passed tests */
  int passCount() const {
    int count = 0;
    if(gpuComm) count++;
    if(imu) count++;
    if(environmental) count++;
    if(hub75) count++;
    if(oled) count++;
    if(ledStrips) count++;
    if(wifi) count++;
    if(sdCard) count++;
    if(gps) count++;
    if(microphone) count++;
    return count;
  }
  
  /** Get total number of tests */
  static constexpr int totalTests() { return 10; }
};

// ============================================================
// Mode Manager
// ============================================================

/**
 * @brief System Mode Manager singleton
 * 
 * Manages system operational modes and transitions.
 * 
 * @example
 * ```cpp
 * auto& mode = SystemAPI::Mode::Manager::instance();
 * 
 * // Register mode change callback
 * mode.onModeChange([](const ModeEventData& e) {
 *   printf("Mode: %s -> %s\n", 
 *          getModeName(e.previousMode),
 *          getModeName(e.currentMode));
 * });
 * 
 * // Enter debug mode
 * mode.enterDebugMode();
 * 
 * // Run self-test
 * mode.runSystemTest();
 * ```
 */
class Manager {
public:
  static Manager& instance(){
    static Manager inst;
    return inst;
  }
  
  // ---- Initialization ----
  
  /**
   * @brief Initialize mode manager
   * @param startMode Initial mode (default: BOOT)
   */
  void initialize(SystemMode startMode = SystemMode::BOOT){
    currentMode_ = startMode;
    previousMode_ = startMode;
    testStatus_ = TestStatus{};
    initialized_ = true;
  }
  
  // ---- Mode Queries ----
  
  /** Get current system mode */
  SystemMode getCurrentMode() const { return currentMode_; }
  
  /** Get previous system mode */
  SystemMode getPreviousMode() const { return previousMode_; }
  
  /** Check if in specific mode */
  bool isMode(SystemMode mode) const { return currentMode_ == mode; }
  
  /** Check if in boot mode */
  bool isBooting() const { return currentMode_ == SystemMode::BOOT; }
  
  /** Check if running normally */
  bool isRunning() const { return currentMode_ == SystemMode::RUNNING; }
  
  /** Check if in debug mode */
  bool isDebug() const { return currentMode_ == SystemMode::DEBUG; }
  
  /** Check if running system test */
  bool isSystemTest() const { return currentMode_ == SystemMode::SYSTEM_TEST; }
  
  // ---- Mode Transitions ----
  
  /**
   * @brief Transition to a new mode
   * @param newMode Target mode
   * @return true if transition was valid and completed
   */
  bool setMode(SystemMode newMode){
    if(!isValidTransition(currentMode_, newMode)){
      return false;
    }
    
    // Exit current mode
    emitEvent(ModeEvent::MODE_EXIT);
    callModeExitHandlers();
    
    previousMode_ = currentMode_;
    currentMode_ = newMode;
    
    // Enter new mode
    callModeEnterHandlers();
    emitEvent(ModeEvent::MODE_ENTER);
    emitEvent(ModeEvent::MODE_CHANGED);
    
    return true;
  }
  
  /** Enter running mode (from boot) */
  bool enterRunning(){
    return setMode(SystemMode::RUNNING);
  }
  
  /** Enter debug mode */
  bool enterDebugMode(){
    return setMode(SystemMode::DEBUG);
  }
  
  /** Exit debug mode (return to running) */
  bool exitDebugMode(){
    if(currentMode_ == SystemMode::DEBUG){
      return setMode(SystemMode::RUNNING);
    }
    return false;
  }
  
  /** Toggle debug mode */
  void toggleDebugMode(){
    if(currentMode_ == SystemMode::DEBUG){
      exitDebugMode();
    }else if(currentMode_ == SystemMode::RUNNING){
      enterDebugMode();
    }
  }
  
  /** Enter system test mode */
  bool enterSystemTest(){
    return setMode(SystemMode::SYSTEM_TEST);
  }
  
  /** Exit system test mode (return to running) */
  bool exitSystemTest(){
    if(currentMode_ == SystemMode::SYSTEM_TEST){
      return setMode(SystemMode::RUNNING);
    }
    return false;
  }
  
  // ---- Mode Handler Registration ----
  
  /**
   * @brief Register a handler for a specific mode
   * @param mode The mode to attach this handler to
   * @param handler The handler with callbacks for this mode
   * 
   * @example
   * ```cpp
   * ModeHandler bootHandler;
   * bootHandler.name = "BootScreen";
   * bootHandler.onEnter = []() {
   *   gpu.clear();
   *   gpu.text(0, 0, "Booting...", 0xFFFF);
   * };
   * bootHandler.onUpdate = [](float dt) {
   *   // Update boot animation
   * };
   * bootHandler.onRender = []() {
   *   // Render boot screen
   * };
   * modeManager.registerHandler(SystemMode::BOOT, bootHandler);
   * ```
   */
  void registerHandler(SystemMode mode, const ModeHandler& handler){
    int idx = static_cast<int>(mode);
    if(idx >= 0 && idx < NUM_MODES){
      modeHandlers_[idx].push_back(handler);
    }
  }
  
  /**
   * @brief Clear all handlers for a specific mode
   * @param mode The mode to clear handlers from
   */
  void clearHandlers(SystemMode mode){
    int idx = static_cast<int>(mode);
    if(idx >= 0 && idx < NUM_MODES){
      modeHandlers_[idx].clear();
    }
  }
  
  /**
   * @brief Clear all handlers for all modes
   */
  void clearAllHandlers(){
    for(int i = 0; i < NUM_MODES; i++){
      modeHandlers_[i].clear();
    }
  }
  
  // ---- Update & Render Loop ----
  
  /**
   * @brief Call update handlers for current mode
   * @param deltaTime Time since last update in seconds
   * 
   * Should be called every frame in main loop.
   * 
   * @example
   * ```cpp
   * uint32_t lastTime = millis();
   * while(true) {
   *   uint32_t now = millis();
   *   float dt = (now - lastTime) / 1000.0f;
   *   lastTime = now;
   *   
   *   modeManager.update(dt);
   *   modeManager.render();
   * }
   * ```
   */
  void update(float deltaTime){
    timestampMs_ += static_cast<uint32_t>(deltaTime * 1000);
    int idx = static_cast<int>(currentMode_);
    if(idx >= 0 && idx < NUM_MODES){
      for(auto& handler : modeHandlers_[idx]){
        if(handler.onUpdate){
          handler.onUpdate(deltaTime);
        }
      }
    }
  }
  
  /**
   * @brief Call render handlers for current mode
   * 
   * Should be called every frame after update().
   */
  void render(){
    int idx = static_cast<int>(currentMode_);
    if(idx >= 0 && idx < NUM_MODES){
      for(auto& handler : modeHandlers_[idx]){
        if(handler.onRender){
          handler.onRender();
        }
      }
    }
  }
  
  /**
   * @brief Call HUB75-specific render handlers for current mode
   */
  void renderHUB75(){
    int idx = static_cast<int>(currentMode_);
    if(idx >= 0 && idx < NUM_MODES){
      for(auto& handler : modeHandlers_[idx]){
        if(handler.onRenderHUB75){
          handler.onRenderHUB75();
        }
      }
    }
  }
  
  /**
   * @brief Call OLED-specific render handlers for current mode
   */
  void renderOLED(){
    int idx = static_cast<int>(currentMode_);
    if(idx >= 0 && idx < NUM_MODES){
      for(auto& handler : modeHandlers_[idx]){
        if(handler.onRenderOLED){
          handler.onRenderOLED();
        }
      }
    }
  }
  
  /**
   * @brief Get number of handlers registered for a mode
   * @param mode The mode to query
   * @return Number of registered handlers
   */
  size_t getHandlerCount(SystemMode mode) const {
    int idx = static_cast<int>(mode);
    if(idx >= 0 && idx < NUM_MODES){
      return modeHandlers_[idx].size();
    }
    return 0;
  }
  
  // ---- System Test ----
  
  /**
   * @brief Run full system self-test
   * @param testCallback Optional callback for test progress
   * @return TestStatus with all test results
   * 
   * Note: This is a template for tests - actual test implementations
   * should be provided by the caller via setTestRunner().
   */
  using TestRunner = std::function<TestStatus()>;
  
  /**
   * @brief Set the test runner function
   * @param runner Function that performs actual hardware tests
   */
  void setTestRunner(TestRunner runner){
    testRunner_ = runner;
  }
  
  /**
   * @brief Run system test
   * @return Test status
   */
  TestStatus runSystemTest(){
    if(!enterSystemTest()){
      return testStatus_;
    }
    
    emitEvent(ModeEvent::TEST_STARTED);
    
    // Run tests if runner is set
    if(testRunner_){
      testStatus_ = testRunner_();
    }else{
      // Default: all pass (placeholder)
      testStatus_ = TestStatus{
        true, true, true, true, true, 
        true, true, true, true, true
      };
    }
    
    // Emit completion event
    if(testStatus_.allPassed()){
      emitEvent(ModeEvent::TEST_COMPLETED);
    }else{
      emitEvent(ModeEvent::TEST_FAILED);
    }
    
    exitSystemTest();
    return testStatus_;
  }
  
  /** Get last test status */
  const TestStatus& getTestStatus() const { return testStatus_; }
  
  // ---- Event Handling ----
  
  /**
   * @brief Register mode change callback
   * @return Callback ID for removal
   */
  int onModeChange(ModeEventCallback callback){
    int id = nextCallbackId_++;
    callbacks_.push_back({id, callback});
    return id;
  }
  
  /**
   * @brief Remove callback
   */
  void removeCallback(int id){
    callbacks_.erase(
      std::remove_if(callbacks_.begin(), callbacks_.end(),
        [id](const auto& p) { return p.first == id; }),
      callbacks_.end());
  }
  
  // ---- Debug Features ----
  
  /** Check if verbose logging should be enabled */
  bool shouldLogVerbose() const {
    return currentMode_ == SystemMode::DEBUG || 
           currentMode_ == SystemMode::SYSTEM_TEST;
  }
  
  /** Check if diagnostic overlays should be shown */
  bool shouldShowDiagnostics() const {
    return currentMode_ == SystemMode::DEBUG;
  }
  
private:
  Manager() = default;
  Manager(const Manager&) = delete;
  Manager& operator=(const Manager&) = delete;
  
  bool initialized_ = false;
  SystemMode currentMode_ = SystemMode::BOOT;
  SystemMode previousMode_ = SystemMode::BOOT;
  TestStatus testStatus_;
  TestRunner testRunner_;
  
  std::vector<std::pair<int, ModeEventCallback>> callbacks_;
  int nextCallbackId_ = 1;
  uint32_t timestampMs_ = 0;
  
  // Mode handlers storage
  std::vector<ModeHandler> modeHandlers_[NUM_MODES];
  
  // Call onExit handlers for current mode
  void callModeExitHandlers(){
    int idx = static_cast<int>(currentMode_);
    if(idx >= 0 && idx < NUM_MODES){
      for(auto& handler : modeHandlers_[idx]){
        if(handler.onExit){
          handler.onExit();
        }
      }
    }
  }
  
  // Call onEnter handlers for current mode
  void callModeEnterHandlers(){
    int idx = static_cast<int>(currentMode_);
    if(idx >= 0 && idx < NUM_MODES){
      for(auto& handler : modeHandlers_[idx]){
        if(handler.onEnter){
          handler.onEnter();
        }
      }
    }
  }
  
  void emitEvent(ModeEvent type){
    ModeEventData event = {};
    event.type = type;
    event.previousMode = previousMode_;
    event.currentMode = currentMode_;
    event.timestamp = timestampMs_;
    event.testResult = testStatus_.allPassed() ? 0 : -1;
    event.message = nullptr;
    
    for(auto& cb : callbacks_){
      cb.second(event);
    }
  }
  
  bool isValidTransition(SystemMode from, SystemMode to){
    // Define valid mode transitions
    // Flow: Boot/Debug at startup → Debug→Boot → Boot→Running/SystemTest
    //       Running ↔ SystemTest, Running/SystemTest → Debug
    switch(from){
      case SystemMode::BOOT:
        // From boot, can go to Running or System Test
        return to == SystemMode::RUNNING || 
               to == SystemMode::SYSTEM_TEST;
               
      case SystemMode::RUNNING:
        // From running, can go to System Test or Debug
        return to == SystemMode::SYSTEM_TEST || 
               to == SystemMode::DEBUG;
               
      case SystemMode::DEBUG:
        // From debug, can only go to Boot
        return to == SystemMode::BOOT;
               
      case SystemMode::SYSTEM_TEST:
        // From system test, can go to Running or Debug
        return to == SystemMode::RUNNING || 
               to == SystemMode::DEBUG;
               
      default:
        return false;
    }
  }
};

} // namespace Mode
} // namespace SystemAPI

#endif // SYSTEM_API_SYSTEM_MODE_HPP_
