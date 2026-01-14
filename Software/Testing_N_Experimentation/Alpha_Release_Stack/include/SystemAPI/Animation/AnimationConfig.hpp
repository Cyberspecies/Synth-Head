/*****************************************************************
 * @file AnimationConfig.hpp
 * @brief Animation Configuration System
 * 
 * Manages animation configurations that can be applied to:
 * - HUB75 Displays (both panels as one, NOT OLEDs)
 * - LED Strips
 * 
 * Configurations can target:
 * - Display only
 * - LEDs only  
 * - Both (paired configuration)
 * 
 * @author ARCOS
 * @version 1.0
 *****************************************************************/

#pragma once

#include <cstdint>
#include <cstring>

namespace SystemAPI {
namespace Animation {

// ============================================================
// Configuration Target Types
// ============================================================

enum class ConfigTarget : uint8_t {
    NONE = 0,
    DISPLAY_ONLY = 1,   // Only HUB75 displays
    LEDS_ONLY = 2,      // Only LED strips
    BOTH = 3            // Both displays and LEDs
};

// ============================================================
// Animation Types for Displays
// ============================================================

enum class DisplayAnimation : uint8_t {
    NONE = 0,
    SOLID_COLOR,
    RAINBOW_H,
    RAINBOW_V,
    GRADIENT,
    PULSE,
    SPARKLE,
    WAVE,
    FIRE,
    MATRIX,
    CUSTOM
};

// ============================================================
// Animation Types for LEDs
// ============================================================

enum class LedAnimation : uint8_t {
    NONE = 0,
    SOLID_COLOR,
    RAINBOW,
    BREATHING,
    WAVE,
    FIRE,
    THEATER_CHASE,
    SPARKLE,
    CUSTOM
};

// ============================================================
// Display Configuration
// ============================================================

struct DisplayConfig {
    DisplayAnimation animation;
    uint8_t color1_r, color1_g, color1_b;
    uint8_t color2_r, color2_g, color2_b;
    uint8_t speed;          // 0-255
    uint8_t brightness;     // 0-255
    uint8_t param1;         // Animation-specific
    uint8_t param2;         // Animation-specific
    uint8_t _reserved[2];
    
    DisplayConfig() {
        memset(this, 0, sizeof(DisplayConfig));
        animation = DisplayAnimation::NONE;
        brightness = 255;
        speed = 128;
    }
    
    void setColor1(uint8_t r, uint8_t g, uint8_t b) {
        color1_r = r; color1_g = g; color1_b = b;
    }
    
    void setColor2(uint8_t r, uint8_t g, uint8_t b) {
        color2_r = r; color2_g = g; color2_b = b;
    }
};

// ============================================================
// LED Configuration
// ============================================================

struct LedConfig {
    LedAnimation animation;
    uint8_t color1_r, color1_g, color1_b;
    uint8_t color2_r, color2_g, color2_b;
    uint8_t speed;          // 0-255
    uint8_t brightness;     // 0-255
    uint8_t param1;         // Animation-specific
    uint8_t param2;         // Animation-specific
    uint8_t _reserved[2];
    
    LedConfig() {
        memset(this, 0, sizeof(LedConfig));
        animation = LedAnimation::NONE;
        brightness = 255;
        speed = 128;
    }
    
    void setColor1(uint8_t r, uint8_t g, uint8_t b) {
        color1_r = r; color1_g = g; color1_b = b;
    }
    
    void setColor2(uint8_t r, uint8_t g, uint8_t b) {
        color2_r = r; color2_g = g; color2_b = b;
    }
};

// ============================================================
// Animation Configuration (combines display + LED)
// ============================================================

struct AnimationConfiguration {
    static constexpr int MAX_NAME_LENGTH = 24;
    
    char name[MAX_NAME_LENGTH];
    ConfigTarget target;
    DisplayConfig display;
    LedConfig leds;
    bool enabled;
    
    AnimationConfiguration() : target(ConfigTarget::NONE), enabled(false) {
        memset(name, 0, MAX_NAME_LENGTH);
        strcpy(name, "Untitled");
    }
    
    AnimationConfiguration(const char* configName, ConfigTarget t = ConfigTarget::BOTH)
        : target(t), enabled(false) {
        memset(name, 0, MAX_NAME_LENGTH);
        strncpy(name, configName, MAX_NAME_LENGTH - 1);
    }
    
    void setName(const char* newName) {
        memset(name, 0, MAX_NAME_LENGTH);
        strncpy(name, newName, MAX_NAME_LENGTH - 1);
    }
    
    bool hasDisplay() const {
        return target == ConfigTarget::DISPLAY_ONLY || target == ConfigTarget::BOTH;
    }
    
    bool hasLeds() const {
        return target == ConfigTarget::LEDS_ONLY || target == ConfigTarget::BOTH;
    }
    
    const char* getTargetName() const {
        switch (target) {
            case ConfigTarget::DISPLAY_ONLY: return "Display";
            case ConfigTarget::LEDS_ONLY: return "LEDs";
            case ConfigTarget::BOTH: return "Both";
            default: return "None";
        }
    }
};

// ============================================================
// Configuration Manager
// ============================================================

class AnimationConfigManager {
public:
    static constexpr int MAX_CONFIGS = 16;
    
    AnimationConfigManager() : _configCount(0), _activeDisplayConfig(-1), _activeLedConfig(-1) {
        // Add some default configurations
        addDefaultConfigs();
    }
    
    // Add a new configuration
    int addConfig(const AnimationConfiguration& config) {
        if (_configCount >= MAX_CONFIGS) return -1;
        _configs[_configCount] = config;
        return _configCount++;
    }
    
    // Create a new empty configuration
    int createConfig(const char* name, ConfigTarget target = ConfigTarget::BOTH) {
        if (_configCount >= MAX_CONFIGS) return -1;
        _configs[_configCount] = AnimationConfiguration(name, target);
        return _configCount++;
    }
    
    // Get configuration by index
    AnimationConfiguration* getConfig(int index) {
        if (index < 0 || index >= _configCount) return nullptr;
        return &_configs[index];
    }
    
    const AnimationConfiguration* getConfig(int index) const {
        if (index < 0 || index >= _configCount) return nullptr;
        return &_configs[index];
    }
    
    // Find configuration by name
    int findConfig(const char* name) const {
        for (int i = 0; i < _configCount; i++) {
            if (strcmp(_configs[i].name, name) == 0) {
                return i;
            }
        }
        return -1;
    }
    
    // Delete configuration
    bool deleteConfig(int index) {
        if (index < 0 || index >= _configCount) return false;
        
        // Shift remaining configs
        for (int i = index; i < _configCount - 1; i++) {
            _configs[i] = _configs[i + 1];
        }
        _configCount--;
        
        // Adjust active indices
        if (_activeDisplayConfig == index) _activeDisplayConfig = -1;
        else if (_activeDisplayConfig > index) _activeDisplayConfig--;
        
        if (_activeLedConfig == index) _activeLedConfig = -1;
        else if (_activeLedConfig > index) _activeLedConfig--;
        
        return true;
    }
    
    // Apply configuration
    // Returns what was applied: 0=nothing, 1=display, 2=leds, 3=both
    int applyConfig(int index) {
        AnimationConfiguration* config = getConfig(index);
        if (!config) return 0;
        
        int applied = 0;
        
        if (config->hasDisplay()) {
            _activeDisplayConfig = index;
            applied |= 1;
            // TODO: Actually send display config to GPU
        }
        
        if (config->hasLeds()) {
            _activeLedConfig = index;
            applied |= 2;
            // TODO: Actually apply LED config
        }
        
        return applied;
    }
    
    // Unapply configuration for display
    void unapplyDisplay() {
        _activeDisplayConfig = -1;
    }
    
    // Unapply configuration for LEDs
    void unapplyLeds() {
        _activeLedConfig = -1;
    }
    
    // Get active configurations
    int getActiveDisplayConfig() const { return _activeDisplayConfig; }
    int getActiveLedConfig() const { return _activeLedConfig; }
    
    // Check if a config is active (and for what)
    // Returns: 0=not active, 1=active for display, 2=active for leds, 3=active for both
    int getConfigActiveState(int index) const {
        int state = 0;
        if (index == _activeDisplayConfig) state |= 1;
        if (index == _activeLedConfig) state |= 2;
        return state;
    }
    
    // Get config count
    int getConfigCount() const { return _configCount; }
    
    // Rename configuration
    bool renameConfig(int index, const char* newName) {
        AnimationConfiguration* config = getConfig(index);
        if (!config) return false;
        config->setName(newName);
        return true;
    }
    
    // Duplicate configuration
    int duplicateConfig(int index) {
        AnimationConfiguration* config = getConfig(index);
        if (!config || _configCount >= MAX_CONFIGS) return -1;
        
        AnimationConfiguration newConfig = *config;
        char newName[AnimationConfiguration::MAX_NAME_LENGTH];
        // Truncate source name to leave room for " Copy" suffix
        int maxSrcLen = AnimationConfiguration::MAX_NAME_LENGTH - 6; // 5 for " Copy" + 1 for null
        snprintf(newName, sizeof(newName), "%.*s Copy", maxSrcLen, config->name);
        newConfig.setName(newName);
        
        return addConfig(newConfig);
    }

private:
    void addDefaultConfigs() {
        // Rainbow Display
        AnimationConfiguration rainbow("Rainbow", ConfigTarget::DISPLAY_ONLY);
        rainbow.display.animation = DisplayAnimation::RAINBOW_H;
        rainbow.display.speed = 128;
        rainbow.display.brightness = 200;
        addConfig(rainbow);
        
        // Solid Red Display
        AnimationConfiguration solidRed("Solid Red", ConfigTarget::DISPLAY_ONLY);
        solidRed.display.animation = DisplayAnimation::SOLID_COLOR;
        solidRed.display.setColor1(255, 0, 0);
        solidRed.display.brightness = 255;
        addConfig(solidRed);
        
        // Rainbow LEDs
        AnimationConfiguration ledRainbow("LED Rainbow", ConfigTarget::LEDS_ONLY);
        ledRainbow.leds.animation = LedAnimation::RAINBOW;
        ledRainbow.leds.speed = 128;
        ledRainbow.leds.brightness = 200;
        addConfig(ledRainbow);
        
        // Breathing LEDs
        AnimationConfiguration breathing("LED Breathing", ConfigTarget::LEDS_ONLY);
        breathing.leds.animation = LedAnimation::BREATHING;
        breathing.leds.setColor1(0, 150, 255);
        breathing.leds.speed = 100;
        breathing.leds.brightness = 255;
        addConfig(breathing);
        
        // Combined Fire
        AnimationConfiguration fire("Fire Effect", ConfigTarget::BOTH);
        fire.display.animation = DisplayAnimation::FIRE;
        fire.display.speed = 180;
        fire.display.brightness = 220;
        fire.leds.animation = LedAnimation::FIRE;
        fire.leds.speed = 180;
        fire.leds.brightness = 220;
        addConfig(fire);
        
        // Combined Wave
        AnimationConfiguration wave("Wave Sync", ConfigTarget::BOTH);
        wave.display.animation = DisplayAnimation::WAVE;
        wave.display.setColor1(0, 100, 255);
        wave.display.setColor2(255, 0, 100);
        wave.display.speed = 150;
        wave.leds.animation = LedAnimation::WAVE;
        wave.leds.setColor1(0, 100, 255);
        wave.leds.setColor2(255, 0, 100);
        wave.leds.speed = 150;
        addConfig(wave);
    }
    
    AnimationConfiguration _configs[MAX_CONFIGS];
    int _configCount;
    int _activeDisplayConfig;
    int _activeLedConfig;
};

// Global instance (defined in CaptivePortal.hpp or similar)
// extern AnimationConfigManager g_animConfigManager;

} // namespace Animation
} // namespace SystemAPI
