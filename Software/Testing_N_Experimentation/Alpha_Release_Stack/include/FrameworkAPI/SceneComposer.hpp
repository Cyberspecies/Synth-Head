/**
 * @file SceneComposer.hpp
 * @brief Scene Composition System for Multi-Display Management
 * 
 * Provides a layer-based scene composition system that:
 * - Manages separate scenes for OLED and HUB75 displays
 * - Supports z-ordered layers within each scene
 * - Allows dynamic loading/unloading of scenes and layers
 * - Supports both static and animated content
 * - Uses RAM arrays for efficient temporary data storage
 */

#pragma once

#include <cstdint>
#include <cstring>
#include <cmath>
#include "GpuDriver/GpuCommands.hpp"

namespace SceneAPI {

// ============================================================
// Configuration Constants
// ============================================================
static constexpr int MAX_SCENES = 8;           // Max scenes per display type
static constexpr int MAX_LAYERS_PER_SCENE = 16; // Max layers in a scene
static constexpr int MAX_TEXT_LENGTH = 32;      // Max text string length
static constexpr int MAX_POINTS = 32;           // Max points for polygon/path

// ============================================================
// Enums
// ============================================================

enum class DisplayTarget : uint8_t {
    OLED = 0,
    HUB75 = 1
};

enum class LayerType : uint8_t {
    NONE = 0,
    RECTANGLE,
    FILLED_RECT,
    CIRCLE,
    FILLED_CIRCLE,
    LINE,
    TEXT,
    PIXEL,
    HLINE,
    VLINE,
    SPRITE,
    CUSTOM        // Uses custom draw callback
};

enum class AnimationType : uint8_t {
    STATIC = 0,   // No animation
    LOOP,         // Loops continuously
    PING_PONG,    // Bounces back and forth
    ONCE,         // Plays once then stops
    CUSTOM        // Uses custom update callback
};

// OLED Orientation modes (hardware-accelerated via GPU command)
enum class OledOrientation : uint8_t {
    NORMAL = 0,           // No transform
    ROTATE_180 = 1,       // Rotate 180° (default for some mounting)
    MIRROR_X = 2,         // Horizontal flip
    MIRROR_Y = 3,         // Vertical flip
    MIRROR_XY = 4,        // Mirror X+Y (same as 180°)
    ROTATE_90_CW = 5,     // Rotate 90° clockwise
    ROTATE_90_CCW = 6,    // Rotate 90° counter-clockwise
    ROTATE_90_CW_MIRROR = 7  // Rotate 90° CW + Mirror X
};

// HUB75 Panel transformation (software-based, applied in SceneComposer)
// This is for cases where panels are physically mounted in different orientations
enum class PanelTransform : uint8_t {
    NONE = 0,             // No transform (0,0 = top-left)
    ROTATE_180 = 1,       // Rotate 180° (0,0 = bottom-right)
    MIRROR_X = 2,         // Mirror horizontally
    MIRROR_Y = 3,         // Mirror vertically
    FLIP_XY = 4           // Swap X and Y axes
};

// ============================================================
// Data Structures
// ============================================================

// Color for HUB75 or monochrome for OLED
struct Color {
    uint8_t r = 255;
    uint8_t g = 255;
    uint8_t b = 255;
    
    Color() = default;
    Color(uint8_t _r, uint8_t _g, uint8_t _b) : r(_r), g(_g), b(_b) {}
    
    // For OLED: returns true if color is "on" (any brightness > 50%)
    bool isOn() const { return (r + g + b) > 384; }
    
    static Color White() { return Color(255, 255, 255); }
    static Color Black() { return Color(0, 0, 0); }
    static Color Red() { return Color(255, 0, 0); }
    static Color Green() { return Color(0, 255, 0); }
    static Color Blue() { return Color(0, 0, 255); }
    static Color Yellow() { return Color(255, 255, 0); }
    static Color Cyan() { return Color(0, 255, 255); }
    static Color Magenta() { return Color(255, 0, 255); }
};

// Forward declarations
struct Layer;
struct Scene;

// Animation update callback: returns true if layer needs redraw
typedef bool (*AnimationUpdateFn)(Layer* layer, uint32_t deltaMs, void* userData);

// Custom draw callback for complex shapes
typedef void (*CustomDrawFn)(GpuCommands* gpu, Layer* layer, DisplayTarget target, void* userData);

// Layer definition
struct Layer {
    // Identity
    uint8_t id = 0;
    bool active = false;
    bool visible = true;
    int8_t zOrder = 0;          // Higher = drawn on top (range: -128 to 127)
    
    // Type and appearance
    LayerType type = LayerType::NONE;
    Color color;
    
    // Position and size (in pixels)
    int16_t x = 0;
    int16_t y = 0;
    int16_t width = 0;
    int16_t height = 0;
    int16_t radius = 0;         // For circles
    
    // Secondary position (for lines)
    int16_t x2 = 0;
    int16_t y2 = 0;
    
    // Text data
    char text[MAX_TEXT_LENGTH] = {0};
    
    // Sprite data
    int16_t spriteId = -1;
    
    // Animation
    AnimationType animType = AnimationType::STATIC;
    AnimationUpdateFn updateFn = nullptr;
    void* userData = nullptr;
    
    // Animation state
    float animProgress = 0.0f;  // 0.0 to 1.0
    float animSpeed = 1.0f;     // Multiplier
    bool animForward = true;    // Direction for ping-pong
    
    // Custom draw
    CustomDrawFn drawFn = nullptr;
    
    // Helper methods
    void reset() {
        id = 0;
        active = false;
        visible = true;
        zOrder = 0;
        type = LayerType::NONE;
        color = Color::White();
        x = y = width = height = radius = x2 = y2 = 0;
        memset(text, 0, MAX_TEXT_LENGTH);
        spriteId = -1;
        animType = AnimationType::STATIC;
        updateFn = nullptr;
        userData = nullptr;
        animProgress = 0.0f;
        animSpeed = 1.0f;
        animForward = true;
        drawFn = nullptr;
    }
};

// Scene definition
struct Scene {
    // Identity
    uint8_t id = 0;
    bool active = false;
    char name[16] = {0};
    
    // Display target
    DisplayTarget target = DisplayTarget::OLED;
    
    // Layers (stored in array, sorted by zOrder when rendering)
    Layer layers[MAX_LAYERS_PER_SCENE];
    uint8_t layerCount = 0;
    
    // Scene-level properties
    bool clearBeforeRender = true;
    Color backgroundColor;
    
    // Orientation/Transform (for software-based coordinate transformation)
    PanelTransform panelTransform = PanelTransform::NONE;
    
    // Display dimensions (for transform calculations)
    int16_t displayWidth = 128;
    int16_t displayHeight = 32;  // 32 for HUB75, 128 for OLED
    
    // Animation timing
    uint32_t lastUpdateTime = 0;
    bool paused = false;
    
    // Helper methods
    void reset() {
        id = 0;
        active = false;
        memset(name, 0, sizeof(name));
        target = DisplayTarget::OLED;
        for (int i = 0; i < MAX_LAYERS_PER_SCENE; i++) {
            layers[i].reset();
        }
        layerCount = 0;
        clearBeforeRender = true;
        backgroundColor = Color::Black();
        panelTransform = PanelTransform::NONE;
        displayWidth = 128;
        displayHeight = (target == DisplayTarget::OLED) ? 128 : 32;
        lastUpdateTime = 0;
        paused = false;
    }
    
    Layer* findLayer(uint8_t layerId) {
        for (int i = 0; i < MAX_LAYERS_PER_SCENE; i++) {
            if (layers[i].active && layers[i].id == layerId) {
                return &layers[i];
            }
        }
        return nullptr;
    }
    
    Layer* allocateLayer() {
        for (int i = 0; i < MAX_LAYERS_PER_SCENE; i++) {
            if (!layers[i].active) {
                layers[i].reset();
                layers[i].active = true;
                layers[i].id = i + 1;
                layerCount++;
                return &layers[i];
            }
        }
        return nullptr;
    }
    
    bool removeLayer(uint8_t layerId) {
        Layer* layer = findLayer(layerId);
        if (layer) {
            layer->reset();
            layerCount--;
            return true;
        }
        return false;
    }
};

// ============================================================
// Scene Composer Class
// ============================================================

class SceneComposer {
public:
    SceneComposer() {
        reset();
    }
    
    // Initialize with GPU commands reference
    void init(GpuCommands* gpuCommands) {
        gpu = gpuCommands;
        reset();
    }
    
    // Reset all scenes
    void reset() {
        for (int i = 0; i < MAX_SCENES; i++) {
            oledScenes[i].reset();
            hub75Scenes[i].reset();
        }
        activeOledScene = nullptr;
        activeHub75Scene = nullptr;
    }
    
    // ========== Scene Management ==========
    
    // Create a new scene for the specified display
    Scene* createScene(DisplayTarget target, const char* name = nullptr) {
        Scene* scenes = (target == DisplayTarget::OLED) ? oledScenes : hub75Scenes;
        
        for (int i = 0; i < MAX_SCENES; i++) {
            if (!scenes[i].active) {
                scenes[i].reset();
                scenes[i].active = true;
                scenes[i].id = i + 1;
                scenes[i].target = target;
                if (name) {
                    strncpy(scenes[i].name, name, sizeof(scenes[i].name) - 1);
                }
                return &scenes[i];
            }
        }
        return nullptr;
    }
    
    // Delete a scene by ID
    bool deleteScene(DisplayTarget target, uint8_t sceneId) {
        Scene* scene = findScene(target, sceneId);
        if (scene) {
            // Deactivate if it was the active scene
            if (target == DisplayTarget::OLED && activeOledScene == scene) {
                activeOledScene = nullptr;
            } else if (target == DisplayTarget::HUB75 && activeHub75Scene == scene) {
                activeHub75Scene = nullptr;
            }
            scene->reset();
            return true;
        }
        return false;
    }
    
    // Find scene by ID
    Scene* findScene(DisplayTarget target, uint8_t sceneId) {
        Scene* scenes = (target == DisplayTarget::OLED) ? oledScenes : hub75Scenes;
        for (int i = 0; i < MAX_SCENES; i++) {
            if (scenes[i].active && scenes[i].id == sceneId) {
                return &scenes[i];
            }
        }
        return nullptr;
    }
    
    // Find scene by name
    Scene* findSceneByName(DisplayTarget target, const char* name) {
        Scene* scenes = (target == DisplayTarget::OLED) ? oledScenes : hub75Scenes;
        for (int i = 0; i < MAX_SCENES; i++) {
            if (scenes[i].active && strcmp(scenes[i].name, name) == 0) {
                return &scenes[i];
            }
        }
        return nullptr;
    }
    
    // Set the active scene (what gets rendered)
    void setActiveScene(DisplayTarget target, Scene* scene) {
        if (target == DisplayTarget::OLED) {
            activeOledScene = scene;
        } else {
            activeHub75Scene = scene;
        }
    }
    
    void setActiveSceneById(DisplayTarget target, uint8_t sceneId) {
        setActiveScene(target, findScene(target, sceneId));
    }
    
    // Get active scene
    Scene* getActiveScene(DisplayTarget target) {
        return (target == DisplayTarget::OLED) ? activeOledScene : activeHub75Scene;
    }
    
    // ========== Orientation & Transform Management ==========
    
    // Set OLED hardware orientation (uses GPU command)
    // Mode 0-7 corresponds to OledOrientation enum
    bool setOledOrientation(OledOrientation orientation) {
        if (!gpu) return false;
        gpu->oledSetOrientation(static_cast<uint8_t>(orientation));
        currentOledOrientation = orientation;
        return true;
    }
    
    // Get current OLED orientation
    OledOrientation getOledOrientation() const {
        return currentOledOrientation;
    }
    
    // Set panel transform for a specific scene (software-based)
    // This affects how coordinates are transformed when rendering
    bool setSceneTransform(Scene* scene, PanelTransform transform) {
        if (!scene) return false;
        scene->panelTransform = transform;
        return true;
    }
    
    // Set panel transform for the active scene of a display type
    bool setActiveSceneTransform(DisplayTarget target, PanelTransform transform) {
        Scene* scene = getActiveScene(target);
        if (!scene) return false;
        scene->panelTransform = transform;
        return true;
    }
    
    // Set display dimensions for transform calculations
    void setSceneDisplaySize(Scene* scene, int16_t width, int16_t height) {
        if (!scene) return;
        scene->displayWidth = width;
        scene->displayHeight = height;
    }
    
    // Transform coordinates based on panel transform setting
    void transformCoordinates(const Scene* scene, int16_t& x, int16_t& y) const {
        if (!scene) return;
        
        int16_t w = scene->displayWidth;
        int16_t h = scene->displayHeight;
        int16_t tx = x, ty = y;
        
        switch (scene->panelTransform) {
            case PanelTransform::NONE:
                // No transformation
                break;
                
            case PanelTransform::ROTATE_180:
                // Rotate 180 degrees: (x,y) -> (w-1-x, h-1-y)
                tx = w - 1 - x;
                ty = h - 1 - y;
                break;
                
            case PanelTransform::MIRROR_X:
                // Mirror horizontally: (x,y) -> (w-1-x, y)
                tx = w - 1 - x;
                break;
                
            case PanelTransform::MIRROR_Y:
                // Mirror vertically: (x,y) -> (x, h-1-y)
                ty = h - 1 - y;
                break;
                
            case PanelTransform::FLIP_XY:
                // Swap X and Y: (x,y) -> (y, x)
                tx = y;
                ty = x;
                break;
        }
        
        x = tx;
        y = ty;
    }
    
    // Transform rectangle coordinates and dimensions
    void transformRect(const Scene* scene, int16_t& x, int16_t& y, int16_t& w, int16_t& h) const {
        if (!scene || scene->panelTransform == PanelTransform::NONE) return;
        
        int16_t x2 = x + w - 1;
        int16_t y2 = y + h - 1;
        
        transformCoordinates(scene, x, y);
        transformCoordinates(scene, x2, y2);
        
        // Ensure x < x2 and y < y2
        if (x > x2) { int16_t t = x; x = x2; x2 = t; }
        if (y > y2) { int16_t t = y; y = y2; y2 = t; }
        
        w = x2 - x + 1;
        h = y2 - y + 1;
        
        // Handle FLIP_XY: swap width and height
        if (scene->panelTransform == PanelTransform::FLIP_XY) {
            int16_t temp = w;
            w = h;
            h = temp;
        }
    }
    
    // Transform line endpoints
    void transformLine(const Scene* scene, int16_t& x1, int16_t& y1, int16_t& x2, int16_t& y2) const {
        if (!scene || scene->panelTransform == PanelTransform::NONE) return;
        transformCoordinates(scene, x1, y1);
        transformCoordinates(scene, x2, y2);
    }
    
    // ========== Layer Creation Helpers ==========
    
    // Add a rectangle layer
    Layer* addRectangle(Scene* scene, int16_t x, int16_t y, int16_t w, int16_t h, 
                        Color color = Color::White(), bool filled = false, int8_t zOrder = 0) {
        Layer* layer = scene->allocateLayer();
        if (!layer) return nullptr;
        
        layer->type = filled ? LayerType::FILLED_RECT : LayerType::RECTANGLE;
        layer->x = x;
        layer->y = y;
        layer->width = w;
        layer->height = h;
        layer->color = color;
        layer->zOrder = zOrder;
        return layer;
    }
    
    // Add a circle layer
    Layer* addCircle(Scene* scene, int16_t cx, int16_t cy, int16_t radius,
                     Color color = Color::White(), bool filled = false, int8_t zOrder = 0) {
        Layer* layer = scene->allocateLayer();
        if (!layer) return nullptr;
        
        layer->type = filled ? LayerType::FILLED_CIRCLE : LayerType::CIRCLE;
        layer->x = cx;
        layer->y = cy;
        layer->radius = radius;
        layer->color = color;
        layer->zOrder = zOrder;
        return layer;
    }
    
    // Add a line layer
    Layer* addLine(Scene* scene, int16_t x1, int16_t y1, int16_t x2, int16_t y2,
                   Color color = Color::White(), int8_t zOrder = 0) {
        Layer* layer = scene->allocateLayer();
        if (!layer) return nullptr;
        
        layer->type = LayerType::LINE;
        layer->x = x1;
        layer->y = y1;
        layer->x2 = x2;
        layer->y2 = y2;
        layer->color = color;
        layer->zOrder = zOrder;
        return layer;
    }
    
    // Add a text layer
    Layer* addText(Scene* scene, int16_t x, int16_t y, const char* text,
                   Color color = Color::White(), int8_t zOrder = 0) {
        Layer* layer = scene->allocateLayer();
        if (!layer) return nullptr;
        
        layer->type = LayerType::TEXT;
        layer->x = x;
        layer->y = y;
        strncpy(layer->text, text, MAX_TEXT_LENGTH - 1);
        layer->color = color;
        layer->zOrder = zOrder;
        return layer;
    }
    
    // Add a pixel layer
    Layer* addPixel(Scene* scene, int16_t x, int16_t y, 
                    Color color = Color::White(), int8_t zOrder = 0) {
        Layer* layer = scene->allocateLayer();
        if (!layer) return nullptr;
        
        layer->type = LayerType::PIXEL;
        layer->x = x;
        layer->y = y;
        layer->color = color;
        layer->zOrder = zOrder;
        return layer;
    }
    
    // Add a horizontal line layer
    Layer* addHLine(Scene* scene, int16_t x, int16_t y, int16_t length,
                    Color color = Color::White(), int8_t zOrder = 0) {
        Layer* layer = scene->allocateLayer();
        if (!layer) return nullptr;
        
        layer->type = LayerType::HLINE;
        layer->x = x;
        layer->y = y;
        layer->width = length;
        layer->color = color;
        layer->zOrder = zOrder;
        return layer;
    }
    
    // Add a vertical line layer
    Layer* addVLine(Scene* scene, int16_t x, int16_t y, int16_t length,
                    Color color = Color::White(), int8_t zOrder = 0) {
        Layer* layer = scene->allocateLayer();
        if (!layer) return nullptr;
        
        layer->type = LayerType::VLINE;
        layer->x = x;
        layer->y = y;
        layer->height = length;
        layer->color = color;
        layer->zOrder = zOrder;
        return layer;
    }
    
    // Add a sprite layer
    Layer* addSprite(Scene* scene, int16_t x, int16_t y, int16_t spriteId, int8_t zOrder = 0) {
        Layer* layer = scene->allocateLayer();
        if (!layer) return nullptr;
        
        layer->type = LayerType::SPRITE;
        layer->x = x;
        layer->y = y;
        layer->spriteId = spriteId;
        layer->zOrder = zOrder;
        return layer;
    }
    
    // Add a custom layer with draw callback
    Layer* addCustom(Scene* scene, CustomDrawFn drawFn, void* userData = nullptr, int8_t zOrder = 0) {
        Layer* layer = scene->allocateLayer();
        if (!layer) return nullptr;
        
        layer->type = LayerType::CUSTOM;
        layer->drawFn = drawFn;
        layer->userData = userData;
        layer->zOrder = zOrder;
        return layer;
    }
    
    // ========== Animation ==========
    
    // Set animation on a layer
    void setAnimation(Layer* layer, AnimationType type, float speed = 1.0f, 
                      AnimationUpdateFn updateFn = nullptr, void* userData = nullptr) {
        if (!layer) return;
        layer->animType = type;
        layer->animSpeed = speed;
        layer->updateFn = updateFn;
        layer->userData = userData;
        layer->animProgress = 0.0f;
        layer->animForward = true;
    }
    
    // ========== Update & Render ==========
    
    // Update all active scenes (call each frame with delta time in ms)
    void update(uint32_t currentTimeMs) {
        if (activeOledScene && !activeOledScene->paused) {
            updateScene(activeOledScene, currentTimeMs);
        }
        if (activeHub75Scene && !activeHub75Scene->paused) {
            updateScene(activeHub75Scene, currentTimeMs);
        }
    }
    
    // Render active scenes to displays
    void render() {
        if (!gpu) return;
        
        if (activeOledScene) {
            renderScene(activeOledScene);
        }
        if (activeHub75Scene) {
            renderScene(activeHub75Scene);
        }
    }
    
    // Render and present (convenience method)
    void renderAndPresent() {
        if (!gpu) return;
        
        static uint32_t frameCounter = 0;
        frameCounter++;
        
        if (activeOledScene) {
            renderScene(activeOledScene);
            gpu->oledPresent();
        }
        if (activeHub75Scene) {
            renderScene(activeHub75Scene);
            gpu->hub75Present();
            
            // Debug: log every 60 frames
            if (frameCounter % 60 == 0) {
                ESP_LOGD("SceneComp", "Frame %lu: HUB75 rendered %d layers", 
                        frameCounter, activeHub75Scene->layerCount);
            }
        }
    }
    
    // Render only OLED
    void renderOled() {
        if (!gpu || !activeOledScene) return;
        renderScene(activeOledScene);
    }
    
    // Render only HUB75
    void renderHub75() {
        if (!gpu || !activeHub75Scene) return;
        renderScene(activeHub75Scene);
    }
    
private:
    GpuCommands* gpu = nullptr;
    
    // Scene storage
    Scene oledScenes[MAX_SCENES];
    Scene hub75Scenes[MAX_SCENES];
    
    // Currently active scenes (one per display type)
    Scene* activeOledScene = nullptr;
    Scene* activeHub75Scene = nullptr;
    
    // Current OLED orientation (tracked for getter)
    OledOrientation currentOledOrientation = OledOrientation::NORMAL;
    
    // Temporary array for z-order sorting
    Layer* sortedLayers[MAX_LAYERS_PER_SCENE];
    
    // Update a single scene
    void updateScene(Scene* scene, uint32_t currentTimeMs) {
        uint32_t deltaMs = currentTimeMs - scene->lastUpdateTime;
        scene->lastUpdateTime = currentTimeMs;
        
        for (int i = 0; i < MAX_LAYERS_PER_SCENE; i++) {
            Layer* layer = &scene->layers[i];
            if (!layer->active || !layer->visible) continue;
            if (layer->animType == AnimationType::STATIC) continue;
            
            // Update animation progress
            float progressDelta = (deltaMs / 1000.0f) * layer->animSpeed;
            
            // Always update animProgress first (unless STATIC)
            switch (layer->animType) {
                case AnimationType::LOOP:
                    layer->animProgress += progressDelta;
                    if (layer->animProgress >= 1.0f) {
                        layer->animProgress = fmodf(layer->animProgress, 1.0f);
                    }
                    break;
                    
                case AnimationType::PING_PONG:
                    if (layer->animForward) {
                        layer->animProgress += progressDelta;
                        if (layer->animProgress >= 1.0f) {
                            layer->animProgress = 1.0f;
                            layer->animForward = false;
                        }
                    } else {
                        layer->animProgress -= progressDelta;
                        if (layer->animProgress <= 0.0f) {
                            layer->animProgress = 0.0f;
                            layer->animForward = true;
                        }
                    }
                    break;
                    
                case AnimationType::ONCE:
                    layer->animProgress += progressDelta;
                    if (layer->animProgress >= 1.0f) {
                        layer->animProgress = 1.0f;
                    }
                    break;
                    
                case AnimationType::CUSTOM:
                    // Custom animation manages its own progress
                    break;
                    
                default:
                    break;
            }
            
            // Then call custom update function if present
            if (layer->updateFn) {
                layer->updateFn(layer, deltaMs, layer->userData);
            }
        }
    }
    
    // Render a single scene
    void renderScene(Scene* scene) {
        if (!scene || !gpu) return;
        
        // Clear if needed
        if (scene->clearBeforeRender) {
            if (scene->target == DisplayTarget::OLED) {
                gpu->oledClear();
            } else {
                gpu->hub75Clear(scene->backgroundColor.r, 
                               scene->backgroundColor.g, 
                               scene->backgroundColor.b);
            }
        }
        
        // Collect and sort visible layers by z-order
        int visibleCount = 0;
        for (int i = 0; i < MAX_LAYERS_PER_SCENE; i++) {
            if (scene->layers[i].active && scene->layers[i].visible) {
                sortedLayers[visibleCount++] = &scene->layers[i];
            }
        }
        
        // Simple insertion sort by z-order (lowest first = background)
        for (int i = 1; i < visibleCount; i++) {
            Layer* key = sortedLayers[i];
            int j = i - 1;
            while (j >= 0 && sortedLayers[j]->zOrder > key->zOrder) {
                sortedLayers[j + 1] = sortedLayers[j];
                j--;
            }
            sortedLayers[j + 1] = key;
        }
        
        // Render each layer
        for (int i = 0; i < visibleCount; i++) {
            renderLayer(sortedLayers[i], scene);
        }
    }
    
    // Render a single layer with optional coordinate transformation
    void renderLayer(Layer* layer, const Scene* scene) {
        if (!layer || !gpu || !scene) return;
        
        DisplayTarget target = scene->target;
        bool isOled = (target == DisplayTarget::OLED);
        bool on = layer->color.isOn();
        bool hasTransform = (scene->panelTransform != PanelTransform::NONE);
        
        // Create transformed copies of coordinates
        int16_t x = layer->x;
        int16_t y = layer->y;
        int16_t x2 = layer->x2;
        int16_t y2 = layer->y2;
        int16_t w = layer->width;
        int16_t h = layer->height;
        
        // Apply transforms if needed (for HUB75 software transforms)
        if (hasTransform && !isOled) {
            switch (layer->type) {
                case LayerType::RECTANGLE:
                case LayerType::FILLED_RECT:
                    transformRect(scene, x, y, w, h);
                    break;
                case LayerType::LINE:
                    transformLine(scene, x, y, x2, y2);
                    break;
                case LayerType::HLINE:
                    x2 = x + w - 1;
                    y2 = y;
                    transformLine(scene, x, y, x2, y2);
                    break;
                case LayerType::VLINE:
                    x2 = x;
                    y2 = y + h - 1;
                    transformLine(scene, x, y, x2, y2);
                    break;
                default:
                    transformCoordinates(scene, x, y);
                    break;
            }
        }
        
        switch (layer->type) {
            case LayerType::RECTANGLE:
                if (isOled) {
                    gpu->oledRect(layer->x, layer->y, layer->width, layer->height, on);
                } else {
                    gpu->hub75Rect(x, y, w, h,
                                  layer->color.r, layer->color.g, layer->color.b);
                }
                break;
                
            case LayerType::FILLED_RECT:
                if (isOled) {
                    gpu->oledFill(layer->x, layer->y, layer->width, layer->height, on);
                } else {
                    gpu->hub75Fill(x, y, w, h,
                                  layer->color.r, layer->color.g, layer->color.b);
                }
                break;
                
            case LayerType::CIRCLE:
                if (isOled) {
                    gpu->oledCircle(layer->x, layer->y, layer->radius, on);
                } else {
                    gpu->hub75Circle(x, y, layer->radius,
                                    layer->color.r, layer->color.g, layer->color.b);
                }
                break;
                
            case LayerType::FILLED_CIRCLE:
                if (isOled) {
                    gpu->oledFillCircle(layer->x, layer->y, layer->radius, on);
                } else {
                    // HUB75 doesn't have fillCircle - draw outline only
                    gpu->hub75Circle(x, y, layer->radius,
                                    layer->color.r, layer->color.g, layer->color.b);
                }
                break;
                
            case LayerType::LINE:
                if (isOled) {
                    gpu->oledLine(layer->x, layer->y, layer->x2, layer->y2, on);
                } else {
                    gpu->hub75Line(x, y, x2, y2,
                                  layer->color.r, layer->color.g, layer->color.b);
                }
                break;
                
            case LayerType::TEXT:
                if (isOled) {
                    gpu->oledText(layer->x, layer->y, layer->text);
                } else {
                    gpu->hub75Text(x, y, layer->text,
                                  layer->color.r, layer->color.g, layer->color.b);
                }
                break;
                
            case LayerType::PIXEL:
                if (isOled) {
                    gpu->oledPixel(layer->x, layer->y, on);
                } else {
                    gpu->hub75Pixel(x, y,
                                   layer->color.r, layer->color.g, layer->color.b);
                }
                break;
                
            case LayerType::HLINE:
                if (isOled) {
                    gpu->oledHLine(layer->x, layer->y, layer->width, on);
                } else {
                    // HUB75 - use transformed line
                    gpu->hub75Line(x, y, x2, y2,
                                  layer->color.r, layer->color.g, layer->color.b);
                }
                break;
                
            case LayerType::VLINE:
                if (isOled) {
                    gpu->oledVLine(layer->x, layer->y, layer->height, on);
                } else {
                    // HUB75 - use transformed line
                    gpu->hub75Line(x, y, x2, y2,
                                  layer->color.r, layer->color.g, layer->color.b);
                }
                break;
                
            case LayerType::SPRITE:
                if (layer->spriteId >= 0) {
                    gpu->blitSprite(layer->spriteId, x, y);
                }
                break;
                
            case LayerType::CUSTOM:
                if (layer->drawFn) {
                    layer->drawFn(gpu, layer, target, layer->userData);
                }
                break;
                
            default:
                break;
        }
    }
};

} // namespace SceneAPI
