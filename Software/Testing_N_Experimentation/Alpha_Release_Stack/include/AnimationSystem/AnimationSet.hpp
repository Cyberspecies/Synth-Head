/*****************************************************************
 * @file AnimationSet.hpp
 * @brief Animation Set Base Class
 * 
 * An AnimationSet is a collection of related animations that share
 * parameters. It provides:
 * - List of configurable parameters (for auto-generated UI)
 * - Update function (called each frame)
 * - Render function (outputs to GPU driver)
 * 
 * Animation sets register themselves with the ParameterRegistry
 * so the web UI can query and display their parameters.
 * 
 * @author ARCOS
 * @version 2.0
 *****************************************************************/

#pragma once

#include "ParameterDef.hpp"
#include "AnimationContext.hpp"
#include <vector>
#include <string>
#include <functional>

namespace AnimationSystem {

// Forward declaration
class AnimationMode;

// ============================================================
// Render Output Interface
// ============================================================

/**
 * @brief Interface for GPU commands output
 * Animation sets use this to send render commands
 */
class IRenderOutput {
public:
    virtual ~IRenderOutput() = default;
    
    // Basic drawing
    virtual void clear(uint8_t r, uint8_t g, uint8_t b) = 0;
    virtual void setPixel(int x, int y, uint8_t r, uint8_t g, uint8_t b) = 0;
    virtual void drawRect(int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b) = 0;
    virtual void fillRect(int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b) = 0;
    virtual void drawCircle(int cx, int cy, int radius, uint8_t r, uint8_t g, uint8_t b) = 0;
    virtual void fillCircle(int cx, int cy, int radius, uint8_t r, uint8_t g, uint8_t b) = 0;
    
    // Sprite operations
    virtual void blitSprite(int spriteId, float x, float y) = 0;
    virtual void blitSpriteRotated(int spriteId, float x, float y, float angle) = 0;
    virtual void blitSpriteScaled(int spriteId, float x, float y, float scale) = 0;
    
    // Present frame
    virtual void present() = 0;
};

// ============================================================
// Animation Set Base Class
// ============================================================

/**
 * @brief Base class for all animation sets
 */
class AnimationSet {
public:
    AnimationSet() = default;
    virtual ~AnimationSet() = default;
    
    // ========================================================
    // Identification
    // ========================================================
    
    /**
     * @brief Get unique identifier for this animation set
     */
    virtual const char* getId() const = 0;
    
    /**
     * @brief Get display name
     */
    virtual const char* getName() const = 0;
    
    /**
     * @brief Get description
     */
    virtual const char* getDescription() const { return ""; }
    
    /**
     * @brief Get category/group name
     */
    virtual const char* getCategory() const { return "General"; }
    
    // ========================================================
    // Parameters
    // ========================================================
    
    /**
     * @brief Get list of configurable parameters
     * Called by settings generator to build UI
     */
    virtual std::vector<ParameterDef>& getParameters() {
        return parameters_;
    }
    
    /**
     * @brief Get parameter by ID
     */
    ParameterDef* getParameter(const char* id) {
        for (auto& p : parameters_) {
            if (p.id == id) return &p;
        }
        return nullptr;
    }
    
    /**
     * @brief Set parameter value (from web UI)
     * @return true if parameter found and updated
     */
    virtual bool setParameterValue(const char* id, float value) {
        auto* p = getParameter(id);
        if (p) {
            p->floatValue = value;
            p->intValue = static_cast<int>(value);
            onParameterChanged(id);
            return true;
        }
        return false;
    }
    
    virtual bool setParameterValue(const char* id, int value) {
        auto* p = getParameter(id);
        if (p) {
            p->intValue = value;
            p->floatValue = static_cast<float>(value);
            onParameterChanged(id);
            return true;
        }
        return false;
    }
    
    virtual bool setParameterValue(const char* id, bool value) {
        auto* p = getParameter(id);
        if (p) {
            p->boolValue = value;
            onParameterChanged(id);
            return true;
        }
        return false;
    }
    
    virtual bool setParameterValue(const char* id, const std::string& value) {
        auto* p = getParameter(id);
        if (p) {
            p->stringValue = value;
            onParameterChanged(id);
            return true;
        }
        return false;
    }
    
    /**
     * @brief Called when a parameter changes
     * Override to handle specific parameter updates
     */
    virtual void onParameterChanged(const char* id) {}
    
    // ========================================================
    // Lifecycle
    // ========================================================
    
    /**
     * @brief Initialize the animation set
     * Called when animation becomes active
     */
    virtual void init(AnimationContext* context) {
        context_ = context;
    }
    
    /**
     * @brief Update animation state
     * @param deltaTimeMs Time since last update
     */
    virtual void update(uint32_t deltaTimeMs) = 0;
    
    /**
     * @brief Render the animation
     * @param output GPU render output interface
     */
    virtual void render(IRenderOutput* output) = 0;
    
    /**
     * @brief Cleanup when animation becomes inactive
     */
    virtual void cleanup() {}
    
    // ========================================================
    // State
    // ========================================================
    
    bool isActive() const { return active_; }
    void setActive(bool active) { active_ = active; }
    
    /**
     * @brief Reset all parameters to default values
     */
    virtual void resetToDefaults() {
        for (auto& p : parameters_) {
            p.floatValue = p.defaultValue;
            p.intValue = static_cast<int>(p.defaultValue);
            p.boolValue = (p.defaultValue != 0);
        }
    }
    
protected:
    std::vector<ParameterDef> parameters_;
    AnimationContext* context_ = nullptr;
    bool active_ = false;
};

// ============================================================
// Built-in Animation Sets
// ============================================================

/**
 * @brief Gyro-controlled eye animation
 * Uses IMU pitch/roll to move pupil sprites
 */
class GyroEyeAnimationSet : public AnimationSet {
public:
    const char* getId() const override { return "gyro_eye"; }
    const char* getName() const override { return "Gyro Eye"; }
    const char* getDescription() const override {
        return "Eye animation that tracks head movement using IMU";
    }
    const char* getCategory() const override { return "Eyes"; }
    
    GyroEyeAnimationSet() {
        // Define all parameters
        parameters_ = {
            // Position settings
            ParameterDef::Separator("Eye Positions"),
            ParameterDef::SliderInt("left_eye_x", "Left Eye X", 0, 128, 32, "px")
                .withCategory(ParameterCategory::POSITION),
            ParameterDef::SliderInt("left_eye_y", "Left Eye Y", 0, 32, 16, "px")
                .withCategory(ParameterCategory::POSITION),
            ParameterDef::SliderInt("right_eye_x", "Right Eye X", 0, 128, 96, "px")
                .withCategory(ParameterCategory::POSITION),
            ParameterDef::SliderInt("right_eye_y", "Right Eye Y", 0, 32, 16, "px")
                .withCategory(ParameterCategory::POSITION),
            
            // Movement settings
            ParameterDef::Separator("Movement"),
            ParameterDef::Slider("intensity", "Movement Intensity", 0.0f, 3.0f, 1.0f)
                .withCategory(ParameterCategory::MOVEMENT)
                .withDescription("How much the eyes move in response to head tilt"),
            ParameterDef::Slider("max_offset_x", "Max X Offset", 0.0f, 32.0f, 8.0f, "px")
                .withCategory(ParameterCategory::MOVEMENT),
            ParameterDef::Slider("max_offset_y", "Max Y Offset", 0.0f, 16.0f, 6.0f, "px")
                .withCategory(ParameterCategory::MOVEMENT),
            ParameterDef::Slider("smoothing", "Smoothing", 0.0f, 1.0f, 0.15f)
                .withCategory(ParameterCategory::MOVEMENT)
                .withDescription("Higher = smoother but slower response"),
            
            // Sprite selection
            ParameterDef::Separator("Appearance"),
            ParameterDef::SpriteSelect("pupil_sprite", "Pupil Sprite", -1)
                .withDescription("Select sprite for pupil, or use default circle"),
            ParameterDef::SliderInt("pupil_size", "Pupil Size", 2, 16, 6, "px"),
            
            // Background color
            ParameterDef::Color("bg_color", "Background", 0, 0, 0),
            ParameterDef::Color("eye_color", "Eye Color", 255, 255, 255),
            
            // Input bindings
            ParameterDef::Separator("Input Bindings"),
            ParameterDef::InputSelect("pitch_input", "Pitch Input", "imu.pitch"),
            ParameterDef::InputSelect("roll_input", "Roll Input", "imu.roll"),
            ParameterDef::Toggle("invert_pitch", "Invert Pitch", false),
            ParameterDef::Toggle("invert_roll", "Invert Roll", false),
        };
    }
    
    void update(uint32_t deltaTimeMs) override {
        if (!context_) return;
        
        // Get input values
        std::string pitchInput = getParameter("pitch_input")->stringValue;
        std::string rollInput = getParameter("roll_input")->stringValue;
        
        float pitch = context_->getInput(pitchInput.c_str());
        float roll = context_->getInput(rollInput.c_str());
        
        // Apply inversion
        if (getParameter("invert_pitch")->boolValue) pitch = -pitch;
        if (getParameter("invert_roll")->boolValue) roll = -roll;
        
        // Get movement parameters
        float intensity = getParameter("intensity")->floatValue;
        float maxOffsetX = getParameter("max_offset_x")->floatValue;
        float maxOffsetY = getParameter("max_offset_y")->floatValue;
        float smoothing = getParameter("smoothing")->floatValue;
        
        // Calculate target offset (normalize pitch/roll to -1..1 range)
        float targetX = (roll / 90.0f) * maxOffsetX * intensity;
        float targetY = (pitch / 90.0f) * maxOffsetY * intensity;
        
        // Apply smoothing
        currentOffsetX_ += (targetX - currentOffsetX_) * (1.0f - smoothing);
        currentOffsetY_ += (targetY - currentOffsetY_) * (1.0f - smoothing);
    }
    
    void render(IRenderOutput* output) override {
        // Get position parameters
        int leftX = getParameter("left_eye_x")->intValue;
        int leftY = getParameter("left_eye_y")->intValue;
        int rightX = getParameter("right_eye_x")->intValue;
        int rightY = getParameter("right_eye_y")->intValue;
        int pupilSize = getParameter("pupil_size")->intValue;
        int spriteId = getParameter("pupil_sprite")->intValue;
        
        // Get colors
        auto* bgColor = getParameter("bg_color");
        auto* eyeColor = getParameter("eye_color");
        
        // Clear background
        output->clear(bgColor->colorR, bgColor->colorG, bgColor->colorB);
        
        // Calculate pupil positions
        int leftPupilX = leftX + static_cast<int>(currentOffsetX_);
        int leftPupilY = leftY + static_cast<int>(currentOffsetY_);
        int rightPupilX = rightX + static_cast<int>(currentOffsetX_);
        int rightPupilY = rightY + static_cast<int>(currentOffsetY_);
        
        // Render pupils
        if (spriteId >= 0) {
            // Use sprite
            output->blitSprite(spriteId, leftPupilX, leftPupilY);
            output->blitSprite(spriteId, rightPupilX, rightPupilY);
        } else {
            // Use circles
            output->fillCircle(leftPupilX, leftPupilY, pupilSize / 2,
                               eyeColor->colorR, eyeColor->colorG, eyeColor->colorB);
            output->fillCircle(rightPupilX, rightPupilY, pupilSize / 2,
                               eyeColor->colorR, eyeColor->colorG, eyeColor->colorB);
        }
        
        output->present();
    }
    
private:
    float currentOffsetX_ = 0.0f;
    float currentOffsetY_ = 0.0f;
};

/**
 * @brief Simple static sprite display
 */
class StaticSpriteAnimationSet : public AnimationSet {
public:
    const char* getId() const override { return "static_sprite"; }
    const char* getName() const override { return "Static Sprite"; }
    const char* getDescription() const override {
        return "Display a sprite at a fixed position";
    }
    const char* getCategory() const override { return "Basic"; }
    
    StaticSpriteAnimationSet() {
        parameters_ = {
            ParameterDef::SpriteSelect("sprite", "Sprite", 0),
            
            ParameterDef::Separator("Position"),
            ParameterDef::Slider("x", "X Position", 0.0f, 128.0f, 64.0f, "px")
                .withCategory(ParameterCategory::POSITION),
            ParameterDef::Slider("y", "Y Position", 0.0f, 32.0f, 16.0f, "px")
                .withCategory(ParameterCategory::POSITION),
            ParameterDef::Slider("rotation", "Rotation", 0.0f, 360.0f, 0.0f, "°")
                .withCategory(ParameterCategory::POSITION),
            ParameterDef::Slider("scale", "Scale", 0.1f, 4.0f, 1.0f)
                .withCategory(ParameterCategory::SIZE),
            
            ParameterDef::Separator("Background"),
            ParameterDef::Color("bg_color", "Background Color", 0, 0, 0),
        };
    }
    
    void update(uint32_t deltaTimeMs) override {
        // Static sprite doesn't need updates
    }
    
    void render(IRenderOutput* output) override {
        auto* bgColor = getParameter("bg_color");
        output->clear(bgColor->colorR, bgColor->colorG, bgColor->colorB);
        
        int spriteId = getParameter("sprite")->intValue;
        float x = getParameter("x")->floatValue;
        float y = getParameter("y")->floatValue;
        float rotation = getParameter("rotation")->floatValue;
        
        if (rotation != 0.0f) {
            output->blitSpriteRotated(spriteId, x, y, rotation);
        } else {
            output->blitSprite(spriteId, x, y);
        }
        
        output->present();
    }
};

/**
 * @brief Rotating sprite animation
 */
class RotatingSpriteAnimationSet : public AnimationSet {
public:
    const char* getId() const override { return "rotating_sprite"; }
    const char* getName() const override { return "Rotating Sprite"; }
    const char* getDescription() const override {
        return "Sprite that rotates continuously";
    }
    const char* getCategory() const override { return "Basic"; }
    
    RotatingSpriteAnimationSet() {
        parameters_ = {
            ParameterDef::SpriteSelect("sprite", "Sprite", 0),
            
            ParameterDef::Separator("Position"),
            ParameterDef::Slider("x", "X Position", 0.0f, 128.0f, 64.0f, "px"),
            ParameterDef::Slider("y", "Y Position", 0.0f, 32.0f, 16.0f, "px"),
            
            ParameterDef::Separator("Rotation"),
            ParameterDef::Slider("speed", "Rotation Speed", -360.0f, 360.0f, 45.0f, "°/s")
                .withDescription("Degrees per second, negative for counter-clockwise"),
            
            ParameterDef::Separator("Background"),
            ParameterDef::Color("bg_color", "Background Color", 5, 5, 15),
        };
    }
    
    void update(uint32_t deltaTimeMs) override {
        float speed = getParameter("speed")->floatValue;
        angle_ += speed * (deltaTimeMs / 1000.0f);
        
        // Keep angle in 0-360 range
        while (angle_ >= 360.0f) angle_ -= 360.0f;
        while (angle_ < 0.0f) angle_ += 360.0f;
    }
    
    void render(IRenderOutput* output) override {
        auto* bgColor = getParameter("bg_color");
        output->clear(bgColor->colorR, bgColor->colorG, bgColor->colorB);
        
        int spriteId = getParameter("sprite")->intValue;
        float x = getParameter("x")->floatValue;
        float y = getParameter("y")->floatValue;
        
        output->blitSpriteRotated(spriteId, x, y, angle_);
        output->present();
    }
    
private:
    float angle_ = 0.0f;
};

} // namespace AnimationSystem
