/**
 * @file CurrentMode.cpp
 * @brief Current mode implementation using SystemAPI
 * 
 * This is the main application mode that orchestrates:
 * - Hardware drivers (GPS, Mic, IMU, Fan) via modular driver files
 * - GPU communication via GpuCommands and SceneComposer
 * - Web server and captive portal
 * - Dual-core application layer (animation on Core 1)
 * 
 * Hardware drivers are now in separate files under src/Drivers/ and include/Drivers/
 */

#include "CurrentMode.hpp"
#include "SystemAPI/SystemAPI.hpp"
#include "SystemAPI/Web/CaptivePortal.hpp"
#include "SystemAPI/Security/SecurityDriver.hpp"
#include "SystemAPI/Misc/SyncState.hpp"
#include "SystemAPI/Utils/FileSystemService.hpp"
#include "SystemAPI/GPU/GpuDriver.h"  // SAME as WifiSpriteUploadTest
#include "Application/Application.hpp"  // Dual-core application layer
#include "Application/Pipeline/SceneRenderer.hpp"  // Scene renderer for Core 1

// Scene Composition System
#include "GpuDriver/GpuCommands.hpp"
#include "FrameworkAPI/SceneComposer.hpp"

// Animation System
#include "AnimationSystem/AnimationSystem.hpp"
#include "AnimationSystem/AnimationSandbox.hpp"
#include "AnimationSystem/Animations/ComplexTransitionAnim.hpp"
#include "Modes/AnimationHandler.hpp"

// Modular hardware drivers
#include "Drivers/GpsDriver.hpp"
#include "Drivers/MicDriver.hpp"
#include "Drivers/ImuDriver.hpp"
#include "Drivers/FanDriver.hpp"
#include "Drivers/BmeDriver.hpp"

// YAML-based Scene Manager (new modular scene system)
#include "Drivers/YamlSceneDriver.hpp"

// LED Strip support (ESP-IDF RMT driver)
#include "driver/rmt_tx.h"
#include "HAL/led_strip_encoder.h"
#include "HAL/CPU_HAL_Config.hpp"

// Test harness for console-based scene testing
#include "SystemAPI/Testing/SceneTestHarness.hpp"
#include "SystemAPI/Testing/LedStripTestHarness.hpp"

// Modular OLED Menu System
#include "SystemAPI/OledMenu/OledMenuSystem.hpp"

#include "esp_timer.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <vector>
#include <map>
#include <string>

namespace Modes {

//=============================================================================
// GPU UART Driver - ESP-to-ESP Communication (Proper Protocol)
//=============================================================================
// Use GpuCommands for both direct GPU access and SceneComposer
// Import types from SystemAPI namespace for cleaner usage (but use GpuCommands for GPU)
using SystemAPI::GpuConfig;
using SystemAPI::GpuTarget;
using SystemAPI::SpriteFormat;
using SystemAPI::Color;

// NOTE: We now use GpuCommands instead of SystemAPI::GpuDriver
// GpuCommands is required by SceneComposer and provides the same functionality
static GpuCommands g_gpu;  // Global GpuCommands instance - used by SceneComposer

//=============================================================================
// Scene Composition System
//=============================================================================
// SceneComposer manages layer-based scene composition for displays
static SceneAPI::SceneComposer g_sceneComposer;  // Scene composition system
static bool g_sceneComposerEnabled = true;  // Use SceneComposer for HUB75 rendering
static SceneAPI::Scene* g_activeHubScene = nullptr;  // Current active HUB75 scene
static SceneAPI::Scene* g_activeOledScene = nullptr;  // Current active OLED scene

namespace GpuDriverState {
    // GPU UART pins (from PIN_MAPPING_CPU.md)
    static constexpr int GPU_TX_PIN = 12;   // CPU TX -> GPU RX
    static constexpr int GPU_RX_PIN = 11;   // CPU RX <- GPU TX
    
    static bool initialized = false;
    static bool connected = false;
    static uint32_t gpuUptimeMs = 0;    // GPU uptime from PONG response
    static uint32_t lastPingTime = 0;
    // static uint32_t lastStatsTime = 0;  // Reserved for GPU stats polling
    static constexpr uint32_t PING_INTERVAL_MS = 5000;  // Ping every 5s (reduced to avoid congestion)
    // static constexpr uint32_t STATS_INTERVAL_MS = 10000; // Reserved for GPU stats polling
    
    // GPU stats (reserved for future diagnostic display)
    // static float gpuFps = 0.0f;
    // static uint32_t gpuFreeHeap = 0;
    // static uint32_t gpuMinHeap = 0;
    // static uint8_t gpuLoad = 0;
    // static uint32_t gpuTotalFrames = 0;
    // static bool gpuHub75Ok = false;
    // static bool gpuOledOk = false;
    
    // ====== SPRITE RENDERING STATE (like WifiSpriteUploadTest) ======
    // This allows continuous rendering at ~60fps from the update loop
    static bool spriteReady = false;        // Is sprite uploaded and ready to render?
    static uint8_t activeSpriteId = 0;      // GPU sprite cache slot
    static float spriteX = 64.0f;           // Center X position
    static float spriteY = 16.0f;           // Center Y position
    static float spriteAngle = 0.0f;        // Rotation angle (degrees)
    static uint8_t bgR = 0, bgG = 0, bgB = 0;  // Background color
    static uint32_t lastRenderTime = 0;     // For fps throttle
    static constexpr uint32_t RENDER_INTERVAL_MS = 33;  // ~30fps (GPU-safe, prevents buffer overflow)
    static bool autoRotate = false;         // Auto-rotate flag (true for test, false for web uploads)
    
    // ====== SANDBOX MODE STATE ======
    static bool sandboxEnabled = false;  // Disabled - use scene-based rendering
    static float sandboxPitch = 0.0f;
    static float sandboxRoll = 0.0f;
    static float sandboxAudioLevel = 0.0f;
    
    // ====== LED STRIP STATE (RMT-based) ======
    struct LedStripHandle {
        rmt_channel_handle_t channel;
        rmt_encoder_handle_t encoder;
        uint8_t* pixelBuffer;   // GRB format
        uint8_t ledCount;
        gpio_num_t pin;
        bool initialized;
    };
    static LedStripHandle ledStrips[6] = {};
    static bool ledsInitialized = false;
    static uint8_t currentLedR = 0, currentLedG = 0, currentLedB = 0;
    static uint8_t currentLedBrightness = 80;
    static bool ledsEnabled = false;
    
    // LED strip configuration from PIN_MAPPING_CPU.md
    static const gpio_num_t LED_PINS[6] = {
        GPIO_NUM_16, GPIO_NUM_18, GPIO_NUM_8, 
        GPIO_NUM_39, GPIO_NUM_38, GPIO_NUM_37
    };
    static const uint8_t LED_COUNTS[6] = {0, 13, 9, 0, 13, 14};
    static const char* LED_NAMES[6] = {
        "Unused0", "LeftFin", "Tongue", "Unused3", "RightFin", "ScaleLEDs"
    };
    
    // Initialize sandbox on first use (reserved for future sandbox feature)
    // static bool sandboxInitialized = false;
    
    // ====== SCENE-BASED ANIMATION STATE ======
    // Animation modes from scene manager
    enum class SceneAnimMode {
        NONE,             // No animation (shows black)
        GYRO_EYES,        // Gyro-controlled eyes (legacy)
        STATIC_IMAGE,     // Static sprite display (uses mirrorEnabled flag)
        STATIC_MIRRORED,  // Legacy: Static sprite on both panels (mapped to STATIC_IMAGE + mirror)
        SWAY,             // Swaying sprite animation (legacy)
        SDF_MORPH,        // SDF morphing animation (legacy)
        REACTIVE_EYES     // IMU-reactive eye animation with asymmetric left/right movement
    };
    static SceneAnimMode currentAnimMode = SceneAnimMode::NONE;  // Default to black until scene is activated
    static bool sceneAnimInitialized = false;
    
    // Gyro eyes state
    static float eyeSize = 12.0f;
    static float eyeSensitivity = 1.0f;
    static bool eyeMirror = true;
    static int eyeSpriteId = -1;  // -1 = use default circle
    
    // Sway animation state (legacy - disabled)
    // static float swayTime = 0.0f;
    static float swayXIntensity = 10.0f;
    static float swayYIntensity = 5.0f;
    static float swayRotRange = 15.0f;
    static float swaySpeed = 1.0f;
    static bool swayCosX = false;
    
    // Reactive eyes animation state (IMU-based asymmetric movement)
    // Uses CALIBRATED device-frame GYRO values (not raw IMU, not accelerometer)
    // Movement behavior (per calibrated gyro axis):
    //   +Y gyro: Right sprite moves UP, Left sprite moves DOWN
    //   +Z gyro: Right sprite moves FORWARD (right), Left sprite moves BACKWARD (left)
    //   +X gyro: Both sprites move DOWN + rotate (Right=clockwise, Left=counter-clockwise)
    // Base positions: center of each display half (Left=32,16  Right=96,16)
    static float reactiveYSensitivity = 15.0f;    // Sensitivity for Y-axis (vertical asymmetric)
    static float reactiveZSensitivity = 10.0f;    // Sensitivity for Z-axis (horizontal asymmetric)
    static float reactiveXSensitivity = 12.0f;    // Sensitivity for X-axis (vertical + rotation)
    static float reactiveRotSensitivity = 10.0f;  // Rotation sensitivity for X-axis
    static float reactiveSmoothing = 0.15f;       // Smoothing factor (0.0-1.0, lower = smoother)
    static float reactiveLPosXSmooth = 32.0f;     // Smoothed left X position (starts at center)
    static float reactiveLPosYSmooth = 16.0f;     // Smoothed left Y position (starts at center)
    static float reactiveLRotSmooth = 0.0f;       // Smoothed left rotation
    static float reactiveRPosXSmooth = 96.0f;     // Smoothed right X position (starts at center)
    static float reactiveRPosYSmooth = 16.0f;     // Smoothed right Y position (starts at center)
    static float reactiveRRotSmooth = 180.0f;     // Smoothed right rotation (180° = facing opposite)
    static float reactiveBaseLeftX = 32.0f;       // Base center X for left sprite (center of left display: 0-63)
    static float reactiveBaseLeftY = 16.0f;       // Base center Y for left sprite (center of 32-pixel height)
    static float reactiveBaseRightX = 96.0f;      // Base center X for right sprite (center of right display: 64-127)
    static float reactiveBaseRightY = 16.0f;      // Base center Y for right sprite (center of 32-pixel height)
    static float reactiveScale = 1.0f;            // Scale for both sprites
    static float reactiveLeftRotOffset = 0.0f;    // Rotation offset for left sprite (added to final rotation)
    static float reactiveRightRotOffset = 0.0f;   // Rotation offset for right sprite (added to final rotation, base is 180)
    static bool reactiveLeftFlipX = false;        // Horizontal flip for left sprite
    static bool reactiveRightFlipX = false;       // Horizontal flip for right sprite
    
    // Static image state (used when mirrorEnabled = false)
    static float staticScale = 1.0f;
    static float staticRotation = 0.0f;
    static float staticPosX = 64.0f;
    static float staticPosY = 16.0f;
    static bool staticFlipX = false;  // Horizontal flip for single sprite
    
    // Mirror toggle - when true, shows left/right controls; when false, shows single sprite
    static bool mirrorEnabled = false;
    
    // Static mirrored state (used when mirrorEnabled = true)
    static float leftPosX = 32.0f;
    static float leftPosY = 16.0f;
    static float leftRotation = 0.0f;
    static float leftScale = 1.0f;
    static bool leftFlipX = false;   // Horizontal flip for left sprite
    static float rightPosX = 96.0f;
    static float rightPosY = 16.0f;
    static float rightRotation = 180.0f;
    static float rightScale = 1.0f;
    static bool rightFlipX = false;  // Horizontal flip for right sprite
    
    // ====== SHADER STATE ======
    static uint8_t shaderType = 0;           // 0 = None, 1 = Color Override, 2 = Hue Cycle
    static bool shaderInvert = false;        // Invert colors before masking
    static bool shaderMaskEnabled = true;    // Mask transparency enabled
    static uint8_t shaderMaskR = 0;          // Mask color R (default black)
    static uint8_t shaderMaskG = 0;          // Mask color G
    static uint8_t shaderMaskB = 0;          // Mask color B
    static uint8_t shaderOverrideR = 255;    // Override color R (default white)
    static uint8_t shaderOverrideG = 255;    // Override color G
    static uint8_t shaderOverrideB = 255;    // Override color B
    static bool shaderDirty = true;          // Flag to send shader config to GPU
    
    // Hue/Gradient cycle shader specific state
    static uint16_t shaderHueCycleSpeed = 1000;  // Speed in ms per color transition
    static uint8_t shaderHueCycleColorCount = 5; // Number of colors in palette (1-32)
    static const uint8_t MAX_HUE_COLORS = 32;
    static uint8_t shaderHuePalette[MAX_HUE_COLORS * 3] = {
        255, 0, 0,       // Color 1: Red (default)
        255, 255, 0,     // Color 2: Yellow (default)
        0, 255, 0,       // Color 3: Green (default)
        0, 0, 255,       // Color 4: Blue (default)
        128, 0, 255,     // Color 5: Purple (default)
        // Remaining 27 colors default to red
        255, 0, 0, 255, 0, 0, 255, 0, 0, 255, 0, 0, 255, 0, 0,
        255, 0, 0, 255, 0, 0, 255, 0, 0, 255, 0, 0, 255, 0, 0,
        255, 0, 0, 255, 0, 0, 255, 0, 0, 255, 0, 0, 255, 0, 0,
        255, 0, 0, 255, 0, 0, 255, 0, 0, 255, 0, 0, 255, 0, 0,
        255, 0, 0, 255, 0, 0, 255, 0, 0, 255, 0, 0, 255, 0, 0,
        255, 0, 0, 255, 0, 0
    };
    static bool shaderPaletteDirty = true;   // Flag to send palette to GPU
    
    // Gradient cycle shader specific state
    static uint16_t shaderGradientDistance = 20;  // Pixels between color bands
    static int16_t shaderGradientAngle = 0;       // Travel angle in degrees (-180 to 180)
    static bool shaderGradientMirror = false;     // Mirror gradient on right panel
    static bool shaderGradientParamsDirty = true; // Flag to send gradient params to GPU
    
    // Glitch shader specific state
    static uint8_t shaderGlitchSpeed = 50;        // Speed of glitch updates (0-100)
    static uint8_t shaderGlitchIntensity = 30;    // Intensity of displacement (0-100)
    static uint8_t shaderGlitchChromatic = 20;    // Chromatic aberration amount (0-100)
    static uint8_t shaderGlitchQuantity = 50;     // Quantity/density of glitch bands (0-100)
    static bool shaderGlitchParamsDirty = true;   // Flag to send glitch params to GPU
    
    // ====== MIC REACTIVITY MANAGER ======
    // Proper encapsulated system for mic reactivity
    // Base values are stored separately and NEVER modified
    // Final values are calculated on-demand: final = base + smoothedOffset
    class MicReactivityManager {
    public:
        struct MicSetting {
            bool enabled = false;
            float x = 0.0f;   // Threshold
            float y = 1.0f;   // Multiplier
            float z = 0.0f;   // Offset
        };
        
    private:
        std::map<std::string, float> baseValues_;      // Original values from scene
        std::map<std::string, float> smoothedOffsets_; // Smoothed mic offsets
        std::map<std::string, MicSetting> settings_;   // Mic settings per param
        bool initialized_ = false;
        float smoothingFactor_ = 0.3f;
        
    public:
        // Initialize with scene data - call when scene is activated
        void initialize() {
            baseValues_.clear();
            smoothedOffsets_.clear();
            settings_.clear();
            initialized_ = true;
            printf("[MicReact] Manager initialized\n");
        }
        
        // Store a base value (from scene params)
        void setBaseValue(const std::string& param, float value) {
            baseValues_[param] = value;
        }
        
        // Load mic settings from active scene
        void loadSettingsFromScene() {
            settings_.clear();
            auto& httpServer = SystemAPI::Web::HttpServer::instance();
            const auto* activeScene = httpServer.getActiveScene();
            if (!activeScene) return;
            
            for (const auto& pair : activeScene->micReact) {
                MicSetting s;
                s.enabled = pair.second.enabled;
                s.x = pair.second.x;
                s.y = pair.second.y;
                s.z = pair.second.z;
                settings_[pair.first] = s;
                if (s.enabled) {
                    printf("[MicReact] Loaded setting: '%s' X=%.2f Y=%.2f Z=%.2f\n",
                           pair.first.c_str(), s.x, s.y, s.z);
                }
            }
        }
        
        // Update smoothed values based on current mic level
        // Call this once per frame
        // The equation (Y*(mic-X))+Z produces the FINAL value directly
        void update(float micDb) {
            if (!initialized_) return;
            
            for (auto& pair : settings_) {
                if (!pair.second.enabled) continue;
                
                const std::string& paramName = pair.first;
                const MicSetting& setting = pair.second;
                
                // Calculate equation result: Y * (mic - X) + Z
                // This IS the final value, not an offset
                float rawValue = setting.y * (micDb - setting.x) + setting.z;
                
                // Apply smoothing
                float& smoothed = smoothedOffsets_[paramName];
                smoothed = smoothed + (rawValue - smoothed) * smoothingFactor_;
            }
        }
        
        // Get the final value for a parameter (base + offset)
        // If no mic reactivity for this param, returns the base value
        // NOTE: The equation (Y*(mic-X))+Z produces the FINAL value directly,
        // not an offset. So when enabled, we return the equation result directly.
        float getFinalValue(const std::string& param, float fallbackBase) const {
            float base = fallbackBase;
            auto baseIt = baseValues_.find(param);
            if (baseIt != baseValues_.end()) {
                base = baseIt->second;
            }
            
            auto settingIt = settings_.find(param);
            if (settingIt == settings_.end() || !settingIt->second.enabled) {
                return base;  // No mic reactivity, return base
            }
            
            auto offsetIt = smoothedOffsets_.find(param);
            if (offsetIt == smoothedOffsets_.end()) {
                return base;  // No offset calculated yet
            }
            
            // The equation (Y*(mic-X))+Z produces the FINAL value directly
            // NOT an offset to add to base
            return offsetIt->second;
        }
        
        // Check if a parameter has mic reactivity enabled
        bool hasReactivity(const std::string& param) const {
            auto it = settings_.find(param);
            return it != settings_.end() && it->second.enabled;
        }
        
        // Debug print current state
        void debugPrint() const {
            printf("[MicReact] Manager state:\n");
            for (const auto& pair : settings_) {
                if (pair.second.enabled) {
                    float base = 0;
                    auto baseIt = baseValues_.find(pair.first);
                    if (baseIt != baseValues_.end()) base = baseIt->second;
                    
                    float offset = 0;
                    auto offsetIt = smoothedOffsets_.find(pair.first);
                    if (offsetIt != smoothedOffsets_.end()) offset = offsetIt->second;
                    
                    printf("  '%s': base=%.2f offset=%.2f final=%.2f\n",
                           pair.first.c_str(), base, offset, base + offset);
                }
            }
        }
    };
    
    // Global mic reactivity manager instance
    static MicReactivityManager micReactManager;
    
    // Update mic reactivity manager with current mic level
    // Call this once per frame before rendering
    static void updateMicReactivity() {
        auto& syncState = SystemAPI::SYNC_STATE.state();
        float micDb = syncState.micDb;
        micReactManager.update(micDb);
    }
    
    // Get a parameter value with mic reactivity applied
    // Use this when rendering instead of the raw variable
    static float getMicReactiveValue(const std::string& param, float baseValue) {
        return micReactManager.getFinalValue(param, baseValue);
    }
    
    // Initialize mic reactivity for a new scene
    static void initMicReactivityForScene() {
        micReactManager.initialize();
        micReactManager.loadSettingsFromScene();
    }
    
    // Store base value in the manager
    static void storeBaseValue(const char* paramName, float value) {
        micReactManager.setBaseValue(paramName, value);
    }
    
    // Flag to prevent re-saving params while loading from scene
    static bool loadingSceneParams = false;
    
    // Force all shader dirty flags to trigger resync to GPU
    // Call this after loading scene shader params to ensure GPU gets updated
    static void forceShaderSync() {
        shaderDirty = true;
        shaderPaletteDirty = true;
        shaderGradientParamsDirty = true;
        shaderGlitchParamsDirty = true;
        printf("[Shader] Force sync: type=%d, invert=%d, gradient(dist=%d,angle=%d,mirror=%d), glitch(spd=%d,int=%d,chr=%d,qty=%d)\n",
               shaderType, shaderInvert, shaderGradientDistance, shaderGradientAngle, shaderGradientMirror,
               shaderGlitchSpeed, shaderGlitchIntensity, shaderGlitchChromatic, shaderGlitchQuantity);
    }
    
    // ====== COMPLEX TRANSITION ANIMATION ======
    static AnimationSystem::Animations::ComplexTransitionAnim complexAnim;
    static bool complexAnimEnabled = false;  // Disabled - use sandbox instead
    
    // ====== OLED MENU SYSTEM (now in OledMenuSystem.hpp) ======
    // All OLED menu logic moved to SystemAPI/OledMenu/OledMenuSystem.hpp
    
    // ====== HIGH-FREQUENCY IMU TASK ======
    static TaskHandle_t imuTaskHandle = nullptr;
    static bool imuTaskRunning = false;
    
    // ====== DEFAULT VECTOR EYE DRAWING ======
    // Draw the default organic eye shape using GPU line commands
    // SVG source: 445x308, scaled to fit 64x32 panel
    // Scale factors: x = 64/445 = 0.144, y = 32/308 = 0.104
    // Using y scale (0.104) to maintain aspect ratio, then center horizontally
    static void drawDefaultVectorEye(GpuCommands& gpu, float offsetX, float offsetY, bool mirror, float rotation = 0.0f) {
        const float scale = 0.095f;  // Slightly smaller to fit with margin
        const float svgCenterX = 222.5f;  // Approx center of SVG
        const float svgCenterY = 154.0f;  // Approx center of SVG
        
        // Convert rotation to radians
        const float rad = rotation * 3.14159265f / 180.0f;
        const float cosR = cosf(rad);
        const float sinR = sinf(rad);
        
        // Transform a point from SVG coords to panel coords with rotation
        auto transformX = [&](float x, float y) -> int16_t {
            float tx = (x - svgCenterX) * scale;
            float ty = (y - svgCenterY) * scale;
            if (mirror) tx = -tx;
            // Apply rotation around center
            float rx = tx * cosR - ty * sinR;
            return (int16_t)(rx + offsetX);
        };
        auto transformY = [&](float x, float y) -> int16_t {
            float tx = (x - svgCenterX) * scale;
            float ty = (y - svgCenterY) * scale;
            if (mirror) tx = -tx;
            // Apply rotation around center
            float ry = tx * sinR + ty * cosR;
            return (int16_t)(ry + offsetY);
        };
        
        // Draw pupil circle (cx=216, cy=114, r=39.5)
        int16_t pupilX = transformX(216, 114);
        int16_t pupilY = transformY(216, 114);
        int16_t pupilR = (int16_t)(39.5f * scale);
        gpu.hub75Circle(pupilX, pupilY, pupilR, 255, 255, 255);
        
        // SIMPLIFIED eye outline - just 12 key points for smooth performance
        const float eyeOutline[][2] = {
            {238, 3}, {142, 1.5}, {72.5, 10.5}, {35.5, 30.5}, {14, 57.5}, {0.5, 109},
            {5, 126}, {58.5, 144.5}, {117, 177.5}, {170.5, 218}, {230, 226.5}, {292, 198.5},
            {323, 153.5}, {328.5, 106}, {310.5, 51}, {267.5, 14.5}, {238, 3}
        };
        const int numOutlinePoints = sizeof(eyeOutline) / sizeof(eyeOutline[0]);
        
        // Draw eye outline (16 lines instead of 54)
        for (int i = 0; i < numOutlinePoints - 1; i++) {
            gpu.hub75Line(
                transformX(eyeOutline[i][0], eyeOutline[i][1]), transformY(eyeOutline[i][0], eyeOutline[i][1]),
                transformX(eyeOutline[i+1][0], eyeOutline[i+1][1]), transformY(eyeOutline[i+1][0], eyeOutline[i+1][1]),
                255, 255, 255
            );
        }
        
        // SIMPLIFIED tear duct - just 6 key points
        const float tearDuct[][2] = {
            {384.5, 130.5}, {345.5, 99.5}, {332, 171}, {285, 235.5}, {372, 266.5}, {444, 305.5},
            {434, 240}, {384.5, 130.5}
        };
        const int numTearPoints = sizeof(tearDuct) / sizeof(tearDuct[0]);
        
        // Draw tear duct outline (7 lines instead of 35)
        for (int i = 0; i < numTearPoints - 1; i++) {
            gpu.hub75Line(
                transformX(tearDuct[i][0], tearDuct[i][1]), transformY(tearDuct[i][0], tearDuct[i][1]),
                transformX(tearDuct[i+1][0], tearDuct[i+1][1]), transformY(tearDuct[i+1][0], tearDuct[i+1][1]),
                255, 255, 255
            );
        }
    }

    // ====== EYE SPRITE CREATION ======
    // Create a filled circle sprite for AA eye rendering
    static void createCircleSprite(uint8_t* data, int size, uint8_t r, uint8_t g, uint8_t b) {
        float cx = size / 2.0f;
        float cy = size / 2.0f;
        float radius = (size / 2.0f) - 1.0f;
        
        for (int y = 0; y < size; y++) {
            for (int x = 0; x < size; x++) {
                int idx = (y * size + x) * 3;
                float dx = x - cx + 0.5f;
                float dy = y - cy + 0.5f;
                float dist = sqrtf(dx * dx + dy * dy);
                
                if (dist <= radius) {
                    // Inside circle - full color with slight gradient for depth
                    float shade = 1.0f - (dist / radius) * 0.2f;
                    data[idx + 0] = (uint8_t)(r * shade);
                    data[idx + 1] = (uint8_t)(g * shade);
                    data[idx + 2] = (uint8_t)(b * shade);
                } else {
                    // Outside circle - transparent (black for now, GPU handles transparency)
                    data[idx + 0] = 0;
                    data[idx + 1] = 0;
                    data[idx + 2] = 0;
                }
            }
        }
    }
    
    // Upload eye sprites to GPU for AA rendering
    static void uploadEyeSprites() {
        const int EYE_SIZE = 24;  // 24x24 pixel circle sprites
        uint8_t spriteData[EYE_SIZE * EYE_SIZE * 3];
        
        printf("  uploadEyeSprites: Creating %dx%d circle sprites...\n", EYE_SIZE, EYE_SIZE);
        
        // Sprite 0: Left eye (white circle)
        createCircleSprite(spriteData, EYE_SIZE, 255, 255, 255);
        
        // Debug: print first 12 bytes of sprite data
        printf("  Sprite 0 first 12 bytes: ");
        for (int i = 0; i < 12; i++) {
            printf("%02X ", spriteData[i]);
        }
        printf("\n");
        
        bool result0 = g_gpu.uploadSprite(0, spriteData, EYE_SIZE, EYE_SIZE);
        printf("  Eye sprite 0 upload: %s\n", result0 ? "SUCCESS" : "FAILED");
        vTaskDelay(pdMS_TO_TICKS(100));  // Give GPU time to process
        
        // Sprite 1: Right eye (same as left - white circle)
        createCircleSprite(spriteData, EYE_SIZE, 255, 255, 255);
        bool result1 = g_gpu.uploadSprite(1, spriteData, EYE_SIZE, EYE_SIZE);
        printf("  Eye sprite 1 upload: %s\n", result1 ? "SUCCESS" : "FAILED");
        vTaskDelay(pdMS_TO_TICKS(100));
        
        printf("  uploadEyeSprites: Done!\n");
    }
    
    // IMU task: Polls gyro at 100Hz (10ms) for better responsiveness
    static void imuTask(void* param) {
        printf("  GPU: IMU high-frequency task started (100Hz)\n");
        while (imuTaskRunning) {
            // Update IMU at 100Hz
            Drivers::ImuDriver::update();
            vTaskDelay(pdMS_TO_TICKS(10));  // 10ms = 100Hz
        }
        vTaskDelete(nullptr);
    }
    
    bool init() {
        if (initialized) return true;
        
        // Initialize GPU using GpuCommands (required by SceneComposer)
        // GpuCommands::init(port, txPin, rxPin, baud)
        if (!g_gpu.init(UART_NUM_1, GPU_TX_PIN, GPU_RX_PIN, 921600)) {  // Lowered for reliability testing
            printf("  GPU: Init failed\n");
            return false;
        }
        
        // Reset and clear display
        g_gpu.reset();
        vTaskDelay(pdMS_TO_TICKS(200));
        
        // Test GPU communication with blocking stats request
        GpuCommands::GpuStatsResponse testStats;
        if (g_gpu.requestStats(testStats, 500)) {
            printf("  GPU: Stats test PASSED - FPS=%.1f heap=%lu uptime=%lums\n",
                   testStats.fps, (unsigned long)testStats.freeHeap, 
                   (unsigned long)testStats.uptimeMs);
        } else {
            printf("  GPU: Stats test FAILED - RX line may not be connected\n");
        }
        
        // Flush UART RX buffer after blocking test to ensure clean state for async
        g_gpu.flushRxBuffer();
        
        // Clear HUB75 display on init
        g_gpu.hub75Clear(0, 0, 0);
        g_gpu.hub75Present();
        
        initialized = true;
        connected = true;  // Assume connected after successful init
        lastPingTime = 0;
        printf("  GPU: Initialized via GpuCommands (TX:%d, RX:%d @ 921600)\n", GPU_TX_PIN, GPU_RX_PIN);
        
        // ====== SCENE COMPOSER INITIALIZATION ======
        if (g_sceneComposerEnabled) {
            printf("  SceneComposer: Initializing...\n");
            g_sceneComposer.init(&g_gpu);
            
            // Create default HUB75 scene with gyro eyes
            g_activeHubScene = g_sceneComposer.createScene(SceneAPI::DisplayTarget::HUB75, "MainHUB75");
            if (g_activeHubScene) {
                g_activeHubScene->backgroundColor = SceneAPI::Color(0, 0, 0);
                g_activeHubScene->displayWidth = 128;
                g_activeHubScene->displayHeight = 32;
                g_sceneComposer.setActiveScene(SceneAPI::DisplayTarget::HUB75, g_activeHubScene);
                printf("  SceneComposer: HUB75 scene created\n");
            }
            
            // Create default OLED scene
            g_activeOledScene = g_sceneComposer.createScene(SceneAPI::DisplayTarget::OLED, "MainOLED");
            if (g_activeOledScene) {
                g_activeOledScene->displayWidth = 128;
                g_activeOledScene->displayHeight = 128;
                g_sceneComposer.setActiveScene(SceneAPI::DisplayTarget::OLED, g_activeOledScene);
                printf("  SceneComposer: OLED scene created\n");
            }
            
            printf("  SceneComposer: Ready\n");
        }
        
        // Start high-frequency IMU task (100Hz for better gyro responsiveness)
        imuTaskRunning = true;
        xTaskCreatePinnedToCore(
            imuTask,
            "IMU_Task",
            2048,  // Stack size
            nullptr,
            5,     // Priority (higher than main loop)
            &imuTaskHandle,
            0      // Core 0 (same as main)
        );
        
        // Initialize OLED Menu System
        OledMenu::OledMenuSystem::instance().init(&g_gpu);
        printf("  OLED Menu: Initialized\n");
        
        return true;
    }
    
    // Create and upload test sprite on startup (same as SimpleSpriteTest)
    void uploadTestSprite() {
        if (!initialized) return;
        
        printf("\n  ====== UPLOADING STARTUP TEST SPRITE ======\n");
        
        // Create 16x16 test sprite with X pattern (same as SimpleSpriteTest)
        constexpr int SPRITE_W = 16;
        constexpr int SPRITE_H = 16;
        std::vector<uint8_t> spriteData(SPRITE_W * SPRITE_H * 3);  // RGB888
        
        for (int y = 0; y < SPRITE_H; y++) {
            for (int x = 0; x < SPRITE_W; x++) {
                int idx = (y * SPRITE_W + x) * 3;
                
                // Draw X pattern with green border and blue fill
                bool isEdge = (x == 0 || x == SPRITE_W-1 || y == 0 || y == SPRITE_H-1);
                bool isDiag1 = (x == y);
                bool isDiag2 = (x == SPRITE_W - 1 - y);
                
                if (isEdge) {
                    // Green border
                    spriteData[idx] = 0;
                    spriteData[idx+1] = 255;
                    spriteData[idx+2] = 0;
                } else if (isDiag1 || isDiag2) {
                    // Red X pattern
                    spriteData[idx] = 255;
                    spriteData[idx+1] = 0;
                    spriteData[idx+2] = 0;
                } else {
                    // Blue fill
                    spriteData[idx] = 0;
                    spriteData[idx+1] = 0;
                    spriteData[idx+2] = 128;
                }
            }
        }
        
        // Upload to GPU slot 0
        uint8_t spriteId = 0;
        g_gpu.deleteSprite(spriteId);
        vTaskDelay(pdMS_TO_TICKS(50));
        
        if (g_gpu.uploadSprite(spriteId, spriteData.data(), SPRITE_W, SPRITE_H)) {
            printf("  Test sprite uploaded to GPU slot %d\n", spriteId);
            vTaskDelay(pdMS_TO_TICKS(200));  // Wait for GPU to process
            
            // Enable continuous rotation rendering
            spriteReady = true;
            activeSpriteId = spriteId;
            spriteX = 64.0f;
            spriteY = 16.0f;
            spriteAngle = 0.0f;
            bgR = 5; bgG = 5; bgB = 15;  // Dark blue background
            autoRotate = true;  // TEST sprite rotates
            
            printf("  Continuous rotation rendering ENABLED\n");
            printf("  ====== TEST SPRITE READY ======\n\n");
        } else {
            printf("  ERROR: Failed to upload test sprite!\n");
        }
    }
    
    // Increment rotation angle each frame (for test sprite)
    void incrementAngle() {
        if (spriteReady) {
            spriteAngle += 1.0f;
            if (spriteAngle >= 360.0f) spriteAngle -= 360.0f;
        }
    }
    
    // Non-blocking update - periodically ping GPU
    // Track render loop for debugging
    static uint32_t renderFrameCount = 0;
    static uint32_t lastRenderDebugTime = 0;
    
    void update(uint32_t currentTimeMs) {
        if (!initialized) return;
        
        // Check for GPU responses multiple times per update (non-blocking)
        // This ensures we don't miss responses due to UART RX buffer overflow
        g_gpu.checkForResponses();
        
        // Update cached uptime from async responses
        if (g_gpu.hasValidUptime()) {
            gpuUptimeMs = g_gpu.getCachedUptimeMs();
        }
        
        // Send periodic stats request (non-blocking) - more comprehensive than ping
        if (currentTimeMs - lastPingTime >= PING_INTERVAL_MS) {
            lastPingTime = currentTimeMs;
            
            // Request full stats (includes uptime, FPS, memory, etc.)
            g_gpu.requestStatsAsync();
            
            // Give GPU more time to respond and then check for the response
            // GPU needs to process command + format response + transmit
            vTaskDelay(pdMS_TO_TICKS(20));
            g_gpu.checkForResponses();
            
            // Check again after more delay in case response was delayed
            vTaskDelay(pdMS_TO_TICKS(10));
            g_gpu.checkForResponses();
        }
        
        // ====== CONTINUOUS RENDERING AT ~30fps ======
        // ALWAYS render regardless of ping status - GPU processes frames even if PONG is delayed
        if (currentTimeMs - lastRenderTime >= RENDER_INTERVAL_MS) {
            lastRenderTime = currentTimeMs;
            renderFrameCount++;
            
            // No setTarget needed - GpuCommands uses hub75* methods directly
            
            // Check if complex transition animation is enabled (new default)
            if (complexAnimEnabled) {
                // Get accelerometer data DIRECTLY from IMU driver for lowest latency
                float ax = (float)Drivers::ImuDriver::accelX / 1000.0f;
                float ay = (float)Drivers::ImuDriver::accelY / 1000.0f;
                float az = (float)Drivers::ImuDriver::accelZ / 1000.0f;
                
                // Update and render complex transition animation
                complexAnim.update(RENDER_INTERVAL_MS, ax, ay, az);
                
                // Set up GPU callbacks (GpuCommands uses hub75* methods)
                auto clear = [](uint8_t r, uint8_t g, uint8_t b) {
                    g_gpu.hub75Clear(r, g, b);
                };
                auto fillRect = [](int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b) {
                    if (w > 0 && h > 0) {
                        g_gpu.hub75Fill(x, y, w, h, r, g, b);
                    }
                };
                auto drawPixel = [](int x, int y, uint8_t r, uint8_t g, uint8_t b) {
                    g_gpu.hub75Pixel(x, y, r, g, b);
                };
                auto present = []() {
                    g_gpu.hub75Present();
                    vTaskDelay(pdMS_TO_TICKS(1));
                };
                
                complexAnim.render(fillRect, drawPixel, clear, present);
                
                // Debug every 60 frames
                if (renderFrameCount % 60 == 0) {
                    printf("COMPLEX_ANIM: Frame %lu - stage=%d time=%.2f accel=(%.2f,%.2f,%.2f)\n",
                           renderFrameCount, (int)complexAnim.currentStage, complexAnim.stageTime, ax, ay, az);
                }
            }
            // ====== SCENE-BASED ANIMATION RENDERING ======
            else if (currentAnimMode != SceneAnimMode::NONE) {
                // Mic reactivity is now applied per-render-case using getMicReactiveValue()
                // This keeps base values clean and calculates final values on-demand
                
                // Sync shader palette to GPU if changed (must be done before shader config)
                if (shaderPaletteDirty && (shaderType == 2 || shaderType == 3)) {  // 2 = HUE_CYCLE, 3 = GRADIENT_CYCLE
                    g_gpu.setShaderPalette(shaderHuePalette, shaderHueCycleColorCount);
                    shaderPaletteDirty = false;
                    printf("  SHADER PALETTE: Sent %d colors to GPU\n", shaderHueCycleColorCount);
                }
                
                // Sync gradient params to GPU if changed (before shader config)
                if (shaderGradientParamsDirty && shaderType == 3) {  // 3 = GRADIENT_CYCLE
                    g_gpu.setGradientParams(shaderGradientDistance, shaderGradientAngle, shaderGradientMirror);
                    shaderGradientParamsDirty = false;
                    printf("  GRADIENT PARAMS: distance=%d, angle=%d, mirror=%d\n", shaderGradientDistance, shaderGradientAngle, shaderGradientMirror);
                }
                
                // Sync glitch params to GPU if changed (before shader config)
                if (shaderGlitchParamsDirty && shaderType == 4) {  // 4 = GLITCH
                    g_gpu.setGlitchParams(shaderGlitchSpeed, shaderGlitchIntensity, shaderGlitchChromatic, shaderGlitchQuantity);
                    shaderGlitchParamsDirty = false;
                    printf("  GLITCH PARAMS: speed=%d, intensity=%d, chromatic=%d, quantity=%d\n", shaderGlitchSpeed, shaderGlitchIntensity, shaderGlitchChromatic, shaderGlitchQuantity);
                }
                
                // Sync shader config to GPU if changed
                if (shaderDirty) {
                    if (shaderType == 2 || shaderType == 3) {  // HUE_CYCLE or GRADIENT_CYCLE
                        // For HUE_CYCLE/GRADIENT_CYCLE: param1,param2 = speed (16-bit), param3 unused
                        g_gpu.setSpriteShader(
                            static_cast<GpuCommands::ShaderType>(shaderType),
                            shaderInvert,
                            shaderMaskR, shaderMaskG, shaderMaskB,
                            shaderMaskEnabled,
                            (uint8_t)(shaderHueCycleSpeed & 0xFF),   // speed low byte
                            (uint8_t)(shaderHueCycleSpeed >> 8),     // speed high byte
                            0  // unused
                        );
                        printf("  SHADER: Synced type=%d to GPU (speed=%d, invert=%d, mask=%d)\n",
                               shaderType, shaderHueCycleSpeed, shaderInvert, shaderMaskEnabled);
                    } else if (shaderType == 4) {  // GLITCH
                        // For GLITCH: params sent separately via setGlitchParams
                        g_gpu.setSpriteShader(
                            static_cast<GpuCommands::ShaderType>(shaderType),
                            shaderInvert,
                            shaderMaskR, shaderMaskG, shaderMaskB,
                            shaderMaskEnabled,
                            0, 0, 0  // params handled by SET_GLITCH_PARAMS
                        );
                        // Also force glitch params sync
                        shaderGlitchParamsDirty = true;
                        printf("  SHADER: Synced GLITCH type=%d to GPU (invert=%d, mask=%d)\n",
                               shaderType, shaderInvert, shaderMaskEnabled);
                    } else {
                        // For NONE and COLOR_OVERRIDE
                        g_gpu.setSpriteShader(
                            static_cast<GpuCommands::ShaderType>(shaderType),
                            shaderInvert,
                            shaderMaskR, shaderMaskG, shaderMaskB,
                            shaderMaskEnabled,
                            shaderOverrideR, shaderOverrideG, shaderOverrideB
                        );
                        printf("  SHADER: Synced to GPU (type=%d, invert=%d, mask=%d, maskRGB=(%d,%d,%d), overrideRGB=(%d,%d,%d))\n",
                               shaderType, shaderInvert, shaderMaskEnabled,
                               shaderMaskR, shaderMaskG, shaderMaskB,
                               shaderOverrideR, shaderOverrideG, shaderOverrideB);
                    }
                    shaderDirty = false;
                }
                
                // Get gyro data DIRECTLY from IMU driver for lowest latency
                // (Used for debug prints - REACTIVE_EYES uses SYNC_STATE calibrated data)
                (void)Drivers::ImuDriver::gyroX;
                (void)Drivers::ImuDriver::gyroY;
                
                switch (currentAnimMode) {
                    case SceneAnimMode::GYRO_EYES:
                        // DEPRECATED: Gyro eyes removed - fall through to STATIC_IMAGE
                        // This case should not happen due to migration, but handle gracefully
                    
                    case SceneAnimMode::STATIC_IMAGE: {
                        // Static sprite display - uses mirrorEnabled flag to determine rendering
                        g_gpu.hub75Clear(bgR, bgG, bgB);
                        
                        // Debug: log mirror state every 120 frames
                        if (renderFrameCount % 120 == 0) {
                            printf("STATIC_IMAGE: mirrorEnabled=%s\n", mirrorEnabled ? "YES" : "NO");
                        }
                        
                        if (mirrorEnabled) {
                            // Mirrored mode: Draw on both panels with independent left/right controls
                            if (spriteReady) {
                                g_gpu.blitSpriteRotatedScaledFlip(activeSpriteId, leftPosX, leftPosY, leftRotation, leftScale, leftFlipX);
                                g_gpu.blitSpriteRotatedScaledFlip(activeSpriteId, rightPosX, rightPosY, rightRotation, rightScale, rightFlipX);
                            } else {
                                drawDefaultVectorEye(g_gpu, leftPosX, leftPosY, false, leftRotation);
                                drawDefaultVectorEye(g_gpu, rightPosX, rightPosY, true, rightRotation);
                            }
                            
                            if (renderFrameCount % 120 == 0) {
                                printf("STATIC (mirrored): Frame %lu - L(%.0f,%.0f,%.0f,%.2f,flip=%d) R(%.0f,%.0f,%.0f,%.2f,flip=%d)\n", 
                                       renderFrameCount, leftPosX, leftPosY, leftRotation, leftScale, leftFlipX,
                                       rightPosX, rightPosY, rightRotation, rightScale, rightFlipX);
                            }
                        } else {
                            // Single sprite mode: One sprite at specified position
                            if (spriteReady) {
                                g_gpu.blitSpriteRotatedScaledFlip(activeSpriteId, staticPosX, staticPosY, staticRotation, staticScale, staticFlipX);
                            } else {
                                drawDefaultVectorEye(g_gpu, staticPosX, staticPosY, false, staticRotation);
                            }
                            
                            if (renderFrameCount % 120 == 0) {
                                printf("STATIC (single): Frame %lu - pos=(%.0f,%.0f) rot=%.0f scale=%.2f flip=%d\n", 
                                       renderFrameCount, staticPosX, staticPosY, staticRotation, staticScale, staticFlipX);
                            }
                        }
                        
                        g_gpu.hub75Present();
                        break;
                    }
                    
                    case SceneAnimMode::STATIC_MIRRORED: {
                        // Legacy: Static sprite on both panels - now just set mirrorEnabled and use STATIC_IMAGE logic
                        mirrorEnabled = true;
                        g_gpu.hub75Clear(bgR, bgG, bgB);
                        
                        // Update mic reactivity each frame (calculates smoothed offsets)
                        updateMicReactivity();
                        
                        // Get final values with mic reactivity applied (base + offset)
                        // These do NOT modify the base variables
                        float finalLeftX = getMicReactiveValue("left_x", leftPosX);
                        float finalLeftY = getMicReactiveValue("left_y", leftPosY);
                        float finalLeftRot = getMicReactiveValue("left_rotation", leftRotation);
                        float finalLeftScale = getMicReactiveValue("left_scale", leftScale);
                        float finalRightX = getMicReactiveValue("right_x", rightPosX);
                        float finalRightY = getMicReactiveValue("right_y", rightPosY);
                        float finalRightRot = getMicReactiveValue("right_rotation", rightRotation);
                        float finalRightScale = getMicReactiveValue("right_scale", rightScale);
                        
                        // DEBUG: Print mic reactivity state every 60 frames
                        if (renderFrameCount % 60 == 0) {
                            printf("[MicDebug] Base: leftScale=%.3f rightScale=%.3f | Final: leftScale=%.3f rightScale=%.3f\n",
                                   leftScale, rightScale, finalLeftScale, finalRightScale);
                            micReactManager.debugPrint();
                        }
                        
                        if (spriteReady) {
                            // Draw on left panel with mic-reactive params
                            g_gpu.blitSpriteRotatedScaledFlip(activeSpriteId, finalLeftX, finalLeftY, finalLeftRot, finalLeftScale, leftFlipX);
                            // Draw on right panel with mic-reactive params
                            g_gpu.blitSpriteRotatedScaledFlip(activeSpriteId, finalRightX, finalRightY, finalRightRot, finalRightScale, rightFlipX);
                        } else {
                            // No sprite uploaded - use default vector eye drawing
                            drawDefaultVectorEye(g_gpu, finalLeftX, finalLeftY, false, finalLeftRot);
                            drawDefaultVectorEye(g_gpu, finalRightX, finalRightY, true, finalRightRot);
                        }
                        
                        g_gpu.hub75Present();
                        
                        if (renderFrameCount % 120 == 0) {
                            printf("STATIC_MIRRORED: Frame %lu - L(%.0f,%.0f,%.0f,%.2f,flip=%d) R(%.0f,%.0f,%.0f,%.2f,flip=%d)\n", 
                                   renderFrameCount, finalLeftX, finalLeftY, finalLeftRot, finalLeftScale, leftFlipX,
                                   finalRightX, finalRightY, finalRightRot, finalRightScale, rightFlipX);
                        }
                        break;
                    }
                    
                    case SceneAnimMode::SWAY:
                        // DEPRECATED: Sway animation removed - fall through to STATIC_IMAGE behavior
                        {
                            g_gpu.hub75Clear(bgR, bgG, bgB);
                            if (spriteReady) {
                                g_gpu.blitSpriteRotated(activeSpriteId, staticPosX, staticPosY, staticRotation);
                            } else {
                                drawDefaultVectorEye(g_gpu, 64.0f, 16.0f, false, staticRotation);
                            }
                            g_gpu.hub75Present();
                        }
                        break;
                    
                    case SceneAnimMode::SDF_MORPH:
                        // DEPRECATED: SDF_MORPH removed - fall through to static
                        {
                            g_gpu.hub75Clear(bgR, bgG, bgB);
                            if (spriteReady) {
                                g_gpu.blitSpriteRotated(activeSpriteId, staticPosX, staticPosY, staticRotation);
                            } else {
                                drawDefaultVectorEye(g_gpu, 64.0f, 16.0f, false, staticRotation);
                            }
                            g_gpu.hub75Present();
                        }
                        break;
                    
                    case SceneAnimMode::REACTIVE_EYES: {
                        // IMU-reactive eye animation with asymmetric left/right movement
                        // Use CALIBRATED device-frame GYRO data (after IMU calibration applied)
                        auto& syncState = SystemAPI::SYNC_STATE.state();
                        float gx = syncState.deviceGyroX / 1000.0f;  // mdps to dps (calibrated gyro)
                        float gy = syncState.deviceGyroY / 1000.0f;  // mdps to dps (calibrated gyro)
                        float gz = syncState.deviceGyroZ / 1000.0f;  // mdps to dps (calibrated gyro)
                        
                        // Base positions: center of each display half
                        // Left display: X 0-63, center at (32, 16)
                        // Right display: X 64-127, center at (96, 16)
                        
                        // Movement rules (using calibrated device-frame GYRO values):
                        // +Y: Right sprite moves UP, Left sprite moves DOWN
                        // +Z: Right sprite moves FORWARD (right), Left sprite moves BACKWARD (left)
                        // +X: Both sprites move DOWN + rotation (Right=CW, Left=CCW)
                        
                        // Left sprite target (center of left display = 32, 16)
                        float leftTargetX = reactiveBaseLeftX - gz * reactiveZSensitivity;  // +Z = left moves backward (left)
                        float leftTargetY = reactiveBaseLeftY + gy * reactiveYSensitivity   // +Y = left moves DOWN (positive Y screen = down)
                                           + gx * reactiveXSensitivity;                      // +X = both move down
                        float leftTargetRot = reactiveLeftRotOffset + gx * reactiveRotSensitivity;  // Base offset + gyro rotation
                        
                        // Right sprite target (center of right display = 96, 16)
                        float rightTargetX = reactiveBaseRightX + gz * reactiveZSensitivity; // +Z = right moves forward (right)
                        float rightTargetY = reactiveBaseRightY - gy * reactiveYSensitivity  // +Y = right moves UP (negative Y screen = up)
                                            + gx * reactiveXSensitivity;                      // +X = both move down
                        // Right rotation is MIRRORED: when left rotates CW, right rotates CCW
                        // This gives a natural "both eyes looking the same direction" effect
                        float rightTargetRot = (180.0f + reactiveRightRotOffset) - gx * reactiveRotSensitivity;
                        
                        // Apply smoothing (exponential moving average)
                        reactiveLPosXSmooth += (leftTargetX - reactiveLPosXSmooth) * reactiveSmoothing;
                        reactiveLPosYSmooth += (leftTargetY - reactiveLPosYSmooth) * reactiveSmoothing;
                        reactiveLRotSmooth += (leftTargetRot - reactiveLRotSmooth) * reactiveSmoothing;
                        reactiveRPosXSmooth += (rightTargetX - reactiveRPosXSmooth) * reactiveSmoothing;
                        reactiveRPosYSmooth += (rightTargetY - reactiveRPosYSmooth) * reactiveSmoothing;
                        reactiveRRotSmooth += (rightTargetRot - reactiveRRotSmooth) * reactiveSmoothing;
                        
                        // Clamp positions to reasonable bounds
                        float lx = fmaxf(0.0f, fminf(63.0f, reactiveLPosXSmooth));
                        float ly = fmaxf(0.0f, fminf(31.0f, reactiveLPosYSmooth));
                        float rx = fmaxf(64.0f, fminf(127.0f, reactiveRPosXSmooth));
                        float ry = fmaxf(0.0f, fminf(31.0f, reactiveRPosYSmooth));
                        
                        // Update mic reactivity and get final scale value
                        updateMicReactivity();
                        float finalScale = getMicReactiveValue("reactive_scale", reactiveScale);
                        
                        // DEBUG: Print mic reactivity state every 60 frames
                        if (renderFrameCount % 60 == 0) {
                            printf("[MicDebug] reactive_scale: base=%.3f equation=%.3f (used when mic enabled)\n", reactiveScale, finalScale);
                            micReactManager.debugPrint();
                        }
                        
                        // Render
                        g_gpu.hub75Clear(bgR, bgG, bgB);
                        
                        if (spriteReady) {
                            // Draw left and right sprites with reactive positions, rotations, and flips
                            g_gpu.blitSpriteRotatedScaledFlip(activeSpriteId, lx, ly, reactiveLRotSmooth, finalScale, reactiveLeftFlipX);
                            g_gpu.blitSpriteRotatedScaledFlip(activeSpriteId, rx, ry, reactiveRRotSmooth, finalScale, reactiveRightFlipX);
                        } else {
                            // Fallback vector eyes
                            drawDefaultVectorEye(g_gpu, lx, ly, false, reactiveLRotSmooth);
                            drawDefaultVectorEye(g_gpu, rx, ry, true, reactiveRRotSmooth);
                        }
                        
                        g_gpu.hub75Present();
                        
                        // Debug logging every 2 seconds
                        if (renderFrameCount % 120 == 0) {
                            printf("REACTIVE_EYES: GYRO(%.2f,%.2f,%.2f) L(%.1f,%.1f,%.1f) R(%.1f,%.1f,%.1f)\n",
                                   gx, gy, gz, lx, ly, reactiveLRotSmooth, rx, ry, reactiveRRotSmooth);
                        }
                        break;
                    }
                    
                    case SceneAnimMode::NONE:
                        // No animation - just clear screen
                        g_gpu.hub75Clear(0, 0, 0);
                        g_gpu.hub75Present();
                        break;
                    
                    default:
                        break;
                }
            }
            else if (spriteReady) {
                // Sprite rendering mode (like WifiSpriteUploadTest)
                if (autoRotate) {
                    incrementAngle();
                }
                
                g_gpu.hub75Clear(bgR, bgG, bgB);
                g_gpu.blitSpriteRotated(activeSpriteId, spriteX, spriteY, spriteAngle);
                g_gpu.hub75Present();
                
                if (renderFrameCount % 30 == 0) {
                    printf("DEBUG RENDER: Frame %lu - sprite=%u pos=(%.1f,%.1f) angle=%.1f\n",
                           renderFrameCount, activeSpriteId, spriteX, spriteY, spriteAngle);
                }
            }
            else {
                // Fallback: ALWAYS render something to keep GPU active
                // This ensures continuous frame output even when no animation is explicitly set
                g_gpu.hub75Clear(bgR, bgG, bgB);
                g_gpu.hub75Present();
                
                if (renderFrameCount % 120 == 0) {
                    printf("RENDER FALLBACK: Frame %lu - no active animation, clearing\n", renderFrameCount);
                }
            }
        }
        
        // Debug: Print render state every 5 seconds
        if (currentTimeMs - lastRenderDebugTime >= 5000) {
            lastRenderDebugTime = currentTimeMs;
            printf("DEBUG STATE: sandbox=%d spriteReady=%d connected=%d frames=%lu\n",
                   sandboxEnabled, spriteReady, connected, renderFrameCount);
        }
        
        // ====== OLED MENU SYSTEM (modular) ======
        // Feed sensor data to the OLED menu system
        {
            // IMU Data
            OledMenu::ImuData imu;
            imu.accelX = (float)Drivers::ImuDriver::accelX / 1000.0f;  // Convert mg to g
            imu.accelY = (float)Drivers::ImuDriver::accelY / 1000.0f;
            imu.accelZ = (float)Drivers::ImuDriver::accelZ / 1000.0f;
            imu.gyroX = (float)Drivers::ImuDriver::gyroX;  // Already in dps
            imu.gyroY = (float)Drivers::ImuDriver::gyroY;
            imu.gyroZ = (float)Drivers::ImuDriver::gyroZ;
            imu.magX = 0.0f;  // TODO: Add magnetometer if available
            imu.magY = 0.0f;
            imu.magZ = 0.0f;
            // Calculate pitch/roll from accelerometer
            imu.pitch = atan2f((float)Drivers::ImuDriver::accelX, 
                               sqrtf((float)(Drivers::ImuDriver::accelY * Drivers::ImuDriver::accelY + 
                                            Drivers::ImuDriver::accelZ * Drivers::ImuDriver::accelZ))) * 180.0f / 3.14159f;
            imu.roll = atan2f((float)Drivers::ImuDriver::accelY, 
                              (float)Drivers::ImuDriver::accelZ) * 180.0f / 3.14159f;
            imu.yaw = 0.0f;  // Need magnetometer for yaw
            imu.temperature = 0.0f;
            imu.connected = Drivers::ImuDriver::isInitialized();
            OledMenu::OledMenuSystem::instance().setImuData(imu);
            
            // GPS Data
            OledMenu::GpsData gps;
            gps.latitude = Drivers::GpsDriver::latitude;
            gps.longitude = Drivers::GpsDriver::longitude;
            gps.altitude = Drivers::GpsDriver::altitude;
            gps.speed = Drivers::GpsDriver::speed;
            gps.course = Drivers::GpsDriver::heading;
            gps.satellites = Drivers::GpsDriver::satellites;
            gps.hour = Drivers::GpsDriver::hour;
            gps.minute = Drivers::GpsDriver::minute;
            gps.second = Drivers::GpsDriver::second;
            gps.day = Drivers::GpsDriver::day;
            gps.month = Drivers::GpsDriver::month;
            gps.year = Drivers::GpsDriver::year;
            gps.hasFix = Drivers::GpsDriver::valid;
            gps.connected = (Drivers::GpsDriver::bytesReceived > 0);  // Connected if receiving data
            OledMenu::OledMenuSystem::instance().setGpsData(gps);
            
            // Mic Data
            OledMenu::MicData mic;
            mic.dbLevel = Drivers::MicDriver::currentDb;  // Instantaneous level
            mic.peakDb = Drivers::MicDriver::currentDb;    // Use current as peak (no peak tracking yet)
            mic.avgDb = Drivers::MicDriver::avgDb;
            mic.connected = Drivers::MicDriver::isInitialized();
            OledMenu::OledMenuSystem::instance().setMicData(mic);
            
            // BME Data - from modular BmeDriver
            Drivers::BmeDriver::update();  // Update sensor readings
            OledMenu::BmeData bme;
            bme.temperature = Drivers::BmeDriver::getTemperature();
            bme.humidity = Drivers::BmeDriver::getHumidity();
            bme.pressure = Drivers::BmeDriver::getPressure();
            bme.altitude = Drivers::BmeDriver::getAltitude();
            bme.connected = Drivers::BmeDriver::connected;
            OledMenu::OledMenuSystem::instance().setBmeData(bme);
            
            // Calibration Data
            OledMenu::CalibrationData calib;
            calib.imuCalibrated = Drivers::ImuDriver::isInitialized();
            calib.bmeCalibrated = Drivers::BmeDriver::isInitialized();
            calib.displayCalibrated = connected;
            calib.imuAccuracy = Drivers::ImuDriver::isInitialized() ? 3 : 0;
            OledMenu::OledMenuSystem::instance().setCalibrationData(calib);
        }
        
        // Update the modular OLED menu system
        OledMenu::OledMenuSystem::instance().update(currentTimeMs);
        
        // Note: GpuDriver from SystemAPI doesn't have stats/alerts like GpuCommands
        // Those would need to be added separately if needed
    }
    
    // Set sprite scene for continuous rendering (web uploads use this - no auto-rotate)
    void setSpriteScene(uint8_t spriteId, float x, float y, float angle, uint8_t r, uint8_t g, uint8_t b) {
        activeSpriteId = spriteId;
        spriteX = x;
        spriteY = y;
        spriteAngle = angle;
        bgR = r;
        bgG = g;
        bgB = b;
        spriteReady = true;
        autoRotate = false;  // Web uploads don't auto-rotate
        printf("DEBUG: Sprite scene set (static) - ID=%u pos=(%.1f,%.1f) angle=%.1f bg=(%u,%u,%u)\n",
               spriteId, x, y, angle, r, g, b);
    }
    
    // Clear sprite scene (stop rendering)
    void clearSpriteScene() {
        spriteReady = false;
        printf("DEBUG: Sprite scene cleared\n");
    }
    
    // ====== SANDBOX MODE ======
    // When enabled, sandbox takes over the render loop
    void enableSandbox(bool enable) {
        sandboxEnabled = enable;
        if (enable) {
            spriteReady = false;  // Disable sprite rendering when sandbox is on
        }
    }
    
    bool isSandboxEnabled() { return sandboxEnabled; }
    bool isSpriteReady() { return spriteReady; }
    bool isConnected() { return connected; }
    
    // ====== SCENE-BASED ANIMATION ======
    void setSceneAnimation(const std::string& animType) {
        printf("  SCENE: Setting animation type to '%s'\n", animType.c_str());
        
        if (animType == "static_mirrored") {
            currentAnimMode = SceneAnimMode::STATIC_MIRRORED;
            mirrorEnabled = true;
        } else if (animType == "static_sprite" || animType == "static_image" || animType == "static") {
            currentAnimMode = SceneAnimMode::STATIC_IMAGE;
            mirrorEnabled = false;  // Single sprite mode
        } else if (animType == "reactive_eyes") {
            currentAnimMode = SceneAnimMode::REACTIVE_EYES;
            mirrorEnabled = true;  // Reactive eyes always uses left/right sprites
            
            // Reset smoothed positions to center of each panel
            // Left panel: X 0-63, center at (32, 16)
            // Right panel: X 64-127, center at (96, 16)
            reactiveLPosXSmooth = reactiveBaseLeftX;   // 32.0 = center of left panel
            reactiveLPosYSmooth = reactiveBaseLeftY;   // 16.0 = center Y
            reactiveLRotSmooth = reactiveLeftRotOffset;  // Use offset as initial rotation
            reactiveRPosXSmooth = reactiveBaseRightX;  // 96.0 = center of right panel
            reactiveRPosYSmooth = reactiveBaseRightY;  // 16.0 = center Y
            reactiveRRotSmooth = 180.0f + reactiveRightRotOffset;  // 180 base + offset
            printf("  REACTIVE_EYES: Reset to center positions L(%.0f,%.0f) R(%.0f,%.0f)\n",
                   reactiveBaseLeftX, reactiveBaseLeftY, reactiveBaseRightX, reactiveBaseRightY);
        } else {
            // Default fallback
            currentAnimMode = SceneAnimMode::STATIC_IMAGE;
            mirrorEnabled = false;
        }
        
        // Reset animation state
        sceneAnimInitialized = false;
        
        printf("  SCENE: Animation mode set to %d, mirror=%s\n", (int)currentAnimMode, mirrorEnabled ? "YES" : "NO");
    }
    
    SceneAnimMode getSceneAnimMode() { return currentAnimMode; }
    
    void setGyroEyeParams(float size, float sensitivity, bool mirror, int spriteId) {
        eyeSize = size;
        eyeSensitivity = sensitivity;
        eyeMirror = mirror;
        eyeSpriteId = spriteId;
    }
    
    void setSwayParams(float xInt, float yInt, float rotRange, float speed, bool cosX) {
        swayXIntensity = xInt;
        swayYIntensity = yInt;
        swayRotRange = rotRange;
        swaySpeed = speed;
        swayCosX = cosX;
    }
    
    void setStaticParams(float scale, float rotation, float posX, float posY) {
        staticScale = scale;
        staticRotation = rotation;
        staticPosX = posX;
        staticPosY = posY;
        
        // Store as base values for mic reactivity
        storeBaseValue("static_scale", scale);
        storeBaseValue("static_rotation", rotation);
        storeBaseValue("static_pos_x", posX);
        storeBaseValue("static_pos_y", posY);
    }
    
    // Set flip state for single sprite mode
    void setStaticFlip(bool flipX) {
        staticFlipX = flipX;
        printf("[setStaticFlip] staticFlipX = %s\n", flipX ? "true" : "false");
    }
    
    // Set flip states for mirrored mode (left and right sprites)
    void setMirroredFlips(bool leftFlip, bool rightFlip) {
        leftFlipX = leftFlip;
        rightFlipX = rightFlip;
        printf("[setMirroredFlips] leftFlipX = %s, rightFlipX = %s\n", 
               leftFlip ? "true" : "false", rightFlip ? "true" : "false");
    }
    
    void setMirroredParams(float lx, float ly, float lrot, float lscale,
                           float rx, float ry, float rrot, float rscale) {
        leftPosX = lx;
        leftPosY = ly;
        leftRotation = lrot;
        leftScale = lscale;
        rightPosX = rx;
        rightPosY = ry;
        rightRotation = rrot;
        rightScale = rscale;
        
        // Store as base values for mic reactivity (use consistent names)
        storeBaseValue("left_x", lx);
        storeBaseValue("left_y", ly);
        storeBaseValue("left_rotation", lrot);
        storeBaseValue("left_scale", lscale);
        storeBaseValue("right_x", rx);
        storeBaseValue("right_y", ry);
        storeBaseValue("right_rotation", rrot);
        storeBaseValue("right_scale", rscale);
        printf("[setMirroredParams] Stored base values: L(%.1f,%.1f) R(%.1f,%.1f)\n", lx, ly, rx, ry);
    }
    
    // Reset reactive eyes smoothed values (call after loading params)
    void resetReactiveEyesSmooth() {
        reactiveLPosXSmooth = reactiveBaseLeftX;
        reactiveLPosYSmooth = reactiveBaseLeftY;
        reactiveLRotSmooth = reactiveLeftRotOffset;
        reactiveRPosXSmooth = reactiveBaseRightX;
        reactiveRPosYSmooth = reactiveBaseRightY;
        reactiveRRotSmooth = 180.0f + reactiveRightRotOffset;
        printf("[resetReactiveEyesSmooth] L(%.0f,%.0f,rot=%.0f) R(%.0f,%.0f,rot=%.0f)\n",
               reactiveLPosXSmooth, reactiveLPosYSmooth, reactiveLRotSmooth,
               reactiveRPosXSmooth, reactiveRPosYSmooth, reactiveRRotSmooth);
    }
    
    // Update a single parameter by name - for live slider updates
    void setSingleParam(const char* paramName, float value) {
        printf("[setSingleParam] '%s' = %.2f (currentAnimMode=%d, mirrorEnabled=%d)\n", paramName, value, (int)currentAnimMode, mirrorEnabled);
        
        // Mirror toggle - special case (boolean as 0/1)
        if (strcmp(paramName, "mirror") == 0) {
            mirrorEnabled = (value > 0.5f);
            printf("  -> mirrorEnabled = %s\n", mirrorEnabled ? "true" : "false");
        }
        // Left panel params
        else if (strcmp(paramName, "left_x") == 0) { leftPosX = value; printf("  -> leftPosX = %.2f\n", leftPosX); }
        else if (strcmp(paramName, "left_y") == 0) { leftPosY = value; printf("  -> leftPosY = %.2f\n", leftPosY); }
        else if (strcmp(paramName, "left_rotation") == 0) { leftRotation = value; printf("  -> leftRotation = %.2f\n", leftRotation); }
        else if (strcmp(paramName, "left_scale") == 0) { leftScale = value; printf("  -> leftScale = %.2f\n", leftScale); }
        // Right panel params
        else if (strcmp(paramName, "right_x") == 0) { rightPosX = value; printf("  -> rightPosX = %.2f\n", rightPosX); }
        else if (strcmp(paramName, "right_y") == 0) { rightPosY = value; printf("  -> rightPosY = %.2f\n", rightPosY); }
        else if (strcmp(paramName, "right_rotation") == 0) { rightRotation = value; printf("  -> rightRotation = %.2f\n", rightRotation); }
        else if (strcmp(paramName, "right_scale") == 0) { rightScale = value; printf("  -> rightScale = %.2f\n", rightScale); }
        // Single sprite params (non-mirrored mode)
        else if (strcmp(paramName, "center_x") == 0 || strcmp(paramName, "x") == 0) { staticPosX = value; printf("  -> staticPosX = %.2f\n", staticPosX); }
        else if (strcmp(paramName, "center_y") == 0 || strcmp(paramName, "y") == 0) { staticPosY = value; printf("  -> staticPosY = %.2f\n", staticPosY); }
        else if (strcmp(paramName, "rotation") == 0) { staticRotation = value; printf("  -> staticRotation = %.2f\n", staticRotation); }
        else if (strcmp(paramName, "scale") == 0) { staticScale = value; printf("  -> staticScale = %.2f\n", staticScale); }
        // Flip params (boolean as 0/1)
        else if (strcmp(paramName, "flip_x") == 0) { staticFlipX = (value > 0.5f); printf("  -> staticFlipX = %s\n", staticFlipX ? "true" : "false"); }
        else if (strcmp(paramName, "left_flip_x") == 0) { leftFlipX = (value > 0.5f); printf("  -> leftFlipX = %s\n", leftFlipX ? "true" : "false"); }
        else if (strcmp(paramName, "right_flip_x") == 0) { rightFlipX = (value > 0.5f); printf("  -> rightFlipX = %s\n", rightFlipX ? "true" : "false"); }
        // Shader params
        else if (strcmp(paramName, "shader_type") == 0) { 
            shaderType = (uint8_t)value; 
            shaderDirty = true;
            // When switching to HUE_CYCLE or GRADIENT_CYCLE, ensure palette/params are sent
            if (shaderType == 2 || shaderType == 3) {
                shaderPaletteDirty = true;
            }
            if (shaderType == 3) {
                shaderGradientParamsDirty = true;
            }
            printf("  -> shaderType = %d\n", shaderType); 
        }
        else if (strcmp(paramName, "shader_invert") == 0) { 
            shaderInvert = (value > 0.5f); 
            shaderDirty = true;
            printf("  -> shaderInvert = %s\n", shaderInvert ? "true" : "false"); 
        }
        else if (strcmp(paramName, "shader_mask_enabled") == 0) { 
            shaderMaskEnabled = (value > 0.5f); 
            shaderDirty = true;
            printf("  -> shaderMaskEnabled = %s\n", shaderMaskEnabled ? "true" : "false"); 
        }
        else if (strcmp(paramName, "shader_mask_r") == 0) { 
            shaderMaskR = (uint8_t)value; 
            shaderDirty = true;
            printf("  -> shaderMaskR = %d\n", shaderMaskR); 
        }
        else if (strcmp(paramName, "shader_mask_g") == 0) { 
            shaderMaskG = (uint8_t)value; 
            shaderDirty = true;
            printf("  -> shaderMaskG = %d\n", shaderMaskG); 
        }
        else if (strcmp(paramName, "shader_mask_b") == 0) { 
            shaderMaskB = (uint8_t)value; 
            shaderDirty = true;
            printf("  -> shaderMaskB = %d\n", shaderMaskB); 
        }
        else if (strcmp(paramName, "shader_override_r") == 0) { 
            shaderOverrideR = (uint8_t)value; 
            shaderDirty = true;
            printf("  -> shaderOverrideR = %d\n", shaderOverrideR); 
        }
        else if (strcmp(paramName, "shader_override_g") == 0) { 
            shaderOverrideG = (uint8_t)value; 
            shaderDirty = true;
            printf("  -> shaderOverrideG = %d\n", shaderOverrideG); 
        }
        else if (strcmp(paramName, "shader_override_b") == 0) { 
            shaderOverrideB = (uint8_t)value; 
            shaderDirty = true;
            printf("  -> shaderOverrideB = %d\n", shaderOverrideB); 
        }
        // Hue Cycle shader params (accept both naming conventions)
        else if (strcmp(paramName, "shader_hue_speed") == 0 || strcmp(paramName, "shader_hue_cycle_speed") == 0) { 
            shaderHueCycleSpeed = (uint16_t)value; 
            shaderDirty = true;
            printf("  -> shaderHueCycleSpeed = %d\n", shaderHueCycleSpeed); 
        }
        else if (strcmp(paramName, "shader_hue_color_count") == 0) { 
            uint8_t count = (uint8_t)value;
            if (count < 1) count = 1;
            if (count > 32) count = 32;
            shaderHueCycleColorCount = count; 
            shaderPaletteDirty = true;
            shaderDirty = true;
            printf("  -> shaderHueCycleColorCount = %d\n", shaderHueCycleColorCount); 
        }
        // Individual palette color components (0-31, each with r/g/b)
        else if (strncmp(paramName, "shader_hue_color_", 17) == 0) {
            // Parse: shader_hue_color_N_c or shader_hue_color_NN_c where N=0-31, c=r/g/b
            int colorIndex = 0;
            int underscorePos = 18;
            if (paramName[18] >= '0' && paramName[18] <= '9') {
                // Two-digit color index
                colorIndex = (paramName[17] - '0') * 10 + (paramName[18] - '0');
                underscorePos = 19;
            } else {
                // Single-digit color index
                colorIndex = paramName[17] - '0';
                underscorePos = 18;
            }
            
            if (colorIndex >= 0 && colorIndex < 32 && paramName[underscorePos] == '_') {
                char component = paramName[underscorePos + 1];
                int offset = colorIndex * 3;
                if (component == 'r') {
                    shaderHuePalette[offset + 0] = (uint8_t)value;
                    shaderPaletteDirty = true;
                    printf("  -> shaderHuePalette[%d].r = %d\n", colorIndex, (uint8_t)value);
                } else if (component == 'g') {
                    shaderHuePalette[offset + 1] = (uint8_t)value;
                    shaderPaletteDirty = true;
                    printf("  -> shaderHuePalette[%d].g = %d\n", colorIndex, (uint8_t)value);
                } else if (component == 'b') {
                    shaderHuePalette[offset + 2] = (uint8_t)value;
                    shaderPaletteDirty = true;
                    printf("  -> shaderHuePalette[%d].b = %d\n", colorIndex, (uint8_t)value);
                }
            }
        }
        // Gradient Cycle shader params
        else if (strcmp(paramName, "shader_gradient_distance") == 0) { 
            shaderGradientDistance = (uint16_t)value; 
            shaderGradientParamsDirty = true;
            printf("  -> shaderGradientDistance = %d\n", shaderGradientDistance); 
        }
        else if (strcmp(paramName, "shader_gradient_angle") == 0) { 
            shaderGradientAngle = (int16_t)value; 
            shaderGradientParamsDirty = true;
            printf("  -> shaderGradientAngle = %d\n", shaderGradientAngle); 
        }
        else if (strcmp(paramName, "shader_gradient_mirror") == 0) { 
            shaderGradientMirror = (value != 0); 
            shaderGradientParamsDirty = true;
            printf("  -> shaderGradientMirror = %s\n", shaderGradientMirror ? "true" : "false"); 
        }
        // Glitch shader params
        else if (strcmp(paramName, "shader_glitch_speed") == 0) { 
            shaderGlitchSpeed = (uint8_t)value; 
            shaderGlitchParamsDirty = true;
            printf("  -> shaderGlitchSpeed = %d\n", shaderGlitchSpeed); 
        }
        else if (strcmp(paramName, "shader_glitch_intensity") == 0) { 
            shaderGlitchIntensity = (uint8_t)value; 
            shaderGlitchParamsDirty = true;
            printf("  -> shaderGlitchIntensity = %d\n", shaderGlitchIntensity); 
        }
        else if (strcmp(paramName, "shader_glitch_chromatic") == 0) { 
            shaderGlitchChromatic = (uint8_t)value; 
            shaderGlitchParamsDirty = true;
            printf("  -> shaderGlitchChromatic = %d\n", shaderGlitchChromatic); 
        }
        else if (strcmp(paramName, "shader_glitch_quantity") == 0) { 
            shaderGlitchQuantity = (uint8_t)value; 
            shaderGlitchParamsDirty = true;
            printf("  -> shaderGlitchQuantity = %d\n", shaderGlitchQuantity); 
        }
        // Reactive eyes params
        else if (strcmp(paramName, "reactive_y_sensitivity") == 0) { reactiveYSensitivity = value; printf("  -> reactiveYSensitivity = %.2f\n", reactiveYSensitivity); }
        else if (strcmp(paramName, "reactive_z_sensitivity") == 0) { reactiveZSensitivity = value; printf("  -> reactiveZSensitivity = %.2f\n", reactiveZSensitivity); }
        else if (strcmp(paramName, "reactive_x_sensitivity") == 0) { reactiveXSensitivity = value; printf("  -> reactiveXSensitivity = %.2f\n", reactiveXSensitivity); }
        else if (strcmp(paramName, "reactive_rot_sensitivity") == 0) { reactiveRotSensitivity = value; printf("  -> reactiveRotSensitivity = %.2f\n", reactiveRotSensitivity); }
        else if (strcmp(paramName, "reactive_smoothing") == 0) { reactiveSmoothing = fmaxf(0.01f, fminf(1.0f, value)); printf("  -> reactiveSmoothing = %.2f\n", reactiveSmoothing); }
        else if (strcmp(paramName, "reactive_scale") == 0) { 
            reactiveScale = value; 
            storeBaseValue("reactive_scale", value);  // Store as base for mic reactivity
            printf("  -> reactiveScale = %.2f (stored as base)\n", reactiveScale); 
        }
        else if (strcmp(paramName, "reactive_base_left_x") == 0) { reactiveBaseLeftX = value; printf("  -> reactiveBaseLeftX = %.2f\n", reactiveBaseLeftX); }
        else if (strcmp(paramName, "reactive_base_left_y") == 0) { reactiveBaseLeftY = value; printf("  -> reactiveBaseLeftY = %.2f\n", reactiveBaseLeftY); }
        else if (strcmp(paramName, "reactive_left_flip_x") == 0) { reactiveLeftFlipX = (value > 0.5f); printf("  -> reactiveLeftFlipX = %s\n", reactiveLeftFlipX ? "true" : "false"); }
        else if (strcmp(paramName, "reactive_left_rot_offset") == 0) { reactiveLeftRotOffset = value; printf("  -> reactiveLeftRotOffset = %.2f\n", reactiveLeftRotOffset); }
        else if (strcmp(paramName, "reactive_base_right_x") == 0) { reactiveBaseRightX = value; printf("  -> reactiveBaseRightX = %.2f\n", reactiveBaseRightX); }
        else if (strcmp(paramName, "reactive_base_right_y") == 0) { reactiveBaseRightY = value; printf("  -> reactiveBaseRightY = %.2f\n", reactiveBaseRightY); }
        else if (strcmp(paramName, "reactive_right_flip_x") == 0) { reactiveRightFlipX = (value > 0.5f); printf("  -> reactiveRightFlipX = %s\n", reactiveRightFlipX ? "true" : "false"); }
        else if (strcmp(paramName, "reactive_right_rot_offset") == 0) { reactiveRightRotOffset = value; printf("  -> reactiveRightRotOffset = %.2f\n", reactiveRightRotOffset); }
        else { printf("  -> UNKNOWN PARAM\n"); }
        
        // Auto-save shader params to active scene (if not loading from scene)
        // This ensures shader config persists per-scene
        if (!loadingSceneParams && strncmp(paramName, "shader_", 7) == 0) {
            if (SystemAPI::Web::HttpServer::updateActiveSceneParam(paramName, value)) {
                // Throttled save to storage
                SystemAPI::Web::HttpServer::saveActiveSceneParams();
            }
        }
    }
    
    // Set mirror enabled state directly
    void setMirrorEnabled(bool enabled) {
        mirrorEnabled = enabled;
        printf("[setMirrorEnabled] mirrorEnabled = %s\n", enabled ? "true" : "false");
    }
    
    bool isMirrorEnabled() { return mirrorEnabled; }
    
    void setBackgroundColor(uint8_t r, uint8_t g, uint8_t b) {
        bgR = r;
        bgG = g;
        bgB = b;
    }

    void updateSandboxSensors(float gyroX, float gyroY, float gyroZ) {
        sandboxPitch = gyroX;
        sandboxRoll = gyroY;
        sandboxAudioLevel = gyroZ;
    }
    
    // ====== LED STRIP CONTROL (RMT-based) ======
    static const uint32_t RMT_LED_RESOLUTION_HZ = 10000000; // 10MHz for WS2812
    
    bool initLedStrips() {
        if (ledsInitialized) return true;
        
        printf("  LED: Initializing LED strips (RMT driver)...\n");
        
        int initCount = 0;
        for (int i = 0; i < 6; i++) {
            if (LED_COUNTS[i] > 0) {
                // Configure RMT TX channel
                rmt_tx_channel_config_t tx_config = {};
                tx_config.gpio_num = LED_PINS[i];
                tx_config.clk_src = RMT_CLK_SRC_DEFAULT;
                tx_config.resolution_hz = RMT_LED_RESOLUTION_HZ;
                tx_config.mem_block_symbols = 64;  // 64 symbols per block
                tx_config.trans_queue_depth = 4;
                
                esp_err_t err = rmt_new_tx_channel(&tx_config, &ledStrips[i].channel);
                if (err != ESP_OK) {
                    printf("  LED: Strip %d RMT channel failed (err=%d)\n", i, err);
                    continue;
                }
                
                // Create LED strip encoder
                led_strip_encoder_config_t encoder_config = {};
                encoder_config.resolution = RMT_LED_RESOLUTION_HZ;
                err = rmt_new_led_strip_encoder(&encoder_config, &ledStrips[i].encoder);
                if (err != ESP_OK) {
                    printf("  LED: Strip %d encoder failed (err=%d)\n", i, err);
                    rmt_del_channel(ledStrips[i].channel);
                    continue;
                }
                
                // Enable channel
                err = rmt_enable(ledStrips[i].channel);
                if (err != ESP_OK) {
                    printf("  LED: Strip %d enable failed (err=%d)\n", i, err);
                    rmt_del_encoder(ledStrips[i].encoder);
                    rmt_del_channel(ledStrips[i].channel);
                    continue;
                }
                
                // Allocate pixel buffer (GRBW format, 4 bytes per LED for SK6812 RGBW)
                ledStrips[i].pixelBuffer = (uint8_t*)malloc(LED_COUNTS[i] * 4);
                if (!ledStrips[i].pixelBuffer) {
                    printf("  LED: Strip %d buffer alloc failed\n", i);
                    rmt_disable(ledStrips[i].channel);
                    rmt_del_encoder(ledStrips[i].encoder);
                    rmt_del_channel(ledStrips[i].channel);
                    continue;
                }
                
                memset(ledStrips[i].pixelBuffer, 0, LED_COUNTS[i] * 4);
                ledStrips[i].ledCount = LED_COUNTS[i];
                ledStrips[i].pin = LED_PINS[i];
                ledStrips[i].initialized = true;
                
                printf("  LED: Strip %d (%s) init OK: pin=%d, LEDs=%d\n", 
                       i, LED_NAMES[i], LED_PINS[i], LED_COUNTS[i]);
                initCount++;
            }
        }
        
        ledsInitialized = (initCount > 0);
        printf("  LED: %d strips initialized, %s\n", initCount, ledsInitialized ? "Ready" : "FAILED");
        return ledsInitialized;
    }
    
    // Send pixel buffer to LED strip via RMT
    void showStrip(int index) {
        if (index < 0 || index >= 6 || !ledStrips[index].initialized) return;
        
        rmt_transmit_config_t tx_config = {};
        tx_config.loop_count = 0;  // No loop
        
        rmt_transmit(ledStrips[index].channel, ledStrips[index].encoder, 
                     ledStrips[index].pixelBuffer, ledStrips[index].ledCount * 4, 
                     &tx_config);
        rmt_tx_wait_all_done(ledStrips[index].channel, pdMS_TO_TICKS(100));
    }
    
    void setLedColor(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness) {
        if (!ledsInitialized) {
            initLedStrips();
        }
        
        currentLedR = r;
        currentLedG = g;
        currentLedB = b;
        currentLedBrightness = brightness;
        
        // Apply brightness (scale RGB values)
        uint8_t scaledR = (r * brightness) / 255;
        uint8_t scaledG = (g * brightness) / 255;
        uint8_t scaledB = (b * brightness) / 255;
        
        // Apply to all strips
        for (int i = 0; i < 6; i++) {
            if (ledStrips[i].initialized) {
                // Fill pixel buffer (GRBW format for SK6812 RGBW)
                for (int j = 0; j < ledStrips[i].ledCount; j++) {
                    ledStrips[i].pixelBuffer[j * 4 + 0] = scaledG;  // G
                    ledStrips[i].pixelBuffer[j * 4 + 1] = scaledR;  // R
                    ledStrips[i].pixelBuffer[j * 4 + 2] = scaledB;  // B
                    ledStrips[i].pixelBuffer[j * 4 + 3] = 0;        // W (unused for now)
                }
                showStrip(i);
            }
        }
        printf("  LED: Color set R=%d G=%d B=%d Brightness=%d\n", r, g, b, brightness);
    }
    
    void setLedsEnabled(bool enabled) {
        if (!ledsInitialized) {
            initLedStrips();
        }
        
        ledsEnabled = enabled;
        
        if (enabled) {
            // Apply current color
            setLedColor(currentLedR, currentLedG, currentLedB, currentLedBrightness);
        } else {
            // Turn off all LEDs
            for (int i = 0; i < 6; i++) {
                if (ledStrips[i].initialized) {
                    memset(ledStrips[i].pixelBuffer, 0, ledStrips[i].ledCount * 4);
                    showStrip(i);
                }
            }
            printf("  LED: Off\n");
        }
    }
    
    bool areLedsEnabled() { return ledsEnabled; }
    
    // Getters - provide access to the global GpuCommands
    GpuCommands& getGpu() { return g_gpu; }
    uint32_t getGpuUptime() { return gpuUptimeMs; }
    // Note: isSpriteReady() and isConnected() are defined earlier in this namespace
}

//=============================================================================
// CurrentMode Implementation
//=============================================================================

void CurrentMode::onStart() {
    printf("\n");
    printf("  ╔════════════════════════════════════╗\n");
    printf("  ║        CURRENT MODE STARTED        ║\n");
    printf("  ╚════════════════════════════════════╝\n\n");
    
    // Initialize GPS driver (using modular driver)
    if (Drivers::GpsDriver::init()) {
        printf("  GPS: Ready\n");
    } else {
        printf("  GPS: Init failed - will show N/C\n");
    }
    
    // Initialize Microphone driver (using modular driver)
    if (Drivers::MicDriver::init()) {
        printf("  MIC: Ready\n");
    } else {
        printf("  MIC: Init failed - will use simulation\n");
    }
    
    // Initialize IMU driver (using modular driver)
    if (Drivers::ImuDriver::init()) {
        printf("  IMU: Ready\n");
    } else {
        printf("  IMU: Init failed - will use simulation\n");
    }

    // Initialize BME280 driver (shares I2C bus with IMU - must init after IMU)
    if (Drivers::BmeDriver::init()) {
        printf("  BME: Ready\n");
    } else {
        printf("  BME: Init failed - will show N/C\n");
    }

    // Initialize Fan driver (using modular driver)
    if (Drivers::FanDriver::init()) {
        printf("  FAN: Ready\n");
    } else {
        printf("  FAN: Init failed\n");
    }

    // Initialize GPU UART driver
    if (GpuDriverState::init()) {
        printf("  GPU: UART Ready - waiting for connection\n");
        
        // NOTE: Test sprite upload removed - web upload pipeline verified working!
        // GpuDriverState::uploadTestSprite();  // Uncomment to test GPU on startup
    } else {
        printf("  GPU: UART init failed - will show N/C\n");
    }
    
    // Initialize SD Card
    auto& sdCard = SystemAPI::Utils::FileSystemService::instance();
    SystemAPI::Utils::SdCardPins sdPins = {
        .miso = 14,
        .mosi = 47,
        .clk = 21,
        .cs = 48
    };
    if (sdCard.init(sdPins)) {
        printf("  SD Card: Ready (%llu MB total, %llu MB free)\n", 
               sdCard.getTotalBytes() / (1024 * 1024),
               sdCard.getFreeBytes() / (1024 * 1024));
    } else {
        printf("  SD Card: Not available\n");
    }
    
    // =====================================================
    // YAML Scene Driver (DISABLED - conflicts with FileSystemService SD card)
    // TODO: Integrate SDManager with FileSystemService or use a shared SD mount
    // =====================================================
    // The YAML Scene Driver is ready but disabled to avoid SD card conflicts.
    // The existing HttpServer scene system continues to work.
    // Enable this when SDManager is updated to use the existing mount.
    #if 0
    printf("\n  ┌────────────────────────────────────┐\n");
    printf("  │   YAML SCENE MANAGER               │\n");
    printf("  └────────────────────────────────────┘\n");
    
    // Set up sprite upload callback (uploads to GPU)
    Drivers::YamlSceneDriver::setSpriteUploadCallback(
        [](int gpuSlot, const uint8_t* data, size_t size, int width, int height) -> bool {
            printf("  YamlScene: Uploading sprite to GPU slot %d (%dx%d)\n", gpuSlot, width, height);
            GpuDriverState::getGpu().deleteSprite(static_cast<uint8_t>(gpuSlot));
            vTaskDelay(pdMS_TO_TICKS(10));
            return GpuDriverState::getGpu().uploadSprite(
                static_cast<uint8_t>(gpuSlot), data, 
                static_cast<uint16_t>(width), static_cast<uint16_t>(height));
        }
    );
    
    // Set up scene activation callback (configures animation)
    Drivers::YamlSceneDriver::setSceneActivateCallback(
        [](const std::string& animType, int spriteId, float posX, float posY,
           float sensitivity, bool mirror, uint8_t bgR, uint8_t bgG, uint8_t bgB,
           bool displayEnabled, bool ledsEnabled,
           uint8_t ledR, uint8_t ledG, uint8_t ledB, int ledBrightness) {
            
            printf("  YamlScene: Activating animation '%s'\n", animType.c_str());
            
            if (displayEnabled) {
                // Set animation type
                GpuDriverState::setSceneAnimation(animType);
                
                // Configure gyro eyes if that's the animation type
                if (animType == "gyro_eyes") {
                    GpuDriverState::setGyroEyeParams(12.0f, sensitivity, mirror, spriteId);
                }
                
                // Set background color
                GpuDriverState::setSpriteScene(
                    spriteId >= 0 ? static_cast<uint8_t>(spriteId) : 0,
                    posX, posY, 0.0f, bgR, bgG, bgB);
            } else {
                GpuDriverState::setSceneAnimation("none");
            }
            
            // Handle LEDs
            if (ledsEnabled) {
                GpuDriverState::setLedColor(ledR, ledG, ledB, 
                    static_cast<uint8_t>(ledBrightness * 255 / 100));
                GpuDriverState::setLedsEnabled(true);
            } else {
                GpuDriverState::setLedsEnabled(false);
            }
        }
    );
    
    // Initialize the YAML scene driver (loads scenes from /scenes/)
    if (Drivers::YamlSceneDriver::init(14, 47, 21, 48)) {
        printf("  YAML Scenes: Loaded %zu scenes\n", Drivers::YamlSceneDriver::getSceneCount());
        
        // List loaded scenes
        for (const auto& scene : Drivers::YamlSceneDriver::getScenes()) {
            printf("    - [%d] %s (%s)\n", scene.id, scene.name.c_str(), 
                   scene.animation.type.c_str());
        }
        
        // Activate first scene if available
        if (Drivers::YamlSceneDriver::getSceneCount() > 0) {
            int firstSceneId = Drivers::YamlSceneDriver::getScenes()[0].id;
            Drivers::YamlSceneDriver::activateScene(firstSceneId);
            printf("  YAML Scenes: Activated scene %d\n", firstSceneId);
        }
    } else {
        printf("  YAML Scenes: Init failed (SD card issue?)\n");
    }
    #endif
    printf("  YAML Scenes: Disabled (use HttpServer scenes for now)\n");
    
    // =====================================================
    // DISABLED: Dual-Core Application Layer
    // The Application layer has its own GpuPipeline on Core 1 which
    // conflicts with the GpuDriver we're using. To match the working
    // WifiSpriteUploadTest, we use only GpuDriver from Core 0.
    // =====================================================
    printf("\n  ┌────────────────────────────────────┐\n");
    printf("  │   SINGLE-CORE GPU MODE (TEST)      │\n");
    printf("  └────────────────────────────────────┘\n");
    printf("  Using GpuDriver from Core 0 only (like WifiSpriteUploadTest)\n");
    printf("  Application layer DISABLED to avoid UART conflict\n\n");
    
    // =====================================================
    // Initialize Animation Handler
    // =====================================================
    auto& animHandler = Modes::getAnimationHandler();
    if (animHandler.init()) {
        printf("  AnimationHandler: Initialized\n");
        
        // Wire GPU callbacks to AnimationHandler (using GpuCommands hub75* methods)
        animHandler.wireGpuCallbacks(
            // Clear function
            [](uint8_t r, uint8_t g, uint8_t b) {
                GpuDriverState::getGpu().hub75Clear(r, g, b);
            },
            // Blit sprite function (use float for smooth sub-pixel positioning)
            [](int id, float x, float y) {
                GpuDriverState::getGpu().blitSpriteF(static_cast<uint8_t>(id), x, y);
            },
            // Blit sprite rotated function
            [](int id, float x, float y, float angle) {
                GpuDriverState::getGpu().blitSpriteRotated(static_cast<uint8_t>(id), x, y, angle);
            },
            // Fill circle function (GpuCommands uses hub75Circle)
            [](int cx, int cy, int r, uint8_t red, uint8_t green, uint8_t blue) {
                GpuDriverState::getGpu().hub75Circle(static_cast<int16_t>(cx), 
                                                     static_cast<int16_t>(cy), 
                                                     static_cast<int16_t>(r), 
                                                     red, green, blue);
            },
            // Fill rect function (GpuCommands uses hub75Fill)
            [](int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b) {
                GpuDriverState::getGpu().hub75Fill(static_cast<int16_t>(x), 
                                                   static_cast<int16_t>(y), 
                                                   static_cast<int16_t>(w), 
                                                   static_cast<int16_t>(h), 
                                                   r, g, b);
            },
            // Present function
            []() {
                GpuDriverState::getGpu().hub75Present();
            }
        );
        printf("  AnimationHandler: GPU callbacks wired\n");
    } else {
        printf("  AnimationHandler: Init failed\n");
    }
    
    // =====================================================
    // Initialize Animation Sandbox (EXPERIMENTAL)
    // =====================================================
    auto& sandbox = AnimationSystem::Sandbox::getSandbox();
    sandbox.clear = [](uint8_t r, uint8_t g, uint8_t b) {
        GpuDriverState::getGpu().hub75Clear(r, g, b);
    };
    sandbox.fillRect = [](int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b) {
        GpuDriverState::getGpu().hub75Fill(static_cast<int16_t>(x), 
                                           static_cast<int16_t>(y), 
                                           static_cast<int16_t>(w), 
                                           static_cast<int16_t>(h), 
                                           r, g, b);
    };
    sandbox.drawPixel = [](int x, int y, uint8_t r, uint8_t g, uint8_t b) {
        GpuDriverState::getGpu().hub75Pixel(static_cast<int16_t>(x), 
                                            static_cast<int16_t>(y), 
                                            r, g, b);
    };
    sandbox.blitSprite = [](int id, float x, float y) {
        GpuDriverState::getGpu().blitSpriteF(static_cast<uint8_t>(id), x, y);
    };
    sandbox.blitSpriteRotated = [](int id, float x, float y, float angle) {
        GpuDriverState::getGpu().blitSpriteRotated(static_cast<uint8_t>(id), x, y, angle);
    };
    sandbox.drawCircleF = [](float x, float y, float radius, uint8_t r, uint8_t g, uint8_t b) {
        GpuDriverState::getGpu().hub75Circle((int16_t)x, (int16_t)y, (int16_t)radius, r, g, b);
    };
    sandbox.present = []() {
        GpuDriverState::getGpu().hub75Present();
        // Small yield to let GPU process (prevents UART buffer overflow at high FPS)
        vTaskDelay(1);  // 1 tick = ~1ms - gives GPU time to drain command buffer
    };
    // Don't enable sandbox by default - use scene-based animation instead
    // sandbox.setEnabled(true);
    // GpuDriverState::enableSandbox(true);
    printf("  AnimationSandbox: Configured (will be enabled when scene uses SDF_MORPH)\n");
    
    // Set default animation to STATIC_IMAGE with mirror enabled (vector eye on both panels)
    GpuDriverState::setSceneAnimation("static_sprite");
    GpuDriverState::setMirrorEnabled(true);  // Start with mirrored (both eyes visible)
    printf("  Default Animation: STATIC with mirror=ON (vector eye)\n");
    
    // Upload eye sprites for AA rendering
    printf("  Uploading eye sprites for AA rendering...\n");
    GpuDriverState::uploadEyeSprites();
    printf("  Eye sprites ready!\n");
    
    /*
    // DISABLED: This creates a conflicting UART driver on Core 1
    if (Application::init()) {
        printf("  App Layer: Initialized\\n");
        
        // Configure eye controller
        auto& eye = Application::eye();
        Application::EyeControllerConfig eyeConfig;
        eyeConfig.autoBlinkEnabled = true;
        eyeConfig.autoBlinkIntervalMin = 2.5f;
        eyeConfig.autoBlinkIntervalMax = 5.0f;
        eyeConfig.idleLookEnabled = true;
        eyeConfig.idleLookRange = 0.3f;
        eyeConfig.imuLookEnabled = true;  // Enable IMU-driven eye movement
        eyeConfig.imuSensitivity = 0.03f;
        eyeConfig.imuDeadzone = 8.0f;
        eyeConfig.defaultShader = 1;      // Rainbow
        eyeConfig.defaultBrightness = 80;
        eyeConfig.mirrorMode = true;
        eye.configure(eyeConfig);
        printf("  Eye Controller: Configured\\n");
        
        // Start dual-core execution (GPU task on Core 1)
        if (Application::start()) {
            printf("  Core 1 GPU Task: Started\\n");
            printf("  Animation Pipeline: Running at 60 FPS\\n");
        } else {
            printf("  Core 1 GPU Task: FAILED TO START\\n");
        }
    } else {
        printf("  App Layer: INIT FAILED\\n");
    }
    */
    
    // =====================================================
    // Set up Web -> GPU Pipeline Callbacks
    // These connect web UI actions to GPU rendering
    // =====================================================
    auto& httpServer = SystemAPI::Web::HttpServer::instance();
    
    // Callback for displaying a sprite scene on HUB75
    // This sets up CONTINUOUS rendering at ~30fps via GpuDriverState::update()
    // (Exactly like WifiSpriteUploadTest approach)
    httpServer.setSpriteDisplayCallback([](const SystemAPI::Web::StaticSpriteSceneConfig& config) {
        printf("\n  ========================================\n");
        printf("  SPRITE DISPLAY - Setting Scene Config\n");
        printf("  Sprite ID: %d\n", config.spriteId);
        printf("  Position: (%d, %d)\n", config.posX, config.posY);
        printf("  Background: RGB(%d, %d, %d)\n", config.bgR, config.bgG, config.bgB);
        
        // Look up the sprite to get pixel data
        auto* sprite = SystemAPI::Web::HttpServer::findSpriteById(config.spriteId);
        if (sprite) {
            printf("  Sprite found: '%s' (%dx%d), %zu bytes\n", 
                   sprite->name.c_str(), sprite->width, sprite->height,
                   sprite->pixelData.size());
            
            // Always re-upload sprite to GPU to ensure fresh data
            if (!sprite->pixelData.empty()) {
                printf("  Uploading sprite to GPU cache...\n");
                uint8_t gpuSpriteId = 0;  // Use slot 0 for web-uploaded sprites
                
                // Delete old sprite first to ensure clean state
                GpuDriverState::getGpu().deleteSprite(gpuSpriteId);
                vTaskDelay(pdMS_TO_TICKS(10));  // Small delay
                
                // Upload sprite (GpuCommands signature: id, data, width, height)
                if (GpuDriverState::getGpu().uploadSprite(gpuSpriteId, 
                                                          sprite->pixelData.data(),
                                                          sprite->width, 
                                                          sprite->height)) {
                    // CRITICAL: Wait for GPU to fully process sprite upload
                    vTaskDelay(pdMS_TO_TICKS(200));  // 200ms for safety
                    
                    // Set up continuous rendering scene (like WifiSpriteUploadTest)
                    // Use blitSpriteRotated which centers the sprite at (x, y)
                    // Center of 128x32 display is (64, 16)
                    float centerX = 64.0f;
                    float centerY = 16.0f;
                    float angle = 0.0f;  // No rotation
                    
                    GpuDriverState::setSpriteScene(
                        gpuSpriteId,
                        centerX, centerY, angle,
                        config.bgR, config.bgG, config.bgB
                    );
                    
                    SystemAPI::Web::HttpServer::markSpriteUploaded(config.spriteId);
                    printf("  Sprite uploaded to GPU slot %d (%zu bytes)\n", 
                           gpuSpriteId, sprite->pixelData.size());
                    printf("  Continuous rendering enabled at ~30fps\n");
                } else {
                    printf("  ERROR: Failed to upload sprite to GPU!\n");
                }
            } else {
                printf("  WARNING: No pixel data - showing test pattern\n");
            }
        } else {
            printf("  WARNING: Sprite ID %d not found!\n", config.spriteId);
        }
        printf("  ========================================\n\n");
    });
    
    // Callback for clearing the display (returns to animation mode)
    httpServer.setDisplayClearCallback([]() {
        printf("  Clearing display via GpuCommands\n");
        
        // Stop continuous sprite rendering
        GpuDriverState::clearSpriteScene();
        
        // Clear the HUB75 display
        GpuDriverState::getGpu().hub75Clear(0, 0, 0);
        GpuDriverState::getGpu().hub75Present();
        
        printf("  Display cleared\n");
    });
    
    // Callback for scene activation - switches animation mode based on scene settings
    httpServer.setSceneActivatedCallback([](const SystemAPI::Web::SavedScene& scene) {
        printf("\n  ========================================\n");
        printf("  SCENE ACTIVATED: %s (id=%d)\n", scene.name.c_str(), scene.id);
        printf("  Animation Type: %s\n", scene.animType.c_str());
        printf("  Display Enabled: %s\n", scene.displayEnabled ? "YES" : "NO");
        printf("  Background: RGB(%d,%d,%d)\n", scene.bgR, scene.bgG, scene.bgB);
        printf("  [Memory] Free heap: %lu bytes, Min: %lu bytes\n", 
               esp_get_free_heap_size(), esp_get_minimum_free_heap_size());
        printf("  ========================================\n\n");
        
        // Initialize mic reactivity manager for this scene (clears state, will load settings later)
        GpuDriverState::initMicReactivityForScene();
        
        // Update OLED to show this as the active scene
        OledMenu::OledMenuSystem::instance().setActiveSceneId(scene.id);
        
        // Set background color
        GpuDriverState::setBackgroundColor(scene.bgR, scene.bgG, scene.bgB);
        
        // Update animation mode based on scene settings
        if (scene.displayEnabled) {
            // Set the animation type
            GpuDriverState::setSceneAnimation(scene.animType);
            
            // Apply animation-specific parameters from scene.params
            // Note: params is a map<string, float>
            
            // Mirror is determined by:
            // 1. scene.mirrorSprite field (explicit toggle from web UI)
            // 2. animation type: static_mirrored = mirrored
            // 3. Override with explicit mirror param if present
            bool mirrorParam = scene.mirrorSprite;  // Use the dedicated field first!
            if (scene.animType == "static_mirrored") {
                mirrorParam = true;
            } else if (scene.params.count("mirror")) {
                mirrorParam = scene.params.at("mirror") > 0.5f;
            }
            GpuDriverState::setMirrorEnabled(mirrorParam);
            printf("  Mirror mode: %s (mirrorSprite=%s, animType='%s')\n", 
                   mirrorParam ? "YES" : "NO", scene.mirrorSprite ? "YES" : "NO", scene.animType.c_str());
            
            // Gyro eyes parameters - always set defaults if not found
            if (scene.animType == "gyro_eyes") {
                float eyeSize = scene.params.count("eye_size") ? scene.params.at("eye_size") : 12.0f;
                float intensity = scene.params.count("intensity") ? scene.params.at("intensity") : 1.0f;
                bool mirror = scene.params.count("mirror") ? (scene.params.at("mirror") > 0.5f) : true;
                GpuDriverState::setGyroEyeParams(eyeSize, intensity, mirror, scene.spriteId);
                printf("  Gyro Eyes: size=%.1f, intensity=%.1f, mirror=%s, spriteId=%d\n",
                       eyeSize, intensity, mirror ? "YES" : "NO", scene.spriteId);
            }
            // Static image parameters (now unified - uses mirrorEnabled flag)
            else if (scene.animType == "static_image" || scene.animType == "static_sprite" || scene.animType == "static") {
                // Always read both single-sprite and mirrored params
                float posX = scene.params.count("center_x") ? scene.params.at("center_x") : 
                             (scene.params.count("x") ? scene.params.at("x") : 64.0f);
                float posY = scene.params.count("center_y") ? scene.params.at("center_y") : 
                             (scene.params.count("y") ? scene.params.at("y") : 16.0f);
                float rot = scene.params.count("rotation") ? scene.params.at("rotation") : 0.0f;
                float scale = scene.params.count("scale") ? scene.params.at("scale") : 1.0f;
                bool flipX = scene.params.count("flip_x") ? (scene.params.at("flip_x") > 0.5f) : false;
                GpuDriverState::setStaticParams(scale, rot, posX, posY);
                GpuDriverState::setStaticFlip(flipX);
                
                // Also read mirrored params if they exist
                float lx = scene.params.count("left_x") ? scene.params.at("left_x") : 32.0f;
                float ly = scene.params.count("left_y") ? scene.params.at("left_y") : 16.0f;
                float lrot = scene.params.count("left_rotation") ? scene.params.at("left_rotation") : 0.0f;
                float lscale = scene.params.count("left_scale") ? scene.params.at("left_scale") : 1.0f;
                bool leftFlip = scene.params.count("left_flip_x") ? (scene.params.at("left_flip_x") > 0.5f) : false;
                float rx = scene.params.count("right_x") ? scene.params.at("right_x") : 96.0f;
                float ry = scene.params.count("right_y") ? scene.params.at("right_y") : 16.0f;
                float rrot = scene.params.count("right_rotation") ? scene.params.at("right_rotation") : 180.0f;
                float rscale = scene.params.count("right_scale") ? scene.params.at("right_scale") : 1.0f;
                bool rightFlip = scene.params.count("right_flip_x") ? (scene.params.at("right_flip_x") > 0.5f) : false;
                GpuDriverState::setMirroredParams(lx, ly, lrot, lscale, rx, ry, rrot, rscale);
                GpuDriverState::setMirroredFlips(leftFlip, rightFlip);
                
                printf("  Static: pos=(%.1f,%.1f), rot=%.1f, scale=%.1f, flipX=%s, mirror=%s\n", 
                       posX, posY, rot, scale, flipX ? "YES" : "NO", mirrorParam ? "YES" : "NO");
            }
            // Sway parameters
            else if (scene.animType == "sway") {
                float swayX = scene.params.count("sway_x") ? scene.params.at("sway_x") : 10.0f;
                float swayY = scene.params.count("sway_y") ? scene.params.at("sway_y") : 5.0f;
                float rotRange = scene.params.count("rot_range") ? scene.params.at("rot_range") : 15.0f;
                float speed = scene.params.count("speed") ? scene.params.at("speed") : 1.0f;
                GpuDriverState::setSwayParams(swayX, swayY, rotRange, speed, false);
            }
            // Static mirrored - legacy, read left/right individual parameters
            else if (scene.animType == "static_mirrored") {
                // Left panel params
                float lx = scene.params.count("left_x") ? scene.params.at("left_x") : 32.0f;
                float ly = scene.params.count("left_y") ? scene.params.at("left_y") : 16.0f;
                float lrot = scene.params.count("left_rotation") ? scene.params.at("left_rotation") : 0.0f;
                float lscale = scene.params.count("left_scale") ? scene.params.at("left_scale") : 1.0f;
                bool leftFlip = scene.params.count("left_flip_x") ? (scene.params.at("left_flip_x") > 0.5f) : false;
                // Right panel params
                float rx = scene.params.count("right_x") ? scene.params.at("right_x") : 96.0f;
                float ry = scene.params.count("right_y") ? scene.params.at("right_y") : 16.0f;
                float rrot = scene.params.count("right_rotation") ? scene.params.at("right_rotation") : 180.0f;
                float rscale = scene.params.count("right_scale") ? scene.params.at("right_scale") : 1.0f;
                bool rightFlip = scene.params.count("right_flip_x") ? (scene.params.at("right_flip_x") > 0.5f) : false;
                
                GpuDriverState::setMirroredParams(lx, ly, lrot, lscale, rx, ry, rrot, rscale);
                GpuDriverState::setMirroredFlips(leftFlip, rightFlip);
                printf("  Static Mirrored: L(%.0f,%.0f,%.0f,flip=%s) R(%.0f,%.0f,%.0f,flip=%s)\n", 
                       lx, ly, lrot, leftFlip ? "Y" : "N", rx, ry, rrot, rightFlip ? "Y" : "N");
            }
            // Reactive eyes - apply saved params from scene
            else if (scene.animType == "reactive_eyes") {
                printf("  Reactive Eyes: Applying params from scene...\n");
                
                // Reset to defaults first
                GpuDriverState::setSingleParam("reactive_y_sensitivity", 5.0f);
                GpuDriverState::setSingleParam("reactive_z_sensitivity", 5.0f);
                GpuDriverState::setSingleParam("reactive_x_sensitivity", 12.0f);
                GpuDriverState::setSingleParam("reactive_rot_sensitivity", 10.0f);
                GpuDriverState::setSingleParam("reactive_smoothing", 0.15f);
                GpuDriverState::setSingleParam("reactive_scale", 1.0f);
                GpuDriverState::setSingleParam("reactive_base_left_x", 32.0f);
                GpuDriverState::setSingleParam("reactive_base_left_y", 16.0f);
                GpuDriverState::setSingleParam("reactive_base_right_x", 96.0f);
                GpuDriverState::setSingleParam("reactive_base_right_y", 16.0f);
                GpuDriverState::setSingleParam("reactive_left_flip_x", 0.0f);
                GpuDriverState::setSingleParam("reactive_right_flip_x", 0.0f);
                GpuDriverState::setSingleParam("reactive_left_rot_offset", 0.0f);
                GpuDriverState::setSingleParam("reactive_right_rot_offset", 0.0f);
                
                // Apply saved values from scene.params (overrides defaults if present)
                if (scene.params.count("reactive_y_sensitivity")) {
                    GpuDriverState::setSingleParam("reactive_y_sensitivity", scene.params.at("reactive_y_sensitivity"));
                    printf("    reactive_y_sensitivity = %.2f\n", scene.params.at("reactive_y_sensitivity"));
                }
                if (scene.params.count("reactive_z_sensitivity")) {
                    GpuDriverState::setSingleParam("reactive_z_sensitivity", scene.params.at("reactive_z_sensitivity"));
                    printf("    reactive_z_sensitivity = %.2f\n", scene.params.at("reactive_z_sensitivity"));
                }
                if (scene.params.count("reactive_x_sensitivity")) {
                    GpuDriverState::setSingleParam("reactive_x_sensitivity", scene.params.at("reactive_x_sensitivity"));
                    printf("    reactive_x_sensitivity = %.2f\n", scene.params.at("reactive_x_sensitivity"));
                }
                if (scene.params.count("reactive_rot_sensitivity")) {
                    GpuDriverState::setSingleParam("reactive_rot_sensitivity", scene.params.at("reactive_rot_sensitivity"));
                    printf("    reactive_rot_sensitivity = %.2f\n", scene.params.at("reactive_rot_sensitivity"));
                }
                if (scene.params.count("reactive_smoothing")) {
                    GpuDriverState::setSingleParam("reactive_smoothing", scene.params.at("reactive_smoothing"));
                    printf("    reactive_smoothing = %.2f\n", scene.params.at("reactive_smoothing"));
                }
                if (scene.params.count("reactive_scale")) {
                    GpuDriverState::setSingleParam("reactive_scale", scene.params.at("reactive_scale"));
                    printf("    reactive_scale = %.2f\n", scene.params.at("reactive_scale"));
                }
                if (scene.params.count("reactive_base_left_x")) {
                    GpuDriverState::setSingleParam("reactive_base_left_x", scene.params.at("reactive_base_left_x"));
                    printf("    reactive_base_left_x = %.2f\n", scene.params.at("reactive_base_left_x"));
                }
                if (scene.params.count("reactive_base_left_y")) {
                    GpuDriverState::setSingleParam("reactive_base_left_y", scene.params.at("reactive_base_left_y"));
                    printf("    reactive_base_left_y = %.2f\n", scene.params.at("reactive_base_left_y"));
                }
                if (scene.params.count("reactive_base_right_x")) {
                    GpuDriverState::setSingleParam("reactive_base_right_x", scene.params.at("reactive_base_right_x"));
                    printf("    reactive_base_right_x = %.2f\n", scene.params.at("reactive_base_right_x"));
                }
                if (scene.params.count("reactive_base_right_y")) {
                    GpuDriverState::setSingleParam("reactive_base_right_y", scene.params.at("reactive_base_right_y"));
                    printf("    reactive_base_right_y = %.2f\n", scene.params.at("reactive_base_right_y"));
                }
                if (scene.params.count("reactive_left_flip_x")) {
                    GpuDriverState::setSingleParam("reactive_left_flip_x", scene.params.at("reactive_left_flip_x"));
                    printf("    reactive_left_flip_x = %.2f\n", scene.params.at("reactive_left_flip_x"));
                }
                if (scene.params.count("reactive_right_flip_x")) {
                    GpuDriverState::setSingleParam("reactive_right_flip_x", scene.params.at("reactive_right_flip_x"));
                    printf("    reactive_right_flip_x = %.2f\n", scene.params.at("reactive_right_flip_x"));
                }
                if (scene.params.count("reactive_left_rot_offset")) {
                    GpuDriverState::setSingleParam("reactive_left_rot_offset", scene.params.at("reactive_left_rot_offset"));
                    printf("    reactive_left_rot_offset = %.2f\n", scene.params.at("reactive_left_rot_offset"));
                }
                if (scene.params.count("reactive_right_rot_offset")) {
                    GpuDriverState::setSingleParam("reactive_right_rot_offset", scene.params.at("reactive_right_rot_offset"));
                    printf("    reactive_right_rot_offset = %.2f\n", scene.params.at("reactive_right_rot_offset"));
                }
                
                // Reset smoothed positions/rotations now that offsets are loaded
                GpuDriverState::resetReactiveEyesSmooth();
                printf("  Reactive Eyes: Params applied and smoothed positions reset\n");
            }
            
            // Handle sprite upload if scene uses a sprite
            // IMPORTANT: Clear ALL sprite RAM first to avoid accumulation when switching scenes
            SystemAPI::Web::HttpServer::clearAllSpriteRam();
            printf("  [Memory] Cleared all sprite RAM before loading new sprite\n");
            
            if (scene.spriteId >= 0) {
                auto* sprite = SystemAPI::Web::HttpServer::findSpriteById(scene.spriteId);
                if (sprite && !sprite->pixelData.empty()) {
                    printf("  Uploading scene sprite %d to GPU (size=%dx%d, pixels=%zu)...\n", 
                           scene.spriteId, sprite->width, sprite->height, sprite->pixelData.size());
                    uint8_t gpuSpriteId = 0;
                    GpuDriverState::getGpu().deleteSprite(gpuSpriteId);
                    vTaskDelay(pdMS_TO_TICKS(10));
                    
                    // GpuCommands signature: id, data, width, height
                    if (GpuDriverState::getGpu().uploadSprite(gpuSpriteId,
                                                              sprite->pixelData.data(),
                                                              sprite->width,
                                                              sprite->height)) {
                        vTaskDelay(pdMS_TO_TICKS(100));
                        GpuDriverState::setSpriteScene(gpuSpriteId, 64.0f, 16.0f, 0.0f, scene.bgR, scene.bgG, scene.bgB);
                        printf("  Sprite uploaded to GPU slot 0, spriteReady=true\n");
                        
                        // Clear sprite RAM after successful upload to GPU
                        SystemAPI::Web::HttpServer::clearSpriteRam(scene.spriteId);
                        printf("  [Memory] Sprite %d RAM cleared after GPU upload\n", scene.spriteId);
                    } else {
                        printf("  ERROR: Sprite upload to GPU failed!\n");
                    }
                } else {
                    printf("  WARNING: Sprite %d not found or has no pixel data\n", scene.spriteId);
                }
            } else {
                // No sprite selected - clear existing sprite from GPU and stop sprite rendering
                printf("  No sprite selected (spriteId=%d), clearing sprite scene\n", scene.spriteId);
                GpuDriverState::getGpu().deleteSprite(0);
                vTaskDelay(pdMS_TO_TICKS(10));
                GpuDriverState::clearSpriteScene();
            }
        } else {
            // Display disabled - show nothing or just LEDs
            GpuDriverState::setSceneAnimation("none");
        }
        
        // Apply shader params from scene.params (stored from Advanced editor)
        // These are stored with "shader_" prefix in the params map
        // IMPORTANT: Reset ALL shader params to defaults when switching scenes
        // This ensures no leftover config from previous scene affects the new one
        printf("  [Shader] Resetting ALL shader params to defaults...\n");
        
        // Set flag to prevent saving reset values back to scene
        GpuDriverState::loadingSceneParams = true;
        
        // Reset shader type and basic params
        GpuDriverState::setSingleParam("shader_type", 0.0f);  // Reset to NONE
        GpuDriverState::setSingleParam("shader_invert", 0.0f);
        GpuDriverState::setSingleParam("shader_mask_enabled", 1.0f);  // Default on
        GpuDriverState::setSingleParam("shader_mask_r", 0.0f);
        GpuDriverState::setSingleParam("shader_mask_g", 0.0f);
        GpuDriverState::setSingleParam("shader_mask_b", 0.0f);
        GpuDriverState::setSingleParam("shader_override_r", 255.0f);
        GpuDriverState::setSingleParam("shader_override_g", 255.0f);
        GpuDriverState::setSingleParam("shader_override_b", 255.0f);
        
        // Reset gradient params
        GpuDriverState::setSingleParam("shader_gradient_distance", 20.0f);
        GpuDriverState::setSingleParam("shader_gradient_angle", 0.0f);
        GpuDriverState::setSingleParam("shader_gradient_mirror", 0.0f);
        
        // Reset glitch params
        GpuDriverState::setSingleParam("shader_glitch_speed", 50.0f);
        GpuDriverState::setSingleParam("shader_glitch_intensity", 30.0f);
        GpuDriverState::setSingleParam("shader_glitch_chromatic", 20.0f);
        GpuDriverState::setSingleParam("shader_glitch_quantity", 50.0f);
        
        // Reset hue cycle params
        GpuDriverState::setSingleParam("shader_hue_speed", 1000.0f);
        GpuDriverState::setSingleParam("shader_hue_color_count", 5.0f);
        // Reset palette colors to defaults
        for (int i = 0; i < 32; i++) {
            char rKey[32], gKey[32], bKey[32];
            snprintf(rKey, sizeof(rKey), "shader_hue_color_%d_r", i);
            snprintf(gKey, sizeof(gKey), "shader_hue_color_%d_g", i);
            snprintf(bKey, sizeof(bKey), "shader_hue_color_%d_b", i);
            // Default: first 5 colors are rainbow, rest red
            uint8_t r = 255, g = 0, b = 0;
            if (i == 1) { r = 255; g = 255; b = 0; }
            else if (i == 2) { r = 0; g = 255; b = 0; }
            else if (i == 3) { r = 0; g = 0; b = 255; }
            else if (i == 4) { r = 128; g = 0; b = 255; }
            GpuDriverState::setSingleParam(rKey, (float)r);
            GpuDriverState::setSingleParam(gKey, (float)g);
            GpuDriverState::setSingleParam(bKey, (float)b);
        }
        
        printf("  [Shader] All params reset to defaults\n");
        
        // Apply dedicated shader fields from scene struct (fallback if params don't have them)
        // These are the fields saved/loaded by saveScenesStorage/loadScenesFromStorage
        if (scene.shaderInvert) {
            GpuDriverState::setSingleParam("shader_invert", 1.0f);
            printf("  [Shader] Invert: ON (from scene.shaderInvert)\n");
        }
        
        // Apply shaderColorMode: solid=1, hue_cycle=2, gradient_cycle=3, glitch=4
        // Track if shaderColorMode explicitly sets a shader type
        // NOTE: "none" and empty mean "use params" - they don't force shader_type
        bool shaderColorModeExplicit = false;
        if (scene.shaderColorMode == "solid") {
            GpuDriverState::setSingleParam("shader_type", 1.0f);
            printf("  [Shader] Type: SOLID (from scene.shaderColorMode)\n");
            shaderColorModeExplicit = true;
        } else if (scene.shaderColorMode == "rainbow" || scene.shaderColorMode == "hue_cycle") {
            GpuDriverState::setSingleParam("shader_type", 2.0f);
            printf("  [Shader] Type: HUE_CYCLE (from scene.shaderColorMode)\n");
            shaderColorModeExplicit = true;
        } else if (scene.shaderColorMode == "gradient" || scene.shaderColorMode == "gradient_cycle") {
            GpuDriverState::setSingleParam("shader_type", 3.0f);
            printf("  [Shader] Type: GRADIENT_CYCLE (from scene.shaderColorMode)\n");
            shaderColorModeExplicit = true;
        } else if (scene.shaderColorMode == "glitch") {
            GpuDriverState::setSingleParam("shader_type", 4.0f);
            printf("  [Shader] Type: GLITCH (from scene.shaderColorMode)\n");
            shaderColorModeExplicit = true;
        }
        // "none" or empty shaderColorMode = let params control shader_type
        
        // Parse shaderColor hex string to RGB for solid color mode
        if (!scene.shaderColor.empty() && scene.shaderColor[0] == '#' && scene.shaderColor.length() >= 7) {
            // Parse hex color properly - extract 2 chars at a time
            const char* hex = scene.shaderColor.c_str() + 1;  // Skip '#'
            char rHex[3] = {hex[0], hex[1], 0};
            char gHex[3] = {hex[2], hex[3], 0};
            char bHex[3] = {hex[4], hex[5], 0};
            int r = (int)strtol(rHex, NULL, 16);
            int g = (int)strtol(gHex, NULL, 16);
            int b = (int)strtol(bHex, NULL, 16);
            GpuDriverState::setSingleParam("shader_override_r", (float)r);
            GpuDriverState::setSingleParam("shader_override_g", (float)g);
            GpuDriverState::setSingleParam("shader_override_b", (float)b);
            printf("  [Shader] Color: %s -> RGB(%d,%d,%d)\n", scene.shaderColor.c_str(), r, g, b);
        }
        
        // Debug: Log all shader params in this scene (overrides from Advanced editor)
        printf("  [Shader] Scene params containing 'shader_': ");
        for (const auto& kv : scene.params) {
            if (kv.first.find("shader_") == 0) {
                printf("%s=%.2f ", kv.first.c_str(), kv.second);
            }
        }
        printf("\n");
        
        // Override with explicit params if present (from Advanced editor or API)
        // BUT only if shaderColorMode wasn't explicitly set - shaderColorMode takes priority
        if (!shaderColorModeExplicit && scene.params.count("shader_type")) {
            float shaderTypeVal = scene.params.at("shader_type");
            GpuDriverState::setSingleParam("shader_type", shaderTypeVal);
            printf("  Shader Type Override: %d (from params, no shaderColorMode)\n", (int)shaderTypeVal);
        }
        if (scene.params.count("shader_invert")) {
            GpuDriverState::setSingleParam("shader_invert", scene.params.at("shader_invert"));
        }
        if (scene.params.count("shader_mask_enabled")) {
            GpuDriverState::setSingleParam("shader_mask_enabled", scene.params.at("shader_mask_enabled"));
        }
        if (scene.params.count("shader_mask_r")) {
            GpuDriverState::setSingleParam("shader_mask_r", scene.params.at("shader_mask_r"));
        }
        if (scene.params.count("shader_mask_g")) {
            GpuDriverState::setSingleParam("shader_mask_g", scene.params.at("shader_mask_g"));
        }
        if (scene.params.count("shader_mask_b")) {
            GpuDriverState::setSingleParam("shader_mask_b", scene.params.at("shader_mask_b"));
        }
        // Hue/Gradient cycle speed (check both naming conventions)
        if (scene.params.count("shader_hue_cycle_speed")) {
            GpuDriverState::setSingleParam("shader_hue_speed", scene.params.at("shader_hue_cycle_speed"));
        }
        if (scene.params.count("shader_hue_speed")) {
            GpuDriverState::setSingleParam("shader_hue_speed", scene.params.at("shader_hue_speed"));
        }
        // Hue color count and palette
        if (scene.params.count("shader_hue_color_count")) {
            GpuDriverState::setSingleParam("shader_hue_color_count", scene.params.at("shader_hue_color_count"));
        }
        for (int i = 0; i < 32; i++) {
            char rKey[32], gKey[32], bKey[32];
            snprintf(rKey, sizeof(rKey), "shader_hue_color_%d_r", i);
            snprintf(gKey, sizeof(gKey), "shader_hue_color_%d_g", i);
            snprintf(bKey, sizeof(bKey), "shader_hue_color_%d_b", i);
            if (scene.params.count(rKey)) GpuDriverState::setSingleParam(rKey, scene.params.at(rKey));
            if (scene.params.count(gKey)) GpuDriverState::setSingleParam(gKey, scene.params.at(gKey));
            if (scene.params.count(bKey)) GpuDriverState::setSingleParam(bKey, scene.params.at(bKey));
        }
        // Gradient params
        if (scene.params.count("shader_gradient_distance")) {
            GpuDriverState::setSingleParam("shader_gradient_distance", scene.params.at("shader_gradient_distance"));
        }
        if (scene.params.count("shader_gradient_angle")) {
            GpuDriverState::setSingleParam("shader_gradient_angle", scene.params.at("shader_gradient_angle"));
        }
        if (scene.params.count("shader_gradient_mirror")) {
            GpuDriverState::setSingleParam("shader_gradient_mirror", scene.params.at("shader_gradient_mirror"));
        }
        // Glitch params
        if (scene.params.count("shader_glitch_speed")) {
            GpuDriverState::setSingleParam("shader_glitch_speed", scene.params.at("shader_glitch_speed"));
        }
        if (scene.params.count("shader_glitch_intensity")) {
            GpuDriverState::setSingleParam("shader_glitch_intensity", scene.params.at("shader_glitch_intensity"));
        }
        if (scene.params.count("shader_glitch_chromatic")) {
            GpuDriverState::setSingleParam("shader_glitch_chromatic", scene.params.at("shader_glitch_chromatic"));
        }
        if (scene.params.count("shader_glitch_quantity")) {
            GpuDriverState::setSingleParam("shader_glitch_quantity", scene.params.at("shader_glitch_quantity"));
        }
        
        // Done loading scene params - clear flag to allow saving
        GpuDriverState::loadingSceneParams = false;
        
        // Base values are now stored by setMirroredParams() and setStaticParams()
        // The MicReactivityManager handles all mic reactive calculations
        printf("  [MicReact] Base values stored via setMirroredParams/setStaticParams\n");
        
        // Force sync shader to GPU after applying all scene params
        // This ensures the GPU gets the complete shader config immediately
        GpuDriverState::forceShaderSync();
        printf("  [Shader] Forced sync to GPU after scene activation\n");
        
        // LED control removed - LEDs are now managed independently of scenes
    });
    
    // Callback for live scene parameter updates - updates GpuDriverState when sliders change
    httpServer.setSceneUpdatedCallback([](const SystemAPI::Web::SavedScene& scene) {
        printf("\n*** SCENE UPDATED CALLBACK ***\n");
        printf("  Scene ID: %d, AnimType: '%s'\n", scene.id, scene.animType.c_str());
        printf("  Current AnimMode BEFORE: %d, mirrorEnabled BEFORE: %s\n", 
               (int)GpuDriverState::getSceneAnimMode(), GpuDriverState::isMirrorEnabled() ? "YES" : "NO");
        printf("  Background: RGB(%d,%d,%d)\n", scene.bgR, scene.bgG, scene.bgB);
        printf("  Params count: %zu\n", scene.params.size());
        for (const auto& p : scene.params) {
            printf("    %s = %.2f\n", p.first.c_str(), p.second);
        }
        
        // Set background color
        GpuDriverState::setBackgroundColor(scene.bgR, scene.bgG, scene.bgB);
        
        // Only update animation mode if animType is valid and different from current
        // Map animType string to expected SceneAnimMode (use fully qualified name in lambda)
        using SAM = GpuDriverState::SceneAnimMode;
        [[maybe_unused]] SAM expectedMode = SAM::STATIC_IMAGE;  // For debug prints
        bool mirrorEnabled = false;
        
        if (scene.animType == "static_mirrored") {
            expectedMode = SAM::STATIC_MIRRORED;
            mirrorEnabled = true;
            printf("  -> Matched 'static_mirrored': mode=STATIC_MIRRORED, mirror=true\n");
        } else if (scene.animType == "static_sprite" || scene.animType == "static_image" || scene.animType == "static") {
            expectedMode = SAM::STATIC_IMAGE;
            mirrorEnabled = false;  // Single sprite mode
            printf("  -> Matched '%s': mode=STATIC_IMAGE, mirror=false\n", scene.animType.c_str());
        } else if (scene.animType == "reactive_eyes") {
            expectedMode = SAM::REACTIVE_EYES;
            mirrorEnabled = true;  // Reactive eyes always uses left/right sprites
            printf("  -> Matched 'reactive_eyes': mode=REACTIVE_EYES, mirror=true\n");
        } else if (scene.animType == "gyro_eyes") {
            expectedMode = SAM::GYRO_EYES;
            printf("  -> Matched 'gyro_eyes'\n");
        } else if (scene.animType == "sway") {
            expectedMode = SAM::SWAY;
            printf("  -> Matched 'sway'\n");
        } else if (scene.animType == "none" || scene.animType.empty()) {
            expectedMode = SAM::NONE;
            printf("  -> Matched 'none' or empty\n");
        } else {
            printf("  -> No match for animType '%s', using defaults\n", scene.animType.c_str());
        }
        
        // Always update mirror flag based on animation type selection
        printf("  -> Calling setMirrorEnabled(%s)\n", mirrorEnabled ? "true" : "false");
        GpuDriverState::setMirrorEnabled(mirrorEnabled);
        printf("  -> After setMirrorEnabled, mirrorEnabled now: %s\n", GpuDriverState::isMirrorEnabled() ? "YES" : "NO");
        
        // Always call setSceneAnimation to update mode
        printf("  -> Calling setSceneAnimation('%s')\n", scene.animType.c_str());
        GpuDriverState::setSceneAnimation(scene.animType);
        printf("  -> After setSceneAnimation, mode=%d, mirror=%s\n", 
               (int)GpuDriverState::getSceneAnimMode(), GpuDriverState::isMirrorEnabled() ? "YES" : "NO");
        
        // Apply animation-specific params based on current rendering mode (not scene.animType)
        // This ensures params go to the right variables regardless of animType string
        SAM currentMode = GpuDriverState::getSceneAnimMode();
        
        if (currentMode == SAM::STATIC_IMAGE) {
            float posX = scene.params.count("center_x") ? scene.params.at("center_x") : 
                         (scene.params.count("x") ? scene.params.at("x") : 64.0f);
            float posY = scene.params.count("center_y") ? scene.params.at("center_y") : 
                         (scene.params.count("y") ? scene.params.at("y") : 16.0f);
            float rot = scene.params.count("rotation") ? scene.params.at("rotation") : 0.0f;
            float scale = scene.params.count("scale") ? scene.params.at("scale") : 1.0f;
            bool flipX = scene.params.count("flip_x") ? (scene.params.at("flip_x") > 0.5f) : false;
            printf("  -> setStaticParams(scale=%.2f, rot=%.2f, x=%.2f, y=%.2f)\n", scale, rot, posX, posY);
            GpuDriverState::setStaticParams(scale, rot, posX, posY);
            GpuDriverState::setStaticFlip(flipX);
        }
        else if (currentMode == SAM::STATIC_MIRRORED) {
            float lx = scene.params.count("left_x") ? scene.params.at("left_x") : 32.0f;
            float ly = scene.params.count("left_y") ? scene.params.at("left_y") : 16.0f;
            float lrot = scene.params.count("left_rotation") ? scene.params.at("left_rotation") : 0.0f;
            float lscale = scene.params.count("left_scale") ? scene.params.at("left_scale") : 1.0f;
            bool leftFlip = scene.params.count("left_flip_x") ? (scene.params.at("left_flip_x") > 0.5f) : false;
            float rx = scene.params.count("right_x") ? scene.params.at("right_x") : 96.0f;
            float ry = scene.params.count("right_y") ? scene.params.at("right_y") : 16.0f;
            float rrot = scene.params.count("right_rotation") ? scene.params.at("right_rotation") : 180.0f;
            float rscale = scene.params.count("right_scale") ? scene.params.at("right_scale") : 1.0f;
            bool rightFlip = scene.params.count("right_flip_x") ? (scene.params.at("right_flip_x") > 0.5f) : false;
            printf("  -> setMirroredParams(L: %.0f,%.0f,%.0f,%.2f R: %.0f,%.0f,%.0f,%.2f)\n", 
                   lx, ly, lrot, lscale, rx, ry, rrot, rscale);
            GpuDriverState::setMirroredParams(lx, ly, lrot, lscale, rx, ry, rrot, rscale);
            GpuDriverState::setMirroredFlips(leftFlip, rightFlip);
        }
        else if (currentMode == SAM::GYRO_EYES) {
            float eyeSize = scene.params.count("eye_size") ? scene.params.at("eye_size") : 12.0f;
            float intensity = scene.params.count("intensity") ? scene.params.at("intensity") : 1.0f;
            bool mirror = scene.params.count("mirror") ? (scene.params.at("mirror") > 0.5f) : true;
            printf("  -> setGyroEyeParams(size=%.2f, intensity=%.2f, mirror=%d)\n", eyeSize, intensity, mirror);
            GpuDriverState::setGyroEyeParams(eyeSize, intensity, mirror, scene.spriteId);
        }
        else if (currentMode == SAM::SWAY) {
            float swayX = scene.params.count("sway_x") ? scene.params.at("sway_x") : 10.0f;
            float swayY = scene.params.count("sway_y") ? scene.params.at("sway_y") : 5.0f;
            float rotRange = scene.params.count("rot_range") ? scene.params.at("rot_range") : 15.0f;
            float speed = scene.params.count("speed") ? scene.params.at("speed") : 1.0f;
            printf("  -> setSwayParams(swayX=%.2f, swayY=%.2f)\n", swayX, swayY);
            GpuDriverState::setSwayParams(swayX, swayY, rotRange, speed, false);
        }
        else if (currentMode == SAM::REACTIVE_EYES) {
            // Reset reactive_eyes params to defaults first, then apply saved values
            // This ensures no leftover state from previous scenes
            GpuDriverState::setSingleParam("reactive_y_sensitivity", 5.0f);
            GpuDriverState::setSingleParam("reactive_z_sensitivity", 5.0f);
            GpuDriverState::setSingleParam("reactive_x_sensitivity", 12.0f);
            GpuDriverState::setSingleParam("reactive_rot_sensitivity", 10.0f);
            GpuDriverState::setSingleParam("reactive_smoothing", 0.15f);
            GpuDriverState::setSingleParam("reactive_scale", 1.0f);
            GpuDriverState::setSingleParam("reactive_base_left_x", 32.0f);
            GpuDriverState::setSingleParam("reactive_base_left_y", 16.0f);
            GpuDriverState::setSingleParam("reactive_base_right_x", 96.0f);
            GpuDriverState::setSingleParam("reactive_base_right_y", 16.0f);
            GpuDriverState::setSingleParam("reactive_left_flip_x", 0.0f);
            GpuDriverState::setSingleParam("reactive_right_flip_x", 0.0f);
            GpuDriverState::setSingleParam("reactive_left_rot_offset", 0.0f);
            GpuDriverState::setSingleParam("reactive_right_rot_offset", 0.0f);
            printf("  -> Reset reactive_eyes params to defaults\n");
            
            // Now apply saved values from scene (overrides defaults if present)
            if (scene.params.count("reactive_y_sensitivity")) {
                GpuDriverState::setSingleParam("reactive_y_sensitivity", scene.params.at("reactive_y_sensitivity"));
            }
            if (scene.params.count("reactive_z_sensitivity")) {
                GpuDriverState::setSingleParam("reactive_z_sensitivity", scene.params.at("reactive_z_sensitivity"));
            }
            if (scene.params.count("reactive_x_sensitivity")) {
                GpuDriverState::setSingleParam("reactive_x_sensitivity", scene.params.at("reactive_x_sensitivity"));
            }
            if (scene.params.count("reactive_rot_sensitivity")) {
                GpuDriverState::setSingleParam("reactive_rot_sensitivity", scene.params.at("reactive_rot_sensitivity"));
            }
            if (scene.params.count("reactive_smoothing")) {
                GpuDriverState::setSingleParam("reactive_smoothing", scene.params.at("reactive_smoothing"));
            }
            if (scene.params.count("reactive_scale")) {
                GpuDriverState::setSingleParam("reactive_scale", scene.params.at("reactive_scale"));
            }
            if (scene.params.count("reactive_base_left_x")) {
                GpuDriverState::setSingleParam("reactive_base_left_x", scene.params.at("reactive_base_left_x"));
            }
            if (scene.params.count("reactive_base_left_y")) {
                GpuDriverState::setSingleParam("reactive_base_left_y", scene.params.at("reactive_base_left_y"));
            }
            if (scene.params.count("reactive_base_right_x")) {
                GpuDriverState::setSingleParam("reactive_base_right_x", scene.params.at("reactive_base_right_x"));
            }
            if (scene.params.count("reactive_base_right_y")) {
                GpuDriverState::setSingleParam("reactive_base_right_y", scene.params.at("reactive_base_right_y"));
            }
            if (scene.params.count("reactive_left_flip_x")) {
                GpuDriverState::setSingleParam("reactive_left_flip_x", scene.params.at("reactive_left_flip_x"));
            }
            if (scene.params.count("reactive_left_rot_offset")) {
                GpuDriverState::setSingleParam("reactive_left_rot_offset", scene.params.at("reactive_left_rot_offset"));
            }
            if (scene.params.count("reactive_right_flip_x")) {
                GpuDriverState::setSingleParam("reactive_right_flip_x", scene.params.at("reactive_right_flip_x"));
            }
            if (scene.params.count("reactive_right_rot_offset")) {
                GpuDriverState::setSingleParam("reactive_right_rot_offset", scene.params.at("reactive_right_rot_offset"));
            }
            
            // Reset smoothed positions/rotations now that offsets are loaded
            GpuDriverState::resetReactiveEyesSmooth();
            
            printf("  -> Applied reactive_eyes params from scene\n");
        }
        else {
            printf("  -> Current mode is %d, applying no specific params\n", (int)currentMode);
        }
        
        // Handle shader settings from scene struct
        // Convert shaderColorMode string to shader_type value
        // NOTE: "none" or empty means "don't change shader" - use current/params value
        if (scene.shaderColorMode == "solid") {
            GpuDriverState::setSingleParam("shader_type", 1.0f);
            printf("  -> Set shader_type=1 (SOLID/COLOR_OVERRIDE)\n");
        } else if (scene.shaderColorMode == "rainbow" || scene.shaderColorMode == "hue_cycle") {
            GpuDriverState::setSingleParam("shader_type", 2.0f);
            printf("  -> Set shader_type=2 (HUE_CYCLE)\n");
        } else if (scene.shaderColorMode == "gradient" || scene.shaderColorMode == "gradient_cycle") {
            GpuDriverState::setSingleParam("shader_type", 3.0f);
            printf("  -> Set shader_type=3 (GRADIENT_CYCLE)\n");
        } else if (scene.shaderColorMode == "glitch") {
            GpuDriverState::setSingleParam("shader_type", 4.0f);
            printf("  -> Set shader_type=4 (GLITCH)\n");
        }
        // "none" or empty = don't touch shader_type, let params or current value apply
        
        // Handle shaderInvert
        GpuDriverState::setSingleParam("shader_invert", scene.shaderInvert ? 1.0f : 0.0f);
        
        // Parse shaderColor hex string to RGB for solid color mode
        if (!scene.shaderColor.empty() && scene.shaderColor[0] == '#' && scene.shaderColor.length() >= 7) {
            // Parse hex color properly - extract 2 chars at a time
            const char* hex = scene.shaderColor.c_str() + 1;  // Skip '#'
            char rHex[3] = {hex[0], hex[1], 0};
            char gHex[3] = {hex[2], hex[3], 0};
            char bHex[3] = {hex[4], hex[5], 0};
            int r = (int)strtol(rHex, NULL, 16);
            int g = (int)strtol(gHex, NULL, 16);
            int b = (int)strtol(bHex, NULL, 16);
            GpuDriverState::setSingleParam("shader_override_r", (float)r);
            GpuDriverState::setSingleParam("shader_override_g", (float)g);
            GpuDriverState::setSingleParam("shader_override_b", (float)b);
            printf("  -> Set shader override color: %s -> RGB(%d,%d,%d)\n", scene.shaderColor.c_str(), r, g, b);
        }
        
        printf("*** END CALLBACK ***\n\n");
    });
    
    // Single param callback - for live slider updates without resetting other values
    httpServer.setSingleParamCallback([](const char* paramName, float value) {
        // Just update the single param directly
        GpuDriverState::setSingleParam(paramName, value);
    });
    
    // LED Preset activation callback - applies LED preset to LED strips
    httpServer.setLedPresetActivatedCallback([](const SystemAPI::Web::SavedLedPreset& preset) {
        printf("\n  ========================================\n");
        printf("  LED PRESET ACTIVATED: %s (id=%d)\n", preset.name.c_str(), preset.id);
        printf("  Animation: %s\n", preset.animation.c_str());
        printf("  Color: RGB(%d,%d,%d), ColorCount: %d\n", preset.r, preset.g, preset.b, preset.colorCount);
        printf("  Brightness: %d, Speed: %d\n", preset.brightness, preset.speed);
        printf("  ========================================\n\n");
        
        // Build color palette from preset
        std::vector<std::tuple<uint8_t, uint8_t, uint8_t>> colors;
        
        if (preset.colors.size() > 0) {
            // Use the color array from preset
            colors = preset.colors;
        } else {
            // Fall back to single color
            colors.push_back(std::make_tuple(preset.r, preset.g, preset.b));
        }
        
        // Start the animation with palette
        SystemAPI::Testing::LedStripTestHarness::startAnimationWithPalette(
            preset.animation, colors, (uint8_t)preset.speed, (uint8_t)preset.brightness
        );
        
        printf("  Applied LED animation: %s with %zu colors, speed=%d\n", 
               preset.animation.c_str(), colors.size(), preset.speed);
    });
    
    printf("  Web-GPU Callbacks: Registered\n");
    
    // =====================================================
    // Wire up OLED Menu System -> HttpServer Scene Activation
    // This allows the OLED buttons to activate scenes from the preset list
    // =====================================================
    {
        // Set callback: when OLED user selects a preset, activate that scene
        OledMenu::OledMenuSystem::instance().setSceneActivateCallback([](int sceneId) {
            printf("OLED_MENU: Scene activation callback - id=%d\n", sceneId);
            SystemAPI::Web::HttpServer::instance().activateSceneById(sceneId);
            // Update OLED to show this as the active scene
            OledMenu::OledMenuSystem::instance().setActiveSceneId(sceneId);
        });
        
        // Build scene list for OLED (id, name pairs)
        const auto& scenes = httpServer.getSavedScenes();
        std::vector<std::pair<int, std::string>> sceneList;
        sceneList.reserve(scenes.size());
        for (const auto& scene : scenes) {
            sceneList.emplace_back(scene.id, scene.name);
        }
        OledMenu::OledMenuSystem::instance().setAvailableScenes(sceneList);
        
        // Set the initial active scene ID
        OledMenu::OledMenuSystem::instance().setActiveSceneId(SystemAPI::Web::activeSceneId_);
        
        printf("  OLED-Scene Callback: Registered (%zu scenes, activeId=%d)\n", 
               sceneList.size(), SystemAPI::Web::activeSceneId_);
    }
    
    // =====================================================
    // Wire up OLED Menu System -> HttpServer LED Preset Activation
    // This allows the OLED buttons to activate LED presets
    // =====================================================
    {
        // Set callback: when OLED user selects a LED preset, activate it
        OledMenu::OledMenuSystem::instance().setLedPresetActivateCallback([](int presetId) {
            printf("OLED_MENU: LED preset activation callback - id=%d\n", presetId);
            SystemAPI::Web::HttpServer::instance().activateLedPresetById(presetId);
            // Update OLED to show this as the active LED preset
            OledMenu::OledMenuSystem::instance().setActiveLedPresetId(presetId);
        });
        
        // Build LED preset list for OLED (id, name pairs)
        const auto& ledPresets = httpServer.getSavedLedPresets();
        std::vector<std::pair<int, std::string>> ledPresetList;
        ledPresetList.reserve(ledPresets.size());
        for (const auto& preset : ledPresets) {
            ledPresetList.emplace_back(preset.id, preset.name);
        }
        OledMenu::OledMenuSystem::instance().setAvailableLedPresets(ledPresetList);
        
        // Set the initial active LED preset ID
        OledMenu::OledMenuSystem::instance().setActiveLedPresetId(SystemAPI::Web::activeLedPresetId_);
        
        printf("  OLED-LED Preset Callback: Registered (%zu LED presets, activeId=%d)\n", 
               ledPresetList.size(), SystemAPI::Web::activeLedPresetId_);
    }
    
    // Auto-activate the saved scene from storage (restores last state on boot)
    httpServer.autoActivateSavedScene();
    
    // Update OLED active scene ID after auto-activate (in case it changed)
    OledMenu::OledMenuSystem::instance().setActiveSceneId(SystemAPI::Web::activeSceneId_);
    
    // Print sprite storage summary
    {
        auto& httpServer = SystemAPI::Web::HttpServer::instance();
        const auto& sprites = httpServer.getSprites();
        printf("\n  ┌────────────────────────────────────┐\n");
        printf("  │   SPRITE STORAGE SUMMARY           │\n");
        printf("  └────────────────────────────────────┘\n");
        printf("  Total Sprites Loaded: %zu\n", sprites.size());
        int builtIn = 0, storage = 0;
        for (const auto& sp : sprites) {
            if (sp.id < 100) builtIn++;  // IDs 0-99 are built-in
            else storage++;               // IDs 100+ are from storage
        }
        printf("  Built-in Sprites: %d\n", builtIn);
        printf("  From Storage: %d\n", storage);
        if (!sprites.empty()) {
            printf("  Sprite List:\n");
            for (const auto& sp : sprites) {
                printf("    [%d] %s (%dx%d, %zu bytes)%s\n", 
                       sp.id, sp.name.c_str(), sp.width, sp.height, sp.pixelData.size(),
                       sp.id >= 100 ? " [SAVED]" : "");
            }
        }
        printf("\n");
    }
    
    // Print initial credentials
    auto& security = arcos::security::SecurityDriver::instance();
    printf("  WiFi SSID: %s\n", security.getSSID());
    printf("  WiFi Pass: %s\n", security.getPassword());
    printf("  Portal IP: 192.168.4.1\n");
    printf("  Easy URL:  Type ANY domain (e.g. go.to, a.a)\n");
    printf("\n");
    
    // Initialize Scene Test Harness
    SystemAPI::Testing::SceneTestHarness::init();
    
    // Initialize LED Strip Test Harness
    SystemAPI::Testing::LedStripTestHarness::init();
    
    // LED test auto-start disabled - use LED:FULL command to run tests manually
    // ESP_LOGI("LED_TEST", "LED strip test will auto-start in 10 seconds...");
    // vTaskDelay(pdMS_TO_TICKS(10000));
    // SystemAPI::Testing::LedStripTestHarness::runQuickVisualTest();
    
    // Set state query callback for test harness
    SystemAPI::Testing::SceneTestHarness::setStateQueryCallback([]() -> std::string {
        char buf[512];
        snprintf(buf, sizeof(buf),
            "║ Animation Mode:    %d (%s)\n"
            "║ Sandbox Enabled:   %s\n"
            "║ Sprite Ready:      %s\n"
            "║ GPU Connected:     %s\n"
            "║ Active Scene ID:   %d\n"
            "║ Eye Size:          %.1f\n"
            "║ Eye Sensitivity:   %.2f\n"
            "║ Eye Sprite ID:     %d",
            (int)GpuDriverState::getSceneAnimMode(),
            GpuDriverState::getSceneAnimMode() == GpuDriverState::SceneAnimMode::GYRO_EYES ? "GYRO_EYES" :
            GpuDriverState::getSceneAnimMode() == GpuDriverState::SceneAnimMode::STATIC_IMAGE ? "STATIC_IMAGE" :
            GpuDriverState::getSceneAnimMode() == GpuDriverState::SceneAnimMode::STATIC_MIRRORED ? "STATIC_MIRRORED" :
            GpuDriverState::getSceneAnimMode() == GpuDriverState::SceneAnimMode::SWAY ? "SWAY" :
            GpuDriverState::getSceneAnimMode() == GpuDriverState::SceneAnimMode::SDF_MORPH ? "SDF_MORPH" :
            GpuDriverState::getSceneAnimMode() == GpuDriverState::SceneAnimMode::REACTIVE_EYES ? "REACTIVE_EYES" : "NONE",
            GpuDriverState::isSandboxEnabled() ? "YES" : "NO",
            GpuDriverState::isSpriteReady() ? "YES" : "NO",
            GpuDriverState::isConnected() ? "YES" : "NO",
            SystemAPI::Web::activeSceneId_,
            12.0f,  // TODO: expose actual eye params
            1.0f,
            0
        );
        return std::string(buf);
    });
    
    // Set animation change callback for test harness
    SystemAPI::Testing::SceneTestHarness::setAnimationChangeCallback(
        [](const std::string& animType, int spriteId) {
            printf("[TEST] Animation change requested: type='%s', spriteId=%d\n", 
                   animType.c_str(), spriteId);
            GpuDriverState::setSceneAnimation(animType);
        }
    );
    
    m_updateCount = 0;
    m_totalTime = 0;
    m_credentialPrintTime = 0;

    // Initialize SyncState with simulated data
    auto& state = SystemAPI::SYNC_STATE.state();
    state.mode = SystemAPI::SystemMode::RUNNING;
    snprintf(state.statusText, sizeof(state.statusText), "Running");
}

void CurrentMode::onUpdate(uint32_t deltaMs) {
    m_updateCount++;
    m_totalTime += deltaMs;
    m_credentialPrintTime += deltaMs;
    
    // Check for serial test commands (using stdio since we're on USB CDC)
    static char cmdBuffer[128] = {0};
    static int cmdPos = 0;
    
    // Non-blocking check for input from USB serial
    int c = getchar_unlocked();
    while (c != EOF) {
        if (c == '\n' || c == '\r') {
            if (cmdPos > 0) {
                cmdBuffer[cmdPos] = '\0';
                // Process the command - check both test harnesses
                if (strncmp(cmdBuffer, "TEST:", 5) == 0) {
                    SystemAPI::Testing::SceneTestHarness::processCommand(cmdBuffer);
                } else if (strncmp(cmdBuffer, "LED:", 4) == 0) {
                    SystemAPI::Testing::LedStripTestHarness::handleCommand(cmdBuffer);
                }
                cmdPos = 0;
            }
        } else if (cmdPos < 127) {
            cmdBuffer[cmdPos++] = (char)c;
        }
        c = getchar_unlocked();
    }
    
    // Update captive portal (handles DNS, HTTP, WebSocket)
    auto& portal = SystemAPI::Web::CaptivePortal::instance();
    portal.update();
    
    // Update hardware drivers (non-blocking) - using modular drivers
    uint32_t currentTimeMs = (uint32_t)(esp_timer_get_time() / 1000);
    Drivers::GpsDriver::update();
    Drivers::MicDriver::update();
    // IMU now runs in dedicated 100Hz task for better responsiveness
    // Drivers::ImuDriver::update();  // REMOVED - runs in imuTask at 100Hz
    GpuDriverState::update(currentTimeMs);

    // Get SyncState reference
    auto& state = SystemAPI::SYNC_STATE.state();

    // Update fan based on web UI state (using modular driver)
    Drivers::FanDriver::update(state.fanEnabled);
    
    // Update system stats
    state.uptime = esp_timer_get_time() / 1000000; // seconds
    state.freeHeap = esp_get_free_heap_size();
    
    // Smooth CPU and FPS values using exponential moving average to prevent jitter
    static float smoothedCpu = 40.0f;
    static float smoothedFps = 60.0f;
    float targetCpu = 35.0f + (rand() % 200) / 10.0f;
    float targetFps = 58.0f + (rand() % 40) / 10.0f;
    smoothedCpu = smoothedCpu * 0.95f + targetCpu * 0.05f;  // Heavy smoothing
    smoothedFps = smoothedFps * 0.95f + targetFps * 0.05f;
    state.cpuUsage = smoothedCpu;
    state.fps = smoothedFps;
    
    // Environmental sensor from BmeDriver (shares I2C with IMU)
    // Note: BmeDriver::update() already called in OLED menu section
    state.temperature = Drivers::BmeDriver::getTemperature();
    state.humidity = Drivers::BmeDriver::getHumidity();
    state.pressure = Drivers::BmeDriver::getPressure();
    
    // Update IMU from real driver (values in mg and deg/s) - using modular driver
    state.accelX = Drivers::ImuDriver::accelX;
    state.accelY = Drivers::ImuDriver::accelY;
    state.accelZ = Drivers::ImuDriver::accelZ;
    state.gyroX = Drivers::ImuDriver::gyroX;
    state.gyroY = Drivers::ImuDriver::gyroY;
    state.gyroZ = Drivers::ImuDriver::gyroZ;
    
    // Process IMU calibration (accumulate samples if calibrating)
    SystemAPI::Web::HttpServer::processImuCalibration();
    // Apply IMU calibration to get device-frame values
    SystemAPI::Web::HttpServer::applyImuCalibration();
    
    // Update microphone from real driver (rolling average for stability) - using modular driver
    state.micConnected = Drivers::MicDriver::initialized;
    state.micLevel = Drivers::MicDriver::level;
    state.micDb = Drivers::MicDriver::avgDb;  // Use averaged dB for stable display
    
    // Update GPS from real driver (full data including speed, heading, time) - using modular driver
    state.gpsValid = Drivers::GpsDriver::valid;
    state.satellites = Drivers::GpsDriver::satellites;
    state.latitude = Drivers::GpsDriver::latitude;
    state.longitude = Drivers::GpsDriver::longitude;
    state.altitude = Drivers::GpsDriver::altitude;
    state.gpsSpeed = Drivers::GpsDriver::speed;
    state.gpsHeading = Drivers::GpsDriver::heading;
    state.gpsHdop = Drivers::GpsDriver::hdop;
    state.gpsHour = Drivers::GpsDriver::hour;
    state.gpsMinute = Drivers::GpsDriver::minute;
    state.gpsSecond = Drivers::GpsDriver::second;
    state.gpsDay = Drivers::GpsDriver::day;
    state.gpsMonth = Drivers::GpsDriver::month;
    state.gpsYear = Drivers::GpsDriver::year;
    
    // Update GPU connection status
    state.gpuConnected = GpuDriverState::connected;
    
    // Debug: Print stats validity periodically
    static uint32_t lastStatsDebugTime = 0;
    if (state.uptime - lastStatsDebugTime >= 5) {
        lastStatsDebugTime = state.uptime;
        printf("GPU STATS DEBUG: hasValidStats=%d hasValidUptime=%d statsAge=%lu\n",
               g_gpu.hasValidStats(), g_gpu.hasValidUptime(), 
               (unsigned long)g_gpu.getStatsAgeMs());
    }
    
    // Update GPU stats from actual async responses
    if (g_gpu.hasValidStats()) {
        const auto& stats = g_gpu.getCachedStats();
        state.gpuFps = stats.fps;
        state.gpuFreeHeap = stats.freeHeap;
        state.gpuMinHeap = stats.minHeap;
        state.gpuLoad = stats.loadPercent;
        state.gpuTotalFrames = stats.totalFrames;
        state.gpuUptime = stats.uptimeMs;
        state.gpuHub75Ok = stats.hub75Ok;
        state.gpuOledOk = stats.oledOk;
    } else {
        // No valid stats from GPU RX line - show estimated values
        // GPU is connected (TX works), but we can't receive stats
        state.gpuFps = 60.0f;  // Assume target FPS
        state.gpuFreeHeap = 200000;  // Estimate ~200KB free
        state.gpuMinHeap = 150000;
        state.gpuLoad = 30;  // Moderate load estimate
        state.gpuTotalFrames = state.uptime * 30;  // Estimate based on 30fps and uptime
        state.gpuUptime = state.uptime * 1000;  // Use CPU uptime as proxy
        state.gpuHub75Ok = GpuDriverState::connected;
        state.gpuOledOk = GpuDriverState::connected;
    }
    
    // GPU alerts from GpuCommands
    const auto& alerts = g_gpu.getAlertStats();
    state.gpuAlertsReceived = alerts.alertsReceived;
    state.gpuDroppedFrames = alerts.droppedFrames;
    state.gpuBufferOverflows = alerts.bufferOverflows;
    state.gpuBufferWarning = alerts.bufferWarning;
    state.gpuHeapWarning = alerts.heapWarning;
    
    // =====================================================
    // DISABLED: Application Layer Update (conflicts with GpuDriver)
    // =====================================================
    
    // Calculate pitch and roll from accelerometer for future use
    // accelX, accelY, accelZ are in milli-g
    float ax = state.accelX / 1000.0f;
    float ay = state.accelY / 1000.0f;
    float az = state.accelZ / 1000.0f;
    
    // Calculate pitch and roll angles (simplified) - kept for sensor data logging
    float pitch = atan2f(-ax, sqrtf(ay * ay + az * az)) * 180.0f / 3.14159f;
    float roll = atan2f(ay, az) * 180.0f / 3.14159f;
    
    // =====================================================
    // Update AnimationHandler with sensor data
    // =====================================================
    auto& animHandler = Modes::getAnimationHandler();
    if (animHandler.isInitialized()) {
        // Update IMU inputs (pitch, roll, yaw, accel, gyro)
        animHandler.updateSensorInputs(
            pitch, roll, 0.0f,  // yaw not available without magnetometer
            ax, ay, az,
            state.gyroX / 1000.0f, state.gyroY / 1000.0f, state.gyroZ / 1000.0f
        );
        
        // Update GPS inputs
        animHandler.updateGpsInputs(
            state.latitude, state.longitude, state.altitude,
            state.gpsSpeed, state.satellites, state.gpsValid
        );
        
        // Update audio inputs
        animHandler.updateAudioInputs(
            state.micLevel / 100.0f,  // Normalize 0-100 to 0-1
            state.micDb / 100.0f,     // Use dB as peak proxy
            0.0f, 0.0f, 0.0f          // FFT bands not available yet
        );
        
        // Update environment inputs
        animHandler.updateEnvironmentInputs(
            state.temperature, state.humidity, state.pressure
        );
        
        // Update animation system (runs active animation set)
        animHandler.update(deltaMs);
        
        // Render if animation is enabled and GPU connected
        if (animHandler.isAnimationEnabled() && GpuDriverState::connected) {
            animHandler.render();
        }
    }
    
    // =====================================================
    // Update Sandbox Sensors (render happens in GpuDriverState::update)
    // =====================================================
    if (GpuDriverState::isSandboxEnabled()) {
        // Use device gyro (calibrated) X, Y, Z directly
        GpuDriverState::updateSandboxSensors(state.deviceGyroX, state.deviceGyroY, state.deviceGyroZ);
    }
    
    /*
    // DISABLED: Application layer
    // Update eye controller with IMU data for look tracking
    auto& eye = Application::eye();
    
    // Update eye controller from IMU
    eye.updateFromIMU(pitch, roll);
    
    // Update eye controller from audio (for reactive effects)
    eye.updateFromAudio(state.micDb);
    
    // Update application layer (publishes to Core 1)
    Application::update(deltaMs);
    
    // Also publish sensor data to the application buffer
    Application::SensorData sensorData;
    sensorData.accelX = ax;
    sensorData.accelY = ay;
    sensorData.accelZ = az;
    sensorData.gyroX = state.gyroX;
    sensorData.gyroY = state.gyroY;
    sensorData.gyroZ = state.gyroZ;
    sensorData.pitch = pitch;
    sensorData.roll = roll;
    sensorData.temperature = state.temperature;
    sensorData.humidity = state.humidity;
    sensorData.pressure = state.pressure;
    sensorData.latitude = state.latitude;
    sensorData.longitude = state.longitude;
    sensorData.altitude = state.altitude;
    sensorData.speed = state.gpsSpeed;
    sensorData.satellites = state.satellites;
    sensorData.gpsValid = state.gpsValid;
    sensorData.audioLevel = state.micDb;
    sensorData.audioLevelPercent = state.micLevel;
    sensorData.timestampMs = currentTimeMs;
    Application::publishSensorData(sensorData);
    */
    
    // Print credentials every 10 seconds
    if (m_credentialPrintTime >= 10000) {
        auto& security = arcos::security::SecurityDriver::instance();
        printf("  ----------------------------------------\n");
        printf("  WiFi SSID: %s\n", security.getSSID());
        printf("  WiFi Pass: %s\n", security.getPassword());
        printf("  Portal: 192.168.4.1 or type any URL\n");
        printf("  GPS: %s (Sats: %d, RX: %lu bytes)\n", 
               Drivers::GpsDriver::valid ? "Fix" : "Searching", 
               Drivers::GpsDriver::satellites,
               (unsigned long)Drivers::GpsDriver::bytesReceived);
        printf("  GPU: %s\n", GpuDriverState::connected ? "Connected" : "N/C");
        printf("  MIC: %.1f dB (avg)\n", Drivers::MicDriver::avgDb);
        
        // Print sprite summary once (first time after boot) so it's visible in serial
        static bool spriteSummaryPrinted = false;
        if (!spriteSummaryPrinted) {
            spriteSummaryPrinted = true;
            auto& httpServer = SystemAPI::Web::HttpServer::instance();
            const auto& sprites = httpServer.getSprites();
            printf("  ---- SPRITES ----\n");
            printf("  Total: %zu (Built-in: ", sprites.size());
            int builtIn = 0, storage = 0;
            for (const auto& sp : sprites) {
                if (sp.id < 100) builtIn++; else storage++;
            }
            printf("%d, From SD: %d)\n", builtIn, storage);
            if (storage > 0) {
                printf("  Saved sprites from storage:\n");
                for (const auto& sp : sprites) {
                    if (sp.id >= 100) {
                        printf("    [%d] %s (%dx%d)\n", sp.id, sp.name.c_str(), sp.width, sp.height);
                    }
                }
            }
        }
        printf("  ----------------------------------------\n");
        m_credentialPrintTime = 0;
    }
    
    // Example: Print status every 5 seconds
    if (m_totalTime >= 5000) {
        printf("  Update #%lu | Clients: %d\n",
               (unsigned long)m_updateCount,
               portal.getClientCount());
        m_totalTime = 0;
    }
}

void CurrentMode::onStop() {
    printf("  Current mode stopped after %lu updates\\n", (unsigned long)m_updateCount);
    
    // GpuCommands doesn't have explicit shutdown - clear the display instead
    g_gpu.hub75Clear(0, 0, 0);
    g_gpu.hub75Present();
    printf("  GpuCommands shutdown complete\\n");
    
    /*
    // DISABLED: Application layer
    Application::stop();
    Application::shutdown();
    printf("  Application layer shutdown complete\\n");
    */
}

} // namespace Modes
