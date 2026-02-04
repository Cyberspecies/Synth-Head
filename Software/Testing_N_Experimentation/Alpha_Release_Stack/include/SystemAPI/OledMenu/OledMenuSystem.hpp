#pragma once
/**
 * @file OledMenuSystem.hpp
 * @brief Modular OLED Menu System for device control and information display
 * 
 * Features:
 * - Mode selection with smooth scrolling
 * - Multi-page navigation within modes
 * - Sensor data display with pagination
 * - Breadcrumb trail with auto-scroll
 * - NVS persistence for on-time tracking
 * - Animation preset preview in Standard Mode dashboard
 */

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <vector>
#include <string>
#include <functional>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "GpuDriver/GpuCommands.hpp"
#include "SystemAPI/Security/SecurityDriver.hpp"

// Forward declaration for scene activation
namespace SystemAPI { namespace Web { struct SavedScene; } }

namespace OledMenu {

// ============================================================================
// MENU NAVIGATION STRUCTURE
// ============================================================================

enum class MenuState {
    MODE_SELECT,        // Top-level mode selection
    PAGE_VIEW,          // Viewing pages within a mode
    SENSOR_LIST,        // List of sensors (within System Info)
    SENSOR_DETAIL,      // Detailed sensor view (may have multiple pages)
};

enum class Mode {
    STANDARD = 0,       // Default mode: Dashboard + Viewport + HUB75 mirror
    SYSTEM_INFO,
    // Future modes: LED_CONTROL, ANIMATION, SETTINGS, DEBUG
    MODE_COUNT
};

// Available sensors for the sensor page
enum class Sensor {
    IMU_ICM20948 = 0,
    BME280,
    GPS_NEO8M,
    MICROPHONE,
    CALIBRATION,
    SENSOR_COUNT
};

// ============================================================================
// OLED DISPLAY CONSTANTS
// ============================================================================

static constexpr int OLED_WIDTH = 128;
static constexpr int OLED_HEIGHT = 128;
static constexpr int CHAR_WIDTH = 6;        // 5px char + 1px spacing
static constexpr int CHAR_HEIGHT = 8;       // Font height
static constexpr int MAX_CHARS_PER_LINE = 21;  // 128 / 6 = 21
static constexpr int LINE_HEIGHT = 11;      // Line spacing
static constexpr int HEADER_HEIGHT = 14;    // Title bar height
static constexpr int FOOTER_HEIGHT = 10;    // Breadcrumb bar height
static constexpr int CONTENT_START_Y = 18;  // Y position after header
static constexpr int CONTENT_END_Y = 116;   // Y position before footer
static constexpr int MAX_CONTENT_LINES = 9; // (116-18)/11 = ~9 lines

// ============================================================================
// TIMING CONSTANTS
// ============================================================================

static constexpr uint32_t DEBOUNCE_MS = 100;        // Button debounce
static constexpr uint32_t RENDER_NORMAL_MS = 66;    // 15fps for menus
static constexpr uint32_t RENDER_FAST_MS = 50;      // 20fps for animations
static constexpr uint32_t RENDER_MARQUEE_MS = 66;   // 15fps marquee scroll
static constexpr uint32_t RENDER_SENSOR_MS = 66;    // 15fps sensor data
static constexpr uint32_t RENDER_MAX_MS = 1000;     // Force refresh at least 1Hz
static constexpr uint32_t ON_TIME_SAVE_INTERVAL_MS = 60000;  // Save every minute
static constexpr uint32_t HOLD_HOME_MS = 3000;      // Hold back for 3s to go home

// ============================================================================
// SENSOR DATA STRUCTURES
// ============================================================================

struct ImuData {
    float accelX, accelY, accelZ;
    float gyroX, gyroY, gyroZ;
    float magX, magY, magZ;
    float pitch, roll, yaw;
    float temperature;
    bool connected;
};

struct BmeData {
    float temperature;
    float humidity;
    float pressure;
    float altitude;
    bool connected;
};

struct GpsData {
    float latitude;
    float longitude;
    float altitude;
    float speed;
    float course;
    int satellites;
    int hour, minute, second;
    int day, month, year;
    bool hasFix;
    bool connected;
};

struct MicData {
    float dbLevel;
    float peakDb;
    float avgDb;
    bool connected;
};

struct CalibrationData {
    bool imuCalibrated;
    bool bmeCalibrated;
    bool displayCalibrated;
    int imuAccuracy;  // 0-3
};

// ============================================================================
// OLED MENU SYSTEM CLASS
// ============================================================================

class OledMenuSystem {
public:
    // Singleton access
    static OledMenuSystem& instance() {
        static OledMenuSystem inst;
        return inst;
    }
    
    // Initialize the menu system
    void init(GpuCommands* gpu) {
        gpu_ = gpu;
        loadOnTime();
        initButtons();
        needsRender_ = true;
        printf("OLED_MENU: Menu system initialized\n");
    }
    
    // Main update function - call this every frame
    void update(uint32_t currentTimeMs) {
        if (!gpu_) return;
        
        // Update scroll animation
        updateScrollAnimation();
        
        // Handle button input
        handleButtons(currentTimeMs);
        
        // Periodic on-time save
        if (currentTimeMs - lastOnTimeSave_ >= ON_TIME_SAVE_INTERVAL_MS) {
            saveOnTime();
            lastOnTimeSave_ = currentTimeMs;
        }
        
        // Determine render interval based on state
        uint32_t renderInterval = getRenderInterval();
        
        // Also enforce maximum interval to ensure OLED refreshes at least 1Hz
        uint32_t elapsed = currentTimeMs - lastRenderTime_;
        bool forceRender = (elapsed >= RENDER_MAX_MS);
        
        if (needsRender_ || forceRender || (elapsed >= renderInterval)) {
            lastRenderTime_ = currentTimeMs;
            needsRender_ = false;
            render(currentTimeMs);
        }
    }
    
    // Set sensor data (called by main app with fresh sensor readings)
    void setImuData(const ImuData& data) { imuData_ = data; }
    void setBmeData(const BmeData& data) { bmeData_ = data; }
    void setGpsData(const GpsData& data) { gpsData_ = data; }
    void setMicData(const MicData& data) { micData_ = data; }
    void setCalibrationData(const CalibrationData& data) { calibData_ = data; }
    
    // Get current menu state (for external use)
    MenuState getState() const { return state_; }
    
    // Scene/preset preview support
    using SceneActivateCallback = std::function<void(int sceneId)>;
    using LedPresetActivateCallback = std::function<void(int presetId)>;
    
    void setSceneActivateCallback(SceneActivateCallback cb) { sceneActivateCallback_ = std::move(cb); }
    void setAvailableScenes(const std::vector<std::pair<int, std::string>>& scenes) { 
        availableScenes_ = scenes;
        // Clamp preview index to valid range
        if (!availableScenes_.empty() && presetPreviewIndex_ >= (int)availableScenes_.size()) {
            presetPreviewIndex_ = 0;
        }
        // Sync to active scene after updating list
        syncPresetIndexToActive();
    }
    void setActiveSceneId(int sceneId) { 
        activeSceneId_ = sceneId;
        // If not actively browsing, sync the preview index to the new active scene
        if (!presetPreviewActive_) {
            syncPresetIndexToActive();
        }
    }
    int getActiveSceneId() const { return activeSceneId_; }
    int getSelectedPresetIndex() const { return presetPreviewIndex_; }
    
    // Sync presetPreviewIndex_ to point to the active scene (if it exists)
    void syncPresetIndexToActive() {
        for (int i = 0; i < (int)availableScenes_.size(); i++) {
            if (availableScenes_[i].first == activeSceneId_) {
                presetPreviewIndex_ = i;
                return;
            }
        }
        // Active scene not found - keep current index but clamp to valid range
        if (!availableScenes_.empty()) {
            if (presetPreviewIndex_ >= (int)availableScenes_.size()) {
                presetPreviewIndex_ = 0;
            }
        }
    }
    
    // LED Preset support - similar to scene support
    void setLedPresetActivateCallback(LedPresetActivateCallback cb) { ledPresetActivateCallback_ = std::move(cb); }
    void setAvailableLedPresets(const std::vector<std::pair<int, std::string>>& presets) {
        availableLedPresets_ = presets;
        if (!availableLedPresets_.empty() && ledPresetPreviewIndex_ >= (int)availableLedPresets_.size()) {
            ledPresetPreviewIndex_ = 0;
        }
        syncLedPresetIndexToActive();
    }
    void setActiveLedPresetId(int presetId) {
        activeLedPresetId_ = presetId;
        if (!presetPreviewActive_) {
            syncLedPresetIndexToActive();
        }
    }
    int getActiveLedPresetId() const { return activeLedPresetId_; }
    
    // Sync ledPresetPreviewIndex_ to point to the active LED preset
    void syncLedPresetIndexToActive() {
        for (int i = 0; i < (int)availableLedPresets_.size(); i++) {
            if (availableLedPresets_[i].first == activeLedPresetId_) {
                ledPresetPreviewIndex_ = i;
                return;
            }
        }
        if (!availableLedPresets_.empty()) {
            if (ledPresetPreviewIndex_ >= (int)availableLedPresets_.size()) {
                ledPresetPreviewIndex_ = 0;
            }
        }
    }
    
private:
    OledMenuSystem() = default;
    
    // GPU reference
    GpuCommands* gpu_ = nullptr;
    
    // Menu navigation state
    MenuState state_ = MenuState::MODE_SELECT;
    int modeIndex_ = 0;
    int pageIndex_ = 0;
    int sensorIndex_ = 0;
    int sensorPageIndex_ = 0;
    float scrollOffset_ = 0.0f;      // Mode select scroll
    float targetOffset_ = 0.0f;
    float sensorScrollOffset_ = 0.0f; // Sensor list scroll (separate from mode)
    float sensorTargetOffset_ = 0.0f;
    
    // Sensor data cache
    ImuData imuData_ = {};
    BmeData bmeData_ = {};
    GpsData gpsData_ = {};
    MicData micData_ = {};
    CalibrationData calibData_ = {};
    
    // Device info
    const char* deviceName_ = "Lucidius";
    const char* deviceModel_ = "DX.3";
    const char* projectName_ = "Cyberspecies/Synth-Head";
    const char* manufacturedDate_ = "2026-01-27";
    
    // On-time tracking
    uint32_t totalOnTimeSeconds_ = 0;
    uint32_t sessionStartTime_ = 0;
    bool onTimeLoaded_ = false;
    uint32_t lastOnTimeSave_ = 0;
    
    // Button state
    bool lastBtnA_ = true;  // Active LOW (true = released)
    bool lastBtnB_ = true;
    bool lastBtnC_ = true;
    bool lastBtnD_ = true;
    bool buttonsInitialized_ = false;
    uint32_t lastButtonTime_ = 0;
    
    // Hold-to-home state (hold back button 3s to jump to mode select)
    uint32_t backHoldStartTime_ = 0;  // Time when back button was pressed
    bool backHoldActive_ = false;      // Currently tracking a hold
    bool hasSavedState_ = false;       // True if we have a state to return to
    MenuState savedState_ = MenuState::MODE_SELECT;
    int savedModeIndex_ = 0;
    int savedPageIndex_ = 0;
    int savedSensorIndex_ = 0;
    int savedSensorPageIndex_ = 0;
    
    // Render state
    uint32_t lastRenderTime_ = 0;
    bool needsRender_ = true;
    
    // ========================================================================
    // STANDARD MODE - DASHBOARD SELECTION
    // ========================================================================
    // Dashboard has two selection modes: Display Scenes and LED Presets
    // BACK button toggles between them, UP/DOWN navigates, SELECT activates
    enum class DashboardMode {
        DISPLAY_SCENES = 0,   // Navigate/activate display scenes
        LED_PRESETS = 1       // Navigate/activate LED presets
    };
    DashboardMode dashboardMode_ = DashboardMode::DISPLAY_SCENES;
    
    // Display scene preview state
    int presetPreviewIndex_ = 0;         // Index into cached scene list
    bool presetPreviewActive_ = false;   // True when user is browsing presets
    uint32_t presetPreviewTimeout_ = 0;  // Auto-hide preview after 5 seconds of inactivity
    static constexpr uint32_t PRESET_PREVIEW_TIMEOUT_MS = 5000;
    
    // Scene activation callback (set by main app to route to HttpServer)
    SceneActivateCallback sceneActivateCallback_ = nullptr;
    
    // Cached list of available scenes: (id, name) pairs
    std::vector<std::pair<int, std::string>> availableScenes_;
    
    // Currently active scene ID (for showing indicator)
    int activeSceneId_ = -1;
    
    // LED Preset support (similar to display scenes)
    LedPresetActivateCallback ledPresetActivateCallback_ = nullptr;
    std::vector<std::pair<int, std::string>> availableLedPresets_;  // (id, name) pairs
    int activeLedPresetId_ = -1;
    int ledPresetPreviewIndex_ = 0;      // Index into LED preset list
    
    // Multi-slot auto-scroll marquee system
    // Each slot tracks a unique text by its screen position (y*256 + x)
    static constexpr int MAX_MARQUEE_SLOTS = 8;
    static constexpr int MARQUEE_PAUSE_FRAMES = 12;  // Pause at ends
    
    struct MarqueeSlot {
        uint16_t posId;         // Position ID (y*256 + x) for tracking
        int8_t offset;          // Current scroll offset
        int8_t maxOffset;       // Maximum offset for this text
        uint8_t pauseCounter;   // Pause frames remaining
        bool scrollRight;       // Scroll direction
        bool active;            // Slot in use this frame
    };
    MarqueeSlot marqueeSlots_[MAX_MARQUEE_SLOTS] = {};
    uint32_t lastMarqueeTime_ = 0;
    
    // Breadcrumb marquee state (separate for header)
    int breadcrumbOffset_ = 0;
    uint32_t lastBreadcrumbTime_ = 0;
    bool breadcrumbScrollRight_ = true;
    
    // Mode names
    static constexpr const char* modeNames_[] = {
        "Standard",
        "System Info",
        // "LED Control",
        // "Animation", 
        // "Settings",
        // "Debug"
    };
    static constexpr int modeCount_ = 2;  // Update when adding modes
    
    // Sensor names
    static constexpr const char* sensorNames_[] = {
        "IMU (ICM20948)",
        "BME280",
        "GPS (NEO-8M)",
        "Microphone",
        "Calibration"
    };
    static constexpr int sensorCount_ = 5;
    
    // ========================================================================
    // BUTTON HANDLING
    // ========================================================================
    
    void initButtons() {
        // Configure button GPIOs (already configured in main app)
        // Just initialize state tracking
        buttonsInitialized_ = false;
    }
    
    void handleButtons(uint32_t currentTimeMs) {
        // Read button states (active LOW)
        bool btnA = gpio_get_level(GPIO_NUM_5) != 0;   // UP
        bool btnB = gpio_get_level(GPIO_NUM_6) != 0;   // SELECT
        bool btnC = gpio_get_level(GPIO_NUM_7) != 0;   // DOWN
        bool btnD = gpio_get_level(GPIO_NUM_15) != 0;  // BACK
        
        // Initialize on first read
        if (!buttonsInitialized_) {
            lastBtnA_ = btnA;
            lastBtnB_ = btnB;
            lastBtnC_ = btnC;
            lastBtnD_ = btnD;
            buttonsInitialized_ = true;
            printf("OLED_MENU: Buttons initialized A=%d B=%d C=%d D=%d\n", btnA, btnB, btnC, btnD);
            return;
        }
        
        // ================================================================
        // HOLD-TO-HOME: Hold back button for 3 seconds
        // ================================================================
        bool backHeld = !btnD;  // Button is active LOW, so !btnD = pressed
        
        if (backHeld) {
            if (!backHoldActive_) {
                // Just started holding back
                backHoldActive_ = true;
                backHoldStartTime_ = currentTimeMs;
            } else if (currentTimeMs - backHoldStartTime_ >= HOLD_HOME_MS) {
                // Held for 3 seconds - trigger home action
                backHoldActive_ = false;  // Reset hold tracking
                
                if (state_ == MenuState::MODE_SELECT && hasSavedState_) {
                    // Already at home with saved state - return to saved location
                    state_ = savedState_;
                    modeIndex_ = savedModeIndex_;
                    pageIndex_ = savedPageIndex_;
                    sensorIndex_ = savedSensorIndex_;
                    sensorPageIndex_ = savedSensorPageIndex_;
                    hasSavedState_ = false;
                    resetAllMarqueeSlots();
                    needsRender_ = true;
                    printf("OLED_MENU: Returned to saved state\n");
                } else if (state_ != MenuState::MODE_SELECT) {
                    // Not at home - save current state and go home
                    savedState_ = state_;
                    savedModeIndex_ = modeIndex_;
                    savedPageIndex_ = pageIndex_;
                    savedSensorIndex_ = sensorIndex_;
                    savedSensorPageIndex_ = sensorPageIndex_;
                    hasSavedState_ = true;
                    state_ = MenuState::MODE_SELECT;
                    resetAllMarqueeSlots();
                    needsRender_ = true;
                    printf("OLED_MENU: Hold-home triggered, state saved\n");
                }
                // Skip normal button processing this frame
                lastBtnD_ = btnD;
                return;
            }
        } else {
            // Back button released - cancel hold tracking
            backHoldActive_ = false;
        }
        
        // Debounce
        if (currentTimeMs - lastButtonTime_ < DEBOUNCE_MS) {
            return;
        }
        
        // Detect button presses (falling edge: was HIGH, now LOW)
        bool pressedA = (lastBtnA_ && !btnA);
        bool pressedB = (lastBtnB_ && !btnB);
        bool pressedC = (lastBtnC_ && !btnC);
        bool pressedD = (lastBtnD_ && !btnD);
        
        if (pressedA || pressedB || pressedC || pressedD) {
            lastButtonTime_ = currentTimeMs;
            needsRender_ = true;
            
            switch (state_) {
                case MenuState::MODE_SELECT:
                    handleModeSelectInput(pressedA, pressedB, pressedC, pressedD);
                    break;
                case MenuState::PAGE_VIEW:
                    handlePageViewInput(pressedA, pressedB, pressedC, pressedD);
                    break;
                case MenuState::SENSOR_LIST:
                    handleSensorListInput(pressedA, pressedB, pressedC, pressedD);
                    break;
                case MenuState::SENSOR_DETAIL:
                    handleSensorDetailInput(pressedA, pressedB, pressedC, pressedD);
                    break;
            }
        }
        
        lastBtnA_ = btnA;
        lastBtnB_ = btnB;
        lastBtnC_ = btnC;
        lastBtnD_ = btnD;
    }
    
    void handleModeSelectInput(bool up, bool select, bool down, bool back) {
        if (up) {
            modeIndex_ = (modeIndex_ - 1 + modeCount_) % modeCount_;
            targetOffset_ = (float)modeIndex_;
        } else if (down) {
            modeIndex_ = (modeIndex_ + 1) % modeCount_;
            targetOffset_ = (float)modeIndex_;
        } else if (select) {
            state_ = MenuState::PAGE_VIEW;
            pageIndex_ = 0;
            hasSavedState_ = false;  // Clear saved state when entering a mode normally
            resetAllMarqueeSlots();  // Clear scroll states when changing views
            printf("OLED_MENU: Entered mode '%s'\n", modeNames_[modeIndex_]);
        } else if (back && hasSavedState_) {
            // Return to saved location (from hold-back shortcut)
            state_ = savedState_;
            modeIndex_ = savedModeIndex_;
            pageIndex_ = savedPageIndex_;
            sensorIndex_ = savedSensorIndex_;
            sensorPageIndex_ = savedSensorPageIndex_;
            hasSavedState_ = false;
            resetAllMarqueeSlots();
            printf("OLED_MENU: Returned to saved state via back press\n");
        }
        // back does nothing at top level when no saved state
    }
    
    void handlePageViewInput(bool up, bool select, bool down, bool back) {
        // ====================================================================
        // STANDARD MODE: Dashboard Navigation
        // BACK toggles between Display Scenes and LED Presets
        // UP/DOWN cycle through items, SELECT activates
        // (Long-hold BACK for 3s exits to mode select - handled in handleButtons)
        // ====================================================================
        if (modeIndex_ == (int)Mode::STANDARD) {
            if (back) {
                // Toggle between Display Scenes and LED Presets mode
                if (dashboardMode_ == DashboardMode::DISPLAY_SCENES) {
                    dashboardMode_ = DashboardMode::LED_PRESETS;
                    printf("OLED_MENU: Switched to LED Presets mode\n");
                } else {
                    dashboardMode_ = DashboardMode::DISPLAY_SCENES;
                    printf("OLED_MENU: Switched to Display Scenes mode\n");
                }
                presetPreviewActive_ = false;  // Reset preview state on switch
                needsRender_ = true;
                return;  // Back button handled - toggle only, no exit
            }
            
            if (dashboardMode_ == DashboardMode::DISPLAY_SCENES) {
                // === Display Scenes Navigation ===
                if (up || down) {
                    if (!availableScenes_.empty()) {
                        if (up) {
                            presetPreviewIndex_ = (presetPreviewIndex_ - 1 + (int)availableScenes_.size()) % (int)availableScenes_.size();
                        } else {
                            presetPreviewIndex_ = (presetPreviewIndex_ + 1) % (int)availableScenes_.size();
                        }
                        presetPreviewActive_ = true;
                        presetPreviewTimeout_ = (uint32_t)(esp_timer_get_time() / 1000) + PRESET_PREVIEW_TIMEOUT_MS;
                        needsRender_ = true;
                        printf("OLED_MENU: Scene preview index=%d\n", presetPreviewIndex_);
                    }
                } else if (select) {
                    if (presetPreviewActive_ && !availableScenes_.empty() && 
                        presetPreviewIndex_ >= 0 && presetPreviewIndex_ < (int)availableScenes_.size()) {
                        int sceneId = availableScenes_[presetPreviewIndex_].first;
                        printf("OLED_MENU: Activating scene id=%d\n", sceneId);
                        if (sceneActivateCallback_) {
                            sceneActivateCallback_(sceneId);
                        }
                        presetPreviewTimeout_ = (uint32_t)(esp_timer_get_time() / 1000) + PRESET_PREVIEW_TIMEOUT_MS;
                    }
                }
            } else {
                // === LED Presets Navigation ===
                if (up || down) {
                    if (!availableLedPresets_.empty()) {
                        if (up) {
                            ledPresetPreviewIndex_ = (ledPresetPreviewIndex_ - 1 + (int)availableLedPresets_.size()) % (int)availableLedPresets_.size();
                        } else {
                            ledPresetPreviewIndex_ = (ledPresetPreviewIndex_ + 1) % (int)availableLedPresets_.size();
                        }
                        presetPreviewActive_ = true;
                        presetPreviewTimeout_ = (uint32_t)(esp_timer_get_time() / 1000) + PRESET_PREVIEW_TIMEOUT_MS;
                        needsRender_ = true;
                        printf("OLED_MENU: LED preset preview index=%d\n", ledPresetPreviewIndex_);
                    }
                } else if (select) {
                    if (presetPreviewActive_ && !availableLedPresets_.empty() && 
                        ledPresetPreviewIndex_ >= 0 && ledPresetPreviewIndex_ < (int)availableLedPresets_.size()) {
                        int presetId = availableLedPresets_[ledPresetPreviewIndex_].first;
                        printf("OLED_MENU: Activating LED preset id=%d\n", presetId);
                        if (ledPresetActivateCallback_) {
                            ledPresetActivateCallback_(presetId);
                        }
                        presetPreviewTimeout_ = (uint32_t)(esp_timer_get_time() / 1000) + PRESET_PREVIEW_TIMEOUT_MS;
                    }
                }
            }
            return;  // Standard mode handled, don't fall through
        }
        
        // ====================================================================
        // SYSTEM INFO MODE: Page navigation and sensor selection
        // ====================================================================
        if (modeIndex_ == (int)Mode::SYSTEM_INFO) {
            if (up && pageIndex_ > 0) {
                pageIndex_--;
                resetAllMarqueeSlots();
            } else if (down && pageIndex_ < 1) {
                pageIndex_++;
                resetAllMarqueeSlots();
            } else if (select && pageIndex_ == 1) {
                // Page 2 = Sensor List, enter sensor selection
                state_ = MenuState::SENSOR_LIST;
                sensorIndex_ = 0;
                sensorScrollOffset_ = 0;
                sensorTargetOffset_ = 0;
                resetAllMarqueeSlots();
            } else if (back) {
                state_ = MenuState::MODE_SELECT;
                hasSavedState_ = false;  // Clear saved state when navigating normally
                resetAllMarqueeSlots();
                printf("OLED_MENU: Back to mode select\n");
            }
        }
    }
    
    void handleSensorListInput(bool up, bool select, bool down, bool back) {
        if (up) {
            sensorIndex_ = (sensorIndex_ - 1 + sensorCount_) % sensorCount_;
            sensorTargetOffset_ = (float)sensorIndex_;
        } else if (down) {
            sensorIndex_ = (sensorIndex_ + 1) % sensorCount_;
            sensorTargetOffset_ = (float)sensorIndex_;
        } else if (select) {
            state_ = MenuState::SENSOR_DETAIL;
            sensorPageIndex_ = 0;
            resetAllMarqueeSlots();  // Reset scroll states for new view
            printf("OLED_MENU: Viewing sensor '%s'\n", sensorNames_[sensorIndex_]);
        } else if (back) {
            state_ = MenuState::PAGE_VIEW;
            pageIndex_ = 1;  // Back to sensor list page
            resetAllMarqueeSlots();
        }
    }
    
    void handleSensorDetailInput(bool up, bool select, bool down, bool back) {
        int maxPages = getSensorPageCount((Sensor)sensorIndex_);
        
        if (up && sensorPageIndex_ > 0) {
            sensorPageIndex_--;
        } else if (down && sensorPageIndex_ < maxPages - 1) {
            sensorPageIndex_++;
        } else if (back) {
            state_ = MenuState::SENSOR_LIST;
        }
        // select does nothing in detail view
    }
    
    // ========================================================================
    // SCROLL ANIMATION
    // ========================================================================
    
    void updateScrollAnimation() {
        float diff = targetOffset_ - scrollOffset_;
        if (fabsf(diff) > 0.01f) {
            scrollOffset_ += diff * 0.3f;
            needsRender_ = true;
        } else {
            scrollOffset_ = targetOffset_;
        }
    }
    
    // ========================================================================
    // RENDER INTERVAL
    // ========================================================================
    
    uint32_t getRenderInterval() const {
        if (scrollOffset_ != targetOffset_) {
            return RENDER_FAST_MS;  // Animating
        }
        switch (state_) {
            case MenuState::SENSOR_DETAIL:
                return RENDER_SENSOR_MS;  // Live sensor data
            case MenuState::SENSOR_LIST:
                return RENDER_SENSOR_MS;  // Sensor list also uses slower refresh
            case MenuState::PAGE_VIEW:
                // System Info pages have lots of text - use slower refresh
                if (modeIndex_ == (int)Mode::SYSTEM_INFO) {
                    return RENDER_SENSOR_MS;
                }
                return RENDER_MARQUEE_MS;  // Marquee text
            default:
                return RENDER_NORMAL_MS;
        }
    }
    
    // ========================================================================
    // RENDERING
    // ========================================================================
    
    void render(uint32_t currentTimeMs) {
        // Standard mode has its own independent rendering - no menu overhead
        if (state_ == MenuState::PAGE_VIEW && modeIndex_ == (int)Mode::STANDARD) {
            gpu_->oledClear();  // Has built-in 3ms delay
            renderStandardMode(currentTimeMs);
            gpu_->oledPresent();  // Has built-in 2ms delay
            return;
        }
        
        // Update marquee animations
        updateMarqueeAnimations(currentTimeMs);
        beginMarqueeFrame();
        
        gpu_->oledClear();
        
        // Small yield to let other tasks run
        taskYIELD();
        
        switch (state_) {
            case MenuState::MODE_SELECT:
                renderModeSelect();
                break;
            case MenuState::PAGE_VIEW:
                renderPageView(currentTimeMs);
                break;
            case MenuState::SENSOR_LIST:
                renderSensorList();
                break;
            case MenuState::SENSOR_DETAIL:
                renderSensorDetail(currentTimeMs);
                break;
        }
        
        // Always render breadcrumb at bottom (except Standard mode handled above)
        renderBreadcrumb(currentTimeMs);
        
        endMarqueeFrame();
        gpu_->oledPresent();  // Has built-in 2ms delay
    }
    
    void renderModeSelect() {
        // Title bar
        gpu_->oledFill(0, 0, 128, HEADER_HEIGHT, true);
        gpu_->oledTextNative(16, 3, "SELECT MODE", 1, false);
        
        const int centerY = 64;
        const int itemHeight = 24;
        
        // For single mode, just show centered
        if (modeCount_ == 1) {
            int yPos = centerY - 8;
            gpu_->oledFill(4, yPos - 2, 120, 20, true);
            gpu_->oledTextNative(8, yPos + 2, modeNames_[0], 1, false);
            gpu_->oledTextNative(110, yPos + 2, "<", 1, false);
        } else {
            // Multiple modes - scrolling list
            int displayRange = (modeCount_ <= 3) ? (modeCount_ / 2) : 2;
            bool drawn[16] = {false};
            
            for (int i = -displayRange; i <= displayRange; i++) {
                int idx = ((int)scrollOffset_ + i + modeCount_ * 10) % modeCount_;
                if (idx < 0) idx += modeCount_;
                if (drawn[idx]) continue;
                drawn[idx] = true;
                
                float offset = (float)i - (scrollOffset_ - floorf(scrollOffset_));
                int yPos = centerY + (int)(offset * itemHeight) - 8;
                if (yPos < 16 || yPos > 100) continue;
                
                bool selected = (i == 0);
                if (selected) {
                    gpu_->oledFill(4, yPos - 2, 120, 20, true);
                    gpu_->oledTextNative(8, yPos + 2, modeNames_[idx], 1, false);
                    gpu_->oledTextNative(110, yPos + 2, "<", 1, false);
                } else {
                    gpu_->oledTextNative(12, yPos + 2, modeNames_[idx], 1, true);
                }
            }
        }
    }
    
    void renderPageView(uint32_t currentTimeMs) {
        if (modeIndex_ == (int)Mode::STANDARD) {
            renderStandardMode(currentTimeMs);
        } else if (modeIndex_ == (int)Mode::SYSTEM_INFO) {
            if (pageIndex_ == 0) {
                renderSystemInfoPage1(currentTimeMs);
            } else {
                renderSystemInfoPage2(currentTimeMs);
            }
        }
    }
    
    // ========================================================================
    // STANDARD MODE - Default home screen
    // Layout: Dashboard (32px top) + Viewport (64px middle) + HUB75 Mirror (32px bottom)
    // ========================================================================
    
    void renderStandardMode(uint32_t currentTimeMs) {
        // Check for preset preview timeout (revert to showing active)
        uint32_t nowMs = (currentTimeMs > 0) ? currentTimeMs : (uint32_t)(esp_timer_get_time() / 1000);
        if (presetPreviewActive_ && nowMs > presetPreviewTimeout_) {
            presetPreviewActive_ = false;
            if (dashboardMode_ == DashboardMode::DISPLAY_SCENES) {
                syncPresetIndexToActive();
            } else {
                syncLedPresetIndexToActive();
            }
        }
        
        // OLED layout (128x128) with 180° base rotation applied by GPU:
        // - Code Y=0-31 → Physical TOP of display (Dashboard)
        // - Code Y=32-95 → Physical MIDDLE (Viewport)
        // - Code Y=96-127 → Physical BOTTOM (HUB75 Mirror)
        
        // === DASHBOARD SECTION (Top 32 pixels: code Y 0-31) ===
        // White header bar at top - shows which mode we're in
        gpu_->oledFill(0, 0, 128, 12, true);
        
        if (dashboardMode_ == DashboardMode::DISPLAY_SCENES) {
            // === Display Scenes Mode ===
            if (availableScenes_.empty()) {
                gpu_->oledTextNative(4, 2, "DISPLAY SCENES", 1, false);
                gpu_->oledTextNative(4, 16, "None - Add via Web", 1, true);
                gpu_->oledTextNative(4, 24, "[BACK] = LED Mode", 1, true);
            } else {
                if (!presetPreviewActive_) {
                    syncPresetIndexToActive();
                }
                
                if (presetPreviewIndex_ < 0) presetPreviewIndex_ = 0;
                if (presetPreviewIndex_ >= (int)availableScenes_.size()) {
                    presetPreviewIndex_ = (int)availableScenes_.size() - 1;
                }
                
                const auto& scene = availableScenes_[presetPreviewIndex_];
                int sceneId = scene.first;
                const std::string& name = scene.second;
                bool isActive = (sceneId == activeSceneId_);
                
                // Header shows mode
                if (presetPreviewActive_) {
                    gpu_->oledTextNative(4, 2, "SELECT SCENE", 1, false);
                } else if (isActive) {
                    gpu_->oledTextNative(4, 2, "[DISPLAY SCENE]", 1, false);
                } else {
                    gpu_->oledTextNative(4, 2, "DISPLAY SCENE", 1, false);
                }
                
                char presetBuf[48];
                if (isActive) {
                    snprintf(presetBuf, sizeof(presetBuf), "<* %s *>", name.c_str());
                } else {
                    snprintf(presetBuf, sizeof(presetBuf), "< %s >", name.c_str());
                }
                
                int textLen = strlen(presetBuf);
                int textX = (128 - textLen * 6) / 2;
                if (textX < 2) textX = 2;
                gpu_->oledTextNative(textX, 14, presetBuf, 1, true);
                
                char indexBuf[32];
                if (isActive) {
                    snprintf(indexBuf, sizeof(indexBuf), "[%d/%d]", presetPreviewIndex_ + 1, (int)availableScenes_.size());
                } else {
                    snprintf(indexBuf, sizeof(indexBuf), "%d/%d", presetPreviewIndex_ + 1, (int)availableScenes_.size());
                }
                int indexX = (128 - strlen(indexBuf) * 6) / 2;
                gpu_->oledTextNative(indexX, 24, indexBuf, 1, true);
            }
        } else {
            // === LED Presets Mode ===
            if (availableLedPresets_.empty()) {
                gpu_->oledTextNative(4, 2, "LED PRESETS", 1, false);
                gpu_->oledTextNative(4, 16, "None - Add via Web", 1, true);
                gpu_->oledTextNative(4, 24, "[BACK] = Scene Mode", 1, true);
            } else {
                if (!presetPreviewActive_) {
                    syncLedPresetIndexToActive();
                }
                
                if (ledPresetPreviewIndex_ < 0) ledPresetPreviewIndex_ = 0;
                if (ledPresetPreviewIndex_ >= (int)availableLedPresets_.size()) {
                    ledPresetPreviewIndex_ = (int)availableLedPresets_.size() - 1;
                }
                
                const auto& preset = availableLedPresets_[ledPresetPreviewIndex_];
                int presetId = preset.first;
                const std::string& name = preset.second;
                bool isActive = (presetId == activeLedPresetId_);
                
                // Header shows mode
                if (presetPreviewActive_) {
                    gpu_->oledTextNative(4, 2, "SELECT LED", 1, false);
                } else if (isActive) {
                    gpu_->oledTextNative(4, 2, "[LED PRESET]", 1, false);
                } else {
                    gpu_->oledTextNative(4, 2, "LED PRESET", 1, false);
                }
                
                char presetBuf[48];
                if (isActive) {
                    snprintf(presetBuf, sizeof(presetBuf), "<* %s *>", name.c_str());
                } else {
                    snprintf(presetBuf, sizeof(presetBuf), "< %s >", name.c_str());
                }
                
                int textLen = strlen(presetBuf);
                int textX = (128 - textLen * 6) / 2;
                if (textX < 2) textX = 2;
                gpu_->oledTextNative(textX, 14, presetBuf, 1, true);
                
                char indexBuf[32];
                if (isActive) {
                    snprintf(indexBuf, sizeof(indexBuf), "[%d/%d]", ledPresetPreviewIndex_ + 1, (int)availableLedPresets_.size());
                } else {
                    snprintf(indexBuf, sizeof(indexBuf), "%d/%d", ledPresetPreviewIndex_ + 1, (int)availableLedPresets_.size());
                }
                int indexX = (128 - strlen(indexBuf) * 6) / 2;
                gpu_->oledTextNative(indexX, 24, indexBuf, 1, true);
            }
        }
        
        // === VIEWPORT SECTION (Middle 64 pixels: code Y 32-95) ===
        // Draw viewport outline/border (rectangle around the viewport area)
        gpu_->oledLine(0, 32, 127, 32, true);    // Top edge
        gpu_->oledLine(0, 95, 127, 95, true);    // Bottom edge
        gpu_->oledLine(0, 32, 0, 95, true);      // Left edge
        gpu_->oledLine(127, 32, 127, 95, true);  // Right edge
        
        // Declare statusBuf here for use in viewport section
        char statusBuf[32];
        int viewportY = 34;  // Start inside the border
        
        // Show device name centered
        int nameLen = strlen(deviceName_);
        int nameX = (128 - nameLen * 6) / 2;
        gpu_->oledTextNative(nameX, viewportY, deviceName_, 1, true);
        viewportY += 10;
        
        // Show model
        char modelBuf[32];
        snprintf(modelBuf, sizeof(modelBuf), "Model: %s", deviceModel_);
        gpu_->oledTextNative(4, viewportY, modelBuf, 1, true);
        viewportY += 10;
        
        // IMU quick data
        snprintf(statusBuf, sizeof(statusBuf), "IMU: %.1f %.1f %.1f", 
                 imuData_.accelX, imuData_.accelY, imuData_.accelZ);
        gpu_->oledTextNative(4, viewportY, statusBuf, 1, true);
        viewportY += 10;
        
        // Time from GPS (or loading message if no fix)
        if (gpsData_.hasFix && gpsData_.connected) {
            // Show time in HH:MM:SS format
            snprintf(statusBuf, sizeof(statusBuf), "Time: %02d:%02d:%02d", 
                     gpsData_.hour, gpsData_.minute, gpsData_.second);
        } else if (gpsData_.connected) {
            snprintf(statusBuf, sizeof(statusBuf), "GPS loading...");
        } else {
            snprintf(statusBuf, sizeof(statusBuf), "GPS loading...");
        }
        gpu_->oledTextNative(4, viewportY, statusBuf, 1, true);
        viewportY += 10;
        
        // Mic level bar visualization (using dB level, scale from -60dB to 0dB)
        gpu_->oledTextNative(4, viewportY, "MIC:", 1, true);
        // Convert dB to bar width: -60dB = 0, 0dB = 96 pixels
        int barWidth = (int)((micData_.dbLevel + 60.0f) * 96.0f / 60.0f);
        if (barWidth < 0) barWidth = 0;
        if (barWidth > 96) barWidth = 96;
        if (barWidth > 0) {
            gpu_->oledFill(28, viewportY, barWidth, 8, true);
        }
        
        // === HUB75 MIRROR SECTION (Bottom 32 pixels: code Y 96-127) ===
        // Mirror the HUB75 panel content (128x32) to bottom of OLED at 1:1 scale
        // Note: oledMirrorHUB75 writes directly to buffer, bypassing orientation transform
        // With 180° rotation, buffer Y=0-31 = visual bottom, so yOffset=0
        gpu_->oledMirrorHUB75(100, 0, 0);  // threshold 100 for good visibility
    }
    
    void renderSystemInfoPage1(uint32_t currentTimeMs) {
        (void)currentTimeMs;  // Now handled by auto-scroll system
        
        // Title
        gpu_->oledFill(0, 0, 128, HEADER_HEIGHT, true);
        gpu_->oledTextNative(8, 3, "SYSTEM INFO (1/2)", 1, false);
        
        int y = CONTENT_START_Y;
        char buf[64];
        
        // Device Name - auto-scrolls if too long
        snprintf(buf, sizeof(buf), "Name: %s", deviceName_);
        oledText(2, y, buf);
        y += LINE_HEIGHT;
        
        // Model - auto-scrolls if too long
        snprintf(buf, sizeof(buf), "Model: %s", deviceModel_);
        oledText(2, y, buf);
        y += LINE_HEIGHT;
        
        // Project - auto-scrolls if too long
        snprintf(buf, sizeof(buf), "Proj: %s", projectName_);
        oledText(2, y, buf);
        y += LINE_HEIGHT;
        
        // Get WiFi credentials from security driver
        auto& security = arcos::security::SecurityDriver::instance();
        
        // WiFi SSID - auto-scrolls if too long
        snprintf(buf, sizeof(buf), "SSID: %s", security.getSSID());
        oledText(2, y, buf);
        y += LINE_HEIGHT;
        
        // WiFi Password - auto-scrolls if too long
        snprintf(buf, sizeof(buf), "Pass: %s", security.getPassword());
        oledText(2, y, buf);
        y += LINE_HEIGHT;
        
        // Manufactured Date
        snprintf(buf, sizeof(buf), "Mfg: %s", manufacturedDate_);
        oledText(2, y, buf);
        y += LINE_HEIGHT;
        
        // On Time - auto-scrolls if too long
        char timeBuf[24];
        formatOnTime(timeBuf, sizeof(timeBuf), getCurrentOnTime());
        snprintf(buf, sizeof(buf), "On-Time: %s", timeBuf);
        oledText(2, y, buf);
    }
    
    void renderSystemInfoPage2(uint32_t currentTimeMs) {
        (void)currentTimeMs;  // Now handled by auto-scroll system
        
        // Title
        gpu_->oledFill(0, 0, 128, HEADER_HEIGHT, true);
        gpu_->oledTextNative(8, 3, "SENSORS (2/2)", 1, false);
        
        int y = CONTENT_START_Y;
        
        // Auto-scrolls: "Press B to view sensors"
        oledText(2, y, "Press B to view sensors");
        y += LINE_HEIGHT * 2;
        
        // Quick sensor status overview
        char buf[32];
        snprintf(buf, sizeof(buf), "IMU: %s", imuData_.connected ? "OK" : "N/A");
        oledText(2, y, buf);
        y += LINE_HEIGHT;
        
        snprintf(buf, sizeof(buf), "BME: %s", bmeData_.connected ? "OK" : "N/A");
        oledText(2, y, buf);
        y += LINE_HEIGHT;
        
        snprintf(buf, sizeof(buf), "GPS: %s", gpsData_.connected ? (gpsData_.hasFix ? "Fix" : "No Fix") : "N/A");
        oledText(2, y, buf);
        y += LINE_HEIGHT;
        
        snprintf(buf, sizeof(buf), "MIC: %s", micData_.connected ? "OK" : "N/A");
        oledText(2, y, buf);
    }
    
    void renderSensorList() {
        // Title
        gpu_->oledFill(0, 0, 128, HEADER_HEIGHT, true);
        gpu_->oledTextNative(16, 3, "SENSORS", 1, false);
        
        // Smooth scroll animation for sensor list
        if (sensorScrollOffset_ != sensorTargetOffset_) {
            float diff = sensorTargetOffset_ - sensorScrollOffset_;
            sensorScrollOffset_ += diff * 0.3f;  // Smooth interpolation
            if (fabsf(diff) < 0.05f) sensorScrollOffset_ = sensorTargetOffset_;
        }
        
        const int centerY = 64;
        const int itemHeight = 16;
        
        // Render all sensors in a vertical list centered on selection
        for (int i = 0; i < sensorCount_; i++) {
            // Calculate position relative to current selection
            float offset = (float)i - sensorScrollOffset_;
            int yPos = centerY + (int)(offset * itemHeight) - 4;
            
            // Skip if outside visible area
            if (yPos < 16 || yPos > 104) continue;
            
            bool selected = (i == sensorIndex_);
            if (selected) {
                gpu_->oledFill(2, yPos - 2, 124, 14, true);
                // Use auto-scroll for long sensor names
                oledText(6, yPos, sensorNames_[i], 19, false);
            } else {
                oledText(6, yPos, sensorNames_[i], 19, true);
            }
        }
    }
    
    void renderSensorDetail(uint32_t currentTimeMs) {
        switch ((Sensor)sensorIndex_) {
            case Sensor::IMU_ICM20948:
                renderImuDetail();
                break;
            case Sensor::BME280:
                renderBmeDetail();
                break;
            case Sensor::GPS_NEO8M:
                renderGpsDetail();
                break;
            case Sensor::MICROPHONE:
                renderMicDetail();
                break;
            case Sensor::CALIBRATION:
                renderCalibrationDetail();
                break;
            default:
                break;
        }
    }
    
    void renderImuDetail() {
        int maxPages = 2;  // Page 0: Accel/Gyro, Page 1: Mag/Orientation
        char buf[32];
        
        // Title with page number
        gpu_->oledFill(0, 0, 128, HEADER_HEIGHT, true);
        snprintf(buf, sizeof(buf), "IMU (%d/%d)", sensorPageIndex_ + 1, maxPages);
        gpu_->oledTextNative(24, 3, buf, 1, false);
        
        int y = CONTENT_START_Y;
        
        if (!imuData_.connected) {
            gpu_->oledTextNative(20, 50, "NOT CONNECTED", 1, true);
            return;
        }
        
        if (sensorPageIndex_ == 0) {
            // Accelerometer
            gpu_->oledTextNative(2, y, "Accelerometer:", 1, true);
            y += LINE_HEIGHT;
            snprintf(buf, sizeof(buf), " X: %.2f g", imuData_.accelX);
            gpu_->oledTextNative(2, y, buf, 1, true);
            y += LINE_HEIGHT;
            snprintf(buf, sizeof(buf), " Y: %.2f g", imuData_.accelY);
            gpu_->oledTextNative(2, y, buf, 1, true);
            y += LINE_HEIGHT;
            snprintf(buf, sizeof(buf), " Z: %.2f g", imuData_.accelZ);
            gpu_->oledTextNative(2, y, buf, 1, true);
            y += LINE_HEIGHT + 2;
            
            // Gyroscope
            gpu_->oledTextNative(2, y, "Gyroscope:", 1, true);
            y += LINE_HEIGHT;
            snprintf(buf, sizeof(buf), " X: %.1f dps", imuData_.gyroX);
            gpu_->oledTextNative(2, y, buf, 1, true);
            y += LINE_HEIGHT;
            snprintf(buf, sizeof(buf), " Y: %.1f dps", imuData_.gyroY);
            gpu_->oledTextNative(2, y, buf, 1, true);
            y += LINE_HEIGHT;
            snprintf(buf, sizeof(buf), " Z: %.1f dps", imuData_.gyroZ);
            gpu_->oledTextNative(2, y, buf, 1, true);
        } else {
            // Magnetometer
            gpu_->oledTextNative(2, y, "Magnetometer:", 1, true);
            y += LINE_HEIGHT;
            snprintf(buf, sizeof(buf), " X: %.1f uT", imuData_.magX);
            gpu_->oledTextNative(2, y, buf, 1, true);
            y += LINE_HEIGHT;
            snprintf(buf, sizeof(buf), " Y: %.1f uT", imuData_.magY);
            gpu_->oledTextNative(2, y, buf, 1, true);
            y += LINE_HEIGHT;
            snprintf(buf, sizeof(buf), " Z: %.1f uT", imuData_.magZ);
            gpu_->oledTextNative(2, y, buf, 1, true);
            y += LINE_HEIGHT + 2;
            
            // Orientation
            gpu_->oledTextNative(2, y, "Orientation:", 1, true);
            y += LINE_HEIGHT;
            snprintf(buf, sizeof(buf), " Pitch: %.1f", imuData_.pitch);
            gpu_->oledTextNative(2, y, buf, 1, true);
            y += LINE_HEIGHT;
            snprintf(buf, sizeof(buf), " Roll: %.1f", imuData_.roll);
            gpu_->oledTextNative(2, y, buf, 1, true);
            y += LINE_HEIGHT;
            snprintf(buf, sizeof(buf), " Temp: %.1f C", imuData_.temperature);
            gpu_->oledTextNative(2, y, buf, 1, true);
        }
    }
    
    void renderBmeDetail() {
        char buf[32];
        
        gpu_->oledFill(0, 0, 128, HEADER_HEIGHT, true);
        gpu_->oledTextNative(24, 3, "BME280", 1, false);
        
        int y = CONTENT_START_Y;
        
        if (!bmeData_.connected) {
            gpu_->oledTextNative(20, 50, "NOT CONNECTED", 1, true);
            return;
        }
        
        snprintf(buf, sizeof(buf), "Temp: %.1f C", bmeData_.temperature);
        gpu_->oledTextNative(2, y, buf, 1, true);
        y += LINE_HEIGHT;
        
        snprintf(buf, sizeof(buf), "Humidity: %.1f %%", bmeData_.humidity);
        gpu_->oledTextNative(2, y, buf, 1, true);
        y += LINE_HEIGHT;
        
        snprintf(buf, sizeof(buf), "Pressure: %.1f hPa", bmeData_.pressure);
        gpu_->oledTextNative(2, y, buf, 1, true);
        y += LINE_HEIGHT;
        
        snprintf(buf, sizeof(buf), "Altitude: %.1f m", bmeData_.altitude);
        gpu_->oledTextNative(2, y, buf, 1, true);
    }
    
    void renderGpsDetail() {
        int maxPages = 2;
        char buf[32];
        
        gpu_->oledFill(0, 0, 128, HEADER_HEIGHT, true);
        snprintf(buf, sizeof(buf), "GPS (%d/%d)", sensorPageIndex_ + 1, maxPages);
        gpu_->oledTextNative(24, 3, buf, 1, false);
        
        int y = CONTENT_START_Y;
        
        if (!gpsData_.connected) {
            gpu_->oledTextNative(20, 50, "NOT CONNECTED", 1, true);
            return;
        }
        
        if (sensorPageIndex_ == 0) {
            // Position data
            snprintf(buf, sizeof(buf), "Fix: %s", gpsData_.hasFix ? "YES" : "NO");
            gpu_->oledTextNative(2, y, buf, 1, true);
            y += LINE_HEIGHT;
            
            snprintf(buf, sizeof(buf), "Sats: %d", gpsData_.satellites);
            gpu_->oledTextNative(2, y, buf, 1, true);
            y += LINE_HEIGHT;
            
            snprintf(buf, sizeof(buf), "Lat: %.6f", gpsData_.latitude);
            gpu_->oledTextNative(2, y, buf, 1, true);
            y += LINE_HEIGHT;
            
            snprintf(buf, sizeof(buf), "Lon: %.6f", gpsData_.longitude);
            gpu_->oledTextNative(2, y, buf, 1, true);
            y += LINE_HEIGHT;
            
            snprintf(buf, sizeof(buf), "Alt: %.1f m", gpsData_.altitude);
            gpu_->oledTextNative(2, y, buf, 1, true);
        } else {
            // Speed/Time data
            snprintf(buf, sizeof(buf), "Speed: %.1f km/h", gpsData_.speed);
            gpu_->oledTextNative(2, y, buf, 1, true);
            y += LINE_HEIGHT;
            
            snprintf(buf, sizeof(buf), "Course: %.1f", gpsData_.course);
            gpu_->oledTextNative(2, y, buf, 1, true);
            y += LINE_HEIGHT + 4;
            
            gpu_->oledTextNative(2, y, "UTC Time:", 1, true);
            y += LINE_HEIGHT;
            snprintf(buf, sizeof(buf), " %02d:%02d:%02d", gpsData_.hour, gpsData_.minute, gpsData_.second);
            gpu_->oledTextNative(2, y, buf, 1, true);
            y += LINE_HEIGHT;
            
            gpu_->oledTextNative(2, y, "Date:", 1, true);
            y += LINE_HEIGHT;
            snprintf(buf, sizeof(buf), " %04d-%02d-%02d", gpsData_.year, gpsData_.month, gpsData_.day);
            gpu_->oledTextNative(2, y, buf, 1, true);
        }
    }
    
    void renderMicDetail() {
        char buf[32];
        
        gpu_->oledFill(0, 0, 128, HEADER_HEIGHT, true);
        gpu_->oledTextNative(16, 3, "MICROPHONE", 1, false);
        
        int y = CONTENT_START_Y;
        
        if (!micData_.connected) {
            gpu_->oledTextNative(20, 50, "NOT CONNECTED", 1, true);
            return;
        }
        
        snprintf(buf, sizeof(buf), "Level: %.1f dB", micData_.dbLevel);
        gpu_->oledTextNative(2, y, buf, 1, true);
        y += LINE_HEIGHT;
        
        snprintf(buf, sizeof(buf), "Peak: %.1f dB", micData_.peakDb);
        gpu_->oledTextNative(2, y, buf, 1, true);
        y += LINE_HEIGHT;
        
        snprintf(buf, sizeof(buf), "Avg: %.1f dB", micData_.avgDb);
        gpu_->oledTextNative(2, y, buf, 1, true);
        y += LINE_HEIGHT * 2;
        
        // Simple level bar
        gpu_->oledTextNative(2, y, "Level:", 1, true);
        y += LINE_HEIGHT;
        int barWidth = (int)((micData_.dbLevel + 60.0f) * 2.0f);  // -60dB to 0dB -> 0 to 120px
        if (barWidth < 0) barWidth = 0;
        if (barWidth > 120) barWidth = 120;
        gpu_->oledRect(2, y, 124, 10, true);
        if (barWidth > 2) {
            gpu_->oledFill(4, y + 2, barWidth, 6, true);
        }
    }
    
    void renderCalibrationDetail() {
        char buf[32];
        
        gpu_->oledFill(0, 0, 128, HEADER_HEIGHT, true);
        gpu_->oledTextNative(12, 3, "CALIBRATION", 1, false);
        
        int y = CONTENT_START_Y;
        
        snprintf(buf, sizeof(buf), "IMU: %s", calibData_.imuCalibrated ? "Calibrated" : "Not Cal");
        gpu_->oledTextNative(2, y, buf, 1, true);
        y += LINE_HEIGHT;
        
        snprintf(buf, sizeof(buf), " Accuracy: %d/3", calibData_.imuAccuracy);
        gpu_->oledTextNative(2, y, buf, 1, true);
        y += LINE_HEIGHT;
        
        snprintf(buf, sizeof(buf), "BME: %s", calibData_.bmeCalibrated ? "Calibrated" : "Not Cal");
        gpu_->oledTextNative(2, y, buf, 1, true);
        y += LINE_HEIGHT;
        
        snprintf(buf, sizeof(buf), "Display: %s", calibData_.displayCalibrated ? "Calibrated" : "Not Cal");
        gpu_->oledTextNative(2, y, buf, 1, true);
    }
    
    // ========================================================================
    // BREADCRUMB
    // ========================================================================
    
    void renderBreadcrumb(uint32_t currentTimeMs) {
        (void)currentTimeMs;  // Now using auto-scroll system
        
        // Draw separator line
        gpu_->oledLine(0, CONTENT_END_Y + 1, 127, CONTENT_END_Y + 1, true);
        
        // Build breadcrumb string
        char breadcrumb[64];
        buildBreadcrumb(breadcrumb, sizeof(breadcrumb));
        
        int textLen = strlen(breadcrumb);
        int maxChars = MAX_CHARS_PER_LINE;
        
        if (textLen <= maxChars) {
            // Fits - render centered
            int xPos = (OLED_WIDTH - textLen * CHAR_WIDTH) / 2;
            gpu_->oledTextNative(xPos, CONTENT_END_Y + 3, breadcrumb, 1, true);
        } else {
            // Too long - use auto-scroll system (position 2, CONTENT_END_Y+3 is unique)
            oledText(2, CONTENT_END_Y + 3, breadcrumb, maxChars);
        }
    }
    
    void buildBreadcrumb(char* buf, size_t bufSize) {
        switch (state_) {
            case MenuState::MODE_SELECT:
                snprintf(buf, bufSize, "B=Enter");
                break;
            case MenuState::PAGE_VIEW:
                if (modeIndex_ == (int)Mode::SYSTEM_INFO) {
                    snprintf(buf, bufSize, "SysInfo > Page %d", pageIndex_ + 1);
                } else {
                    snprintf(buf, bufSize, "%s", modeNames_[modeIndex_]);
                }
                break;
            case MenuState::SENSOR_LIST:
                snprintf(buf, bufSize, "SysInfo > Sensors");
                break;
            case MenuState::SENSOR_DETAIL:
                {
                    int maxPages = getSensorPageCount((Sensor)sensorIndex_);
                    if (maxPages > 1) {
                        snprintf(buf, bufSize, "SysInfo > Sensors > %s > %d/%d", 
                                 getSensorShortName((Sensor)sensorIndex_),
                                 sensorPageIndex_ + 1, maxPages);
                    } else {
                        snprintf(buf, bufSize, "SysInfo > Sensors > %s", 
                                 getSensorShortName((Sensor)sensorIndex_));
                    }
                }
                break;
        }
    }
    
    const char* getSensorShortName(Sensor s) {
        switch (s) {
            case Sensor::IMU_ICM20948: return "IMU";
            case Sensor::BME280: return "BME";
            case Sensor::GPS_NEO8M: return "GPS";
            case Sensor::MICROPHONE: return "MIC";
            case Sensor::CALIBRATION: return "CAL";
            default: return "?";
        }
    }
    
    int getSensorPageCount(Sensor s) {
        switch (s) {
            case Sensor::IMU_ICM20948: return 2;
            case Sensor::GPS_NEO8M: return 2;
            default: return 1;
        }
    }
    
    void updateBreadcrumbMarquee(uint32_t currentTimeMs, int maxOffset) {
        if (currentTimeMs - lastBreadcrumbTime_ < RENDER_MARQUEE_MS * 2) return;
        lastBreadcrumbTime_ = currentTimeMs;
        
        if (breadcrumbScrollRight_) {
            breadcrumbOffset_++;
            if (breadcrumbOffset_ >= maxOffset) {
                breadcrumbOffset_ = maxOffset;
                breadcrumbScrollRight_ = false;
            }
        } else {
            breadcrumbOffset_--;
            if (breadcrumbOffset_ <= 0) {
                breadcrumbOffset_ = 0;
                breadcrumbScrollRight_ = true;
            }
        }
    }
    
    // ========================================================================
    // AUTO-SCROLL TEXT SYSTEM
    // ========================================================================
    // Automatically scrolls any text that's too long for the display.
    // Use oledText() instead of oledTextNative() for auto-scroll support.
    // Each screen position (x,y) gets its own independent scroll state.
    // ========================================================================
    
    // Call at start of each render frame to mark all slots inactive
    void beginMarqueeFrame() {
        for (int i = 0; i < MAX_MARQUEE_SLOTS; i++) {
            marqueeSlots_[i].active = false;
        }
    }
    
    // Call at end of render frame to clean up unused slots
    void endMarqueeFrame() {
        for (int i = 0; i < MAX_MARQUEE_SLOTS; i++) {
            if (!marqueeSlots_[i].active && marqueeSlots_[i].posId != 0) {
                // Slot was used last frame but not this frame - reset it
                marqueeSlots_[i].posId = 0;
                marqueeSlots_[i].offset = 0;
                marqueeSlots_[i].pauseCounter = MARQUEE_PAUSE_FRAMES;
                marqueeSlots_[i].scrollRight = true;
            }
        }
    }
    
    // Reset all marquee slots (call when changing menu views)
    void resetAllMarqueeSlots() {
        for (int i = 0; i < MAX_MARQUEE_SLOTS; i++) {
            marqueeSlots_[i].posId = 0;
            marqueeSlots_[i].offset = 0;
            marqueeSlots_[i].pauseCounter = MARQUEE_PAUSE_FRAMES;
            marqueeSlots_[i].scrollRight = true;
            marqueeSlots_[i].active = false;
        }
    }
    
    // Find or create a marquee slot for this position
    MarqueeSlot* getMarqueeSlot(int x, int y, int maxOffset) {
        uint16_t posId = (uint16_t)(y * 256 + x);
        
        // Look for existing slot
        for (int i = 0; i < MAX_MARQUEE_SLOTS; i++) {
            if (marqueeSlots_[i].posId == posId) {
                marqueeSlots_[i].active = true;
                marqueeSlots_[i].maxOffset = (int8_t)maxOffset;
                return &marqueeSlots_[i];
            }
        }
        
        // Find empty slot
        for (int i = 0; i < MAX_MARQUEE_SLOTS; i++) {
            if (marqueeSlots_[i].posId == 0) {
                marqueeSlots_[i].posId = posId;
                marqueeSlots_[i].offset = 0;
                marqueeSlots_[i].maxOffset = (int8_t)maxOffset;
                marqueeSlots_[i].pauseCounter = MARQUEE_PAUSE_FRAMES;
                marqueeSlots_[i].scrollRight = true;
                marqueeSlots_[i].active = true;
                return &marqueeSlots_[i];
            }
        }
        
        // No slots available - reuse first inactive
        for (int i = 0; i < MAX_MARQUEE_SLOTS; i++) {
            if (!marqueeSlots_[i].active) {
                marqueeSlots_[i].posId = posId;
                marqueeSlots_[i].offset = 0;
                marqueeSlots_[i].maxOffset = (int8_t)maxOffset;
                marqueeSlots_[i].pauseCounter = MARQUEE_PAUSE_FRAMES;
                marqueeSlots_[i].scrollRight = true;
                marqueeSlots_[i].active = true;
                return &marqueeSlots_[i];
            }
        }
        
        return nullptr;  // All slots full and active
    }
    
    // Update all active marquee animations
    void updateMarqueeAnimations(uint32_t currentTimeMs) {
        if (currentTimeMs - lastMarqueeTime_ < RENDER_MARQUEE_MS) return;
        lastMarqueeTime_ = currentTimeMs;
        
        for (int i = 0; i < MAX_MARQUEE_SLOTS; i++) {
            MarqueeSlot& slot = marqueeSlots_[i];
            if (!slot.active || slot.posId == 0) continue;
            
            if (slot.pauseCounter > 0) {
                slot.pauseCounter--;
            } else if (slot.scrollRight) {
                slot.offset++;
                if (slot.offset >= slot.maxOffset) {
                    slot.offset = slot.maxOffset;
                    slot.scrollRight = false;
                    slot.pauseCounter = MARQUEE_PAUSE_FRAMES;
                }
            } else {
                slot.offset--;
                if (slot.offset <= 0) {
                    slot.offset = 0;
                    slot.scrollRight = true;
                    slot.pauseCounter = MARQUEE_PAUSE_FRAMES;
                }
            }
        }
    }
    
    /**
     * @brief Auto-scrolling text display
     * 
     * Displays text at the given position. If the text is longer than maxChars,
     * it will automatically scroll back and forth to show the full content.
     * Each unique (x,y) position maintains its own scroll state.
     * 
     * @param x X position on screen
     * @param y Y position on screen  
     * @param text The text to display
     * @param maxChars Maximum characters that fit (default: MAX_CHARS_PER_LINE)
     * @param white true for white text, false for black
     */
    void oledText(int x, int y, const char* text, int maxChars = MAX_CHARS_PER_LINE, bool white = true) {
        int textLen = strlen(text);
        
        // Text fits - just display it
        if (textLen <= maxChars) {
            gpu_->oledTextNative(x, y, text, 1, white);
            return;
        }
        
        // Text needs scrolling
        int maxOffset = textLen - maxChars;
        MarqueeSlot* slot = getMarqueeSlot(x, y, maxOffset);
        
        int offset = 0;
        if (slot) {
            offset = slot->offset;
            if (offset > maxOffset) offset = maxOffset;
            if (offset < 0) offset = 0;
        }
        
        char displayBuf[32];
        strncpy(displayBuf, text + offset, maxChars);
        displayBuf[maxChars] = '\0';
        gpu_->oledTextNative(x, y, displayBuf, 1, white);
    }
    
    // Legacy wrapper for compatibility
    void renderMarqueeText(int x, int y, const char* text, int maxChars, uint32_t /*currentTimeMs*/) {
        oledText(x, y, text, maxChars, true);
    }
    
    // ========================================================================
    // ON-TIME TRACKING
    // ========================================================================
    
    void loadOnTime() {
        nvs_handle_t handle;
        esp_err_t err = nvs_open("oled_menu", NVS_READONLY, &handle);
        if (err == ESP_OK) {
            err = nvs_get_u32(handle, "total_ontime", &totalOnTimeSeconds_);
            if (err != ESP_OK) totalOnTimeSeconds_ = 0;
            nvs_close(handle);
            printf("OLED_MENU: Loaded on-time: %lu sec\n", totalOnTimeSeconds_);
        } else {
            totalOnTimeSeconds_ = 0;
        }
        onTimeLoaded_ = true;
        sessionStartTime_ = (uint32_t)(esp_timer_get_time() / 1000000);
    }
    
    void saveOnTime() {
        nvs_handle_t handle;
        esp_err_t err = nvs_open("oled_menu", NVS_READWRITE, &handle);
        if (err == ESP_OK) {
            uint32_t sessionSec = (uint32_t)(esp_timer_get_time() / 1000000) - sessionStartTime_;
            uint32_t newTotal = totalOnTimeSeconds_ + sessionSec;
            nvs_set_u32(handle, "total_ontime", newTotal);
            nvs_commit(handle);
            nvs_close(handle);
            totalOnTimeSeconds_ = newTotal;
            sessionStartTime_ = (uint32_t)(esp_timer_get_time() / 1000000);
        }
    }
    
    uint32_t getCurrentOnTime() {
        uint32_t sessionSec = (uint32_t)(esp_timer_get_time() / 1000000) - sessionStartTime_;
        return totalOnTimeSeconds_ + sessionSec;
    }
    
    void formatOnTime(char* buf, size_t bufSize, uint32_t seconds) {
        uint32_t days = seconds / 86400;
        uint32_t hours = (seconds % 86400) / 3600;
        uint32_t mins = (seconds % 3600) / 60;
        
        if (days > 0) {
            snprintf(buf, bufSize, "%lud %luh %lum", days, hours, mins);
        } else if (hours > 0) {
            snprintf(buf, bufSize, "%luh %lum", hours, mins);
        } else {
            snprintf(buf, bufSize, "%lum %lus", mins, seconds % 60);
        }
    }
};

// Constexpr static member definitions
constexpr const char* OledMenuSystem::modeNames_[];
constexpr const char* OledMenuSystem::sensorNames_[];

} // namespace OledMenu
