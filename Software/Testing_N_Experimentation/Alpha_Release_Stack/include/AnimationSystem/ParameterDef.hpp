/*****************************************************************
 * @file ParameterDef.hpp
 * @brief Parameter Definition for Auto-Generated Settings
 * 
 * Defines the structure of modifiable parameters that can be
 * automatically rendered as UI controls (sliders, toggles, etc.)
 * 
 * Each animation set provides a list of ParameterDef objects
 * that describe its configurable parameters. The web UI then
 * auto-generates appropriate controls based on these definitions.
 * 
 * @author ARCOS
 * @version 2.0
 *****************************************************************/

#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <functional>

namespace AnimationSystem {

// ============================================================
// Parameter Types
// ============================================================

/**
 * @brief Type of UI control to generate
 */
enum class ParameterType : uint8_t {
    SLIDER = 0,         // Float slider with min/max
    SLIDER_INT,         // Integer slider
    TOGGLE,             // Boolean on/off
    COLOR,              // RGB color picker
    DROPDOWN,           // Select from options
    INPUT_SELECT,       // Select from available inputs
    SPRITE_SELECT,      // Select from available sprites
    EQUATION_SELECT,    // Select from available equations
    TEXT,               // Text input
    BUTTON,             // Action button
    SEPARATOR,          // Visual separator (no value)
    LABEL               // Read-only label
};

/**
 * @brief Parameter category for grouping in UI
 */
enum class ParameterCategory : uint8_t {
    GENERAL = 0,
    POSITION,
    SIZE,
    MOVEMENT,
    COLOR,
    TIMING,
    INPUT_BINDING,
    ADVANCED
};

// ============================================================
// Dropdown Option
// ============================================================

struct DropdownOption {
    std::string label;
    int value;
    
    DropdownOption() = default;
    DropdownOption(const char* l, int v) : label(l), value(v) {}
};

// ============================================================
// Parameter Definition
// ============================================================

/**
 * @brief Complete definition of a modifiable parameter
 */
struct ParameterDef {
    // Identification
    std::string id;             // Unique ID within animation set
    std::string name;           // Display name
    std::string description;    // Tooltip/help text
    
    // Type and category
    ParameterType type = ParameterType::SLIDER;
    ParameterCategory category = ParameterCategory::GENERAL;
    
    // Value constraints (for numeric types)
    float minValue = 0.0f;
    float maxValue = 1.0f;
    float step = 0.01f;
    float defaultValue = 0.5f;
    
    // Current value storage
    float floatValue = 0.0f;
    int intValue = 0;
    bool boolValue = false;
    std::string stringValue;
    uint8_t colorR = 255, colorG = 255, colorB = 255;
    
    // Dropdown options (for DROPDOWN type)
    std::vector<DropdownOption> options;
    
    // Unit suffix (e.g., "px", "%", "ms")
    std::string unit;
    
    // Visibility/enable conditions
    bool visible = true;
    bool enabled = true;
    
    // Change callback
    std::function<void(const ParameterDef&)> onChange;
    
    // ========================================================
    // Builder Methods
    // ========================================================
    
    static ParameterDef Slider(const char* id, const char* name,
                               float minVal, float maxVal, float defaultVal,
                               const char* unit = "") {
        ParameterDef def;
        def.id = id;
        def.name = name;
        def.type = ParameterType::SLIDER;
        def.minValue = minVal;
        def.maxValue = maxVal;
        def.defaultValue = defaultVal;
        def.floatValue = defaultVal;
        def.step = (maxVal - minVal) / 100.0f;
        def.unit = unit;
        return def;
    }
    
    static ParameterDef SliderInt(const char* id, const char* name,
                                   int minVal, int maxVal, int defaultVal,
                                   const char* unit = "") {
        ParameterDef def;
        def.id = id;
        def.name = name;
        def.type = ParameterType::SLIDER_INT;
        def.minValue = static_cast<float>(minVal);
        def.maxValue = static_cast<float>(maxVal);
        def.defaultValue = static_cast<float>(defaultVal);
        def.intValue = defaultVal;
        def.step = 1.0f;
        def.unit = unit;
        return def;
    }
    
    static ParameterDef Toggle(const char* id, const char* name, bool defaultVal) {
        ParameterDef def;
        def.id = id;
        def.name = name;
        def.type = ParameterType::TOGGLE;
        def.boolValue = defaultVal;
        def.defaultValue = defaultVal ? 1.0f : 0.0f;
        return def;
    }
    
    static ParameterDef Color(const char* id, const char* name,
                              uint8_t r, uint8_t g, uint8_t b) {
        ParameterDef def;
        def.id = id;
        def.name = name;
        def.type = ParameterType::COLOR;
        def.colorR = r;
        def.colorG = g;
        def.colorB = b;
        return def;
    }
    
    static ParameterDef Dropdown(const char* id, const char* name,
                                  std::vector<DropdownOption> opts, int defaultIdx = 0) {
        ParameterDef def;
        def.id = id;
        def.name = name;
        def.type = ParameterType::DROPDOWN;
        def.options = opts;
        def.intValue = defaultIdx < (int)opts.size() ? opts[defaultIdx].value : 0;
        return def;
    }
    
    static ParameterDef InputSelect(const char* id, const char* name,
                                     const char* defaultInput = "") {
        ParameterDef def;
        def.id = id;
        def.name = name;
        def.type = ParameterType::INPUT_SELECT;
        def.stringValue = defaultInput;
        def.category = ParameterCategory::INPUT_BINDING;
        return def;
    }
    
    static ParameterDef SpriteSelect(const char* id, const char* name,
                                      int defaultSpriteId = -1) {
        ParameterDef def;
        def.id = id;
        def.name = name;
        def.type = ParameterType::SPRITE_SELECT;
        def.intValue = defaultSpriteId;
        return def;
    }
    
    static ParameterDef Separator(const char* label = "") {
        ParameterDef def;
        def.id = "_sep";
        def.name = label;
        def.type = ParameterType::SEPARATOR;
        return def;
    }
    
    static ParameterDef Label(const char* id, const char* text) {
        ParameterDef def;
        def.id = id;
        def.name = text;
        def.type = ParameterType::LABEL;
        return def;
    }
    
    static ParameterDef Button(const char* id, const char* label) {
        ParameterDef def;
        def.id = id;
        def.name = label;
        def.type = ParameterType::BUTTON;
        return def;
    }
    
    // ========================================================
    // Modifier Methods (for chaining)
    // ========================================================
    
    ParameterDef& withCategory(ParameterCategory cat) {
        category = cat;
        return *this;
    }
    
    ParameterDef& withDescription(const char* desc) {
        description = desc;
        return *this;
    }
    
    ParameterDef& withStep(float s) {
        step = s;
        return *this;
    }
    
    ParameterDef& withOnChange(std::function<void(const ParameterDef&)> cb) {
        onChange = cb;
        return *this;
    }
    
    ParameterDef& hidden() {
        visible = false;
        return *this;
    }
    
    ParameterDef& disabled() {
        enabled = false;
        return *this;
    }
    
    // ========================================================
    // Value Access
    // ========================================================
    
    float getFloat() const { return floatValue; }
    int getInt() const { return intValue; }
    bool getBool() const { return boolValue; }
    const std::string& getString() const { return stringValue; }
    
    void setFloat(float v) {
        floatValue = v;
        if (onChange) onChange(*this);
    }
    
    void setInt(int v) {
        intValue = v;
        if (onChange) onChange(*this);
    }
    
    void setBool(bool v) {
        boolValue = v;
        if (onChange) onChange(*this);
    }
    
    void setString(const std::string& v) {
        stringValue = v;
        if (onChange) onChange(*this);
    }
    
    void setColor(uint8_t r, uint8_t g, uint8_t b) {
        colorR = r;
        colorG = g;
        colorB = b;
        if (onChange) onChange(*this);
    }
};

} // namespace AnimationSystem
