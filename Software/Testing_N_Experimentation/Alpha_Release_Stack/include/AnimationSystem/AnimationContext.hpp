/*****************************************************************
 * @file AnimationContext.hpp
 * @brief Unified Context for Animation System
 * 
 * Provides centralized access to:
 * - Sensor inputs (IMU, GPS, Mic, Environment)
 * - Equation outputs (computed values)
 * - Available sprites (from SD card and GPU cache)
 * - System state (time, frame count, etc.)
 * 
 * This is the single source of truth for all animation data.
 * Animation sets read from this context, never directly from hardware.
 * 
 * @author ARCOS
 * @version 2.0
 *****************************************************************/

#pragma once

#include <cstdint>
#include <cstring>
#include <cmath>
#include <vector>
#include <string>
#include <functional>
#include <map>

namespace AnimationSystem {

// ============================================================
// Input Types
// ============================================================

/**
 * @brief Sensor input categories
 */
enum class InputCategory : uint8_t {
    IMU = 0,        // Accelerometer, Gyroscope
    GPS,            // Location, Speed, Altitude
    AUDIO,          // Microphone levels
    ENVIRONMENT,    // Temperature, Humidity, Pressure
    BUTTONS,        // User input
    TIME,           // System time values
    CUSTOM          // User-defined inputs
};

/**
 * @brief Single input value with metadata
 */
struct InputValue {
    float value = 0.0f;
    float minValue = -1.0f;
    float maxValue = 1.0f;
    float defaultValue = 0.0f;
    InputCategory category = InputCategory::CUSTOM;
    const char* name = "";
    const char* unit = "";
    uint32_t lastUpdateMs = 0;
    bool valid = false;
};

// ============================================================
// Sprite Information
// ============================================================

/**
 * @brief Sprite info for animation system
 */
struct SpriteInfo {
    int id = -1;                  // Unique sprite ID
    std::string name;             // Display name
    int width = 0;
    int height = 0;
    bool inGpuCache = false;      // Is it uploaded to GPU?
    bool onSdCard = false;        // Is it saved to SD?
    std::string sdPath;           // Path on SD card
};

// ============================================================
// Equation Output
// ============================================================

/**
 * @brief Equation output with metadata
 */
struct EquationOutput {
    int id = 0;
    std::string name;
    std::string expression;
    float value = 0.0f;
    float lastValue = 0.0f;
    bool valid = false;
};

// ============================================================
// Animation Context
// ============================================================

class AnimationContext {
public:
    static constexpr int MAX_INPUTS = 64;
    static constexpr int MAX_SPRITES = 32;
    static constexpr int MAX_EQUATIONS = 16;
    
    AnimationContext() = default;
    
    /**
     * @brief Initialize context with default inputs
     */
    void init() {
        // Register standard IMU inputs
        registerInput("imu.pitch", InputCategory::IMU, -90.0f, 90.0f, 0.0f, "deg");
        registerInput("imu.roll", InputCategory::IMU, -180.0f, 180.0f, 0.0f, "deg");
        registerInput("imu.yaw", InputCategory::IMU, -180.0f, 180.0f, 0.0f, "deg");
        registerInput("imu.accel_x", InputCategory::IMU, -16.0f, 16.0f, 0.0f, "g");
        registerInput("imu.accel_y", InputCategory::IMU, -16.0f, 16.0f, 0.0f, "g");
        registerInput("imu.accel_z", InputCategory::IMU, -16.0f, 16.0f, 1.0f, "g");
        registerInput("imu.gyro_x", InputCategory::IMU, -2000.0f, 2000.0f, 0.0f, "dps");
        registerInput("imu.gyro_y", InputCategory::IMU, -2000.0f, 2000.0f, 0.0f, "dps");
        registerInput("imu.gyro_z", InputCategory::IMU, -2000.0f, 2000.0f, 0.0f, "dps");
        
        // Register GPS inputs
        registerInput("gps.latitude", InputCategory::GPS, -90.0f, 90.0f, 0.0f, "deg");
        registerInput("gps.longitude", InputCategory::GPS, -180.0f, 180.0f, 0.0f, "deg");
        registerInput("gps.altitude", InputCategory::GPS, -1000.0f, 50000.0f, 0.0f, "m");
        registerInput("gps.speed", InputCategory::GPS, 0.0f, 500.0f, 0.0f, "km/h");
        registerInput("gps.satellites", InputCategory::GPS, 0.0f, 32.0f, 0.0f, "");
        registerInput("gps.valid", InputCategory::GPS, 0.0f, 1.0f, 0.0f, "");
        
        // Register audio inputs
        registerInput("audio.level", InputCategory::AUDIO, 0.0f, 1.0f, 0.0f, "");
        registerInput("audio.peak", InputCategory::AUDIO, 0.0f, 1.0f, 0.0f, "");
        registerInput("audio.bass", InputCategory::AUDIO, 0.0f, 1.0f, 0.0f, "");
        registerInput("audio.mid", InputCategory::AUDIO, 0.0f, 1.0f, 0.0f, "");
        registerInput("audio.treble", InputCategory::AUDIO, 0.0f, 1.0f, 0.0f, "");
        
        // Register environment inputs
        registerInput("env.temperature", InputCategory::ENVIRONMENT, -40.0f, 85.0f, 22.0f, "C");
        registerInput("env.humidity", InputCategory::ENVIRONMENT, 0.0f, 100.0f, 50.0f, "%");
        registerInput("env.pressure", InputCategory::ENVIRONMENT, 300.0f, 1100.0f, 1013.25f, "hPa");
        
        // Register button inputs
        registerInput("button.a", InputCategory::BUTTONS, 0.0f, 1.0f, 0.0f, "");
        registerInput("button.b", InputCategory::BUTTONS, 0.0f, 1.0f, 0.0f, "");
        registerInput("button.c", InputCategory::BUTTONS, 0.0f, 1.0f, 0.0f, "");
        registerInput("button.d", InputCategory::BUTTONS, 0.0f, 1.0f, 0.0f, "");
        
        // Register time inputs
        registerInput("time.seconds", InputCategory::TIME, 0.0f, 60.0f, 0.0f, "s");
        registerInput("time.minutes", InputCategory::TIME, 0.0f, 60.0f, 0.0f, "m");
        registerInput("time.hours", InputCategory::TIME, 0.0f, 24.0f, 0.0f, "h");
        registerInput("time.sin", InputCategory::TIME, -1.0f, 1.0f, 0.0f, "");
        registerInput("time.cos", InputCategory::TIME, -1.0f, 1.0f, 1.0f, "");
        registerInput("time.frame", InputCategory::TIME, 0.0f, 1000000.0f, 0.0f, "");
        
        initialized_ = true;
    }
    
    /**
     * @brief Update context (call every frame)
     */
    void update(uint32_t deltaTimeMs) {
        totalTimeMs_ += deltaTimeMs;
        frameCount_++;
        
        // Update time-based inputs
        float seconds = (totalTimeMs_ / 1000) % 60;
        float minutes = (totalTimeMs_ / 60000) % 60;
        float hours = (totalTimeMs_ / 3600000) % 24;
        float phase = (totalTimeMs_ % 1000) / 1000.0f * 2.0f * 3.14159f;
        
        setInput("time.seconds", seconds);
        setInput("time.minutes", minutes);
        setInput("time.hours", hours);
        setInput("time.sin", sinf(phase));
        setInput("time.cos", cosf(phase));
        setInput("time.frame", static_cast<float>(frameCount_));
    }
    
    // ========================================================
    // Input Management
    // ========================================================
    
    /**
     * @brief Register a new input
     */
    void registerInput(const char* name, InputCategory category,
                       float minVal, float maxVal, float defaultVal,
                       const char* unit) {
        if (inputCount_ >= MAX_INPUTS) return;
        
        InputValue& input = inputs_[inputCount_];
        input.name = name;
        input.category = category;
        input.minValue = minVal;
        input.maxValue = maxVal;
        input.defaultValue = defaultVal;
        input.value = defaultVal;
        input.unit = unit;
        input.valid = true;
        
        inputNameMap_[name] = inputCount_;
        inputCount_++;
    }
    
    /**
     * @brief Set input value by name
     */
    void setInput(const char* name, float value) {
        auto it = inputNameMap_.find(name);
        if (it != inputNameMap_.end()) {
            inputs_[it->second].value = value;
            inputs_[it->second].lastUpdateMs = totalTimeMs_;
            inputs_[it->second].valid = true;
        }
    }
    
    /**
     * @brief Get input value by name
     */
    float getInput(const char* name, float defaultVal = 0.0f) const {
        auto it = inputNameMap_.find(name);
        if (it != inputNameMap_.end() && inputs_[it->second].valid) {
            return inputs_[it->second].value;
        }
        return defaultVal;
    }
    
    /**
     * @brief Get normalized input (0-1 range)
     */
    float getInputNormalized(const char* name) const {
        auto it = inputNameMap_.find(name);
        if (it != inputNameMap_.end() && inputs_[it->second].valid) {
            const InputValue& input = inputs_[it->second];
            float range = input.maxValue - input.minValue;
            if (range > 0.0001f) {
                return (input.value - input.minValue) / range;
            }
        }
        return 0.0f;
    }
    
    /**
     * @brief Get all inputs for a category
     */
    std::vector<const InputValue*> getInputsByCategory(InputCategory category) const {
        std::vector<const InputValue*> result;
        for (int i = 0; i < inputCount_; i++) {
            if (inputs_[i].category == category) {
                result.push_back(&inputs_[i]);
            }
        }
        return result;
    }
    
    /**
     * @brief Get all input names
     */
    std::vector<std::string> getInputNames() const {
        std::vector<std::string> names;
        for (int i = 0; i < inputCount_; i++) {
            names.push_back(inputs_[i].name);
        }
        return names;
    }
    
    // ========================================================
    // Sprite Management
    // ========================================================
    
    /**
     * @brief Register a sprite
     */
    void registerSprite(int id, const char* name, int width, int height,
                        bool inGpu, bool onSd, const char* sdPath = "") {
        if (spriteCount_ >= MAX_SPRITES) return;
        
        SpriteInfo& sprite = sprites_[spriteCount_];
        sprite.id = id;
        sprite.name = name;
        sprite.width = width;
        sprite.height = height;
        sprite.inGpuCache = inGpu;
        sprite.onSdCard = onSd;
        sprite.sdPath = sdPath;
        
        spriteCount_++;
    }
    
    /**
     * @brief Get all available sprites
     */
    std::vector<const SpriteInfo*> getSprites() const {
        std::vector<const SpriteInfo*> result;
        for (int i = 0; i < spriteCount_; i++) {
            result.push_back(&sprites_[i]);
        }
        return result;
    }
    
    /**
     * @brief Get sprite by ID
     */
    const SpriteInfo* getSpriteById(int id) const {
        for (int i = 0; i < spriteCount_; i++) {
            if (sprites_[i].id == id) {
                return &sprites_[i];
            }
        }
        return nullptr;
    }
    
    /**
     * @brief Clear sprite list
     */
    void clearSprites() {
        spriteCount_ = 0;
    }
    
    // ========================================================
    // Equation Management
    // ========================================================
    
    /**
     * @brief Register an equation output
     */
    void registerEquation(int id, const char* name, const char* expression) {
        if (equationCount_ >= MAX_EQUATIONS) return;
        
        EquationOutput& eq = equations_[equationCount_];
        eq.id = id;
        eq.name = name;
        eq.expression = expression;
        eq.valid = true;
        
        equationCount_++;
    }
    
    /**
     * @brief Set equation output value
     */
    void setEquationValue(int id, float value) {
        for (int i = 0; i < equationCount_; i++) {
            if (equations_[i].id == id) {
                equations_[i].lastValue = equations_[i].value;
                equations_[i].value = value;
                return;
            }
        }
    }
    
    /**
     * @brief Get equation output value
     */
    float getEquationValue(int id, float defaultVal = 0.0f) const {
        for (int i = 0; i < equationCount_; i++) {
            if (equations_[i].id == id && equations_[i].valid) {
                return equations_[i].value;
            }
        }
        return defaultVal;
    }
    
    /**
     * @brief Get all equations
     */
    std::vector<const EquationOutput*> getEquations() const {
        std::vector<const EquationOutput*> result;
        for (int i = 0; i < equationCount_; i++) {
            result.push_back(&equations_[i]);
        }
        return result;
    }
    
    // ========================================================
    // Accessors
    // ========================================================
    
    uint32_t getTotalTimeMs() const { return totalTimeMs_; }
    uint32_t getFrameCount() const { return frameCount_; }
    bool isInitialized() const { return initialized_; }
    
    // ========================================================
    // JSON Export for Web API
    // ========================================================
    
    /**
     * @brief Export all inputs as JSON for web API
     */
    std::string exportInputsJson() const {
        std::string json = "{\"inputs\":[";
        bool first = true;
        
        for (int i = 0; i < inputCount_; i++) {
            if (!first) json += ",";
            first = false;
            
            const InputValue& input = inputs_[i];
            const char* categoryStr = "";
            switch (input.category) {
                case InputCategory::IMU: categoryStr = "IMU"; break;
                case InputCategory::GPS: categoryStr = "GPS"; break;
                case InputCategory::AUDIO: categoryStr = "Audio"; break;
                case InputCategory::ENVIRONMENT: categoryStr = "Environment"; break;
                case InputCategory::BUTTONS: categoryStr = "Buttons"; break;
                case InputCategory::TIME: categoryStr = "Time"; break;
                case InputCategory::CUSTOM: categoryStr = "Custom"; break;
            }
            
            char buf[256];
            snprintf(buf, sizeof(buf),
                "{\"id\":\"%s\",\"category\":\"%s\",\"value\":%.3f,\"min\":%.3f,\"max\":%.3f,\"unit\":\"%s\"}",
                input.name, categoryStr, input.value, input.minValue, input.maxValue, input.unit);
            json += buf;
        }
        
        json += "]}";
        return json;
    }
    
    /**
     * @brief Export sprites as JSON for web API
     */
    std::string exportSpritesJson() const {
        std::string json = "{\"sprites\":[";
        bool first = true;
        
        for (int i = 0; i < spriteCount_; i++) {
            if (!first) json += ",";
            first = false;
            
            const SpriteInfo& sprite = sprites_[i];
            char buf[256];
            snprintf(buf, sizeof(buf),
                "{\"id\":%d,\"name\":\"%s\",\"width\":%d,\"height\":%d,\"inGpu\":%s,\"onSd\":%s}",
                sprite.id, sprite.name.c_str(), sprite.width, sprite.height,
                sprite.inGpuCache ? "true" : "false",
                sprite.onSdCard ? "true" : "false");
            json += buf;
        }
        
        json += "]}";
        return json;
    }
    
private:
    InputValue inputs_[MAX_INPUTS];
    int inputCount_ = 0;
    std::map<std::string, int> inputNameMap_;
    
    SpriteInfo sprites_[MAX_SPRITES];
    int spriteCount_ = 0;
    
    EquationOutput equations_[MAX_EQUATIONS];
    int equationCount_ = 0;
    
    uint32_t totalTimeMs_ = 0;
    uint32_t frameCount_ = 0;
    bool initialized_ = false;
};

} // namespace AnimationSystem
