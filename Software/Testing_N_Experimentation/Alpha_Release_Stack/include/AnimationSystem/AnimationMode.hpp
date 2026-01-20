/*****************************************************************
 * @file AnimationMode.hpp
 * @brief Animation Mode Handler
 * 
 * Manages the lifecycle of animations, similar to how BootMode
 * and CurrentMode work in the main lifecycle.
 * 
 * Handles:
 * - Active animation set selection
 * - Animation updating
 * - Render output to GPU
 * - Scene composition
 * 
 * @author ARCOS
 * @version 2.0
 *****************************************************************/

#pragma once

#include "AnimationSet.hpp"
#include "AnimationContext.hpp"
#include "ParameterRegistry.hpp"
#include <string>
#include <functional>

namespace AnimationSystem {

// ============================================================
// GPU Output Adapter
// ============================================================

/**
 * @brief Callback types for GPU commands
 */
using GpuClearCallback = std::function<void(uint8_t r, uint8_t g, uint8_t b)>;
using GpuPixelCallback = std::function<void(int x, int y, uint8_t r, uint8_t g, uint8_t b)>;
using GpuRectCallback = std::function<void(int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b)>;
using GpuCircleCallback = std::function<void(int cx, int cy, int radius, uint8_t r, uint8_t g, uint8_t b)>;
using GpuSpriteCallback = std::function<void(int spriteId, float x, float y)>;
using GpuSpriteRotatedCallback = std::function<void(int spriteId, float x, float y, float angle)>;
using GpuSpriteScaledCallback = std::function<void(int spriteId, float x, float y, float scale)>;
using GpuPresentCallback = std::function<void()>;

/**
 * @brief GPU output adapter that routes to callbacks
 */
class GpuOutputAdapter : public IRenderOutput {
public:
    void clear(uint8_t r, uint8_t g, uint8_t b) override {
        if (onClear) onClear(r, g, b);
    }
    
    void setPixel(int x, int y, uint8_t r, uint8_t g, uint8_t b) override {
        if (onPixel) onPixel(x, y, r, g, b);
    }
    
    void drawRect(int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b) override {
        if (onDrawRect) onDrawRect(x, y, w, h, r, g, b);
    }
    
    void fillRect(int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b) override {
        if (onFillRect) onFillRect(x, y, w, h, r, g, b);
    }
    
    void drawCircle(int cx, int cy, int radius, uint8_t r, uint8_t g, uint8_t b) override {
        if (onDrawCircle) onDrawCircle(cx, cy, radius, r, g, b);
    }
    
    void fillCircle(int cx, int cy, int radius, uint8_t r, uint8_t g, uint8_t b) override {
        if (onFillCircle) onFillCircle(cx, cy, radius, r, g, b);
    }
    
    void blitSprite(int spriteId, float x, float y) override {
        if (onBlitSprite) onBlitSprite(spriteId, x, y);
    }
    
    void blitSpriteRotated(int spriteId, float x, float y, float angle) override {
        if (onBlitSpriteRotated) onBlitSpriteRotated(spriteId, x, y, angle);
    }
    
    void blitSpriteScaled(int spriteId, float x, float y, float scale) override {
        if (onBlitSpriteScaled) onBlitSpriteScaled(spriteId, x, y, scale);
    }
    
    void present() override {
        if (onPresent) onPresent();
    }
    
    // Callbacks
    GpuClearCallback onClear;
    GpuPixelCallback onPixel;
    GpuRectCallback onDrawRect;
    GpuRectCallback onFillRect;
    GpuCircleCallback onDrawCircle;
    GpuCircleCallback onFillCircle;
    GpuSpriteCallback onBlitSprite;
    GpuSpriteRotatedCallback onBlitSpriteRotated;
    GpuSpriteScaledCallback onBlitSpriteScaled;
    GpuPresentCallback onPresent;
};

// ============================================================
// Animation Mode
// ============================================================

class AnimationMode {
public:
    AnimationMode() = default;
    
    /**
     * @brief Initialize animation mode
     */
    void init() {
        initialized_ = true;
    }
    
    /**
     * @brief Set the context to use
     */
    void setContext(AnimationContext* context) {
        context_ = context;
    }
    
    /**
     * @brief Set the parameter registry
     */
    void setRegistry(ParameterRegistry* registry) {
        registry_ = registry;
    }
    
    /**
     * @brief Get the GPU output adapter for wiring to actual GPU
     */
    GpuOutputAdapter& getOutputAdapter() {
        return outputAdapter_;
    }
    
    // ========================================================
    // Animation Set Management
    // ========================================================
    
    /**
     * @brief Set the active animation set by ID
     */
    bool setActiveAnimationSet(const std::string& id) {
        if (!registry_) return false;
        
        // Cleanup previous animation
        if (activeSet_) {
            activeSet_->setActive(false);
            activeSet_->cleanup();
        }
        
        // Get new animation set
        activeSet_ = registry_->getAnimationSet(id);
        if (activeSet_) {
            activeSetId_ = id;
            activeSet_->init(context_);
            activeSet_->setActive(true);
            return true;
        }
        
        activeSetId_.clear();
        return false;
    }
    
    /**
     * @brief Get active animation set ID
     */
    const std::string& getActiveAnimationSetId() const {
        return activeSetId_;
    }
    
    /**
     * @brief Get active animation set
     */
    AnimationSet* getActiveAnimationSet() {
        return activeSet_;
    }
    
    /**
     * @brief Check if animation is enabled
     */
    bool isEnabled() const { return enabled_; }
    
    /**
     * @brief Enable/disable animation
     */
    void setEnabled(bool enabled) {
        enabled_ = enabled;
    }
    
    /**
     * @brief Activate an animation set by ID (alias for setActiveAnimationSet + enable)
     */
    bool activateAnimationSet(const std::string& id) {
        if (setActiveAnimationSet(id)) {
            enabled_ = true;
            return true;
        }
        return false;
    }
    
    /**
     * @brief Stop the current animation
     */
    void stop() {
        enabled_ = false;
        if (activeSet_) {
            activeSet_->setActive(false);
        }
    }
    
    // ========================================================
    // Update and Render
    // ========================================================
    
    /**
     * @brief Update animation (call every frame)
     * @param deltaTimeMs Time since last update
     */
    void update(uint32_t deltaTimeMs) {
        if (!enabled_ || !activeSet_) return;
        
        // Update animation set
        activeSet_->update(deltaTimeMs);
    }
    
    /**
     * @brief Render animation to GPU
     * Call this after update to send commands to GPU
     */
    void render() {
        if (!enabled_ || !activeSet_) return;
        
        // Render animation set
        activeSet_->render(&outputAdapter_);
        
        frameCount_++;
    }
    
    // ========================================================
    // Stats
    // ========================================================
    
    uint32_t getFrameCount() const { return frameCount_; }
    
    // ========================================================
    // Scene Export
    // ========================================================
    
    /**
     * @brief Export current scene configuration as JSON
     * Includes active animation set and all parameter values
     */
    std::string exportSceneJson() const {
        std::string json = "{";
        
        // Active animation set
        json += "\"activeSet\":\"" + activeSetId_ + "\",";
        json += "\"enabled\":" + std::string(enabled_ ? "true" : "false") + ",";
        
        // Parameter values for active set
        json += "\"params\":";
        if (activeSet_) {
            json += "[";
            const auto& params = activeSet_->getParameters();
            bool first = true;
            for (const auto& p : params) {
                if (!first) json += ",";
                first = false;
                
                json += "{\"id\":\"" + p.id + "\",";
                
                // Export based on type
                switch (p.type) {
                    case ParameterType::SLIDER:
                        json += "\"value\":" + floatToStr(p.floatValue);
                        break;
                    case ParameterType::SLIDER_INT:
                    case ParameterType::DROPDOWN:
                    case ParameterType::SPRITE_SELECT:
                    case ParameterType::EQUATION_SELECT:
                        json += "\"value\":" + std::to_string(p.intValue);
                        break;
                    case ParameterType::TOGGLE:
                        json += "\"value\":" + std::string(p.boolValue ? "true" : "false");
                        break;
                    case ParameterType::COLOR:
                        json += "\"r\":" + std::to_string(p.colorR) + ",";
                        json += "\"g\":" + std::to_string(p.colorG) + ",";
                        json += "\"b\":" + std::to_string(p.colorB);
                        break;
                    case ParameterType::INPUT_SELECT:
                    case ParameterType::TEXT:
                        json += "\"value\":\"" + p.stringValue + "\"";
                        break;
                    default:
                        json += "\"value\":null";
                        break;
                }
                
                json += "}";
            }
            json += "]";
        } else {
            json += "[]";
        }
        
        json += "}";
        return json;
    }

private:
    AnimationContext* context_ = nullptr;
    ParameterRegistry* registry_ = nullptr;
    AnimationSet* activeSet_ = nullptr;
    std::string activeSetId_;
    
    GpuOutputAdapter outputAdapter_;
    
    bool initialized_ = false;
    bool enabled_ = false;
    uint32_t frameCount_ = 0;
    
    static std::string floatToStr(float value) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.4f", value);
        return buf;
    }
};

} // namespace AnimationSystem
