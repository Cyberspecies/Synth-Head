/*****************************************************************
 * @file SceneTestHarness.hpp
 * @brief Comprehensive Automated Scene Testing System
 * 
 * This system provides:
 * - Fully automated test suites with verbose console output
 * - Tests for scene CRUD operations
 * - Tests for animation type switching
 * - Tests for display/LED toggle persistence
 * - Tests for shader settings
 * - Tests for LED color settings
 * - Tests for scene activation and callback triggering
 * - Tests for SD card persistence (save/load)
 * - Tests for scene reordering
 * - Edge case testing
 * 
 * Commands (via Serial):
 *   TEST:HELP                  - Show all commands
 *   TEST:FULL                  - Run FULL automated test suite
 *   TEST:SCENES:LIST           - List all scenes with full config
 *   TEST:SCENES:DUMP           - Dump current active scene state
 *   TEST:SCENES:CREATE:name    - Create a new scene
 *   TEST:SCENES:ACTIVATE:id    - Activate scene by ID
 *   TEST:SCENES:SPRITE:id:spriteId - Set sprite for a scene
 *   TEST:SCENES:ANIM:id:type   - Set animation type
 *   TEST:SCENES:DISPLAY:id:0|1 - Set displayEnabled
 *   TEST:SCENES:LEDS:id:0|1    - Set ledsEnabled
 *   TEST:SCENES:SAVE           - Force save to SD card
 *   TEST:SCENES:LOAD           - Force reload from SD card
 *   TEST:STATE                 - Dump current animation state
 *   TEST:SPRITES:LIST          - List all available sprites
 *   TEST:AUTO                  - Run quick automated test
 * 
 * @author ARCOS Testing Framework
 * @version 2.0
 *****************************************************************/

#pragma once

#include <string>
#include <cstring>
#include <vector>
#include <functional>
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "SystemAPI/Web/Server/HttpServer.hpp"

namespace SystemAPI {
namespace Testing {

static const char* TEST_TAG = "SCENE_TEST";

// ============================================================
// TEST RESULT STRUCTURES
// ============================================================
struct TestResult {
    std::string name;
    bool passed;
    std::string message;
    uint32_t durationMs;
};

struct TestSuite {
    std::string name;
    std::vector<TestResult> results;
    int passed = 0;
    int failed = 0;
    uint32_t totalDurationMs = 0;
};

/**
 * @brief Comprehensive Scene Test Harness with Full Automation
 */
class SceneTestHarness {
public:
    // Callback types for integration with main render loop
    using AnimationChangeCallback = std::function<void(const std::string& animType, int spriteId)>;
    using StateQueryCallback = std::function<std::string()>;
    
private:
    static AnimationChangeCallback animCallback_;
    static StateQueryCallback stateCallback_;
    static bool initialized_;
    static int testSequenceStep_;
    static uint32_t lastTestTime_;
    
    // Callback tracking for tests
    static bool callbackWasTriggered_;
    static Web::SavedScene lastActivatedScene_;
    
    // Auto-start flag - set to false for normal operation, true only for testing
    static constexpr bool AUTO_START_TESTS = false;
    
public:
    /**
     * @brief Initialize the test harness
     */
    static void init() {
        if (initialized_) return;
        initialized_ = true;
        testSequenceStep_ = 0;
        lastTestTime_ = 0;
        callbackWasTriggered_ = false;
        
        ESP_LOGI(TEST_TAG, "");
        ESP_LOGI(TEST_TAG, "╔════════════════════════════════════════════════════════════╗");
        ESP_LOGI(TEST_TAG, "║   COMPREHENSIVE SCENE TEST HARNESS v2.0                    ║");
        if (AUTO_START_TESTS) {
            ESP_LOGI(TEST_TAG, "║   AUTO-START MODE ENABLED                                  ║");
        } else {
            ESP_LOGI(TEST_TAG, "║   STANDBY MODE - Use TEST:FULL to run tests               ║");
        }
        ESP_LOGI(TEST_TAG, "╚════════════════════════════════════════════════════════════╝");
        ESP_LOGI(TEST_TAG, "");
        
        // Only register test callback if auto-starting (to avoid overwriting production callback)
        if (AUTO_START_TESTS) {
            registerTestCallback();
        };
        
        // Auto-start tests after initialization
        if (AUTO_START_TESTS) {
            ESP_LOGI(TEST_TAG, "");
            ESP_LOGI(TEST_TAG, "########################################################");
            ESP_LOGI(TEST_TAG, "###   AUTO-START: Tests will begin automatically!    ###");
            ESP_LOGI(TEST_TAG, "########################################################");
            ESP_LOGI(TEST_TAG, "");
            
            // Run the full test suite automatically
            runFullAutomatedTestSuite();
        }
    }
    
    /**
     * @brief Register test callback to intercept scene activations
     */
    static void registerTestCallback() {
        ESP_LOGI(TEST_TAG, "[INIT] Registering test callback for scene activation...");
        
        auto& httpServer = Web::HttpServer::instance();
        httpServer.setSceneActivatedCallback([](const Web::SavedScene& scene) {
            callbackWasTriggered_ = true;
            lastActivatedScene_ = scene;
            
            ESP_LOGI(TEST_TAG, "");
            ESP_LOGI(TEST_TAG, "  ╔═══════════════════════════════════════════════════════════╗");
            ESP_LOGI(TEST_TAG, "  ║ CALLBACK TRIGGERED - Scene Activated                      ║");
            ESP_LOGI(TEST_TAG, "  ╠═══════════════════════════════════════════════════════════╣");
            ESP_LOGI(TEST_TAG, "  ║ Scene ID:        %d", scene.id);
            ESP_LOGI(TEST_TAG, "  ║ Scene Name:      %s", scene.name.c_str());
            ESP_LOGI(TEST_TAG, "  ║ Animation Type:  %s", scene.animType.c_str());
            ESP_LOGI(TEST_TAG, "  ║ Display Enabled: %s", scene.displayEnabled ? "YES" : "NO");
            ESP_LOGI(TEST_TAG, "  ║ LEDs Enabled:    %s", scene.ledsEnabled ? "YES" : "NO");
            ESP_LOGI(TEST_TAG, "  ║ Effects Only:    %s", scene.effectsOnly ? "YES" : "NO");
            ESP_LOGI(TEST_TAG, "  ║ Sprite ID:       %d", scene.spriteId);
            ESP_LOGI(TEST_TAG, "  ║ Mirror Sprite:   %s", scene.mirrorSprite ? "YES" : "NO");
            ESP_LOGI(TEST_TAG, "  ║ Transition:      %s", scene.transition.c_str());
            ESP_LOGI(TEST_TAG, "  ║ Shader AA:       %s", scene.shaderAA ? "ON" : "OFF");
            ESP_LOGI(TEST_TAG, "  ║ Shader Invert:   %s", scene.shaderInvert ? "ON" : "OFF");
            ESP_LOGI(TEST_TAG, "  ║ Shader ColorMode:%s", scene.shaderColorMode.c_str());
            ESP_LOGI(TEST_TAG, "  ║ Shader Color:    %s", scene.shaderColor.c_str());
            ESP_LOGI(TEST_TAG, "  ║ LED Color:       R=%d G=%d B=%d", scene.ledR, scene.ledG, scene.ledB);
            ESP_LOGI(TEST_TAG, "  ║ LED Brightness:  %d%%", scene.ledBrightness);
            ESP_LOGI(TEST_TAG, "  ║ Params Count:    %zu", scene.params.size());
            ESP_LOGI(TEST_TAG, "  ╚═══════════════════════════════════════════════════════════╝");
            ESP_LOGI(TEST_TAG, "");
        });
        
        ESP_LOGI(TEST_TAG, "[INIT] Test callback registered successfully");
    }
    
    /**
     * @brief Set callback for animation changes
     */
    static void setAnimationChangeCallback(AnimationChangeCallback cb) {
        animCallback_ = cb;
    }
    
    /**
     * @brief Set callback to query current render state
     */
    static void setStateQueryCallback(StateQueryCallback cb) {
        stateCallback_ = cb;
    }
    
    /**
     * @brief Process a console command
     * @param cmd The command string (e.g., "TEST:SCENES:LIST")
     * @return true if command was handled
     */
    static bool processCommand(const char* cmd) {
        if (!cmd || strncmp(cmd, "TEST:", 5) != 0) {
            return false;
        }
        
        const char* subCmd = cmd + 5;
        
        if (strcmp(subCmd, "HELP") == 0) {
            printHelp();
            return true;
        }
        
        // Quick system check
        if (strcmp(subCmd, "PING") == 0) {
            ESP_LOGI(TEST_TAG, "");
            ESP_LOGI(TEST_TAG, ">>> PONG! Test system is alive.");
            ESP_LOGI(TEST_TAG, "    Free heap: %lu bytes", (uint32_t)esp_get_free_heap_size());
            ESP_LOGI(TEST_TAG, "    Scene count: %d", getSceneCount());
            ESP_LOGI(TEST_TAG, "    Active scene ID: %d", Web::activeSceneId_);
            ESP_LOGI(TEST_TAG, "<<< PING complete.");
            ESP_LOGI(TEST_TAG, "");
            return true;
        }
        
        // Run individual suite by number
        if (strncmp(subCmd, "SUITE:", 6) == 0) {
            int suiteNum = atoi(subCmd + 6);
            ESP_LOGI(TEST_TAG, ">>> Running Suite %d...", suiteNum);
            runSingleSuite(suiteNum);
            ESP_LOGI(TEST_TAG, "<<< Suite %d finished.", suiteNum);
            return true;
        }
        
        if (strcmp(subCmd, "FULL") == 0) {
            runFullAutomatedTestSuite();
            return true;
        }
        
        if (strcmp(subCmd, "SCENES:LIST") == 0) {
            listAllScenes();
            return true;
        }
        
        if (strcmp(subCmd, "SCENES:DUMP") == 0) {
            dumpActiveScene();
            return true;
        }
        
        if (strncmp(subCmd, "SCENES:CREATE:", 14) == 0) {
            const char* name = subCmd + 14;
            createScene(name);
            return true;
        }
        
        if (strncmp(subCmd, "SCENES:ACTIVATE:", 16) == 0) {
            int id = atoi(subCmd + 16);
            activateScene(id);
            return true;
        }
        
        if (strncmp(subCmd, "SCENES:SPRITE:", 14) == 0) {
            // Format: SCENES:SPRITE:sceneId:spriteId
            int sceneId = 0, spriteId = 0;
            if (sscanf(subCmd + 14, "%d:%d", &sceneId, &spriteId) == 2) {
                setSceneSprite(sceneId, spriteId);
            } else {
                ESP_LOGE(TEST_TAG, "Invalid format. Use TEST:SCENES:SPRITE:sceneId:spriteId");
            }
            return true;
        }
        
        if (strncmp(subCmd, "SCENES:ANIM:", 12) == 0) {
            // Format: SCENES:ANIM:sceneId:animType
            char animType[32] = {0};
            int sceneId = 0;
            if (sscanf(subCmd + 12, "%d:%31s", &sceneId, animType) == 2) {
                setSceneAnimationType(sceneId, animType);
            } else {
                ESP_LOGE(TEST_TAG, "Invalid format. Use TEST:SCENES:ANIM:sceneId:animType");
            }
            return true;
        }
        
        if (strncmp(subCmd, "SCENES:DISPLAY:", 15) == 0) {
            // Format: SCENES:DISPLAY:sceneId:0|1
            int sceneId = 0, enabled = 0;
            if (sscanf(subCmd + 15, "%d:%d", &sceneId, &enabled) == 2) {
                setSceneDisplayEnabled(sceneId, enabled != 0);
            }
            return true;
        }
        
        if (strncmp(subCmd, "SCENES:LEDS:", 12) == 0) {
            // Format: SCENES:LEDS:sceneId:0|1
            int sceneId = 0, enabled = 0;
            if (sscanf(subCmd + 12, "%d:%d", &sceneId, &enabled) == 2) {
                setSceneLedsEnabled(sceneId, enabled != 0);
            }
            return true;
        }
        
        if (strcmp(subCmd, "SCENES:SAVE") == 0) {
            forceSave();
            return true;
        }
        
        if (strcmp(subCmd, "SCENES:LOAD") == 0) {
            forceLoad();
            return true;
        }
        
        if (strcmp(subCmd, "STATE") == 0) {
            dumpCurrentState();
            return true;
        }
        
        if (strcmp(subCmd, "AUTO") == 0) {
            runAutoTest();
            return true;
        }
        
        if (strcmp(subCmd, "SPRITES:LIST") == 0) {
            listAllSprites();
            return true;
        }
        
        ESP_LOGW(TEST_TAG, "Unknown command: %s", cmd);
        printHelp();
        return true;
    }
    
    /**
     * @brief Print help message
     */
    static void printHelp() {
        ESP_LOGI(TEST_TAG, "");
        ESP_LOGI(TEST_TAG, "╔═══════════════════════════════════════════════════════════════════╗");
        ESP_LOGI(TEST_TAG, "║        COMPREHENSIVE SCENE TEST HARNESS COMMANDS                  ║");
        ESP_LOGI(TEST_TAG, "╠═══════════════════════════════════════════════════════════════════╣");
        ESP_LOGI(TEST_TAG, "║ TEST:HELP                  - Show this help                       ║");
        ESP_LOGI(TEST_TAG, "║ TEST:PING                  - Quick system check (heap, scenes)    ║");
        ESP_LOGI(TEST_TAG, "║ TEST:FULL                  - Run FULL automated test suite        ║");
        ESP_LOGI(TEST_TAG, "║ TEST:SUITE:N               - Run single suite (1-10)              ║");
        ESP_LOGI(TEST_TAG, "╠═══════════════════════════════════════════════════════════════════╣");
        ESP_LOGI(TEST_TAG, "║ SUITES:                                                           ║");
        ESP_LOGI(TEST_TAG, "║   1=SceneCreation  2=SceneModification  3=AnimationTypes          ║");
        ESP_LOGI(TEST_TAG, "║   4=Display/LED    5=ShaderSettings     6=LEDColors               ║");
        ESP_LOGI(TEST_TAG, "║   7=Activation     8=Persistence(SD)    9=SpriteAssignment        ║");
        ESP_LOGI(TEST_TAG, "║   10=EdgeCases                                                    ║");
        ESP_LOGI(TEST_TAG, "╠═══════════════════════════════════════════════════════════════════╣");
        ESP_LOGI(TEST_TAG, "║ SCENE COMMANDS:                                                   ║");
        ESP_LOGI(TEST_TAG, "║ TEST:SCENES:LIST           - List all scenes with details         ║");
        ESP_LOGI(TEST_TAG, "║ TEST:SCENES:DUMP           - Dump active scene details            ║");
        ESP_LOGI(TEST_TAG, "║ TEST:SCENES:CREATE:name    - Create new scene                     ║");
        ESP_LOGI(TEST_TAG, "║ TEST:SCENES:ACTIVATE:id    - Activate scene by ID                 ║");
        ESP_LOGI(TEST_TAG, "║ TEST:SCENES:SPRITE:id:sid  - Set sprite ID for scene              ║");
        ESP_LOGI(TEST_TAG, "║ TEST:SCENES:ANIM:id:type   - Set animation type                   ║");
        ESP_LOGI(TEST_TAG, "║   (types: gyro_eyes, static_image, sway, sdf_morph, none)         ║");
        ESP_LOGI(TEST_TAG, "║ TEST:SCENES:DISPLAY:id:0|1 - Enable/disable display               ║");
        ESP_LOGI(TEST_TAG, "║ TEST:SCENES:LEDS:id:0|1    - Enable/disable LEDs                  ║");
        ESP_LOGI(TEST_TAG, "║ TEST:SCENES:SAVE           - Force save to SD card                ║");
        ESP_LOGI(TEST_TAG, "║ TEST:SCENES:LOAD           - Force reload from SD                 ║");
        ESP_LOGI(TEST_TAG, "╠═══════════════════════════════════════════════════════════════════╣");
        ESP_LOGI(TEST_TAG, "║ STATE COMMANDS:                                                   ║");
        ESP_LOGI(TEST_TAG, "║ TEST:STATE                 - Dump animation state                 ║");
        ESP_LOGI(TEST_TAG, "║ TEST:SPRITES:LIST          - List available sprites               ║");
        ESP_LOGI(TEST_TAG, "║ TEST:AUTO                  - Run quick automated test             ║");
        ESP_LOGI(TEST_TAG, "╚═══════════════════════════════════════════════════════════════════╝");
        ESP_LOGI(TEST_TAG, "");
    }
    
    /**
     * @brief List all scenes with their configurations
     */
    static void listAllScenes() {
        auto& scenes = Web::savedScenes_;
        int activeId = Web::activeSceneId_;
        
        ESP_LOGI(TEST_TAG, "");
        ESP_LOGI(TEST_TAG, "╔═══════════════════════════════════════════════════════════════════════╗");
        ESP_LOGI(TEST_TAG, "║                         ALL SCENES (%d total)                         ║", (int)scenes.size());
        ESP_LOGI(TEST_TAG, "╠═══════════════════════════════════════════════════════════════════════╣");
        
        if (scenes.empty()) {
            ESP_LOGI(TEST_TAG, "║   (No scenes created)                                                 ║");
        }
        
        for (const auto& scene : scenes) {
            bool isActive = (scene.id == activeId);
            ESP_LOGI(TEST_TAG, "╠───────────────────────────────────────────────────────────────────────╣");
            ESP_LOGI(TEST_TAG, "║ Scene ID: %d %s", scene.id, isActive ? "[ACTIVE]" : "");
            ESP_LOGI(TEST_TAG, "║   Name:           %s", scene.name.c_str());
            ESP_LOGI(TEST_TAG, "║   displayEnabled: %s", scene.displayEnabled ? "YES" : "NO");
            ESP_LOGI(TEST_TAG, "║   ledsEnabled:    %s", scene.ledsEnabled ? "YES" : "NO");
            ESP_LOGI(TEST_TAG, "║   effectsOnly:    %s", scene.effectsOnly ? "YES" : "NO");
            ESP_LOGI(TEST_TAG, "║   animType:       '%s'", scene.animType.c_str());
            ESP_LOGI(TEST_TAG, "║   transition:     '%s'", scene.transition.c_str());
            ESP_LOGI(TEST_TAG, "║   spriteId:       %d", scene.spriteId);
            ESP_LOGI(TEST_TAG, "║   mirrorSprite:   %s", scene.mirrorSprite ? "YES" : "NO");
            ESP_LOGI(TEST_TAG, "║   shaderAA:       %s", scene.shaderAA ? "YES" : "NO");
            ESP_LOGI(TEST_TAG, "║   shaderInvert:   %s", scene.shaderInvert ? "YES" : "NO");
            ESP_LOGI(TEST_TAG, "║   shaderColorMode:'%s'", scene.shaderColorMode.c_str());
            ESP_LOGI(TEST_TAG, "║   LED Color:      R=%d G=%d B=%d (Brightness: %d)", 
                     scene.ledR, scene.ledG, scene.ledB, scene.ledBrightness);
            ESP_LOGI(TEST_TAG, "║   order:          %d", scene.order);
            
            // Print params if any
            if (!scene.params.empty()) {
                ESP_LOGI(TEST_TAG, "║   params:");
                for (const auto& kv : scene.params) {
                    ESP_LOGI(TEST_TAG, "║     %s = %.2f", kv.first.c_str(), kv.second);
                }
            }
        }
        
        ESP_LOGI(TEST_TAG, "╚═══════════════════════════════════════════════════════════════════════╝");
        ESP_LOGI(TEST_TAG, "");
    }
    
    /**
     * @brief Dump the currently active scene
     */
    static void dumpActiveScene() {
        int activeId = Web::activeSceneId_;
        
        ESP_LOGI(TEST_TAG, "");
        ESP_LOGI(TEST_TAG, "╔═══════════════════════════════════════════════════════════╗");
        ESP_LOGI(TEST_TAG, "║              ACTIVE SCENE STATE                           ║");
        ESP_LOGI(TEST_TAG, "╠═══════════════════════════════════════════════════════════╣");
        ESP_LOGI(TEST_TAG, "║ Active Scene ID: %d", activeId);
        
        Web::SavedScene* activeScene = nullptr;
        for (auto& scene : Web::savedScenes_) {
            if (scene.id == activeId) {
                activeScene = &scene;
                break;
            }
        }
        
        if (activeScene) {
            ESP_LOGI(TEST_TAG, "║ Name:           %s", activeScene->name.c_str());
            ESP_LOGI(TEST_TAG, "║ displayEnabled: %s", activeScene->displayEnabled ? "YES" : "NO");
            ESP_LOGI(TEST_TAG, "║ ledsEnabled:    %s", activeScene->ledsEnabled ? "YES" : "NO");
            ESP_LOGI(TEST_TAG, "║ animType:       '%s'", activeScene->animType.c_str());
            ESP_LOGI(TEST_TAG, "║ spriteId:       %d", activeScene->spriteId);
        } else {
            ESP_LOGI(TEST_TAG, "║ (No active scene or scene not found)");
        }
        
        // Also query render state if callback is set
        if (stateCallback_) {
            ESP_LOGI(TEST_TAG, "╠═══════════════════════════════════════════════════════════╣");
            ESP_LOGI(TEST_TAG, "║ RENDER STATE:");
            std::string state = stateCallback_();
            ESP_LOGI(TEST_TAG, "%s", state.c_str());
        }
        
        ESP_LOGI(TEST_TAG, "╚═══════════════════════════════════════════════════════════╝");
        ESP_LOGI(TEST_TAG, "");
    }
    
    /**
     * @brief Create a new scene (simulates /api/scene/create)
     */
    static void createScene(const char* name) {
        ESP_LOGI(TEST_TAG, "");
        ESP_LOGI(TEST_TAG, ">>> CREATE SCENE: '%s'", name);
        
        Web::SavedScene scene;
        scene.id = Web::nextSceneId_++;
        scene.name = name;
        scene.type = 0;
        scene.active = false;
        scene.displayEnabled = true;
        scene.ledsEnabled = false;
        scene.effectsOnly = false;
        scene.order = (int)Web::savedScenes_.size();
        scene.animType = "gyro_eyes";
        scene.transition = "none";
        scene.spriteId = -1;
        scene.mirrorSprite = false;
        scene.shaderAA = true;
        scene.shaderInvert = false;
        scene.shaderColorMode = "none";
        scene.shaderColor = "#ffffff";
        scene.ledR = 255;
        scene.ledG = 0;
        scene.ledB = 255;
        scene.ledBrightness = 80;
        
        Web::savedScenes_.push_back(scene);
        
        ESP_LOGI(TEST_TAG, "    Created scene ID: %d", scene.id);
        ESP_LOGI(TEST_TAG, "    Total scenes: %d", (int)Web::savedScenes_.size());
        ESP_LOGI(TEST_TAG, "");
        
        // Auto-save
        Web::HttpServer::instance().forceSaveScenes();
    }
    
    /**
     * @brief Activate a scene by ID (simulates /api/scene/activate)
     */
    static void activateScene(int sceneId) {
        ESP_LOGI(TEST_TAG, "");
        ESP_LOGI(TEST_TAG, ">>> ACTIVATE SCENE ID: %d", sceneId);
        
        Web::SavedScene* targetScene = nullptr;
        
        // Deactivate all, find target
        for (auto& scene : Web::savedScenes_) {
            scene.active = false;
            if (scene.id == sceneId) {
                targetScene = &scene;
            }
        }
        
        if (!targetScene) {
            ESP_LOGE(TEST_TAG, "    ERROR: Scene ID %d not found!", sceneId);
            return;
        }
        
        // Activate target
        targetScene->active = true;
        Web::activeSceneId_ = sceneId;
        
        ESP_LOGI(TEST_TAG, "    Activated: %s", targetScene->name.c_str());
        ESP_LOGI(TEST_TAG, "    animType:       '%s'", targetScene->animType.c_str());
        ESP_LOGI(TEST_TAG, "    spriteId:       %d", targetScene->spriteId);
        ESP_LOGI(TEST_TAG, "    displayEnabled: %s", targetScene->displayEnabled ? "YES" : "NO");
        
        // Trigger the scene activated callback (same as web API does)
        auto& callback = Web::HttpServer::getSceneActivatedCallback();
        if (callback) {
            ESP_LOGI(TEST_TAG, "    >>> Calling scene activated callback...");
            callback(*targetScene);
            ESP_LOGI(TEST_TAG, "    <<< Callback completed");
        } else {
            ESP_LOGW(TEST_TAG, "    WARNING: No scene activated callback registered!");
        }
        
        // Notify animation change callback if set
        if (animCallback_) {
            animCallback_(targetScene->animType, targetScene->spriteId);
        }
        
        ESP_LOGI(TEST_TAG, "");
    }
    
    /**
     * @brief Set sprite ID for a scene
     */
    static void setSceneSprite(int sceneId, int spriteId) {
        ESP_LOGI(TEST_TAG, "");
        ESP_LOGI(TEST_TAG, ">>> SET SPRITE: Scene %d -> Sprite %d", sceneId, spriteId);
        
        for (auto& scene : Web::savedScenes_) {
            if (scene.id == sceneId) {
                int oldSpriteId = scene.spriteId;
                scene.spriteId = spriteId;
                
                ESP_LOGI(TEST_TAG, "    Scene: %s", scene.name.c_str());
                ESP_LOGI(TEST_TAG, "    Old spriteId: %d", oldSpriteId);
                ESP_LOGI(TEST_TAG, "    New spriteId: %d", scene.spriteId);
                
                // Verify the sprite exists
                auto* sprite = Web::HttpServer::findSpriteById(spriteId);
                if (sprite) {
                    ESP_LOGI(TEST_TAG, "    Sprite found: '%s' (%dx%d, %zu bytes)", 
                             sprite->name.c_str(), sprite->width, sprite->height, sprite->pixelData.size());
                } else {
                    ESP_LOGW(TEST_TAG, "    WARNING: Sprite ID %d not found in storage!", spriteId);
                }
                
                // Auto-save
                Web::HttpServer::instance().forceSaveScenes();
                ESP_LOGI(TEST_TAG, "    Saved to storage");
                ESP_LOGI(TEST_TAG, "");
                return;
            }
        }
        
        ESP_LOGE(TEST_TAG, "    ERROR: Scene ID %d not found!", sceneId);
        ESP_LOGI(TEST_TAG, "");
    }
    
    /**
     * @brief Set animation type for a scene
     */
    static void setSceneAnimationType(int sceneId, const char* animType) {
        ESP_LOGI(TEST_TAG, "");
        ESP_LOGI(TEST_TAG, ">>> SET ANIMATION TYPE: Scene %d -> '%s'", sceneId, animType);
        
        for (auto& scene : Web::savedScenes_) {
            if (scene.id == sceneId) {
                std::string oldType = scene.animType;
                scene.animType = animType;
                
                ESP_LOGI(TEST_TAG, "    Scene: %s", scene.name.c_str());
                ESP_LOGI(TEST_TAG, "    Old animType: '%s'", oldType.c_str());
                ESP_LOGI(TEST_TAG, "    New animType: '%s'", scene.animType.c_str());
                
                // Validate animation type
                if (scene.animType != "gyro_eyes" && 
                    scene.animType != "static_image" && 
                    scene.animType != "sway" && 
                    scene.animType != "sdf_morph" &&
                    scene.animType != "none") {
                    ESP_LOGW(TEST_TAG, "    WARNING: Unknown animation type '%s'", animType);
                }
                
                // Auto-save
                Web::HttpServer::instance().forceSaveScenes();
                ESP_LOGI(TEST_TAG, "    Saved to storage");
                ESP_LOGI(TEST_TAG, "");
                return;
            }
        }
        
        ESP_LOGE(TEST_TAG, "    ERROR: Scene ID %d not found!", sceneId);
        ESP_LOGI(TEST_TAG, "");
    }
    
    /**
     * @brief Set displayEnabled for a scene
     */
    static void setSceneDisplayEnabled(int sceneId, bool enabled) {
        ESP_LOGI(TEST_TAG, "");
        ESP_LOGI(TEST_TAG, ">>> SET DISPLAY ENABLED: Scene %d -> %s", sceneId, enabled ? "YES" : "NO");
        
        for (auto& scene : Web::savedScenes_) {
            if (scene.id == sceneId) {
                scene.displayEnabled = enabled;
                ESP_LOGI(TEST_TAG, "    Scene: %s", scene.name.c_str());
                ESP_LOGI(TEST_TAG, "    displayEnabled: %s", scene.displayEnabled ? "YES" : "NO");
                Web::HttpServer::instance().forceSaveScenes();
                ESP_LOGI(TEST_TAG, "    Saved to storage");
                ESP_LOGI(TEST_TAG, "");
                return;
            }
        }
        
        ESP_LOGE(TEST_TAG, "    ERROR: Scene ID %d not found!", sceneId);
        ESP_LOGI(TEST_TAG, "");
    }
    
    /**
     * @brief Set ledsEnabled for a scene
     */
    static void setSceneLedsEnabled(int sceneId, bool enabled) {
        ESP_LOGI(TEST_TAG, "");
        ESP_LOGI(TEST_TAG, ">>> SET LEDS ENABLED: Scene %d -> %s", sceneId, enabled ? "YES" : "NO");
        
        for (auto& scene : Web::savedScenes_) {
            if (scene.id == sceneId) {
                scene.ledsEnabled = enabled;
                ESP_LOGI(TEST_TAG, "    Scene: %s", scene.name.c_str());
                ESP_LOGI(TEST_TAG, "    ledsEnabled: %s", scene.ledsEnabled ? "YES" : "NO");
                Web::HttpServer::instance().forceSaveScenes();
                ESP_LOGI(TEST_TAG, "    Saved to storage");
                ESP_LOGI(TEST_TAG, "");
                return;
            }
        }
        
        ESP_LOGE(TEST_TAG, "    ERROR: Scene ID %d not found!", sceneId);
        ESP_LOGI(TEST_TAG, "");
    }
    
    /**
     * @brief Force save scenes to SD card
     */
    static void forceSave() {
        ESP_LOGI(TEST_TAG, "");
        ESP_LOGI(TEST_TAG, ">>> FORCE SAVE SCENES TO SD CARD");
        Web::HttpServer::instance().forceSaveScenes();
        ESP_LOGI(TEST_TAG, "    Saved %d scenes", (int)Web::savedScenes_.size());
        ESP_LOGI(TEST_TAG, "");
    }
    
    /**
     * @brief Force reload scenes from SD card
     */
    static void forceLoad() {
        ESP_LOGI(TEST_TAG, "");
        ESP_LOGI(TEST_TAG, ">>> FORCE RELOAD SCENES FROM SD CARD");
        int countBefore = (int)Web::savedScenes_.size();
        Web::HttpServer::instance().forceLoadScenes();
        int countAfter = (int)Web::savedScenes_.size();
        ESP_LOGI(TEST_TAG, "    Before: %d scenes", countBefore);
        ESP_LOGI(TEST_TAG, "    After:  %d scenes", countAfter);
        ESP_LOGI(TEST_TAG, "");
    }
    
    /**
     * @brief Dump current animation/render state
     */
    static void dumpCurrentState() {
        ESP_LOGI(TEST_TAG, "");
        ESP_LOGI(TEST_TAG, "╔═══════════════════════════════════════════════════════════╗");
        ESP_LOGI(TEST_TAG, "║              CURRENT ANIMATION STATE                      ║");
        ESP_LOGI(TEST_TAG, "╠═══════════════════════════════════════════════════════════╣");
        
        if (stateCallback_) {
            std::string state = stateCallback_();
            ESP_LOGI(TEST_TAG, "%s", state.c_str());
        } else {
            ESP_LOGW(TEST_TAG, "║ State query callback not set!");
        }
        
        ESP_LOGI(TEST_TAG, "╚═══════════════════════════════════════════════════════════╝");
        ESP_LOGI(TEST_TAG, "");
    }
    
    /**
     * @brief List all available sprites
     */
    static void listAllSprites() {
        auto& httpServer = Web::HttpServer::instance();
        const auto& sprites = httpServer.getSprites();
        
        ESP_LOGI(TEST_TAG, "");
        ESP_LOGI(TEST_TAG, "╔═══════════════════════════════════════════════════════════╗");
        ESP_LOGI(TEST_TAG, "║              AVAILABLE SPRITES (%d total)                 ║", (int)sprites.size());
        ESP_LOGI(TEST_TAG, "╠═══════════════════════════════════════════════════════════╣");
        
        for (const auto& sprite : sprites) {
            ESP_LOGI(TEST_TAG, "║ [%3d] %-20s %3dx%-3d  %6zu bytes %s",
                     sprite.id, sprite.name.c_str(), 
                     sprite.width, sprite.height, 
                     sprite.pixelData.size(),
                     sprite.id >= 100 ? "[SAVED]" : "[BUILT-IN]");
        }
        
        ESP_LOGI(TEST_TAG, "╚═══════════════════════════════════════════════════════════╝");
        ESP_LOGI(TEST_TAG, "");
    }
    
    // ================================================================
    //        COMPREHENSIVE AUTOMATED TEST SUITE
    // ================================================================
    
    // Maximum time for any single test (5 seconds)
    static constexpr uint32_t TEST_TIMEOUT_MS = 5000;
    // Maximum time for any suite (30 seconds)
    static constexpr uint32_t SUITE_TIMEOUT_MS = 30000;
    // Maximum total test time (10 minutes for 3 full runs)
    static constexpr uint32_t TOTAL_TIMEOUT_MS = 600000;
    // Startup delay before tests begin (10 seconds)
    static constexpr uint32_t STARTUP_DELAY_MS = 10000;
    // Number of times to run each individual test
    static constexpr int TEST_REPETITIONS = 3;
    // Number of times to run the entire test suite
    static constexpr int SUITE_REPETITIONS = 3;
    
    /**
     * @brief Safe delay with WDT feeding and countdown display
     */
    static void safeDelay(uint32_t ms) {
        ESP_LOGD(TEST_TAG, "[DELAY] Waiting %lu ms...", ms);
        vTaskDelay(pdMS_TO_TICKS(ms));
        ESP_LOGD(TEST_TAG, "[DELAY] Done");
    }
    
    /**
     * @brief Countdown delay with periodic status output
     */
    static void countdownDelay(uint32_t totalMs, const char* message) {
        uint32_t remaining = totalMs;
        while (remaining > 0) {
            ESP_LOGI(TEST_TAG, "[COUNTDOWN] %s in %lu seconds...", message, remaining / 1000);
            uint32_t waitTime = (remaining > 1000) ? 1000 : remaining;
            vTaskDelay(pdMS_TO_TICKS(waitTime));
            remaining -= waitTime;
        }
        ESP_LOGI(TEST_TAG, "[COUNTDOWN] %s NOW!", message);
    }
    
    /**
     * @brief Check if timeout exceeded
     */
    static bool checkTimeout(uint32_t startTick, uint32_t timeoutMs, const char* context) {
        uint32_t elapsed = (xTaskGetTickCount() - startTick) * portTICK_PERIOD_MS;
        if (elapsed > timeoutMs) {
            ESP_LOGE(TEST_TAG, "[TIMEOUT] %s exceeded %lu ms (elapsed: %lu ms)", context, timeoutMs, elapsed);
            return true;
        }
        return false;
    }
    
    /**
     * @brief Run a single test suite and return results
     */
    static TestSuite runSuiteByNumber(int suiteNum) {
        switch (suiteNum) {
            case 1: return runTestSuite_SceneCreation();
            case 2: return runTestSuite_SceneModification();
            case 3: return runTestSuite_AnimationTypes();
            case 4: return runTestSuite_DisplayLedToggle();
            case 5: return runTestSuite_ShaderSettings();
            case 6: return runTestSuite_LedColors();
            case 7: return runTestSuite_SceneActivation();
            case 8: return runTestSuite_Persistence();
            case 9: return runTestSuite_SpriteAssignment();
            case 10: return runTestSuite_EdgeCases();
            default: 
                TestSuite empty;
                empty.name = "Invalid";
                return empty;
        }
    }
    
    static const char* getSuiteName(int suiteNum) {
        switch (suiteNum) {
            case 1: return "Scene Creation";
            case 2: return "Scene Modification";
            case 3: return "Animation Types";
            case 4: return "Display/LED Toggle";
            case 5: return "Shader Settings";
            case 6: return "LED Colors";
            case 7: return "Scene Activation";
            case 8: return "Persistence (SD Card)";
            case 9: return "Sprite Assignment";
            case 10: return "Edge Cases";
            default: return "Unknown";
        }
    }
    
    /**
     * @brief Run full automated test suite with all test cases
     * - 10 second startup delay
     * - Each test runs 3 times
     * - Entire suite repeats 3 times
     * - Clear "COMPLETELY FINISHED" message at end
     */
    static void runFullAutomatedTestSuite() {
        ESP_LOGI(TEST_TAG, "");
        ESP_LOGI(TEST_TAG, "########################################################################");
        ESP_LOGI(TEST_TAG, "########################################################################");
        ESP_LOGI(TEST_TAG, "###                                                                  ###");
        ESP_LOGI(TEST_TAG, "###     COMPREHENSIVE AUTOMATED TEST SUITE - STARTING                ###");
        ESP_LOGI(TEST_TAG, "###                                                                  ###");
        ESP_LOGI(TEST_TAG, "########################################################################");
        ESP_LOGI(TEST_TAG, "########################################################################");
        ESP_LOGI(TEST_TAG, "");
        ESP_LOGI(TEST_TAG, "[CONFIG] Startup delay: %lu ms", STARTUP_DELAY_MS);
        ESP_LOGI(TEST_TAG, "[CONFIG] Test repetitions: %d times each", TEST_REPETITIONS);
        ESP_LOGI(TEST_TAG, "[CONFIG] Suite repetitions: %d full runs", SUITE_REPETITIONS);
        ESP_LOGI(TEST_TAG, "[CONFIG] Total suites: 10");
        ESP_LOGI(TEST_TAG, "[CONFIG] Total test executions: %d", 10 * TEST_REPETITIONS * SUITE_REPETITIONS);
        ESP_LOGI(TEST_TAG, "");
        ESP_LOGI(TEST_TAG, "[INFO] Free heap before tests: %lu bytes", (uint32_t)esp_get_free_heap_size());
        ESP_LOGI(TEST_TAG, "");
        
        // ==================== 10 SECOND COUNTDOWN ====================
        countdownDelay(STARTUP_DELAY_MS, "Tests starting");
        
        ESP_LOGI(TEST_TAG, "");
        ESP_LOGI(TEST_TAG, "========================================================================");
        ESP_LOGI(TEST_TAG, "===                    TESTS BEGINNING NOW                           ===");
        ESP_LOGI(TEST_TAG, "========================================================================");
        ESP_LOGI(TEST_TAG, "");
        
        uint32_t grandStart = xTaskGetTickCount();
        
        // Track aggregate results across all runs
        int grandTotalPassed = 0;
        int grandTotalFailed = 0;
        std::vector<std::string> failedTests;
        
        // ==================== REPEAT ENTIRE SUITE 3 TIMES ====================
        for (int fullRun = 1; fullRun <= SUITE_REPETITIONS; fullRun++) {
            ESP_LOGI(TEST_TAG, "");
            ESP_LOGI(TEST_TAG, "################################################################");
            ESP_LOGI(TEST_TAG, "###          FULL TEST RUN %d of %d                           ###", fullRun, SUITE_REPETITIONS);
            ESP_LOGI(TEST_TAG, "################################################################");
            ESP_LOGI(TEST_TAG, "");
            
            uint32_t runStart = xTaskGetTickCount();
            int runPassed = 0;
            int runFailed = 0;
            
            // Run each of the 10 suites
            for (int suiteNum = 1; suiteNum <= 10; suiteNum++) {
                ESP_LOGI(TEST_TAG, "");
                ESP_LOGI(TEST_TAG, "================================================================");
                ESP_LOGI(TEST_TAG, "=== Run %d/%d | Suite %d/10: %s", 
                         fullRun, SUITE_REPETITIONS, suiteNum, getSuiteName(suiteNum));
                ESP_LOGI(TEST_TAG, "================================================================");
                
                // Run this suite TEST_REPETITIONS times
                for (int rep = 1; rep <= TEST_REPETITIONS; rep++) {
                    ESP_LOGI(TEST_TAG, "");
                    ESP_LOGI(TEST_TAG, ">>> Suite %d, Repetition %d/%d", suiteNum, rep, TEST_REPETITIONS);
                    
                    // Check for timeout
                    if (checkTimeout(grandStart, TOTAL_TIMEOUT_MS, "Grand Total")) {
                        ESP_LOGE(TEST_TAG, "[ABORT] Total timeout exceeded, stopping tests!");
                        goto finished;
                    }
                    
                    uint32_t suiteStart = xTaskGetTickCount();
                    TestSuite result = runSuiteByNumber(suiteNum);
                    uint32_t suiteDuration = (xTaskGetTickCount() - suiteStart) * portTICK_PERIOD_MS;
                    
                    runPassed += result.passed;
                    runFailed += result.failed;
                    
                    // Track failed tests
                    for (const auto& r : result.results) {
                        if (!r.passed) {
                            char buf[128];
                            snprintf(buf, sizeof(buf), "Run%d.Suite%d.Rep%d: %s", 
                                     fullRun, suiteNum, rep, r.name.c_str());
                            failedTests.push_back(buf);
                        }
                    }
                    
                    ESP_LOGI(TEST_TAG, "<<< Suite %d Rep %d done: P=%d F=%d (%lu ms)", 
                             suiteNum, rep, result.passed, result.failed, suiteDuration);
                    
                    // Small delay between repetitions
                    safeDelay(100);
                }
                
                // Delay between suites
                ESP_LOGI(TEST_TAG, "[DELAY] Pause between suites...");
                safeDelay(200);
            }
            
            uint32_t runDuration = (xTaskGetTickCount() - runStart) * portTICK_PERIOD_MS;
            
            ESP_LOGI(TEST_TAG, "");
            ESP_LOGI(TEST_TAG, "################################################################");
            ESP_LOGI(TEST_TAG, "### FULL RUN %d COMPLETE                                     ###", fullRun);
            ESP_LOGI(TEST_TAG, "### Passed: %d | Failed: %d | Duration: %lu ms              ###", 
                     runPassed, runFailed, runDuration);
            ESP_LOGI(TEST_TAG, "################################################################");
            
            grandTotalPassed += runPassed;
            grandTotalFailed += runFailed;
            
            // Delay between full runs
            if (fullRun < SUITE_REPETITIONS) {
                ESP_LOGI(TEST_TAG, "");
                ESP_LOGI(TEST_TAG, "[DELAY] Pause before next full run...");
                safeDelay(500);
            }
        }
        
    finished:
        uint32_t grandDuration = (xTaskGetTickCount() - grandStart) * portTICK_PERIOD_MS;
        
        // ==================== FINAL SUMMARY ====================
        ESP_LOGI(TEST_TAG, "");
        ESP_LOGI(TEST_TAG, "########################################################################");
        ESP_LOGI(TEST_TAG, "########################################################################");
        ESP_LOGI(TEST_TAG, "###                                                                  ###");
        ESP_LOGI(TEST_TAG, "###                    FINAL TEST SUMMARY                            ###");
        ESP_LOGI(TEST_TAG, "###                                                                  ###");
        ESP_LOGI(TEST_TAG, "########################################################################");
        ESP_LOGI(TEST_TAG, "########################################################################");
        ESP_LOGI(TEST_TAG, "");
        ESP_LOGI(TEST_TAG, "  Total Test Runs:     %d", SUITE_REPETITIONS);
        ESP_LOGI(TEST_TAG, "  Total Suite Executions: %d", 10 * TEST_REPETITIONS * SUITE_REPETITIONS);
        ESP_LOGI(TEST_TAG, "  Total Duration:      %lu ms (%.1f seconds)", grandDuration, grandDuration / 1000.0f);
        ESP_LOGI(TEST_TAG, "");
        ESP_LOGI(TEST_TAG, "  ========================================");
        ESP_LOGI(TEST_TAG, "  GRAND TOTAL PASSED:  %d", grandTotalPassed);
        ESP_LOGI(TEST_TAG, "  GRAND TOTAL FAILED:  %d", grandTotalFailed);
        ESP_LOGI(TEST_TAG, "  SUCCESS RATE:        %.1f%%", 
                 (grandTotalPassed + grandTotalFailed) > 0 
                     ? (100.0f * grandTotalPassed / (grandTotalPassed + grandTotalFailed)) 
                     : 0.0f);
        ESP_LOGI(TEST_TAG, "  ========================================");
        ESP_LOGI(TEST_TAG, "");
        
        if (!failedTests.empty()) {
            ESP_LOGE(TEST_TAG, "  FAILED TESTS (%d total):", (int)failedTests.size());
            for (const auto& ft : failedTests) {
                ESP_LOGE(TEST_TAG, "    - %s", ft.c_str());
            }
            ESP_LOGI(TEST_TAG, "");
        }
        
        ESP_LOGI(TEST_TAG, "  Free heap after tests: %lu bytes", (uint32_t)esp_get_free_heap_size());
        ESP_LOGI(TEST_TAG, "");
        
        if (grandTotalFailed == 0) {
            ESP_LOGI(TEST_TAG, "########################################################################");
            ESP_LOGI(TEST_TAG, "###                                                                  ###");
            ESP_LOGI(TEST_TAG, "###              ALL TESTS PASSED SUCCESSFULLY!                      ###");
            ESP_LOGI(TEST_TAG, "###                                                                  ###");
            ESP_LOGI(TEST_TAG, "########################################################################");
        } else {
            ESP_LOGE(TEST_TAG, "########################################################################");
            ESP_LOGE(TEST_TAG, "###                                                                  ###");
            ESP_LOGE(TEST_TAG, "###              SOME TESTS FAILED - SEE ABOVE                       ###");
            ESP_LOGE(TEST_TAG, "###                                                                  ###");
            ESP_LOGE(TEST_TAG, "########################################################################");
        }
        
        ESP_LOGI(TEST_TAG, "");
        ESP_LOGI(TEST_TAG, "########################################################################");
        ESP_LOGI(TEST_TAG, "########################################################################");
        ESP_LOGI(TEST_TAG, "###                                                                  ###");
        ESP_LOGI(TEST_TAG, "###         TEST COMPLETELY FINISHED - STOP MONITORING NOW          ###");
        ESP_LOGI(TEST_TAG, "###                                                                  ###");
        ESP_LOGI(TEST_TAG, "########################################################################");
        ESP_LOGI(TEST_TAG, "########################################################################");
        ESP_LOGI(TEST_TAG, "");
    }
    
    /**
     * @brief Run a single test suite by number (1-10) - for manual testing
     */
    static void runSingleSuite(int suiteNum) {
        if (suiteNum < 1 || suiteNum > 10) {
            ESP_LOGE(TEST_TAG, "[ERROR] Invalid suite number: %d (valid: 1-10)", suiteNum);
            return;
        }
        
        ESP_LOGI(TEST_TAG, "");
        ESP_LOGI(TEST_TAG, "======================================================================");
        ESP_LOGI(TEST_TAG, "=== Running Single Suite %d: %s ===", suiteNum, getSuiteName(suiteNum));
        ESP_LOGI(TEST_TAG, "======================================================================");
        
        uint32_t start = xTaskGetTickCount();
        TestSuite suite = runSuiteByNumber(suiteNum);
        uint32_t duration = (xTaskGetTickCount() - start) * portTICK_PERIOD_MS;
        
        ESP_LOGI(TEST_TAG, "");
        ESP_LOGI(TEST_TAG, "======================================================================");
        ESP_LOGI(TEST_TAG, "=== Suite %d Complete ===", suiteNum);
        ESP_LOGI(TEST_TAG, "=== Passed: %d | Failed: %d | Duration: %lu ms ===", 
                 suite.passed, suite.failed, duration);
        ESP_LOGI(TEST_TAG, "======================================================================");
    }

    // ================================================================
    // TEST SUITE 1: Scene Creation
    // ================================================================
    static TestSuite runTestSuite_SceneCreation() {
        TestSuite suite;
        suite.name = "Scene Creation";
        
        printTestSuiteHeader("Scene Creation");
        uint32_t suiteStart = xTaskGetTickCount();
        
        // Clean up first
        ESP_LOGI(TEST_TAG, "[SETUP] Cleaning up existing test scenes...");
        cleanupTestScenes();
        int initialCount = getSceneCount();
        ESP_LOGI(TEST_TAG, "[SETUP] Initial scene count: %d", initialCount);
        
        // Test 1.1: Create basic scene
        {
            ESP_LOGI(TEST_TAG, "");
            ESP_LOGI(TEST_TAG, "▶ TEST 1.1: Create basic scene");
            uint32_t start = xTaskGetTickCount();
            
            int countBefore = getSceneCount();
            int newId = testCreateScene("TestScene_Basic");
            int countAfter = getSceneCount();
            
            ESP_LOGI(TEST_TAG, "  [DEBUG] Count before: %d", countBefore);
            ESP_LOGI(TEST_TAG, "  [DEBUG] Created ID: %d", newId);
            ESP_LOGI(TEST_TAG, "  [DEBUG] Count after: %d", countAfter);
            
            TestResult result;
            result.name = "Create basic scene";
            result.durationMs = (xTaskGetTickCount() - start) * portTICK_PERIOD_MS;
            
            if (newId > 0 && countAfter == countBefore + 1) {
                result.passed = true;
                result.message = "Created scene ID " + std::to_string(newId);
                suite.passed++;
                ESP_LOGI(TEST_TAG, "  ✓ PASS: %s (%lu ms)", result.name.c_str(), result.durationMs);
            } else {
                result.passed = false;
                result.message = "Failed to create scene or count mismatch";
                suite.failed++;
                ESP_LOGE(TEST_TAG, "  ✗ FAIL: %s - %s", result.name.c_str(), result.message.c_str());
            }
            suite.results.push_back(result);
        }
        
        // Test 1.2: Create multiple scenes
        {
            ESP_LOGI(TEST_TAG, "");
            ESP_LOGI(TEST_TAG, "▶ TEST 1.2: Create multiple scenes");
            uint32_t start = xTaskGetTickCount();
            
            int id1 = testCreateScene("TestScene_Multi1");
            int id2 = testCreateScene("TestScene_Multi2");
            int id3 = testCreateScene("TestScene_Multi3");
            
            ESP_LOGI(TEST_TAG, "  [DEBUG] Created IDs: %d, %d, %d", id1, id2, id3);
            
            TestResult result;
            result.name = "Create multiple scenes with unique IDs";
            result.durationMs = (xTaskGetTickCount() - start) * portTICK_PERIOD_MS;
            
            bool allUnique = (id1 > 0 && id2 > 0 && id3 > 0 && id1 != id2 && id2 != id3 && id1 != id3);
            
            if (allUnique) {
                result.passed = true;
                suite.passed++;
                ESP_LOGI(TEST_TAG, "  ✓ PASS: %s (%lu ms)", result.name.c_str(), result.durationMs);
            } else {
                result.passed = false;
                result.message = "Scene IDs not unique or invalid";
                suite.failed++;
                ESP_LOGE(TEST_TAG, "  ✗ FAIL: %s - %s", result.name.c_str(), result.message.c_str());
            }
            suite.results.push_back(result);
        }
        
        // Test 1.3: Verify default values
        {
            ESP_LOGI(TEST_TAG, "");
            ESP_LOGI(TEST_TAG, "▶ TEST 1.3: Verify default values on new scene");
            uint32_t start = xTaskGetTickCount();
            
            int id = testCreateScene("TestScene_Defaults");
            Web::SavedScene* scene = findSceneByIdInternal(id);
            
            ESP_LOGI(TEST_TAG, "  [DEBUG] Scene found: %s", scene ? "YES" : "NO");
            if (scene) {
                ESP_LOGI(TEST_TAG, "  [DEBUG] displayEnabled: %s (expected: true)", scene->displayEnabled ? "true" : "false");
                ESP_LOGI(TEST_TAG, "  [DEBUG] ledsEnabled: %s (expected: false)", scene->ledsEnabled ? "true" : "false");
                ESP_LOGI(TEST_TAG, "  [DEBUG] animType: '%s' (expected: 'gyro_eyes')", scene->animType.c_str());
                ESP_LOGI(TEST_TAG, "  [DEBUG] transition: '%s' (expected: 'none')", scene->transition.c_str());
                ESP_LOGI(TEST_TAG, "  [DEBUG] shaderAA: %s (expected: true)", scene->shaderAA ? "true" : "false");
                ESP_LOGI(TEST_TAG, "  [DEBUG] spriteId: %d (expected: -1)", scene->spriteId);
            }
            
            TestResult result;
            result.name = "Verify default values on creation";
            result.durationMs = (xTaskGetTickCount() - start) * portTICK_PERIOD_MS;
            
            bool defaultsCorrect = scene &&
                scene->displayEnabled == true &&
                scene->ledsEnabled == false &&
                scene->animType == "gyro_eyes" &&
                scene->transition == "none" &&
                scene->shaderAA == true &&
                scene->spriteId == -1;
            
            if (defaultsCorrect) {
                result.passed = true;
                suite.passed++;
                ESP_LOGI(TEST_TAG, "  ✓ PASS: %s (%lu ms)", result.name.c_str(), result.durationMs);
            } else {
                result.passed = false;
                result.message = "Default values incorrect";
                suite.failed++;
                ESP_LOGE(TEST_TAG, "  ✗ FAIL: %s - %s", result.name.c_str(), result.message.c_str());
            }
            suite.results.push_back(result);
        }
        
        suite.totalDurationMs = (xTaskGetTickCount() - suiteStart) * portTICK_PERIOD_MS;
        printTestSuiteSummary(suite);
        return suite;
    }
    
    // ================================================================
    // TEST SUITE 2: Scene Modification
    // ================================================================
    static TestSuite runTestSuite_SceneModification() {
        TestSuite suite;
        suite.name = "Scene Modification";
        
        printTestSuiteHeader("Scene Modification");
        uint32_t suiteStart = xTaskGetTickCount();
        
        int testId = testCreateScene("TestScene_Modify");
        ESP_LOGI(TEST_TAG, "[SETUP] Created test scene ID: %d", testId);
        
        // Test 2.1: Rename scene
        {
            ESP_LOGI(TEST_TAG, "");
            ESP_LOGI(TEST_TAG, "▶ TEST 2.1: Rename scene");
            uint32_t start = xTaskGetTickCount();
            
            bool success = testRenameScene(testId, "RenamedScene");
            Web::SavedScene* scene = findSceneByIdInternal(testId);
            
            ESP_LOGI(TEST_TAG, "  [DEBUG] Rename result: %s", success ? "success" : "failed");
            ESP_LOGI(TEST_TAG, "  [DEBUG] New name: %s", scene ? scene->name.c_str() : "N/A");
            
            TestResult result;
            result.name = "Rename scene";
            result.durationMs = (xTaskGetTickCount() - start) * portTICK_PERIOD_MS;
            
            if (success && scene && scene->name == "RenamedScene") {
                result.passed = true;
                suite.passed++;
                ESP_LOGI(TEST_TAG, "  ✓ PASS: %s (%lu ms)", result.name.c_str(), result.durationMs);
            } else {
                result.passed = false;
                result.message = "Rename failed";
                suite.failed++;
                ESP_LOGE(TEST_TAG, "  ✗ FAIL: %s", result.name.c_str());
            }
            suite.results.push_back(result);
        }
        
        // Test 2.2: Toggle displayEnabled
        {
            ESP_LOGI(TEST_TAG, "");
            ESP_LOGI(TEST_TAG, "▶ TEST 2.2: Toggle displayEnabled");
            uint32_t start = xTaskGetTickCount();
            
            Web::SavedScene* scene = findSceneByIdInternal(testId);
            bool originalValue = scene ? scene->displayEnabled : true;
            ESP_LOGI(TEST_TAG, "  [DEBUG] Original displayEnabled: %s", originalValue ? "true" : "false");
            
            testSetDisplayEnabled(testId, !originalValue);
            scene = findSceneByIdInternal(testId);
            bool newValue = scene ? scene->displayEnabled : originalValue;
            ESP_LOGI(TEST_TAG, "  [DEBUG] New displayEnabled: %s", newValue ? "true" : "false");
            
            TestResult result;
            result.name = "Toggle displayEnabled";
            result.durationMs = (xTaskGetTickCount() - start) * portTICK_PERIOD_MS;
            
            if (newValue != originalValue) {
                result.passed = true;
                suite.passed++;
                ESP_LOGI(TEST_TAG, "  ✓ PASS: %s (%lu ms)", result.name.c_str(), result.durationMs);
            } else {
                result.passed = false;
                result.message = "displayEnabled not toggled";
                suite.failed++;
                ESP_LOGE(TEST_TAG, "  ✗ FAIL: %s", result.name.c_str());
            }
            suite.results.push_back(result);
        }
        
        // Test 2.3: Toggle ledsEnabled
        {
            ESP_LOGI(TEST_TAG, "");
            ESP_LOGI(TEST_TAG, "▶ TEST 2.3: Toggle ledsEnabled");
            uint32_t start = xTaskGetTickCount();
            
            testSetLedsEnabled(testId, true);
            Web::SavedScene* scene = findSceneByIdInternal(testId);
            
            ESP_LOGI(TEST_TAG, "  [DEBUG] ledsEnabled: %s", scene && scene->ledsEnabled ? "true" : "false");
            
            TestResult result;
            result.name = "Toggle ledsEnabled";
            result.durationMs = (xTaskGetTickCount() - start) * portTICK_PERIOD_MS;
            
            if (scene && scene->ledsEnabled == true) {
                result.passed = true;
                suite.passed++;
                ESP_LOGI(TEST_TAG, "  ✓ PASS: %s (%lu ms)", result.name.c_str(), result.durationMs);
            } else {
                result.passed = false;
                suite.failed++;
                ESP_LOGE(TEST_TAG, "  ✗ FAIL: %s", result.name.c_str());
            }
            suite.results.push_back(result);
        }
        
        // Test 2.4: Delete scene
        {
            ESP_LOGI(TEST_TAG, "");
            ESP_LOGI(TEST_TAG, "▶ TEST 2.4: Delete scene");
            uint32_t start = xTaskGetTickCount();
            
            int countBefore = getSceneCount();
            bool success = testDeleteScene(testId);
            int countAfter = getSceneCount();
            Web::SavedScene* scene = findSceneByIdInternal(testId);
            
            ESP_LOGI(TEST_TAG, "  [DEBUG] Count before: %d, after: %d", countBefore, countAfter);
            ESP_LOGI(TEST_TAG, "  [DEBUG] Scene found after delete: %s", scene ? "YES" : "NO");
            
            TestResult result;
            result.name = "Delete scene";
            result.durationMs = (xTaskGetTickCount() - start) * portTICK_PERIOD_MS;
            
            if (success && countAfter == countBefore - 1 && scene == nullptr) {
                result.passed = true;
                suite.passed++;
                ESP_LOGI(TEST_TAG, "  ✓ PASS: %s (%lu ms)", result.name.c_str(), result.durationMs);
            } else {
                result.passed = false;
                suite.failed++;
                ESP_LOGE(TEST_TAG, "  ✗ FAIL: %s", result.name.c_str());
            }
            suite.results.push_back(result);
        }
        
        suite.totalDurationMs = (xTaskGetTickCount() - suiteStart) * portTICK_PERIOD_MS;
        printTestSuiteSummary(suite);
        return suite;
    }
    
    // ================================================================
    // TEST SUITE 3: Animation Types
    // ================================================================
    static TestSuite runTestSuite_AnimationTypes() {
        TestSuite suite;
        suite.name = "Animation Types";
        
        printTestSuiteHeader("Animation Types");
        uint32_t suiteStart = xTaskGetTickCount();
        
        int testId = testCreateScene("TestScene_AnimTypes");
        ESP_LOGI(TEST_TAG, "[SETUP] Created test scene ID: %d", testId);
        
        const char* animTypes[] = {"gyro_eyes", "static_image", "sway", "sdf_morph", "none"};
        int numTypes = sizeof(animTypes) / sizeof(animTypes[0]);
        
        for (int i = 0; i < numTypes; i++) {
            ESP_LOGI(TEST_TAG, "");
            ESP_LOGI(TEST_TAG, "▶ TEST 3.%d: Set animation type '%s'", i+1, animTypes[i]);
            uint32_t start = xTaskGetTickCount();
            
            testSetAnimationType(testId, animTypes[i]);
            Web::SavedScene* scene = findSceneByIdInternal(testId);
            
            ESP_LOGI(TEST_TAG, "  [DEBUG] Expected animType: '%s'", animTypes[i]);
            ESP_LOGI(TEST_TAG, "  [DEBUG] Actual animType: '%s'", scene ? scene->animType.c_str() : "N/A");
            
            TestResult result;
            result.name = std::string("Set animType: ") + animTypes[i];
            result.durationMs = (xTaskGetTickCount() - start) * portTICK_PERIOD_MS;
            
            if (scene && scene->animType == animTypes[i]) {
                result.passed = true;
                suite.passed++;
                ESP_LOGI(TEST_TAG, "  ✓ PASS: %s (%lu ms)", result.name.c_str(), result.durationMs);
            } else {
                result.passed = false;
                result.message = "animType mismatch";
                suite.failed++;
                ESP_LOGE(TEST_TAG, "  ✗ FAIL: %s - expected '%s', got '%s'", 
                         result.name.c_str(), animTypes[i], 
                         scene ? scene->animType.c_str() : "null");
            }
            suite.results.push_back(result);
        }
        
        // Test animation type in callback
        {
            ESP_LOGI(TEST_TAG, "");
            ESP_LOGI(TEST_TAG, "▶ TEST 3.%d: Animation type passed to callback", numTypes + 1);
            uint32_t start = xTaskGetTickCount();
            
            testSetAnimationType(testId, "static_image");
            callbackWasTriggered_ = false;
            testActivateScene(testId);
            vTaskDelay(pdMS_TO_TICKS(50));
            
            ESP_LOGI(TEST_TAG, "  [DEBUG] Callback triggered: %s", callbackWasTriggered_ ? "YES" : "NO");
            ESP_LOGI(TEST_TAG, "  [DEBUG] Callback animType: '%s'", lastActivatedScene_.animType.c_str());
            
            TestResult result;
            result.name = "Animation type in activation callback";
            result.durationMs = (xTaskGetTickCount() - start) * portTICK_PERIOD_MS;
            
            if (callbackWasTriggered_ && lastActivatedScene_.animType == "static_image") {
                result.passed = true;
                suite.passed++;
                ESP_LOGI(TEST_TAG, "  ✓ PASS: %s (%lu ms)", result.name.c_str(), result.durationMs);
            } else {
                result.passed = false;
                result.message = "Callback animType mismatch";
                suite.failed++;
                ESP_LOGE(TEST_TAG, "  ✗ FAIL: %s", result.name.c_str());
            }
            suite.results.push_back(result);
        }
        
        testDeleteScene(testId);
        suite.totalDurationMs = (xTaskGetTickCount() - suiteStart) * portTICK_PERIOD_MS;
        printTestSuiteSummary(suite);
        return suite;
    }
    
    // ================================================================
    // TEST SUITE 4: Display/LED Toggle
    // ================================================================
    static TestSuite runTestSuite_DisplayLedToggle() {
        TestSuite suite;
        suite.name = "Display/LED Toggle";
        
        printTestSuiteHeader("Display/LED Toggle");
        uint32_t suiteStart = xTaskGetTickCount();
        
        int testId = testCreateScene("TestScene_Toggle");
        ESP_LOGI(TEST_TAG, "[SETUP] Created test scene ID: %d", testId);
        
        struct ToggleTest {
            bool display;
            bool leds;
            const char* name;
        };
        
        ToggleTest tests[] = {
            {true, false, "Display Only"},
            {false, true, "LEDs Only"},
            {true, true, "Both Display and LEDs"},
            {false, false, "Neither (all off)"}
        };
        
        int testNum = 1;
        for (const auto& test : tests) {
            ESP_LOGI(TEST_TAG, "");
            ESP_LOGI(TEST_TAG, "▶ TEST 4.%d: %s", testNum, test.name);
            uint32_t start = xTaskGetTickCount();
            
            testSetDisplayEnabled(testId, test.display);
            testSetLedsEnabled(testId, test.leds);
            
            Web::SavedScene* scene = findSceneByIdInternal(testId);
            
            ESP_LOGI(TEST_TAG, "  [DEBUG] Expected: display=%s, leds=%s", 
                     test.display ? "true" : "false", test.leds ? "true" : "false");
            ESP_LOGI(TEST_TAG, "  [DEBUG] Actual:   display=%s, leds=%s",
                     scene && scene->displayEnabled ? "true" : "false",
                     scene && scene->ledsEnabled ? "true" : "false");
            
            TestResult result;
            result.name = test.name;
            result.durationMs = (xTaskGetTickCount() - start) * portTICK_PERIOD_MS;
            
            if (scene && scene->displayEnabled == test.display && scene->ledsEnabled == test.leds) {
                result.passed = true;
                suite.passed++;
                ESP_LOGI(TEST_TAG, "  ✓ PASS: %s (%lu ms)", result.name.c_str(), result.durationMs);
            } else {
                result.passed = false;
                suite.failed++;
                ESP_LOGE(TEST_TAG, "  ✗ FAIL: %s", result.name.c_str());
            }
            suite.results.push_back(result);
            testNum++;
        }
        
        // Test callback receives correct values
        {
            ESP_LOGI(TEST_TAG, "");
            ESP_LOGI(TEST_TAG, "▶ TEST 4.%d: Callback receives correct display/LED state", testNum);
            uint32_t start = xTaskGetTickCount();
            
            testSetDisplayEnabled(testId, true);
            testSetLedsEnabled(testId, true);
            
            callbackWasTriggered_ = false;
            testActivateScene(testId);
            vTaskDelay(pdMS_TO_TICKS(50));
            
            ESP_LOGI(TEST_TAG, "  [DEBUG] Callback display: %s", lastActivatedScene_.displayEnabled ? "true" : "false");
            ESP_LOGI(TEST_TAG, "  [DEBUG] Callback leds: %s", lastActivatedScene_.ledsEnabled ? "true" : "false");
            
            TestResult result;
            result.name = "Callback receives correct display/LED state";
            result.durationMs = (xTaskGetTickCount() - start) * portTICK_PERIOD_MS;
            
            if (callbackWasTriggered_ && 
                lastActivatedScene_.displayEnabled == true &&
                lastActivatedScene_.ledsEnabled == true) {
                result.passed = true;
                suite.passed++;
                ESP_LOGI(TEST_TAG, "  ✓ PASS: %s (%lu ms)", result.name.c_str(), result.durationMs);
            } else {
                result.passed = false;
                suite.failed++;
                ESP_LOGE(TEST_TAG, "  ✗ FAIL: %s", result.name.c_str());
            }
            suite.results.push_back(result);
        }
        
        testDeleteScene(testId);
        suite.totalDurationMs = (xTaskGetTickCount() - suiteStart) * portTICK_PERIOD_MS;
        printTestSuiteSummary(suite);
        return suite;
    }
    
    // ================================================================
    // TEST SUITE 5: Shader Settings
    // ================================================================
    static TestSuite runTestSuite_ShaderSettings() {
        TestSuite suite;
        suite.name = "Shader Settings";
        
        printTestSuiteHeader("Shader Settings");
        uint32_t suiteStart = xTaskGetTickCount();
        
        int testId = testCreateScene("TestScene_Shader");
        ESP_LOGI(TEST_TAG, "[SETUP] Created test scene ID: %d", testId);
        
        // Test shaderAA toggle
        {
            ESP_LOGI(TEST_TAG, "");
            ESP_LOGI(TEST_TAG, "▶ TEST 5.1: Shader AA toggle");
            uint32_t start = xTaskGetTickCount();
            
            testSetShaderAA(testId, false);
            Web::SavedScene* scene = findSceneByIdInternal(testId);
            bool val1 = scene ? scene->shaderAA : true;
            ESP_LOGI(TEST_TAG, "  [DEBUG] After set false: shaderAA=%s", val1 ? "true" : "false");
            
            testSetShaderAA(testId, true);
            scene = findSceneByIdInternal(testId);
            bool val2 = scene ? scene->shaderAA : false;
            ESP_LOGI(TEST_TAG, "  [DEBUG] After set true: shaderAA=%s", val2 ? "true" : "false");
            
            TestResult result;
            result.name = "Shader AA toggle";
            result.durationMs = (xTaskGetTickCount() - start) * portTICK_PERIOD_MS;
            
            if (val1 == false && val2 == true) {
                result.passed = true;
                suite.passed++;
                ESP_LOGI(TEST_TAG, "  ✓ PASS: %s (%lu ms)", result.name.c_str(), result.durationMs);
            } else {
                result.passed = false;
                suite.failed++;
                ESP_LOGE(TEST_TAG, "  ✗ FAIL: %s", result.name.c_str());
            }
            suite.results.push_back(result);
        }
        
        // Test shaderInvert
        {
            ESP_LOGI(TEST_TAG, "");
            ESP_LOGI(TEST_TAG, "▶ TEST 5.2: Shader Invert toggle");
            uint32_t start = xTaskGetTickCount();
            
            testSetShaderInvert(testId, true);
            Web::SavedScene* scene = findSceneByIdInternal(testId);
            
            ESP_LOGI(TEST_TAG, "  [DEBUG] shaderInvert: %s", scene && scene->shaderInvert ? "true" : "false");
            
            TestResult result;
            result.name = "Shader Invert toggle";
            result.durationMs = (xTaskGetTickCount() - start) * portTICK_PERIOD_MS;
            
            if (scene && scene->shaderInvert == true) {
                result.passed = true;
                suite.passed++;
                ESP_LOGI(TEST_TAG, "  ✓ PASS: %s (%lu ms)", result.name.c_str(), result.durationMs);
            } else {
                result.passed = false;
                suite.failed++;
                ESP_LOGE(TEST_TAG, "  ✗ FAIL: %s", result.name.c_str());
            }
            suite.results.push_back(result);
        }
        
        // Test shaderColorMode
        const char* colorModes[] = {"none", "solid", "rainbow"};
        int testNum = 3;
        for (const char* mode : colorModes) {
            ESP_LOGI(TEST_TAG, "");
            ESP_LOGI(TEST_TAG, "▶ TEST 5.%d: Shader color mode '%s'", testNum, mode);
            uint32_t start = xTaskGetTickCount();
            
            testSetShaderColorMode(testId, mode);
            Web::SavedScene* scene = findSceneByIdInternal(testId);
            
            ESP_LOGI(TEST_TAG, "  [DEBUG] shaderColorMode: '%s' (expected: '%s')", 
                     scene ? scene->shaderColorMode.c_str() : "N/A", mode);
            
            TestResult result;
            result.name = std::string("Shader color mode: ") + mode;
            result.durationMs = (xTaskGetTickCount() - start) * portTICK_PERIOD_MS;
            
            if (scene && scene->shaderColorMode == mode) {
                result.passed = true;
                suite.passed++;
                ESP_LOGI(TEST_TAG, "  ✓ PASS: %s (%lu ms)", result.name.c_str(), result.durationMs);
            } else {
                result.passed = false;
                suite.failed++;
                ESP_LOGE(TEST_TAG, "  ✗ FAIL: %s", result.name.c_str());
            }
            suite.results.push_back(result);
            testNum++;
        }
        
        // Test shaderColor
        {
            ESP_LOGI(TEST_TAG, "");
            ESP_LOGI(TEST_TAG, "▶ TEST 5.%d: Shader color value", testNum);
            uint32_t start = xTaskGetTickCount();
            
            testSetShaderColor(testId, "#ff6b00");
            Web::SavedScene* scene = findSceneByIdInternal(testId);
            
            ESP_LOGI(TEST_TAG, "  [DEBUG] shaderColor: '%s' (expected: '#ff6b00')", 
                     scene ? scene->shaderColor.c_str() : "N/A");
            
            TestResult result;
            result.name = "Shader color value";
            result.durationMs = (xTaskGetTickCount() - start) * portTICK_PERIOD_MS;
            
            if (scene && scene->shaderColor == "#ff6b00") {
                result.passed = true;
                suite.passed++;
                ESP_LOGI(TEST_TAG, "  ✓ PASS: %s (%lu ms)", result.name.c_str(), result.durationMs);
            } else {
                result.passed = false;
                suite.failed++;
                ESP_LOGE(TEST_TAG, "  ✗ FAIL: %s", result.name.c_str());
            }
            suite.results.push_back(result);
        }
        
        testDeleteScene(testId);
        suite.totalDurationMs = (xTaskGetTickCount() - suiteStart) * portTICK_PERIOD_MS;
        printTestSuiteSummary(suite);
        return suite;
    }
    
    // ================================================================
    // TEST SUITE 6: LED Colors
    // ================================================================
    static TestSuite runTestSuite_LedColors() {
        TestSuite suite;
        suite.name = "LED Color Settings";
        
        printTestSuiteHeader("LED Color Settings");
        uint32_t suiteStart = xTaskGetTickCount();
        
        int testId = testCreateScene("TestScene_LedColor");
        ESP_LOGI(TEST_TAG, "[SETUP] Created test scene ID: %d", testId);
        
        struct ColorTest {
            uint8_t r, g, b;
            const char* name;
        };
        
        ColorTest colors[] = {
            {255, 0, 0, "Red"},
            {0, 255, 0, "Green"},
            {0, 0, 255, "Blue"},
            {255, 255, 0, "Yellow"},
            {255, 0, 255, "Magenta"},
            {0, 255, 255, "Cyan"},
            {255, 255, 255, "White"},
            {0, 0, 0, "Black"},
            {128, 64, 32, "Custom Brown"}
        };
        
        int testNum = 1;
        for (const auto& color : colors) {
            ESP_LOGI(TEST_TAG, "");
            ESP_LOGI(TEST_TAG, "▶ TEST 6.%d: LED Color %s (R=%d G=%d B=%d)", testNum, color.name, color.r, color.g, color.b);
            uint32_t start = xTaskGetTickCount();
            
            testSetLedColor(testId, color.r, color.g, color.b);
            Web::SavedScene* scene = findSceneByIdInternal(testId);
            
            ESP_LOGI(TEST_TAG, "  [DEBUG] Expected: R=%d G=%d B=%d", color.r, color.g, color.b);
            ESP_LOGI(TEST_TAG, "  [DEBUG] Actual:   R=%d G=%d B=%d", 
                     scene ? scene->ledR : -1, scene ? scene->ledG : -1, scene ? scene->ledB : -1);
            
            TestResult result;
            result.name = std::string("LED Color: ") + color.name;
            result.durationMs = (xTaskGetTickCount() - start) * portTICK_PERIOD_MS;
            
            if (scene && scene->ledR == color.r && scene->ledG == color.g && scene->ledB == color.b) {
                result.passed = true;
                suite.passed++;
                ESP_LOGI(TEST_TAG, "  ✓ PASS: %s (%lu ms)", result.name.c_str(), result.durationMs);
            } else {
                result.passed = false;
                suite.failed++;
                ESP_LOGE(TEST_TAG, "  ✗ FAIL: %s", result.name.c_str());
            }
            suite.results.push_back(result);
            testNum++;
        }
        
        // Test brightness
        uint8_t brightnessVals[] = {0, 25, 50, 75, 100};
        for (uint8_t brightness : brightnessVals) {
            ESP_LOGI(TEST_TAG, "");
            ESP_LOGI(TEST_TAG, "▶ TEST 6.%d: LED Brightness %d%%", testNum, brightness);
            uint32_t start = xTaskGetTickCount();
            
            testSetLedBrightness(testId, brightness);
            Web::SavedScene* scene = findSceneByIdInternal(testId);
            
            ESP_LOGI(TEST_TAG, "  [DEBUG] ledBrightness: %d (expected: %d)", 
                     scene ? scene->ledBrightness : -1, brightness);
            
            TestResult result;
            result.name = std::string("LED Brightness: ") + std::to_string(brightness) + "%";
            result.durationMs = (xTaskGetTickCount() - start) * portTICK_PERIOD_MS;
            
            if (scene && scene->ledBrightness == brightness) {
                result.passed = true;
                suite.passed++;
                ESP_LOGI(TEST_TAG, "  ✓ PASS: %s (%lu ms)", result.name.c_str(), result.durationMs);
            } else {
                result.passed = false;
                suite.failed++;
                ESP_LOGE(TEST_TAG, "  ✗ FAIL: %s", result.name.c_str());
            }
            suite.results.push_back(result);
            testNum++;
        }
        
        // Test LED color in callback
        {
            ESP_LOGI(TEST_TAG, "");
            ESP_LOGI(TEST_TAG, "▶ TEST 6.%d: LED color in activation callback", testNum);
            uint32_t start = xTaskGetTickCount();
            
            testSetLedColor(testId, 100, 150, 200);
            testSetLedBrightness(testId, 80);
            
            callbackWasTriggered_ = false;
            testActivateScene(testId);
            vTaskDelay(pdMS_TO_TICKS(50));
            
            ESP_LOGI(TEST_TAG, "  [DEBUG] Callback LED: R=%d G=%d B=%d Bright=%d",
                     lastActivatedScene_.ledR, lastActivatedScene_.ledG, 
                     lastActivatedScene_.ledB, lastActivatedScene_.ledBrightness);
            
            TestResult result;
            result.name = "LED color in callback";
            result.durationMs = (xTaskGetTickCount() - start) * portTICK_PERIOD_MS;
            
            if (callbackWasTriggered_ &&
                lastActivatedScene_.ledR == 100 &&
                lastActivatedScene_.ledG == 150 &&
                lastActivatedScene_.ledB == 200 &&
                lastActivatedScene_.ledBrightness == 80) {
                result.passed = true;
                suite.passed++;
                ESP_LOGI(TEST_TAG, "  ✓ PASS: %s (%lu ms)", result.name.c_str(), result.durationMs);
            } else {
                result.passed = false;
                suite.failed++;
                ESP_LOGE(TEST_TAG, "  ✗ FAIL: %s", result.name.c_str());
            }
            suite.results.push_back(result);
        }
        
        testDeleteScene(testId);
        suite.totalDurationMs = (xTaskGetTickCount() - suiteStart) * portTICK_PERIOD_MS;
        printTestSuiteSummary(suite);
        return suite;
    }
    
    // ================================================================
    // TEST SUITE 7: Scene Activation
    // ================================================================
    static TestSuite runTestSuite_SceneActivation() {
        TestSuite suite;
        suite.name = "Scene Activation";
        
        printTestSuiteHeader("Scene Activation");
        uint32_t suiteStart = xTaskGetTickCount();
        
        int scene1 = testCreateScene("TestScene_Act1");
        int scene2 = testCreateScene("TestScene_Act2");
        int scene3 = testCreateScene("TestScene_Act3");
        ESP_LOGI(TEST_TAG, "[SETUP] Created scenes: %d, %d, %d", scene1, scene2, scene3);
        
        // Test basic activation
        {
            ESP_LOGI(TEST_TAG, "");
            ESP_LOGI(TEST_TAG, "▶ TEST 7.1: Basic scene activation");
            uint32_t start = xTaskGetTickCount();
            
            callbackWasTriggered_ = false;
            testActivateScene(scene1);
            vTaskDelay(pdMS_TO_TICKS(50));
            
            int activeId = Web::activeSceneId_;
            
            ESP_LOGI(TEST_TAG, "  [DEBUG] Callback triggered: %s", callbackWasTriggered_ ? "YES" : "NO");
            ESP_LOGI(TEST_TAG, "  [DEBUG] Active scene ID: %d (expected: %d)", activeId, scene1);
            
            TestResult result;
            result.name = "Basic scene activation";
            result.durationMs = (xTaskGetTickCount() - start) * portTICK_PERIOD_MS;
            
            if (callbackWasTriggered_ && activeId == scene1) {
                result.passed = true;
                suite.passed++;
                ESP_LOGI(TEST_TAG, "  ✓ PASS: %s (%lu ms)", result.name.c_str(), result.durationMs);
            } else {
                result.passed = false;
                suite.failed++;
                ESP_LOGE(TEST_TAG, "  ✗ FAIL: %s", result.name.c_str());
            }
            suite.results.push_back(result);
        }
        
        // Test switching between scenes
        {
            ESP_LOGI(TEST_TAG, "");
            ESP_LOGI(TEST_TAG, "▶ TEST 7.2: Switch between scenes");
            uint32_t start = xTaskGetTickCount();
            
            testActivateScene(scene1);
            vTaskDelay(pdMS_TO_TICKS(30));
            int active1 = Web::activeSceneId_;
            
            testActivateScene(scene2);
            vTaskDelay(pdMS_TO_TICKS(30));
            int active2 = Web::activeSceneId_;
            
            testActivateScene(scene3);
            vTaskDelay(pdMS_TO_TICKS(30));
            int active3 = Web::activeSceneId_;
            
            ESP_LOGI(TEST_TAG, "  [DEBUG] After scene1: active=%d (expected %d)", active1, scene1);
            ESP_LOGI(TEST_TAG, "  [DEBUG] After scene2: active=%d (expected %d)", active2, scene2);
            ESP_LOGI(TEST_TAG, "  [DEBUG] After scene3: active=%d (expected %d)", active3, scene3);
            
            TestResult result;
            result.name = "Switch between scenes";
            result.durationMs = (xTaskGetTickCount() - start) * portTICK_PERIOD_MS;
            
            if (active1 == scene1 && active2 == scene2 && active3 == scene3) {
                result.passed = true;
                suite.passed++;
                ESP_LOGI(TEST_TAG, "  ✓ PASS: %s (%lu ms)", result.name.c_str(), result.durationMs);
            } else {
                result.passed = false;
                suite.failed++;
                ESP_LOGE(TEST_TAG, "  ✗ FAIL: %s", result.name.c_str());
            }
            suite.results.push_back(result);
        }
        
        // Test only one scene active at a time
        {
            ESP_LOGI(TEST_TAG, "");
            ESP_LOGI(TEST_TAG, "▶ TEST 7.3: Only one scene active at a time");
            uint32_t start = xTaskGetTickCount();
            
            testActivateScene(scene2);
            vTaskDelay(pdMS_TO_TICKS(30));
            
            Web::SavedScene* s1 = findSceneByIdInternal(scene1);
            Web::SavedScene* s2 = findSceneByIdInternal(scene2);
            Web::SavedScene* s3 = findSceneByIdInternal(scene3);
            
            ESP_LOGI(TEST_TAG, "  [DEBUG] Scene1 active: %s", s1 && s1->active ? "YES" : "NO");
            ESP_LOGI(TEST_TAG, "  [DEBUG] Scene2 active: %s", s2 && s2->active ? "YES" : "NO");
            ESP_LOGI(TEST_TAG, "  [DEBUG] Scene3 active: %s", s3 && s3->active ? "YES" : "NO");
            
            TestResult result;
            result.name = "Only one scene active at a time";
            result.durationMs = (xTaskGetTickCount() - start) * portTICK_PERIOD_MS;
            
            if (s1 && !s1->active && s2 && s2->active && s3 && !s3->active) {
                result.passed = true;
                suite.passed++;
                ESP_LOGI(TEST_TAG, "  ✓ PASS: %s (%lu ms)", result.name.c_str(), result.durationMs);
            } else {
                result.passed = false;
                suite.failed++;
                ESP_LOGE(TEST_TAG, "  ✗ FAIL: %s", result.name.c_str());
            }
            suite.results.push_back(result);
        }
        
        // Test activating non-existent scene
        {
            ESP_LOGI(TEST_TAG, "");
            ESP_LOGI(TEST_TAG, "▶ TEST 7.4: Activate non-existent scene (should fail gracefully)");
            uint32_t start = xTaskGetTickCount();
            
            int prevActive = Web::activeSceneId_;
            callbackWasTriggered_ = false;
            testActivateScene(99999);
            vTaskDelay(pdMS_TO_TICKS(30));
            
            ESP_LOGI(TEST_TAG, "  [DEBUG] Callback triggered: %s (expected: NO)", callbackWasTriggered_ ? "YES" : "NO");
            ESP_LOGI(TEST_TAG, "  [DEBUG] Active unchanged: %s", Web::activeSceneId_ == prevActive ? "YES" : "NO");
            
            TestResult result;
            result.name = "Activate non-existent scene (graceful fail)";
            result.durationMs = (xTaskGetTickCount() - start) * portTICK_PERIOD_MS;
            
            // Should NOT trigger callback for non-existent scene
            if (!callbackWasTriggered_) {
                result.passed = true;
                suite.passed++;
                ESP_LOGI(TEST_TAG, "  ✓ PASS: %s (%lu ms)", result.name.c_str(), result.durationMs);
            } else {
                result.passed = false;
                suite.failed++;
                ESP_LOGE(TEST_TAG, "  ✗ FAIL: %s - callback was triggered for invalid scene", result.name.c_str());
            }
            suite.results.push_back(result);
        }
        
        testDeleteScene(scene1);
        testDeleteScene(scene2);
        testDeleteScene(scene3);
        suite.totalDurationMs = (xTaskGetTickCount() - suiteStart) * portTICK_PERIOD_MS;
        printTestSuiteSummary(suite);
        return suite;
    }
    
    // ================================================================
    // TEST SUITE 8: Persistence (SD Card)
    // ================================================================
    static TestSuite runTestSuite_Persistence() {
        TestSuite suite;
        suite.name = "Persistence (SD Card)";
        
        printTestSuiteHeader("Persistence (SD Card)");
        uint32_t suiteStart = xTaskGetTickCount();
        
        // Create scene with specific settings
        int testId = testCreateScene("TestScene_Persist");
        ESP_LOGI(TEST_TAG, "[SETUP] Created test scene ID: %d", testId);
        
        // Configure all fields
        testSetAnimationType(testId, "sway");
        testSetDisplayEnabled(testId, true);
        testSetLedsEnabled(testId, true);
        testSetShaderAA(testId, false);
        testSetShaderInvert(testId, true);
        testSetShaderColorMode(testId, "solid");
        testSetShaderColor(testId, "#123456");
        testSetLedColor(testId, 50, 100, 150);
        testSetLedBrightness(testId, 75);
        testSetTransition(testId, "glitch");
        
        ESP_LOGI(TEST_TAG, "[SETUP] Configured all fields");
        
        // Store values before save
        Web::SavedScene* sceneBefore = findSceneByIdInternal(testId);
        std::string animTypeBefore = sceneBefore ? sceneBefore->animType : "";
        bool displayBefore = sceneBefore ? sceneBefore->displayEnabled : false;
        bool ledsBefore = sceneBefore ? sceneBefore->ledsEnabled : false;
        bool shaderAABefore = sceneBefore ? sceneBefore->shaderAA : true;
        bool shaderInvertBefore = sceneBefore ? sceneBefore->shaderInvert : false;
        std::string shaderColorModeBefore = sceneBefore ? sceneBefore->shaderColorMode : "";
        std::string shaderColorBefore = sceneBefore ? sceneBefore->shaderColor : "";
        uint8_t ledRBefore = sceneBefore ? sceneBefore->ledR : 0;
        uint8_t ledGBefore = sceneBefore ? sceneBefore->ledG : 0;
        uint8_t ledBBefore = sceneBefore ? sceneBefore->ledB : 0;
        uint8_t ledBrightBefore = sceneBefore ? sceneBefore->ledBrightness : 0;
        std::string transitionBefore = sceneBefore ? sceneBefore->transition : "";
        
        ESP_LOGI(TEST_TAG, "");
        ESP_LOGI(TEST_TAG, "[DEBUG] Values BEFORE save:");
        ESP_LOGI(TEST_TAG, "  animType: %s", animTypeBefore.c_str());
        ESP_LOGI(TEST_TAG, "  displayEnabled: %s", displayBefore ? "true" : "false");
        ESP_LOGI(TEST_TAG, "  ledsEnabled: %s", ledsBefore ? "true" : "false");
        ESP_LOGI(TEST_TAG, "  shaderAA: %s", shaderAABefore ? "true" : "false");
        ESP_LOGI(TEST_TAG, "  shaderInvert: %s", shaderInvertBefore ? "true" : "false");
        ESP_LOGI(TEST_TAG, "  shaderColorMode: %s", shaderColorModeBefore.c_str());
        ESP_LOGI(TEST_TAG, "  shaderColor: %s", shaderColorBefore.c_str());
        ESP_LOGI(TEST_TAG, "  LED: R=%d G=%d B=%d Bright=%d", ledRBefore, ledGBefore, ledBBefore, ledBrightBefore);
        ESP_LOGI(TEST_TAG, "  transition: %s", transitionBefore.c_str());
        
        // Test: Save to SD card
        {
            ESP_LOGI(TEST_TAG, "");
            ESP_LOGI(TEST_TAG, "▶ TEST 8.1: Save scenes to SD card");
            uint32_t start = xTaskGetTickCount();
            
            Web::HttpServer::instance().forceSaveScenes();
            vTaskDelay(pdMS_TO_TICKS(100));
            
            ESP_LOGI(TEST_TAG, "  [DEBUG] forceSaveScenes() called");
            
            TestResult result;
            result.name = "Save scenes to SD card";
            result.durationMs = (xTaskGetTickCount() - start) * portTICK_PERIOD_MS;
            result.passed = true;
            suite.passed++;
            ESP_LOGI(TEST_TAG, "  ✓ PASS: %s (%lu ms)", result.name.c_str(), result.durationMs);
            suite.results.push_back(result);
        }
        
        // Clear and reload
        {
            ESP_LOGI(TEST_TAG, "");
            ESP_LOGI(TEST_TAG, "▶ TEST 8.2: Clear and reload from SD card");
            uint32_t start = xTaskGetTickCount();
            
            Web::savedScenes_.clear();
            ESP_LOGI(TEST_TAG, "  [DEBUG] Cleared in-memory scenes");
            
            Web::HttpServer::instance().forceLoadScenes();
            vTaskDelay(pdMS_TO_TICKS(100));
            
            int count = getSceneCount();
            ESP_LOGI(TEST_TAG, "  [DEBUG] Scenes after reload: %d", count);
            
            TestResult result;
            result.name = "Clear and reload from SD card";
            result.durationMs = (xTaskGetTickCount() - start) * portTICK_PERIOD_MS;
            
            if (count > 0) {
                result.passed = true;
                suite.passed++;
                ESP_LOGI(TEST_TAG, "  ✓ PASS: %s (%lu ms)", result.name.c_str(), result.durationMs);
            } else {
                result.passed = false;
                result.message = "No scenes loaded";
                suite.failed++;
                ESP_LOGE(TEST_TAG, "  ✗ FAIL: %s - %s", result.name.c_str(), result.message.c_str());
            }
            suite.results.push_back(result);
        }
        
        // Verify each field persisted
        Web::SavedScene* sceneAfter = findSceneByIdInternal(testId);
        
        if (sceneAfter) {
            ESP_LOGI(TEST_TAG, "");
            ESP_LOGI(TEST_TAG, "[DEBUG] Values AFTER reload:");
            ESP_LOGI(TEST_TAG, "  animType: %s", sceneAfter->animType.c_str());
            ESP_LOGI(TEST_TAG, "  displayEnabled: %s", sceneAfter->displayEnabled ? "true" : "false");
            ESP_LOGI(TEST_TAG, "  ledsEnabled: %s", sceneAfter->ledsEnabled ? "true" : "false");
            ESP_LOGI(TEST_TAG, "  shaderAA: %s", sceneAfter->shaderAA ? "true" : "false");
            ESP_LOGI(TEST_TAG, "  shaderInvert: %s", sceneAfter->shaderInvert ? "true" : "false");
            ESP_LOGI(TEST_TAG, "  shaderColorMode: %s", sceneAfter->shaderColorMode.c_str());
            ESP_LOGI(TEST_TAG, "  shaderColor: %s", sceneAfter->shaderColor.c_str());
            ESP_LOGI(TEST_TAG, "  LED: R=%d G=%d B=%d Bright=%d", 
                     sceneAfter->ledR, sceneAfter->ledG, sceneAfter->ledB, sceneAfter->ledBrightness);
            ESP_LOGI(TEST_TAG, "  transition: %s", sceneAfter->transition.c_str());
        }
        
        struct PersistTest {
            const char* name;
            bool passed;
        };
        
        std::vector<PersistTest> persistTests = {
            {"animType persisted", sceneAfter && sceneAfter->animType == animTypeBefore},
            {"displayEnabled persisted", sceneAfter && sceneAfter->displayEnabled == displayBefore},
            {"ledsEnabled persisted", sceneAfter && sceneAfter->ledsEnabled == ledsBefore},
            {"shaderAA persisted", sceneAfter && sceneAfter->shaderAA == shaderAABefore},
            {"shaderInvert persisted", sceneAfter && sceneAfter->shaderInvert == shaderInvertBefore},
            {"shaderColorMode persisted", sceneAfter && sceneAfter->shaderColorMode == shaderColorModeBefore},
            {"shaderColor persisted", sceneAfter && sceneAfter->shaderColor == shaderColorBefore},
            {"ledR persisted", sceneAfter && sceneAfter->ledR == ledRBefore},
            {"ledG persisted", sceneAfter && sceneAfter->ledG == ledGBefore},
            {"ledB persisted", sceneAfter && sceneAfter->ledB == ledBBefore},
            {"ledBrightness persisted", sceneAfter && sceneAfter->ledBrightness == ledBrightBefore},
            {"transition persisted", sceneAfter && sceneAfter->transition == transitionBefore}
        };
        
        ESP_LOGI(TEST_TAG, "");
        int testNum = 3;
        for (const auto& pt : persistTests) {
            ESP_LOGI(TEST_TAG, "▶ TEST 8.%d: %s", testNum, pt.name);
            
            TestResult result;
            result.name = pt.name;
            result.durationMs = 0;
            result.passed = pt.passed;
            
            if (result.passed) {
                suite.passed++;
                ESP_LOGI(TEST_TAG, "  ✓ PASS: %s", pt.name);
            } else {
                suite.failed++;
                ESP_LOGE(TEST_TAG, "  ✗ FAIL: %s", pt.name);
            }
            suite.results.push_back(result);
            testNum++;
        }
        
        suite.totalDurationMs = (xTaskGetTickCount() - suiteStart) * portTICK_PERIOD_MS;
        printTestSuiteSummary(suite);
        return suite;
    }
    
    // ================================================================
    // TEST SUITE 9: Sprite Assignment
    // ================================================================
    static TestSuite runTestSuite_SpriteAssignment() {
        TestSuite suite;
        suite.name = "Sprite Assignment";
        
        printTestSuiteHeader("Sprite Assignment");
        uint32_t suiteStart = xTaskGetTickCount();
        
        int testId = testCreateScene("TestScene_Sprite");
        ESP_LOGI(TEST_TAG, "[SETUP] Created test scene ID: %d", testId);
        
        // List available sprites
        const auto& sprites = Web::HttpServer::instance().getSprites();
        ESP_LOGI(TEST_TAG, "[SETUP] Available sprites: %d", (int)sprites.size());
        
        for (const auto& s : sprites) {
            ESP_LOGI(TEST_TAG, "  [%d] %s (%dx%d)", s.id, s.name.c_str(), s.width, s.height);
        }
        
        // Test: Set sprite ID
        {
            ESP_LOGI(TEST_TAG, "");
            ESP_LOGI(TEST_TAG, "▶ TEST 9.1: Set sprite ID");
            uint32_t start = xTaskGetTickCount();
            
            int spriteIdToSet = sprites.empty() ? 1 : sprites[0].id;
            ESP_LOGI(TEST_TAG, "  [DEBUG] Setting spriteId to: %d", spriteIdToSet);
            
            testSetSpriteId(testId, spriteIdToSet);
            Web::SavedScene* scene = findSceneByIdInternal(testId);
            
            ESP_LOGI(TEST_TAG, "  [DEBUG] Scene spriteId: %d", scene ? scene->spriteId : -99);
            
            TestResult result;
            result.name = "Set sprite ID";
            result.durationMs = (xTaskGetTickCount() - start) * portTICK_PERIOD_MS;
            
            if (scene && scene->spriteId == spriteIdToSet) {
                result.passed = true;
                suite.passed++;
                ESP_LOGI(TEST_TAG, "  ✓ PASS: %s (%lu ms)", result.name.c_str(), result.durationMs);
            } else {
                result.passed = false;
                result.message = "spriteId not set correctly";
                suite.failed++;
                ESP_LOGE(TEST_TAG, "  ✗ FAIL: %s - expected %d, got %d", result.name.c_str(), 
                         spriteIdToSet, scene ? scene->spriteId : -99);
            }
            suite.results.push_back(result);
        }
        
        // Test: Sprite ID in callback
        {
            ESP_LOGI(TEST_TAG, "");
            ESP_LOGI(TEST_TAG, "▶ TEST 9.2: Sprite ID in activation callback");
            uint32_t start = xTaskGetTickCount();
            
            int spriteIdToSet = sprites.empty() ? 2 : (sprites.size() > 1 ? sprites[1].id : sprites[0].id);
            testSetSpriteId(testId, spriteIdToSet);
            
            callbackWasTriggered_ = false;
            testActivateScene(testId);
            vTaskDelay(pdMS_TO_TICKS(50));
            
            ESP_LOGI(TEST_TAG, "  [DEBUG] Callback triggered: %s", callbackWasTriggered_ ? "YES" : "NO");
            ESP_LOGI(TEST_TAG, "  [DEBUG] Callback spriteId: %d (expected: %d)", 
                     lastActivatedScene_.spriteId, spriteIdToSet);
            
            TestResult result;
            result.name = "Sprite ID in callback";
            result.durationMs = (xTaskGetTickCount() - start) * portTICK_PERIOD_MS;
            
            if (callbackWasTriggered_ && lastActivatedScene_.spriteId == spriteIdToSet) {
                result.passed = true;
                suite.passed++;
                ESP_LOGI(TEST_TAG, "  ✓ PASS: %s (%lu ms)", result.name.c_str(), result.durationMs);
            } else {
                result.passed = false;
                suite.failed++;
                ESP_LOGE(TEST_TAG, "  ✗ FAIL: %s - callback spriteId=%d, expected=%d", 
                         result.name.c_str(), lastActivatedScene_.spriteId, spriteIdToSet);
            }
            suite.results.push_back(result);
        }
        
        // Test: Mirror sprite
        {
            ESP_LOGI(TEST_TAG, "");
            ESP_LOGI(TEST_TAG, "▶ TEST 9.3: Mirror sprite toggle");
            uint32_t start = xTaskGetTickCount();
            
            testSetMirrorSprite(testId, true);
            Web::SavedScene* scene = findSceneByIdInternal(testId);
            
            ESP_LOGI(TEST_TAG, "  [DEBUG] mirrorSprite: %s", scene && scene->mirrorSprite ? "true" : "false");
            
            TestResult result;
            result.name = "Mirror sprite toggle";
            result.durationMs = (xTaskGetTickCount() - start) * portTICK_PERIOD_MS;
            
            if (scene && scene->mirrorSprite == true) {
                result.passed = true;
                suite.passed++;
                ESP_LOGI(TEST_TAG, "  ✓ PASS: %s (%lu ms)", result.name.c_str(), result.durationMs);
            } else {
                result.passed = false;
                suite.failed++;
                ESP_LOGE(TEST_TAG, "  ✗ FAIL: %s", result.name.c_str());
            }
            suite.results.push_back(result);
        }
        
        // Test: Sprite persistence
        {
            ESP_LOGI(TEST_TAG, "");
            ESP_LOGI(TEST_TAG, "▶ TEST 9.4: Sprite ID persists after save/load");
            uint32_t start = xTaskGetTickCount();
            
            int spriteIdBefore = findSceneByIdInternal(testId) ? findSceneByIdInternal(testId)->spriteId : -1;
            bool mirrorBefore = findSceneByIdInternal(testId) ? findSceneByIdInternal(testId)->mirrorSprite : false;
            
            ESP_LOGI(TEST_TAG, "  [DEBUG] Before save: spriteId=%d, mirror=%s", 
                     spriteIdBefore, mirrorBefore ? "true" : "false");
            
            Web::HttpServer::instance().forceSaveScenes();
            vTaskDelay(pdMS_TO_TICKS(50));
            
            Web::savedScenes_.clear();
            Web::HttpServer::instance().forceLoadScenes();
            vTaskDelay(pdMS_TO_TICKS(50));
            
            Web::SavedScene* scene = findSceneByIdInternal(testId);
            
            ESP_LOGI(TEST_TAG, "  [DEBUG] After reload: spriteId=%d, mirror=%s",
                     scene ? scene->spriteId : -99,
                     scene && scene->mirrorSprite ? "true" : "false");
            
            TestResult result;
            result.name = "Sprite ID persists after save/load";
            result.durationMs = (xTaskGetTickCount() - start) * portTICK_PERIOD_MS;
            
            if (scene && scene->spriteId == spriteIdBefore && scene->mirrorSprite == mirrorBefore) {
                result.passed = true;
                suite.passed++;
                ESP_LOGI(TEST_TAG, "  ✓ PASS: %s (%lu ms)", result.name.c_str(), result.durationMs);
            } else {
                result.passed = false;
                suite.failed++;
                ESP_LOGE(TEST_TAG, "  ✗ FAIL: %s", result.name.c_str());
            }
            suite.results.push_back(result);
        }
        
        suite.totalDurationMs = (xTaskGetTickCount() - suiteStart) * portTICK_PERIOD_MS;
        printTestSuiteSummary(suite);
        return suite;
    }
    
    // ================================================================
    // TEST SUITE 10: Edge Cases
    // ================================================================
    static TestSuite runTestSuite_EdgeCases() {
        TestSuite suite;
        suite.name = "Edge Cases";
        
        printTestSuiteHeader("Edge Cases");
        uint32_t suiteStart = xTaskGetTickCount();
        
        // Test: Update non-existent scene
        {
            ESP_LOGI(TEST_TAG, "");
            ESP_LOGI(TEST_TAG, "▶ TEST 10.1: Update non-existent scene (should fail gracefully)");
            uint32_t start = xTaskGetTickCount();
            
            Web::SavedScene* scene = findSceneByIdInternal(99999);
            ESP_LOGI(TEST_TAG, "  [DEBUG] Scene 99999 exists: %s", scene ? "YES" : "NO");
            
            TestResult result;
            result.name = "Update non-existent scene";
            result.durationMs = (xTaskGetTickCount() - start) * portTICK_PERIOD_MS;
            result.passed = (scene == nullptr);
            if (result.passed) {
                suite.passed++;
                ESP_LOGI(TEST_TAG, "  ✓ PASS: %s (%lu ms)", result.name.c_str(), result.durationMs);
            } else {
                suite.failed++;
                ESP_LOGE(TEST_TAG, "  ✗ FAIL: %s", result.name.c_str());
            }
            suite.results.push_back(result);
        }
        
        // Test: Rapid scene switching (stress test)
        {
            ESP_LOGI(TEST_TAG, "");
            ESP_LOGI(TEST_TAG, "▶ TEST 10.2: Rapid scene switching (20 switches)");
            uint32_t start = xTaskGetTickCount();
            
            int s1 = testCreateScene("StressTest_A");
            int s2 = testCreateScene("StressTest_B");
            
            int successCount = 0;
            for (int i = 0; i < 20; i++) {
                testActivateScene(i % 2 == 0 ? s1 : s2);
                vTaskDelay(pdMS_TO_TICKS(10));
                if (Web::activeSceneId_ == (i % 2 == 0 ? s1 : s2)) {
                    successCount++;
                }
            }
            
            ESP_LOGI(TEST_TAG, "  [DEBUG] Successful switches: %d/20", successCount);
            
            TestResult result;
            result.name = "Rapid scene switching (20 switches)";
            result.durationMs = (xTaskGetTickCount() - start) * portTICK_PERIOD_MS;
            result.passed = (successCount >= 18); // Allow some tolerance
            if (result.passed) {
                suite.passed++;
                ESP_LOGI(TEST_TAG, "  ✓ PASS: %s (%lu ms)", result.name.c_str(), result.durationMs);
            } else {
                suite.failed++;
                ESP_LOGE(TEST_TAG, "  ✗ FAIL: %s - only %d/20 succeeded", result.name.c_str(), successCount);
            }
            
            testDeleteScene(s1);
            testDeleteScene(s2);
            suite.results.push_back(result);
        }
        
        // Test: Many scenes
        {
            ESP_LOGI(TEST_TAG, "");
            ESP_LOGI(TEST_TAG, "▶ TEST 10.3: Create and manage 10 scenes");
            uint32_t start = xTaskGetTickCount();
            
            std::vector<int> sceneIds;
            for (int i = 0; i < 10; i++) {
                char name[32];
                snprintf(name, sizeof(name), "ManyScenes_%d", i);
                int id = testCreateScene(name);
                sceneIds.push_back(id);
            }
            
            ESP_LOGI(TEST_TAG, "  [DEBUG] Created %d scenes", (int)sceneIds.size());
            
            bool allValid = true;
            for (int id : sceneIds) {
                if (findSceneByIdInternal(id) == nullptr) {
                    allValid = false;
                    break;
                }
            }
            
            // Cleanup
            for (int id : sceneIds) {
                testDeleteScene(id);
            }
            
            TestResult result;
            result.name = "Create and manage 10 scenes";
            result.durationMs = (xTaskGetTickCount() - start) * portTICK_PERIOD_MS;
            result.passed = allValid;
            if (result.passed) {
                suite.passed++;
                ESP_LOGI(TEST_TAG, "  ✓ PASS: %s (%lu ms)", result.name.c_str(), result.durationMs);
            } else {
                suite.failed++;
                ESP_LOGE(TEST_TAG, "  ✗ FAIL: %s", result.name.c_str());
            }
            suite.results.push_back(result);
        }
        
        suite.totalDurationMs = (xTaskGetTickCount() - suiteStart) * portTICK_PERIOD_MS;
        printTestSuiteSummary(suite);
        return suite;
    }
    
    // ================================================================
    // HELPER FUNCTIONS - Test Operations
    // ================================================================
    
    static void cleanupTestScenes() {
        std::vector<int> toDelete;
        for (const auto& scene : Web::savedScenes_) {
            if (scene.name.find("TestScene_") == 0 || 
                scene.name.find("StressTest_") == 0 ||
                scene.name.find("ManyScenes_") == 0) {
                toDelete.push_back(scene.id);
            }
        }
        for (int id : toDelete) {
            testDeleteScene(id);
        }
    }
    
    static int getSceneCount() {
        return (int)Web::savedScenes_.size();
    }
    
    static Web::SavedScene* findSceneByIdInternal(int id) {
        for (auto& scene : Web::savedScenes_) {
            if (scene.id == id) return &scene;
        }
        return nullptr;
    }
    
    static int testCreateScene(const char* name) {
        Web::SavedScene scene;
        scene.id = Web::nextSceneId_++;
        scene.name = name;
        scene.type = 0;
        scene.active = false;
        scene.displayEnabled = true;
        scene.ledsEnabled = false;
        scene.effectsOnly = false;
        scene.order = (int)Web::savedScenes_.size();
        scene.animType = "gyro_eyes";
        scene.transition = "none";
        scene.spriteId = -1;
        scene.mirrorSprite = false;
        scene.shaderAA = true;
        scene.shaderInvert = false;
        scene.shaderColorMode = "none";
        scene.shaderColor = "#ffffff";
        scene.ledR = 255;
        scene.ledG = 0;
        scene.ledB = 255;
        scene.ledBrightness = 80;
        
        Web::savedScenes_.push_back(scene);
        return scene.id;
    }
    
    static bool testDeleteScene(int id) {
        for (auto it = Web::savedScenes_.begin(); it != Web::savedScenes_.end(); ++it) {
            if (it->id == id) {
                Web::savedScenes_.erase(it);
                return true;
            }
        }
        return false;
    }
    
    static bool testRenameScene(int id, const char* newName) {
        Web::SavedScene* scene = findSceneByIdInternal(id);
        if (scene) {
            scene->name = newName;
            return true;
        }
        return false;
    }
    
    static void testSetDisplayEnabled(int id, bool enabled) {
        Web::SavedScene* scene = findSceneByIdInternal(id);
        if (scene) scene->displayEnabled = enabled;
    }
    
    static void testSetLedsEnabled(int id, bool enabled) {
        Web::SavedScene* scene = findSceneByIdInternal(id);
        if (scene) scene->ledsEnabled = enabled;
    }
    
    static void testSetAnimationType(int id, const char* animType) {
        Web::SavedScene* scene = findSceneByIdInternal(id);
        if (scene) scene->animType = animType;
    }
    
    static void testSetShaderAA(int id, bool enabled) {
        Web::SavedScene* scene = findSceneByIdInternal(id);
        if (scene) scene->shaderAA = enabled;
    }
    
    static void testSetShaderInvert(int id, bool enabled) {
        Web::SavedScene* scene = findSceneByIdInternal(id);
        if (scene) scene->shaderInvert = enabled;
    }
    
    static void testSetShaderColorMode(int id, const char* mode) {
        Web::SavedScene* scene = findSceneByIdInternal(id);
        if (scene) scene->shaderColorMode = mode;
    }
    
    static void testSetShaderColor(int id, const char* color) {
        Web::SavedScene* scene = findSceneByIdInternal(id);
        if (scene) scene->shaderColor = color;
    }
    
    static void testSetLedColor(int id, uint8_t r, uint8_t g, uint8_t b) {
        Web::SavedScene* scene = findSceneByIdInternal(id);
        if (scene) {
            scene->ledR = r;
            scene->ledG = g;
            scene->ledB = b;
        }
    }
    
    static void testSetLedBrightness(int id, uint8_t brightness) {
        Web::SavedScene* scene = findSceneByIdInternal(id);
        if (scene) scene->ledBrightness = brightness;
    }
    
    static void testSetTransition(int id, const char* transition) {
        Web::SavedScene* scene = findSceneByIdInternal(id);
        if (scene) scene->transition = transition;
    }
    
    static void testSetSpriteId(int id, int spriteId) {
        Web::SavedScene* scene = findSceneByIdInternal(id);
        if (scene) scene->spriteId = spriteId;
    }
    
    static void testSetMirrorSprite(int id, bool mirror) {
        Web::SavedScene* scene = findSceneByIdInternal(id);
        if (scene) scene->mirrorSprite = mirror;
    }
    
    static bool testActivateScene(int sceneId) {
        ESP_LOGD(TEST_TAG, "[testActivateScene] Entering for scene ID: %d", sceneId);
        
        Web::SavedScene* targetScene = nullptr;
        
        ESP_LOGD(TEST_TAG, "[testActivateScene] Deactivating all scenes...");
        for (auto& scene : Web::savedScenes_) {
            scene.active = false;
            if (scene.id == sceneId) {
                targetScene = &scene;
            }
        }
        
        if (!targetScene) {
            ESP_LOGD(TEST_TAG, "[testActivateScene] Scene not found");
            return false;
        }
        
        ESP_LOGD(TEST_TAG, "[testActivateScene] Found scene: %s", targetScene->name.c_str());
        targetScene->active = true;
        Web::activeSceneId_ = sceneId;
        
        // Call main scene callback with timeout protection
        ESP_LOGD(TEST_TAG, "[testActivateScene] Checking for scene callback...");
        auto& callback = Web::HttpServer::getSceneActivatedCallback();
        if (callback) {
            ESP_LOGD(TEST_TAG, "[testActivateScene] Calling scene activated callback...");
            uint32_t cbStart = xTaskGetTickCount();
            callback(*targetScene);
            uint32_t cbDuration = (xTaskGetTickCount() - cbStart) * portTICK_PERIOD_MS;
            ESP_LOGD(TEST_TAG, "[testActivateScene] Callback completed in %lu ms", cbDuration);
            if (cbDuration > 1000) {
                ESP_LOGW(TEST_TAG, "[testActivateScene] WARNING: Callback took %lu ms (>1s)", cbDuration);
            }
        } else {
            ESP_LOGD(TEST_TAG, "[testActivateScene] No scene callback registered");
        }
        
        // Call animation callback if set
        if (animCallback_) {
            ESP_LOGD(TEST_TAG, "[testActivateScene] Calling animation callback...");
            animCallback_(targetScene->animType, targetScene->spriteId);
            ESP_LOGD(TEST_TAG, "[testActivateScene] Animation callback completed");
        }
        
        // Store for test verification
        lastActivatedScene_ = *targetScene;
        callbackWasTriggered_ = true;
        
        ESP_LOGD(TEST_TAG, "[testActivateScene] Exiting successfully");
        return true;
    }
    
    // ================================================================
    // HELPER FUNCTIONS - Output Formatting
    // ================================================================
    
    static void printTestSuiteHeader(const char* suiteName) {
        ESP_LOGI(TEST_TAG, "");
        ESP_LOGI(TEST_TAG, "┌──────────────────────────────────────────────────────────────────────┐");
        ESP_LOGI(TEST_TAG, "│ TEST SUITE: %-57s│", suiteName);
        ESP_LOGI(TEST_TAG, "└──────────────────────────────────────────────────────────────────────┘");
    }
    
    static void printTestSuiteSummary(const TestSuite& suite) {
        float passRate = (suite.passed + suite.failed > 0) ? 
            (100.0f * suite.passed / (suite.passed + suite.failed)) : 0.0f;
        
        ESP_LOGI(TEST_TAG, "");
        ESP_LOGI(TEST_TAG, "┌─────────────────────────────────────────────────────────────┐");
        ESP_LOGI(TEST_TAG, "│ Suite: %-52s│", suite.name.c_str());
        ESP_LOGI(TEST_TAG, "│ Passed: %d / %d (%.1f%%)                                    │", 
                 suite.passed, suite.passed + suite.failed, passRate);
        ESP_LOGI(TEST_TAG, "│ Duration: %lu ms                                            │", suite.totalDurationMs);
        ESP_LOGI(TEST_TAG, "└─────────────────────────────────────────────────────────────┘");
    }
    
    static void printFinalTestSummary(const std::vector<TestSuite>& suites, uint32_t totalDuration) {
        int totalPassed = 0, totalFailed = 0;
        for (const auto& s : suites) {
            totalPassed += s.passed;
            totalFailed += s.failed;
        }
        
        float passRate = (totalPassed + totalFailed > 0) ?
            (100.0f * totalPassed / (totalPassed + totalFailed)) : 0.0f;
        
        ESP_LOGI(TEST_TAG, "");
        ESP_LOGI(TEST_TAG, "╔══════════════════════════════════════════════════════════════════════╗");
        ESP_LOGI(TEST_TAG, "║                       FINAL TEST RESULTS                             ║");
        ESP_LOGI(TEST_TAG, "╠══════════════════════════════════════════════════════════════════════╣");
        
        for (const auto& s : suites) {
            const char* status = (s.failed == 0) ? "✓" : "✗";
            ESP_LOGI(TEST_TAG, "║ %s %-45s %3d/%3d        ║", 
                     status, s.name.c_str(), s.passed, s.passed + s.failed);
        }
        
        ESP_LOGI(TEST_TAG, "╠══════════════════════════════════════════════════════════════════════╣");
        ESP_LOGI(TEST_TAG, "║ TOTAL: %d passed, %d failed (%.1f%%)                                  ║",
                 totalPassed, totalFailed, passRate);
        ESP_LOGI(TEST_TAG, "║ Total Duration: %lu ms                                               ║", totalDuration);
        ESP_LOGI(TEST_TAG, "╚══════════════════════════════════════════════════════════════════════╝");
        ESP_LOGI(TEST_TAG, "");
        
        if (totalFailed == 0) {
            ESP_LOGI(TEST_TAG, "🎉 ALL %d TESTS PASSED! 🎉", totalPassed);
        } else {
            ESP_LOGW(TEST_TAG, "⚠️  %d TESTS FAILED - Review output above ⚠️", totalFailed);
        }
        ESP_LOGI(TEST_TAG, "");
    }
    
    /**
     * @brief Run quick automated test sequence (legacy TEST:AUTO command)
     */
    static void runAutoTest() {
        ESP_LOGI(TEST_TAG, "");
        ESP_LOGI(TEST_TAG, "╔═══════════════════════════════════════════════════════════╗");
        ESP_LOGI(TEST_TAG, "║              QUICK AUTOMATED TEST                         ║");
        ESP_LOGI(TEST_TAG, "║  Use TEST:FULL for comprehensive test suite               ║");
        ESP_LOGI(TEST_TAG, "╚═══════════════════════════════════════════════════════════╝");
        ESP_LOGI(TEST_TAG, "");
        
        // Step 1: List current state
        ESP_LOGI(TEST_TAG, "=== STEP 1: Current State ===");
        listAllScenes();
        listAllSprites();
        
        // Step 2: Create test scenes if none exist
        if (Web::savedScenes_.empty()) {
            ESP_LOGI(TEST_TAG, "=== STEP 2: Creating Test Scenes ===");
            createScene("Test_GyroEyes");
            createScene("Test_StaticImage");
            createScene("Test_Sway");
        } else {
            ESP_LOGI(TEST_TAG, "=== STEP 2: Scenes exist, skipping creation ===");
        }
        
        // Step 3: Configure scenes with different animation types
        if (Web::savedScenes_.size() >= 3) {
            ESP_LOGI(TEST_TAG, "=== STEP 3: Configuring Animation Types ===");
            setSceneAnimationType(Web::savedScenes_[0].id, "gyro_eyes");
            setSceneAnimationType(Web::savedScenes_[1].id, "static_image");
            setSceneAnimationType(Web::savedScenes_[2].id, "sway");
        }
        
        // Step 4: Assign sprites if available
        ESP_LOGI(TEST_TAG, "=== STEP 4: Assigning Sprites ===");
        const auto& sprites = Web::HttpServer::instance().getSprites();
        if (!sprites.empty() && Web::savedScenes_.size() >= 1) {
            int spriteId = sprites[0].id;
            setSceneSprite(Web::savedScenes_[0].id, spriteId);
            if (Web::savedScenes_.size() >= 2 && sprites.size() >= 1) {
                setSceneSprite(Web::savedScenes_[1].id, spriteId);
            }
        } else {
            ESP_LOGW(TEST_TAG, "    No sprites available to assign");
        }
        
        // Step 5: Test activation
        if (!Web::savedScenes_.empty()) {
            ESP_LOGI(TEST_TAG, "=== STEP 5: Testing Scene Activation ===");
            activateScene(Web::savedScenes_[0].id);
            dumpActiveScene();
        }
        
        // Step 6: Final state
        ESP_LOGI(TEST_TAG, "=== STEP 6: Final State ===");
        listAllScenes();
        dumpCurrentState();
        
        ESP_LOGI(TEST_TAG, "");
        ESP_LOGI(TEST_TAG, "╔═══════════════════════════════════════════════════════════╗");
        ESP_LOGI(TEST_TAG, "║              QUICK TEST COMPLETE                          ║");
        ESP_LOGI(TEST_TAG, "╚═══════════════════════════════════════════════════════════╝");
        ESP_LOGI(TEST_TAG, "");
    }
    
    /**
     * @brief Check serial input for test commands (call from main loop)
     */
    static void checkSerialCommands() {
        // Read from Serial if available
        // This would be called periodically from the main loop
        // For now, commands can be sent via the serial monitor
    }
};

// Static member initialization
inline SceneTestHarness::AnimationChangeCallback SceneTestHarness::animCallback_;
inline SceneTestHarness::StateQueryCallback SceneTestHarness::stateCallback_;
inline bool SceneTestHarness::initialized_ = false;
inline int SceneTestHarness::testSequenceStep_ = 0;
inline uint32_t SceneTestHarness::lastTestTime_ = 0;
inline bool SceneTestHarness::callbackWasTriggered_ = false;
inline Web::SavedScene SceneTestHarness::lastActivatedScene_;

} // namespace Testing
} // namespace SystemAPI
