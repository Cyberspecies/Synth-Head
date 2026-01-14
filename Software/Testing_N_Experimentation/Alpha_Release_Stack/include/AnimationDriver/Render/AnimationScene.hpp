/*****************************************************************
 * @file AnimationScene.hpp
 * @brief High-level scene that combines shaders, animations, bindings
 * 
 * A scene represents a complete animated visual setup that can be
 * rendered to a display target.
 *****************************************************************/

#pragma once

#include "../Core/Types.hpp"
#include "../Core/Color.hpp"
#include "../Shaders/Shaders.hpp"
#include "../Animation/Animation.hpp"
#include "../Binding/Bindings.hpp"
#include "RenderTarget.hpp"
#include <cstring>

namespace AnimationDriver {

// ============================================================
// Scene Layer - Shader with animation and binding support
// ============================================================

struct SceneLayer {
    ShaderBase* shader = nullptr;
    AnimationClip* animation = nullptr;
    BlendMode blendMode = BlendMode::REPLACE;
    float opacity = 1.0f;
    bool enabled = true;
    char name[24] = {0};
    
    SceneLayer() = default;
    
    SceneLayer& setName(const char* n) {
        strncpy(name, n, sizeof(name) - 1);
        return *this;
    }
};

// ============================================================
// Animation Scene
// ============================================================

class AnimationScene {
public:
    static constexpr int MAX_LAYERS = 8;
    static constexpr int MAX_NAME_LEN = 32;
    
    AnimationScene() : layerCount_(0), enabled_(true), time_(0.0f) {
        name_[0] = '\0';
    }
    
    AnimationScene(const char* name) : AnimationScene() {
        setName(name);
    }
    
    // Naming
    AnimationScene& setName(const char* name) {
        strncpy(name_, name, MAX_NAME_LEN - 1);
        name_[MAX_NAME_LEN - 1] = '\0';
        return *this;
    }
    
    const char* getName() const { return name_; }
    
    // Add layers
    AnimationScene& addLayer(ShaderBase* shader, BlendMode mode = BlendMode::REPLACE, float opacity = 1.0f) {
        if (layerCount_ < MAX_LAYERS && shader) {
            layers_[layerCount_].shader = shader;
            layers_[layerCount_].blendMode = mode;
            layers_[layerCount_].opacity = opacity;
            layers_[layerCount_].enabled = true;
            layerCount_++;
        }
        return *this;
    }
    
    AnimationScene& addLayer(const char* name, ShaderBase* shader, BlendMode mode = BlendMode::REPLACE) {
        if (layerCount_ < MAX_LAYERS && shader) {
            layers_[layerCount_].setName(name);
            layers_[layerCount_].shader = shader;
            layers_[layerCount_].blendMode = mode;
            layers_[layerCount_].enabled = true;
            layerCount_++;
        }
        return *this;
    }
    
    // Get layer access
    SceneLayer* getLayer(int index) {
        return (index >= 0 && index < layerCount_) ? &layers_[index] : nullptr;
    }
    
    SceneLayer* getLayer(const char* name) {
        for (int i = 0; i < layerCount_; i++) {
            if (strncmp(layers_[i].name, name, sizeof(layers_[i].name)) == 0) {
                return &layers_[i];
            }
        }
        return nullptr;
    }
    
    // Layer manipulation
    AnimationScene& setLayerEnabled(int index, bool enabled) {
        if (index >= 0 && index < layerCount_) {
            layers_[index].enabled = enabled;
        }
        return *this;
    }
    
    AnimationScene& setLayerOpacity(int index, float opacity) {
        if (index >= 0 && index < layerCount_) {
            layers_[index].opacity = opacity;
        }
        return *this;
    }
    
    // Update scene
    void update(float deltaTime) {
        if (!enabled_) return;
        
        time_ += deltaTime;
        
        // Update all layers
        for (int i = 0; i < layerCount_; i++) {
            if (layers_[i].enabled) {
                if (layers_[i].shader) {
                    layers_[i].shader->update(deltaTime);
                }
                if (layers_[i].animation) {
                    layers_[i].animation->update(deltaTime);
                }
            }
        }
    }
    
    // Render to target
    void render(RenderTarget* target) {
        if (!enabled_ || !target) return;
        
        int width = target->getWidth();
        int height = target->getHeight();
        ShaderContext& ctx = target->getContext();
        FrameBuffer* buffer = target->getBuffer();
        
        // Clear with background
        target->clear(backgroundColor_);
        
        // Render each layer
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                ctx.setPixel(x, y, width, height);
                ctx.time = time_;
                
                RGB result = backgroundColor_;
                
                for (int i = 0; i < layerCount_; i++) {
                    const SceneLayer& layer = layers_[i];
                    
                    if (!layer.enabled || !layer.shader || !layer.shader->isEnabled()) {
                        continue;
                    }
                    
                    RGB layerColor = layer.shader->render(ctx);
                    float combinedOpacity = layer.opacity * layer.shader->getOpacity();
                    
                    result = ShaderBlend::blend(result, layerColor, layer.blendMode, combinedOpacity);
                }
                
                buffer->setPixel(x, y, result);
            }
        }
    }
    
    // Scene control
    AnimationScene& setEnabled(bool enabled) {
        enabled_ = enabled;
        return *this;
    }
    
    AnimationScene& setBackgroundColor(const RGB& color) {
        backgroundColor_ = color;
        return *this;
    }
    
    // Reset scene
    void reset() {
        time_ = 0.0f;
        for (int i = 0; i < layerCount_; i++) {
            if (layers_[i].shader) {
                layers_[i].shader->reset();
            }
            if (layers_[i].animation) {
                layers_[i].animation->stop();
            }
        }
    }
    
    // State queries
    bool isEnabled() const { return enabled_; }
    float getTime() const { return time_; }
    int getLayerCount() const { return layerCount_; }
    
private:
    char name_[MAX_NAME_LEN];
    SceneLayer layers_[MAX_LAYERS];
    int layerCount_;
    bool enabled_;
    float time_;
    RGB backgroundColor_ = RGB::Black();
};

} // namespace AnimationDriver
