/*****************************************************************
 * @file YamlSceneDriver.hpp
 * @brief YAML Scene Driver - Integration Layer
 * 
 * Provides a bridge between the YAML-based SceneManager and the
 * existing CurrentMode animation system. This allows gradual 
 * migration from JSON scenes to YAML scenes.
 * 
 * Usage:
 * 1. Call YamlSceneDriver::init() after SDManager::init()
 * 2. Load scenes with YamlSceneDriver::loadAllScenes()
 * 3. Activate scenes with YamlSceneDriver::activateScene()
 * 
 * The driver fires callbacks that CurrentMode can use to
 * update the GpuDriverState animation parameters.
 * 
 * @author ARCOS
 * @version 1.0
 *****************************************************************/

#pragma once

#include <string>
#include <vector>
#include <functional>
#include <cstdint>

#include "esp_log.h"
#include "Drivers/SDManager.hpp"
#include "Drivers/SceneManager.hpp"

namespace Drivers {

//=============================================================================
// Callback Types for Integration
//=============================================================================

/**
 * @brief Called when a YAML scene is activated
 * 
 * CurrentMode should use this to:
 * - Set the animation type (gyro_eyes, static_image, sway)
 * - Configure animation parameters
 * - Enable/disable displays and LEDs
 */
using YamlSceneActivateCallback = std::function<void(
    const std::string& animType,
    int spriteId,
    float posX, float posY,
    float sensitivity,
    bool mirror,
    uint8_t bgR, uint8_t bgG, uint8_t bgB,
    bool displayEnabled,
    bool ledsEnabled,
    uint8_t ledR, uint8_t ledG, uint8_t ledB,
    int ledBrightness
)>;

/**
 * @brief Called when sprites need to be uploaded to GPU
 */
using YamlSpriteUploadCallback = std::function<bool(
    int gpuSlot,
    const uint8_t* data,
    size_t size,
    int width,
    int height
)>;

//=============================================================================
// YamlSceneDriver Class
//=============================================================================

/**
 * @brief YAML Scene Driver - Manages YAML-based scenes
 * 
 * This driver provides a clean interface for loading and activating
 * scenes from YAML files. It integrates with the existing CurrentMode
 * through callbacks.
 * 
 * File structure on SD card:
 *   /scenes/           - Scene YAML files (1 per scene)
 *     scene_1.yaml
 *     scene_2.yaml
 *     ...
 *   /sprites/          - Sprite binary files
 *     eye_32x32.bin
 *     pupil_16x16.bin
 *     ...
 */
class YamlSceneDriver {
public:
    //=========================================================================
    // Initialization
    //=========================================================================
    
    /**
     * @brief Initialize the YAML scene driver
     * 
     * Call after SDManager::init() has been called.
     * 
     * @param sdMiso MISO pin for SD card
     * @param sdMosi MOSI pin for SD card
     * @param sdClk CLK pin for SD card
     * @param sdCs CS pin for SD card
     * @return true if successful
     */
    static bool init(int sdMiso = 14, int sdMosi = 47, int sdClk = 21, int sdCs = 48) {
        if (s_initialized) {
            ESP_LOGW(TAG, "Already initialized");
            return true;
        }
        
        ESP_LOGI(TAG, "Initializing YAML Scene Driver...");
        
        // Initialize SD card if not already done
        if (!SDManager::isMounted()) {
            if (!SDManager::init(sdMiso, sdMosi, sdClk, sdCs)) {
                ESP_LOGE(TAG, "Failed to initialize SD card");
                return false;
            }
        }
        
        // Set up SceneManager callbacks
        SceneManager::setOnSceneChange([](const SceneConfig& scene) {
            onSceneChanged(scene);
        });
        
        SceneManager::setOnSpriteLoad([](int gpuSlot, const uint8_t* data, 
                                          size_t size, int width, int height) -> bool {
            if (s_spriteUploadCallback) {
                return s_spriteUploadCallback(gpuSlot, data, size, width, height);
            }
            ESP_LOGW(TAG, "No sprite upload callback set");
            return false;
        });
        
        // Initialize SceneManager
        if (!SceneManager::init()) {
            ESP_LOGE(TAG, "Failed to initialize SceneManager");
            return false;
        }
        
        s_initialized = true;
        ESP_LOGI(TAG, "YAML Scene Driver ready (%zu scenes loaded)", 
                 SceneManager::getScenes().size());
        
        return true;
    }
    
    /**
     * @brief Check if driver is initialized
     */
    static bool isInitialized() {
        return s_initialized;
    }
    
    //=========================================================================
    // Callbacks
    //=========================================================================
    
    /**
     * @brief Set callback for scene activation
     */
    static void setSceneActivateCallback(YamlSceneActivateCallback callback) {
        s_sceneActivateCallback = callback;
    }
    
    /**
     * @brief Set callback for sprite upload
     */
    static void setSpriteUploadCallback(YamlSpriteUploadCallback callback) {
        s_spriteUploadCallback = callback;
    }
    
    //=========================================================================
    // Scene Operations
    //=========================================================================
    
    /**
     * @brief Get all available scenes
     */
    static const std::vector<SceneConfig>& getScenes() {
        return SceneManager::getScenes();
    }
    
    /**
     * @brief Get scene count
     */
    static size_t getSceneCount() {
        return SceneManager::getScenes().size();
    }
    
    /**
     * @brief Create a new scene with default settings
     * @param name Scene name
     * @return Scene ID or 0 on error
     */
    static int createScene(const std::string& name) {
        return SceneManager::createScene(name);
    }
    
    /**
     * @brief Get a scene by ID
     */
    static SceneConfig* getScene(int id) {
        return SceneManager::getScene(id);
    }
    
    /**
     * @brief Save a scene to its YAML file
     */
    static bool saveScene(int id) {
        return SceneManager::saveScene(id);
    }
    
    /**
     * @brief Delete a scene
     */
    static bool deleteScene(int id) {
        return SceneManager::deleteScene(id);
    }
    
    /**
     * @brief Activate a scene by ID
     * 
     * This will:
     * 1. Load the scene's sprites to GPU
     * 2. Fire the scene activate callback
     * 3. Set the active scene
     * 
     * @param id Scene ID
     * @return true if activated
     */
    static bool activateScene(int id) {
        return SceneManager::activateScene(id);
    }
    
    /**
     * @brief Get the currently active scene ID
     */
    static int getActiveSceneId() {
        return SceneManager::getActiveSceneId();
    }
    
    /**
     * @brief Reload all scenes from SD card
     */
    static bool reloadScenes() {
        if (!s_initialized) return false;
        return SceneManager::init();
    }
    
    //=========================================================================
    // Sprite Operations
    //=========================================================================
    
    /**
     * @brief Get all known sprites
     */
    static const std::vector<SpriteInfo>& getSprites() {
        return SceneManager::getSprites();
    }
    
    /**
     * @brief Register a new sprite
     */
    static int registerSprite(const std::string& name, const std::string& path,
                              int width, int height) {
        return SceneManager::registerSprite(name, path, width, height);
    }
    
    //=========================================================================
    // JSON Export (for Web API compatibility)
    //=========================================================================
    
    /**
     * @brief Export scenes as JSON for web API
     */
    static std::string scenesToJson() {
        return SceneManager::scenesToJson();
    }
    
    /**
     * @brief Export sprites as JSON for web API
     */
    static std::string spritesToJson() {
        return SceneManager::spritesToJson();
    }
    
    //=========================================================================
    // Quick Scene Setup Helpers
    //=========================================================================
    
    /**
     * @brief Create a gyro eyes scene with default settings
     */
    static int createGyroEyesScene(const std::string& name, int spriteId = -1) {
        int id = SceneManager::createScene(name);
        if (id <= 0) return 0;
        
        SceneConfig* scene = SceneManager::getScene(id);
        if (!scene) return 0;
        
        scene->animation.type = "gyro_eyes";
        scene->animation.spriteId = spriteId;
        scene->animation.posX = 64;
        scene->animation.posY = 16;
        scene->animation.sensitivity = 1.5f;
        scene->animation.mirror = true;
        scene->displayEnabled = true;
        scene->ledsEnabled = false;
        
        SceneManager::saveScene(id);
        return id;
    }
    
    /**
     * @brief Create a static image scene
     */
    static int createStaticScene(const std::string& name, int spriteId,
                                  int posX = 64, int posY = 16) {
        int id = SceneManager::createScene(name);
        if (id <= 0) return 0;
        
        SceneConfig* scene = SceneManager::getScene(id);
        if (!scene) return 0;
        
        scene->animation.type = "static";
        scene->animation.spriteId = spriteId;
        scene->animation.posX = posX;
        scene->animation.posY = posY;
        scene->displayEnabled = true;
        scene->ledsEnabled = false;
        
        SceneManager::saveScene(id);
        return id;
    }

private:
    static constexpr const char* TAG = "YamlSceneDriver";
    
    static inline bool s_initialized = false;
    static inline YamlSceneActivateCallback s_sceneActivateCallback;
    static inline YamlSpriteUploadCallback s_spriteUploadCallback;
    
    /**
     * @brief Called when SceneManager activates a scene
     */
    static void onSceneChanged(const SceneConfig& scene) {
        ESP_LOGI(TAG, "Scene changed: %s (id=%d)", scene.name.c_str(), scene.id);
        ESP_LOGI(TAG, "  Animation: %s, Sprite: %d", 
                 scene.animation.type.c_str(), scene.animation.spriteId);
        ESP_LOGI(TAG, "  Display: %s, LEDs: %s",
                 scene.displayEnabled ? "ON" : "OFF",
                 scene.ledsEnabled ? "ON" : "OFF");
        
        // Fire the callback to CurrentMode
        if (s_sceneActivateCallback) {
            s_sceneActivateCallback(
                scene.animation.type,
                scene.animation.spriteId,
                static_cast<float>(scene.animation.posX),
                static_cast<float>(scene.animation.posY),
                scene.animation.sensitivity,
                scene.animation.mirror,
                scene.animation.bgColor.r,
                scene.animation.bgColor.g,
                scene.animation.bgColor.b,
                scene.displayEnabled,
                scene.ledsEnabled,
                scene.leds.color.r,
                scene.leds.color.g,
                scene.leds.color.b,
                scene.leds.brightness
            );
        }
    }
};

} // namespace Drivers
