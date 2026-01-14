/*****************************************************************
 * @file ShaderStack.hpp
 * @brief Stack multiple shaders with blending for complex effects
 *****************************************************************/

#pragma once

#include "ShaderBase.hpp"
#include <memory>

namespace AnimationDriver {

// ============================================================
// Shader Layer - Shader with blend settings
// ============================================================

struct ShaderLayer {
    ShaderBase* shader = nullptr;
    BlendMode blendMode = BlendMode::REPLACE;
    float opacity = 1.0f;
    bool enabled = true;
    
    ShaderLayer() = default;
    ShaderLayer(ShaderBase* s, BlendMode mode = BlendMode::REPLACE, float op = 1.0f)
        : shader(s), blendMode(mode), opacity(op), enabled(true) {}
};

// ============================================================
// Shader Stack - Composite multiple shaders
// ============================================================

class ShaderStack : public ShaderBase {
public:
    static constexpr int MAX_LAYERS = 8;
    
    ShaderStack() : layerCount_(0) {
        setName("ShaderStack");
    }
    
    // Add shader to stack
    ShaderStack& add(ShaderBase* shader, BlendMode mode = BlendMode::REPLACE, float opacity = 1.0f) {
        if (layerCount_ < MAX_LAYERS && shader) {
            layers_[layerCount_++] = ShaderLayer(shader, mode, opacity);
        }
        return *this;
    }
    
    // Add shader with alpha blend (common case)
    ShaderStack& addAlpha(ShaderBase* shader, float opacity) {
        return add(shader, BlendMode::REPLACE, opacity);
    }
    
    // Add additive layer
    ShaderStack& addAdditive(ShaderBase* shader, float opacity = 1.0f) {
        return add(shader, BlendMode::ADD, opacity);
    }
    
    // Add multiply layer
    ShaderStack& addMultiply(ShaderBase* shader, float opacity = 1.0f) {
        return add(shader, BlendMode::MULTIPLY, opacity);
    }
    
    // Get layer for modification
    ShaderLayer* getLayer(int index) {
        return (index >= 0 && index < layerCount_) ? &layers_[index] : nullptr;
    }
    
    // Set layer enabled state
    ShaderStack& setLayerEnabled(int index, bool enabled) {
        if (index >= 0 && index < layerCount_) {
            layers_[index].enabled = enabled;
        }
        return *this;
    }
    
    // Set layer opacity
    ShaderStack& setLayerOpacity(int index, float opacity) {
        if (index >= 0 && index < layerCount_) {
            layers_[index].opacity = opacity;
        }
        return *this;
    }
    
    // Get number of layers
    int getLayerCount() const { return layerCount_; }
    
    // Clear all layers
    void clear() { layerCount_ = 0; }
    
    // Render composite result
    RGB render(const ShaderContext& ctx) override {
        RGB result = baseColor_;
        
        for (int i = 0; i < layerCount_; i++) {
            const ShaderLayer& layer = layers_[i];
            
            if (!layer.enabled || !layer.shader || !layer.shader->isEnabled()) {
                continue;
            }
            
            RGB layerColor = layer.shader->render(ctx);
            float combinedOpacity = layer.opacity * layer.shader->getOpacity();
            
            result = ShaderBlend::blend(result, layerColor, layer.blendMode, combinedOpacity);
        }
        
        return result;
    }
    
    // Update all layers
    void update(float deltaTime) override {
        for (int i = 0; i < layerCount_; i++) {
            if (layers_[i].shader) {
                layers_[i].shader->update(deltaTime);
            }
        }
    }
    
    // Reset all layers
    void reset() override {
        for (int i = 0; i < layerCount_; i++) {
            if (layers_[i].shader) {
                layers_[i].shader->reset();
            }
        }
    }
    
    // Set base color (background)
    ShaderStack& setBaseColor(const RGB& color) {
        baseColor_ = color;
        return *this;
    }
    
private:
    ShaderLayer layers_[MAX_LAYERS];
    int layerCount_;
    RGB baseColor_ = RGB::Black();
};

// ============================================================
// Masked Shader - Apply mask to shader output
// ============================================================

class MaskedShader : public ShaderBase {
public:
    MaskedShader(ShaderBase* source = nullptr, ShaderBase* mask = nullptr)
        : source_(source), mask_(mask), inverted_(false), threshold_(0.5f) {
        setName("MaskedShader");
    }
    
    MaskedShader& setSource(ShaderBase* shader) {
        source_ = shader;
        return *this;
    }
    
    MaskedShader& setMask(ShaderBase* shader) {
        mask_ = shader;
        return *this;
    }
    
    MaskedShader& setInverted(bool inverted) {
        inverted_ = inverted;
        return *this;
    }
    
    MaskedShader& setThreshold(float threshold) {
        threshold_ = threshold;
        return *this;
    }
    
    MaskedShader& setBackgroundColor(const RGB& color) {
        bgColor_ = color;
        return *this;
    }
    
    RGB render(const ShaderContext& ctx) override {
        if (!source_) return bgColor_;
        
        RGB sourceColor = source_->render(ctx);
        
        if (!mask_) return sourceColor;
        
        // Get mask luminance as alpha
        RGB maskColor = mask_->render(ctx);
        float luminance = (maskColor.r * 0.299f + maskColor.g * 0.587f + maskColor.b * 0.114f) / 255.0f;
        
        if (inverted_) {
            luminance = 1.0f - luminance;
        }
        
        // Apply threshold or use as alpha
        if (threshold_ >= 0.0f) {
            return (luminance >= threshold_) ? sourceColor : bgColor_;
        } else {
            return ColorBlend::alpha(bgColor_, sourceColor, luminance);
        }
    }
    
    void update(float deltaTime) override {
        if (source_) source_->update(deltaTime);
        if (mask_) mask_->update(deltaTime);
    }
    
private:
    ShaderBase* source_;
    ShaderBase* mask_;
    bool inverted_;
    float threshold_;
    RGB bgColor_ = RGB::Black();
};

// ============================================================
// Region Shader - Apply shader to specific region
// ============================================================

class RegionShader : public ShaderBase {
public:
    struct Region {
        float x1, y1, x2, y2;  // Normalized coordinates
        
        Region() : x1(0), y1(0), x2(1), y2(1) {}
        Region(float _x1, float _y1, float _x2, float _y2)
            : x1(_x1), y1(_y1), x2(_x2), y2(_y2) {}
        
        bool contains(float x, float y) const {
            return x >= x1 && x <= x2 && y >= y1 && y <= y2;
        }
        
        // Map global coords to local region coords
        float localX(float x) const {
            return (x2 > x1) ? (x - x1) / (x2 - x1) : 0.0f;
        }
        
        float localY(float y) const {
            return (y2 > y1) ? (y - y1) / (y2 - y1) : 0.0f;
        }
    };
    
    RegionShader(ShaderBase* shader = nullptr) : shader_(shader) {
        setName("RegionShader");
    }
    
    RegionShader& setShader(ShaderBase* shader) {
        shader_ = shader;
        return *this;
    }
    
    RegionShader& setRegion(const Region& region) {
        region_ = region;
        return *this;
    }
    
    RegionShader& setRegion(float x1, float y1, float x2, float y2) {
        region_ = Region(x1, y1, x2, y2);
        return *this;
    }
    
    RegionShader& setBackgroundColor(const RGB& color) {
        bgColor_ = color;
        return *this;
    }
    
    RegionShader& setLocalCoords(bool local) {
        useLocalCoords_ = local;
        return *this;
    }
    
    RGB render(const ShaderContext& ctx) override {
        if (!shader_) return bgColor_;
        
        if (!region_.contains(ctx.x, ctx.y)) {
            return bgColor_;
        }
        
        if (useLocalCoords_) {
            // Create modified context with local coordinates
            ShaderContext localCtx = ctx;
            localCtx.x = region_.localX(ctx.x);
            localCtx.y = region_.localY(ctx.y);
            return shader_->render(localCtx);
        }
        
        return shader_->render(ctx);
    }
    
    void update(float deltaTime) override {
        if (shader_) shader_->update(deltaTime);
    }
    
private:
    ShaderBase* shader_;
    Region region_;
    RGB bgColor_ = RGB::Black();
    bool useLocalCoords_ = true;
};

} // namespace AnimationDriver
