/*****************************************************************
 * @file SceneManager.hpp
 * @brief Scene Manager - YAML-based Scene System
 * 
 * Manages scenes where each scene is stored as a separate YAML file.
 * One scene = one YAML file. Provides:
 * - Scene creation, loading, saving, deletion
 * - Scene activation with callbacks
 * - Sprite management and GPU upload integration
 * - JSON export for web API
 * 
 * Scenes are stored in /scenes/ directory on SD card.
 * Sprites are stored in /sprites/ directory on SD card.
 * 
 * @author ARCOS
 * @version 1.0
 *****************************************************************/

#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <cstdint>
#include <algorithm>

#include "esp_log.h"
#include "Drivers/YamlParser.hpp"
#include "Drivers/SDManager.hpp"

namespace Drivers {

//=============================================================================
// Configuration Structures
//=============================================================================

/**
 * @brief RGB Color value
 */
struct ColorRGB {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    
    ColorRGB() = default;
    ColorRGB(uint8_t r_, uint8_t g_, uint8_t b_) : r(r_), g(g_), b(b_) {}
};

/**
 * @brief Animation configuration
 */
struct AnimationConfig {
    std::string type = "static";    ///< Animation type (gyro_eyes, static, etc.)
    int spriteId = -1;              ///< GPU sprite slot to use (-1 = none)
    int posX = 64;                  ///< X position on display
    int posY = 16;                  ///< Y position on display
    float rotation = 0.0f;          ///< Initial rotation in degrees
    float sensitivity = 1.0f;       ///< Gyro sensitivity multiplier
    bool mirror = false;            ///< Mirror the animation
    ColorRGB bgColor;               ///< Background color
};

/**
 * @brief LED configuration
 */
struct LedConfig {
    int brightness = 80;            ///< LED brightness (0-255)
    ColorRGB color;                 ///< LED color
};

/**
 * @brief Sprite definition
 */
struct SpriteInfo {
    int id = 0;                     ///< Unique sprite ID
    std::string name;               ///< Sprite name
    std::string path;               ///< Path to sprite file on SD card
    int width = 0;                  ///< Sprite width in pixels
    int height = 0;                 ///< Sprite height in pixels
    int gpuSlot = -1;               ///< GPU slot when loaded (-1 = not loaded)
    bool loaded = false;            ///< Whether loaded to GPU
};

/**
 * @brief Complete scene configuration
 */
struct SceneConfig {
    int id = 0;                     ///< Unique scene ID
    std::string name;               ///< Scene name
    std::string filename;           ///< YAML filename (without path)
    float version = 1.0f;           ///< Scene version
    
    AnimationConfig animation;      ///< Animation settings
    bool displayEnabled = true;     ///< Whether display is enabled
    bool ledsEnabled = false;       ///< Whether LEDs are enabled
    LedConfig leds;                 ///< LED settings
    
    std::vector<SpriteInfo> sprites; ///< Sprites used by this scene
};

//=============================================================================
// Callback Types
//=============================================================================

/**
 * @brief Called when a scene is activated
 * @param scene The activated scene configuration
 */
using SceneChangeCallback = std::function<void(const SceneConfig& scene)>;

/**
 * @brief Called when a sprite needs to be uploaded to GPU
 * @param gpuSlot GPU slot to upload to
 * @param data Sprite pixel data
 * @param size Data size in bytes
 * @param width Sprite width
 * @param height Sprite height
 * @return true if upload successful
 */
using SpriteLoadCallback = std::function<bool(int gpuSlot, const uint8_t* data, 
                                               size_t size, int width, int height)>;

//=============================================================================
// SceneManager Class
//=============================================================================

/**
 * @brief Static scene manager
 * 
 * Manages scenes stored as YAML files on SD card.
 * Each scene is one YAML file in the /scenes/ directory.
 * 
 * Example usage:
 * @code
 *   // Initialize (after SDManager::init())
 *   SceneManager::setOnSceneChange(mySceneCallback);
 *   SceneManager::setOnSpriteLoad(mySpriteCallback);
 *   SceneManager::init();
 *   
 *   // Create a new scene
 *   int id = SceneManager::createScene("MyScene");
 *   
 *   // Modify and save
 *   SceneConfig* scene = SceneManager::getScene(id);
 *   scene->animation.type = "gyro_eyes";
 *   SceneManager::saveScene(id);
 *   
 *   // Activate it
 *   SceneManager::activateScene(id);
 * @endcode
 */
class SceneManager {
public:
    //=========================================================================
    // Initialization
    //=========================================================================
    
    /**
     * @brief Initialize scene manager
     * 
     * Loads all scenes from /scenes/ directory on SD card.
     * Call after SDManager::init().
     * 
     * @return true if successful
     */
    static bool init() {
        ESP_LOGI(TAG, "Initializing Scene Manager...");
        
        if (!SDManager::isMounted()) {
            ESP_LOGE(TAG, "SD card not mounted!");
            return false;
        }
        
        // Ensure directories exist
        SDManager::createDirectory(SCENES_DIR);
        SDManager::createDirectory(SPRITES_DIR);
        
        // Load all scenes
        s_scenes.clear();
        s_sprites.clear();
        s_nextSceneId = 1;
        s_nextSpriteId = 1;
        s_activeSceneId = -1;
        
        auto files = SDManager::listDirectory(SCENES_DIR);
        for (const auto& file : files) {
            if (!file.isDirectory && hasYamlExtension(file.name)) {
                std::string path = std::string(SCENES_DIR) + "/" + file.name;
                SceneConfig scene = loadSceneFromFile(path);
                if (scene.id > 0) {
                    scene.filename = file.name;
                    s_scenes.push_back(scene);
                    if (scene.id >= s_nextSceneId) {
                        s_nextSceneId = scene.id + 1;
                    }
                    
                    // Track sprites from this scene
                    for (const auto& sprite : scene.sprites) {
                        if (sprite.id >= s_nextSpriteId) {
                            s_nextSpriteId = sprite.id + 1;
                        }
                        // Add to global sprite list if not already there
                        addSpriteIfNew(sprite);
                    }
                }
            }
        }
        
        ESP_LOGI(TAG, "Loaded %zu scenes, %zu sprites", 
                 s_scenes.size(), s_sprites.size());
        
        return true;
    }
    
    //=========================================================================
    // Callbacks
    //=========================================================================
    
    /**
     * @brief Set callback for scene changes
     */
    static void setOnSceneChange(SceneChangeCallback callback) {
        s_onSceneChange = callback;
    }
    
    /**
     * @brief Set callback for sprite loading
     */
    static void setOnSpriteLoad(SpriteLoadCallback callback) {
        s_onSpriteLoad = callback;
    }
    
    //=========================================================================
    // Scene Management
    //=========================================================================
    
    /**
     * @brief Create a new empty scene
     * @param name Scene name
     * @return Scene ID (>0) or 0 on error
     */
    static int createScene(const std::string& name) {
        SceneConfig scene;
        scene.id = s_nextSceneId++;
        scene.name = name.empty() ? "Scene_" + std::to_string(scene.id) : name;
        scene.filename = generateFilename(scene.name, scene.id);
        scene.version = 1.0f;
        
        // Set defaults
        scene.animation.type = "static";
        scene.animation.posX = 64;
        scene.animation.posY = 16;
        scene.displayEnabled = true;
        scene.ledsEnabled = false;
        scene.leds.brightness = 80;
        
        s_scenes.push_back(scene);
        
        // Save to SD card
        if (!saveScene(scene.id)) {
            ESP_LOGE(TAG, "Failed to save new scene to SD card");
            s_scenes.pop_back();
            s_nextSceneId--;
            return 0;
        }
        
        ESP_LOGI(TAG, "Created scene: %s (id=%d, file=%s)", 
                 scene.name.c_str(), scene.id, scene.filename.c_str());
        
        return scene.id;
    }
    
    /**
     * @brief Get a scene by ID
     * @param id Scene ID
     * @return Pointer to scene, or nullptr if not found
     */
    static SceneConfig* getScene(int id) {
        for (auto& scene : s_scenes) {
            if (scene.id == id) {
                return &scene;
            }
        }
        return nullptr;
    }
    
    /**
     * @brief Get all scenes
     */
    static const std::vector<SceneConfig>& getScenes() {
        return s_scenes;
    }
    
    /**
     * @brief Save a scene to its YAML file
     * @param id Scene ID
     * @return true if successful
     */
    static bool saveScene(int id) {
        SceneConfig* scene = getScene(id);
        if (!scene) {
            ESP_LOGE(TAG, "Scene not found: %d", id);
            return false;
        }
        
        std::string yaml = sceneToYaml(*scene);
        std::string path = std::string(SCENES_DIR) + "/" + scene->filename;
        
        if (!SDManager::writeText(path, yaml)) {
            ESP_LOGE(TAG, "Failed to write scene file: %s", path.c_str());
            return false;
        }
        
        ESP_LOGI(TAG, "Saved scene: %s", path.c_str());
        return true;
    }
    
    /**
     * @brief Delete a scene
     * @param id Scene ID
     * @return true if deleted
     */
    static bool deleteScene(int id) {
        for (auto it = s_scenes.begin(); it != s_scenes.end(); ++it) {
            if (it->id == id) {
                std::string path = std::string(SCENES_DIR) + "/" + it->filename;
                SDManager::deleteFile(path);
                
                if (s_activeSceneId == id) {
                    s_activeSceneId = -1;
                }
                
                ESP_LOGI(TAG, "Deleted scene: %s (id=%d)", it->name.c_str(), id);
                s_scenes.erase(it);
                return true;
            }
        }
        return false;
    }
    
    /**
     * @brief Activate a scene
     * @param id Scene ID
     * @return true if activated
     */
    static bool activateScene(int id) {
        SceneConfig* scene = getScene(id);
        if (!scene) {
            ESP_LOGE(TAG, "Cannot activate: scene not found: %d", id);
            return false;
        }
        
        s_activeSceneId = id;
        
        // Load sprites for this scene
        for (auto& sprite : scene->sprites) {
            loadSpriteToGpu(sprite);
        }
        
        // Fire callback
        if (s_onSceneChange) {
            s_onSceneChange(*scene);
        }
        
        ESP_LOGI(TAG, "Activated scene: %s (id=%d)", scene->name.c_str(), id);
        return true;
    }
    
    /**
     * @brief Get currently active scene ID
     * @return Scene ID or -1 if none active
     */
    static int getActiveSceneId() {
        return s_activeSceneId;
    }
    
    /**
     * @brief Get currently active scene
     * @return Pointer to active scene, or nullptr
     */
    static SceneConfig* getActiveScene() {
        if (s_activeSceneId < 0) return nullptr;
        return getScene(s_activeSceneId);
    }
    
    //=========================================================================
    // Sprite Management
    //=========================================================================
    
    /**
     * @brief Get all known sprites
     */
    static const std::vector<SpriteInfo>& getSprites() {
        return s_sprites;
    }
    
    /**
     * @brief Get a sprite by ID
     */
    static SpriteInfo* getSprite(int id) {
        for (auto& sprite : s_sprites) {
            if (sprite.id == id) {
                return &sprite;
            }
        }
        return nullptr;
    }
    
    /**
     * @brief Register a new sprite
     * @param name Sprite name
     * @param path Path to sprite file on SD card
     * @param width Sprite width
     * @param height Sprite height
     * @return Sprite ID (>0) or 0 on error
     */
    static int registerSprite(const std::string& name, const std::string& path,
                              int width, int height) {
        SpriteInfo sprite;
        sprite.id = s_nextSpriteId++;
        sprite.name = name;
        sprite.path = path;
        sprite.width = width;
        sprite.height = height;
        sprite.gpuSlot = -1;
        sprite.loaded = false;
        
        s_sprites.push_back(sprite);
        
        ESP_LOGI(TAG, "Registered sprite: %s (id=%d, %dx%d)", 
                 name.c_str(), sprite.id, width, height);
        
        return sprite.id;
    }
    
    /**
     * @brief Load a sprite to GPU
     * @param sprite Sprite info (gpuSlot and loaded will be updated)
     * @return true if loaded successfully
     */
    static bool loadSpriteToGpu(SpriteInfo& sprite) {
        if (sprite.loaded) {
            return true; // Already loaded
        }
        
        if (!s_onSpriteLoad) {
            ESP_LOGW(TAG, "No sprite load callback set");
            return false;
        }
        
        // Read sprite data from SD card
        std::vector<uint8_t> data = SDManager::readBinary(sprite.path);
        if (data.empty()) {
            ESP_LOGE(TAG, "Failed to read sprite: %s", sprite.path.c_str());
            return false;
        }
        
        // Assign GPU slot (use sprite ID as slot)
        sprite.gpuSlot = sprite.id;
        
        // Upload to GPU
        if (s_onSpriteLoad(sprite.gpuSlot, data.data(), data.size(),
                          sprite.width, sprite.height)) {
            sprite.loaded = true;
            ESP_LOGI(TAG, "Loaded sprite to GPU slot %d: %s", 
                     sprite.gpuSlot, sprite.name.c_str());
            return true;
        }
        
        sprite.gpuSlot = -1;
        return false;
    }
    
    //=========================================================================
    // YAML Conversion
    //=========================================================================
    
    /**
     * @brief Convert a scene to YAML string
     */
    static std::string sceneToYaml(const SceneConfig& scene) {
        YamlNode root;
        
        // Header comment will be added by serialize
        root["name"] = YamlNode(scene.name);
        root["id"] = YamlNode(scene.id);
        root["version"] = YamlNode(scene.version);
        
        // Animation
        root["animation"].makeMap();
        root["animation"]["type"] = YamlNode(scene.animation.type);
        root["animation"]["spriteId"] = YamlNode(scene.animation.spriteId);
        root["animation"]["posX"] = YamlNode(scene.animation.posX);
        root["animation"]["posY"] = YamlNode(scene.animation.posY);
        root["animation"]["rotation"] = YamlNode(scene.animation.rotation);
        root["animation"]["sensitivity"] = YamlNode(scene.animation.sensitivity);
        root["animation"]["mirror"] = YamlNode(scene.animation.mirror);
        
        root["animation"]["bgColor"].makeMap();
        root["animation"]["bgColor"]["r"] = YamlNode(static_cast<int>(scene.animation.bgColor.r));
        root["animation"]["bgColor"]["g"] = YamlNode(static_cast<int>(scene.animation.bgColor.g));
        root["animation"]["bgColor"]["b"] = YamlNode(static_cast<int>(scene.animation.bgColor.b));
        
        // Display
        root["displayEnabled"] = YamlNode(scene.displayEnabled);
        root["ledsEnabled"] = YamlNode(scene.ledsEnabled);
        
        // LEDs
        root["leds"].makeMap();
        root["leds"]["brightness"] = YamlNode(scene.leds.brightness);
        root["leds"]["color"].makeMap();
        root["leds"]["color"]["r"] = YamlNode(static_cast<int>(scene.leds.color.r));
        root["leds"]["color"]["g"] = YamlNode(static_cast<int>(scene.leds.color.g));
        root["leds"]["color"]["b"] = YamlNode(static_cast<int>(scene.leds.color.b));
        
        // Sprites
        if (!scene.sprites.empty()) {
            root["sprites"].makeArray();
            for (const auto& sprite : scene.sprites) {
                YamlNode spriteNode;
                spriteNode["name"] = YamlNode(sprite.name);
                spriteNode["id"] = YamlNode(sprite.id);
                spriteNode["path"] = YamlNode(sprite.path);
                spriteNode["width"] = YamlNode(sprite.width);
                spriteNode["height"] = YamlNode(sprite.height);
                root["sprites"].push(spriteNode);
            }
        }
        
        // Add header comment
        std::string yaml = "# Scene Configuration\n";
        yaml += "# Generated by ARCOS SceneManager\n\n";
        yaml += YamlParser::serialize(root);
        
        return yaml;
    }
    
    /**
     * @brief Parse a scene from YAML string
     */
    static SceneConfig yamlToScene(const std::string& yaml) {
        SceneConfig scene;
        YamlNode root = YamlParser::parse(yaml);
        
        scene.name = root["name"].asString("Unnamed");
        scene.id = root["id"].asInt(0);
        scene.version = root["version"].asFloat(1.0f);
        
        // Animation
        if (root.hasKey("animation")) {
            auto& anim = root["animation"];
            scene.animation.type = anim["type"].asString("static");
            scene.animation.spriteId = anim["spriteId"].asInt(-1);
            scene.animation.posX = anim["posX"].asInt(64);
            scene.animation.posY = anim["posY"].asInt(16);
            scene.animation.rotation = anim["rotation"].asFloat(0.0f);
            scene.animation.sensitivity = anim["sensitivity"].asFloat(1.0f);
            scene.animation.mirror = anim["mirror"].asBool(false);
            
            if (anim.hasKey("bgColor")) {
                scene.animation.bgColor.r = static_cast<uint8_t>(anim["bgColor"]["r"].asInt(0));
                scene.animation.bgColor.g = static_cast<uint8_t>(anim["bgColor"]["g"].asInt(0));
                scene.animation.bgColor.b = static_cast<uint8_t>(anim["bgColor"]["b"].asInt(0));
            }
        }
        
        // Display
        scene.displayEnabled = root["displayEnabled"].asBool(true);
        scene.ledsEnabled = root["ledsEnabled"].asBool(false);
        
        // LEDs
        if (root.hasKey("leds")) {
            auto& leds = root["leds"];
            scene.leds.brightness = leds["brightness"].asInt(80);
            if (leds.hasKey("color")) {
                scene.leds.color.r = static_cast<uint8_t>(leds["color"]["r"].asInt(255));
                scene.leds.color.g = static_cast<uint8_t>(leds["color"]["g"].asInt(255));
                scene.leds.color.b = static_cast<uint8_t>(leds["color"]["b"].asInt(255));
            }
        }
        
        // Sprites
        if (root.hasKey("sprites") && root["sprites"].isArray()) {
            for (size_t i = 0; i < root["sprites"].size(); i++) {
                auto& s = root["sprites"][i];
                SpriteInfo sprite;
                sprite.name = s["name"].asString();
                sprite.id = s["id"].asInt(0);
                sprite.path = s["path"].asString();
                sprite.width = s["width"].asInt(0);
                sprite.height = s["height"].asInt(0);
                sprite.gpuSlot = -1;
                sprite.loaded = false;
                scene.sprites.push_back(sprite);
            }
        }
        
        return scene;
    }
    
    //=========================================================================
    // JSON Export (for Web API)
    //=========================================================================
    
    /**
     * @brief Export all scenes as JSON
     */
    static std::string scenesToJson() {
        std::string json = "[";
        for (size_t i = 0; i < s_scenes.size(); i++) {
            if (i > 0) json += ",";
            json += sceneToJson(s_scenes[i]);
        }
        json += "]";
        return json;
    }
    
    /**
     * @brief Export a single scene as JSON
     */
    static std::string sceneToJson(const SceneConfig& scene) {
        char buf[512];
        snprintf(buf, sizeof(buf),
            "{\"id\":%d,\"name\":\"%s\",\"active\":%s,"
            "\"animation\":{\"type\":\"%s\",\"spriteId\":%d},"
            "\"displayEnabled\":%s,\"ledsEnabled\":%s}",
            scene.id, scene.name.c_str(),
            (scene.id == s_activeSceneId) ? "true" : "false",
            scene.animation.type.c_str(), scene.animation.spriteId,
            scene.displayEnabled ? "true" : "false",
            scene.ledsEnabled ? "true" : "false");
        return std::string(buf);
    }
    
    /**
     * @brief Export all sprites as JSON
     */
    static std::string spritesToJson() {
        std::string json = "[";
        for (size_t i = 0; i < s_sprites.size(); i++) {
            if (i > 0) json += ",";
            json += spriteToJson(s_sprites[i]);
        }
        json += "]";
        return json;
    }
    
    /**
     * @brief Export a single sprite as JSON
     */
    static std::string spriteToJson(const SpriteInfo& sprite) {
        char buf[256];
        snprintf(buf, sizeof(buf),
            "{\"id\":%d,\"name\":\"%s\",\"path\":\"%s\","
            "\"width\":%d,\"height\":%d,\"loaded\":%s,\"gpuSlot\":%d}",
            sprite.id, sprite.name.c_str(), sprite.path.c_str(),
            sprite.width, sprite.height,
            sprite.loaded ? "true" : "false", sprite.gpuSlot);
        return std::string(buf);
    }

private:
    static constexpr const char* TAG = "SceneManager";
    static constexpr const char* SCENES_DIR = "/scenes";
    static constexpr const char* SPRITES_DIR = "/sprites";
    
    static inline std::vector<SceneConfig> s_scenes;
    static inline std::vector<SpriteInfo> s_sprites;
    static inline int s_nextSceneId = 1;
    static inline int s_nextSpriteId = 1;
    static inline int s_activeSceneId = -1;
    
    static inline SceneChangeCallback s_onSceneChange;
    static inline SpriteLoadCallback s_onSpriteLoad;
    
    /**
     * @brief Check if filename has YAML extension
     */
    static bool hasYamlExtension(const std::string& filename) {
        size_t dot = filename.rfind('.');
        if (dot == std::string::npos) return false;
        std::string ext = filename.substr(dot);
        // Convert to lowercase for comparison
        for (char& c : ext) c = tolower(c);
        return (ext == ".yaml" || ext == ".yml");
    }
    
    /**
     * @brief Generate a safe filename from scene name
     */
    static std::string generateFilename(const std::string& name, int id) {
        std::string safe;
        for (char c : name) {
            if (isalnum(c) || c == '_' || c == '-') {
                safe += c;
            } else if (c == ' ') {
                safe += '_';
            }
        }
        if (safe.empty()) {
            safe = "scene";
        }
        // Append ID to ensure uniqueness
        safe += "_" + std::to_string(id) + ".yaml";
        return safe;
    }
    
    /**
     * @brief Load a scene from a YAML file
     */
    static SceneConfig loadSceneFromFile(const std::string& path) {
        std::string yaml = SDManager::readText(path);
        if (yaml.empty()) {
            ESP_LOGE(TAG, "Failed to read scene file: %s", path.c_str());
            return SceneConfig();
        }
        
        SceneConfig scene = yamlToScene(yaml);
        ESP_LOGI(TAG, "Loaded scene from %s: %s (id=%d)", 
                 path.c_str(), scene.name.c_str(), scene.id);
        return scene;
    }
    
    /**
     * @brief Add a sprite to global list if not already present
     */
    static void addSpriteIfNew(const SpriteInfo& sprite) {
        for (const auto& existing : s_sprites) {
            if (existing.id == sprite.id || existing.path == sprite.path) {
                return; // Already exists
            }
        }
        s_sprites.push_back(sprite);
    }
};

} // namespace Drivers
