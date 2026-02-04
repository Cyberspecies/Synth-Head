/*****************************************************************
 * @file HttpServer.hpp
 * @brief HTTP Server for Captive Portal
 * 
 * Implements the HTTP server with URI routing and handlers.
 * 
 * @author ARCOS
 * @version 1.0
 *****************************************************************/

#pragma once

#include "SystemAPI/Web/WebTypes.hpp"
#include "SystemAPI/Web/Interfaces/ICommandHandler.hpp"
#include "SystemAPI/Web/Content/WebContent.hpp"
#include "SystemAPI/Web/Content/PageSdCard.hpp"
#include "SystemAPI/Web/Content/PageDisplayConfig.hpp"
#include "SystemAPI/Misc/SyncState.hpp"
#include "SystemAPI/Security/SecurityDriver.hpp"
#include "SystemAPI/Animation/AnimationConfig.hpp"
#include "SystemAPI/Utils/FileSystemService.hpp"
#include "SystemAPI/Storage/StorageManager.hpp"
#include "SystemAPI/Storage/SdCardManager.hpp"
#include "AnimationSystem/AnimationSystem.hpp"
#include "AnimationSystem/Core/ShaderRegistry.hpp"
#include "AnimationSystem/Core/TransitionRegistry.hpp"
#include "AnimationSystem/Shaders/AllShaders.hpp"
#include "AnimationSystem/Transitions/AllTransitions.hpp"

#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_random.h"
#include "esp_spiffs.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "cJSON.h"

// Socket includes for getpeername
#include "lwip/sockets.h"
#include "lwip/inet.h"

#include <cstring>
#include <cerrno>
#include <cmath>
#include <vector>
#include <map>
#include <functional>
#include <string>
#include <algorithm>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>

namespace SystemAPI {
namespace Web {

static const char* HTTP_TAG = "HttpServer";

/**
 * @brief Saved sprite metadata with lazy-loaded pixel data
 * Pixel data and previews are loaded from SD card on-demand to save RAM
 */
struct SavedSprite {
    int id = 0;
    std::string name;
    int width = 64;
    int height = 32;
    int scale = 100;
    std::string preview;  // base64 PNG thumbnail (cleared after save to SD)
    std::vector<uint8_t> pixelData;  // Raw RGB888 pixel data (cleared after save to SD)
    bool uploadedToGpu = false;      // Track if sprite is in GPU cache
    bool savedToSd = false;          // True if pixel data is on SD card (lazy load)
};

/**
 * @brief Variable definition for equations
 */
struct EquationVariable {
    std::string name;
    std::string type;   // "static", "sensor", "equation"
    std::string value;  // static value, sensor id, or equation id
};

/**
 * @brief Saved equation definition
 */
struct SavedEquation {
    int id = 0;
    std::string name;
    std::string expression;
    std::vector<EquationVariable> variables;
};

/**
 * @brief Gyro Eye scene configuration
 * Tracks pupil position using device pitch/roll
 * Formula: pupil_x = center_x + (roll * max_offset * intensity) + eye_offset
 *          pupil_y = center_y + (pitch * max_offset * intensity)
 */
struct GyroEyeSceneConfig {
    int spriteId = -1;           // Sprite for pupil (-1 = use circle)
    float intensity = 1.0f;      // Movement multiplier
    float maxOffsetX = 8.0f;     // Max horizontal pixel offset
    float maxOffsetY = 6.0f;     // Max vertical pixel offset
    float smoothingFactor = 0.15f; // Low-pass filter strength
    int eyeOffset = 0;           // Offset between left/right eyes
    int leftEyeCenterX = 32;     // Left eye center X
    int leftEyeCenterY = 16;     // Left eye center Y
    int rightEyeCenterX = 96;    // Right eye center X
    int rightEyeCenterY = 16;    // Right eye center Y
    bool invertPitch = false;    // Invert up/down
    bool invertRoll = false;     // Invert left/right
    uint8_t bgR = 0, bgG = 0, bgB = 0;  // Background color
};

/**
 * @brief Static sprite scene configuration
 */
struct StaticSpriteSceneConfig {
    int spriteId = 0;
    int posX = 0;
    int posY = 0;
    uint8_t bgR = 0, bgG = 0, bgB = 0;
};

/**
 * @brief Saved scene definition
 * Scene Types: 0=NONE, 1=GYRO_EYES, 2=STATIC_SPRITE, 3=ANIMATED
 * AnimType: static_image, gyro_eyes
 * Transition: none, sdf_morph, glitch, particles
 */
struct SavedScene {
    int id = 0;
    std::string name;
    int type = 0;
    bool active = false;
    
    // Modular scene system - LED and Display are independent
    bool displayEnabled = true;   // Controls HUB75 displays
    bool ledsEnabled = false;     // Controls WS2812 LEDs
    bool effectsOnly = false;     // Effect overlay (no primary animation)
    int order = 0;                // For drag-drop reordering
    
    // Background color for display
    uint8_t bgR = 0, bgG = 0, bgB = 0;
    
    // New AnimationSystem fields
    std::string animType = "static_sprite";  // Animation type ID: static_sprite (only static for now)
    std::string transition = "none";     // Transition type: none, sdf, glitch, particle
    int spriteId = -1;          // Overlay sprite ID (-1 = none)
    bool mirrorSprite = false;  // Mirror sprite on right panel
    std::map<std::string, float> params;  // Animation-specific parameters
    
    // Shader settings
    bool shaderAA = true;
    bool shaderInvert = false;
    std::string shaderColorMode = "none"; // none, solid, rainbow
    std::string shaderColor = "#ffffff";
    
    // LED settings
    uint8_t ledR = 255;
    uint8_t ledG = 0;
    uint8_t ledB = 255;
    uint8_t ledBrightness = 80;
    
    // Effects (constant overlays like glitch, scanlines, color_shift)
    struct Effect {
        bool enabled = false;
        float intensity = 0.5f;
    };
    std::map<std::string, Effect> effects;
    
    // Legacy config (kept for backwards compatibility)
    bool hasGyroEyeConfig = false;
    bool hasStaticSpriteConfig = false;
    GyroEyeSceneConfig gyroEye;
    StaticSpriteSceneConfig staticSprite;
};

// Sprite storage (persisted to SD card)
// Use inline for C++17 to ensure single definition across translation units
inline std::vector<SavedSprite> savedSprites_;
inline int nextSpriteId_ = 100;  // Start at 100 so user sprites are "From Storage" (IDs < 100 would be built-in)

// Equation storage (persisted to SD card)
inline std::vector<SavedEquation> savedEquations_;
inline int nextEquationId_ = 1;

// Scene storage (persisted to SD card)
inline std::vector<SavedScene> savedScenes_;
inline int nextSceneId_ = 1;
inline int activeSceneId_ = -1;
// Note: Scene callbacks moved to HttpServer class instance members

// LED Preset storage (persisted to SD card)
struct SavedLedPreset {
    int id;
    std::string name;
    std::string animation;  // solid, breathe, rainbow, pulse, chase, sparkle, fire, wave, gradient
    uint8_t r = 255;        // Primary color (or first color)
    uint8_t g = 255;
    uint8_t b = 255;
    uint8_t brightness = 100;
    int8_t speed = 50;      // Speed can be negative for reverse animations
    int colorCount = 1;     // Number of colors for multi-color animations
    std::vector<std::tuple<uint8_t, uint8_t, uint8_t>> colors;  // Array of (r, g, b) colors
    std::map<std::string, int> params;  // Extra animation-specific parameters
    int order = 0;
};
inline std::vector<SavedLedPreset> savedLedPresets_;
inline int nextLedPresetId_ = 1;
inline int activeLedPresetId_ = -1;

inline bool spiffs_initialized_ = false;  // Still used for legacy SPIFFS
inline bool sdcard_storage_ready_ = false;

// SD Card paths (primary storage)
static const char* SPRITE_DIR = "/sdcard/Sprites";
static const char* SPRITE_INDEX_FILE = "/sdcard/Sprites/index.dat";
static const char* EQUATION_DIR = "/sdcard/Equations";
static const char* EQUATION_INDEX_FILE = "/sdcard/Equations/index.json";
static const char* SCENE_DIR = "/sdcard/Scenes";
static const char* SCENE_INDEX_FILE = "/sdcard/Scenes/index.json";
static const char* LED_PRESET_DIR = "/sdcard/LedPresets";
static const char* LED_PRESET_INDEX_FILE = "/sdcard/LedPresets/index.json";

// Legacy SPIFFS paths (fallback)
static const char* SPRITE_DIR_SPIFFS = "/spiffs/Sprites";
static const char* SPRITE_INDEX_FILE_SPIFFS = "/spiffs/Sprites/index.json";
static const char* EQUATION_INDEX_FILE_SPIFFS = "/spiffs/Equations.json";
static const char* SCENE_INDEX_FILE_SPIFFS = "/spiffs/Scenes.json";

/**
 * @brief HTTP Server for Web Portal
 * 
 * Handles all HTTP requests including API endpoints,
 * static content, and captive portal detection.
 */
class HttpServer {
public:
    using CommandCallback = std::function<void(CommandType, cJSON*)>;
    
    /**
     * @brief Get singleton instance
     */
    static HttpServer& instance() {
        static HttpServer inst;
        return inst;
    }
    
    /**
     * @brief Start the HTTP server
     * @return true on success
     */
    bool start() {
        if (server_) return true;
        
        // Initialize SD card storage (primary storage for sprites/equations)
        initSdCardStorage();
        
        // Initialize SPIFFS as fallback only if SD card not ready
        if (!sdcard_storage_ready_) {
            initSpiffs();
        }
        
        // Load saved sprites, equations, and scenes from SD card (or SPIFFS fallback)
        loadSpritesFromStorage();
        loadEquationsFromStorage();
        loadScenesFromStorage();
        loadLedPresetsFromStorage();
        
        // Create default scene if none exist (ensures animation list is never empty)
        if (savedScenes_.empty()) {
            ESP_LOGI(HTTP_TAG, "No scenes found, creating default scene");
            createFallbackDefaultScene();
        }
        
        httpd_config_t config = HTTPD_DEFAULT_CONFIG();
        config.max_uri_handlers = 80;
        config.stack_size = 8192;
        config.lru_purge_enable = true;
        config.uri_match_fn = httpd_uri_match_wildcard;
        
        if (httpd_start(&server_, &config) != ESP_OK) {
            ESP_LOGE(HTTP_TAG, "Failed to start HTTP server");
            return false;
        }
        
        registerHandlers();
        
        ESP_LOGI(HTTP_TAG, "HTTP server started on port %d", HTTP_PORT);
        return true;
    }
    
    /**
     * @brief Stop the HTTP server
     */
    void stop() {
        if (server_) {
            httpd_stop(server_);
            server_ = nullptr;
            ESP_LOGI(HTTP_TAG, "HTTP server stopped");
        }
    }
    
    /**
     * @brief Check if server is running
     */
    bool isRunning() const { return server_ != nullptr; }
    
    /**
     * @brief Set command callback
     */
    void setCommandCallback(CommandCallback callback) {
        command_callback_ = callback;
    }
    
    /**
     * @brief Set scene activated callback (called when user activates a scene)
     */
    void setSceneActivatedCallback(std::function<void(const SavedScene&)> callback) {
        sceneActivatedCallback_ = callback;
        ESP_LOGI(HTTP_TAG, "Scene activated callback registered: %s", callback ? "YES" : "NO");
    }
    
    /**
     * @brief Auto-activate the saved active scene on startup
     * Call this after setting the scene activated callback
     */
    void autoActivateSavedScene() {
        if (!sceneActivatedCallback_) {
            ESP_LOGW(HTTP_TAG, "No scene callback registered, cannot auto-activate");
            return;
        }
        
        if (activeSceneId_ < 0) {
            ESP_LOGI(HTTP_TAG, "No active scene saved, skipping auto-activate");
            return;
        }
        
        // Find and activate the saved active scene
        for (auto& scene : savedScenes_) {
            if (scene.id == activeSceneId_) {
                ESP_LOGI(HTTP_TAG, "Auto-activating saved scene: '%s' (id=%d)", 
                         scene.name.c_str(), scene.id);
                sceneActivatedCallback_(scene);
                return;
            }
        }
        
        ESP_LOGW(HTTP_TAG, "Saved active scene id=%d not found in scene list", activeSceneId_);
    }
    
    /**
     * @brief Set scene updated callback (called when active scene config changes)
     */
    void setSceneUpdatedCallback(std::function<void(const SavedScene&)> callback) {
        sceneUpdatedCallback_ = callback;
        ESP_LOGI(HTTP_TAG, "Scene updated callback registered: %s", callback ? "YES" : "NO");
    }
    
    /**
     * @brief Set single param update callback (called when one param slider changes)
     */
    void setSingleParamCallback(std::function<void(const char*, float)> callback) {
        singleParamCallback_ = callback;
        ESP_LOGI(HTTP_TAG, "Single param callback registered: %s", callback ? "YES" : "NO");
    }
    
    /**
     * @brief Set sprite display callback (for immediate GPU rendering)
     */
    void setSpriteDisplayCallback(std::function<void(const StaticSpriteSceneConfig&)> callback) {
        spriteDisplayCallback_ = callback;
        ESP_LOGI(HTTP_TAG, "Sprite display callback registered: %s", callback ? "YES" : "NO");
    }
    
    /**
     * @brief Set display clear callback
     */
    void setDisplayClearCallback(std::function<void()> callback) {
        displayClearCallback_ = callback;
        ESP_LOGI(HTTP_TAG, "Display clear callback registered: %s", callback ? "YES" : "NO");
    }
    
    /**
     * @brief Get the active scene (if any)
     */
    const SavedScene* getActiveScene() const {
        for (const auto& scene : savedScenes_) {
            if (scene.active) return &scene;
        }
        return nullptr;
    }
    
    /**
     * @brief Get all saved scenes
     */
    const std::vector<SavedScene>& getSavedScenes() const {
        return savedScenes_;
    }
    
    /**
     * @brief Activate a scene by ID (programmatically)
     * Used by OLED menu to activate scenes via button press
     * @param sceneId The ID of the scene to activate
     * @return true if scene was found and activated, false otherwise
     */
    bool activateSceneById(int sceneId) {
        // Deactivate all scenes first
        for (auto& scene : savedScenes_) {
            scene.active = false;
        }
        
        // Activate the selected scene
        for (auto& scene : savedScenes_) {
            if (scene.id == sceneId) {
                scene.active = true;
                activeSceneId_ = scene.id;
                
                // Notify scene renderer if callback is set
                if (sceneActivatedCallback_) {
                    sceneActivatedCallback_(scene);
                }
                
                ESP_LOGI(HTTP_TAG, "Activated scene via OLED: %s (id %d)", scene.name.c_str(), scene.id);
                saveScenesStorage();
                return true;
            }
        }
        
        ESP_LOGW(HTTP_TAG, "Scene id=%d not found", sceneId);
        return false;
    }
    
    /**
     * @brief Update a parameter on the active scene
     * Saves the param value to the active scene's params map
     * @param paramName The name of the parameter (e.g., "shader_gradient_distance")
     * @param value The value to set
     * @return true if active scene found and updated
     */
    static bool updateActiveSceneParam(const char* paramName, float value) {
        if (activeSceneId_ < 0) return false;
        
        for (auto& scene : savedScenes_) {
            if (scene.id == activeSceneId_) {
                scene.params[paramName] = value;
                ESP_LOGI(HTTP_TAG, "[updateActiveSceneParam] scene %d: %s = %.2f", 
                         activeSceneId_, paramName, value);
                return true;
            }
        }
        return false;
    }
    
    /**
     * @brief Force save active scene's params to storage
     */
    static void saveActiveSceneParams() {
        static uint32_t lastSaveTime = 0;
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
        // Throttle saves to every 2 seconds
        if (now - lastSaveTime > 2000) {
            saveScenesStorage();
            lastSaveTime = now;
            ESP_LOGI(HTTP_TAG, "Saved scenes to storage (throttled)");
        }
    }
    
    /**
     * @brief Get all saved sprites (for diagnostic/debug purposes)
     */
    const std::vector<SavedSprite>& getSprites() const {
        return savedSprites_;
    }
    
    /**
     * @brief Get the httpd handle (for advanced use)
     */
    httpd_handle_t getHandle() const { return server_; }
    
    /**
     * @brief Force save scenes to storage (for testing)
     */
    void forceSaveScenes() {
        saveScenesStorage();
    }
    
    /**
     * @brief Force load scenes from storage (for testing)
     */
    void forceLoadScenes() {
        loadScenesFromStorage();
    }
    
    /**
     * @brief Get scene activated callback (for test harness)
     */
    static std::function<void(const SavedScene&)>& getSceneActivatedCallback() {
        return instance().sceneActivatedCallback_;
    }
    
    /**
     * @brief Set LED preset activated callback (called when user activates an LED preset)
     */
    void setLedPresetActivatedCallback(std::function<void(const SavedLedPreset&)> callback) {
        ledPresetActivatedCallback_ = callback;
        ESP_LOGI(HTTP_TAG, "LED preset activated callback registered: %s", callback ? "YES" : "NO");
    }
    
    /**
     * @brief Get LED preset activated callback (for API handlers)
     */
    static std::function<void(const SavedLedPreset&)>& getLedPresetActivatedCallback() {
        return instance().ledPresetActivatedCallback_;
    }
    
    /**
     * @brief Get the active LED preset (if any)
     */
    const SavedLedPreset* getActiveLedPreset() const {
        for (const auto& preset : savedLedPresets_) {
            if (preset.id == activeLedPresetId_) return &preset;
        }
        return nullptr;
    }
    
    /**
     * @brief Get all saved LED presets
     */
    const std::vector<SavedLedPreset>& getSavedLedPresets() const {
        return savedLedPresets_;
    }
    
    /**
     * @brief Activate an LED preset by ID (programmatically)
     * Used by OLED menu to activate LED presets via button press
     */
    bool activateLedPresetById(int presetId) {
        for (auto& preset : savedLedPresets_) {
            if (preset.id == presetId) {
                activeLedPresetId_ = presetId;
                
                if (ledPresetActivatedCallback_) {
                    ledPresetActivatedCallback_(preset);
                }
                
                ESP_LOGI(HTTP_TAG, "Activated LED preset: %s (id %d)", preset.name.c_str(), preset.id);
                saveLedPresetsStorage();
                return true;
            }
        }
        
        ESP_LOGW(HTTP_TAG, "LED preset id=%d not found", presetId);
        return false;
    }

private:
    HttpServer() = default;
    ~HttpServer() { stop(); }
    
    // Prevent copying
    HttpServer(const HttpServer&) = delete;
    HttpServer& operator=(const HttpServer&) = delete;
    
    /**
     * @brief Register all URI handlers
     */
    void registerHandlers() {
        // Login page (always accessible)
        registerHandler("/login", HTTP_GET, handleLoginPage);
        registerHandler("/api/login", HTTP_POST, handleApiLogin);
        registerHandler("/api/logout", HTTP_POST, handleApiLogout);
        
        // Page routes - each tab is a separate page
        registerHandler("/", HTTP_GET, handlePageBasic);
        registerHandler("/system", HTTP_GET, handlePageSystem);
        registerHandler("/advanced", HTTP_GET, handlePageAdvancedMenu);
        registerHandler("/advanced/sprites", HTTP_GET, handlePageSprite);
        registerHandler("/advanced/scenes", HTTP_GET, handlePageSceneList);
        registerHandler("/advanced/scenes/edit", HTTP_GET, handlePageSceneEdit);
        registerHandler("/sprites", HTTP_GET, handlePageSprite);  // Legacy redirect
        registerHandler("/settings", HTTP_GET, handlePageSettings);
        registerHandler("/display-config", HTTP_GET, handlePageDisplayConfig);
        registerHandler("/sdcard", HTTP_GET, handlePageSdCard);  // SD Card Browser
        
        // Static content handlers
        registerHandler("/style.css", HTTP_GET, handleCss);
        
        // API endpoints
        registerHandler("/api/state", HTTP_GET, handleApiState);
        registerHandler("/api/command", HTTP_POST, handleApiCommand);
        registerHandler("/api/scan", HTTP_GET, handleApiScan);
        
        // Sprite API endpoints
        registerHandler("/api/sprites", HTTP_GET, handleApiSprites);
        registerHandler("/api/sprite/save", HTTP_POST, handleApiSpriteSave);
        registerHandler("/api/sprite/rename", HTTP_POST, handleApiSpriteRename);
        registerHandler("/api/sprite/delete", HTTP_POST, handleApiSpriteDelete);
        registerHandler("/api/sprite/apply", HTTP_POST, handleApiSpriteApply);
        registerHandler("/api/storage", HTTP_GET, handleApiStorage);
        
        // Configuration API endpoints
        registerHandler("/api/configs", HTTP_GET, handleApiConfigs);
        registerHandler("/api/config/apply", HTTP_POST, handleApiConfigApply);
        registerHandler("/api/config/save", HTTP_POST, handleApiConfigSave);
        registerHandler("/api/config/create", HTTP_POST, handleApiConfigCreate);
        registerHandler("/api/config/rename", HTTP_POST, handleApiConfigRename);
        registerHandler("/api/config/duplicate", HTTP_POST, handleApiConfigDuplicate);
        registerHandler("/api/config/delete", HTTP_POST, handleApiConfigDelete);
        
        // Scene API endpoints
        registerHandler("/api/scenes", HTTP_GET, handleApiScenes);
        registerHandler("/api/scene/create", HTTP_POST, handleApiSceneCreate);
        registerHandler("/api/scene/delete", HTTP_POST, handleApiSceneDelete);
        registerHandler("/api/scene/rename", HTTP_POST, handleApiSceneRename);
        registerHandler("/api/scene/get", HTTP_GET, handleApiSceneGet);
        registerHandler("/api/scene/activate", HTTP_POST, handleApiSceneActivate);
        registerHandler("/api/scene/update", HTTP_POST, handleApiSceneUpdate);
        registerHandler("/api/scene/config", HTTP_GET, handleApiSceneConfig);
        registerHandler("/api/scene/display", HTTP_POST, handleApiSceneDisplay);
        registerHandler("/api/scene/clear", HTTP_POST, handleApiSceneClear);
        registerHandler("/api/scenes/reorder", HTTP_POST, handleApiScenesReorder);
        
        // LED Preset page routes and API endpoints
        registerHandler("/advanced/ledpresets", HTTP_GET, handlePageLedPresetList);
        registerHandler("/advanced/ledpresets/edit", HTTP_GET, handlePageLedPresetEdit);
        registerHandler("/api/ledpresets", HTTP_GET, handleApiLedPresets);
        registerHandler("/api/ledpreset/create", HTTP_POST, handleApiLedPresetCreate);
        registerHandler("/api/ledpreset/get", HTTP_GET, handleApiLedPresetGet);
        registerHandler("/api/ledpreset/update", HTTP_POST, handleApiLedPresetUpdate);
        registerHandler("/api/ledpreset/delete", HTTP_POST, handleApiLedPresetDelete);
        registerHandler("/api/ledpreset/activate", HTTP_POST, handleApiLedPresetActivate);
        registerHandler("/api/ledpreset/preview", HTTP_POST, handleApiLedPresetPreview);
        registerHandler("/api/ledpresets/reorder", HTTP_POST, handleApiLedPresetsReorder);
        
        // Equation Editor page and API endpoints
        registerHandler("/advanced/equations", HTTP_GET, handlePageEquations);
        registerHandler("/api/equations", HTTP_GET, handleApiEquations);
        registerHandler("/api/equation/save", HTTP_POST, handleApiEquationSave);
        registerHandler("/api/equation/delete", HTTP_POST, handleApiEquationDelete);
        registerHandler("/api/sensors", HTTP_GET, handleApiSensors);
        
        // IMU Calibration API endpoints
        registerHandler("/api/imu/calibrate", HTTP_POST, handleApiImuCalibrate);
        registerHandler("/api/imu/status", HTTP_GET, handleApiImuStatus);
        registerHandler("/api/imu/clear", HTTP_POST, handleApiImuClear);

        // Fan Control API endpoint
        registerHandler("/api/fan/toggle", HTTP_POST, handleApiFanToggle);

        // SD Card API endpoints
        registerHandler("/api/sdcard/status", HTTP_GET, handleApiSdCardStatus);
        registerHandler("/api/sdcard/format", HTTP_POST, handleApiSdCardFormat);
        registerHandler("/api/sdcard/format-init", HTTP_POST, handleApiSdCardFormatInit);
        registerHandler("/api/sdcard/setup-defaults", HTTP_POST, handleApiSdCardSetupDefaults);
        registerHandler("/api/sdcard/setup", HTTP_POST, handleApiSdCardSetup);
        registerHandler("/api/sdcard/clear", HTTP_POST, handleApiSdCardClear);
        registerHandler("/api/sdcard/list", HTTP_GET, handleApiSdCardList);
        registerHandler("/api/sdcard/hex", HTTP_GET, handleApiSdCardHex);
        registerHandler("/api/sdcard/read", HTTP_GET, handleApiSdCardRead);
        registerHandler("/api/sdcard/download", HTTP_GET, handleApiSdCardDownload);
        registerHandler("/api/sdcard/delete", HTTP_POST, handleApiSdCardDelete);
        
        // Animation System API endpoints
        registerHandler("/api/animation/sets", HTTP_GET, handleApiAnimationSets);
        registerHandler("/api/animation/params", HTTP_GET, handleApiAnimationParams);
        registerHandler("/api/animation/param", HTTP_POST, handleApiAnimationParam);
        registerHandler("/api/animation/inputs", HTTP_GET, handleApiAnimationInputs);
        registerHandler("/api/animation/activate", HTTP_POST, handleApiAnimationActivate);
        registerHandler("/api/animation/stop", HTTP_POST, handleApiAnimationStop);
        registerHandler("/api/animation/reset", HTTP_POST, handleApiAnimationReset);
        registerHandler("/api/registry/shaders", HTTP_GET, handleApiRegistryShaders);
        registerHandler("/api/registry/transitions", HTTP_GET, handleApiRegistryTransitions);
        registerHandler("/api/registry/animations", HTTP_GET, handleApiRegistryAnimations);
        registerHandler("/api/scene/save", HTTP_POST, handleApiSceneSave);
        registerHandler("/api/scene/param", HTTP_POST, handleApiSceneParam);
        registerHandler("/api/scene/preview", HTTP_POST, handleApiScenePreview);
        registerHandler("/api/scene/stop", HTTP_POST, handleApiSceneStop);
        
        // Captive portal detection endpoints (comprehensive list for all devices)
        const char* redirect_paths[] = {
            // Android (various versions & OEMs)
            "/generate_204", "/gen_204",
            "/connectivitycheck.gstatic.com",
            "/mobile/status.php",
            "/wifi/test.html",
            "/check_network_status.txt",
            "/connectivitycheck.android.com",
            // Samsung
            "/generate_204_samsung",
            // Huawei/Honor
            "/generate_204_huawei", 
            // Xiaomi
            "/generate_204_xiaomi",
            // Windows
            "/connecttest.txt", "/fwlink", "/redirect",
            "/ncsi.txt", "/connecttest.html",
            "/msftconnecttest.com",
            "/msftncsi.com",
            // Apple iOS/macOS (multiple variants)
            "/library/test/success.html",
            "/hotspot-detect.html",
            "/captive.apple.com",
            "/library/test/success",
            "/hotspot-detect",
            // Amazon Kindle/Fire
            "/kindle-wifi/wifistub.html",
            "/kindle-wifi/test",
            // Firefox
            "/success.txt", "/canonical.html",
            "/detectportal.firefox.com",
            // Generic/Other
            "/chat", "/favicon.ico",
            "/portal.html", "/portal",
            "/login", "/login.html"
        };
        
        for (const char* path : redirect_paths) {
            registerHandler(path, HTTP_GET, handleRedirect);
        }
        
        // Wildcard catch-all (must be last) - handle all HTTP methods
        registerHandler("/*", HTTP_GET, handleCatchAll);
        registerHandler("/*", HTTP_POST, handleCatchAll);
        registerHandler("/*", HTTP_PUT, handleCatchAll);
        registerHandler("/*", HTTP_DELETE, handleCatchAll);
        registerHandler("/*", HTTP_HEAD, handleCatchAll);
    }
    
    /**
     * @brief Helper to register a handler
     */
    void registerHandler(const char* uri, httpd_method_t method, esp_err_t (*handler)(httpd_req_t*)) {
        httpd_uri_t uri_handler = {
            .uri = uri,
            .method = method,
            .handler = handler,
            .user_ctx = this
        };
        httpd_register_uri_handler(server_, &uri_handler);
    }
    
    // ========== SD Card / SPIFFS Storage for Sprites ==========
    
    /**
     * @brief Initialize SD card storage for sprites and equations
     * Primary storage - uses FileSystemService
     */
    static void initSdCardStorage() {
        ESP_LOGI(HTTP_TAG, "========================================");
        ESP_LOGI(HTTP_TAG, "  INITIALIZING SD CARD STORAGE");
        ESP_LOGI(HTTP_TAG, "========================================");
        
        if (sdcard_storage_ready_) {
            ESP_LOGI(HTTP_TAG, "SD storage already initialized, skipping");
            return;
        }
        
        auto& fs = Utils::FileSystemService::instance();
        
        // Check if SD card is ready
        if (!fs.isReady() || !fs.isMounted()) {
            ESP_LOGW(HTTP_TAG, "SD card not available, will use SPIFFS fallback");
            return;
        }
        
        // Create directory structure
        struct stat st;
        const char* dirs[] = { SPRITE_DIR, EQUATION_DIR, SCENE_DIR, LED_PRESET_DIR };
        const char* names[] = { "sprites", "equations", "scenes", "led-presets" };
        
        for (int i = 0; i < 4; i++) {
            if (stat(dirs[i], &st) != 0) {
                int ret = mkdir(dirs[i], 0755);
                if (ret == 0) {
                    ESP_LOGI(HTTP_TAG, "Created SD card %s directory: %s", names[i], dirs[i]);
                } else {
                    ESP_LOGE(HTTP_TAG, "Failed to create %s directory: %s (errno=%d: %s)", 
                             names[i], dirs[i], errno, strerror(errno));
                }
            } else {
                ESP_LOGI(HTTP_TAG, "SD card %s directory exists: %s", names[i], dirs[i]);
            }
        }
        
        sdcard_storage_ready_ = true;
        ESP_LOGI(HTTP_TAG, "SD card storage initialized. Total: %llu MB, Free: %llu MB",
            fs.getTotalBytes() / (1024 * 1024),
            fs.getFreeBytes() / (1024 * 1024));
    }
    
    /**
     * @brief Initialize SPIFFS filesystem (fallback storage)
     */
    static void initSpiffs() {
        if (spiffs_initialized_) return;
        
        esp_vfs_spiffs_conf_t conf = {
            .base_path = "/spiffs",
            .partition_label = NULL,
            .max_files = 10,
            .format_if_mount_failed = true
        };
        
        esp_err_t ret = esp_vfs_spiffs_register(&conf);
        if (ret != ESP_OK) {
            if (ret == ESP_FAIL) {
                ESP_LOGE(HTTP_TAG, "Failed to mount SPIFFS");
            } else if (ret == ESP_ERR_NOT_FOUND) {
                ESP_LOGE(HTTP_TAG, "SPIFFS partition not found");
            } else {
                ESP_LOGE(HTTP_TAG, "SPIFFS init failed: %s", esp_err_to_name(ret));
            }
            return;
        }
        
        // Create sprites directory if it doesn't exist (only for SPIFFS fallback)
        struct stat st;
        if (stat(SPRITE_DIR_SPIFFS, &st) != 0) {
            mkdir(SPRITE_DIR_SPIFFS, 0755);
            ESP_LOGI(HTTP_TAG, "Created SPIFFS sprites directory");
        }
        
        spiffs_initialized_ = true;
        
        size_t total = 0, used = 0;
        esp_spiffs_info(NULL, &total, &used);
        ESP_LOGI(HTTP_TAG, "SPIFFS initialized as fallback. Total: %d KB, Used: %d KB", total/1024, used/1024);
    }
    
    /**
     * @brief Get the active sprite index file path
     * Prefers SD card, falls back to SPIFFS
     */
    static const char* getSpriteIndexPath() {
        return sdcard_storage_ready_ ? SPRITE_INDEX_FILE : SPRITE_INDEX_FILE_SPIFFS;
    }
    
    /**
     * @brief Get the active equation index file path
     * Prefers SD card, falls back to SPIFFS
     */
    static const char* getEquationIndexPath() {
        return sdcard_storage_ready_ ? EQUATION_INDEX_FILE : EQUATION_INDEX_FILE_SPIFFS;
    }
    
    /**
     * @brief Save sprite index to storage (SD card preferred)
     * Uses FileSystemService for all file operations to ensure proper handling
     */
    static void saveSpritesToStorage() {
        ESP_LOGI(HTTP_TAG, "saveSpritesToStorage: sdcard=%d, spiffs=%d", 
                 sdcard_storage_ready_, spiffs_initialized_);
        
        auto& fs = Utils::FileSystemService::instance();
        
        // Only use SD card via FileSystemService - no SPIFFS fallback
        if (!sdcard_storage_ready_ || !fs.isReady() || !fs.isMounted()) {
            ESP_LOGE(HTTP_TAG, "SD card not available for sprite storage!");
            return;
        }
        
        // Use relative paths for FileSystemService (it adds /sdcard prefix)
        const char* spritesRelDir = "/Sprites";
        const char* indexRelPath = "/Sprites/index.dat";
        
        // Ensure directory exists
        if (!fs.dirExists(spritesRelDir)) {
            ESP_LOGI(HTTP_TAG, "Creating sprites directory");
            fs.createDir(spritesRelDir);
            vTaskDelay(pdMS_TO_TICKS(200));  // Long delay after mkdir
            SystemAPI::Utils::syncFilesystem();  // Force sync
        }
        
        // Save files ONE AT A TIME with full sync between each operation
        // This is slower but much more reliable for FAT filesystem
        for (const auto& sprite : savedSprites_) {
            // Save pixel file first
            if (!sprite.pixelData.empty()) {
                char pixelRelPath[64];
                snprintf(pixelRelPath, sizeof(pixelRelPath), "/Sprites/sprite_%d.bin", sprite.id);
                
                ESP_LOGI(HTTP_TAG, "Saving pixel file: sprite_%d.bin (%zu bytes)", 
                         sprite.id, sprite.pixelData.size());
                
                if (fs.writeFile(pixelRelPath, sprite.pixelData.data(), sprite.pixelData.size())) {
                    ESP_LOGI(HTTP_TAG, "Saved pixel data for sprite %d", sprite.id);
                } else {
                    ESP_LOGE(HTTP_TAG, "Failed to save pixel file for sprite %d", sprite.id);
                }
                
                // CRITICAL: Long delay and sync after each file write
                vTaskDelay(pdMS_TO_TICKS(300));
                SystemAPI::Utils::syncFilesystem();
            }
            
            // Save preview file (after full sync from pixel file)
            if (!sprite.preview.empty()) {
                char previewRelPath[64];
                snprintf(previewRelPath, sizeof(previewRelPath), "/Sprites/preview_%d.txt", sprite.id);
                
                ESP_LOGI(HTTP_TAG, "Saving preview: preview_%d.txt (%zu bytes)", 
                         sprite.id, sprite.preview.size());
                
                // Explicitly delete existing file first  
                fs.deleteFile(previewRelPath);
                vTaskDelay(pdMS_TO_TICKS(200));
                SystemAPI::Utils::syncFilesystem();
                vTaskDelay(pdMS_TO_TICKS(100));
                
                if (fs.writeFile(previewRelPath, sprite.preview.c_str(), sprite.preview.size())) {
                    ESP_LOGI(HTTP_TAG, "Saved preview for sprite %d", sprite.id);
                } else {
                    ESP_LOGE(HTTP_TAG, "Failed to save preview file for sprite %d", sprite.id);
                }
                
                // CRITICAL: Long delay and sync after preview write
                vTaskDelay(pdMS_TO_TICKS(300));
                SystemAPI::Utils::syncFilesystem();
            }
        }
        
        // Wait for filesystem to settle after pixel writes
        vTaskDelay(pdMS_TO_TICKS(200));
        
        // MEMORY OPTIMIZATION: Clear pixel data from RAM after saving to SD card
        // This prevents memory exhaustion when saving many sprites
        // Pixel data will be lazy-loaded from SD when needed
        size_t freedBytes = 0;
        for (auto& sprite : savedSprites_) {
            if (!sprite.pixelData.empty()) {
                freedBytes += sprite.pixelData.size();
                sprite.pixelData.clear();
                sprite.pixelData.shrink_to_fit();  // Actually release memory
                sprite.savedToSd = true;
            }
            // Also clear preview from RAM - it's saved to preview_X.txt
            if (!sprite.preview.empty()) {
                freedBytes += sprite.preview.size();
                sprite.preview.clear();
                sprite.preview.shrink_to_fit();
            }
        }
        if (freedBytes > 0) {
            ESP_LOGI(HTTP_TAG, "Freed %zu bytes of sprite data from RAM (lazy load enabled)", freedBytes);
        }
        
        // Build JSON index
        cJSON* root = cJSON_CreateObject();
        cJSON_AddNumberToObject(root, "version", 2);
        cJSON_AddNumberToObject(root, "nextId", nextSpriteId_);
        cJSON_AddNumberToObject(root, "count", (int)savedSprites_.size());
        
        cJSON* sprites = cJSON_CreateArray();
        for (const auto& sprite : savedSprites_) {
            cJSON* item = cJSON_CreateObject();
            cJSON_AddNumberToObject(item, "id", sprite.id);
            cJSON_AddStringToObject(item, "name", sprite.name.c_str());
            cJSON_AddNumberToObject(item, "width", sprite.width);
            cJSON_AddNumberToObject(item, "height", sprite.height);
            cJSON_AddNumberToObject(item, "scale", sprite.scale);
            cJSON_AddBoolToObject(item, "hasPixels", !sprite.pixelData.empty());
            cJSON_AddNumberToObject(item, "pixelSize", (int)sprite.pixelData.size());
            // Note: preview not saved to index - too large, regenerate on load if needed
            cJSON_AddItemToArray(sprites, item);
        }
        cJSON_AddItemToObject(root, "sprites", sprites);
        
        char* json = cJSON_PrintUnformatted(root);
        cJSON_Delete(root);
        
        if (!json) {
            ESP_LOGE(HTTP_TAG, "Failed to serialize sprite index JSON");
            return;
        }
        
        size_t jsonLen = strlen(json);
        ESP_LOGI(HTTP_TAG, "Writing sprite index: %zu bytes, %d sprites", jsonLen, (int)savedSprites_.size());
        
        // Write via FileSystemService with retry logic
        // Delete existing first to avoid FAT issues
        fs.deleteFile(indexRelPath);
        vTaskDelay(pdMS_TO_TICKS(100));  // Wait for FAT table to update
        
        bool writeSuccess = false;
        for (int retry = 0; retry < 3 && !writeSuccess; retry++) {
            if (retry > 0) {
                ESP_LOGW(HTTP_TAG, "Retrying index write (attempt %d/3)...", retry + 1);
                vTaskDelay(pdMS_TO_TICKS(200));
            }
            
            if (fs.writeFile(indexRelPath, json, jsonLen)) {
                // Verify write
                vTaskDelay(pdMS_TO_TICKS(50));
                uint64_t writtenSize = fs.getFileSize(indexRelPath);
                if (writtenSize == jsonLen) {
                    ESP_LOGI(HTTP_TAG, "Saved %d sprites to SD card (verified: %llu bytes)", 
                             (int)savedSprites_.size(), writtenSize);
                    writeSuccess = true;
                } else {
                    ESP_LOGW(HTTP_TAG, "Index file size mismatch: expected %zu, got %llu", jsonLen, writtenSize);
                    fs.deleteFile(indexRelPath);  // Delete corrupt file and retry
                    vTaskDelay(pdMS_TO_TICKS(100));
                }
            } else {
                ESP_LOGE(HTTP_TAG, "Failed to write sprite index (attempt %d)", retry + 1);
            }
        }
        
        if (!writeSuccess) {
            ESP_LOGE(HTTP_TAG, "CRITICAL: Failed to save sprite index after 3 retries!");
        }
        
        free(json);
    }
    
    /**
     * @brief Scan for orphaned sprite files (recovery mechanism)
     * If index.json doesn't exist but sprite_*.bin files do, recover them
     */
    static void recoverOrphanedSprites() {
        auto& fs = Utils::FileSystemService::instance();
        
        if (!sdcard_storage_ready_ || !fs.isReady() || !fs.isMounted()) {
            return;
        }
        
        const char* spritesRelDir = "/Sprites";
        
        std::vector<int> foundIds;
        
        // Scan directory for sprite_*.bin files
        fs.listDir(spritesRelDir, [&](const Utils::FileInfo& info) {
            if (info.isDirectory) return true;
            
            int id;
            if (sscanf(info.name, "sprite_%d.bin", &id) == 1) {
                foundIds.push_back(id);
            }
            return true;
        });
        
        if (foundIds.empty()) return;
        
        ESP_LOGI(HTTP_TAG, "Found %d sprite files, checking for orphans...", (int)foundIds.size());
        
        int recovered = 0;
        for (int id : foundIds) {
            // Check if we already have this sprite loaded
            bool exists = false;
            for (const auto& s : savedSprites_) {
                if (s.id == id) { exists = true; break; }
            }
            if (exists) continue;
            
            char pixelRelPath[64];
            snprintf(pixelRelPath, sizeof(pixelRelPath), "/Sprites/sprite_%d.bin", id);
            
            uint64_t fileSize = fs.getFileSize(pixelRelPath);
            if (fileSize == 0 || fileSize > 1024*1024) continue;
            
            SavedSprite sprite;
            sprite.id = id;
            sprite.name = "Recovered_" + std::to_string(id);
            
            // Try to guess dimensions from file size (assume RGB888)
            int pixels = (int)(fileSize / 3);
            
            // Common sizes: 32x32=1024, 64x64=4096, 64x32=2048, 31x31=961
            if (pixels == 1024) { sprite.width = 32; sprite.height = 32; }
            else if (pixels == 4096) { sprite.width = 64; sprite.height = 64; }
            else if (pixels == 2048) { sprite.width = 64; sprite.height = 32; }
            else if (pixels == 961) { sprite.width = 31; sprite.height = 31; }
            else {
                // Default to square-ish
                sprite.width = (int)sqrt(pixels);
                sprite.height = pixels / sprite.width;
            }
            
            sprite.scale = 100;
            // MEMORY OPTIMIZATION: Don't load pixel data, just mark as saved to SD
            sprite.savedToSd = true;
            sprite.uploadedToGpu = false;
            savedSprites_.push_back(sprite);
            ESP_LOGI(HTTP_TAG, "Recovered sprite %d (%dx%d, lazy load enabled)", 
                     id, sprite.width, sprite.height);
                     
            // Update nextSpriteId if needed
            if (id >= nextSpriteId_) nextSpriteId_ = id + 1;
            recovered++;
        }
        
        // If we recovered sprites, save the updated index
        if (recovered > 0) {
            ESP_LOGI(HTTP_TAG, "Recovered %d orphaned sprites, saving index...", recovered);
            vTaskDelay(pdMS_TO_TICKS(100));
            saveSpritesToStorage();
        }
    }
    
    /**
     * @brief Load sprite index from storage using FileSystemService
     */
    static void loadSpritesFromStorage() {
        ESP_LOGI(HTTP_TAG, "========================================");
        ESP_LOGI(HTTP_TAG, "  LOADING SPRITES FROM STORAGE");
        ESP_LOGI(HTTP_TAG, "========================================");
        
        auto& fs = Utils::FileSystemService::instance();
        
        // Debug: Print SD card status
        ESP_LOGI(HTTP_TAG, "SD card status:");
        ESP_LOGI(HTTP_TAG, "  sdcard_storage_ready_: %d", sdcard_storage_ready_);
        ESP_LOGI(HTTP_TAG, "  fs.isReady(): %d", fs.isReady());
        ESP_LOGI(HTTP_TAG, "  fs.isMounted(): %d", fs.isMounted());
        
        if (!sdcard_storage_ready_ || !fs.isReady() || !fs.isMounted()) {
            ESP_LOGW(HTTP_TAG, "SD card not available for loading sprites");
            return;
        }
        
        // Debug: List sprites directory contents
        ESP_LOGI(HTTP_TAG, "Listing /sprites directory:");
        int fileCount = 0;
        fs.listDir("/sprites", [&](const Utils::FileInfo& info) -> bool {
            ESP_LOGI(HTTP_TAG, "  [%s] %s (%lu bytes)", 
                     info.isDirectory ? "DIR" : "FILE", 
                     info.name, (unsigned long)info.size);
            fileCount++;
            return true;
        });
        ESP_LOGI(HTTP_TAG, "Found %d entries in /sprites", fileCount);
        
        const char* indexRelPath = "/sprites/index.dat";
        
        // Check if index file exists
        if (!fs.fileExists(indexRelPath)) {
            ESP_LOGW(HTTP_TAG, "No sprite index found at %s", indexRelPath);
            ESP_LOGI(HTTP_TAG, "Scanning for orphaned sprite files...");
            recoverOrphanedSprites();
            return;
        }
        
        // Get file size first
        uint64_t indexSize = fs.getFileSize(indexRelPath);
        ESP_LOGI(HTTP_TAG, "Index file exists: %s (%llu bytes)", indexRelPath, indexSize);
        
        // Read the index file
        char* buf = nullptr;
        size_t bufSize = 0;
        if (!fs.readFile(indexRelPath, &buf, &bufSize) || !buf) {
            ESP_LOGE(HTTP_TAG, "Failed to read sprite index!");
            recoverOrphanedSprites();
            return;
        }
        
        ESP_LOGI(HTTP_TAG, "Read sprite index: %zu bytes", bufSize);
        
        // Debug: Print first 200 chars of index
        if (bufSize > 0) {
            char preview[201];
            size_t previewLen = bufSize < 200 ? bufSize : 200;
            memcpy(preview, buf, previewLen);
            preview[previewLen] = '\0';
            ESP_LOGI(HTTP_TAG, "Index content preview: %s", preview);
        }
        
        cJSON* root = cJSON_Parse(buf);
        free(buf);
        
        if (!root) {
            ESP_LOGE(HTTP_TAG, "Failed to parse sprite index JSON!");
            ESP_LOGE(HTTP_TAG, "cJSON error: %s", cJSON_GetErrorPtr() ? cJSON_GetErrorPtr() : "unknown");
            recoverOrphanedSprites();
            return;
        }
        
        // Parse version and nextId
        cJSON* version = cJSON_GetObjectItem(root, "version");
        cJSON* nextId = cJSON_GetObjectItem(root, "nextId");
        if (nextId && cJSON_IsNumber(nextId)) {
            nextSpriteId_ = nextId->valueint;
        }
        
        ESP_LOGI(HTTP_TAG, "Index version=%d, nextId=%d", 
                 version ? version->valueint : 1, nextSpriteId_);
        
        cJSON* sprites = cJSON_GetObjectItem(root, "sprites");
        if (sprites && cJSON_IsArray(sprites)) {
            savedSprites_.clear();
            
            cJSON* item = NULL;
            cJSON_ArrayForEach(item, sprites) {
                SavedSprite sprite;
                
                cJSON* id = cJSON_GetObjectItem(item, "id");
                cJSON* name = cJSON_GetObjectItem(item, "name");
                cJSON* width = cJSON_GetObjectItem(item, "width");
                cJSON* height = cJSON_GetObjectItem(item, "height");
                cJSON* scale = cJSON_GetObjectItem(item, "scale");
                // cJSON* pixelSize = cJSON_GetObjectItem(item, "pixelSize"); // Reserved for future use
                
                if (id && cJSON_IsNumber(id)) sprite.id = id->valueint;
                if (name && cJSON_IsString(name)) sprite.name = name->valuestring;
                if (width && cJSON_IsNumber(width)) sprite.width = width->valueint;
                if (height && cJSON_IsNumber(height)) sprite.height = height->valueint;
                if (scale && cJSON_IsNumber(scale)) sprite.scale = scale->valueint;
                
                ESP_LOGI(HTTP_TAG, "Loading sprite %d '%s' (%dx%d)", 
                         sprite.id, sprite.name.c_str(), sprite.width, sprite.height);
                
                // Try to load pixel data
                char pixelRelPath[64];
                snprintf(pixelRelPath, sizeof(pixelRelPath), "/Sprites/sprite_%d.bin", sprite.id);
                
                // MEMORY OPTIMIZATION: Don't load pixel data into RAM on boot
                // Just check if file exists and set savedToSd flag for lazy loading
                if (fs.fileExists(pixelRelPath)) {
                    uint64_t fileSize = fs.getFileSize(pixelRelPath);
                    if (fileSize > 0 && fileSize < 1024*1024) {  // Max 1MB
                        // Mark as saved to SD for lazy loading - don't load into RAM now!
                        sprite.savedToSd = true;
                        ESP_LOGI(HTTP_TAG, "  Sprite pixel file exists (%llu bytes, lazy load enabled)", fileSize);
                    } else {
                        ESP_LOGW(HTTP_TAG, "  Invalid pixel file size: %llu", fileSize);
                    }
                } else {
                    ESP_LOGW(HTTP_TAG, "  No pixel file found");
                }
                
                // MEMORY OPTIMIZATION: Don't load preview into RAM either
                // Preview is only needed for web UI display and will be loaded on-demand
                char previewRelPath[64];
                snprintf(previewRelPath, sizeof(previewRelPath), "/Sprites/preview_%d.txt", sprite.id);
                
                if (fs.fileExists(previewRelPath)) {
                    ESP_LOGI(HTTP_TAG, "  Preview file exists (lazy load enabled)");
                } else {
                    ESP_LOGW(HTTP_TAG, "  No preview file found");
                }
                
                sprite.uploadedToGpu = false;
                savedSprites_.push_back(sprite);
            }
            
            ESP_LOGI(HTTP_TAG, "Loaded %d sprites from SD card", (int)savedSprites_.size());
        }
        
        cJSON_Delete(root);
        
        // Try to recover any orphaned sprite files
        recoverOrphanedSprites();
    }
    
    /**
     * @brief Save equations to storage (SD card preferred)
     */
    static void saveEquationsToStorage() {
        if (!sdcard_storage_ready_ && !spiffs_initialized_) return;
        
        const char* indexPath = getEquationIndexPath();
        
        cJSON* root = cJSON_CreateObject();
        cJSON_AddNumberToObject(root, "nextId", nextEquationId_);
        cJSON_AddStringToObject(root, "storage", sdcard_storage_ready_ ? "sdcard" : "spiffs");
        
        cJSON* equations = cJSON_CreateArray();
        for (const auto& eq : savedEquations_) {
            cJSON* item = cJSON_CreateObject();
            cJSON_AddNumberToObject(item, "id", eq.id);
            cJSON_AddStringToObject(item, "name", eq.name.c_str());
            cJSON_AddStringToObject(item, "expression", eq.expression.c_str());
            
            cJSON* vars = cJSON_CreateArray();
            for (const auto& v : eq.variables) {
                cJSON* varItem = cJSON_CreateObject();
                cJSON_AddStringToObject(varItem, "name", v.name.c_str());
                cJSON_AddStringToObject(varItem, "type", v.type.c_str());
                cJSON_AddStringToObject(varItem, "value", v.value.c_str());
                cJSON_AddItemToArray(vars, varItem);
            }
            cJSON_AddItemToObject(item, "variables", vars);
            
            cJSON_AddItemToArray(equations, item);
        }
        cJSON_AddItemToObject(root, "equations", equations);
        
        char* json = cJSON_PrintUnformatted(root);
        cJSON_Delete(root);
        
        if (json) {
            FILE* f = fopen(indexPath, "w");
            if (f) {
                fprintf(f, "%s", json);
                fclose(f);
                ESP_LOGI(HTTP_TAG, "Saved %d equations to %s", savedEquations_.size(),
                    sdcard_storage_ready_ ? "SD card" : "SPIFFS");
            } else {
                ESP_LOGE(HTTP_TAG, "Failed to open equation index for writing: %s", indexPath);
            }
            free(json);
        }
    }
    
    /**
     * @brief Get scene index path based on available storage
     */
    static const char* getSceneIndexPath() {
        if (sdcard_storage_ready_) return SCENE_INDEX_FILE;
        return SCENE_INDEX_FILE_SPIFFS;
    }
    
    /**
     * @brief Save scenes to storage (SD card preferred)
     */
    static void saveScenesStorage() {
        if (!sdcard_storage_ready_ && !spiffs_initialized_) return;
        
        const char* indexPath = getSceneIndexPath();
        
        // Create scenes directory if using SD card
        if (sdcard_storage_ready_) {
            struct stat st;
            if (stat(SCENE_DIR, &st) != 0) {
                mkdir(SCENE_DIR, 0755);
            }
        }
        
        cJSON* root = cJSON_CreateObject();
        cJSON_AddNumberToObject(root, "nextId", nextSceneId_);
        cJSON_AddNumberToObject(root, "activeId", activeSceneId_);
        cJSON_AddStringToObject(root, "storage", sdcard_storage_ready_ ? "sdcard" : "spiffs");
        
        cJSON* scenes = cJSON_CreateArray();
        for (const auto& s : savedScenes_) {
            cJSON* item = cJSON_CreateObject();
            cJSON_AddNumberToObject(item, "id", s.id);
            cJSON_AddStringToObject(item, "name", s.name.c_str());
            cJSON_AddNumberToObject(item, "type", s.type);
            cJSON_AddBoolToObject(item, "active", s.active);
            
            // Modular scene fields (new system)
            cJSON_AddBoolToObject(item, "displayEnabled", s.displayEnabled);
            cJSON_AddBoolToObject(item, "ledsEnabled", s.ledsEnabled);
            cJSON_AddBoolToObject(item, "effectsOnly", s.effectsOnly);
            cJSON_AddNumberToObject(item, "order", s.order);
            
            // Shader settings
            cJSON_AddBoolToObject(item, "shaderAA", s.shaderAA);
            cJSON_AddBoolToObject(item, "shaderInvert", s.shaderInvert);
            cJSON_AddStringToObject(item, "shaderColorMode", s.shaderColorMode.c_str());
            cJSON_AddStringToObject(item, "shaderColor", s.shaderColor.c_str());
            
            // LED settings
            cJSON_AddNumberToObject(item, "ledR", s.ledR);
            cJSON_AddNumberToObject(item, "ledG", s.ledG);
            cJSON_AddNumberToObject(item, "ledB", s.ledB);
            cJSON_AddNumberToObject(item, "ledBrightness", s.ledBrightness);
            
            // Background color (main scene level)
            cJSON_AddNumberToObject(item, "bgR", s.bgR);
            cJSON_AddNumberToObject(item, "bgG", s.bgG);
            cJSON_AddNumberToObject(item, "bgB", s.bgB);
            
            // AnimationSystem fields
            cJSON_AddStringToObject(item, "animType", s.animType.c_str());
            cJSON_AddStringToObject(item, "transition", s.transition.c_str());
            cJSON_AddNumberToObject(item, "spriteId", s.spriteId);
            cJSON_AddBoolToObject(item, "mirrorSprite", s.mirrorSprite);
            
            // Save params object
            ESP_LOGI(HTTP_TAG, "[SaveScene] id=%d animType='%s' params.size=%d", s.id, s.animType.c_str(), s.params.size());
            if (!s.params.empty()) {
                cJSON* params = cJSON_CreateObject();
                for (const auto& kv : s.params) {
                    ESP_LOGI(HTTP_TAG, "  [SaveParam] '%s' = %.2f", kv.first.c_str(), kv.second);
                    cJSON_AddNumberToObject(params, kv.first.c_str(), kv.second);
                }
                cJSON_AddItemToObject(item, "params", params);
            }
            
            // Save effects object
            if (!s.effects.empty()) {
                cJSON* effects = cJSON_CreateObject();
                for (const auto& kv : s.effects) {
                    cJSON* effectObj = cJSON_CreateObject();
                    cJSON_AddBoolToObject(effectObj, "enabled", kv.second.enabled);
                    cJSON_AddNumberToObject(effectObj, "intensity", kv.second.intensity);
                    cJSON_AddItemToObject(effects, kv.first.c_str(), effectObj);
                }
                cJSON_AddItemToObject(item, "effects", effects);
            }
            
            // Save gyro eye config if present (legacy)
            if (s.hasGyroEyeConfig) {
                cJSON* gyro = cJSON_CreateObject();
                cJSON_AddNumberToObject(gyro, "spriteId", s.gyroEye.spriteId);
                cJSON_AddNumberToObject(gyro, "intensity", s.gyroEye.intensity);
                cJSON_AddNumberToObject(gyro, "maxOffsetX", s.gyroEye.maxOffsetX);
                cJSON_AddNumberToObject(gyro, "maxOffsetY", s.gyroEye.maxOffsetY);
                cJSON_AddNumberToObject(gyro, "smoothingFactor", s.gyroEye.smoothingFactor);
                cJSON_AddNumberToObject(gyro, "eyeOffset", s.gyroEye.eyeOffset);
                cJSON_AddNumberToObject(gyro, "leftEyeCenterX", s.gyroEye.leftEyeCenterX);
                cJSON_AddNumberToObject(gyro, "leftEyeCenterY", s.gyroEye.leftEyeCenterY);
                cJSON_AddNumberToObject(gyro, "rightEyeCenterX", s.gyroEye.rightEyeCenterX);
                cJSON_AddNumberToObject(gyro, "rightEyeCenterY", s.gyroEye.rightEyeCenterY);
                cJSON_AddBoolToObject(gyro, "invertPitch", s.gyroEye.invertPitch);
                cJSON_AddBoolToObject(gyro, "invertRoll", s.gyroEye.invertRoll);
                cJSON_AddNumberToObject(gyro, "bgR", s.gyroEye.bgR);
                cJSON_AddNumberToObject(gyro, "bgG", s.gyroEye.bgG);
                cJSON_AddNumberToObject(gyro, "bgB", s.gyroEye.bgB);
                cJSON_AddItemToObject(item, "gyroEye", gyro);
            }
            
            // Save static sprite config if present (legacy)
            if (s.hasStaticSpriteConfig) {
                cJSON* sprite = cJSON_CreateObject();
                cJSON_AddNumberToObject(sprite, "spriteId", s.staticSprite.spriteId);
                cJSON_AddNumberToObject(sprite, "posX", s.staticSprite.posX);
                cJSON_AddNumberToObject(sprite, "posY", s.staticSprite.posY);
                cJSON_AddNumberToObject(sprite, "bgR", s.staticSprite.bgR);
                cJSON_AddNumberToObject(sprite, "bgG", s.staticSprite.bgG);
                cJSON_AddNumberToObject(sprite, "bgB", s.staticSprite.bgB);
                cJSON_AddItemToObject(item, "staticSprite", sprite);
            }
            
            cJSON_AddItemToArray(scenes, item);
        }
        cJSON_AddItemToObject(root, "scenes", scenes);
        
        char* json = cJSON_PrintUnformatted(root);
        cJSON_Delete(root);
        
        if (json) {
            FILE* f = fopen(indexPath, "w");
            if (f) {
                fprintf(f, "%s", json);
                fclose(f);
                ESP_LOGI(HTTP_TAG, "Saved %d scenes to %s", savedScenes_.size(),
                    sdcard_storage_ready_ ? "SD card" : "SPIFFS");
            } else {
                ESP_LOGE(HTTP_TAG, "Failed to open scene index for writing: %s", indexPath);
            }
            free(json);
        }
    }
    
    /**
     * @brief Create a fallback default scene when no scenes exist
     * Ensures the animation list is never empty
     */
    static void createFallbackDefaultScene() {
        SavedScene scene;
        scene.id = nextSceneId_++;
        scene.name = "Default Eyes";
        scene.type = 0;  // Generic
        scene.active = true;
        scene.displayEnabled = true;
        scene.ledsEnabled = false;
        scene.effectsOnly = false;
        scene.order = 0;
        scene.animType = "static_mirrored";  // Default to mirrored display
        scene.transition = "none";
        scene.spriteId = 0;
        scene.mirrorSprite = true;  // Default to mirrored (both eyes)
        scene.shaderAA = true;
        scene.shaderInvert = false;
        scene.shaderColorMode = "none";
        scene.shaderColor = "#ffffff";
        scene.ledR = 255;
        scene.ledG = 0;
        scene.ledB = 255;
        scene.ledBrightness = 80;
        
        // Set default params for mirrored display
        scene.params["mirror"] = 1.0f;
        scene.params["left_x"] = 32.0f;
        scene.params["left_y"] = 16.0f;
        scene.params["left_rotation"] = 0.0f;
        scene.params["left_scale"] = 1.0f;
        scene.params["right_x"] = 96.0f;
        scene.params["right_y"] = 16.0f;
        scene.params["right_rotation"] = 180.0f;
        scene.params["right_scale"] = 1.0f;
        
        scene.hasGyroEyeConfig = false;
        scene.hasStaticSpriteConfig = false;
        
        savedScenes_.push_back(scene);
        activeSceneId_ = scene.id;
        
        // Save to storage
        saveScenesStorage();
        
        ESP_LOGI(HTTP_TAG, "Created fallback default scene with ID %d", scene.id);
    }
    
    /**
     * @brief Load scenes from storage (SD card or SPIFFS)
     */
    static void loadScenesFromStorage() {
        if (!sdcard_storage_ready_ && !spiffs_initialized_) return;
        
        const char* indexPath = getSceneIndexPath();
        
        // Also try to migrate from SPIFFS to SD card
        if (sdcard_storage_ready_) {
            struct stat sdStat, spiffsStat;
            bool hasSpiffsData = (stat(SCENE_INDEX_FILE_SPIFFS, &spiffsStat) == 0);
            bool hasSDData = (stat(SCENE_INDEX_FILE, &sdStat) == 0);
            
            if (hasSpiffsData && !hasSDData) {
                ESP_LOGI(HTTP_TAG, "Migrating scenes from SPIFFS to SD card...");
                indexPath = SCENE_INDEX_FILE_SPIFFS;
            }
        }
        
        struct stat st;
        if (stat(indexPath, &st) != 0) {
            ESP_LOGI(HTTP_TAG, "No scene index found at %s, starting fresh", indexPath);
            return;
        }
        
        FILE* f = fopen(indexPath, "r");
        if (!f) {
            ESP_LOGE(HTTP_TAG, "Failed to open scene index for reading: %s", indexPath);
            return;
        }
        
        char* buf = (char*)malloc(st.st_size + 1);
        if (!buf) {
            fclose(f);
            return;
        }
        
        size_t bytesRead = fread(buf, 1, st.st_size, f);
        buf[bytesRead] = '\0';
        fclose(f);
        
        cJSON* root = cJSON_Parse(buf);
        free(buf);
        
        if (!root) {
            ESP_LOGE(HTTP_TAG, "Failed to parse scene index JSON");
            return;
        }
        
        cJSON* nextId = cJSON_GetObjectItem(root, "nextId");
        if (nextId && cJSON_IsNumber(nextId)) {
            nextSceneId_ = nextId->valueint;
        }
        
        cJSON* activeId = cJSON_GetObjectItem(root, "activeId");
        if (activeId && cJSON_IsNumber(activeId)) {
            activeSceneId_ = activeId->valueint;
        }
        
        cJSON* scenes = cJSON_GetObjectItem(root, "scenes");
        if (scenes && cJSON_IsArray(scenes)) {
            savedScenes_.clear();
            
            cJSON* item = NULL;
            cJSON_ArrayForEach(item, scenes) {
                SavedScene scene;
                
                cJSON* id = cJSON_GetObjectItem(item, "id");
                cJSON* name = cJSON_GetObjectItem(item, "name");
                cJSON* type = cJSON_GetObjectItem(item, "type");
                cJSON* active = cJSON_GetObjectItem(item, "active");
                
                if (id && cJSON_IsNumber(id)) scene.id = id->valueint;
                if (name && cJSON_IsString(name)) scene.name = name->valuestring;
                if (type && cJSON_IsNumber(type)) scene.type = type->valueint;
                if (active) scene.active = cJSON_IsTrue(active);
                
                // Load modular scene fields (new system)
                cJSON* val;
                if ((val = cJSON_GetObjectItem(item, "displayEnabled"))) {
                    scene.displayEnabled = cJSON_IsTrue(val);
                }
                if ((val = cJSON_GetObjectItem(item, "ledsEnabled"))) {
                    scene.ledsEnabled = cJSON_IsTrue(val);
                }
                if ((val = cJSON_GetObjectItem(item, "effectsOnly"))) {
                    scene.effectsOnly = cJSON_IsTrue(val);
                }
                if ((val = cJSON_GetObjectItem(item, "order")) && cJSON_IsNumber(val)) {
                    scene.order = val->valueint;
                }
                
                // Load shader settings
                if ((val = cJSON_GetObjectItem(item, "shaderAA"))) {
                    scene.shaderAA = cJSON_IsTrue(val);
                }
                if ((val = cJSON_GetObjectItem(item, "shaderInvert"))) {
                    scene.shaderInvert = cJSON_IsTrue(val);
                }
                if ((val = cJSON_GetObjectItem(item, "shaderColorMode")) && cJSON_IsString(val)) {
                    scene.shaderColorMode = val->valuestring;
                }
                if ((val = cJSON_GetObjectItem(item, "shaderColor")) && cJSON_IsString(val)) {
                    scene.shaderColor = val->valuestring;
                }
                
                // Load LED settings
                if ((val = cJSON_GetObjectItem(item, "ledR")) && cJSON_IsNumber(val)) {
                    scene.ledR = (uint8_t)val->valueint;
                }
                if ((val = cJSON_GetObjectItem(item, "ledG")) && cJSON_IsNumber(val)) {
                    scene.ledG = (uint8_t)val->valueint;
                }
                if ((val = cJSON_GetObjectItem(item, "ledB")) && cJSON_IsNumber(val)) {
                    scene.ledB = (uint8_t)val->valueint;
                }
                if ((val = cJSON_GetObjectItem(item, "ledBrightness")) && cJSON_IsNumber(val)) {
                    scene.ledBrightness = (uint8_t)val->valueint;
                }
                
                // Load background color (main scene level)
                if ((val = cJSON_GetObjectItem(item, "bgR")) && cJSON_IsNumber(val)) {
                    scene.bgR = (uint8_t)val->valueint;
                }
                if ((val = cJSON_GetObjectItem(item, "bgG")) && cJSON_IsNumber(val)) {
                    scene.bgG = (uint8_t)val->valueint;
                }
                if ((val = cJSON_GetObjectItem(item, "bgB")) && cJSON_IsNumber(val)) {
                    scene.bgB = (uint8_t)val->valueint;
                }
                
                // Load AnimationSystem fields
                if ((val = cJSON_GetObjectItem(item, "animType")) && cJSON_IsString(val)) {
                    scene.animType = val->valuestring;
                }
                if ((val = cJSON_GetObjectItem(item, "transition")) && cJSON_IsString(val)) {
                    scene.transition = val->valuestring;
                }
                if ((val = cJSON_GetObjectItem(item, "spriteId")) && cJSON_IsNumber(val)) {
                    scene.spriteId = val->valueint;
                }
                if ((val = cJSON_GetObjectItem(item, "mirrorSprite"))) {
                    scene.mirrorSprite = cJSON_IsTrue(val);
                }
                
                // Load params object
                cJSON* params = cJSON_GetObjectItem(item, "params");
                if (params && cJSON_IsObject(params)) {
                    cJSON* param = NULL;
                    cJSON_ArrayForEach(param, params) {
                        if (cJSON_IsNumber(param) && param->string) {
                            scene.params[param->string] = (float)param->valuedouble;
                            ESP_LOGI(HTTP_TAG, "  [LoadParam] '%s' = %.2f", param->string, (float)param->valuedouble);
                        }
                    }
                }
                ESP_LOGI(HTTP_TAG, "[LoadScene] id=%d animType='%s' params.size=%d", scene.id, scene.animType.c_str(), scene.params.size());
                
                // Load effects object
                cJSON* effects = cJSON_GetObjectItem(item, "effects");
                if (effects && cJSON_IsObject(effects)) {
                    cJSON* effect = NULL;
                    cJSON_ArrayForEach(effect, effects) {
                        if (cJSON_IsObject(effect) && effect->string) {
                            SavedScene::Effect eff;
                            cJSON* enabledVal = cJSON_GetObjectItem(effect, "enabled");
                            cJSON* intensityVal = cJSON_GetObjectItem(effect, "intensity");
                            if (enabledVal) eff.enabled = cJSON_IsTrue(enabledVal);
                            if (intensityVal && cJSON_IsNumber(intensityVal)) {
                                eff.intensity = (float)intensityVal->valuedouble;
                            }
                            scene.effects[effect->string] = eff;
                        }
                    }
                }
                
                // Load gyro eye config (legacy)
                cJSON* gyro = cJSON_GetObjectItem(item, "gyroEye");
                if (gyro) {
                    scene.hasGyroEyeConfig = true;
                    cJSON* val;
                    if ((val = cJSON_GetObjectItem(gyro, "spriteId"))) scene.gyroEye.spriteId = val->valueint;
                    if ((val = cJSON_GetObjectItem(gyro, "intensity"))) scene.gyroEye.intensity = val->valuedouble;
                    if ((val = cJSON_GetObjectItem(gyro, "maxOffsetX"))) scene.gyroEye.maxOffsetX = val->valuedouble;
                    if ((val = cJSON_GetObjectItem(gyro, "maxOffsetY"))) scene.gyroEye.maxOffsetY = val->valuedouble;
                    if ((val = cJSON_GetObjectItem(gyro, "smoothingFactor"))) scene.gyroEye.smoothingFactor = val->valuedouble;
                    if ((val = cJSON_GetObjectItem(gyro, "eyeOffset"))) scene.gyroEye.eyeOffset = val->valueint;
                    if ((val = cJSON_GetObjectItem(gyro, "leftEyeCenterX"))) scene.gyroEye.leftEyeCenterX = val->valueint;
                    if ((val = cJSON_GetObjectItem(gyro, "leftEyeCenterY"))) scene.gyroEye.leftEyeCenterY = val->valueint;
                    if ((val = cJSON_GetObjectItem(gyro, "rightEyeCenterX"))) scene.gyroEye.rightEyeCenterX = val->valueint;
                    if ((val = cJSON_GetObjectItem(gyro, "rightEyeCenterY"))) scene.gyroEye.rightEyeCenterY = val->valueint;
                    if ((val = cJSON_GetObjectItem(gyro, "invertPitch"))) scene.gyroEye.invertPitch = cJSON_IsTrue(val);
                    if ((val = cJSON_GetObjectItem(gyro, "invertRoll"))) scene.gyroEye.invertRoll = cJSON_IsTrue(val);
                    if ((val = cJSON_GetObjectItem(gyro, "bgR"))) scene.gyroEye.bgR = val->valueint;
                    if ((val = cJSON_GetObjectItem(gyro, "bgG"))) scene.gyroEye.bgG = val->valueint;
                    if ((val = cJSON_GetObjectItem(gyro, "bgB"))) scene.gyroEye.bgB = val->valueint;
                }
                
                // Load static sprite config (legacy)
                cJSON* sprite = cJSON_GetObjectItem(item, "staticSprite");
                if (sprite) {
                    scene.hasStaticSpriteConfig = true;
                    cJSON* val;
                    if ((val = cJSON_GetObjectItem(sprite, "spriteId"))) scene.staticSprite.spriteId = val->valueint;
                    if ((val = cJSON_GetObjectItem(sprite, "posX"))) scene.staticSprite.posX = val->valueint;
                    if ((val = cJSON_GetObjectItem(sprite, "posY"))) scene.staticSprite.posY = val->valueint;
                    if ((val = cJSON_GetObjectItem(sprite, "bgR"))) scene.staticSprite.bgR = val->valueint;
                    if ((val = cJSON_GetObjectItem(sprite, "bgG"))) scene.staticSprite.bgG = val->valueint;
                    if ((val = cJSON_GetObjectItem(sprite, "bgB"))) scene.staticSprite.bgB = val->valueint;
                }
                
                // MIGRATION: Force unknown animation types to static_sprite
                // Known types: static_sprite, static_mirrored, static_image, reactive_eyes
                if (scene.animType != "static_sprite" && 
                    scene.animType != "static_mirrored" && 
                    scene.animType != "static_image" &&
                    scene.animType != "reactive_eyes") {
                    ESP_LOGW(HTTP_TAG, "Migrating scene '%s' from animType '%s' to 'static_sprite'",
                             scene.name.c_str(), scene.animType.c_str());
                    scene.animType = "static_sprite";
                }
                
                savedScenes_.push_back(scene);
            }
            
            ESP_LOGI(HTTP_TAG, "Loaded %d scenes from %s", savedScenes_.size(),
                sdcard_storage_ready_ ? "SD card" : "SPIFFS");
            
            // If we loaded from SPIFFS but SD is now ready, migrate to SD
            if (sdcard_storage_ready_ && strcmp(indexPath, SCENE_INDEX_FILE_SPIFFS) == 0) {
                ESP_LOGI(HTTP_TAG, "Saving scenes to SD card after migration");
                saveScenesStorage();
            }
            
            // Auto-activate the scene that was marked as active
            if (activeSceneId_ >= 0) {
                for (auto& s : savedScenes_) {
                    if (s.id == activeSceneId_ && s.active) {
                        ESP_LOGI(HTTP_TAG, "Auto-activating scene: %s (id=%d, animType=%s)", 
                                 s.name.c_str(), s.id, s.animType.c_str());
                        auto& callback = getSceneActivatedCallback();
                        if (callback) {
                            callback(s);
                        }
                        break;
                    }
                }
            }
        }
        
        cJSON_Delete(root);
    }
    
    /**
     * @brief Load equations from storage (SD card or SPIFFS)
     */
    static void loadEquationsFromStorage() {
        if (!sdcard_storage_ready_ && !spiffs_initialized_) return;
        
        const char* indexPath = getEquationIndexPath();
        
        // Also try to migrate from SPIFFS to SD card if we have SD but data is in SPIFFS
        if (sdcard_storage_ready_) {
            struct stat sdStat, spiffsStat;
            bool hasSpiffsData = (stat(EQUATION_INDEX_FILE_SPIFFS, &spiffsStat) == 0);
            bool hasSDData = (stat(EQUATION_INDEX_FILE, &sdStat) == 0);
            
            // If we have SPIFFS data but no SD data, migrate it
            if (hasSpiffsData && !hasSDData) {
                ESP_LOGI(HTTP_TAG, "Migrating equations from SPIFFS to SD card...");
                indexPath = EQUATION_INDEX_FILE_SPIFFS;  // Load from SPIFFS
            }
        }
        
        struct stat st;
        if (stat(indexPath, &st) != 0) {
            ESP_LOGI(HTTP_TAG, "No equation index found at %s, starting fresh", indexPath);
            return;
        }
        
        FILE* f = fopen(indexPath, "r");
        if (!f) {
            ESP_LOGE(HTTP_TAG, "Failed to open equation index for reading: %s", indexPath);
            return;
        }
        
        char* buf = (char*)malloc(st.st_size + 1);
        if (!buf) {
            fclose(f);
            return;
        }
        
        size_t bytesRead = fread(buf, 1, st.st_size, f);
        buf[bytesRead] = '\0';
        fclose(f);
        
        cJSON* root = cJSON_Parse(buf);
        free(buf);
        
        if (!root) {
            ESP_LOGE(HTTP_TAG, "Failed to parse equation index JSON");
            return;
        }
        
        cJSON* nextId = cJSON_GetObjectItem(root, "nextId");
        if (nextId && cJSON_IsNumber(nextId)) {
            nextEquationId_ = nextId->valueint;
        }
        
        cJSON* equations = cJSON_GetObjectItem(root, "equations");
        if (equations && cJSON_IsArray(equations)) {
            savedEquations_.clear();
            
            cJSON* item = NULL;
            cJSON_ArrayForEach(item, equations) {
                SavedEquation eq;
                
                cJSON* id = cJSON_GetObjectItem(item, "id");
                cJSON* name = cJSON_GetObjectItem(item, "name");
                cJSON* expression = cJSON_GetObjectItem(item, "expression");
                cJSON* variables = cJSON_GetObjectItem(item, "variables");
                
                if (id && cJSON_IsNumber(id)) eq.id = id->valueint;
                if (name && cJSON_IsString(name)) eq.name = name->valuestring;
                if (expression && cJSON_IsString(expression)) eq.expression = expression->valuestring;
                
                if (variables && cJSON_IsArray(variables)) {
                    cJSON* varItem = NULL;
                    cJSON_ArrayForEach(varItem, variables) {
                        EquationVariable v;
                        cJSON* vname = cJSON_GetObjectItem(varItem, "name");
                        cJSON* vtype = cJSON_GetObjectItem(varItem, "type");
                        cJSON* vvalue = cJSON_GetObjectItem(varItem, "value");
                        
                        if (vname && cJSON_IsString(vname)) v.name = vname->valuestring;
                        if (vtype && cJSON_IsString(vtype)) v.type = vtype->valuestring;
                        if (vvalue && cJSON_IsString(vvalue)) v.value = vvalue->valuestring;
                        
                        eq.variables.push_back(v);
                    }
                }
                
                savedEquations_.push_back(eq);
            }
            
            ESP_LOGI(HTTP_TAG, "Loaded %d equations from %s", savedEquations_.size(),
                sdcard_storage_ready_ ? "SD card" : "SPIFFS");
            
            // If we loaded from SPIFFS but SD is now ready, migrate to SD
            if (sdcard_storage_ready_ && strcmp(indexPath, EQUATION_INDEX_FILE_SPIFFS) == 0) {
                ESP_LOGI(HTTP_TAG, "Saving equations to SD card after migration");
                saveEquationsToStorage();
            }
        }
        
        cJSON_Delete(root);
    }
    
    // ========== LED Preset Storage Functions ==========
    
    /**
     * @brief Get the LED preset index file path
     * Uses SD card only (no SPIFFS fallback for LED presets)
     */
    static const char* getLedPresetIndexPath() {
        return LED_PRESET_INDEX_FILE;
    }
    
    /**
     * @brief Save LED presets to storage (SD card)
     */
    static void saveLedPresetsStorage() {
        if (!sdcard_storage_ready_) {
            ESP_LOGW(HTTP_TAG, "SD card not ready for LED preset storage");
            return;
        }
        
        // Create LED presets directory if it doesn't exist
        struct stat st;
        if (stat(LED_PRESET_DIR, &st) != 0) {
            mkdir(LED_PRESET_DIR, 0755);
        }
        
        cJSON* root = cJSON_CreateObject();
        cJSON_AddNumberToObject(root, "nextId", nextLedPresetId_);
        cJSON_AddNumberToObject(root, "activeId", activeLedPresetId_);
        cJSON_AddStringToObject(root, "storage", "sdcard");
        
        cJSON* presets = cJSON_CreateArray();
        for (const auto& p : savedLedPresets_) {
            cJSON* item = cJSON_CreateObject();
            cJSON_AddNumberToObject(item, "id", p.id);
            cJSON_AddStringToObject(item, "name", p.name.c_str());
            cJSON_AddStringToObject(item, "animation", p.animation.c_str());
            cJSON_AddNumberToObject(item, "r", p.r);
            cJSON_AddNumberToObject(item, "g", p.g);
            cJSON_AddNumberToObject(item, "b", p.b);
            cJSON_AddNumberToObject(item, "brightness", p.brightness);
            cJSON_AddNumberToObject(item, "speed", p.speed);
            cJSON_AddNumberToObject(item, "order", p.order);
            
            // Save params object
            if (!p.params.empty()) {
                cJSON* params = cJSON_CreateObject();
                for (const auto& kv : p.params) {
                    cJSON_AddNumberToObject(params, kv.first.c_str(), kv.second);
                }
                cJSON_AddItemToObject(item, "params", params);
            }
            
            // Save color count and colors array
            cJSON_AddNumberToObject(item, "colorCount", p.colorCount);
            if (!p.colors.empty()) {
                cJSON* colorsArr = cJSON_CreateArray();
                for (const auto& c : p.colors) {
                    cJSON* colorItem = cJSON_CreateObject();
                    cJSON_AddNumberToObject(colorItem, "r", std::get<0>(c));
                    cJSON_AddNumberToObject(colorItem, "g", std::get<1>(c));
                    cJSON_AddNumberToObject(colorItem, "b", std::get<2>(c));
                    cJSON_AddItemToArray(colorsArr, colorItem);
                }
                cJSON_AddItemToObject(item, "colors", colorsArr);
            }
            
            cJSON_AddItemToArray(presets, item);
        }
        cJSON_AddItemToObject(root, "presets", presets);
        
        char* json = cJSON_PrintUnformatted(root);
        cJSON_Delete(root);
        
        if (json) {
            FILE* f = fopen(LED_PRESET_INDEX_FILE, "w");
            if (f) {
                fprintf(f, "%s", json);
                fclose(f);
                ESP_LOGI(HTTP_TAG, "Saved %d LED presets to SD card", savedLedPresets_.size());
            } else {
                ESP_LOGE(HTTP_TAG, "Failed to open LED preset index for writing: %s", LED_PRESET_INDEX_FILE);
            }
            free(json);
        }
    }
    
    /**
     * @brief Load LED presets from storage (SD card)
     */
    static void loadLedPresetsFromStorage() {
        if (!sdcard_storage_ready_) {
            ESP_LOGI(HTTP_TAG, "SD card not ready, skipping LED preset load");
            return;
        }
        
        struct stat st;
        if (stat(LED_PRESET_INDEX_FILE, &st) != 0) {
            ESP_LOGI(HTTP_TAG, "No LED preset index found, creating defaults");
            createDefaultLedPresets();
            return;
        }
        
        FILE* f = fopen(LED_PRESET_INDEX_FILE, "r");
        if (!f) {
            ESP_LOGE(HTTP_TAG, "Failed to open LED preset index for reading");
            return;
        }
        
        char* buf = (char*)malloc(st.st_size + 1);
        if (!buf) {
            fclose(f);
            return;
        }
        
        size_t bytesRead = fread(buf, 1, st.st_size, f);
        buf[bytesRead] = '\0';
        fclose(f);
        
        cJSON* root = cJSON_Parse(buf);
        free(buf);
        
        if (!root) {
            ESP_LOGE(HTTP_TAG, "Failed to parse LED preset index JSON");
            return;
        }
        
        cJSON* nextId = cJSON_GetObjectItem(root, "nextId");
        if (nextId && cJSON_IsNumber(nextId)) {
            nextLedPresetId_ = nextId->valueint;
        }
        
        cJSON* activeId = cJSON_GetObjectItem(root, "activeId");
        if (activeId && cJSON_IsNumber(activeId)) {
            activeLedPresetId_ = activeId->valueint;
        }
        
        cJSON* presets = cJSON_GetObjectItem(root, "presets");
        if (presets && cJSON_IsArray(presets)) {
            savedLedPresets_.clear();
            
            cJSON* item = NULL;
            cJSON_ArrayForEach(item, presets) {
                SavedLedPreset preset;
                
                cJSON* val;
                if ((val = cJSON_GetObjectItem(item, "id")) && cJSON_IsNumber(val)) {
                    preset.id = val->valueint;
                }
                if ((val = cJSON_GetObjectItem(item, "name")) && cJSON_IsString(val)) {
                    preset.name = val->valuestring;
                }
                if ((val = cJSON_GetObjectItem(item, "animation")) && cJSON_IsString(val)) {
                    preset.animation = val->valuestring;
                }
                if ((val = cJSON_GetObjectItem(item, "r")) && cJSON_IsNumber(val)) {
                    preset.r = (uint8_t)val->valueint;
                }
                if ((val = cJSON_GetObjectItem(item, "g")) && cJSON_IsNumber(val)) {
                    preset.g = (uint8_t)val->valueint;
                }
                if ((val = cJSON_GetObjectItem(item, "b")) && cJSON_IsNumber(val)) {
                    preset.b = (uint8_t)val->valueint;
                }
                if ((val = cJSON_GetObjectItem(item, "brightness")) && cJSON_IsNumber(val)) {
                    preset.brightness = (uint8_t)val->valueint;
                }
                if ((val = cJSON_GetObjectItem(item, "speed")) && cJSON_IsNumber(val)) {
                    preset.speed = (int8_t)val->valueint;
                }
                if ((val = cJSON_GetObjectItem(item, "order")) && cJSON_IsNumber(val)) {
                    preset.order = val->valueint;
                }
                
                // Load params object
                cJSON* params = cJSON_GetObjectItem(item, "params");
                if (params && cJSON_IsObject(params)) {
                    cJSON* param = NULL;
                    cJSON_ArrayForEach(param, params) {
                        if (cJSON_IsNumber(param) && param->string) {
                            preset.params[param->string] = param->valueint;
                        }
                    }
                }
                
                // Load colorCount and colors array
                if ((val = cJSON_GetObjectItem(item, "colorCount")) && cJSON_IsNumber(val)) {
                    preset.colorCount = val->valueint;
                }
                cJSON* colorsArr = cJSON_GetObjectItem(item, "colors");
                if (colorsArr && cJSON_IsArray(colorsArr)) {
                    cJSON* colorItem = NULL;
                    cJSON_ArrayForEach(colorItem, colorsArr) {
                        int cr = 255, cg = 255, cb = 255;
                        cJSON* cv;
                        if ((cv = cJSON_GetObjectItem(colorItem, "r")) && cJSON_IsNumber(cv)) cr = cv->valueint;
                        if ((cv = cJSON_GetObjectItem(colorItem, "g")) && cJSON_IsNumber(cv)) cg = cv->valueint;
                        if ((cv = cJSON_GetObjectItem(colorItem, "b")) && cJSON_IsNumber(cv)) cb = cv->valueint;
                        preset.colors.push_back(std::make_tuple((uint8_t)cr, (uint8_t)cg, (uint8_t)cb));
                    }
                }
                
                savedLedPresets_.push_back(preset);
            }
            
            ESP_LOGI(HTTP_TAG, "Loaded %d LED presets from SD card", savedLedPresets_.size());
        }
        
        cJSON_Delete(root);
    }
    
    /**
     * @brief Create default LED presets if none exist
     */
    static void createDefaultLedPresets() {
        // Solid Pink - default active
        SavedLedPreset solid;
        solid.id = nextLedPresetId_++;
        solid.name = "Solid Pink";
        solid.animation = "solid";
        solid.r = 255; solid.g = 0; solid.b = 255;
        solid.brightness = 80;
        solid.speed = 50;
        solid.order = 0;
        solid.colorCount = 1;
        solid.colors.push_back(std::make_tuple((uint8_t)255, (uint8_t)0, (uint8_t)255));
        savedLedPresets_.push_back(solid);
        
        // Rainbow Cycle
        SavedLedPreset rainbow;
        rainbow.id = nextLedPresetId_++;
        rainbow.name = "Rainbow Cycle";
        rainbow.animation = "rainbow";
        rainbow.r = 255; rainbow.g = 255; rainbow.b = 255;
        rainbow.brightness = 100;
        rainbow.speed = 50;
        rainbow.order = 1;
        rainbow.colorCount = 1;
        rainbow.colors.push_back(std::make_tuple((uint8_t)255, (uint8_t)255, (uint8_t)255));
        savedLedPresets_.push_back(rainbow);
        
        // Breathing Blue
        SavedLedPreset breathe;
        breathe.id = nextLedPresetId_++;
        breathe.name = "Breathing Blue";
        breathe.animation = "breathe";
        breathe.r = 0; breathe.g = 100; breathe.b = 255;
        breathe.brightness = 100;
        breathe.speed = 30;
        breathe.order = 2;
        breathe.colorCount = 1;
        breathe.colors.push_back(std::make_tuple((uint8_t)0, (uint8_t)100, (uint8_t)255));
        savedLedPresets_.push_back(breathe);
        
        // Fire Effect
        SavedLedPreset fire;
        fire.id = nextLedPresetId_++;
        fire.name = "Fire Effect";
        fire.animation = "fire";
        fire.r = 255; fire.g = 50; fire.b = 0;
        fire.brightness = 100;
        fire.speed = 70;
        fire.order = 3;
        fire.colorCount = 1;
        fire.colors.push_back(std::make_tuple((uint8_t)255, (uint8_t)50, (uint8_t)0));
        savedLedPresets_.push_back(fire);
        
        // Sunset Gradient - Orange to Purple
        SavedLedPreset sunset;
        sunset.id = nextLedPresetId_++;
        sunset.name = "Sunset Gradient";
        sunset.animation = "gradient";
        sunset.r = 255; sunset.g = 100; sunset.b = 0;
        sunset.brightness = 100;
        sunset.speed = 50;
        sunset.order = 4;
        sunset.colorCount = 4;
        sunset.colors.push_back(std::make_tuple((uint8_t)255, (uint8_t)60, (uint8_t)0));    // Deep orange
        sunset.colors.push_back(std::make_tuple((uint8_t)255, (uint8_t)0, (uint8_t)80));    // Red-pink
        sunset.colors.push_back(std::make_tuple((uint8_t)180, (uint8_t)0, (uint8_t)180));   // Purple
        sunset.colors.push_back(std::make_tuple((uint8_t)80, (uint8_t)0, (uint8_t)200));    // Deep violet
        savedLedPresets_.push_back(sunset);
        
        // Ocean Wave - Cyan to Deep Blue
        SavedLedPreset ocean;
        ocean.id = nextLedPresetId_++;
        ocean.name = "Ocean Wave";
        ocean.animation = "wave";
        ocean.r = 0; ocean.g = 200; ocean.b = 255;
        ocean.brightness = 100;
        ocean.speed = 40;
        ocean.order = 5;
        ocean.colorCount = 1;
        ocean.colors.push_back(std::make_tuple((uint8_t)0, (uint8_t)200, (uint8_t)255));
        savedLedPresets_.push_back(ocean);
        
        // Neon Chase - Hot Pink
        SavedLedPreset neonChase;
        neonChase.id = nextLedPresetId_++;
        neonChase.name = "Neon Chase";
        neonChase.animation = "chase";
        neonChase.r = 255; neonChase.g = 0; neonChase.b = 128;
        neonChase.brightness = 100;
        neonChase.speed = 80;
        neonChase.order = 6;
        neonChase.colorCount = 1;
        neonChase.colors.push_back(std::make_tuple((uint8_t)255, (uint8_t)0, (uint8_t)128));
        savedLedPresets_.push_back(neonChase);
        
        // Cyberpunk - Magenta to Cyan rainbow
        SavedLedPreset cyber;
        cyber.id = nextLedPresetId_++;
        cyber.name = "Cyberpunk";
        cyber.animation = "rainbow";
        cyber.r = 255; cyber.g = 0; cyber.b = 255;
        cyber.brightness = 100;
        cyber.speed = 60;
        cyber.order = 7;
        cyber.colorCount = 3;
        cyber.colors.push_back(std::make_tuple((uint8_t)255, (uint8_t)0, (uint8_t)255));   // Magenta
        cyber.colors.push_back(std::make_tuple((uint8_t)0, (uint8_t)255, (uint8_t)255));   // Cyan
        cyber.colors.push_back(std::make_tuple((uint8_t)255, (uint8_t)0, (uint8_t)128));   // Hot pink
        savedLedPresets_.push_back(cyber);
        
        // Golden Sparkle
        SavedLedPreset sparkle;
        sparkle.id = nextLedPresetId_++;
        sparkle.name = "Golden Sparkle";
        sparkle.animation = "sparkle";
        sparkle.r = 255; sparkle.g = 180; sparkle.b = 0;
        sparkle.brightness = 100;
        sparkle.speed = 70;
        sparkle.order = 8;
        sparkle.colorCount = 1;
        sparkle.colors.push_back(std::make_tuple((uint8_t)255, (uint8_t)180, (uint8_t)0));
        savedLedPresets_.push_back(sparkle);
        
        activeLedPresetId_ = solid.id;  // Set solid pink as default
        saveLedPresetsStorage();
        
        ESP_LOGI(HTTP_TAG, "Created %d default LED presets", savedLedPresets_.size());
    }
    
    // ========== Authentication Helpers ==========
    
    /**
     * @brief Check if request is coming from external network (not direct AP)
     * 
     * If the device is connected to an external network (APSTA mode),
     * requests can come either from:
     * - AP clients (192.168.4.x) - these are direct device connections
     * - STA network (the external network IP range) - these need auth
     */
    static bool isExternalNetworkRequest(httpd_req_t* req) {
        auto& state = SYNC_STATE.state();
        
        // If external WiFi is not connected, all requests are from AP
        if (!state.extWifiIsConnected) {
            return false;
        }
        
        // Get client IP from the request
        int sockfd = httpd_req_to_sockfd(req);
        struct sockaddr_in6 addr;
        socklen_t addr_len = sizeof(addr);
        
        if (getpeername(sockfd, (struct sockaddr*)&addr, &addr_len) != 0) {
            return false;  // Can't determine, assume safe
        }
        
        // Convert to IPv4 if mapped
        uint32_t client_ip = 0;
        if (addr.sin6_family == AF_INET) {
            client_ip = ((struct sockaddr_in*)&addr)->sin_addr.s_addr;
        } else if (addr.sin6_family == AF_INET6) {
            // Check for IPv4-mapped address
            if (IN6_IS_ADDR_V4MAPPED(&addr.sin6_addr)) {
                memcpy(&client_ip, &addr.sin6_addr.s6_addr[12], 4);
            }
        }
        
        // AP subnet is 192.168.4.0/24 = 0xC0A80400
        // Mask: 255.255.255.0 = 0xFFFFFF00
        uint32_t ap_network = 0x0404A8C0;  // 192.168.4.0 in little-endian
        uint32_t ap_mask = 0x00FFFFFF;     // 255.255.255.0 in little-endian
        
        // If client is on AP subnet, it's a direct connection
        if ((client_ip & ap_mask) == (ap_network & ap_mask)) {
            ESP_LOGD(HTTP_TAG, "Request from AP client (direct connection)");
            return false;
        }
        
        // Otherwise, it's from the external network
        ESP_LOGI(HTTP_TAG, "Request from external network client");
        return true;
    }
    
    /**
     * @brief Check if request has valid authentication token (cookie)
     */
    static bool isAuthenticated(httpd_req_t* req) {
        auto& state = SYNC_STATE.state();
        
        // If auth not enabled, everyone is authenticated
        if (!state.authEnabled || strlen(state.authPassword) == 0) {
            return true;
        }
        
        // Get cookie header
        char cookie[128] = {0};
        if (httpd_req_get_hdr_value_str(req, "Cookie", cookie, sizeof(cookie)) != ESP_OK) {
            return false;
        }
        
        // Look for auth token in cookie
        // Format: auth_token=<token>
        char* token_start = strstr(cookie, "auth_token=");
        if (!token_start) {
            return false;
        }
        
        token_start += 11;  // Skip "auth_token="
        char* token_end = strchr(token_start, ';');
        
        char token[65] = {0};
        size_t token_len = token_end ? (token_end - token_start) : strlen(token_start);
        if (token_len >= sizeof(token)) token_len = sizeof(token) - 1;
        strncpy(token, token_start, token_len);
        
        // Compare with stored session token
        return strcmp(token, state.authSessionToken) == 0 && strlen(token) > 0;
    }
    
    /**
     * @brief Check if auth is required and redirect to login if needed
     * @return true if request should be blocked (redirected to login)
     */
    static bool requiresAuthRedirect(httpd_req_t* req) {
        auto& state = SYNC_STATE.state();
        
        // Auth only required when:
        // 1. External WiFi is connected
        // 2. Auth is enabled
        // 3. Request is from external network
        // 4. User is not authenticated
        
        if (!state.extWifiIsConnected) return false;
        if (!state.authEnabled) return false;
        if (strlen(state.authPassword) == 0) return false;
        if (!isExternalNetworkRequest(req)) return false;
        if (isAuthenticated(req)) return false;
        
        return true;
    }
    
    /**
     * @brief Redirect to login page
     */
    static esp_err_t redirectToLogin(httpd_req_t* req) {
        httpd_resp_set_status(req, "302 Found");
        httpd_resp_set_hdr(req, "Location", "/login");
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    }
    
    /**
     * @brief Check if auth is required for JSON API endpoints
     * @return true if request should be blocked
     */
    static bool requiresAuthJson(httpd_req_t* req) {
        auto& state = SYNC_STATE.state();
        
        // Same logic as requiresAuthRedirect but for API endpoints
        if (!state.extWifiIsConnected) return false;
        if (!state.authEnabled) return false;
        if (strlen(state.authPassword) == 0) return false;
        if (!isExternalNetworkRequest(req)) return false;
        if (isAuthenticated(req)) return false;
        
        return true;
    }
    
    /**
     * @brief Send JSON error response
     */
    static esp_err_t sendJsonError(httpd_req_t* req, int statusCode, const char* message) {
        char statusStr[32];
        snprintf(statusStr, sizeof(statusStr), "%d", statusCode);
        
        char response[256];
        snprintf(response, sizeof(response), 
                 "{\"success\":false,\"error\":\"%s\"}", message);
        
        httpd_resp_set_status(req, statusCode == 401 ? "401 Unauthorized" : 
                                   statusCode == 400 ? "400 Bad Request" :
                                   statusCode == 404 ? "404 Not Found" : 
                                   statusCode == 500 ? "500 Internal Server Error" : "400 Bad Request");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    // ========== Login Page ==========
    
    static const char* getLoginPage() {
        return R"rawliteral(<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Login - Lucidius</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body { background: #0a0a0a; color: #fff; font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; min-height: 100vh; display: flex; align-items: center; justify-content: center; }
    .login-container { background: #141414; border-radius: 16px; padding: 40px; width: 100%; max-width: 400px; margin: 20px; border: 1px solid #222; }
    h1 { text-align: center; margin-bottom: 8px; color: #ff6b00; }
    .subtitle { text-align: center; color: #888; margin-bottom: 32px; font-size: 14px; }
    .warning { background: rgba(255, 59, 48, 0.1); border: 1px solid rgba(255, 59, 48, 0.3); border-radius: 8px; padding: 12px 16px; margin-bottom: 24px; color: #ff6b6b; font-size: 13px; text-align: center; }
    .form-group { margin-bottom: 20px; }
    label { display: block; color: #888; font-size: 13px; margin-bottom: 8px; }
    input { width: 100%; padding: 14px 16px; background: #1a1a1a; border: 1px solid #333; border-radius: 8px; color: #fff; font-size: 16px; transition: border-color 0.2s; }
    input:focus { outline: none; border-color: #ff6b00; }
    .btn { width: 100%; padding: 14px; background: linear-gradient(135deg, #ff6b00, #ff8533); color: #fff; border: none; border-radius: 8px; font-size: 16px; font-weight: 600; cursor: pointer; transition: transform 0.2s, box-shadow 0.2s; }
    .btn:hover { transform: translateY(-2px); box-shadow: 0 4px 20px rgba(255, 107, 0, 0.3); }
    .btn:active { transform: translateY(0); }
    .error { color: #ff6b6b; font-size: 13px; margin-top: 16px; text-align: center; display: none; }
    .error.show { display: block; }
  </style>
</head>
<body>
  <div class="login-container">
    <h1>Lucidius</h1>
    <p class="subtitle">External Network Access</p>
    <div class="warning">
      You are connecting via an external network.<br>
      Authentication is required for security.
    </div>
    <form id="login-form">
      <div class="form-group">
        <label for="username">Username</label>
        <input type="text" id="username" name="username" autocomplete="username" required>
      </div>
      <div class="form-group">
        <label for="password">Password</label>
        <input type="password" id="password" name="password" autocomplete="current-password" required>
      </div>
      <button type="submit" class="btn">Log In</button>
      <p class="error" id="error-msg">Invalid username or password</p>
    </form>
  </div>
  <script>
    document.getElementById('login-form').addEventListener('submit', function(e) {
      e.preventDefault();
      var username = document.getElementById('username').value;
      var password = document.getElementById('password').value;
      
      fetch('/api/login', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ username: username, password: password })
      })
      .then(r => r.json())
      .then(data => {
        if (data.success) {
          window.location.href = '/';
        } else {
          document.getElementById('error-msg').classList.add('show');
        }
      })
      .catch(err => {
        document.getElementById('error-msg').textContent = 'Connection error';
        document.getElementById('error-msg').classList.add('show');
      });
    });
  </script>
</body>
</html>)rawliteral";
    }
    
    static esp_err_t handleLoginPage(httpd_req_t* req) {
        // If already authenticated or not from external network, redirect to home
        auto& state = SYNC_STATE.state();
        if (!state.extWifiIsConnected || !state.authEnabled || 
            !isExternalNetworkRequest(req) || isAuthenticated(req)) {
            httpd_resp_set_status(req, "302 Found");
            httpd_resp_set_hdr(req, "Location", "/");
            httpd_resp_send(req, NULL, 0);
            return ESP_OK;
        }
        
        httpd_resp_set_type(req, "text/html");
        httpd_resp_send(req, getLoginPage(), HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    static esp_err_t handleApiLogin(httpd_req_t* req) {
        char buf[256];
        int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
        if (ret <= 0) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No body");
            return ESP_FAIL;
        }
        buf[ret] = '\0';
        
        cJSON* root = cJSON_Parse(buf);
        if (!root) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
            return ESP_FAIL;
        }
        
        cJSON* username = cJSON_GetObjectItem(root, "username");
        cJSON* password = cJSON_GetObjectItem(root, "password");
        
        auto& state = SYNC_STATE.state();
        bool success = false;
        
        if (username && password && username->valuestring && password->valuestring) {
            // Check credentials
            if (strcmp(username->valuestring, state.authUsername) == 0 &&
                strcmp(password->valuestring, state.authPassword) == 0) {
                
                // Generate session token
                uint32_t r1 = esp_random();
                uint32_t r2 = esp_random();
                uint32_t r3 = esp_random();
                uint32_t r4 = esp_random();
                snprintf(state.authSessionToken, sizeof(state.authSessionToken),
                        "%08lx%08lx%08lx%08lx", 
                        (unsigned long)r1, (unsigned long)r2, 
                        (unsigned long)r3, (unsigned long)r4);
                
                success = true;
                ESP_LOGI(HTTP_TAG, "Login successful for user: %s", state.authUsername);
            } else {
                ESP_LOGW(HTTP_TAG, "Login failed for user: %s", username->valuestring);
            }
        }
        
        cJSON_Delete(root);
        
        httpd_resp_set_type(req, "application/json");
        
        if (success) {
            // Set auth cookie
            char cookie[128];
            snprintf(cookie, sizeof(cookie), "auth_token=%s; Path=/; HttpOnly; SameSite=Strict",
                    state.authSessionToken);
            httpd_resp_set_hdr(req, "Set-Cookie", cookie);
            httpd_resp_send(req, "{\"success\":true}", HTTPD_RESP_USE_STRLEN);
        } else {
            httpd_resp_send(req, "{\"success\":false,\"error\":\"Invalid credentials\"}", HTTPD_RESP_USE_STRLEN);
        }
        
        return ESP_OK;
    }
    
    static esp_err_t handleApiLogout(httpd_req_t* req) {
        auto& state = SYNC_STATE.state();
        
        // Clear session token
        memset(state.authSessionToken, 0, sizeof(state.authSessionToken));
        
        // Clear cookie
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_hdr(req, "Set-Cookie", "auth_token=; Path=/; Expires=Thu, 01 Jan 1970 00:00:00 GMT");
        httpd_resp_send(req, "{\"success\":true}", HTTPD_RESP_USE_STRLEN);
        
        ESP_LOGI(HTTP_TAG, "User logged out");
        return ESP_OK;
    }
    
    // ========== Page Handlers ==========
    
    static esp_err_t handlePageBasic(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return redirectToLogin(req);
        
        ESP_LOGI(HTTP_TAG, "Serving Basic page");
        httpd_resp_set_type(req, "text/html");
        httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
        httpd_resp_send(req, Content::PAGE_BASIC, HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    static esp_err_t handlePageSystem(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return redirectToLogin(req);
        
        ESP_LOGI(HTTP_TAG, "Serving System page");
        httpd_resp_set_type(req, "text/html");
        httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
        httpd_resp_send(req, Content::PAGE_SYSTEM, HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    static esp_err_t handlePageAdvancedMenu(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return redirectToLogin(req);
        
        ESP_LOGI(HTTP_TAG, "Serving Advanced Menu page");
        httpd_resp_set_type(req, "text/html");
        httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
        httpd_resp_send(req, Content::PAGE_ADVANCED_MENU, HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    static esp_err_t handlePageSceneList(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return redirectToLogin(req);
        
        ESP_LOGI(HTTP_TAG, "Serving Scene List page");
        httpd_resp_set_type(req, "text/html");
        httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
        httpd_resp_send(req, Content::PAGE_SCENE_LIST, HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    static esp_err_t handlePageSceneEdit(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return redirectToLogin(req);
        
        ESP_LOGI(HTTP_TAG, "Serving Scene Edit page");
        httpd_resp_set_type(req, "text/html");
        httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
        httpd_resp_send(req, Content::PAGE_SCENE_EDIT, HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    static esp_err_t handlePageLedPresetList(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return redirectToLogin(req);
        
        ESP_LOGI(HTTP_TAG, "Serving LED Preset List page");
        httpd_resp_set_type(req, "text/html");
        httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
        httpd_resp_send(req, Content::PAGE_LED_PRESET_LIST, HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    static esp_err_t handlePageLedPresetEdit(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return redirectToLogin(req);
        
        ESP_LOGI(HTTP_TAG, "Serving LED Preset Edit page");
        httpd_resp_set_type(req, "text/html");
        httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
        httpd_resp_send(req, Content::PAGE_LED_PRESET_EDIT, HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    static esp_err_t handlePageSprite(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return redirectToLogin(req);
        
        ESP_LOGI(HTTP_TAG, "Serving Sprite page");
        httpd_resp_set_type(req, "text/html");
        httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
        httpd_resp_send(req, Content::PAGE_SPRITE, HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    static esp_err_t handlePageEquations(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return redirectToLogin(req);
        
        ESP_LOGI(HTTP_TAG, "Serving Equations page");
        httpd_resp_set_type(req, "text/html");
        httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
        httpd_resp_send(req, Content::PAGE_EQUATIONS, HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    static esp_err_t handlePageSettings(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return redirectToLogin(req);
        
        ESP_LOGI(HTTP_TAG, "Serving Settings page");
        httpd_resp_set_type(req, "text/html");
        httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
        httpd_resp_send(req, Content::PAGE_SETTINGS, HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    /**
     * @brief Serve the Display Configuration page
     * Uses YAML-driven UI generation
     */
    static esp_err_t handlePageDisplayConfig(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return redirectToLogin(req);
        
        ESP_LOGI(HTTP_TAG, "Serving Display Config page");
        httpd_resp_set_type(req, "text/html");
        httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
        httpd_resp_send(req, Content::PAGE_DISPLAY_CONFIG, HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    /**
     * @brief API: Get scene configuration as JSON
     * GET /api/scene/config?id=<sceneId>
     * Returns the full scene YAML as JSON for the UI to render
     */
    static esp_err_t handleApiSceneConfig(httpd_req_t* req) {
        if (requiresAuthJson(req)) return sendJsonError(req, 401, "Authentication required");
        
        // Parse query parameters
        char query[64] = {0};
        if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK) {
            // No query - return active scene
            for (const auto& scene : savedScenes_) {
                if (scene.active) {
                    return sendSceneConfigJson(req, scene);
                }
            }
            // No active scene - return first or empty
            if (!savedScenes_.empty()) {
                return sendSceneConfigJson(req, savedScenes_[0]);
            }
            return sendJsonError(req, 404, "No scenes found");
        }
        
        // Get scene ID
        char param[16] = {0};
        if (httpd_query_key_value(query, "id", param, sizeof(param)) == ESP_OK) {
            int sceneId = atoi(param);
            for (const auto& scene : savedScenes_) {
                if (scene.id == sceneId) {
                    return sendSceneConfigJson(req, scene);
                }
            }
            return sendJsonError(req, 404, "Scene not found");
        }
        
        return sendJsonError(req, 400, "Missing scene id");
    }
    
    /**
     * @brief Helper: Send scene config as JSON
     */
    static esp_err_t sendSceneConfigJson(httpd_req_t* req, const SavedScene& scene) {
        ESP_LOGI(HTTP_TAG, "[SceneConfig] id=%d animType='%s' params.size=%d", scene.id, scene.animType.c_str(), scene.params.size());
        for (const auto& kv : scene.params) {
            if (kv.first.rfind("reactive_", 0) == 0) {
                ESP_LOGI(HTTP_TAG, "  [ConfigParam] '%s' = %.2f", kv.first.c_str(), kv.second);
            }
        }
        
        cJSON* root = cJSON_CreateObject();
        cJSON_AddBoolToObject(root, "success", true);
        
        // Create config object matching YAML structure
        cJSON* config = cJSON_CreateObject();
        
        // Global section
        cJSON* global = cJSON_CreateObject();
        cJSON_AddStringToObject(global, "name", scene.name.c_str());
        cJSON_AddNumberToObject(global, "id", scene.id);
        cJSON_AddStringToObject(global, "description", "Scene configuration");
        cJSON_AddNumberToObject(global, "version", 1.0);
        cJSON_AddStringToObject(global, "author", "ARCOS");
        cJSON_AddItemToObject(config, "Global", global);
        
        // Display section
        cJSON* display = cJSON_CreateObject();
        cJSON_AddBoolToObject(display, "enabled", scene.displayEnabled);
        cJSON_AddStringToObject(display, "animation_type", scene.animType.c_str());
        cJSON_AddNumberToObject(display, "main_sprite_id", scene.spriteId);
        
        // Position subobject
        cJSON* pos = cJSON_CreateObject();
        auto it = scene.params.find("center_x");
        cJSON_AddNumberToObject(pos, "x", it != scene.params.end() ? it->second : 64.0f);
        it = scene.params.find("center_y");
        cJSON_AddNumberToObject(pos, "y", it != scene.params.end() ? it->second : 16.0f);
        cJSON_AddItemToObject(display, "position", pos);
        
        // Other display params
        it = scene.params.find("rotation");
        cJSON_AddNumberToObject(display, "rotation", it != scene.params.end() ? it->second : 0.0f);
        it = scene.params.find("intensity");
        cJSON_AddNumberToObject(display, "sensitivity", it != scene.params.end() ? it->second : 1.5f);
        
        // Mirror param - check params map first, then mirrorSprite flag
        it = scene.params.find("mirror");
        bool mirrorValue = it != scene.params.end() ? (it->second > 0.5f) : scene.mirrorSprite;
        cJSON_AddBoolToObject(display, "mirror", mirrorValue);
        cJSON_AddBoolToObject(display, "mirrorSprite", mirrorValue);  // Legacy field
        
        // Background color
        cJSON* bg = cJSON_CreateObject();
        cJSON_AddNumberToObject(bg, "r", scene.bgR);
        cJSON_AddNumberToObject(bg, "g", scene.bgG);
        cJSON_AddNumberToObject(bg, "b", scene.bgB);
        cJSON_AddItemToObject(display, "background", bg);
        
        // Include ALL animation params from the scene
        // This covers static_sprite, static_mirrored, reactive_eyes, and any other animation type
        cJSON* params = cJSON_CreateObject();
        
        // Always include the mirror param
        cJSON_AddNumberToObject(params, "mirror", mirrorValue ? 1.0f : 0.0f);
        
        // Add ALL params from the scene (not just hardcoded ones)
        // This ensures reactive_eyes and other animation types work correctly
        for (const auto& kv : scene.params) {
            // Skip shader_ prefixed params (they go in the Shader section)
            if (kv.first.rfind("shader_", 0) == 0) continue;
            cJSON_AddNumberToObject(params, kv.first.c_str(), kv.second);
        }
        
        // Add default values for static params if not present (backwards compat)
        if (cJSON_GetObjectItem(params, "x") == NULL) {
            cJSON_AddNumberToObject(params, "x", 64.0f);
        }
        if (cJSON_GetObjectItem(params, "y") == NULL) {
            cJSON_AddNumberToObject(params, "y", 16.0f);
        }
        if (cJSON_GetObjectItem(params, "rotation") == NULL) {
            cJSON_AddNumberToObject(params, "rotation", 0.0f);
        }
        if (cJSON_GetObjectItem(params, "scale") == NULL) {
            cJSON_AddNumberToObject(params, "scale", 1.0f);
        }
        if (cJSON_GetObjectItem(params, "left_x") == NULL) {
            cJSON_AddNumberToObject(params, "left_x", 32.0f);
        }
        if (cJSON_GetObjectItem(params, "left_y") == NULL) {
            cJSON_AddNumberToObject(params, "left_y", 16.0f);
        }
        if (cJSON_GetObjectItem(params, "left_rotation") == NULL) {
            cJSON_AddNumberToObject(params, "left_rotation", 0.0f);
        }
        if (cJSON_GetObjectItem(params, "left_scale") == NULL) {
            cJSON_AddNumberToObject(params, "left_scale", 1.0f);
        }
        if (cJSON_GetObjectItem(params, "right_x") == NULL) {
            cJSON_AddNumberToObject(params, "right_x", 96.0f);
        }
        if (cJSON_GetObjectItem(params, "right_y") == NULL) {
            cJSON_AddNumberToObject(params, "right_y", 16.0f);
        }
        if (cJSON_GetObjectItem(params, "right_rotation") == NULL) {
            cJSON_AddNumberToObject(params, "right_rotation", 180.0f);
        }
        if (cJSON_GetObjectItem(params, "right_scale") == NULL) {
            cJSON_AddNumberToObject(params, "right_scale", 1.0f);
        }
        
        cJSON_AddItemToObject(display, "params", params);
        
        cJSON_AddItemToObject(config, "Display", display);
        
        // Shader section - read from scene.params with shader_ prefix
        cJSON* shader = cJSON_CreateObject();
        
        // Shader type (0=None, 1=ColorOverride, 2=HueCycle, 3=GradientCycle, 4=Glitch)
        it = scene.params.find("shader_type");
        cJSON_AddNumberToObject(shader, "type", it != scene.params.end() ? (int)it->second : 0);
        
        // Invert
        it = scene.params.find("shader_invert");
        cJSON_AddNumberToObject(shader, "invert", it != scene.params.end() ? (int)it->second : 0);
        
        // Mask settings
        it = scene.params.find("shader_mask_enabled");
        cJSON_AddNumberToObject(shader, "mask_enabled", it != scene.params.end() ? (int)it->second : 1);
        
        it = scene.params.find("shader_mask_r");
        cJSON_AddNumberToObject(shader, "mask_r", it != scene.params.end() ? (int)it->second : 0);
        it = scene.params.find("shader_mask_g");
        cJSON_AddNumberToObject(shader, "mask_g", it != scene.params.end() ? (int)it->second : 0);
        it = scene.params.find("shader_mask_b");
        cJSON_AddNumberToObject(shader, "mask_b", it != scene.params.end() ? (int)it->second : 0);
        
        // Override color
        it = scene.params.find("shader_override_r");
        cJSON_AddNumberToObject(shader, "override_r", it != scene.params.end() ? (int)it->second : 255);
        it = scene.params.find("shader_override_g");
        cJSON_AddNumberToObject(shader, "override_g", it != scene.params.end() ? (int)it->second : 255);
        it = scene.params.find("shader_override_b");
        cJSON_AddNumberToObject(shader, "override_b", it != scene.params.end() ? (int)it->second : 255);
        
        // Hue/Gradient cycle settings
        it = scene.params.find("shader_hue_speed");
        cJSON_AddNumberToObject(shader, "hue_speed", it != scene.params.end() ? (int)it->second : 1000);
        
        it = scene.params.find("shader_hue_color_count");
        int colorCount = it != scene.params.end() ? (int)it->second : 5;
        cJSON_AddNumberToObject(shader, "hue_color_count", colorCount);
        
        // Hue palette colors (up to 32)
        for (int i = 0; i < 32; i++) {
            char paramR[32], paramG[32], paramB[32];
            snprintf(paramR, sizeof(paramR), "shader_hue_color_%d_r", i);
            snprintf(paramG, sizeof(paramG), "shader_hue_color_%d_g", i);
            snprintf(paramB, sizeof(paramB), "shader_hue_color_%d_b", i);
            
            auto itR = scene.params.find(paramR);
            auto itG = scene.params.find(paramG);
            auto itB = scene.params.find(paramB);
            
            // Only add if at least one color component exists
            if (itR != scene.params.end() || itG != scene.params.end() || itB != scene.params.end()) {
                char fieldR[24], fieldG[24], fieldB[24];
                snprintf(fieldR, sizeof(fieldR), "hue_color_%d_r", i);
                snprintf(fieldG, sizeof(fieldG), "hue_color_%d_g", i);
                snprintf(fieldB, sizeof(fieldB), "hue_color_%d_b", i);
                cJSON_AddNumberToObject(shader, fieldR, itR != scene.params.end() ? (int)itR->second : 255);
                cJSON_AddNumberToObject(shader, fieldG, itG != scene.params.end() ? (int)itG->second : 0);
                cJSON_AddNumberToObject(shader, fieldB, itB != scene.params.end() ? (int)itB->second : 0);
            }
        }
        
        // Gradient settings
        it = scene.params.find("shader_gradient_distance");
        cJSON_AddNumberToObject(shader, "gradient_distance", it != scene.params.end() ? (int)it->second : 20);
        
        it = scene.params.find("shader_gradient_angle");
        cJSON_AddNumberToObject(shader, "gradient_angle", it != scene.params.end() ? (int)it->second : 0);
        
        it = scene.params.find("shader_gradient_mirror");
        cJSON_AddNumberToObject(shader, "gradient_mirror", it != scene.params.end() ? (int)it->second : 0);
        
        // Glitch settings
        it = scene.params.find("shader_glitch_speed");
        cJSON_AddNumberToObject(shader, "glitch_speed", it != scene.params.end() ? (int)it->second : 50);
        
        it = scene.params.find("shader_glitch_intensity");
        cJSON_AddNumberToObject(shader, "glitch_intensity", it != scene.params.end() ? (int)it->second : 30);
        
        it = scene.params.find("shader_glitch_chromatic");
        cJSON_AddNumberToObject(shader, "glitch_chromatic", it != scene.params.end() ? (int)it->second : 20);
        
        cJSON_AddItemToObject(config, "Shader", shader);
        
        // LEDS section
        cJSON* leds = cJSON_CreateObject();
        cJSON_AddBoolToObject(leds, "enabled", scene.ledsEnabled);
        cJSON_AddNumberToObject(leds, "brightness", 80);
        
        cJSON* color = cJSON_CreateObject();
        cJSON_AddNumberToObject(color, "r", 255);
        cJSON_AddNumberToObject(color, "g", 128);
        cJSON_AddNumberToObject(color, "b", 0);
        cJSON_AddItemToObject(leds, "color", color);
        
        // LED strips
        cJSON* strips = cJSON_CreateObject();
        const char* stripNames[] = {"left_fin", "right_fin", "tongue", "scales"};
        for (const char* name : stripNames) {
            cJSON* strip = cJSON_CreateObject();
            cJSON_AddBoolToObject(strip, "enabled", true);
            cJSON_AddBoolToObject(strip, "color_override", false);
            cJSON_AddItemToObject(strips, name, strip);
        }
        cJSON_AddItemToObject(leds, "strips", strips);
        cJSON_AddItemToObject(config, "LEDS", leds);
        
        cJSON_AddItemToObject(root, "config", config);
        
        // Send response
        char* json = cJSON_PrintUnformatted(root);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, json, strlen(json));
        
        cJSON_free(json);
        cJSON_Delete(root);
        return ESP_OK;
    }
    
    // ========== Animation System Handlers ==========
    
    /**
     * @brief Get list of all available animation sets
     * GET /api/animation/sets
     */
    static esp_err_t handleApiAnimationSets(httpd_req_t* req) {
        if (requiresAuthJson(req)) return sendJsonError(req, 401, "Authentication required");
        
        ESP_LOGI(HTTP_TAG, "API: Get animation sets");
        
        auto& registry = AnimationSystem::getParameterRegistry();
        std::string json = registry.exportAnimationSetsJson();
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, json.c_str(), json.length());
        return ESP_OK;
    }
    
    /**
     * @brief Get parameters for a specific animation set
     * GET /api/animation/params?set=<setId>
     */
    static esp_err_t handleApiAnimationParams(httpd_req_t* req) {
        if (requiresAuthJson(req)) return sendJsonError(req, 401, "Authentication required");
        
        // Parse query parameters
        char query[128] = {0};
        if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK) {
            return sendJsonError(req, 400, "Missing query parameters");
        }
        
        char setId[64] = {0};
        if (httpd_query_key_value(query, "set", setId, sizeof(setId)) != ESP_OK) {
            return sendJsonError(req, 400, "Missing 'set' parameter");
        }
        
        ESP_LOGI(HTTP_TAG, "API: Get parameters for set '%s'", setId);
        
        auto& registry = AnimationSystem::getParameterRegistry();
        std::string json = registry.exportParametersJson(setId);
        
        if (json.empty()) {
            return sendJsonError(req, 404, "Animation set not found");
        }
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, json.c_str(), json.length());
        return ESP_OK;
    }
    
    /**
     * @brief Update a specific parameter value
     * POST /api/animation/param
     * Body: {"set": "setId", "param": "paramId", "value": <value>}
     */
    static esp_err_t handleApiAnimationParam(httpd_req_t* req) {
        if (requiresAuthJson(req)) return sendJsonError(req, 401, "Authentication required");
        
        // Read request body
        char body[512] = {0};
        int received = httpd_req_recv(req, body, sizeof(body) - 1);
        if (received <= 0) {
            return sendJsonError(req, 400, "Empty request body");
        }
        body[received] = '\0';
        
        ESP_LOGI(HTTP_TAG, "API: Update parameter - %s", body);
        
        // Parse JSON manually (simple parsing)
        char setId[64] = {0};
        char paramId[64] = {0};
        float value = 0;
        
        // Extract "set" field
        const char* setStart = strstr(body, "\"set\"");
        if (setStart) {
            setStart = strchr(setStart, ':');
            if (setStart) {
                setStart = strchr(setStart, '"');
                if (setStart) {
                    setStart++;
                    const char* setEnd = strchr(setStart, '"');
                    if (setEnd) {
                        size_t len = setEnd - setStart;
                        if (len < sizeof(setId)) {
                            strncpy(setId, setStart, len);
                        }
                    }
                }
            }
        }
        
        // Extract "param" field
        const char* paramStart = strstr(body, "\"param\"");
        if (paramStart) {
            paramStart = strchr(paramStart, ':');
            if (paramStart) {
                paramStart = strchr(paramStart, '"');
                if (paramStart) {
                    paramStart++;
                    const char* paramEnd = strchr(paramStart, '"');
                    if (paramEnd) {
                        size_t len = paramEnd - paramStart;
                        if (len < sizeof(paramId)) {
                            strncpy(paramId, paramStart, len);
                        }
                    }
                }
            }
        }
        
        // Extract "value" field
        const char* valueStart = strstr(body, "\"value\"");
        if (valueStart) {
            valueStart = strchr(valueStart, ':');
            if (valueStart) {
                valueStart++;
                while (*valueStart == ' ') valueStart++;
                value = atof(valueStart);
            }
        }
        
        if (strlen(setId) == 0 || strlen(paramId) == 0) {
            return sendJsonError(req, 400, "Missing 'set' or 'param' field");
        }
        
        // Update the parameter
        auto& registry = AnimationSystem::getParameterRegistry();
        AnimationSystem::AnimationSet* animSet = registry.getAnimationSet(setId);
        if (!animSet) {
            return sendJsonError(req, 404, "Animation set not found");
        }
        
        if (!animSet->setParameterValue(paramId, value)) {
            return sendJsonError(req, 404, "Parameter not found");
        }
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"success\":true}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    /**
     * @brief Get available inputs for animation binding
     * GET /api/animation/inputs
     */
    static esp_err_t handleApiAnimationInputs(httpd_req_t* req) {
        if (requiresAuthJson(req)) return sendJsonError(req, 401, "Authentication required");
        
        ESP_LOGI(HTTP_TAG, "API: Get animation inputs");
        
        auto& ctx = AnimationSystem::getContext();
        std::string json = ctx.exportInputsJson();
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, json.c_str(), json.length());
        return ESP_OK;
    }
    
    /**
     * @brief Activate an animation set
     * POST /api/animation/activate
     * Body: {"set": "setId"}
     */
    static esp_err_t handleApiAnimationActivate(httpd_req_t* req) {
        if (requiresAuthJson(req)) return sendJsonError(req, 401, "Authentication required");
        
        // Read request body
        char body[256] = {0};
        int received = httpd_req_recv(req, body, sizeof(body) - 1);
        if (received <= 0) {
            return sendJsonError(req, 400, "Empty request body");
        }
        body[received] = '\0';
        
        // Extract set ID
        char setId[64] = {0};
        const char* setStart = strstr(body, "\"set\"");
        if (setStart) {
            setStart = strchr(setStart, ':');
            if (setStart) {
                setStart = strchr(setStart, '"');
                if (setStart) {
                    setStart++;
                    const char* setEnd = strchr(setStart, '"');
                    if (setEnd) {
                        size_t len = setEnd - setStart;
                        if (len < sizeof(setId)) {
                            strncpy(setId, setStart, len);
                        }
                    }
                }
            }
        }
        
        if (strlen(setId) == 0) {
            return sendJsonError(req, 400, "Missing 'set' field");
        }
        
        ESP_LOGI(HTTP_TAG, "API: Activate animation set '%s'", setId);
        
        auto& mode = AnimationSystem::getAnimationMode();
        if (!mode.activateAnimationSet(setId)) {
            return sendJsonError(req, 404, "Animation set not found");
        }
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"success\":true}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    /**
     * @brief Stop the active animation
     * POST /api/animation/stop
     */
    static esp_err_t handleApiAnimationStop(httpd_req_t* req) {
        if (requiresAuthJson(req)) return sendJsonError(req, 401, "Authentication required");
        
        ESP_LOGI(HTTP_TAG, "API: Stop animation");
        
        auto& mode = AnimationSystem::getAnimationMode();
        mode.stop();
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"success\":true}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    /**
     * @brief Reset animation parameters to defaults
     * POST /api/animation/reset
     * Body: {"set": "setId"} or {} for all
     */
    static esp_err_t handleApiAnimationReset(httpd_req_t* req) {
        if (requiresAuthJson(req)) return sendJsonError(req, 401, "Authentication required");
        
        // Read request body
        char body[256] = {0};
        int received = httpd_req_recv(req, body, sizeof(body) - 1);
        if (received > 0) {
            body[received] = '\0';
        }
        
        // Extract optional set ID
        char setId[64] = {0};
        const char* setStart = strstr(body, "\"set\"");
        if (setStart) {
            setStart = strchr(setStart, ':');
            if (setStart) {
                setStart = strchr(setStart, '"');
                if (setStart) {
                    setStart++;
                    const char* setEnd = strchr(setStart, '"');
                    if (setEnd) {
                        size_t len = setEnd - setStart;
                        if (len < sizeof(setId)) {
                            strncpy(setId, setStart, len);
                        }
                    }
                }
            }
        }
        
        ESP_LOGI(HTTP_TAG, "API: Reset animation parameters%s%s", 
                 strlen(setId) > 0 ? " for " : "", setId);
        
        auto& registry = AnimationSystem::getParameterRegistry();
        
        if (strlen(setId) > 0) {
            AnimationSystem::AnimationSet* animSet = registry.getAnimationSet(setId);
            if (!animSet) {
                return sendJsonError(req, 404, "Animation set not found");
            }
            animSet->resetToDefaults();
        } else {
            registry.resetAllToDefaults();
        }
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"success\":true}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    // ========================================================
    // REGISTRY API ENDPOINTS
    // Dynamic type discovery for UI
    // ========================================================
    
    /**
     * @brief Get registered shader types
     * GET /api/registry/shaders
     * Returns: {"shaders": [{"id": "...", "name": "...", "params": [...]}]}
     */
    static esp_err_t handleApiRegistryShaders(httpd_req_t* req) {
        ESP_LOGI(HTTP_TAG, "API: Get shader registry");
        
        auto& registry = AnimationSystem::ShaderRegistry::instance();
        std::string json = registry.exportJson();
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, json.c_str(), HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    /**
     * @brief Get registered transition types
     * GET /api/registry/transitions
     * Returns: {"transitions": [{"id": "...", "name": "...", "icon": "...", "params": [...]}]}
     */
    static esp_err_t handleApiRegistryTransitions(httpd_req_t* req) {
        ESP_LOGI(HTTP_TAG, "API: Get transition registry");
        
        auto& registry = AnimationSystem::TransitionRegistry::instance();
        std::string json = registry.exportJson();
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, json.c_str(), HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    /**
     * @brief Get registered animation types
     * GET /api/registry/animations
     * Returns: {"animations": [{"id": "...", "name": "...", "description": "...", "category": "..."}]}
     */
    static esp_err_t handleApiRegistryAnimations(httpd_req_t* req) {
        ESP_LOGI(HTTP_TAG, "API: Get animation registry");
        
        // getParameterRegistry() auto-initializes on first access
        auto& paramReg = AnimationSystem::getParameterRegistry();
        
        // Debug: List registered animation IDs
        auto ids = paramReg.getAnimationSetIds();
        ESP_LOGI(HTTP_TAG, "Registered animations count: %d", (int)ids.size());
        for (const auto& id : ids) {
            ESP_LOGI(HTTP_TAG, "  - %s", id.c_str());
        }
        
        // Build JSON from ParameterRegistry
        std::string json = "{\"animations\":[";
        auto infos = paramReg.getAnimationSetInfos();
        bool first = true;
        
        for (const auto& info : infos) {
            if (!first) json += ",";
            first = false;
            
            json += "{";
            json += "\"id\":\"" + info.id + "\",";
            json += "\"name\":\"" + info.name + "\",";
            json += "\"description\":\"" + info.description + "\",";
            json += "\"category\":\"" + info.category + "\"";
            json += "}";
        }
        json += "]}";
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, json.c_str(), HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    /**
     * @brief Save current scene configuration
     * POST /api/scene/save
     * Body: {"name": "sceneName"}
     */
    static esp_err_t handleApiSceneSave(httpd_req_t* req) {
        if (requiresAuthJson(req)) return sendJsonError(req, 401, "Authentication required");
        
        // Read request body
        char body[256] = {0};
        int received = httpd_req_recv(req, body, sizeof(body) - 1);
        if (received <= 0) {
            return sendJsonError(req, 400, "Empty request body");
        }
        body[received] = '\0';
        
        // Extract scene name
        char sceneName[64] = {0};
        const char* nameStart = strstr(body, "\"name\"");
        if (nameStart) {
            nameStart = strchr(nameStart, ':');
            if (nameStart) {
                nameStart = strchr(nameStart, '"');
                if (nameStart) {
                    nameStart++;
                    const char* nameEnd = strchr(nameStart, '"');
                    if (nameEnd) {
                        size_t len = nameEnd - nameStart;
                        if (len < sizeof(sceneName)) {
                            strncpy(sceneName, nameStart, len);
                        }
                    }
                }
            }
        }
        
        if (strlen(sceneName) == 0) {
            return sendJsonError(req, 400, "Missing 'name' field");
        }
        
        ESP_LOGI(HTTP_TAG, "API: Save scene '%s'", sceneName);
        
        // Export current scene configuration
        auto& mode = AnimationSystem::getAnimationMode();
        std::string sceneJson = mode.exportSceneJson();
        
        // Build file path
        char filePath[128];
        snprintf(filePath, sizeof(filePath), "%s/%s.json", SCENE_DIR, sceneName);
        
        // Write to file
        FILE* f = fopen(filePath, "w");
        if (!f) {
            return sendJsonError(req, 500, "Failed to create scene file");
        }
        fwrite(sceneJson.c_str(), 1, sceneJson.length(), f);
        fclose(f);
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"success\":true}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    /**
     * @brief Update a single scene parameter in real-time
     * POST /api/scene/param
     * Body: {"sceneId": 1, "animType": "gyro_eyes", "param": "gyro_sensitivity", "value": 1.5}
     */
    static esp_err_t handleApiSceneParam(httpd_req_t* req) {
        if (requiresAuthJson(req)) return sendJsonError(req, 401, "Authentication required");
        
        char body[512] = {0};
        int received = httpd_req_recv(req, body, sizeof(body) - 1);
        if (received <= 0) {
            return sendJsonError(req, 400, "Empty request body");
        }
        body[received] = '\0';
        
        cJSON* root = cJSON_Parse(body);
        if (!root) {
            return sendJsonError(req, 400, "Invalid JSON");
        }
        
        cJSON* sceneIdItem = cJSON_GetObjectItem(root, "sceneId");
        // cJSON* animTypeItem = cJSON_GetObjectItem(root, \"animType\"); // Reserved for animation type filtering
        cJSON* paramItem = cJSON_GetObjectItem(root, "param");
        cJSON* valueItem = cJSON_GetObjectItem(root, "value");
        
        if (!paramItem || !cJSON_IsString(paramItem) || !valueItem) {
            cJSON_Delete(root);
            return sendJsonError(req, 400, "Missing param or value");
        }
        
        const char* paramId = paramItem->valuestring;
        float value = cJSON_IsNumber(valueItem) ? (float)valueItem->valuedouble : 
                      cJSON_IsBool(valueItem) ? (cJSON_IsTrue(valueItem) ? 1.0f : 0.0f) : 0.0f;
        
        ESP_LOGI(HTTP_TAG, "API: Update scene param %s = %.2f", paramId, value);
        
        // Update the scene's params if sceneId provided
        if (sceneIdItem && cJSON_IsNumber(sceneIdItem)) {
            int sceneId = sceneIdItem->valueint;
            for (auto& scene : savedScenes_) {
                if (scene.id == sceneId) {
                    scene.params[paramId] = value;
                    // Scene found and updated
                    break;
                }
            }
        }
        
        // Also update the live animation system's active set parameter
        auto& mode = AnimationSystem::getAnimationMode();
        AnimationSystem::AnimationSet* activeSet = mode.getActiveAnimationSet();
        if (activeSet) {
            activeSet->setParameterValue(paramId, value);
        }
        
        // Use single param callback for live updates (doesn't reset other params)
        if (getSingleParamCallback()) {
            getSingleParamCallback()(paramId, value);
        }
        
        cJSON_Delete(root);
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"success\":true}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    /**
     * @brief Start scene preview
     * POST /api/scene/preview
     * Body: {"animType": "gyro_eyes", "transition": "glitch", "params": {...}}
     */
    static esp_err_t handleApiScenePreview(httpd_req_t* req) {
        if (requiresAuthJson(req)) return sendJsonError(req, 401, "Authentication required");
        
        char body[2048] = {0};
        int received = httpd_req_recv(req, body, sizeof(body) - 1);
        if (received <= 0) {
            return sendJsonError(req, 400, "Empty request body");
        }
        body[received] = '\0';
        
        cJSON* root = cJSON_Parse(body);
        if (!root) {
            return sendJsonError(req, 400, "Invalid JSON");
        }
        
        cJSON* animTypeItem = cJSON_GetObjectItem(root, "animType");
        cJSON* transitionItem = cJSON_GetObjectItem(root, "transition");
        cJSON* spriteIdItem = cJSON_GetObjectItem(root, "spriteId");
        cJSON* mirrorItem = cJSON_GetObjectItem(root, "mirrorSprite");
        cJSON* paramsItem = cJSON_GetObjectItem(root, "params");
        
        const char* animType = (animTypeItem && cJSON_IsString(animTypeItem)) ? animTypeItem->valuestring : "";
        const char* transition = (transitionItem && cJSON_IsString(transitionItem)) ? transitionItem->valuestring : "none";
        int spriteId = (spriteIdItem && cJSON_IsNumber(spriteIdItem)) ? spriteIdItem->valueint : -1;
        bool mirror = (mirrorItem && cJSON_IsBool(mirrorItem)) ? cJSON_IsTrue(mirrorItem) : false;
        
        ESP_LOGI(HTTP_TAG, "API: Preview scene animType=%s transition=%s sprite=%d mirror=%d", 
                 animType, transition, spriteId, mirror);
        
        // Build a temporary SavedScene to pass through the callback system
        // This ensures the preview uses the same rendering path as scene activation
        SavedScene previewScene;
        previewScene.id = -1;  // Negative ID indicates preview mode
        previewScene.name = "Preview";
        previewScene.active = true;
        previewScene.displayEnabled = true;
        previewScene.animType = animType;
        previewScene.transition = transition;
        previewScene.spriteId = spriteId;
        previewScene.mirrorSprite = mirror;
        
        // Copy parameters from JSON
        if (paramsItem && cJSON_IsObject(paramsItem)) {
            cJSON* param = NULL;
            cJSON_ArrayForEach(param, paramsItem) {
                if (cJSON_IsNumber(param) && param->string) {
                    previewScene.params[param->string] = (float)param->valuedouble;
                }
            }
        }
        
        cJSON_Delete(root);
        
        // Use the scene activation callback to trigger actual rendering
        // This connects to GpuDriverState::setSceneAnimation() in CurrentMode.cpp
        auto& callback = getSceneActivatedCallback();
        if (callback) {
            callback(previewScene);
            httpd_resp_set_type(req, "application/json");
            httpd_resp_send(req, "{\"success\":true}", HTTPD_RESP_USE_STRLEN);
        } else {
            ESP_LOGW(HTTP_TAG, "No scene callback registered, preview not available");
            httpd_resp_set_type(req, "application/json");
            httpd_resp_send(req, "{\"success\":false,\"error\":\"Scene callback not registered\"}", HTTPD_RESP_USE_STRLEN);
        }
        return ESP_OK;
    }
    
    /**
     * @brief Stop scene preview
     * POST /api/scene/stop
     */
    static esp_err_t handleApiSceneStop(httpd_req_t* req) {
        if (requiresAuthJson(req)) return sendJsonError(req, 401, "Authentication required");
        
        ESP_LOGI(HTTP_TAG, "API: Stop scene preview");
        
        // Create a "none" scene to stop the animation through the proper callback
        SavedScene stopScene;
        stopScene.id = -1;
        stopScene.name = "Stop";
        stopScene.active = true;
        stopScene.displayEnabled = true;
        stopScene.animType = "none";  // This triggers SceneAnimMode::NONE
        
        auto& callback = getSceneActivatedCallback();
        if (callback) {
            callback(stopScene);
        }
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"success\":true}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    // ========== Static Content Handlers ==========
    
    static esp_err_t handleCss(httpd_req_t* req) {
        httpd_resp_set_type(req, "text/css");
        httpd_resp_send(req, Content::STYLE_CSS, HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    // ========== Utility Functions ==========
    
    /**
     * @brief Decode base64 string to binary data
     * @param input Base64 encoded string
     * @param output Output buffer
     * @param maxOutputLen Maximum output buffer size
     * @param outputLen Actual decoded length
     * @return true on success
     */
    static bool decodeBase64(const char* input, uint8_t* output, size_t maxOutputLen, size_t* outputLen) {
        static const uint8_t b64table[256] = {
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 62, 64, 64, 64, 63,
            52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 64, 64, 64, 64, 64, 64,
            64,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
            15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 64, 64, 64, 64, 64,
            64, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
            41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 64, 64, 64, 64, 64,
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64
        };
        
        size_t inputLen = strlen(input);
        if (inputLen == 0) {
            *outputLen = 0;
            return true;
        }
        
        // Remove padding from length calculation
        size_t padding = 0;
        if (inputLen >= 1 && input[inputLen - 1] == '=') padding++;
        if (inputLen >= 2 && input[inputLen - 2] == '=') padding++;
        
        size_t expectedLen = (inputLen * 3) / 4 - padding;
        if (expectedLen > maxOutputLen) {
            return false;
        }
        
        size_t outIdx = 0;
        uint32_t buf = 0;
        int bits = 0;
        
        for (size_t i = 0; i < inputLen; i++) {
            uint8_t c = (uint8_t)input[i];
            if (c == '=') break;
            
            uint8_t v = b64table[c];
            if (v == 64) continue; // Skip invalid chars
            
            buf = (buf << 6) | v;
            bits += 6;
            
            if (bits >= 8) {
                bits -= 8;
                if (outIdx < maxOutputLen) {
                    output[outIdx++] = (buf >> bits) & 0xFF;
                }
            }
        }
        
        *outputLen = outIdx;
        return true;
    }
    
    // ========== API Handlers ==========
    
    /**
     * @brief Return 401 Unauthorized for API requests
     */
    static esp_err_t sendUnauthorized(httpd_req_t* req) {
        httpd_resp_set_status(req, "401 Unauthorized");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"error\":\"Unauthorized\",\"login_required\":true}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    static esp_err_t handleApiState(httpd_req_t* req) {
        // Check auth for external network requests
        if (requiresAuthRedirect(req)) return sendUnauthorized(req);
        
        auto& state = SYNC_STATE.state();
        
        cJSON* root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "ssid", state.ssid);
        cJSON_AddStringToObject(root, "ip", state.ipAddress);
        cJSON_AddNumberToObject(root, "clients", state.wifiClients);
        cJSON_AddNumberToObject(root, "uptime", state.uptime);
        cJSON_AddNumberToObject(root, "freeHeap", state.freeHeap);
        cJSON_AddNumberToObject(root, "brightness", state.brightness);
        cJSON_AddNumberToObject(root, "cpuUsage", state.cpuUsage);
        cJSON_AddNumberToObject(root, "fps", state.fps);
        
        // Sensor data
        cJSON* sensors = cJSON_CreateObject();
        cJSON_AddNumberToObject(sensors, "temperature", state.temperature);
        cJSON_AddNumberToObject(sensors, "humidity", state.humidity);
        cJSON_AddNumberToObject(sensors, "pressure", state.pressure);
        cJSON_AddItemToObject(root, "sensors", sensors);
        
        // IMU data (raw)
        cJSON* imu = cJSON_CreateObject();
        cJSON_AddNumberToObject(imu, "accelX", state.accelX);
        cJSON_AddNumberToObject(imu, "accelY", state.accelY);
        cJSON_AddNumberToObject(imu, "accelZ", state.accelZ);
        cJSON_AddNumberToObject(imu, "gyroX", state.gyroX);
        cJSON_AddNumberToObject(imu, "gyroY", state.gyroY);
        cJSON_AddNumberToObject(imu, "gyroZ", state.gyroZ);
        cJSON_AddItemToObject(root, "imu", imu);

        // Device IMU data (calibrated)
        cJSON* deviceImu = cJSON_CreateObject();
        cJSON_AddNumberToObject(deviceImu, "accelX", state.deviceAccelX);
        cJSON_AddNumberToObject(deviceImu, "accelY", state.deviceAccelY);
        cJSON_AddNumberToObject(deviceImu, "accelZ", state.deviceAccelZ);
        cJSON_AddNumberToObject(deviceImu, "gyroX", state.deviceGyroX);
        cJSON_AddNumberToObject(deviceImu, "gyroY", state.deviceGyroY);
        cJSON_AddNumberToObject(deviceImu, "gyroZ", state.deviceGyroZ);
        cJSON_AddBoolToObject(deviceImu, "calibrated", state.imuCalibrated);
        cJSON_AddItemToObject(root, "deviceImu", deviceImu);

        // GPS data
        cJSON* gps = cJSON_CreateObject();
        cJSON_AddNumberToObject(gps, "latitude", state.latitude);
        cJSON_AddNumberToObject(gps, "longitude", state.longitude);
        cJSON_AddNumberToObject(gps, "altitude", state.altitude);
        cJSON_AddNumberToObject(gps, "satellites", state.satellites);
        cJSON_AddBoolToObject(gps, "valid", state.gpsValid);
        cJSON_AddNumberToObject(gps, "speed", state.gpsSpeed);
        cJSON_AddNumberToObject(gps, "heading", state.gpsHeading);
        cJSON_AddNumberToObject(gps, "hdop", state.gpsHdop);
        
        // GPS Time as formatted string
        char timeStr[20];
        snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d", 
                 state.gpsHour, state.gpsMinute, state.gpsSecond);
        cJSON_AddStringToObject(gps, "time", timeStr);
        
        char dateStr[16];
        snprintf(dateStr, sizeof(dateStr), "%04d-%02d-%02d", 
                 state.gpsYear, state.gpsMonth, state.gpsDay);
        cJSON_AddStringToObject(gps, "date", dateStr);
        cJSON_AddItemToObject(root, "gps", gps);
        
        // Connection status
        cJSON_AddBoolToObject(root, "gpuConnected", state.gpuConnected);
        
        // GPU stats
        cJSON* gpuStats = cJSON_CreateObject();
        cJSON_AddNumberToObject(gpuStats, "fps", state.gpuFps);
        cJSON_AddNumberToObject(gpuStats, "freeHeap", state.gpuFreeHeap);
        cJSON_AddNumberToObject(gpuStats, "minHeap", state.gpuMinHeap);
        cJSON_AddNumberToObject(gpuStats, "load", state.gpuLoad);
        cJSON_AddNumberToObject(gpuStats, "totalFrames", state.gpuTotalFrames);
        cJSON_AddNumberToObject(gpuStats, "uptime", state.gpuUptime);
        cJSON_AddBoolToObject(gpuStats, "hub75Ok", state.gpuHub75Ok);
        cJSON_AddBoolToObject(gpuStats, "oledOk", state.gpuOledOk);
        cJSON_AddItemToObject(root, "gpu", gpuStats);
        
        // Microphone
        cJSON_AddNumberToObject(root, "mic", state.micLevel);
        cJSON_AddBoolToObject(root, "micConnected", state.micConnected);
        cJSON_AddNumberToObject(root, "micDb", state.micDb);
        
        // System mode
        const char* modeStr = "idle";
        switch (state.mode) {
            case SystemMode::RUNNING: modeStr = "running"; break;
            case SystemMode::PAUSED: modeStr = "paused"; break;
            case SystemMode::ERROR: modeStr = "error"; break;
            default: modeStr = "idle"; break;
        }
        cJSON_AddStringToObject(root, "mode", modeStr);
        cJSON_AddStringToObject(root, "statusText", state.statusText);
        
        // External WiFi state
        cJSON_AddBoolToObject(root, "extWifiEnabled", state.extWifiEnabled);
        cJSON_AddBoolToObject(root, "extWifiConnected", state.extWifiConnected);
        cJSON_AddBoolToObject(root, "extWifiIsConnected", state.extWifiIsConnected);
        cJSON_AddStringToObject(root, "extWifiSSID", state.extWifiSSID);
        cJSON_AddStringToObject(root, "extWifiIP", state.extWifiIP);
        cJSON_AddNumberToObject(root, "extWifiRSSI", state.extWifiRSSI);
        
        // Authentication state (don't send password!)
        cJSON_AddBoolToObject(root, "authEnabled", state.authEnabled);
        cJSON_AddStringToObject(root, "authUsername", state.authUsername);

        // Fan state
        cJSON_AddBoolToObject(root, "fanEnabled", state.fanEnabled);
        cJSON_AddNumberToObject(root, "fanSpeed", state.fanSpeed);

        char* json = cJSON_PrintUnformatted(root);
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, json, strlen(json));
        
        free(json);
        cJSON_Delete(root);
        
        return ESP_OK;
    }
    
    static esp_err_t handleApiCommand(httpd_req_t* req) {
        // Check auth for external network requests
        if (requiresAuthRedirect(req)) return sendUnauthorized(req);
        
        HttpServer* self = static_cast<HttpServer*>(req->user_ctx);
        
        // Read body
        char buf[HTTP_BUFFER_SIZE];
        int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
        if (ret <= 0) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No body");
            return ESP_FAIL;
        }
        buf[ret] = '\0';
        
        cJSON* root = cJSON_Parse(buf);
        if (!root) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
            return ESP_FAIL;
        }
        
        cJSON* cmd = cJSON_GetObjectItem(root, "cmd");
        if (cmd && cmd->valuestring) {
            CommandType type = stringToCommand(cmd->valuestring);
            self->processCommand(type, root);
        }
        
        cJSON_Delete(root);
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"success\":true}", HTTPD_RESP_USE_STRLEN);
        
        return ESP_OK;
    }
    
    /**
     * @brief Handle WiFi network scanning
     */
    static esp_err_t handleApiScan(httpd_req_t* req) {
        // Check auth for external network requests
        if (requiresAuthRedirect(req)) return sendUnauthorized(req);
        
        ESP_LOGI(HTTP_TAG, "Starting WiFi scan...");
        
        // Get current WiFi mode
        wifi_mode_t mode;
        esp_wifi_get_mode(&mode);
        
        // Ensure STA interface exists for scanning
        bool wasAPOnly = (mode == WIFI_MODE_AP);
        if (wasAPOnly) {
            ESP_LOGI(HTTP_TAG, "Switching to APSTA mode for scan");
            
            // Create STA netif if not exists
            esp_netif_t* sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
            if (!sta_netif) {
                sta_netif = esp_netif_create_default_wifi_sta();
                ESP_LOGI(HTTP_TAG, "Created STA netif for scanning");
            }
            
            esp_wifi_set_mode(WIFI_MODE_APSTA);
            vTaskDelay(pdMS_TO_TICKS(200)); // Longer delay for mode switch
        }
        
        // Configure scan - use passive scan which works better in APSTA mode
        wifi_scan_config_t scan_config = {};
        scan_config.ssid = nullptr;
        scan_config.bssid = nullptr;
        scan_config.channel = 0;
        scan_config.show_hidden = false;
        scan_config.scan_type = WIFI_SCAN_TYPE_PASSIVE;
        scan_config.scan_time.passive = 200;
        
        // Start scan (blocking)
        esp_err_t err = esp_wifi_scan_start(&scan_config, true);
        if (err != ESP_OK) {
            ESP_LOGE(HTTP_TAG, "WiFi scan failed: %s", esp_err_to_name(err));
            // Restore AP-only mode if we switched
            if (wasAPOnly) {
                esp_wifi_set_mode(WIFI_MODE_AP);
            }
            httpd_resp_set_type(req, "application/json");
            char errJson[100];
            snprintf(errJson, sizeof(errJson), "{\"networks\":[], \"error\":\"Scan failed: %s\"}", esp_err_to_name(err));
            httpd_resp_send(req, errJson, HTTPD_RESP_USE_STRLEN);
            return ESP_OK;
        }
        
        // Get scan results
        uint16_t ap_count = 0;
        esp_wifi_scan_get_ap_num(&ap_count);
        
        if (ap_count == 0) {
            httpd_resp_set_type(req, "application/json");
            httpd_resp_send(req, "{\"networks\":[]}", HTTPD_RESP_USE_STRLEN);
            return ESP_OK;
        }
        
        // Limit to reasonable number
        if (ap_count > 20) ap_count = 20;
        
        wifi_ap_record_t* ap_records = (wifi_ap_record_t*)malloc(sizeof(wifi_ap_record_t) * ap_count);
        if (!ap_records) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Memory allocation failed");
            return ESP_FAIL;
        }
        
        esp_wifi_scan_get_ap_records(&ap_count, ap_records);
        
        // Build JSON response
        cJSON* root = cJSON_CreateObject();
        cJSON* networks = cJSON_CreateArray();
        
        for (int i = 0; i < ap_count; i++) {
            // Skip networks with empty SSID
            if (strlen((char*)ap_records[i].ssid) == 0) continue;
            
            cJSON* net = cJSON_CreateObject();
            cJSON_AddStringToObject(net, "ssid", (char*)ap_records[i].ssid);
            cJSON_AddNumberToObject(net, "rssi", ap_records[i].rssi);
            cJSON_AddNumberToObject(net, "channel", ap_records[i].primary);
            cJSON_AddBoolToObject(net, "secure", ap_records[i].authmode != WIFI_AUTH_OPEN);
            
            // Auth mode string
            const char* authStr = "Unknown";
            switch (ap_records[i].authmode) {
                case WIFI_AUTH_OPEN: authStr = "Open"; break;
                case WIFI_AUTH_WEP: authStr = "WEP"; break;
                case WIFI_AUTH_WPA_PSK: authStr = "WPA"; break;
                case WIFI_AUTH_WPA2_PSK: authStr = "WPA2"; break;
                case WIFI_AUTH_WPA_WPA2_PSK: authStr = "WPA/WPA2"; break;
                case WIFI_AUTH_WPA3_PSK: authStr = "WPA3"; break;
                case WIFI_AUTH_WPA2_WPA3_PSK: authStr = "WPA2/WPA3"; break;
                default: authStr = "Enterprise"; break;
            }
            cJSON_AddStringToObject(net, "auth", authStr);
            
            cJSON_AddItemToArray(networks, net);
        }
        
        cJSON_AddItemToObject(root, "networks", networks);
        
        free(ap_records);
        
        char* json = cJSON_PrintUnformatted(root);
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, json, strlen(json));
        
        free(json);
        cJSON_Delete(root);
        
        ESP_LOGI(HTTP_TAG, "WiFi scan complete, found %d networks", ap_count);
        return ESP_OK;
    }
    
    // ========== Configuration API Handlers ==========
    
    /**
     * @brief Get all animation configurations
     */
    static esp_err_t handleApiConfigs(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return sendUnauthorized(req);
        
        HttpServer* self = static_cast<HttpServer*>(req->user_ctx);
        auto& mgr = self->animConfigManager_;
        
        cJSON* root = cJSON_CreateObject();
        cJSON* configs = cJSON_CreateArray();
        
        for (int i = 0; i < mgr.getConfigCount(); i++) {
            const Animation::AnimationConfiguration* cfg = mgr.getConfig(i);
            if (!cfg) continue;
            
            cJSON* item = cJSON_CreateObject();
            cJSON_AddStringToObject(item, "name", cfg->name);
            cJSON_AddNumberToObject(item, "index", i);
            cJSON_AddNumberToObject(item, "target", static_cast<int>(cfg->target));
            
            // Display config
            cJSON* display = cJSON_CreateObject();
            cJSON_AddNumberToObject(display, "animation", static_cast<int>(cfg->display.animation));
            cJSON_AddNumberToObject(display, "speed", cfg->display.speed);
            cJSON_AddNumberToObject(display, "brightness", cfg->display.brightness);
            cJSON* color1d = cJSON_CreateObject();
            cJSON_AddNumberToObject(color1d, "r", cfg->display.color1_r);
            cJSON_AddNumberToObject(color1d, "g", cfg->display.color1_g);
            cJSON_AddNumberToObject(color1d, "b", cfg->display.color1_b);
            cJSON_AddItemToObject(display, "color1", color1d);
            cJSON* color2d = cJSON_CreateObject();
            cJSON_AddNumberToObject(color2d, "r", cfg->display.color2_r);
            cJSON_AddNumberToObject(color2d, "g", cfg->display.color2_g);
            cJSON_AddNumberToObject(color2d, "b", cfg->display.color2_b);
            cJSON_AddItemToObject(display, "color2", color2d);
            cJSON_AddItemToObject(item, "display", display);
            
            // LED config
            cJSON* leds = cJSON_CreateObject();
            cJSON_AddNumberToObject(leds, "animation", static_cast<int>(cfg->leds.animation));
            cJSON_AddNumberToObject(leds, "speed", cfg->leds.speed);
            cJSON_AddNumberToObject(leds, "brightness", cfg->leds.brightness);
            cJSON* color1l = cJSON_CreateObject();
            cJSON_AddNumberToObject(color1l, "r", cfg->leds.color1_r);
            cJSON_AddNumberToObject(color1l, "g", cfg->leds.color1_g);
            cJSON_AddNumberToObject(color1l, "b", cfg->leds.color1_b);
            cJSON_AddItemToObject(leds, "color1", color1l);
            cJSON* color2l = cJSON_CreateObject();
            cJSON_AddNumberToObject(color2l, "r", cfg->leds.color2_r);
            cJSON_AddNumberToObject(color2l, "g", cfg->leds.color2_g);
            cJSON_AddNumberToObject(color2l, "b", cfg->leds.color2_b);
            cJSON_AddItemToObject(leds, "color2", color2l);
            cJSON_AddItemToObject(item, "leds", leds);
            
            cJSON_AddItemToArray(configs, item);
        }
        
        cJSON_AddItemToObject(root, "configs", configs);
        cJSON_AddNumberToObject(root, "activeDisplay", mgr.getActiveDisplayConfig());
        cJSON_AddNumberToObject(root, "activeLeds", mgr.getActiveLedConfig());
        
        char* json = cJSON_PrintUnformatted(root);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, json, strlen(json));
        
        free(json);
        cJSON_Delete(root);
        return ESP_OK;
    }
    
    /**
     * @brief Apply a configuration
     */
    static esp_err_t handleApiConfigApply(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return sendUnauthorized(req);
        
        HttpServer* self = static_cast<HttpServer*>(req->user_ctx);
        
        char buf[256];
        int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
        if (ret <= 0) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No body");
            return ESP_FAIL;
        }
        buf[ret] = '\0';
        
        cJSON* root = cJSON_Parse(buf);
        if (!root) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
            return ESP_FAIL;
        }
        
        cJSON* index = cJSON_GetObjectItem(root, "index");
        if (!index || !cJSON_IsNumber(index)) {
            cJSON_Delete(root);
            httpd_resp_set_type(req, "application/json");
            httpd_resp_send(req, "{\"success\":false,\"error\":\"Missing index\"}", HTTPD_RESP_USE_STRLEN);
            return ESP_OK;
        }
        
        int applied = self->animConfigManager_.applyConfig(index->valueint);
        cJSON_Delete(root);
        
        char response[64];
        snprintf(response, sizeof(response), "{\"success\":true,\"applied\":%d}", applied);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
        
        ESP_LOGI(HTTP_TAG, "Applied config %d, result: %d", index->valueint, applied);
        return ESP_OK;
    }
    
    /**
     * @brief Save configuration changes
     */
    static esp_err_t handleApiConfigSave(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return sendUnauthorized(req);
        
        HttpServer* self = static_cast<HttpServer*>(req->user_ctx);
        
        char buf[1024];
        int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
        if (ret <= 0) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No body");
            return ESP_FAIL;
        }
        buf[ret] = '\0';
        
        cJSON* root = cJSON_Parse(buf);
        if (!root) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
            return ESP_FAIL;
        }
        
        cJSON* index = cJSON_GetObjectItem(root, "index");
        if (!index || !cJSON_IsNumber(index)) {
            cJSON_Delete(root);
            httpd_resp_set_type(req, "application/json");
            httpd_resp_send(req, "{\"success\":false,\"error\":\"Missing index\"}", HTTPD_RESP_USE_STRLEN);
            return ESP_OK;
        }
        
        Animation::AnimationConfiguration* cfg = self->animConfigManager_.getConfig(index->valueint);
        if (!cfg) {
            cJSON_Delete(root);
            httpd_resp_set_type(req, "application/json");
            httpd_resp_send(req, "{\"success\":false,\"error\":\"Config not found\"}", HTTPD_RESP_USE_STRLEN);
            return ESP_OK;
        }
        
        // Update name
        cJSON* name = cJSON_GetObjectItem(root, "name");
        if (name && name->valuestring) {
            cfg->setName(name->valuestring);
        }
        
        // Update target
        cJSON* target = cJSON_GetObjectItem(root, "target");
        if (target && cJSON_IsNumber(target)) {
            cfg->target = static_cast<Animation::ConfigTarget>(target->valueint);
        }
        
        // Update display config
        cJSON* display = cJSON_GetObjectItem(root, "display");
        if (display) {
            cJSON* anim = cJSON_GetObjectItem(display, "animation");
            if (anim) cfg->display.animation = static_cast<Animation::DisplayAnimation>(anim->valueint);
            
            cJSON* speed = cJSON_GetObjectItem(display, "speed");
            if (speed) cfg->display.speed = speed->valueint;
            
            cJSON* brightness = cJSON_GetObjectItem(display, "brightness");
            if (brightness) cfg->display.brightness = brightness->valueint;
            
            cJSON* color1 = cJSON_GetObjectItem(display, "color1");
            if (color1) {
                cJSON* r = cJSON_GetObjectItem(color1, "r");
                cJSON* g = cJSON_GetObjectItem(color1, "g");
                cJSON* b = cJSON_GetObjectItem(color1, "b");
                if (r) cfg->display.color1_r = r->valueint;
                if (g) cfg->display.color1_g = g->valueint;
                if (b) cfg->display.color1_b = b->valueint;
            }
            
            cJSON* color2 = cJSON_GetObjectItem(display, "color2");
            if (color2) {
                cJSON* r = cJSON_GetObjectItem(color2, "r");
                cJSON* g = cJSON_GetObjectItem(color2, "g");
                cJSON* b = cJSON_GetObjectItem(color2, "b");
                if (r) cfg->display.color2_r = r->valueint;
                if (g) cfg->display.color2_g = g->valueint;
                if (b) cfg->display.color2_b = b->valueint;
            }
        }
        
        // Update LED config
        cJSON* leds = cJSON_GetObjectItem(root, "leds");
        if (leds) {
            cJSON* anim = cJSON_GetObjectItem(leds, "animation");
            if (anim) cfg->leds.animation = static_cast<Animation::LedAnimation>(anim->valueint);
            
            cJSON* speed = cJSON_GetObjectItem(leds, "speed");
            if (speed) cfg->leds.speed = speed->valueint;
            
            cJSON* brightness = cJSON_GetObjectItem(leds, "brightness");
            if (brightness) cfg->leds.brightness = brightness->valueint;
            
            cJSON* color1 = cJSON_GetObjectItem(leds, "color1");
            if (color1) {
                cJSON* r = cJSON_GetObjectItem(color1, "r");
                cJSON* g = cJSON_GetObjectItem(color1, "g");
                cJSON* b = cJSON_GetObjectItem(color1, "b");
                if (r) cfg->leds.color1_r = r->valueint;
                if (g) cfg->leds.color1_g = g->valueint;
                if (b) cfg->leds.color1_b = b->valueint;
            }
            
            cJSON* color2 = cJSON_GetObjectItem(leds, "color2");
            if (color2) {
                cJSON* r = cJSON_GetObjectItem(color2, "r");
                cJSON* g = cJSON_GetObjectItem(color2, "g");
                cJSON* b = cJSON_GetObjectItem(color2, "b");
                if (r) cfg->leds.color2_r = r->valueint;
                if (g) cfg->leds.color2_g = g->valueint;
                if (b) cfg->leds.color2_b = b->valueint;
            }
        }
        
        // Check if we should also apply
        cJSON* apply = cJSON_GetObjectItem(root, "apply");
        int applied = 0;
        if (apply && cJSON_IsTrue(apply)) {
            applied = self->animConfigManager_.applyConfig(index->valueint);
        }
        
        cJSON_Delete(root);
        
        char response[64];
        snprintf(response, sizeof(response), "{\"success\":true,\"applied\":%d}", applied);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
        
        ESP_LOGI(HTTP_TAG, "Saved config %d", index->valueint);
        return ESP_OK;
    }
    
    /**
     * @brief Create a new configuration
     */
    static esp_err_t handleApiConfigCreate(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return sendUnauthorized(req);
        
        HttpServer* self = static_cast<HttpServer*>(req->user_ctx);
        
        char buf[256];
        int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
        if (ret <= 0) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No body");
            return ESP_FAIL;
        }
        buf[ret] = '\0';
        
        cJSON* root = cJSON_Parse(buf);
        if (!root) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
            return ESP_FAIL;
        }
        
        cJSON* name = cJSON_GetObjectItem(root, "name");
        const char* configName = (name && name->valuestring) ? name->valuestring : "New Configuration";
        
        int newIndex = self->animConfigManager_.createConfig(configName, Animation::ConfigTarget::BOTH);
        cJSON_Delete(root);
        
        if (newIndex < 0) {
            httpd_resp_set_type(req, "application/json");
            httpd_resp_send(req, "{\"success\":false,\"error\":\"Max configs reached\"}", HTTPD_RESP_USE_STRLEN);
            return ESP_OK;
        }
        
        char response[64];
        snprintf(response, sizeof(response), "{\"success\":true,\"index\":%d}", newIndex);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
        
        ESP_LOGI(HTTP_TAG, "Created config '%s' at index %d", configName, newIndex);
        return ESP_OK;
    }
    
    /**
     * @brief Rename a configuration
     */
    static esp_err_t handleApiConfigRename(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return sendUnauthorized(req);
        
        HttpServer* self = static_cast<HttpServer*>(req->user_ctx);
        
        char buf[256];
        int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
        if (ret <= 0) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No body");
            return ESP_FAIL;
        }
        buf[ret] = '\0';
        
        cJSON* root = cJSON_Parse(buf);
        if (!root) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
            return ESP_FAIL;
        }
        
        cJSON* index = cJSON_GetObjectItem(root, "index");
        cJSON* name = cJSON_GetObjectItem(root, "name");
        
        bool success = false;
        if (index && cJSON_IsNumber(index) && name && name->valuestring) {
            success = self->animConfigManager_.renameConfig(index->valueint, name->valuestring);
        }
        
        cJSON_Delete(root);
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, success ? "{\"success\":true}" : "{\"success\":false}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    /**
     * @brief Duplicate a configuration
     */
    static esp_err_t handleApiConfigDuplicate(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return sendUnauthorized(req);
        
        HttpServer* self = static_cast<HttpServer*>(req->user_ctx);
        
        char buf[256];
        int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
        if (ret <= 0) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No body");
            return ESP_FAIL;
        }
        buf[ret] = '\0';
        
        cJSON* root = cJSON_Parse(buf);
        if (!root) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
            return ESP_FAIL;
        }
        
        cJSON* index = cJSON_GetObjectItem(root, "index");
        int newIndex = -1;
        
        if (index && cJSON_IsNumber(index)) {
            newIndex = self->animConfigManager_.duplicateConfig(index->valueint);
        }
        
        cJSON_Delete(root);
        
        if (newIndex < 0) {
            httpd_resp_set_type(req, "application/json");
            httpd_resp_send(req, "{\"success\":false,\"error\":\"Failed to duplicate\"}", HTTPD_RESP_USE_STRLEN);
            return ESP_OK;
        }
        
        char response[64];
        snprintf(response, sizeof(response), "{\"success\":true,\"index\":%d}", newIndex);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    /**
     * @brief Delete a configuration
     */
    static esp_err_t handleApiConfigDelete(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return sendUnauthorized(req);
        
        HttpServer* self = static_cast<HttpServer*>(req->user_ctx);
        
        char buf[256];
        int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
        if (ret <= 0) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No body");
            return ESP_FAIL;
        }
        buf[ret] = '\0';
        
        cJSON* root = cJSON_Parse(buf);
        if (!root) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
            return ESP_FAIL;
        }
        
        cJSON* index = cJSON_GetObjectItem(root, "index");
        bool success = false;
        
        if (index && cJSON_IsNumber(index)) {
            success = self->animConfigManager_.deleteConfig(index->valueint);
        }
        
        cJSON_Delete(root);
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, success ? "{\"success\":true}" : "{\"success\":false}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    // ========== Scene API Handlers ==========
    
    /**
     * @brief Get list of scenes
     */
    static esp_err_t handleApiScenes(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return sendUnauthorized(req);
        
        // Ensure at least one scene exists (fallback if storage failed to load)
        if (savedScenes_.empty()) {
            ESP_LOGI(HTTP_TAG, "handleApiScenes: No scenes found, creating fallback");
            createFallbackDefaultScene();
        }
        
        ESP_LOGI(HTTP_TAG, "handleApiScenes: Returning %d scenes, activeId=%d", 
                 (int)savedScenes_.size(), activeSceneId_);
        
        cJSON* root = cJSON_CreateObject();
        cJSON* scenes = cJSON_CreateArray();
        
        for (const auto& scene : savedScenes_) {
            cJSON* item = cJSON_CreateObject();
            cJSON_AddNumberToObject(item, "id", scene.id);
            cJSON_AddStringToObject(item, "name", scene.name.c_str());
            cJSON_AddNumberToObject(item, "type", scene.type);
            cJSON_AddBoolToObject(item, "active", scene.active);
            
            // Modular scene fields
            cJSON_AddBoolToObject(item, "displayEnabled", scene.displayEnabled);
            cJSON_AddBoolToObject(item, "ledsEnabled", scene.ledsEnabled);
            cJSON_AddBoolToObject(item, "effectsOnly", scene.effectsOnly);
            cJSON_AddNumberToObject(item, "order", scene.order);
            
            // Animation type (for list display)
            cJSON_AddStringToObject(item, "animationType", scene.animType.c_str());
            cJSON_AddStringToObject(item, "transition", scene.transition.c_str());
            
            // Add gyro eye config if type is GYRO_EYES
            if (scene.type == 1 && scene.hasGyroEyeConfig) {
                cJSON* gyroEye = cJSON_CreateObject();
                cJSON_AddNumberToObject(gyroEye, "spriteId", scene.gyroEye.spriteId);
                cJSON_AddNumberToObject(gyroEye, "intensity", scene.gyroEye.intensity);
                cJSON_AddNumberToObject(gyroEye, "maxOffsetX", scene.gyroEye.maxOffsetX);
                cJSON_AddNumberToObject(gyroEye, "maxOffsetY", scene.gyroEye.maxOffsetY);
                cJSON_AddNumberToObject(gyroEye, "smoothingFactor", scene.gyroEye.smoothingFactor);
                cJSON_AddNumberToObject(gyroEye, "eyeOffset", scene.gyroEye.eyeOffset);
                cJSON_AddNumberToObject(gyroEye, "leftEyeCenterX", scene.gyroEye.leftEyeCenterX);
                cJSON_AddNumberToObject(gyroEye, "leftEyeCenterY", scene.gyroEye.leftEyeCenterY);
                cJSON_AddNumberToObject(gyroEye, "rightEyeCenterX", scene.gyroEye.rightEyeCenterX);
                cJSON_AddNumberToObject(gyroEye, "rightEyeCenterY", scene.gyroEye.rightEyeCenterY);
                cJSON_AddBoolToObject(gyroEye, "invertPitch", scene.gyroEye.invertPitch);
                cJSON_AddBoolToObject(gyroEye, "invertRoll", scene.gyroEye.invertRoll);
                cJSON_AddNumberToObject(gyroEye, "bgR", scene.gyroEye.bgR);
                cJSON_AddNumberToObject(gyroEye, "bgG", scene.gyroEye.bgG);
                cJSON_AddNumberToObject(gyroEye, "bgB", scene.gyroEye.bgB);
                cJSON_AddItemToObject(item, "gyroEye", gyroEye);
            }
            
            // Add static sprite config if type is STATIC_SPRITE
            if (scene.type == 2 && scene.hasStaticSpriteConfig) {
                cJSON* staticSprite = cJSON_CreateObject();
                cJSON_AddNumberToObject(staticSprite, "spriteId", scene.staticSprite.spriteId);
                cJSON_AddNumberToObject(staticSprite, "posX", scene.staticSprite.posX);
                cJSON_AddNumberToObject(staticSprite, "posY", scene.staticSprite.posY);
                cJSON_AddNumberToObject(staticSprite, "bgR", scene.staticSprite.bgR);
                cJSON_AddNumberToObject(staticSprite, "bgG", scene.staticSprite.bgG);
                cJSON_AddNumberToObject(staticSprite, "bgB", scene.staticSprite.bgB);
                cJSON_AddItemToObject(item, "staticSprite", staticSprite);
            }
            
            cJSON_AddItemToArray(scenes, item);
        }
        
        cJSON_AddItemToObject(root, "scenes", scenes);
        cJSON_AddNumberToObject(root, "activeId", activeSceneId_);
        
        char* json = cJSON_PrintUnformatted(root);
        cJSON_Delete(root);
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
        free(json);
        return ESP_OK;
    }
    
    /**
     * @brief Create a new scene
     */
    static esp_err_t handleApiSceneCreate(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return sendUnauthorized(req);
        
        char buf[512];
        int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
        if (ret <= 0) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No body");
            return ESP_FAIL;
        }
        buf[ret] = '\0';
        
        cJSON* root = cJSON_Parse(buf);
        if (!root) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
            return ESP_FAIL;
        }
        
        cJSON* name = cJSON_GetObjectItem(root, "name");
        cJSON* type = cJSON_GetObjectItem(root, "type");
        
        bool success = false;
        int newId = -1;
        
        if (name && cJSON_IsString(name)) {
            SavedScene scene;
            scene.id = nextSceneId_++;
            scene.name = name->valuestring;
            scene.type = (type && cJSON_IsNumber(type)) ? type->valueint : 0;
            scene.active = false;
            scene.hasGyroEyeConfig = false;
            scene.hasStaticSpriteConfig = false;
            
            // Set modular scene defaults
            scene.displayEnabled = true;
            scene.ledsEnabled = false;
            scene.effectsOnly = false;
            scene.order = (int)savedScenes_.size();
            scene.animType = "static_sprite";
            scene.transition = "none";
            scene.shaderAA = true;
            scene.shaderInvert = false;
            scene.shaderColorMode = "none";
            scene.shaderColor = "#ffffff";
            scene.ledR = 255;
            scene.ledG = 0;
            scene.ledB = 255;
            scene.ledBrightness = 80;
            
            // Initialize default configs based on type (legacy)
            if (scene.type == 1) {
                scene.hasGyroEyeConfig = true;
                scene.gyroEye = GyroEyeSceneConfig();  // Default values
            } else if (scene.type == 2) {
                scene.hasStaticSpriteConfig = true;
                scene.staticSprite = StaticSpriteSceneConfig();
            }
            
            savedScenes_.push_back(scene);
            newId = scene.id;
            success = true;
            saveScenesStorage();
            
            ESP_LOGI(HTTP_TAG, "Created scene: %s (type %d, id %d)", scene.name.c_str(), scene.type, scene.id);
        }
        
        cJSON_Delete(root);
        
        cJSON* resp = cJSON_CreateObject();
        cJSON_AddBoolToObject(resp, "success", success);
        if (success) {
            cJSON_AddNumberToObject(resp, "id", newId);
        }
        
        char* json = cJSON_PrintUnformatted(resp);
        cJSON_Delete(resp);
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
        free(json);
        return ESP_OK;
    }
    
    /**
     * @brief Delete a scene
     */
    static esp_err_t handleApiSceneDelete(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return sendUnauthorized(req);
        
        char buf[256];
        int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
        if (ret <= 0) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No body");
            return ESP_FAIL;
        }
        buf[ret] = '\0';
        
        cJSON* root = cJSON_Parse(buf);
        if (!root) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
            return ESP_FAIL;
        }
        
        cJSON* id = cJSON_GetObjectItem(root, "id");
        bool success = false;
        
        if (id && cJSON_IsNumber(id)) {
            for (auto it = savedScenes_.begin(); it != savedScenes_.end(); ++it) {
                if (it->id == id->valueint) {
                    ESP_LOGI(HTTP_TAG, "Deleting scene: %s (id %d)", it->name.c_str(), it->id);
                    savedScenes_.erase(it);
                    success = true;
                    saveScenesStorage();
                    break;
                }
            }
        }
        
        cJSON_Delete(root);
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, success ? "{\"success\":true}" : "{\"success\":false}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    /**
     * @brief Rename a scene
     */
    static esp_err_t handleApiSceneRename(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return sendUnauthorized(req);
        
        char buf[512];
        int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
        if (ret <= 0) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No body");
            return ESP_FAIL;
        }
        buf[ret] = '\0';
        
        cJSON* root = cJSON_Parse(buf);
        if (!root) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
            return ESP_FAIL;
        }
        
        cJSON* id = cJSON_GetObjectItem(root, "id");
        cJSON* name = cJSON_GetObjectItem(root, "name");
        bool success = false;
        
        if (id && cJSON_IsNumber(id) && name && cJSON_IsString(name)) {
            for (auto& scene : savedScenes_) {
                if (scene.id == id->valueint) {
                    ESP_LOGI(HTTP_TAG, "Renaming scene %d: %s -> %s", scene.id, scene.name.c_str(), name->valuestring);
                    scene.name = name->valuestring;
                    success = true;
                    saveScenesStorage();
                    break;
                }
            }
        }
        
        cJSON_Delete(root);
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, success ? "{\"success\":true}" : "{\"success\":false}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    /**
     * @brief Reorder scenes (for drag-drop in scene list)
     * Expects: { "order": [id1, id2, id3, ...] }
     */
    static esp_err_t handleApiScenesReorder(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return sendUnauthorized(req);
        
        char buf[1024];
        int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
        if (ret <= 0) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No body");
            return ESP_FAIL;
        }
        buf[ret] = '\0';
        
        cJSON* root = cJSON_Parse(buf);
        if (!root) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
            return ESP_FAIL;
        }
        
        cJSON* order = cJSON_GetObjectItem(root, "order");
        bool success = false;
        
        if (order && cJSON_IsArray(order)) {
            int newOrder = 0;
            cJSON* item;
            cJSON_ArrayForEach(item, order) {
                if (cJSON_IsNumber(item)) {
                    int sceneId = item->valueint;
                    for (auto& scene : savedScenes_) {
                        if (scene.id == sceneId) {
                            scene.order = newOrder++;
                            break;
                        }
                    }
                }
            }
            
            // Sort savedScenes_ by order field
            std::sort(savedScenes_.begin(), savedScenes_.end(), 
                [](const SavedScene& a, const SavedScene& b) {
                    return a.order < b.order;
                });
            
            success = true;
            saveScenesStorage();
            ESP_LOGI(HTTP_TAG, "Reordered %d scenes", (int)savedScenes_.size());
        }
        
        cJSON_Delete(root);
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, success ? "{\"success\":true}" : "{\"success\":false}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    // ========== LED Preset API Handlers ==========
    
    /**
     * @brief Get all LED presets
     */
    static esp_err_t handleApiLedPresets(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return sendUnauthorized(req);
        
        ESP_LOGI(HTTP_TAG, "handleApiLedPresets: Returning %d presets, activeId=%d",
                 (int)savedLedPresets_.size(), activeLedPresetId_);
        
        cJSON* root = cJSON_CreateObject();
        cJSON* presets = cJSON_CreateArray();
        
        for (const auto& preset : savedLedPresets_) {
            cJSON* item = cJSON_CreateObject();
            cJSON_AddNumberToObject(item, "id", preset.id);
            cJSON_AddStringToObject(item, "name", preset.name.c_str());
            cJSON_AddStringToObject(item, "animation", preset.animation.c_str());
            cJSON_AddNumberToObject(item, "r", preset.r);
            cJSON_AddNumberToObject(item, "g", preset.g);
            cJSON_AddNumberToObject(item, "b", preset.b);
            cJSON_AddNumberToObject(item, "brightness", preset.brightness);
            cJSON_AddNumberToObject(item, "speed", preset.speed);
            cJSON_AddNumberToObject(item, "order", preset.order);
            cJSON_AddBoolToObject(item, "active", preset.id == activeLedPresetId_);
            
            // Add params
            if (!preset.params.empty()) {
                cJSON* params = cJSON_CreateObject();
                for (const auto& kv : preset.params) {
                    cJSON_AddNumberToObject(params, kv.first.c_str(), kv.second);
                }
                cJSON_AddItemToObject(item, "params", params);
            }
            
            cJSON_AddItemToArray(presets, item);
        }
        
        cJSON_AddItemToObject(root, "presets", presets);
        cJSON_AddNumberToObject(root, "activeId", activeLedPresetId_);
        
        char* json = cJSON_PrintUnformatted(root);
        cJSON_Delete(root);
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
        free(json);
        return ESP_OK;
    }
    
    /**
     * @brief Create a new LED preset
     */
    static esp_err_t handleApiLedPresetCreate(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return sendUnauthorized(req);
        
        char buf[512];
        int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
        if (ret <= 0) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No body");
            return ESP_FAIL;
        }
        buf[ret] = '\0';
        
        cJSON* root = cJSON_Parse(buf);
        if (!root) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
            return ESP_FAIL;
        }
        
        cJSON* name = cJSON_GetObjectItem(root, "name");
        
        SavedLedPreset preset;
        preset.id = nextLedPresetId_++;
        preset.name = (name && cJSON_IsString(name)) ? name->valuestring : "New LED Preset";
        preset.animation = "solid";
        preset.r = 255; preset.g = 0; preset.b = 255;
        preset.brightness = 100;
        preset.speed = 50;
        preset.order = (int)savedLedPresets_.size();
        
        savedLedPresets_.push_back(preset);
        saveLedPresetsStorage();
        
        ESP_LOGI(HTTP_TAG, "Created LED preset: id=%d name=%s", preset.id, preset.name.c_str());
        
        cJSON* resp = cJSON_CreateObject();
        cJSON_AddBoolToObject(resp, "success", true);
        cJSON_AddNumberToObject(resp, "id", preset.id);
        
        char* json = cJSON_PrintUnformatted(resp);
        cJSON_Delete(resp);
        cJSON_Delete(root);
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
        free(json);
        return ESP_OK;
    }
    
    /**
     * @brief Get a single LED preset by ID
     */
    static esp_err_t handleApiLedPresetGet(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return sendUnauthorized(req);
        
        char query[64] = {0};
        if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing id parameter");
            return ESP_FAIL;
        }
        
        char id_str[16] = {0};
        if (httpd_query_key_value(query, "id", id_str, sizeof(id_str)) != ESP_OK) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing id parameter");
            return ESP_FAIL;
        }
        
        int presetId = atoi(id_str);
        SavedLedPreset* found = nullptr;
        
        for (auto& preset : savedLedPresets_) {
            if (preset.id == presetId) {
                found = &preset;
                break;
            }
        }
        
        if (!found) {
            httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "LED preset not found");
            return ESP_FAIL;
        }
        
        cJSON* root = cJSON_CreateObject();
        cJSON_AddBoolToObject(root, "success", true);
        
        cJSON* presetObj = cJSON_CreateObject();
        cJSON_AddNumberToObject(presetObj, "id", found->id);
        cJSON_AddStringToObject(presetObj, "name", found->name.c_str());
        cJSON_AddStringToObject(presetObj, "animation", found->animation.c_str());
        cJSON_AddNumberToObject(presetObj, "r", found->r);
        cJSON_AddNumberToObject(presetObj, "g", found->g);
        cJSON_AddNumberToObject(presetObj, "b", found->b);
        cJSON_AddNumberToObject(presetObj, "brightness", found->brightness);
        cJSON_AddNumberToObject(presetObj, "speed", found->speed);
        cJSON_AddNumberToObject(presetObj, "colorCount", found->colorCount);
        cJSON_AddNumberToObject(presetObj, "order", found->order);
        cJSON_AddBoolToObject(presetObj, "active", found->id == activeLedPresetId_);
        
        // Add colors array
        cJSON* colorsArr = cJSON_CreateArray();
        if (found->colors.size() > 0) {
            for (const auto& color : found->colors) {
                cJSON* colorObj = cJSON_CreateObject();
                cJSON_AddNumberToObject(colorObj, "r", std::get<0>(color));
                cJSON_AddNumberToObject(colorObj, "g", std::get<1>(color));
                cJSON_AddNumberToObject(colorObj, "b", std::get<2>(color));
                cJSON_AddItemToArray(colorsArr, colorObj);
            }
        } else {
            // Fall back to single color
            cJSON* colorObj = cJSON_CreateObject();
            cJSON_AddNumberToObject(colorObj, "r", found->r);
            cJSON_AddNumberToObject(colorObj, "g", found->g);
            cJSON_AddNumberToObject(colorObj, "b", found->b);
            cJSON_AddItemToArray(colorsArr, colorObj);
        }
        cJSON_AddItemToObject(presetObj, "colors", colorsArr);
        
        if (!found->params.empty()) {
            cJSON* params = cJSON_CreateObject();
            for (const auto& kv : found->params) {
                cJSON_AddNumberToObject(params, kv.first.c_str(), kv.second);
            }
            cJSON_AddItemToObject(presetObj, "params", params);
        }
        
        cJSON_AddItemToObject(root, "preset", presetObj);
        
        char* json = cJSON_PrintUnformatted(root);
        cJSON_Delete(root);
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
        free(json);
        return ESP_OK;
    }
    
    /**
     * @brief Update an existing LED preset
     */
    static esp_err_t handleApiLedPresetUpdate(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return sendUnauthorized(req);
        
        char buf[1024];
        int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
        if (ret <= 0) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No body");
            return ESP_FAIL;
        }
        buf[ret] = '\0';
        
        cJSON* root = cJSON_Parse(buf);
        if (!root) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
            return ESP_FAIL;
        }
        
        cJSON* idField = cJSON_GetObjectItem(root, "id");
        if (!idField || !cJSON_IsNumber(idField)) {
            cJSON_Delete(root);
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing id");
            return ESP_FAIL;
        }
        
        int presetId = idField->valueint;
        SavedLedPreset* found = nullptr;
        
        for (auto& preset : savedLedPresets_) {
            if (preset.id == presetId) {
                found = &preset;
                break;
            }
        }
        
        if (!found) {
            cJSON_Delete(root);
            httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "LED preset not found");
            return ESP_FAIL;
        }
        
        // Update fields
        cJSON* val;
        if ((val = cJSON_GetObjectItem(root, "name")) && cJSON_IsString(val)) {
            found->name = val->valuestring;
        }
        if ((val = cJSON_GetObjectItem(root, "animation")) && cJSON_IsString(val)) {
            found->animation = val->valuestring;
        }
        if ((val = cJSON_GetObjectItem(root, "r")) && cJSON_IsNumber(val)) {
            found->r = (uint8_t)val->valueint;
        }
        if ((val = cJSON_GetObjectItem(root, "g")) && cJSON_IsNumber(val)) {
            found->g = (uint8_t)val->valueint;
        }
        if ((val = cJSON_GetObjectItem(root, "b")) && cJSON_IsNumber(val)) {
            found->b = (uint8_t)val->valueint;
        }
        if ((val = cJSON_GetObjectItem(root, "brightness")) && cJSON_IsNumber(val)) {
            found->brightness = (uint8_t)val->valueint;
        }
        if ((val = cJSON_GetObjectItem(root, "speed")) && cJSON_IsNumber(val)) {
            found->speed = (int8_t)val->valueint;
        }
        if ((val = cJSON_GetObjectItem(root, "colorCount")) && cJSON_IsNumber(val)) {
            found->colorCount = val->valueint;
        }
        
        // Update colors array
        cJSON* colors = cJSON_GetObjectItem(root, "colors");
        if (colors && cJSON_IsArray(colors)) {
            found->colors.clear();
            cJSON* color = NULL;
            cJSON_ArrayForEach(color, colors) {
                if (cJSON_IsObject(color)) {
                    cJSON* cr = cJSON_GetObjectItem(color, "r");
                    cJSON* cg = cJSON_GetObjectItem(color, "g");
                    cJSON* cb = cJSON_GetObjectItem(color, "b");
                    uint8_t r = (cr && cJSON_IsNumber(cr)) ? (uint8_t)cr->valueint : 255;
                    uint8_t g = (cg && cJSON_IsNumber(cg)) ? (uint8_t)cg->valueint : 255;
                    uint8_t b = (cb && cJSON_IsNumber(cb)) ? (uint8_t)cb->valueint : 255;
                    found->colors.push_back(std::make_tuple(r, g, b));
                }
            }
            found->colorCount = (int)found->colors.size();
            // Update primary color from first color in array
            if (!found->colors.empty()) {
                found->r = std::get<0>(found->colors[0]);
                found->g = std::get<1>(found->colors[0]);
                found->b = std::get<2>(found->colors[0]);
            }
        }
        
        // Update params
        cJSON* params = cJSON_GetObjectItem(root, "params");
        if (params && cJSON_IsObject(params)) {
            found->params.clear();
            cJSON* param = NULL;
            cJSON_ArrayForEach(param, params) {
                if (cJSON_IsNumber(param) && param->string) {
                    found->params[param->string] = param->valueint;
                }
            }
        }
        
        saveLedPresetsStorage();
        ESP_LOGI(HTTP_TAG, "Updated LED preset: id=%d name=%s", found->id, found->name.c_str());
        
        cJSON_Delete(root);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"success\":true}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    /**
     * @brief Delete an LED preset
     */
    static esp_err_t handleApiLedPresetDelete(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return sendUnauthorized(req);
        
        char buf[128];
        int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
        if (ret <= 0) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No body");
            return ESP_FAIL;
        }
        buf[ret] = '\0';
        
        cJSON* root = cJSON_Parse(buf);
        if (!root) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
            return ESP_FAIL;
        }
        
        cJSON* idField = cJSON_GetObjectItem(root, "id");
        if (!idField || !cJSON_IsNumber(idField)) {
            cJSON_Delete(root);
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing id");
            return ESP_FAIL;
        }
        
        int presetId = idField->valueint;
        cJSON_Delete(root);
        
        auto it = std::remove_if(savedLedPresets_.begin(), savedLedPresets_.end(),
            [presetId](const SavedLedPreset& p) { return p.id == presetId; });
        
        if (it == savedLedPresets_.end()) {
            httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "LED preset not found");
            return ESP_FAIL;
        }
        
        savedLedPresets_.erase(it, savedLedPresets_.end());
        
        // If we deleted the active preset, clear the active ID
        if (activeLedPresetId_ == presetId) {
            activeLedPresetId_ = -1;
        }
        
        saveLedPresetsStorage();
        ESP_LOGI(HTTP_TAG, "Deleted LED preset: id=%d", presetId);
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"success\":true}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    /**
     * @brief Activate an LED preset
     */
    static esp_err_t handleApiLedPresetActivate(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return sendUnauthorized(req);
        
        char buf[128];
        int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
        if (ret <= 0) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No body");
            return ESP_FAIL;
        }
        buf[ret] = '\0';
        
        cJSON* root = cJSON_Parse(buf);
        if (!root) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
            return ESP_FAIL;
        }
        
        cJSON* idField = cJSON_GetObjectItem(root, "id");
        if (!idField || !cJSON_IsNumber(idField)) {
            cJSON_Delete(root);
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing id");
            return ESP_FAIL;
        }
        
        int presetId = idField->valueint;
        cJSON_Delete(root);
        
        SavedLedPreset* found = nullptr;
        for (auto& preset : savedLedPresets_) {
            if (preset.id == presetId) {
                found = &preset;
                break;
            }
        }
        
        if (!found) {
            httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "LED preset not found");
            return ESP_FAIL;
        }
        
        activeLedPresetId_ = presetId;
        saveLedPresetsStorage();
        
        ESP_LOGI(HTTP_TAG, "Activated LED preset: id=%d name=%s anim=%s", 
                 found->id, found->name.c_str(), found->animation.c_str());
        
        // Invoke the LED preset callback if registered
        auto& callback = getLedPresetActivatedCallback();
        if (callback) {
            callback(*found);
        }
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"success\":true}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    /**
     * @brief Preview an LED preset without saving
     */
    static esp_err_t handleApiLedPresetPreview(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return sendUnauthorized(req);
        
        char buf[1024];
        int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
        if (ret <= 0) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No body");
            return ESP_FAIL;
        }
        buf[ret] = '\0';
        
        cJSON* root = cJSON_Parse(buf);
        if (!root) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
            return ESP_FAIL;
        }
        
        // Build a temporary preset from the request
        SavedLedPreset preview;
        preview.id = -1;  // Temporary
        preview.name = "Preview";
        
        cJSON* val;
        if ((val = cJSON_GetObjectItem(root, "animation")) && cJSON_IsString(val)) {
            preview.animation = val->valuestring;
        } else {
            preview.animation = "solid";
        }
        if ((val = cJSON_GetObjectItem(root, "brightness")) && cJSON_IsNumber(val)) {
            preview.brightness = (uint8_t)val->valueint;
        }
        if ((val = cJSON_GetObjectItem(root, "speed")) && cJSON_IsNumber(val)) {
            preview.speed = (int8_t)val->valueint;
        }
        
        // Parse colorCount and colors array
        if ((val = cJSON_GetObjectItem(root, "colorCount")) && cJSON_IsNumber(val)) {
            preview.colorCount = val->valueint;
        }
        cJSON* colors = cJSON_GetObjectItem(root, "colors");
        if (colors && cJSON_IsArray(colors)) {
            cJSON* color = NULL;
            cJSON_ArrayForEach(color, colors) {
                int cr = 255, cg = 255, cb = 255;
                cJSON* cv;
                if ((cv = cJSON_GetObjectItem(color, "r")) && cJSON_IsNumber(cv)) cr = cv->valueint;
                if ((cv = cJSON_GetObjectItem(color, "g")) && cJSON_IsNumber(cv)) cg = cv->valueint;
                if ((cv = cJSON_GetObjectItem(color, "b")) && cJSON_IsNumber(cv)) cb = cv->valueint;
                preview.colors.push_back(std::make_tuple((uint8_t)cr, (uint8_t)cg, (uint8_t)cb));
            }
        }
        // Fallback: if no colors array, use r,g,b
        if (preview.colors.empty()) {
            int r = 255, g = 255, b = 255;
            if ((val = cJSON_GetObjectItem(root, "r")) && cJSON_IsNumber(val)) r = val->valueint;
            if ((val = cJSON_GetObjectItem(root, "g")) && cJSON_IsNumber(val)) g = val->valueint;
            if ((val = cJSON_GetObjectItem(root, "b")) && cJSON_IsNumber(val)) b = val->valueint;
            preview.colors.push_back(std::make_tuple((uint8_t)r, (uint8_t)g, (uint8_t)b));
            preview.colorCount = 1;
        }
        // Set primary r,g,b from first color
        if (!preview.colors.empty()) {
            preview.r = std::get<0>(preview.colors[0]);
            preview.g = std::get<1>(preview.colors[0]);
            preview.b = std::get<2>(preview.colors[0]);
        }
        
        cJSON_Delete(root);
        
        ESP_LOGI(HTTP_TAG, "Previewing LED preset: anim=%s r=%d g=%d b=%d", 
                 preview.animation.c_str(), preview.r, preview.g, preview.b);
        
        // Invoke the LED preset callback for preview
        auto& callback = getLedPresetActivatedCallback();
        if (callback) {
            callback(preview);
        }
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"success\":true}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    /**
     * @brief Reorder LED presets
     */
    static esp_err_t handleApiLedPresetsReorder(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return sendUnauthorized(req);
        
        char buf[1024];
        int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
        if (ret <= 0) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No body");
            return ESP_FAIL;
        }
        buf[ret] = '\0';
        
        cJSON* root = cJSON_Parse(buf);
        if (!root) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
            return ESP_FAIL;
        }
        
        cJSON* order = cJSON_GetObjectItem(root, "order");
        bool success = false;
        
        if (order && cJSON_IsArray(order)) {
            int newOrder = 0;
            cJSON* item;
            cJSON_ArrayForEach(item, order) {
                if (cJSON_IsNumber(item)) {
                    int presetId = item->valueint;
                    for (auto& preset : savedLedPresets_) {
                        if (preset.id == presetId) {
                            preset.order = newOrder++;
                            break;
                        }
                    }
                }
            }
            
            // Sort by order
            std::sort(savedLedPresets_.begin(), savedLedPresets_.end(),
                [](const SavedLedPreset& a, const SavedLedPreset& b) {
                    return a.order < b.order;
                });
            
            success = true;
            saveLedPresetsStorage();
            ESP_LOGI(HTTP_TAG, "Reordered %d LED presets", (int)savedLedPresets_.size());
        }
        
        cJSON_Delete(root);
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, success ? "{\"success\":true}" : "{\"success\":false}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    /**
     * @brief Get a single scene by ID
     */
    static esp_err_t handleApiSceneGet(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return sendUnauthorized(req);
        
        // Parse query string for id parameter
        char query[64] = {0};
        if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing id parameter");
            return ESP_FAIL;
        }
        
        char id_str[16] = {0};
        if (httpd_query_key_value(query, "id", id_str, sizeof(id_str)) != ESP_OK) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing id parameter");
            return ESP_FAIL;
        }
        
        int sceneId = atoi(id_str);
        SavedScene* found = nullptr;
        
        for (auto& scene : savedScenes_) {
            if (scene.id == sceneId) {
                found = &scene;
                break;
            }
        }
        
        if (!found) {
            httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Scene not found");
            return ESP_FAIL;
        }
        
        ESP_LOGI(HTTP_TAG, "[SceneGet] id=%d name='%s' animType='%s' params.size=%d", found->id, found->name.c_str(), found->animType.c_str(), found->params.size());
        for (const auto& kv : found->params) {
            ESP_LOGI(HTTP_TAG, "  [GetParam] '%s' = %.2f", kv.first.c_str(), kv.second);
        }
        
        cJSON* root = cJSON_CreateObject();
        cJSON* sceneObj = cJSON_CreateObject();
        cJSON_AddNumberToObject(sceneObj, "id", found->id);
        cJSON_AddStringToObject(sceneObj, "name", found->name.c_str());
        cJSON_AddNumberToObject(sceneObj, "type", found->type);
        cJSON_AddBoolToObject(sceneObj, "active", found->active);
        
        // Modular scene fields
        cJSON_AddBoolToObject(sceneObj, "displayEnabled", found->displayEnabled);
        cJSON_AddBoolToObject(sceneObj, "ledsEnabled", found->ledsEnabled);
        cJSON_AddBoolToObject(sceneObj, "effectsOnly", found->effectsOnly);
        cJSON_AddNumberToObject(sceneObj, "order", found->order);
        
        // Animation fields
        cJSON_AddStringToObject(sceneObj, "animationType", found->animType.c_str());
        cJSON_AddStringToObject(sceneObj, "transition", found->transition.c_str());
        
        // Shader settings
        cJSON_AddBoolToObject(sceneObj, "shaderAA", found->shaderAA);
        cJSON_AddBoolToObject(sceneObj, "shaderInvert", found->shaderInvert);
        cJSON_AddStringToObject(sceneObj, "shaderColorMode", found->shaderColorMode.c_str());
        cJSON_AddStringToObject(sceneObj, "shaderColor", found->shaderColor.c_str());
        
        // LED settings
        cJSON* ledColorObj = cJSON_CreateObject();
        cJSON_AddNumberToObject(ledColorObj, "r", found->ledR);
        cJSON_AddNumberToObject(ledColorObj, "g", found->ledG);
        cJSON_AddNumberToObject(ledColorObj, "b", found->ledB);
        cJSON_AddItemToObject(sceneObj, "ledColor", ledColorObj);
        cJSON_AddNumberToObject(sceneObj, "ledBrightness", found->ledBrightness);
        
        // Legacy AnimationSystem fields
        cJSON_AddStringToObject(sceneObj, "animType", found->animType.c_str());
        cJSON_AddNumberToObject(sceneObj, "spriteId", found->spriteId);
        cJSON_AddBoolToObject(sceneObj, "mirrorSprite", found->mirrorSprite);
        
        // Animation parameters as object (for new page, also as animParams)
        cJSON* paramsObj = cJSON_CreateObject();
        for (const auto& kv : found->params) {
            cJSON_AddNumberToObject(paramsObj, kv.first.c_str(), kv.second);
        }
        cJSON_AddItemToObject(sceneObj, "params", paramsObj);
        cJSON_AddItemToObject(sceneObj, "animParams", cJSON_Duplicate(paramsObj, 1));
        
        // Effects as object
        cJSON* effectsObj = cJSON_CreateObject();
        for (const auto& kv : found->effects) {
            cJSON* effectObj = cJSON_CreateObject();
            cJSON_AddBoolToObject(effectObj, "enabled", kv.second.enabled);
            cJSON_AddNumberToObject(effectObj, "intensity", kv.second.intensity);
            cJSON_AddItemToObject(effectsObj, kv.first.c_str(), effectObj);
        }
        cJSON_AddItemToObject(sceneObj, "effects", effectsObj);
        
        // Legacy: animation set ID if applicable (using type as proxy for backwards compat)
        if (found->type > 0) {
            const char* animSets[] = {"", "gyro_eye", "static_sprite", "rotating_sprite"};
            if (found->type < 4) {
                cJSON_AddStringToObject(sceneObj, "animSet", animSets[found->type]);
            }
        }
        
        cJSON_AddItemToObject(root, "scene", sceneObj);
        
        char* json = cJSON_PrintUnformatted(root);
        cJSON_Delete(root);
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
        free(json);
        return ESP_OK;
    }
    
    /**
     * @brief Activate a scene
     */
    static esp_err_t handleApiSceneActivate(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return sendUnauthorized(req);
        
        (void)req->user_ctx;  // HttpServer* available via user_ctx if needed
        
        char buf[256];
        int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
        if (ret <= 0) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No body");
            return ESP_FAIL;
        }
        buf[ret] = '\0';
        
        cJSON* root = cJSON_Parse(buf);
        if (!root) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
            return ESP_FAIL;
        }
        
        cJSON* id = cJSON_GetObjectItem(root, "id");
        bool success = false;
        
        if (id && cJSON_IsNumber(id)) {
            // Deactivate all scenes first
            for (auto& scene : savedScenes_) {
                scene.active = false;
            }
            
            // Activate the selected scene
            for (auto& scene : savedScenes_) {
                if (scene.id == id->valueint) {
                    scene.active = true;
                    activeSceneId_ = scene.id;
                    success = true;
                    
                    // Notify scene renderer if callback is set
                    auto& callback = getSceneActivatedCallback();
                    if (callback) {
                        callback(scene);
                    }
                    
                    ESP_LOGI(HTTP_TAG, "Activated scene: %s (id %d)", scene.name.c_str(), scene.id);
                    break;
                }
            }
            
            if (success) {
                saveScenesStorage();
            }
        }
        
        cJSON_Delete(root);
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, success ? "{\"success\":true}" : "{\"success\":false}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    /**
     * @brief Update scene configuration
     * Accepts both legacy config format and new AnimationSystem format
     */
    static esp_err_t handleApiSceneUpdate(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return sendUnauthorized(req);
        
        char buf[2048];
        int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
        if (ret <= 0) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No body");
            return ESP_FAIL;
        }
        buf[ret] = '\0';
        
        // DEBUG: Log incoming request
        printf("[handleApiSceneUpdate] Received: %s\n", buf);
        
        cJSON* root = cJSON_Parse(buf);
        if (!root) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
            return ESP_FAIL;
        }
        
        cJSON* id = cJSON_GetObjectItem(root, "id");
        bool success = false;
        
        if (id && cJSON_IsNumber(id)) {
            for (auto& scene : savedScenes_) {
                if (scene.id == id->valueint) {
                    // Track if animType changed - forces full callback
                    bool animTypeChanged = false;
                    
                    // New AnimationSystem fields (flat format from new Scene page)
                    cJSON* item;
                    if ((item = cJSON_GetObjectItem(root, "animType")) && cJSON_IsString(item)) {
                        if (scene.animType != item->valuestring) {
                            printf("[SceneUpdate] animType CHANGED: '%s' -> '%s'\n", scene.animType.c_str(), item->valuestring);
                            scene.animType = item->valuestring;
                            animTypeChanged = true;
                        } else {
                            printf("[SceneUpdate] animType unchanged: '%s'\n", scene.animType.c_str());
                        }
                    }
                    if ((item = cJSON_GetObjectItem(root, "transition")) && cJSON_IsString(item)) {
                        scene.transition = item->valuestring;
                    }
                    if ((item = cJSON_GetObjectItem(root, "spriteId"))) {
                        if (cJSON_IsNumber(item)) {
                            scene.spriteId = item->valueint;
                        } else if (cJSON_IsNull(item)) {
                            scene.spriteId = -1;
                        }
                    }
                    if ((item = cJSON_GetObjectItem(root, "mirrorSprite")) && cJSON_IsBool(item)) {
                        scene.mirrorSprite = cJSON_IsTrue(item);
                    }
                    
                    // Modular scene fields (new LED/Display system)
                    if ((item = cJSON_GetObjectItem(root, "displayEnabled")) && cJSON_IsBool(item)) {
                        scene.displayEnabled = cJSON_IsTrue(item);
                    }
                    if ((item = cJSON_GetObjectItem(root, "ledsEnabled")) && cJSON_IsBool(item)) {
                        scene.ledsEnabled = cJSON_IsTrue(item);
                    }
                    if ((item = cJSON_GetObjectItem(root, "effectsOnly")) && cJSON_IsBool(item)) {
                        scene.effectsOnly = cJSON_IsTrue(item);
                    }
                    if ((item = cJSON_GetObjectItem(root, "name")) && cJSON_IsString(item)) {
                        scene.name = item->valuestring;
                    }
                    
                    // Shader settings
                    if ((item = cJSON_GetObjectItem(root, "shaderAA")) && cJSON_IsBool(item)) {
                        scene.shaderAA = cJSON_IsTrue(item);
                    }
                    if ((item = cJSON_GetObjectItem(root, "shaderInvert")) && cJSON_IsBool(item)) {
                        scene.shaderInvert = cJSON_IsTrue(item);
                    }
                    if ((item = cJSON_GetObjectItem(root, "shaderColorMode")) && cJSON_IsString(item)) {
                        scene.shaderColorMode = item->valuestring;
                    }
                    if ((item = cJSON_GetObjectItem(root, "shaderColor")) && cJSON_IsString(item)) {
                        scene.shaderColor = item->valuestring;
                    }
                    
                    // Background color (display background)
                    if ((item = cJSON_GetObjectItem(root, "bgR")) && cJSON_IsNumber(item)) {
                        scene.bgR = (uint8_t)item->valueint;
                    }
                    if ((item = cJSON_GetObjectItem(root, "bgG")) && cJSON_IsNumber(item)) {
                        scene.bgG = (uint8_t)item->valueint;
                    }
                    if ((item = cJSON_GetObjectItem(root, "bgB")) && cJSON_IsNumber(item)) {
                        scene.bgB = (uint8_t)item->valueint;
                    }
                    
                    // LED settings
                    cJSON* ledColor = cJSON_GetObjectItem(root, "ledColor");
                    if (ledColor && cJSON_IsObject(ledColor)) {
                        if ((item = cJSON_GetObjectItem(ledColor, "r")) && cJSON_IsNumber(item)) {
                            scene.ledR = (uint8_t)item->valueint;
                        }
                        if ((item = cJSON_GetObjectItem(ledColor, "g")) && cJSON_IsNumber(item)) {
                            scene.ledG = (uint8_t)item->valueint;
                        }
                        if ((item = cJSON_GetObjectItem(ledColor, "b")) && cJSON_IsNumber(item)) {
                            scene.ledB = (uint8_t)item->valueint;
                        }
                    }
                    if ((item = cJSON_GetObjectItem(root, "ledBrightness")) && cJSON_IsNumber(item)) {
                        scene.ledBrightness = (uint8_t)item->valueint;
                    }
                    
                    // Animation parameters from nested 'animParams' object (new page format)
                    // MERGE params instead of clearing - so incremental updates work
                    cJSON* animParams = cJSON_GetObjectItem(root, "animParams");
                    bool hasAnimParamUpdates = false;
                    if (animParams && cJSON_IsObject(animParams)) {
                        printf("[SceneUpdate] Received animParams for scene %d\n", scene.id);
                        cJSON* param = NULL;
                        cJSON_ArrayForEach(param, animParams) {
                            if (param->string) {
                                float value = 0.0f;
                                if (cJSON_IsNumber(param)) {
                                    value = (float)param->valuedouble;
                                    scene.params[param->string] = value;
                                    printf("  [animParam] '%s' = %.2f\n", param->string, value);
                                } else if (cJSON_IsBool(param)) {
                                    value = cJSON_IsTrue(param) ? 1.0f : 0.0f;
                                    scene.params[param->string] = value;
                                    printf("  [animParam] '%s' = %.2f (bool)\n", param->string, value);
                                }
                                // Use single-param callback for immediate live update
                                auto& singleCallback = getSingleParamCallback();
                                printf("  [singleCallback] available=%s\n", singleCallback ? "YES" : "NO");
                                if (singleCallback) {
                                    printf("  [singleCallback] Calling for '%s' = %.2f\n", param->string, value);
                                    singleCallback(param->string, value);
                                    printf("  [singleCallback] Done\n");
                                }
                                hasAnimParamUpdates = true;
                            }
                        }
                    } else {
                        printf("[SceneUpdate] No animParams object in request\n");
                    }
                    
                    // Shader parameters from nested 'shaderParams' object
                    cJSON* shaderParams = cJSON_GetObjectItem(root, "shaderParams");
                    if (shaderParams && cJSON_IsObject(shaderParams)) {
                        printf("[SceneUpdate] Received shaderParams for scene %d\n", scene.id);
                        cJSON* param = NULL;
                        cJSON_ArrayForEach(param, shaderParams) {
                            if (param->string) {
                                // Handle color objects (mask_color, override_color)
                                if (cJSON_IsObject(param)) {
                                    cJSON* rItem = cJSON_GetObjectItem(param, "r");
                                    cJSON* gItem = cJSON_GetObjectItem(param, "g");
                                    cJSON* bItem = cJSON_GetObjectItem(param, "b");
                                    if (rItem && gItem && bItem) {
                                        // Convert color object to individual R/G/B params
                                        if (strcmp(param->string, "mask_color") == 0) {
                                            // Store in scene.params for persistence
                                            scene.params["shader_mask_r"] = (float)rItem->valueint;
                                            scene.params["shader_mask_g"] = (float)gItem->valueint;
                                            scene.params["shader_mask_b"] = (float)bItem->valueint;
                                            auto& singleCallback = getSingleParamCallback();
                                            if (singleCallback) {
                                                singleCallback("shader_mask_r", (float)rItem->valueint);
                                                singleCallback("shader_mask_g", (float)gItem->valueint);
                                                singleCallback("shader_mask_b", (float)bItem->valueint);
                                            }
                                            printf("  [shaderParam] mask_color = (%d,%d,%d)\n", 
                                                   rItem->valueint, gItem->valueint, bItem->valueint);
                                        } else if (strcmp(param->string, "override_color") == 0) {
                                            // Store in scene.params for persistence
                                            scene.params["shader_override_r"] = (float)rItem->valueint;
                                            scene.params["shader_override_g"] = (float)gItem->valueint;
                                            scene.params["shader_override_b"] = (float)bItem->valueint;
                                            auto& singleCallback = getSingleParamCallback();
                                            if (singleCallback) {
                                                singleCallback("shader_override_r", (float)rItem->valueint);
                                                singleCallback("shader_override_g", (float)gItem->valueint);
                                                singleCallback("shader_override_b", (float)bItem->valueint);
                                            }
                                            printf("  [shaderParam] override_color = (%d,%d,%d)\n", 
                                                   rItem->valueint, gItem->valueint, bItem->valueint);
                                        } else if (strncmp(param->string, "hue_color_", 10) == 0) {
                                            // Handle hue cycle palette colors (hue_color_0 to hue_color_31)
                                            // Parse index - could be 1 or 2 digits
                                            int colorIdx = 0;
                                            const char* idxStr = param->string + 10;
                                            if (idxStr[0] >= '0' && idxStr[0] <= '9') {
                                                colorIdx = idxStr[0] - '0';
                                                if (idxStr[1] >= '0' && idxStr[1] <= '9') {
                                                    colorIdx = colorIdx * 10 + (idxStr[1] - '0');
                                                }
                                            }
                                            if (colorIdx >= 0 && colorIdx < 32) {
                                                // Store in scene.params for persistence
                                                char paramNameR[32], paramNameG[32], paramNameB[32];
                                                snprintf(paramNameR, sizeof(paramNameR), "shader_hue_color_%d_r", colorIdx);
                                                snprintf(paramNameG, sizeof(paramNameG), "shader_hue_color_%d_g", colorIdx);
                                                snprintf(paramNameB, sizeof(paramNameB), "shader_hue_color_%d_b", colorIdx);
                                                scene.params[paramNameR] = (float)rItem->valueint;
                                                scene.params[paramNameG] = (float)gItem->valueint;
                                                scene.params[paramNameB] = (float)bItem->valueint;
                                                auto& singleCallback = getSingleParamCallback();
                                                if (singleCallback) {
                                                    singleCallback(paramNameR, (float)rItem->valueint);
                                                    singleCallback(paramNameG, (float)gItem->valueint);
                                                    singleCallback(paramNameB, (float)bItem->valueint);
                                                }
                                                printf("  [shaderParam] hue_color_%d = (%d,%d,%d)\n", 
                                                       colorIdx, rItem->valueint, gItem->valueint, bItem->valueint);
                                            }
                                        }
                                    }
                                } else {
                                    // Handle simple values (type, invert, mask_enabled)
                                    float value = 0.0f;
                                    if (cJSON_IsNumber(param)) {
                                        value = (float)param->valuedouble;
                                    } else if (cJSON_IsBool(param)) {
                                        value = cJSON_IsTrue(param) ? 1.0f : 0.0f;
                                    }
                                    // Map shaderParams names to internal param names and store in scene.params
                                    std::string internalName = std::string("shader_") + param->string;
                                    scene.params[internalName] = value;
                                    auto& singleCallback = getSingleParamCallback();
                                    if (singleCallback) {
                                        singleCallback(internalName.c_str(), value);
                                    }
                                    printf("  [shaderParam] '%s' -> '%s' = %.2f (saved to scene)\n", param->string, internalName.c_str(), value);
                                }
                            }
                        }
                    }
                    
                    // Update params object (legacy format) - also merge instead of clear
                    cJSON* params = cJSON_GetObjectItem(root, "params");
                    if (params && cJSON_IsObject(params)) {
                        cJSON* param = NULL;
                        cJSON_ArrayForEach(param, params) {
                            if (cJSON_IsNumber(param) && param->string) {
                                scene.params[param->string] = (float)param->valuedouble;
                            }
                        }
                    }
                    
                    // Update effects object
                    cJSON* effects = cJSON_GetObjectItem(root, "effects");
                    if (effects && cJSON_IsObject(effects)) {
                        scene.effects.clear();
                        cJSON* effect = NULL;
                        cJSON_ArrayForEach(effect, effects) {
                            if (cJSON_IsObject(effect) && effect->string) {
                                SavedScene::Effect eff;
                                cJSON* enabledItem = cJSON_GetObjectItem(effect, "enabled");
                                cJSON* intensityItem = cJSON_GetObjectItem(effect, "intensity");
                                if (enabledItem) eff.enabled = cJSON_IsTrue(enabledItem);
                                if (intensityItem && cJSON_IsNumber(intensityItem)) {
                                    eff.intensity = (float)intensityItem->valuedouble;
                                }
                                scene.effects[effect->string] = eff;
                            }
                        }
                    }
                    
                    // Legacy config format support
                    cJSON* config = cJSON_GetObjectItem(root, "config");
                    if (config) {
                        // Update gyro eye config
                        cJSON* gyroEye = cJSON_GetObjectItem(config, "gyroEye");
                        if (gyroEye && scene.type == 1) {
                            scene.hasGyroEyeConfig = true;
                            
                            if ((item = cJSON_GetObjectItem(gyroEye, "spriteId"))) scene.gyroEye.spriteId = item->valueint;
                            if ((item = cJSON_GetObjectItem(gyroEye, "intensity"))) scene.gyroEye.intensity = item->valuedouble;
                            if ((item = cJSON_GetObjectItem(gyroEye, "maxOffsetX"))) scene.gyroEye.maxOffsetX = item->valuedouble;
                            if ((item = cJSON_GetObjectItem(gyroEye, "maxOffsetY"))) scene.gyroEye.maxOffsetY = item->valuedouble;
                            if ((item = cJSON_GetObjectItem(gyroEye, "smoothingFactor"))) scene.gyroEye.smoothingFactor = item->valuedouble;
                            if ((item = cJSON_GetObjectItem(gyroEye, "eyeOffset"))) scene.gyroEye.eyeOffset = item->valueint;
                            if ((item = cJSON_GetObjectItem(gyroEye, "leftEyeCenterX"))) scene.gyroEye.leftEyeCenterX = item->valueint;
                            if ((item = cJSON_GetObjectItem(gyroEye, "leftEyeCenterY"))) scene.gyroEye.leftEyeCenterY = item->valueint;
                            if ((item = cJSON_GetObjectItem(gyroEye, "rightEyeCenterX"))) scene.gyroEye.rightEyeCenterX = item->valueint;
                            if ((item = cJSON_GetObjectItem(gyroEye, "rightEyeCenterY"))) scene.gyroEye.rightEyeCenterY = item->valueint;
                            if ((item = cJSON_GetObjectItem(gyroEye, "invertPitch"))) scene.gyroEye.invertPitch = cJSON_IsTrue(item);
                            if ((item = cJSON_GetObjectItem(gyroEye, "invertRoll"))) scene.gyroEye.invertRoll = cJSON_IsTrue(item);
                            if ((item = cJSON_GetObjectItem(gyroEye, "bgR"))) scene.gyroEye.bgR = item->valueint;
                            if ((item = cJSON_GetObjectItem(gyroEye, "bgG"))) scene.gyroEye.bgG = item->valueint;
                            if ((item = cJSON_GetObjectItem(gyroEye, "bgB"))) scene.gyroEye.bgB = item->valueint;
                        }
                        
                        // Update static sprite config
                        cJSON* staticSprite = cJSON_GetObjectItem(config, "staticSprite");
                        if (staticSprite && scene.type == 2) {
                            scene.hasStaticSpriteConfig = true;
                            
                            if ((item = cJSON_GetObjectItem(staticSprite, "spriteId"))) scene.staticSprite.spriteId = item->valueint;
                            if ((item = cJSON_GetObjectItem(staticSprite, "posX"))) scene.staticSprite.posX = item->valueint;
                            if ((item = cJSON_GetObjectItem(staticSprite, "posY"))) scene.staticSprite.posY = item->valueint;
                            if ((item = cJSON_GetObjectItem(staticSprite, "bgR"))) scene.staticSprite.bgR = item->valueint;
                            if ((item = cJSON_GetObjectItem(staticSprite, "bgG"))) scene.staticSprite.bgG = item->valueint;
                            if ((item = cJSON_GetObjectItem(staticSprite, "bgB"))) scene.staticSprite.bgB = item->valueint;
                        }
                    }
                    
                    success = true;
                    
                    // Save to storage periodically for slider updates,
                    // but force immediate save for important changes like animType
                    static uint32_t lastSaveTime = 0;
                    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
                    if (animTypeChanged || (now - lastSaveTime > 2000)) {
                        if (animTypeChanged) {
                            ESP_LOGI(HTTP_TAG, "Forcing immediate save due to animType change");
                        }
                        saveScenesStorage();
                        lastSaveTime = now;
                    }
                    
                    // Call full scene callback if:
                    // 1. animType changed (need to switch animation modes)
                    // 2. OR no animParams were sent (not a slider update)
                    if (animTypeChanged || !hasAnimParamUpdates) {
                        auto& callback = getSceneUpdatedCallback();
                        ESP_LOGI(HTTP_TAG, "Scene %d update complete. Active=%d, Callback=%s, animTypeChanged=%d", 
                                 scene.id, scene.active, callback ? "YES" : "NO", animTypeChanged);
                        if (callback) {
                            ESP_LOGI(HTTP_TAG, "Calling sceneUpdatedCallback for scene %d (animTypeChanged=%d)", scene.id, animTypeChanged);
                            callback(scene);
                        }
                    } else {
                        ESP_LOGI(HTTP_TAG, "Scene %d: Skipping full callback (single-param already applied)", scene.id);
                    }
                    
                    ESP_LOGI(HTTP_TAG, "Updated scene: %s (id %d, animType=%s, transition=%s)", 
                             scene.name.c_str(), scene.id, scene.animType.c_str(), scene.transition.c_str());
                    break;
                }
            }
        }
        
        cJSON_Delete(root);
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, success ? "{\"success\":true}" : "{\"success\":false}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    /**
     * @brief Display sprite on HUB75 directly (for scene testing)
     * Pipeline: Web UI -> Core 0 -> Callback -> Core 1 -> GPU -> HUB75
     */
    static esp_err_t handleApiSceneDisplay(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return sendUnauthorized(req);
        
        char buf[256];
        int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
        if (ret <= 0) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No body");
            return ESP_FAIL;
        }
        buf[ret] = '\0';
        
        cJSON* root = cJSON_Parse(buf);
        if (!root) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
            return ESP_FAIL;
        }
        
        bool success = false;
        
        cJSON* spriteId = cJSON_GetObjectItem(root, "spriteId");
        cJSON* posX = cJSON_GetObjectItem(root, "posX");
        cJSON* posY = cJSON_GetObjectItem(root, "posY");
        cJSON* bgR = cJSON_GetObjectItem(root, "bgR");
        cJSON* bgG = cJSON_GetObjectItem(root, "bgG");
        cJSON* bgB = cJSON_GetObjectItem(root, "bgB");
        
        if (spriteId && cJSON_IsNumber(spriteId)) {
            // Build config for sprite display
            StaticSpriteSceneConfig config;
            config.spriteId = spriteId->valueint;
            config.posX = (posX && cJSON_IsNumber(posX)) ? posX->valueint : 0;
            config.posY = (posY && cJSON_IsNumber(posY)) ? posY->valueint : 0;
            config.bgR = (bgR && cJSON_IsNumber(bgR)) ? (uint8_t)bgR->valueint : 0;
            config.bgG = (bgG && cJSON_IsNumber(bgG)) ? (uint8_t)bgG->valueint : 0;
            config.bgB = (bgB && cJSON_IsNumber(bgB)) ? (uint8_t)bgB->valueint : 0;
            
            ESP_LOGI(HTTP_TAG, "Scene display request: sprite=%d pos=(%d,%d) bg=(%d,%d,%d) sprites_count=%d",
                     config.spriteId, config.posX, config.posY,
                     config.bgR, config.bgG, config.bgB, savedSprites_.size());
            
            // Notify renderer via callback (works even if sprite doesn't exist - shows placeholder)
            auto& callback = getSpriteDisplayCallback();
            if (callback) {
                callback(config);
                ESP_LOGI(HTTP_TAG, "Sprite display callback invoked successfully");
                success = true;  // Set success here, after callback invoked
            } else {
                ESP_LOGW(HTTP_TAG, "No sprite display callback registered!");
            }
        } else {
            ESP_LOGW(HTTP_TAG, "Invalid or missing spriteId in request");
        }
        
        cJSON_Delete(root);
        
        httpd_resp_set_type(req, "application/json");
        if (success) {
            httpd_resp_send(req, "{\"success\":true}", HTTPD_RESP_USE_STRLEN);
        } else {
            httpd_resp_send(req, "{\"success\":false,\"error\":\"Sprite not found or display not ready\"}", HTTPD_RESP_USE_STRLEN);
        }
        return ESP_OK;
    }
    
    /**
     * @brief Clear HUB75 display
     */
    static esp_err_t handleApiSceneClear(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return sendUnauthorized(req);
        
        bool success = false;
        
        auto& callback = getDisplayClearCallback();
        if (callback) {
            callback();
            ESP_LOGI(HTTP_TAG, "Display cleared");
            success = true;  // Set success here, after callback invoked
        } else {
            ESP_LOGW(HTTP_TAG, "No display clear callback registered");
        }
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, success ? "{\"success\":true}" : "{\"success\":false,\"error\":\"Display not ready\"}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    // ========== Sprite API Handlers ==========
    
    /**
     * @brief Get list of saved sprites
     */
    static esp_err_t handleApiSprites(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return sendUnauthorized(req);
        
        ESP_LOGI(HTTP_TAG, "GET /api/sprites - savedSprites_ has %zu entries", savedSprites_.size());
        
        cJSON* root = cJSON_CreateObject();
        cJSON* sprites = cJSON_CreateArray();
        
        auto& fs = Utils::FileSystemService::instance();
        
        for (auto& sprite : savedSprites_) {
            ESP_LOGI(HTTP_TAG, "  Sprite: id=%d, name='%s', %dx%d", 
                     sprite.id, sprite.name.c_str(), sprite.width, sprite.height);
            
            // Lazy load preview if needed for API response
            if (sprite.preview.empty() && sprite.savedToSd && sdcard_storage_ready_ && fs.isReady()) {
                char previewRelPath[64];
                snprintf(previewRelPath, sizeof(previewRelPath), "/Sprites/preview_%d.txt", sprite.id);
                if (fs.fileExists(previewRelPath)) {
                    char* previewBuf = nullptr;
                    size_t previewSize = 0;
                    if (fs.readFile(previewRelPath, &previewBuf, &previewSize) && previewBuf) {
                        sprite.preview = std::string(previewBuf, previewSize);
                        free(previewBuf);
                        ESP_LOGI(HTTP_TAG, "    Lazy loaded preview (%zu bytes)", sprite.preview.size());
                    }
                }
            }
            
            cJSON* item = cJSON_CreateObject();
            cJSON_AddNumberToObject(item, "id", sprite.id);
            cJSON_AddStringToObject(item, "name", sprite.name.c_str());
            cJSON_AddNumberToObject(item, "width", sprite.width);
            cJSON_AddNumberToObject(item, "height", sprite.height);
            cJSON_AddNumberToObject(item, "scale", sprite.scale);
            // Calculate size in bytes (RGB888)
            int sizeBytes = sprite.width * sprite.height * 3;
            cJSON_AddNumberToObject(item, "sizeBytes", sizeBytes);
            cJSON_AddBoolToObject(item, "hasPixels", !sprite.pixelData.empty() || sprite.savedToSd);
            cJSON_AddNumberToObject(item, "pixelDataSize", sprite.pixelData.empty() ? sizeBytes : sprite.pixelData.size());
            cJSON_AddStringToObject(item, "preview", sprite.preview.c_str());
            cJSON_AddItemToArray(sprites, item);
        }
        
        cJSON_AddItemToObject(root, "sprites", sprites);
        
        char* json = cJSON_PrintUnformatted(root);
        cJSON_Delete(root);
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
        free(json);
        return ESP_OK;
    }
    
    /**
     * @brief Save a new sprite
     */
    static esp_err_t handleApiSpriteSave(httpd_req_t* req) {
        ESP_LOGI(HTTP_TAG, "handleApiSpriteSave called, content_len=%d", req->content_len);
        if (requiresAuthRedirect(req)) return sendUnauthorized(req);
        
        ESP_LOGI(HTTP_TAG, "Sprite save request, content length: %d", req->content_len);
        
        if (req->content_len > 128 * 1024) {
            httpd_resp_send_err(req, HTTPD_413_CONTENT_TOO_LARGE, "Payload too large");
            return ESP_FAIL;
        }
        
        char* buf = (char*)malloc(req->content_len + 1);
        if (!buf) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
            return ESP_FAIL;
        }
        
        int total = 0, remaining = req->content_len;
        while (remaining > 0) {
            int ret = httpd_req_recv(req, buf + total, remaining);
            if (ret <= 0) { free(buf); return ESP_FAIL; }
            total += ret;
            remaining -= ret;
        }
        buf[total] = '\0';
        
        cJSON* root = cJSON_Parse(buf);
        free(buf);
        
        if (!root) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
            return ESP_FAIL;
        }
        
        cJSON* name = cJSON_GetObjectItem(root, "name");
        cJSON* width = cJSON_GetObjectItem(root, "width");
        cJSON* height = cJSON_GetObjectItem(root, "height");
        cJSON* scale = cJSON_GetObjectItem(root, "scale");
        cJSON* preview = cJSON_GetObjectItem(root, "preview");
        cJSON* pixels = cJSON_GetObjectItem(root, "pixels");  // Base64 RGB888 data
        
        bool success = false;
        
        if (name && cJSON_IsString(name)) {
            SavedSprite sprite;
            sprite.id = nextSpriteId_++;
            sprite.name = name->valuestring;
            sprite.width = width ? width->valueint : 64;
            sprite.height = height ? height->valueint : 32;
            sprite.scale = scale ? scale->valueint : 100;
            sprite.preview = preview && cJSON_IsString(preview) ? preview->valuestring : "";
            sprite.uploadedToGpu = false;
            
            // Decode base64 pixel data if provided
            if (pixels && cJSON_IsString(pixels)) {
                size_t expectedSize = sprite.width * sprite.height * 3;  // RGB888
                sprite.pixelData.resize(expectedSize);
                size_t decodedSize = 0;
                
                if (decodeBase64(pixels->valuestring, sprite.pixelData.data(), expectedSize, &decodedSize)) {
                    if (decodedSize == expectedSize) {
                        ESP_LOGI(HTTP_TAG, "Decoded %d bytes of pixel data for sprite '%s' (%dx%d)", 
                                 decodedSize, sprite.name.c_str(), sprite.width, sprite.height);
                    } else {
                        ESP_LOGW(HTTP_TAG, "Pixel data size mismatch: expected %d, got %d", expectedSize, decodedSize);
                        sprite.pixelData.clear();
                    }
                } else {
                    ESP_LOGW(HTTP_TAG, "Failed to decode base64 pixel data");
                    sprite.pixelData.clear();
                }
            } else {
                ESP_LOGW(HTTP_TAG, "No pixel data in sprite save request");
            }
            
            savedSprites_.push_back(sprite);
            ESP_LOGI(HTTP_TAG, "Saved sprite '%s' with id %d, pixels=%s", 
                     sprite.name.c_str(), sprite.id, 
                     sprite.pixelData.empty() ? "NO" : "YES");
            saveSpritesToStorage();  // Persist to flash
            success = true;
        }
        
        cJSON_Delete(root);
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, success ? "{\"success\":true}" : "{\"success\":false}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    /**
     * @brief Rename a sprite
     */
    static esp_err_t handleApiSpriteRename(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return sendUnauthorized(req);
        
        char buf[512];
        int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
        if (ret <= 0) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No body");
            return ESP_FAIL;
        }
        buf[ret] = '\0';
        
        cJSON* root = cJSON_Parse(buf);
        if (!root) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
            return ESP_FAIL;
        }
        
        cJSON* id = cJSON_GetObjectItem(root, "id");
        cJSON* name = cJSON_GetObjectItem(root, "name");
        
        bool success = false;
        
        if (id && cJSON_IsNumber(id) && name && cJSON_IsString(name)) {
            int spriteId = id->valueint;
            for (auto& sprite : savedSprites_) {
                if (sprite.id == spriteId) {
                    sprite.name = name->valuestring;
                    ESP_LOGI(HTTP_TAG, "Renamed sprite %d to '%s'", spriteId, sprite.name.c_str());
                    saveSpritesToStorage();  // Persist to flash
                    success = true;
                    break;
                }
            }
        }
        
        cJSON_Delete(root);
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, success ? "{\"success\":true}" : "{\"success\":false}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    /**
     * @brief Delete a sprite
     */
    static esp_err_t handleApiSpriteDelete(httpd_req_t* req) {
        ESP_LOGI(HTTP_TAG, "handleApiSpriteDelete called");
        if (requiresAuthRedirect(req)) return sendUnauthorized(req);
        
        char buf[256];
        int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
        if (ret <= 0) {
            ESP_LOGE(HTTP_TAG, "Delete sprite: No body received");
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No body");
            return ESP_FAIL;
        }
        buf[ret] = '\0';
        ESP_LOGI(HTTP_TAG, "Delete sprite body: %s", buf);
        
        cJSON* root = cJSON_Parse(buf);
        if (!root) {
            ESP_LOGE(HTTP_TAG, "Delete sprite: Invalid JSON");
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
            return ESP_FAIL;
        }
        
        cJSON* id = cJSON_GetObjectItem(root, "id");
        bool success = false;
        
        if (id && cJSON_IsNumber(id)) {
            int spriteId = id->valueint;
            ESP_LOGI(HTTP_TAG, "Attempting to delete sprite ID: %d (total sprites: %d)", 
                     spriteId, savedSprites_.size());
            for (auto it = savedSprites_.begin(); it != savedSprites_.end(); ++it) {
                if (it->id == spriteId) {
                    ESP_LOGI(HTTP_TAG, "Deleted sprite %d ('%s')", spriteId, it->name.c_str());
                    
                    // Actually delete the files from SD card to prevent recovery
                    auto& fs = Utils::FileSystemService::instance();
                    if (fs.isReady() && fs.isMounted()) {
                        char pixelPath[64];
                        char previewPath[64];
                        snprintf(pixelPath, sizeof(pixelPath), "/Sprites/sprite_%d.bin", spriteId);
                        snprintf(previewPath, sizeof(previewPath), "/Sprites/preview_%d.txt", spriteId);
                        
                        ESP_LOGI(HTTP_TAG, "Deleting sprite files: %s, %s", pixelPath, previewPath);
                        fs.deleteFile(pixelPath);
                        fs.deleteFile(previewPath);
                        vTaskDelay(pdMS_TO_TICKS(100));  // Let filesystem sync
                        SystemAPI::Utils::syncFilesystem();
                    }
                    
                    savedSprites_.erase(it);
                    saveSpritesToStorage();  // Persist updated index
                    success = true;
                    break;
                }
            }
            if (!success) {
                ESP_LOGW(HTTP_TAG, "Sprite ID %d not found", spriteId);
            }
        } else {
            ESP_LOGE(HTTP_TAG, "Delete sprite: Missing or invalid 'id' field");
        }
        
        cJSON_Delete(root);
        
        ESP_LOGI(HTTP_TAG, "Delete sprite result: %s", success ? "success" : "failed");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, success ? "{\"success\":true}" : "{\"success\":false}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    /**
     * @brief Handle sprite upload and apply
     * Receives RGB888 pixel data for both panels
     */
    static esp_err_t handleApiSpriteApply(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return sendUnauthorized(req);
        
        ESP_LOGI(HTTP_TAG, "Sprite apply request, content length: %d", req->content_len);
        
        // Sprite data can be large (64*32*3*2 = 12KB), allocate buffer
        if (req->content_len > 64 * 1024) {
            httpd_resp_send_err(req, HTTPD_413_CONTENT_TOO_LARGE, "Payload too large");
            return ESP_FAIL;
        }
        
        char* buf = (char*)malloc(req->content_len + 1);
        if (!buf) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
            return ESP_FAIL;
        }
        
        int total_received = 0;
        int remaining = req->content_len;
        
        while (remaining > 0) {
            int ret = httpd_req_recv(req, buf + total_received, remaining);
            if (ret <= 0) {
                free(buf);
                httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to receive data");
                return ESP_FAIL;
            }
            total_received += ret;
            remaining -= ret;
        }
        buf[total_received] = '\0';
        
        cJSON* root = cJSON_Parse(buf);
        free(buf);
        
        if (!root) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
            return ESP_FAIL;
        }
        
        // Extract parameters
        cJSON* width = cJSON_GetObjectItem(root, "width");
        cJSON* height = cJSON_GetObjectItem(root, "height");
        cJSON* offsetX = cJSON_GetObjectItem(root, "offsetX");
        cJSON* offsetY = cJSON_GetObjectItem(root, "offsetY");
        cJSON* scale = cJSON_GetObjectItem(root, "scale");
        cJSON* mirror = cJSON_GetObjectItem(root, "mirror");
        cJSON* leftPanel = cJSON_GetObjectItem(root, "leftPanel");
        cJSON* rightPanel = cJSON_GetObjectItem(root, "rightPanel");
        
        bool success = false;
        
        if (width && height && leftPanel && rightPanel && 
            cJSON_IsString(leftPanel) && cJSON_IsString(rightPanel)) {
            
            int w = width->valueint;
            int h = height->valueint;
            int expectedSize = w * h * 3;  // RGB888
            
            const char* leftB64 = leftPanel->valuestring;
            const char* rightB64 = rightPanel->valuestring;
            
            ESP_LOGI(HTTP_TAG, "Sprite: %dx%d, decoding base64...", w, h);
            
            // Decode base64 pixel data
            uint8_t* leftPixels = (uint8_t*)malloc(expectedSize);
            uint8_t* rightPixels = (uint8_t*)malloc(expectedSize);
            
            if (leftPixels && rightPixels) {
                size_t leftDecoded = 0, rightDecoded = 0;
                
                if (decodeBase64(leftB64, leftPixels, expectedSize, &leftDecoded) &&
                    decodeBase64(rightB64, rightPixels, expectedSize, &rightDecoded) &&
                    leftDecoded == expectedSize && rightDecoded == expectedSize) {
                    
                    // TODO: Send sprite data to GPU via command system
                    ESP_LOGI(HTTP_TAG, "Sprite data received successfully");
                    ESP_LOGI(HTTP_TAG, "  Offset: (%d, %d), Scale: %d%%, Mirror: %s",
                             offsetX ? offsetX->valueint : 0,
                             offsetY ? offsetY->valueint : 0,
                             scale ? scale->valueint : 100,
                             mirror && mirror->type == cJSON_True ? "yes" : "no");
                    
                    success = true;
                } else {
                    ESP_LOGE(HTTP_TAG, "Base64 decode failed or size mismatch: expected %d, got left=%d right=%d",
                             expectedSize, (int)leftDecoded, (int)rightDecoded);
                }
                
                free(leftPixels);
                free(rightPixels);
            } else {
                if (leftPixels) free(leftPixels);
                if (rightPixels) free(rightPixels);
                ESP_LOGE(HTTP_TAG, "Failed to allocate pixel buffers");
            }
        } else {
            ESP_LOGE(HTTP_TAG, "Missing required sprite fields or wrong type");
        }
        
        cJSON_Delete(root);
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, success ? "{\"success\":true}" : "{\"success\":false,\"error\":\"Invalid data\"}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    /**
     * @brief Get storage information for sprite storage
     */
    static esp_err_t handleApiStorage(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return sendUnauthorized(req);
        
        // Get storage stats based on which storage is active
        uint64_t totalBytes = 0, usedBytes = 0, freeBytes = 0;
        const char* storageType = "none";
        
        if (sdcard_storage_ready_) {
            // Use SD card stats
            auto& fs = Utils::FileSystemService::instance();
            totalBytes = fs.getTotalBytes();
            freeBytes = fs.getFreeBytes();
            usedBytes = totalBytes > freeBytes ? totalBytes - freeBytes : 0;
            storageType = "sdcard";
        } else if (spiffs_initialized_) {
            // Use SPIFFS stats
            size_t spiffsTotal = 0, spiffsUsed = 0;
            esp_spiffs_info(NULL, &spiffsTotal, &spiffsUsed);
            totalBytes = spiffsTotal;
            usedBytes = spiffsUsed;
            freeBytes = totalBytes > usedBytes ? totalBytes - usedBytes : 0;
            storageType = "spiffs";
        }
        
        cJSON* root = cJSON_CreateObject();
        cJSON_AddNumberToObject(root, "total", (double)totalBytes);
        cJSON_AddNumberToObject(root, "used", (double)usedBytes);
        cJSON_AddNumberToObject(root, "free", (double)freeBytes);
        cJSON_AddStringToObject(root, "storageType", storageType);
        cJSON_AddNumberToObject(root, "spriteCount", savedSprites_.size());
        cJSON_AddBoolToObject(root, "sdcardReady", sdcard_storage_ready_);
        cJSON_AddBoolToObject(root, "spiffsReady", spiffs_initialized_);
        
        // List sprite binary files for debugging
        cJSON* spriteFiles = cJSON_CreateArray();
        const char* dataDir = sdcard_storage_ready_ ? "/sdcard/Sprites" : "/spiffs/Sprites";
        DIR* dir = opendir(dataDir);
        if (dir) {
            struct dirent* entry;
            while ((entry = readdir(dir)) != NULL) {
                if (strstr(entry->d_name, ".bin") || strstr(entry->d_name, ".json")) {
                    cJSON* fileObj = cJSON_CreateObject();
                    cJSON_AddStringToObject(fileObj, "name", entry->d_name);
                    
                    // Get file size - limit filename to avoid truncation
                    char fullPath[128];
                    snprintf(fullPath, sizeof(fullPath), "%.15s/%.100s", dataDir, entry->d_name);
                    struct stat st;
                    if (stat(fullPath, &st) == 0) {
                        cJSON_AddNumberToObject(fileObj, "size", st.st_size);
                    }
                    
                    cJSON_AddItemToArray(spriteFiles, fileObj);
                }
            }
            closedir(dir);
        }
        cJSON_AddItemToObject(root, "spriteFiles", spriteFiles);
        cJSON_AddStringToObject(root, "spriteDir", dataDir);
        
        char* json = cJSON_PrintUnformatted(root);
        cJSON_Delete(root);
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
        free(json);
        return ESP_OK;
    }
    
    // ========== Equation API Handlers ==========
    
    /**
     * @brief Get all equations
     */
    static esp_err_t handleApiEquations(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return sendUnauthorized(req);
        
        cJSON* root = cJSON_CreateObject();
        cJSON* equations = cJSON_CreateArray();
        
        for (const auto& eq : savedEquations_) {
            cJSON* item = cJSON_CreateObject();
            cJSON_AddNumberToObject(item, "id", eq.id);
            cJSON_AddStringToObject(item, "name", eq.name.c_str());
            cJSON_AddStringToObject(item, "expression", eq.expression.c_str());
            
            cJSON* vars = cJSON_CreateArray();
            for (const auto& v : eq.variables) {
                cJSON* varItem = cJSON_CreateObject();
                cJSON_AddStringToObject(varItem, "name", v.name.c_str());
                cJSON_AddStringToObject(varItem, "type", v.type.c_str());
                cJSON_AddStringToObject(varItem, "value", v.value.c_str());
                cJSON_AddItemToArray(vars, varItem);
            }
            cJSON_AddItemToObject(item, "variables", vars);
            
            cJSON_AddItemToArray(equations, item);
        }
        
        cJSON_AddItemToObject(root, "equations", equations);
        
        char* json = cJSON_PrintUnformatted(root);
        cJSON_Delete(root);
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
        free(json);
        return ESP_OK;
    }
    
    /**
     * @brief Save (create or update) an equation
     */
    static esp_err_t handleApiEquationSave(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return sendUnauthorized(req);
        
        char buf[4096];
        int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
        if (ret <= 0) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No body");
            return ESP_FAIL;
        }
        buf[ret] = '\0';
        
        cJSON* root = cJSON_Parse(buf);
        if (!root) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
            return ESP_FAIL;
        }
        
        cJSON* id = cJSON_GetObjectItem(root, "id");
        cJSON* name = cJSON_GetObjectItem(root, "name");
        cJSON* expression = cJSON_GetObjectItem(root, "expression");
        cJSON* variables = cJSON_GetObjectItem(root, "variables");
        
        bool success = false;
        
        if (name && cJSON_IsString(name) && expression && cJSON_IsString(expression)) {
            SavedEquation eq;
            eq.name = name->valuestring;
            eq.expression = expression->valuestring;
            
            // Parse variables
            if (variables && cJSON_IsArray(variables)) {
                cJSON* varItem = NULL;
                cJSON_ArrayForEach(varItem, variables) {
                    EquationVariable v;
                    cJSON* vname = cJSON_GetObjectItem(varItem, "name");
                    cJSON* vtype = cJSON_GetObjectItem(varItem, "type");
                    cJSON* vvalue = cJSON_GetObjectItem(varItem, "value");
                    
                    if (vname && cJSON_IsString(vname)) v.name = vname->valuestring;
                    if (vtype && cJSON_IsString(vtype)) v.type = vtype->valuestring;
                    if (vvalue && cJSON_IsString(vvalue)) v.value = vvalue->valuestring;
                    
                    eq.variables.push_back(v);
                }
            }
            
            // Check if updating existing or creating new
            if (id && cJSON_IsNumber(id) && id->valueint > 0) {
                // Update existing
                for (auto& existing : savedEquations_) {
                    if (existing.id == id->valueint) {
                        existing.name = eq.name;
                        existing.expression = eq.expression;
                        existing.variables = eq.variables;
                        ESP_LOGI(HTTP_TAG, "Updated equation %d: '%s'", existing.id, existing.name.c_str());
                        success = true;
                        break;
                    }
                }
            } else {
                // Create new
                eq.id = nextEquationId_++;
                savedEquations_.push_back(eq);
                ESP_LOGI(HTTP_TAG, "Created equation %d: '%s'", eq.id, eq.name.c_str());
                success = true;
            }
            
            if (success) {
                saveEquationsToStorage();
            }
        }
        
        cJSON_Delete(root);
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, success ? "{\"success\":true}" : "{\"success\":false}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    /**
     * @brief Delete an equation
     */
    static esp_err_t handleApiEquationDelete(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return sendUnauthorized(req);
        
        char buf[256];
        int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
        if (ret <= 0) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No body");
            return ESP_FAIL;
        }
        buf[ret] = '\0';
        
        cJSON* root = cJSON_Parse(buf);
        if (!root) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
            return ESP_FAIL;
        }
        
        cJSON* id = cJSON_GetObjectItem(root, "id");
        bool success = false;
        
        if (id && cJSON_IsNumber(id)) {
            int eqId = id->valueint;
            for (auto it = savedEquations_.begin(); it != savedEquations_.end(); ++it) {
                if (it->id == eqId) {
                    ESP_LOGI(HTTP_TAG, "Deleted equation %d ('%s')", eqId, it->name.c_str());
                    savedEquations_.erase(it);
                    saveEquationsToStorage();
                    success = true;
                    break;
                }
            }
        }
        
        cJSON_Delete(root);
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, success ? "{\"success\":true}" : "{\"success\":false}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    /**
     * @brief Get sensor values for equation editor
     */
    static esp_err_t handleApiSensors(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return sendUnauthorized(req);
        
        auto& state = SYNC_STATE.state();
        
        cJSON* root = cJSON_CreateObject();
        
        // System Time
        cJSON_AddNumberToObject(root, "millis", (double)(esp_timer_get_time() / 1000));
        
        // Environment Sensors
        cJSON_AddNumberToObject(root, "temperature", state.temperature);
        cJSON_AddNumberToObject(root, "humidity", state.humidity);
        cJSON_AddNumberToObject(root, "pressure", state.pressure);
        
        // IMU - Accelerometer
        cJSON_AddNumberToObject(root, "accel_x", state.accelX);
        cJSON_AddNumberToObject(root, "accel_y", state.accelY);
        cJSON_AddNumberToObject(root, "accel_z", state.accelZ);
        
        // IMU - Gyroscope
        cJSON_AddNumberToObject(root, "gyro_x", state.gyroX);
        cJSON_AddNumberToObject(root, "gyro_y", state.gyroY);
        cJSON_AddNumberToObject(root, "gyro_z", state.gyroZ);
        
        // GPS
        cJSON_AddNumberToObject(root, "gps_lat", state.latitude);
        cJSON_AddNumberToObject(root, "gps_lon", state.longitude);
        cJSON_AddNumberToObject(root, "gps_alt", state.altitude);
        cJSON_AddNumberToObject(root, "gps_speed", state.gpsSpeed);
        cJSON_AddNumberToObject(root, "gps_sats", state.satellites);
        
        // GPS Time - calculate unix timestamp
        // Simple approximation (not accounting for leap years perfectly)
        uint32_t unixTime = 0;
        if (state.gpsYear >= 1970) {
            uint32_t years = state.gpsYear - 1970;
            uint32_t days = years * 365 + (years + 1) / 4; // Approximate leap years
            static const uint16_t daysBeforeMonth[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
            if (state.gpsMonth >= 1 && state.gpsMonth <= 12) {
                days += daysBeforeMonth[state.gpsMonth - 1];
            }
            days += state.gpsDay - 1;
            unixTime = days * 86400 + state.gpsHour * 3600 + state.gpsMinute * 60 + state.gpsSecond;
        }
        cJSON_AddNumberToObject(root, "gps_unix", unixTime);
        cJSON_AddNumberToObject(root, "gps_hour", state.gpsHour);
        cJSON_AddNumberToObject(root, "gps_min", state.gpsMinute);
        cJSON_AddNumberToObject(root, "gps_sec", state.gpsSecond);
        
        // Microphone
        cJSON_AddNumberToObject(root, "mic_db", state.micDb);
        
        // Utility - random value between -1 and 1
        float randomVal = ((float)(esp_random() % 20001) - 10000.0f) / 10000.0f;
        cJSON_AddNumberToObject(root, "random", randomVal);
        
        // Device-corrected IMU (after calibration applied)
        cJSON_AddNumberToObject(root, "device_accel_x", state.deviceAccelX);
        cJSON_AddNumberToObject(root, "device_accel_y", state.deviceAccelY);
        cJSON_AddNumberToObject(root, "device_accel_z", state.deviceAccelZ);
        cJSON_AddNumberToObject(root, "device_gyro_x", state.deviceGyroX);
        cJSON_AddNumberToObject(root, "device_gyro_y", state.deviceGyroY);
        cJSON_AddNumberToObject(root, "device_gyro_z", state.deviceGyroZ);
        cJSON_AddBoolToObject(root, "imu_calibrated", state.imuCalibrated);
        
        char* json = cJSON_PrintUnformatted(root);
        cJSON_Delete(root);
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
        free(json);
        return ESP_OK;
    }
    
    // ========== IMU Calibration Handlers ==========
    
    // Static calibration state
    static inline bool imuCalibrationInProgress_ = false;
    static inline uint32_t imuCalibrationStartTime_ = 0;
    static inline float imuCalibAccumX_ = 0, imuCalibAccumY_ = 0, imuCalibAccumZ_ = 0;
    static inline uint32_t imuCalibSampleCount_ = 0;
    static constexpr uint32_t IMU_CALIB_DURATION_MS = 3000;
    static constexpr float GRAVITY = 9.81f;
    
    /**
     * @brief Start IMU calibration - record for 3 seconds
     */
    static esp_err_t handleApiImuCalibrate(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return sendUnauthorized(req);
        
        // Start calibration
        imuCalibrationInProgress_ = true;
        imuCalibrationStartTime_ = (uint32_t)(esp_timer_get_time() / 1000);
        imuCalibAccumX_ = 0;
        imuCalibAccumY_ = 0;
        imuCalibAccumZ_ = 0;
        imuCalibSampleCount_ = 0;
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"success\":true,\"message\":\"Calibration started. Keep device still for 3 seconds.\"}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    /**
     * @brief Get IMU calibration status
     */
    static esp_err_t handleApiImuStatus(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return sendUnauthorized(req);
        
        auto& state = SYNC_STATE.state();
        
        cJSON* root = cJSON_CreateObject();
        cJSON_AddBoolToObject(root, "calibrating", imuCalibrationInProgress_);
        cJSON_AddBoolToObject(root, "calibrated", state.imuCalibrated);
        
        if (imuCalibrationInProgress_) {
            uint32_t elapsed = (uint32_t)(esp_timer_get_time() / 1000) - imuCalibrationStartTime_;
            uint32_t remaining = elapsed < IMU_CALIB_DURATION_MS ? IMU_CALIB_DURATION_MS - elapsed : 0;
            cJSON_AddNumberToObject(root, "remainingMs", remaining);
            cJSON_AddNumberToObject(root, "progress", (float)elapsed / IMU_CALIB_DURATION_MS * 100.0f);
        }
        
        // Add current calibration matrix
        cJSON* matrix = cJSON_CreateArray();
        for (int i = 0; i < 9; i++) {
            cJSON_AddItemToArray(matrix, cJSON_CreateNumber(state.imuCalibMatrix[i]));
        }
        cJSON_AddItemToObject(root, "matrix", matrix);
        
        char* json = cJSON_PrintUnformatted(root);
        cJSON_Delete(root);
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
        free(json);
        return ESP_OK;
    }
    
    /**
     * @brief Clear IMU calibration
     */
    static esp_err_t handleApiImuClear(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return sendUnauthorized(req);
        
        auto& state = SYNC_STATE.state();
        
        // Reset to identity matrix
        state.imuCalibMatrix[0] = 1; state.imuCalibMatrix[1] = 0; state.imuCalibMatrix[2] = 0;
        state.imuCalibMatrix[3] = 0; state.imuCalibMatrix[4] = 1; state.imuCalibMatrix[5] = 0;
        state.imuCalibMatrix[6] = 0; state.imuCalibMatrix[7] = 0; state.imuCalibMatrix[8] = 1;
        state.imuCalibrated = false;
        
        // Clear from SD card via StorageManager
        auto& storageManager = Storage::StorageManager::instance();
        storageManager.clearImuCalibration();
        
        // Also clear from NVS (for migration cleanup)
        nvs_handle_t nvs;
        if (nvs_open("imu_calib", NVS_READWRITE, &nvs) == ESP_OK) {
            nvs_erase_all(nvs);
            nvs_commit(nvs);
            nvs_close(nvs);
        }
        
        ESP_LOGI(HTTP_TAG, "IMU calibration cleared from all storage");

        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"success\":true,\"message\":\"Calibration cleared\"}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }

    /**
     * @brief Toggle fan on/off
     */
    static esp_err_t handleApiFanToggle(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return sendUnauthorized(req);

        auto& state = SYNC_STATE.state();

        // Toggle fan state
        state.fanEnabled = !state.fanEnabled;
        SYNC_STATE.notifyChange(SyncState::FLAG_FAN);

        ESP_LOGI(HTTP_TAG, "Fan toggled: %s", state.fanEnabled ? "ON" : "OFF");

        // Build JSON response
        cJSON* root = cJSON_CreateObject();
        cJSON_AddBoolToObject(root, "success", true);
        cJSON_AddBoolToObject(root, "fanEnabled", state.fanEnabled);
        cJSON_AddNumberToObject(root, "fanSpeed", state.fanSpeed);

        char* json = cJSON_PrintUnformatted(root);
        cJSON_Delete(root);

        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
        free(json);
        return ESP_OK;
    }

public:
    /**
     * @brief Process IMU calibration - call this in the main loop
     * 
     * This should be called periodically to accumulate samples and
     * compute the calibration matrix when complete.
     */
    static void processImuCalibration() {
        if (!imuCalibrationInProgress_) return;
        
        auto& state = SYNC_STATE.state();
        uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
        uint32_t elapsed = now - imuCalibrationStartTime_;
        
        // Accumulate samples
        imuCalibAccumX_ += state.accelX;
        imuCalibAccumY_ += state.accelY;
        imuCalibAccumZ_ += state.accelZ;
        imuCalibSampleCount_++;
        
        // Check if calibration is complete
        if (elapsed >= IMU_CALIB_DURATION_MS && imuCalibSampleCount_ > 0) {
            imuCalibrationInProgress_ = false;
            
            // Average the accumulated values
            float avgX = imuCalibAccumX_ / imuCalibSampleCount_;
            float avgY = imuCalibAccumY_ / imuCalibSampleCount_;
            float avgZ = imuCalibAccumZ_ / imuCalibSampleCount_;
            
            // Normalize to get gravity direction in IMU frame
            float mag = sqrtf(avgX * avgX + avgY * avgY + avgZ * avgZ);
            if (mag < 0.1f) {
                ESP_LOGW(HTTP_TAG, "IMU calibration failed - magnitude too low");
                return;
            }
            
            // Gravity vector in IMU coordinates (normalized)
            float gx = avgX / mag;
            float gy = avgY / mag;
            float gz = avgZ / mag;
            
            // We want to rotate so that gravity points in +Z direction (device down = positive Z)
            // Target gravity direction: (0, 0, 1)
            // After calibration: device_x ≈ 0, device_y ≈ 0, device_z ≈ gravity magnitude
            // Build rotation matrix that transforms (gx, gy, gz) to (0, 0, 1)
            
            // Use Rodrigues' rotation formula
            // rotation axis = gravity x target = (gx,gy,gz) x (0,0,1) = (gy*1-gz*0, gz*0-gx*1, gx*0-gy*0) = (gy, -gx, 0)
            // rotation angle = acos(dot(gravity, target)) = acos(gz)
            
            float ax = gy;
            float ay = -gx;
            float az = 0;
            float axisMag = sqrtf(ax * ax + ay * ay);
            
            if (axisMag < 0.001f) {
                // Gravity already aligned with Z axis
                if (gz > 0) {
                    // Already pointing in +Z - identity matrix
                    state.imuCalibMatrix[0] = 1; state.imuCalibMatrix[1] = 0; state.imuCalibMatrix[2] = 0;
                    state.imuCalibMatrix[3] = 0; state.imuCalibMatrix[4] = 1; state.imuCalibMatrix[5] = 0;
                    state.imuCalibMatrix[6] = 0; state.imuCalibMatrix[7] = 0; state.imuCalibMatrix[8] = 1;
                } else {
                    // Pointing in -Z - 180 degree rotation around X to flip
                    state.imuCalibMatrix[0] = 1; state.imuCalibMatrix[1] = 0; state.imuCalibMatrix[2] = 0;
                    state.imuCalibMatrix[3] = 0; state.imuCalibMatrix[4] = -1; state.imuCalibMatrix[5] = 0;
                    state.imuCalibMatrix[6] = 0; state.imuCalibMatrix[7] = 0; state.imuCalibMatrix[8] = -1;
                }
            } else {
                // Normalize rotation axis
                ax /= axisMag;
                ay /= axisMag;
                
                // Rotation angle
                float cosAngle = gz;  // dot(gravity, (0,0,1))
                float angle = acosf(cosAngle > 1.0f ? 1.0f : (cosAngle < -1.0f ? -1.0f : cosAngle));
                float sinAngle = sinf(angle);
                float oneMinusCos = 1.0f - cosAngle;
                
                // Rodrigues' rotation formula: R = I + sin(θ)K + (1-cos(θ))K²
                // where K is the skew-symmetric cross-product matrix of the axis
                state.imuCalibMatrix[0] = cosAngle + ax * ax * oneMinusCos;
                state.imuCalibMatrix[1] = ax * ay * oneMinusCos - az * sinAngle;
                state.imuCalibMatrix[2] = ax * az * oneMinusCos + ay * sinAngle;
                
                state.imuCalibMatrix[3] = ay * ax * oneMinusCos + az * sinAngle;
                state.imuCalibMatrix[4] = cosAngle + ay * ay * oneMinusCos;
                state.imuCalibMatrix[5] = ay * az * oneMinusCos - ax * sinAngle;
                
                state.imuCalibMatrix[6] = az * ax * oneMinusCos - ay * sinAngle;
                state.imuCalibMatrix[7] = az * ay * oneMinusCos + ax * sinAngle;
                state.imuCalibMatrix[8] = cosAngle + az * az * oneMinusCos;
            }
            
            state.imuCalibrated = true;
            
            // Save to SD card (primary) via StorageManager
            auto& storageManager = Storage::StorageManager::instance();
            Storage::ImuCalibrationData calibData;
            calibData.valid = true;
            memcpy(calibData.matrix, state.imuCalibMatrix, sizeof(calibData.matrix));
            calibData.timestamp = (uint32_t)(esp_timer_get_time() / 1000000);  // seconds
            
            if (storageManager.saveImuCalibration(calibData)) {
                ESP_LOGI(HTTP_TAG, "IMU calibration saved to SD card");
            } else {
                // Fallback to NVS if SD card save fails
                nvs_handle_t nvs;
                if (nvs_open("imu_calib", NVS_READWRITE, &nvs) == ESP_OK) {
                    nvs_set_blob(nvs, "matrix", state.imuCalibMatrix, sizeof(state.imuCalibMatrix));
                    nvs_set_u8(nvs, "valid", 1);
                    nvs_commit(nvs);
                    nvs_close(nvs);
                    ESP_LOGI(HTTP_TAG, "IMU calibration saved to NVS (SD card unavailable)");
                }
            }
            
            ESP_LOGI(HTTP_TAG, "IMU calibration complete. Gravity: (%.2f, %.2f, %.2f)", gx, gy, gz);
        }
    }
    
    /**
     * @brief Apply IMU calibration to get device-frame values
     * Call this after reading raw IMU values
     */
    static void applyImuCalibration() {
        auto& state = SYNC_STATE.state();
        
        if (!state.imuCalibrated) {
            // No calibration - use raw values
            state.deviceAccelX = state.accelX;
            state.deviceAccelY = state.accelY;
            state.deviceAccelZ = state.accelZ;
            state.deviceGyroX = state.gyroX;
            state.deviceGyroY = state.gyroY;
            state.deviceGyroZ = state.gyroZ;
            return;
        }
        
        // Apply rotation matrix: device = R * imu
        float* R = state.imuCalibMatrix;
        
        // Transform accelerometer
        state.deviceAccelX = R[0] * state.accelX + R[1] * state.accelY + R[2] * state.accelZ;
        state.deviceAccelY = R[3] * state.accelX + R[4] * state.accelY + R[5] * state.accelZ;
        state.deviceAccelZ = R[6] * state.accelX + R[7] * state.accelY + R[8] * state.accelZ;
        
        // Transform gyroscope
        state.deviceGyroX = R[0] * state.gyroX + R[1] * state.gyroY + R[2] * state.gyroZ;
        state.deviceGyroY = R[3] * state.gyroX + R[4] * state.gyroY + R[5] * state.gyroZ;
        state.deviceGyroZ = R[6] * state.gyroX + R[7] * state.gyroY + R[8] * state.gyroZ;
    }
    
    /**
     * @brief Load IMU calibration from storage (SD card preferred, NVS fallback)
     */
    static void loadImuCalibration() {
        auto& state = SYNC_STATE.state();
        
        // Try SD card first via StorageManager
        auto& storageManager = Storage::StorageManager::instance();
        Storage::ImuCalibrationData calibData;
        
        if (storageManager.loadImuCalibration(calibData) && calibData.valid) {
            memcpy(state.imuCalibMatrix, calibData.matrix, sizeof(state.imuCalibMatrix));
            state.imuCalibrated = true;
            ESP_LOGI(HTTP_TAG, "IMU calibration loaded from SD card (timestamp: %u)", calibData.timestamp);
            return;
        }
        
        // Fallback to NVS
        nvs_handle_t nvs;
        if (nvs_open("imu_calib", NVS_READONLY, &nvs) == ESP_OK) {
            uint8_t valid = 0;
            if (nvs_get_u8(nvs, "valid", &valid) == ESP_OK && valid == 1) {
                size_t len = sizeof(state.imuCalibMatrix);
                if (nvs_get_blob(nvs, "matrix", state.imuCalibMatrix, &len) == ESP_OK) {
                    state.imuCalibrated = true;
                    ESP_LOGI(HTTP_TAG, "IMU calibration loaded from NVS");
                    
                    // Migrate to SD card if available
                    Storage::ImuCalibrationData migrateData;
                    migrateData.valid = true;
                    memcpy(migrateData.matrix, state.imuCalibMatrix, sizeof(migrateData.matrix));
                    migrateData.timestamp = 0;  // Unknown timestamp for migrated data
                    
                    if (storageManager.saveImuCalibration(migrateData)) {
                        ESP_LOGI(HTTP_TAG, "Migrated IMU calibration from NVS to SD card");
                    }
                }
            }
            nvs_close(nvs);
        }
    }
    
    // ========== SD Card API Handlers ==========
    
    /**
     * @brief Get SD card status
     */
    static esp_err_t handleApiSdCardStatus(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return sendUnauthorized(req);
        
        auto& sdCard = Utils::FileSystemService::instance();
        
        cJSON* root = cJSON_CreateObject();
        cJSON_AddBoolToObject(root, "initialized", sdCard.isReady());
        cJSON_AddBoolToObject(root, "mounted", sdCard.isMounted());
        
        if (sdCard.isMounted()) {
            cJSON_AddStringToObject(root, "name", sdCard.getCardName());
            cJSON_AddNumberToObject(root, "total_mb", sdCard.getTotalBytes() / (1024 * 1024));
            cJSON_AddNumberToObject(root, "free_mb", sdCard.getFreeBytes() / (1024 * 1024));
            cJSON_AddNumberToObject(root, "used_mb", sdCard.getUsedBytes() / (1024 * 1024));
        }
        
        char* json = cJSON_PrintUnformatted(root);
        cJSON_Delete(root);
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
        free(json);
        return ESP_OK;
    }
    
    /**
     * @brief Format SD card (erase and create new filesystem)
     */
    static esp_err_t handleApiSdCardFormat(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return sendUnauthorized(req);
        
        auto& sdCard = Utils::FileSystemService::instance();
        
        if (!sdCard.isReady()) {
            httpd_resp_set_type(req, "application/json");
            httpd_resp_send(req, "{\"success\":false,\"error\":\"SD card not initialized\"}", HTTPD_RESP_USE_STRLEN);
            return ESP_OK;
        }
        
        ESP_LOGW(HTTP_TAG, "Formatting SD card (clearing all files)...");
        
        // Use clearAll() which actually deletes files recursively
        // (format() only formats if mount fails, which doesn't work for already mounted cards)
        bool success = sdCard.clearAll();
        
        // Clear all in-memory data since SD card is wiped
        if (success) {
            savedScenes_.clear();
            savedSprites_.clear();
            savedEquations_.clear();
            savedLedPresets_.clear();
            nextSceneId_ = 1;
            activeSceneId_ = -1;
            nextSpriteId_ = 1;
            nextEquationId_ = 1;
            nextLedPresetId_ = 1;
            activeLedPresetId_ = -1;
            
            // Also clear SPIFFS files to prevent re-migration
            if (!spiffs_initialized_) {
                initSpiffs();
            }
            remove(SCENE_INDEX_FILE_SPIFFS);
            remove(SPRITE_INDEX_FILE_SPIFFS);
            remove(EQUATION_INDEX_FILE_SPIFFS);
            SystemAPI::Utils::syncFilesystem();
        }
        
        cJSON* root = cJSON_CreateObject();
        cJSON_AddBoolToObject(root, "success", success);
        if (success) {
            cJSON_AddStringToObject(root, "message", "SD card cleared successfully. Use Setup to create folders.");
            cJSON_AddNumberToObject(root, "total_mb", sdCard.getTotalBytes() / (1024 * 1024));
            cJSON_AddNumberToObject(root, "free_mb", sdCard.getFreeBytes() / (1024 * 1024));
        } else {
            cJSON_AddStringToObject(root, "error", "Failed to clear SD card");
        }
        
        char* json = cJSON_PrintUnformatted(root);
        cJSON_Delete(root);
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
        free(json);
        return ESP_OK;
    }
    
    /**
     * @brief Recursively delete all files in a directory (SD card)
     */
    static int deleteAllFilesInDir(Utils::FileSystemService& fs, const char* dirPath) {
        int deleted = 0;
        std::vector<std::string> filesToDelete;
        std::vector<std::string> subDirs;
        
        fs.listDir(dirPath, [&](const Utils::FileInfo& info) {
            std::string fullPath = std::string(dirPath) + "/" + info.name;
            if (info.isDirectory) {
                // Skip . and .. if present
                if (strcmp(info.name, ".") != 0 && strcmp(info.name, "..") != 0) {
                    subDirs.push_back(fullPath);
                }
            } else {
                filesToDelete.push_back(fullPath);
            }
            return true;
        });
        
        // Delete all files in this directory
        for (const auto& file : filesToDelete) {
            ESP_LOGI(HTTP_TAG, "  Deleting: %s", file.c_str());
            if (fs.deleteFile(file.c_str())) {
                deleted++;
            }
        }
        
        // Recursively delete subdirectories
        for (const auto& subDir : subDirs) {
            deleted += deleteAllFilesInDir(fs, subDir.c_str());
        }
        
        return deleted;
    }
    
    /**
     * @brief Recursively delete all files in a SPIFFS directory
     */
    static int deleteAllSpiffsFiles(const char* dirPath) {
        int deleted = 0;
        DIR* dir = opendir(dirPath);
        if (!dir) return 0;
        
        std::vector<std::string> filesToDelete;
        std::vector<std::string> subDirs;
        struct dirent* entry;
        
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
            
            std::string fullPath = std::string(dirPath) + "/" + entry->d_name;
            struct stat st;
            if (stat(fullPath.c_str(), &st) == 0) {
                if (S_ISDIR(st.st_mode)) {
                    subDirs.push_back(fullPath);
                } else {
                    filesToDelete.push_back(fullPath);
                }
            }
        }
        closedir(dir);
        
        // Delete all files
        for (const auto& file : filesToDelete) {
            ESP_LOGI(HTTP_TAG, "  Deleting SPIFFS: %s", file.c_str());
            if (remove(file.c_str()) == 0) {
                deleted++;
            }
        }
        
        // Recursively delete subdirectories
        for (const auto& subDir : subDirs) {
            deleted += deleteAllSpiffsFiles(subDir.c_str());
            rmdir(subDir.c_str());  // Remove empty directory
        }
        
        return deleted;
    }
    
    /**
     * @brief Format SD card, create folder structure, and populate with default YAML scenes
     */
    static esp_err_t handleApiSdCardFormatInit(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return sendUnauthorized(req);
        
        auto& sdCard = Utils::FileSystemService::instance();
        
        if (!sdCard.isReady()) {
            httpd_resp_set_type(req, "application/json");
            httpd_resp_send(req, "{\"success\":false,\"error\":\"SD card not initialized\"}", HTTPD_RESP_USE_STRLEN);
            return ESP_OK;
        }
        
        ESP_LOGW(HTTP_TAG, "========================================");
        ESP_LOGW(HTTP_TAG, "  FULL SD CARD FORMAT & ERASE");
        ESP_LOGW(HTTP_TAG, "========================================");
        
        // Step 0: Clear ALL in-memory data
        ESP_LOGI(HTTP_TAG, "Step 0: Clearing ALL in-memory data...");
        savedScenes_.clear();
        savedSprites_.clear();
        savedEquations_.clear();
        savedLedPresets_.clear();
        nextSceneId_ = 1;
        activeSceneId_ = -1;
        nextSpriteId_ = 100;  // Start at 100 (0-99 reserved for built-in)
        nextEquationId_ = 1;
        nextLedPresetId_ = 1;
        activeLedPresetId_ = -1;
        
        // Step 0a: Recursively delete ALL files from SD card directories
        ESP_LOGI(HTTP_TAG, "Step 0a: WIPING ALL SD CARD FILES...");
        int totalDeleted = 0;
        const char* dirsToWipe[] = {
            "/Sprites", "/sprites",
            "/Scenes", "/scenes", 
            "/Equations", "/equations",
            "/Animations", "/animations",
            "/Configs", "/configs",
            "/Cache", "/cache",
            "/Calibration", "/calibration",
            "/LedPresets", "/ledpresets"
        };
        
        for (const char* dir : dirsToWipe) {
            ESP_LOGI(HTTP_TAG, "Wiping directory: %s", dir);
            int count = deleteAllFilesInDir(sdCard, dir);
            totalDeleted += count;
            vTaskDelay(pdMS_TO_TICKS(10));  // Yield to prevent watchdog
        }
        ESP_LOGW(HTTP_TAG, "Deleted %d files from SD card", totalDeleted);
        SystemAPI::Utils::syncFilesystem();
        vTaskDelay(pdMS_TO_TICKS(100));
        
        // Step 0b: WIPE ALL SPIFFS DATA
        ESP_LOGI(HTTP_TAG, "Step 0b: WIPING ALL SPIFFS DATA...");
        if (!spiffs_initialized_) {
            initSpiffs();
        }
        
        int spiffsDeleted = 0;
        const char* spiffsDirs[] = {
            "/spiffs/Sprites", "/spiffs/sprites",
            "/spiffs/Scenes", "/spiffs/scenes",
            "/spiffs/Equations", "/spiffs/equations",
            "/spiffs/LedPresets", "/spiffs/ledpresets"
        };
        
        for (const char* dir : spiffsDirs) {
            int count = deleteAllSpiffsFiles(dir);
            spiffsDeleted += count;
        }
        
        // Also delete root-level index files in SPIFFS
        const char* spiffsIndexFiles[] = {
            SCENE_INDEX_FILE_SPIFFS,
            SPRITE_INDEX_FILE_SPIFFS,
            EQUATION_INDEX_FILE_SPIFFS,
            "/spiffs/led_presets.json",
            "/spiffs/Sprites/index.json",
            "/spiffs/Sprites/index.dat",
            "/spiffs/Scenes/index.json",
            "/spiffs/Scenes/index.dat"
        };
        
        for (const char* file : spiffsIndexFiles) {
            if (remove(file) == 0) {
                ESP_LOGI(HTTP_TAG, "Deleted SPIFFS file: %s", file);
                spiffsDeleted++;
            }
        }
        
        ESP_LOGW(HTTP_TAG, "Deleted %d files from SPIFFS", spiffsDeleted);
        SystemAPI::Utils::syncFilesystem();
        vTaskDelay(pdMS_TO_TICKS(100));
        
        // Step 1: Format the SD card (full format)
        ESP_LOGI(HTTP_TAG, "Step 1: Formatting SD card (full format)...");
        bool formatSuccess = sdCard.format();
        if (!formatSuccess) {
            httpd_resp_set_type(req, "application/json");
            httpd_resp_send(req, "{\"success\":false,\"error\":\"Failed to format SD card\"}", HTTPD_RESP_USE_STRLEN);
            return ESP_OK;
        }
        vTaskDelay(pdMS_TO_TICKS(500));
        SystemAPI::Utils::syncFilesystem();
        
        // Step 2: Create folder structure
        ESP_LOGI(HTTP_TAG, "Step 2: Creating folder structure...");
        const char* folders[] = {
            "/Sprites",
            "/Equations", 
            "/Scenes",
            "/Animations",
            "/Configs",
            "/Cache",
            "/Calibration",
            "/LedPresets"
        };
        
        int foldersCreated = 0;
        for (const char* folder : folders) {
            ESP_LOGI(HTTP_TAG, "Creating folder: %s", folder);
            if (sdCard.createDir(folder)) {
                foldersCreated++;
            } else {
                ESP_LOGW(HTTP_TAG, "Failed to create folder: %s", folder);
            }
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        SystemAPI::Utils::syncFilesystem();
        vTaskDelay(pdMS_TO_TICKS(200));
        
        // Also create lowercase versions of directories (some code uses lowercase)
        ESP_LOGI(HTTP_TAG, "Step 2b: Creating lowercase directory variants...");
        const char* lowercaseFolders[] = {
            "/sprites", "/scenes", "/equations", "/animations", "/configs"
        };
        for (const char* folder : lowercaseFolders) {
            sdCard.createDir(folder);
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        
        // Step 3: Create empty index files in ALL locations to prevent recovery/migration
        ESP_LOGI(HTTP_TAG, "Step 3: Creating empty index files (blocking recovery)...");
        
        // Empty scene index
        const char* emptySceneIndex = R"({
  "nextId": 1,
  "activeId": -1,
  "storage": "sdcard",
  "scenes": []
})";
        sdCard.writeFile("/Scenes/index.json", emptySceneIndex, strlen(emptySceneIndex));
        sdCard.writeFile("/scenes/index.json", emptySceneIndex, strlen(emptySceneIndex));
        ESP_LOGI(HTTP_TAG, "Created scene index files");
        
        // Empty sprite index - create in ALL possible locations
        const char* emptySpriteIndex = R"({
  "version": 1,
  "nextId": 100,
  "sprites": []
})";
        sdCard.writeFile("/Sprites/index.json", emptySpriteIndex, strlen(emptySpriteIndex));
        sdCard.writeFile("/Sprites/index.dat", emptySpriteIndex, strlen(emptySpriteIndex));
        sdCard.writeFile("/sprites/index.json", emptySpriteIndex, strlen(emptySpriteIndex));
        sdCard.writeFile("/sprites/index.dat", emptySpriteIndex, strlen(emptySpriteIndex));
        ESP_LOGI(HTTP_TAG, "Created sprite index files (4 locations)");
        
        // Empty LED presets index
        const char* emptyLedPresetsIndex = R"({
  "nextId": 1,
  "activeId": -1,
  "storage": "sdcard",
  "presets": []
})";
        sdCard.writeFile("/LedPresets/index.json", emptyLedPresetsIndex, strlen(emptyLedPresetsIndex));
        sdCard.writeFile("/ledpresets/index.json", emptyLedPresetsIndex, strlen(emptyLedPresetsIndex));
        ESP_LOGI(HTTP_TAG, "Created LED preset index files");
        
        // Empty equations index
        const char* emptyEquationsIndex = R"({
  "nextId": 1,
  "equations": []
})";
        sdCard.writeFile("/Equations/index.json", emptyEquationsIndex, strlen(emptyEquationsIndex));
        sdCard.writeFile("/equations/index.json", emptyEquationsIndex, strlen(emptyEquationsIndex));
        ESP_LOGI(HTTP_TAG, "Created equation index files");
        
        SystemAPI::Utils::syncFilesystem();
        vTaskDelay(pdMS_TO_TICKS(200));
        
        ESP_LOGW(HTTP_TAG, "========================================");
        ESP_LOGW(HTTP_TAG, "  FORMAT COMPLETE - ALL DATA WIPED");
        ESP_LOGW(HTTP_TAG, "  SD: %d files deleted", totalDeleted);
        ESP_LOGW(HTTP_TAG, "  SPIFFS: %d files deleted", spiffsDeleted);
        ESP_LOGW(HTTP_TAG, "========================================");
        
        // Build response
        cJSON* root = cJSON_CreateObject();
        cJSON_AddBoolToObject(root, "success", true);
        cJSON_AddStringToObject(root, "message", "SD card formatted and ALL data wiped. Use Setup Defaults to add default scenes.");
        cJSON_AddNumberToObject(root, "folders_created", foldersCreated);
        cJSON_AddNumberToObject(root, "files_deleted_sd", totalDeleted);
        cJSON_AddNumberToObject(root, "files_deleted_spiffs", spiffsDeleted);
        cJSON_AddNumberToObject(root, "total_mb", sdCard.getTotalBytes() / (1024 * 1024));
        cJSON_AddNumberToObject(root, "free_mb", sdCard.getFreeBytes() / (1024 * 1024));
        
        char* json = cJSON_PrintUnformatted(root);
        cJSON_Delete(root);
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
        free(json);
        
        ESP_LOGI(HTTP_TAG, "SD card format & init complete: %d folders created", foldersCreated);
        return ESP_OK;
    }
    
    /**
     * @brief Setup default system configuration (sprite, scene, LED config)
     * Creates default files without formatting the SD card
     */
    static esp_err_t handleApiSdCardSetupDefaults(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return sendUnauthorized(req);
        
        auto& sdCard = Utils::FileSystemService::instance();
        
        if (!sdCard.isReady() || !sdCard.isMounted()) {
            httpd_resp_set_type(req, "application/json");
            httpd_resp_send(req, "{\"success\":false,\"error\":\"SD card not initialized\"}", HTTPD_RESP_USE_STRLEN);
            return ESP_OK;
        }
        
        ESP_LOGI(HTTP_TAG, "Setting up default configuration...");
        
        // Clear in-memory data first
        savedScenes_.clear();
        savedSprites_.clear();
        savedEquations_.clear();
        savedLedPresets_.clear();
        nextSceneId_ = 1;
        activeSceneId_ = -1;
        nextSpriteId_ = 1;
        nextEquationId_ = 1;
        nextLedPresetId_ = 1;
        activeLedPresetId_ = -1;
        
        // Clear SPIFFS data to prevent re-migration
        // First ensure SPIFFS is mounted
        if (!spiffs_initialized_) {
            initSpiffs();
        }
        // Now delete the files
        if (remove(SCENE_INDEX_FILE_SPIFFS) == 0) {
            ESP_LOGI(HTTP_TAG, "Deleted SPIFFS scene index: %s", SCENE_INDEX_FILE_SPIFFS);
        }
        if (remove(SPRITE_INDEX_FILE_SPIFFS) == 0) {
            ESP_LOGI(HTTP_TAG, "Deleted SPIFFS sprite index: %s", SPRITE_INDEX_FILE_SPIFFS);
        }
        if (remove(EQUATION_INDEX_FILE_SPIFFS) == 0) {
            ESP_LOGI(HTTP_TAG, "Deleted SPIFFS equation index: %s", EQUATION_INDEX_FILE_SPIFFS);
        }
        SystemAPI::Utils::syncFilesystem();
        
        int filesCreated = 0;
        
        // Ensure directories exist
        sdCard.createDir("/Sprites");
        sdCard.createDir("/Scenes");
        sdCard.createDir("/Configs");
        
        // Step 1: Create default eye sprite SVG
        ESP_LOGI(HTTP_TAG, "Creating default eye sprite...");
        const char* defaultEyeSvg = R"(<svg width="445" height="308" viewBox="0 0 445 308" fill="none" xmlns="http://www.w3.org/2000/svg">
<circle cx="216" cy="114" r="39.5" stroke="white"/>
<path d="M384.5 130.5L347.5 77.5L346 76L343.5 76.5L342 78V81L343.5 88L345.5 99.5V112L345 127L342.5 140L338.5 156L332 171L322.5 188.5L311.5 203.5L297.5 216.5L285.5 225L284 230L285 235.5L289 240L302 242L320 245L339 251L355 257.5L372 266.5L404.5 287.5L433 305L439.5 307.5H442.5L444 305.5V290L441.5 272L434 240L419.5 198.5L405 166L384.5 130.5Z" stroke="white"/>
<path d="M238 3L221.5 0.5H161L142 1.5L106 4.5L89 6L72.5 10.5L58.5 16L48.5 21L35.5 30.5L27 39L20 47.5L14 57.5L7 75L1 98.5L0.5 109V116L2 122L5 126L8.5 128.5L21.5 132.5L38 137.5L58.5 144.5L75 151L90 159L101.5 167L117 177.5L131 189L139.5 197.5L149 205.5L158.5 212L170.5 218L186 223.5L201 226.5L216 227.5L230 226.5L242 223.5L258.5 218.5L278.5 208.5L292 198.5L302 188.5L312 176L319 163.5L323 153.5L327 138.5L328.5 122V106L326.5 89L321.5 72.5L316.5 61L310.5 51L303.5 42.5L293.5 31.5L281 22.5L267.5 14.5L255.5 9L238 3Z" stroke="white"/>
</svg>)";
        
        if (sdCard.writeFile("/Sprites/default_eye.svg", defaultEyeSvg, strlen(defaultEyeSvg))) {
            filesCreated++;
            ESP_LOGI(HTTP_TAG, "Created default_eye.svg");
        }
        
        // Step 2: Create sprite index with default sprite
        const char* spriteIndex = R"({
  "version": 1,
  "sprites": [
    {
      "id": 0,
      "name": "Default Eye",
      "filename": "default_eye.svg",
      "type": "vector",
      "antialiased": true,
      "width": 445,
      "height": 308
    }
  ],
  "nextId": 1
})";
        
        // Write to both locations for compatibility
        if (sdCard.writeFile("/Sprites/index.dat", spriteIndex, strlen(spriteIndex))) {
            filesCreated++;
            ESP_LOGI(HTTP_TAG, "Created Sprites/index.dat");
        }
        sdCard.createDir("/sprites");
        if (sdCard.writeFile("/sprites/index.dat", spriteIndex, strlen(spriteIndex))) {
            ESP_LOGI(HTTP_TAG, "Created sprites/index.dat");
        }
        
        // Step 3: Create default scene YAML with static_mirrored animation (new v2.0 format)
        const char* defaultSceneYaml = R"(# ============================================
# Scene Configuration File - v2.0
# ============================================
# This file uses YAML-driven UI configuration.
# The web UI auto-generates controls based on field types.
# ============================================

Global:
  name: "Default Scene"
  id: 1
  description: "Default eye display with mirrored left/right eyes"
  version: "2.0"
  author: "System"

Display:
  enabled: true
  animation_type: "static_mirrored"
  main_sprite_id: 0
  use_default_sprite: true
  antialiasing: true
  position:
    x: 64
    y: 16
  scale: 1.0
  rotation: 0
  sensitivity: 1.0
  mirror: true
  background:
    r: 0
    g: 0
    b: 0

LEDS:
  enabled: true
  brightness: 80
  animation: "solid"
  color:
    r: 255
    g: 0
    b: 255
  strips:
    left_fin:
      enabled: true
      length: 15
    right_fin:
      enabled: true
      length: 15
    tongue:
      enabled: true
      length: 10
    scales:
      enabled: true
      length: 20

Audio:
  enabled: false
  source: "mic"
  sensitivity: 1.0
  frequency_band: "all"
)";
        
        if (sdCard.writeFile("/Scenes/default_scene.yaml", defaultSceneYaml, strlen(defaultSceneYaml))) {
            filesCreated++;
            ESP_LOGI(HTTP_TAG, "Created default_scene.yaml");
        }
        
        // Step 4: Create LED configuration
        const char* ledConfig = R"({
  "enabled": true,
  "brightness": 80,
  "defaultMode": "solid",
  "defaultColor": {
    "r": 255,
    "g": 0,
    "b": 255
  },
  "stripLength": 60
})";
        
        if (sdCard.writeFile("/Configs/leds.json", ledConfig, strlen(ledConfig))) {
            filesCreated++;
            ESP_LOGI(HTTP_TAG, "Created leds.json");
        }
        
        // Step 5: Create scene index referencing the default scene
        const char* sceneIndex = R"({
  "nextId": 2,
  "activeId": 1,
  "storage": "sdcard",
  "scenes": [
    {
      "id": 1,
      "name": "Default Scene",
      "type": 0,
      "active": true,
      "displayEnabled": true,
      "ledsEnabled": true,
      "effectsOnly": false,
      "order": 0,
      "animType": "static_mirrored",
      "spriteId": 0,
      "mirrorSprite": true,
      "shaderAA": true,
      "shaderInvert": false,
      "shaderColorMode": "none",
      "shaderColor": "#ffffff",
      "ledR": 255,
      "ledG": 0,
      "ledB": 255,
      "ledBrightness": 80
    }
  ]
})";
        
        if (sdCard.writeFile("/Scenes/index.json", sceneIndex, strlen(sceneIndex))) {
            filesCreated++;
            ESP_LOGI(HTTP_TAG, "Created scene index.json");
        }
        
        SystemAPI::Utils::syncFilesystem();
        
        // Reload sprites from storage to pick up the new default
        savedSprites_.clear();
        nextSpriteId_ = 1;
        // Don't call loadSpritesFromStorage() - it might recover old files
        // Instead, manually add the default sprite we just created
        SavedSprite defaultSprite;
        defaultSprite.id = 0;
        defaultSprite.name = "Default Eye";
        defaultSprite.width = 445;
        defaultSprite.height = 308;
        defaultSprite.scale = 100;
        defaultSprite.uploadedToGpu = false;
        savedSprites_.push_back(defaultSprite);
        nextSpriteId_ = 1;
        
        // Reload scenes from storage to pick up the new default
        savedScenes_.clear();
        nextSceneId_ = 1;
        activeSceneId_ = -1;
        loadScenesFromStorage();
        
        // Create LED presets folder and defaults
        sdCard.createDir("/LedPresets");
        savedLedPresets_.clear();
        nextLedPresetId_ = 1;
        activeLedPresetId_ = -1;
        createDefaultLedPresets();
        
        // Build response
        cJSON* root = cJSON_CreateObject();
        cJSON_AddBoolToObject(root, "success", true);
        cJSON_AddStringToObject(root, "message", "Default configuration created successfully");
        cJSON_AddNumberToObject(root, "files_created", filesCreated);
        
        char* json = cJSON_PrintUnformatted(root);
        cJSON_Delete(root);
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
        free(json);
        
        ESP_LOGI(HTTP_TAG, "Default setup complete: %d files created", filesCreated);
        return ESP_OK;
    }
    
    /**
     * @brief Setup SD card - clear everything and create folder structure
     */
    static esp_err_t handleApiSdCardSetup(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return sendUnauthorized(req);
        
        auto& sdCard = Utils::FileSystemService::instance();
        
        if (!sdCard.isReady() || !sdCard.isMounted()) {
            httpd_resp_set_type(req, "application/json");
            httpd_resp_send(req, "{\"success\":false,\"error\":\"SD card not initialized\"}", HTTPD_RESP_USE_STRLEN);
            return ESP_OK;
        }
        
        ESP_LOGW(HTTP_TAG, "Setting up SD card (clear all + create folders)...");
        
        // Step 1: Clear all existing files and folders recursively
        ESP_LOGI(HTTP_TAG, "Clearing all existing files and folders...");
        bool clearSuccess = sdCard.clearAll();
        if (!clearSuccess) {
            ESP_LOGW(HTTP_TAG, "Warning: clearAll returned false, continuing anyway...");
        }
        
        // Give filesystem time to settle after clearing
        vTaskDelay(pdMS_TO_TICKS(500));
        SystemAPI::Utils::syncFilesystem();
        vTaskDelay(pdMS_TO_TICKS(200));
        
        // Step 2: Create folder structure
        const char* folders[] = {
            "/Sprites",
            "/Equations", 
            "/Scenes",
            "/Animations",
            "/Configs",
            "/Cache",
            "/Calibration",
            "/LedPresets"
        };
        
        int foldersCreated = 0;
        int foldersFailed = 0;
        
        for (const char* folder : folders) {
            ESP_LOGI(HTTP_TAG, "Creating folder: %s", folder);
            if (sdCard.createDir(folder)) {
                foldersCreated++;
            } else {
                ESP_LOGW(HTTP_TAG, "Failed to create folder: %s", folder);
                foldersFailed++;
            }
            vTaskDelay(pdMS_TO_TICKS(100));  // Small delay between folder creations
        }
        
        // Sync filesystem
        SystemAPI::Utils::syncFilesystem();
        
        // Step 3: Create empty index files and clear in-memory data
        ESP_LOGI(HTTP_TAG, "Creating empty index files...");
        
        // Empty scene index
        const char* emptySceneIndex = R"({
  "nextId": 1,
  "activeId": -1,
  "storage": "sdcard",
  "scenes": []
})";
        sdCard.writeFile("/Scenes/index.json", emptySceneIndex, strlen(emptySceneIndex));
        
        // Empty sprite index
        const char* emptySpriteIndex = R"({
  "version": 1,
  "nextId": 1,
  "sprites": []
})";
        sdCard.writeFile("/Sprites/index.json", emptySpriteIndex, strlen(emptySpriteIndex));
        
        // Empty LED presets index
        const char* emptyLedPresetsIndex = R"({
  "nextId": 1,
  "activeId": -1,
  "storage": "sdcard",
  "presets": []
})";
        sdCard.writeFile("/LedPresets/index.json", emptyLedPresetsIndex, strlen(emptyLedPresetsIndex));
        
        // Clear in-memory data
        savedScenes_.clear();
        savedSprites_.clear();
        savedEquations_.clear();
        savedLedPresets_.clear();
        nextSceneId_ = 1;
        activeSceneId_ = -1;
        nextSpriteId_ = 1;
        nextEquationId_ = 1;
        nextLedPresetId_ = 1;
        activeLedPresetId_ = -1;
        
        SystemAPI::Utils::syncFilesystem();
        
        // Build response
        cJSON* root = cJSON_CreateObject();
        cJSON_AddBoolToObject(root, "success", foldersFailed == 0);
        cJSON_AddStringToObject(root, "message", "SD card setup complete (empty). Use Setup Defaults to add default content.");
        cJSON_AddNumberToObject(root, "folders_created", foldersCreated);
        cJSON_AddNumberToObject(root, "folders_failed", foldersFailed);
        cJSON_AddNumberToObject(root, "total_mb", sdCard.getTotalBytes() / (1024 * 1024));
        cJSON_AddNumberToObject(root, "free_mb", sdCard.getFreeBytes() / (1024 * 1024));
        
        char* json = cJSON_PrintUnformatted(root);
        cJSON_Delete(root);
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
        free(json);
        return ESP_OK;
    }
    
    /**
     * @brief Clear all files from SD card (keep filesystem)
     */
    static esp_err_t handleApiSdCardClear(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return sendUnauthorized(req);
        
        auto& sdCard = Utils::FileSystemService::instance();
        
        if (!sdCard.isMounted()) {
            httpd_resp_set_type(req, "application/json");
            httpd_resp_send(req, "{\"success\":false,\"error\":\"SD card not mounted\"}", HTTPD_RESP_USE_STRLEN);
            return ESP_OK;
        }
        
        ESP_LOGW(HTTP_TAG, "Clearing all files from SD card...");
        
        bool success = sdCard.clearAll();
        
        cJSON* root = cJSON_CreateObject();
        cJSON_AddBoolToObject(root, "success", success);
        if (success) {
            cJSON_AddStringToObject(root, "message", "All files cleared");
            cJSON_AddNumberToObject(root, "free_mb", sdCard.getFreeBytes() / (1024 * 1024));
        } else {
            cJSON_AddStringToObject(root, "error", "Failed to clear some files");
        }
        
        char* json = cJSON_PrintUnformatted(root);
        cJSON_Delete(root);
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
        free(json);
        return ESP_OK;
    }
    
    /**
     * @brief List directory contents
     */
    static esp_err_t handleApiSdCardList(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return sendUnauthorized(req);
        
        auto& sdCard = Utils::FileSystemService::instance();
        
        ESP_LOGI(HTTP_TAG, "SD Card list: mounted=%d, ready=%d", sdCard.isMounted(), sdCard.isReady());
        
        if (!sdCard.isMounted()) {
            httpd_resp_set_type(req, "application/json");
            httpd_resp_send(req, "{\"success\":false,\"error\":\"SD card not mounted\"}", HTTPD_RESP_USE_STRLEN);
            return ESP_OK;
        }
        
        // Get path from query string
        char path[128] = "/";
        char query[256];
        if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
            char value[128];
            if (httpd_query_key_value(query, "path", value, sizeof(value)) == ESP_OK) {
                // URL decode the path
                char* src = value;
                char* dst = path;
                while (*src && (dst - path) < 126) {
                    if (*src == '%' && src[1] && src[2]) {
                        char hex[3] = {src[1], src[2], 0};
                        *dst++ = (char)strtol(hex, nullptr, 16);
                        src += 3;
                    } else if (*src == '+') {
                        *dst++ = ' ';
                        src++;
                    } else {
                        *dst++ = *src++;
                    }
                }
                *dst = '\0';
            }
        }
        
        ESP_LOGI(HTTP_TAG, "Listing directory: '%s'", path);
        
        cJSON* root = cJSON_CreateObject();
        cJSON_AddBoolToObject(root, "success", true);
        cJSON_AddStringToObject(root, "path", path);
        
        cJSON* files = cJSON_AddArrayToObject(root, "files");
        
        int count = sdCard.listDir(path, [&](const Utils::FileInfo& info) -> bool {
            cJSON* item = cJSON_CreateObject();
            cJSON_AddStringToObject(item, "name", info.name);
            cJSON_AddStringToObject(item, "path", info.path);
            cJSON_AddBoolToObject(item, "isDir", info.isDirectory);
            cJSON_AddNumberToObject(item, "size", info.size);
            cJSON_AddItemToArray(files, item);
            return true;
        });
        
        ESP_LOGI(HTTP_TAG, "Directory '%s' has %d entries", path, count);
        
        char* json = cJSON_PrintUnformatted(root);
        cJSON_Delete(root);
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
        free(json);
        return ESP_OK;
    }
    
    /**
     * @brief Get hex dump of file
     */
    static esp_err_t handleApiSdCardHex(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return sendUnauthorized(req);
        
        auto& sdCard = Utils::FileSystemService::instance();
        
        if (!sdCard.isMounted()) {
            httpd_resp_send(req, "SD card not mounted", HTTPD_RESP_USE_STRLEN);
            return ESP_OK;
        }
        
        // Get path from query
        char path[256] = "";
        char query[512];
        if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
            char value[256];
            if (httpd_query_key_value(query, "path", value, sizeof(value)) == ESP_OK) {
                strncpy(path, value, sizeof(path) - 1);
                // URL decode
                for (char* p = path; *p; p++) {
                    if (*p == '%' && p[1] && p[2]) {
                        char hex[3] = {p[1], p[2], 0};
                        *p = (char)strtol(hex, nullptr, 16);
                        memmove(p+1, p+3, strlen(p+3)+1);
                    }
                }
            }
        }
        
        if (path[0] == '\0') {
            httpd_resp_send(req, "Missing path parameter", HTTPD_RESP_USE_STRLEN);
            return ESP_OK;
        }
        
        // Build full path
        char fullPath[280];
        if (path[0] == '/') {
            snprintf(fullPath, sizeof(fullPath), "/sdcard%s", path);
        } else {
            snprintf(fullPath, sizeof(fullPath), "/sdcard/%s", path);
        }
        
        FILE* f = fopen(fullPath, "rb");
        if (!f) {
            char errMsg[320];
            snprintf(errMsg, sizeof(errMsg), "Failed to open: %s (errno=%d)", fullPath, errno);
            httpd_resp_send(req, errMsg, HTTPD_RESP_USE_STRLEN);
            return ESP_OK;
        }
        
        // Read up to 4KB for hex preview
        const int maxBytes = 4096;
        uint8_t* buffer = (uint8_t*)malloc(maxBytes);
        if (!buffer) {
            fclose(f);
            httpd_resp_send(req, "Out of memory", HTTPD_RESP_USE_STRLEN);
            return ESP_OK;
        }
        
        size_t bytesRead = fread(buffer, 1, maxBytes, f);
        fclose(f);
        
        // Build hex dump output
        std::string hexOutput;
        hexOutput.reserve(bytesRead * 4 + 256);
        
        char line[128];
        for (size_t i = 0; i < bytesRead; i += 16) {
            // Offset
            snprintf(line, sizeof(line), "%08zx  ", i);
            hexOutput += line;
            
            // Hex bytes
            for (size_t j = 0; j < 16; j++) {
                if (i + j < bytesRead) {
                    snprintf(line, sizeof(line), "%02x ", buffer[i + j]);
                    hexOutput += line;
                } else {
                    hexOutput += "   ";
                }
                if (j == 7) hexOutput += " ";
            }
            
            // ASCII
            hexOutput += " |";
            for (size_t j = 0; j < 16 && i + j < bytesRead; j++) {
                char c = buffer[i + j];
                hexOutput += (c >= 32 && c < 127) ? c : '.';
            }
            hexOutput += "|\n";
        }
        
        free(buffer);
        
        httpd_resp_set_type(req, "text/plain");
        httpd_resp_send(req, hexOutput.c_str(), HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    /**
     * @brief Read file as text
     */
    static esp_err_t handleApiSdCardRead(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return sendUnauthorized(req);
        
        auto& sdCard = Utils::FileSystemService::instance();
        
        if (!sdCard.isMounted()) {
            httpd_resp_send(req, "SD card not mounted", HTTPD_RESP_USE_STRLEN);
            return ESP_OK;
        }
        
        // Get path from query
        char path[256] = "";
        char query[512];
        if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
            char value[256];
            if (httpd_query_key_value(query, "path", value, sizeof(value)) == ESP_OK) {
                strncpy(path, value, sizeof(path) - 1);
                // URL decode
                for (char* p = path; *p; p++) {
                    if (*p == '%' && p[1] && p[2]) {
                        char hex[3] = {p[1], p[2], 0};
                        *p = (char)strtol(hex, nullptr, 16);
                        memmove(p+1, p+3, strlen(p+3)+1);
                    }
                }
            }
        }
        
        if (path[0] == '\0') {
            httpd_resp_send(req, "Missing path parameter", HTTPD_RESP_USE_STRLEN);
            return ESP_OK;
        }
        
        // Read via FileSystemService
        char* content = nullptr;
        size_t contentSize = 0;
        if (!sdCard.readFile(path, &content, &contentSize) || !content) {
            httpd_resp_send(req, "Failed to read file", HTTPD_RESP_USE_STRLEN);
            return ESP_OK;
        }
        
        httpd_resp_set_type(req, "text/plain");
        httpd_resp_send(req, content, contentSize);
        free(content);
        return ESP_OK;
    }
    
    /**
     * @brief Download file
     */
    static esp_err_t handleApiSdCardDownload(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return sendUnauthorized(req);
        
        auto& sdCard = Utils::FileSystemService::instance();
        
        if (!sdCard.isMounted()) {
            httpd_resp_send(req, "SD card not mounted", HTTPD_RESP_USE_STRLEN);
            return ESP_OK;
        }
        
        // Get path from query
        char path[256] = "";
        char query[512];
        if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
            char value[256];
            if (httpd_query_key_value(query, "path", value, sizeof(value)) == ESP_OK) {
                strncpy(path, value, sizeof(path) - 1);
                // URL decode
                for (char* p = path; *p; p++) {
                    if (*p == '%' && p[1] && p[2]) {
                        char hex[3] = {p[1], p[2], 0};
                        *p = (char)strtol(hex, nullptr, 16);
                        memmove(p+1, p+3, strlen(p+3)+1);
                    }
                }
            }
        }
        
        if (path[0] == '\0') {
            httpd_resp_send(req, "Missing path parameter", HTTPD_RESP_USE_STRLEN);
            return ESP_OK;
        }
        
        // Read via FileSystemService
        char* content = nullptr;
        size_t contentSize = 0;
        if (!sdCard.readFile(path, &content, &contentSize) || !content) {
            httpd_resp_send(req, "Failed to read file", HTTPD_RESP_USE_STRLEN);
            return ESP_OK;
        }
        
        // Get filename from path
        const char* filename = strrchr(path, '/');
        filename = filename ? filename + 1 : path;
        
        // Set download headers - truncate filename if too long
        char safeFilename[64];
        strncpy(safeFilename, filename, sizeof(safeFilename) - 1);
        safeFilename[sizeof(safeFilename) - 1] = '\0';
        
        char header[128];
        snprintf(header, sizeof(header), "attachment; filename=\"%.60s\"", safeFilename);
        httpd_resp_set_hdr(req, "Content-Disposition", header);
        httpd_resp_set_type(req, "application/octet-stream");
        httpd_resp_send(req, content, contentSize);
        free(content);
        return ESP_OK;
    }
    
    /**
     * @brief Delete file from SD card
     */
    static esp_err_t handleApiSdCardDelete(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return sendUnauthorized(req);
        
        auto& sdCard = Utils::FileSystemService::instance();
        
        if (!sdCard.isMounted()) {
            httpd_resp_set_type(req, "application/json");
            httpd_resp_send(req, "{\"success\":false,\"error\":\"SD card not mounted\"}", HTTPD_RESP_USE_STRLEN);
            return ESP_OK;
        }
        
        // Read POST body
        char body[512];
        int bodyLen = httpd_req_recv(req, body, sizeof(body) - 1);
        if (bodyLen <= 0) {
            httpd_resp_set_type(req, "application/json");
            httpd_resp_send(req, "{\"success\":false,\"error\":\"No data\"}", HTTPD_RESP_USE_STRLEN);
            return ESP_OK;
        }
        body[bodyLen] = '\0';
        
        cJSON* json = cJSON_Parse(body);
        if (!json) {
            httpd_resp_set_type(req, "application/json");
            httpd_resp_send(req, "{\"success\":false,\"error\":\"Invalid JSON\"}", HTTPD_RESP_USE_STRLEN);
            return ESP_OK;
        }
        
        cJSON* pathItem = cJSON_GetObjectItem(json, "path");
        if (!pathItem || !cJSON_IsString(pathItem)) {
            cJSON_Delete(json);
            httpd_resp_set_type(req, "application/json");
            httpd_resp_send(req, "{\"success\":false,\"error\":\"Missing path\"}", HTTPD_RESP_USE_STRLEN);
            return ESP_OK;
        }
        
        const char* path = pathItem->valuestring;
        ESP_LOGW(HTTP_TAG, "Deleting file: %s", path);
        
        bool success = sdCard.deleteFile(path);
        
        cJSON_Delete(json);
        
        cJSON* response = cJSON_CreateObject();
        cJSON_AddBoolToObject(response, "success", success);
        if (!success) {
            cJSON_AddStringToObject(response, "error", "Failed to delete file");
        }
        
        char* respJson = cJSON_PrintUnformatted(response);
        cJSON_Delete(response);
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, respJson, HTTPD_RESP_USE_STRLEN);
        free(respJson);
        return ESP_OK;
    }
    
    /**
     * @brief Handle SD card browser page
     */
    static esp_err_t handlePageSdCard(httpd_req_t* req) {
        if (requiresAuthRedirect(req)) return sendUnauthorized(req);
        
        httpd_resp_set_type(req, "text/html");
        httpd_resp_send(req, Content::getPageSdCard(), HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    // ========== Captive Portal Handlers ==========

    /**
     * @brief Handle captive portal detection endpoints
     * 
     * Different OS have different captive portal detection:
     * - Android: Expects 204 from connectivity check, anything else = captive
     * - iOS: Expects specific "Success" response, anything else = captive
     * - Windows: Expects specific content from ncsi.txt
     * 
     * We return HTML that triggers the captive portal popup
     */
    static esp_err_t handleRedirect(httpd_req_t* req) {
        const char* uri = req->uri;
        
        // Android connectivity checks - return non-204 to trigger captive portal
        if (strstr(uri, "generate_204") || strstr(uri, "gen_204") || 
            strstr(uri, "connectivitycheck")) {
            // Return a redirect instead of 204 to trigger Android captive portal
            httpd_resp_set_status(req, "302 Found");
            httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
            httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
            httpd_resp_send(req, nullptr, 0);
            return ESP_OK;
        }
        
        // iOS/Apple captive portal - return non-Success to trigger popup
        if (strstr(uri, "hotspot-detect") || strstr(uri, "captive.apple") || 
            strstr(uri, "library/test/success")) {
            // Return HTML that will show in iOS captive portal popup
            static const char* ios_response = 
                "<!DOCTYPE html><html><head>"
                "<meta http-equiv=\"refresh\" content=\"0;url=http://192.168.4.1/\">"
                "</head><body><a href=\"http://192.168.4.1/\">Click here</a></body></html>";
            httpd_resp_set_type(req, "text/html");
            httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
            httpd_resp_send(req, ios_response, HTTPD_RESP_USE_STRLEN);
            return ESP_OK;
        }
        
        // Windows NCSI check
        if (strstr(uri, "ncsi.txt") || strstr(uri, "connecttest") || strstr(uri, "msft")) {
            httpd_resp_set_status(req, "302 Found");
            httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
            httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
            httpd_resp_send(req, nullptr, 0);
            return ESP_OK;
        }
        
        // Default: redirect to portal
        httpd_resp_set_status(req, "302 Found");
        httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
        httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
        httpd_resp_send(req, nullptr, 0);
        return ESP_OK;
    }
    
    static esp_err_t handleCatchAll(httpd_req_t* req) {
        char host_header[MAX_HOST_HEADER_LENGTH] = {0};
        char user_agent[128] = {0};
        httpd_req_get_hdr_value_str(req, "Host", host_header, sizeof(host_header));
        httpd_req_get_hdr_value_str(req, "User-Agent", user_agent, sizeof(user_agent));
        const char* uri = req->uri;
        
        ESP_LOGI(HTTP_TAG, "Catch-all: Host=%s URI=%s UA=%s", host_header, uri, user_agent);
        
        // Check for captive portal detection user agents
        bool isCaptiveCheck = (
            strstr(user_agent, "CaptiveNetworkSupport") ||  // iOS
            strstr(user_agent, "Microsoft NCSI") ||          // Windows
            strstr(user_agent, "Dalvik") ||                  // Android apps checking connectivity
            strstr(user_agent, "captive") ||
            strstr(user_agent, "NetWorkProbe")               // Various Android OEMs
        );
        
        // Check for captive portal URIs we might have missed
        bool isCaptiveUri = (
            strstr(uri, "generate") ||
            strstr(uri, "connectivity") ||
            strstr(uri, "hotspot") ||
            strstr(uri, "captive") ||
            strstr(uri, "success") ||
            strstr(uri, "ncsi") ||
            strstr(uri, "connect")
        );
        
        // Check if this is a request to an external domain (DNS hijacked)
        bool isExternalHost = (
            strlen(host_header) > 0 && 
            strstr(host_header, "192.168.4.1") == nullptr &&
            strstr(host_header, "lucidius") == nullptr
        );
        
        // If any captive portal indicators, redirect
        if (isCaptiveCheck || isCaptiveUri || isExternalHost) {
            // Request to external domain - redirect to captive portal
            static const char* captive_response = 
                "<!DOCTYPE html><html><head>"
                "<meta http-equiv=\"refresh\" content=\"0;url=http://192.168.4.1/\">"
                "<title>Redirecting...</title>"
                "</head><body>"
                "<h1>Redirecting to Lucidius...</h1>"
                "<p><a href=\"http://192.168.4.1/\">Click here if not redirected</a></p>"
                "</body></html>";
            httpd_resp_set_type(req, "text/html");
            httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
            httpd_resp_send(req, captive_response, HTTPD_RESP_USE_STRLEN);
            return ESP_OK;
        }
        
        // For non-GET requests, respond with redirect
        if (req->method != HTTP_GET) {
            httpd_resp_set_status(req, "302 Found");
            httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
            httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
            httpd_resp_send(req, nullptr, 0);
            return ESP_OK;
        }
        
        // Serve Basic page for any unmatched GET requests
        httpd_resp_set_type(req, "text/html");
        httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
        httpd_resp_set_hdr(req, "Pragma", "no-cache");
        httpd_resp_set_hdr(req, "Expires", "0");
        httpd_resp_send(req, Content::PAGE_BASIC, HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    // ========== Command Processing ==========
    
    void processCommand(CommandType type, cJSON* params) {
        // Invoke callback if set
        if (command_callback_) {
            command_callback_(type, params);
        }
        
        // Default handling
        switch (type) {
            case CommandType::SET_BRIGHTNESS: {
                cJSON* val = cJSON_GetObjectItem(params, "value");
                if (val) SYNC_STATE.setBrightness(val->valueint);
                break;
            }
            
            case CommandType::SET_WIFI_CREDENTIALS: {
                cJSON* ssid = cJSON_GetObjectItem(params, "ssid");
                cJSON* password = cJSON_GetObjectItem(params, "password");
                if (ssid && password && ssid->valuestring && password->valuestring) {
                    ESP_LOGI(HTTP_TAG, "WiFi credentials update: %s", ssid->valuestring);
                    
                    auto& security = arcos::security::SecurityDriver::instance();
                    if (security.setCustomCredentials(ssid->valuestring, password->valuestring)) {
                        ESP_LOGI(HTTP_TAG, "Custom credentials saved successfully");
                        vTaskDelay(pdMS_TO_TICKS(1000));
                        esp_restart();
                    } else {
                        ESP_LOGE(HTTP_TAG, "Failed to save credentials");
                    }
                }
                break;
            }
            
            case CommandType::RESET_WIFI_TO_AUTO: {
                ESP_LOGI(HTTP_TAG, "WiFi reset to auto requested");
                auto& security = arcos::security::SecurityDriver::instance();
                if (security.resetToAuto()) {
                    ESP_LOGI(HTTP_TAG, "Reset to auto credentials successful");
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    esp_restart();
                }
                break;
            }
            
            case CommandType::RESTART:
                ESP_LOGI(HTTP_TAG, "Restart requested");
                vTaskDelay(pdMS_TO_TICKS(500));
                esp_restart();
                break;
                
            case CommandType::KICK_CLIENTS: {
                ESP_LOGI(HTTP_TAG, "Kick clients requested");
                wifi_sta_list_t sta_list;
                esp_wifi_ap_get_sta_list(&sta_list);
                
                ESP_LOGI(HTTP_TAG, "Found %d connected clients", sta_list.num);
                
                int kicked = 0;
                for (int i = 0; i < sta_list.num; i++) {
                    uint16_t aid = i + 1;
                    if (esp_wifi_deauth_sta(aid) == ESP_OK) {
                        kicked++;
                        ESP_LOGI(HTTP_TAG, "Kicked client AID=%d", aid);
                    }
                }
                ESP_LOGI(HTTP_TAG, "Kicked %d clients total", kicked);
                break;
            }
            
            case CommandType::SET_EXT_WIFI: {
                cJSON* enabled = cJSON_GetObjectItem(params, "enabled");
                cJSON* ssid = cJSON_GetObjectItem(params, "ssid");
                cJSON* password = cJSON_GetObjectItem(params, "password");
                
                auto& state = SYNC_STATE.state();
                
                if (enabled) {
                    state.extWifiEnabled = cJSON_IsTrue(enabled);
                }
                if (ssid && ssid->valuestring) {
                    strncpy(state.extWifiSSID, ssid->valuestring, sizeof(state.extWifiSSID) - 1);
                    state.extWifiSSID[sizeof(state.extWifiSSID) - 1] = '\0';
                }
                if (password && password->valuestring) {
                    strncpy(state.extWifiPassword, password->valuestring, sizeof(state.extWifiPassword) - 1);
                    state.extWifiPassword[sizeof(state.extWifiPassword) - 1] = '\0';
                }
                
                ESP_LOGI(HTTP_TAG, "External WiFi config: enabled=%d, ssid=%s", 
                         state.extWifiEnabled, state.extWifiSSID);
                
                // Save to NVS for persistence
                auto& security = arcos::security::SecurityDriver::instance();
                security.saveExtWifiSettings(state.extWifiEnabled, state.extWifiConnected, state.extWifiSSID, 
                                            state.extWifiPassword, state.authEnabled,
                                            state.authUsername, state.authPassword);
                break;
            }
            
            case CommandType::EXT_WIFI_CONNECT: {
                cJSON* connect = cJSON_GetObjectItem(params, "connect");
                auto& state = SYNC_STATE.state();
                
                if (connect) {
                    bool shouldConnect = cJSON_IsTrue(connect);
                    state.extWifiConnected = shouldConnect;
                    
                    ESP_LOGI(HTTP_TAG, "External WiFi connect: %s", shouldConnect ? "true" : "false");
                    
                    // Save connect state to NVS for persistence across boots
                    auto& security = arcos::security::SecurityDriver::instance();
                    security.saveExtWifiSettings(state.extWifiEnabled, state.extWifiConnected, state.extWifiSSID, 
                                                state.extWifiPassword, state.authEnabled,
                                                state.authUsername, state.authPassword);
                    
                    if (shouldConnect && state.extWifiEnabled && strlen(state.extWifiSSID) > 0) {
                        // Ensure STA netif exists
                        esp_netif_t* sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
                        if (!sta_netif) {
                            sta_netif = esp_netif_create_default_wifi_sta();
                            ESP_LOGI(HTTP_TAG, "Created STA netif for connection");
                        }
                        
                        // Initiate station connection
                        wifi_config_t sta_config = {};
                        strncpy((char*)sta_config.sta.ssid, state.extWifiSSID, sizeof(sta_config.sta.ssid));
                        strncpy((char*)sta_config.sta.password, state.extWifiPassword, sizeof(sta_config.sta.password));
                        sta_config.sta.threshold.authmode = WIFI_AUTH_WPA_PSK;  // More permissive
                        sta_config.sta.pmf_cfg.capable = true;
                        sta_config.sta.pmf_cfg.required = false;
                        
                        // Switch to AP+STA mode
                        esp_wifi_set_mode(WIFI_MODE_APSTA);
                        esp_wifi_set_config(WIFI_IF_STA, &sta_config);
                        esp_wifi_connect();
                        
                        ESP_LOGI(HTTP_TAG, "Connecting to external network: %s", state.extWifiSSID);
                    } else if (!shouldConnect) {
                        // Disconnect and switch back to AP-only
                        esp_wifi_disconnect();
                        esp_wifi_set_mode(WIFI_MODE_AP);
                        state.extWifiIsConnected = false;
                        memset(state.extWifiIP, 0, sizeof(state.extWifiIP));
                        state.extWifiRSSI = -100;
                        
                        ESP_LOGI(HTTP_TAG, "Disconnected from external network");
                    }
                }
                break;
            }
            
            case CommandType::SET_AUTH: {
                cJSON* enabled = cJSON_GetObjectItem(params, "enabled");
                cJSON* username = cJSON_GetObjectItem(params, "username");
                cJSON* password = cJSON_GetObjectItem(params, "password");
                
                auto& state = SYNC_STATE.state();
                
                if (enabled) {
                    state.authEnabled = cJSON_IsTrue(enabled);
                }
                if (username && username->valuestring) {
                    strncpy(state.authUsername, username->valuestring, sizeof(state.authUsername) - 1);
                    state.authUsername[sizeof(state.authUsername) - 1] = '\0';
                }
                if (password && password->valuestring && strlen(password->valuestring) > 0) {
                    // In production, this should be hashed!
                    strncpy(state.authPassword, password->valuestring, sizeof(state.authPassword) - 1);
                    state.authPassword[sizeof(state.authPassword) - 1] = '\0';
                }
                
                ESP_LOGI(HTTP_TAG, "Auth config: enabled=%d, username=%s", 
                         state.authEnabled, state.authUsername);
                
                // Save to NVS for persistence
                auto& security = arcos::security::SecurityDriver::instance();
                security.saveExtWifiSettings(state.extWifiEnabled, state.extWifiConnected, state.extWifiSSID, 
                                            state.extWifiPassword, state.authEnabled,
                                            state.authUsername, state.authPassword);
                break;
            }
            
            default:
                ESP_LOGW(HTTP_TAG, "Unknown command type");
                break;
        }
    }
    
    // State
    httpd_handle_t server_ = nullptr;
    CommandCallback command_callback_ = nullptr;
    Animation::AnimationConfigManager animConfigManager_;
    
    // Callbacks stored as instance members (not namespace-level statics)
    std::function<void(const SavedScene&)> sceneActivatedCallback_;
    std::function<void(const SavedScene&)> sceneUpdatedCallback_;
    std::function<void(const char*, float)> singleParamCallback_;
    std::function<void(const StaticSpriteSceneConfig&)> spriteDisplayCallback_;
    std::function<void()> displayClearCallback_;
    std::function<void(const SavedLedPreset&)> ledPresetActivatedCallback_;
    
public:
    /**
     * @brief Get the animation configuration manager
     */
    Animation::AnimationConfigManager& getConfigManager() { return animConfigManager_; }
    
    // Callback accessors for static handlers
    static std::function<void(const StaticSpriteSceneConfig&)>& getSpriteDisplayCallback() {
        return instance().spriteDisplayCallback_;
    }
    static std::function<void()>& getDisplayClearCallback() {
        return instance().displayClearCallback_;
    }
    // Note: getSceneActivatedCallback() is defined earlier as a public method
    static std::function<void(const SavedScene&)>& getSceneUpdatedCallback() {
        return instance().sceneUpdatedCallback_;
    }
    static std::function<void(const char*, float)>& getSingleParamCallback() {
        return instance().singleParamCallback_;
    }
    
    /**
     * @brief Lazy load sprite pixel data from SD card
     * Called when sprite is needed but pixel data was cleared from RAM
     */
    static bool lazyLoadSpritePixels(SavedSprite& sprite) {
        if (!sprite.pixelData.empty()) {
            return true;  // Already loaded
        }
        
        if (!sprite.savedToSd) {
            ESP_LOGW(HTTP_TAG, "Cannot lazy load sprite %d - not saved to SD", sprite.id);
            return false;
        }
        
        auto& fs = Utils::FileSystemService::instance();
        if (!sdcard_storage_ready_ || !fs.isReady() || !fs.isMounted()) {
            ESP_LOGE(HTTP_TAG, "SD card not available for lazy load");
            return false;
        }
        
        char pixelRelPath[64];
        snprintf(pixelRelPath, sizeof(pixelRelPath), "/Sprites/sprite_%d.bin", sprite.id);
        
        if (!fs.fileExists(pixelRelPath)) {
            ESP_LOGW(HTTP_TAG, "Sprite %d pixel file not found: %s", sprite.id, pixelRelPath);
            return false;
        }
        
        uint64_t fileSize = fs.getFileSize(pixelRelPath);
        if (fileSize == 0 || fileSize > 1024*1024) {
            ESP_LOGW(HTTP_TAG, "Invalid pixel file size for sprite %d: %llu", sprite.id, fileSize);
            return false;
        }
        
        sprite.pixelData.resize((size_t)fileSize);
        int bytesRead = fs.readFile(pixelRelPath, sprite.pixelData.data(), sprite.pixelData.size());
        
        if (bytesRead == (int)fileSize) {
            ESP_LOGI(HTTP_TAG, "Lazy loaded %d bytes for sprite %d '%s'", 
                     bytesRead, sprite.id, sprite.name.c_str());
            return true;
        } else {
            ESP_LOGE(HTTP_TAG, "Failed to lazy load sprite %d: expected %llu, got %d", 
                     sprite.id, fileSize, bytesRead);
            sprite.pixelData.clear();
            return false;
        }
    }
    
    /**
     * @brief Find a saved sprite by ID (with lazy loading)
     * @param spriteId The sprite ID to find
     * @return Pointer to sprite, or nullptr if not found
     */
    static SavedSprite* findSpriteById(int spriteId) {
        ESP_LOGI(HTTP_TAG, "findSpriteById(%d): searching %d sprites", spriteId, savedSprites_.size());
        for (auto& sprite : savedSprites_) {
            ESP_LOGI(HTTP_TAG, "  - Checking sprite id=%d '%s' pixels=%s savedToSd=%s", 
                     sprite.id, sprite.name.c_str(), 
                     sprite.pixelData.empty() ? "NO" : "YES",
                     sprite.savedToSd ? "YES" : "NO");
            if (sprite.id == spriteId) {
                ESP_LOGI(HTTP_TAG, "  - FOUND!");
                // Lazy load pixel data if needed
                if (sprite.pixelData.empty() && sprite.savedToSd) {
                    ESP_LOGI(HTTP_TAG, "  - Lazy loading pixel data from SD...");
                    lazyLoadSpritePixels(sprite);
                }
                return &sprite;
            }
        }
        ESP_LOGW(HTTP_TAG, "  - NOT FOUND");
        return nullptr;
    }
    
    /**
     * @brief Mark a sprite as uploaded to GPU
     * @param spriteId The sprite ID to mark
     */
    static void markSpriteUploaded(int spriteId) {
        for (auto& sprite : savedSprites_) {
            if (sprite.id == spriteId) {
                sprite.uploadedToGpu = true;
                break;
            }
        }
    }
    
    /**
     * @brief Clear sprite pixel data from RAM to free memory
     * Call after sprite has been uploaded to GPU
     * @param spriteId The sprite ID to clear RAM for
     */
    static void clearSpriteRam(int spriteId) {
        for (auto& sprite : savedSprites_) {
            if (sprite.id == spriteId) {
                if (!sprite.pixelData.empty()) {
                    size_t freedBytes = sprite.pixelData.size();
                    sprite.pixelData.clear();
                    sprite.pixelData.shrink_to_fit();  // Actually release the memory
                    ESP_LOGI(HTTP_TAG, "Cleared %zu bytes RAM for sprite %d (savedToSd=%s)", 
                             freedBytes, spriteId, sprite.savedToSd ? "YES" : "NO");
                }
                // Also clear preview to save more RAM
                if (!sprite.preview.empty()) {
                    size_t previewBytes = sprite.preview.size();
                    sprite.preview.clear();
                    sprite.preview.shrink_to_fit();
                    ESP_LOGI(HTTP_TAG, "Cleared %zu bytes preview RAM for sprite %d", previewBytes, spriteId);
                }
                break;
            }
        }
    }
    
    /**
     * @brief Clear all sprite pixel data from RAM
     * Call to free memory when sprites are no longer needed in RAM
     */
    static void clearAllSpriteRam() {
        size_t totalFreed = 0;
        for (auto& sprite : savedSprites_) {
            if (!sprite.pixelData.empty()) {
                totalFreed += sprite.pixelData.size();
                sprite.pixelData.clear();
                sprite.pixelData.shrink_to_fit();
            }
            if (!sprite.preview.empty()) {
                totalFreed += sprite.preview.size();
                sprite.preview.clear();
                sprite.preview.shrink_to_fit();
            }
        }
        ESP_LOGI(HTTP_TAG, "Cleared %zu total bytes of sprite RAM (%d sprites)", totalFreed, savedSprites_.size());
    }
};

// Convenience macro
#define HTTP_SERVER HttpServer::instance()

} // namespace Web
} // namespace SystemAPI
