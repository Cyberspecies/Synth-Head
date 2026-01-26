/**
 * @file Style.hpp
 * @brief CSS-like styling for OLED UI elements
 * 
 * This file provides a declarative styling system similar to CSS,
 * allowing elements to be styled with properties like margin, padding,
 * alignment, and visual appearance.
 */

#pragma once

#include "Types.hpp"

namespace OledUI {

//=============================================================================
// Style - CSS-like style properties
//=============================================================================
struct Style {
    //-------------------------------------------------------------------------
    // Layout Properties (like CSS box model)
    //-------------------------------------------------------------------------
    
    // Size constraints (-1 = auto)
    int16_t width = -1;         // Fixed width, -1 for auto
    int16_t height = -1;        // Fixed height, -1 for auto
    int16_t minWidth = 0;       // Minimum width
    int16_t minHeight = 0;      // Minimum height
    int16_t maxWidth = -1;      // Maximum width, -1 for none
    int16_t maxHeight = -1;     // Maximum height, -1 for none
    
    // Spacing
    Spacing margin;             // Outer spacing
    Spacing padding;            // Inner spacing
    
    // Flex properties (like CSS flexbox)
    uint8_t flex = 0;           // Flex grow factor (0 = no flex)
    Align alignSelf = Align::START;  // Override parent's align-items
    FlexDirection flexDirection = FlexDirection::COLUMN;  // Container direction
    Justify justify = Justify::START;  // Main axis alignment
    Align align = Align::START;        // Cross axis alignment
    int16_t gap = 0;                   // Gap between children
    
    //-------------------------------------------------------------------------
    // Visual Properties
    //-------------------------------------------------------------------------
    
    OledColor color;            // Foreground color
    OledColor backgroundColor;  // Background color
    
    bool border = false;        // Show border
    int16_t borderRadius = 0;   // Border radius (0 = square)
    
    bool visible = true;        // Visibility
    bool enabled = true;        // Enabled state (for interactive elements)
    
    //-------------------------------------------------------------------------
    // Text Properties
    //-------------------------------------------------------------------------
    
    TextSize textSize = TextSize::SMALL;
    Align textAlign = Align::START;
    bool textWrap = false;      // Word wrap
    
    //-------------------------------------------------------------------------
    // Builder Pattern Methods
    //-------------------------------------------------------------------------
    
    Style& setSize(int16_t w, int16_t h) {
        width = w; height = h;
        return *this;
    }
    
    Style& setWidth(int16_t w) { width = w; return *this; }
    Style& setHeight(int16_t h) { height = h; return *this; }
    
    Style& setMinSize(int16_t w, int16_t h) {
        minWidth = w; minHeight = h;
        return *this;
    }
    
    Style& setMaxSize(int16_t w, int16_t h) {
        maxWidth = w; maxHeight = h;
        return *this;
    }
    
    Style& setMargin(int16_t all) {
        margin = Spacing(all);
        return *this;
    }
    
    Style& setMargin(int16_t vertical, int16_t horizontal) {
        margin = Spacing(vertical, horizontal);
        return *this;
    }
    
    Style& setMargin(int16_t t, int16_t r, int16_t b, int16_t l) {
        margin = Spacing(t, r, b, l);
        return *this;
    }
    
    Style& setPadding(int16_t all) {
        padding = Spacing(all);
        return *this;
    }
    
    Style& setPadding(int16_t vertical, int16_t horizontal) {
        padding = Spacing(vertical, horizontal);
        return *this;
    }
    
    Style& setPadding(int16_t t, int16_t r, int16_t b, int16_t l) {
        padding = Spacing(t, r, b, l);
        return *this;
    }
    
    Style& setFlex(uint8_t f) { flex = f; return *this; }
    Style& setAlignSelf(Align a) { alignSelf = a; return *this; }
    
    Style& setColor(OledColor c) { color = c; return *this; }
    Style& setBackgroundColor(OledColor c) { backgroundColor = c; return *this; }
    Style& setInverted(bool inv = true) { 
        color.inverted = inv;
        return *this; 
    }
    
    Style& setBorder(bool b, int16_t radius = 0) {
        border = b; borderRadius = radius;
        return *this;
    }
    
    Style& setVisible(bool v) { visible = v; return *this; }
    Style& setEnabled(bool e) { enabled = e; return *this; }
    
    Style& setTextSize(TextSize s) { textSize = s; return *this; }
    Style& setTextAlign(Align a) { textAlign = a; return *this; }
    Style& setTextWrap(bool w) { textWrap = w; return *this; }
    
    //-------------------------------------------------------------------------
    // Preset Styles (like CSS classes)
    //-------------------------------------------------------------------------
    
    static Style Default() {
        return Style();
    }
    
    static Style Centered() {
        Style s;
        s.textAlign = Align::CENTER;
        s.alignSelf = Align::CENTER;
        return s;
    }
    
    static Style FullWidth() {
        Style s;
        s.width = OLED_WIDTH;
        return s;
    }
    
    static Style Card() {
        Style s;
        s.border = true;
        s.borderRadius = 4;
        s.padding = Spacing(4);
        s.margin = Spacing(2);
        return s;
    }
    
    static Style Button() {
        Style s;
        s.border = true;
        s.padding = Spacing(2, 8);
        s.textAlign = Align::CENTER;
        return s;
    }
    
    static Style MenuItem() {
        Style s;
        s.padding = Spacing(2, 4);
        s.width = OLED_WIDTH;
        return s;
    }
    
    static Style Title() {
        Style s;
        s.textSize = TextSize::LARGE;
        s.textAlign = Align::CENTER;
        s.margin = Spacing(4, 0);
        return s;
    }
    
    static Style Subtitle() {
        Style s;
        s.textSize = TextSize::MEDIUM;
        s.margin = Spacing(2, 0);
        return s;
    }
    
    static Style Caption() {
        Style s;
        s.textSize = TextSize::SMALL;
        s.color.inverted = false;
        return s;
    }
    
    static Style StatusBar() {
        Style s;
        s.height = 12;
        s.width = OLED_WIDTH;
        s.padding = Spacing(1, 2);
        s.border = false;
        return s;
    }
};

//=============================================================================
// StyleSheet - Collection of named styles (like CSS stylesheet)
//=============================================================================
class StyleSheet {
public:
    static constexpr int MAX_STYLES = 16;
    
private:
    struct NamedStyle {
        const char* name;
        Style style;
    };
    
    NamedStyle styles_[MAX_STYLES];
    int count_ = 0;
    
public:
    StyleSheet() = default;
    
    // Add a named style
    bool addStyle(const char* name, const Style& style) {
        if (count_ >= MAX_STYLES) return false;
        styles_[count_].name = name;
        styles_[count_].style = style;
        count_++;
        return true;
    }
    
    // Get style by name
    const Style* getStyle(const char* name) const {
        for (int i = 0; i < count_; i++) {
            if (strcmp(styles_[i].name, name) == 0) {
                return &styles_[i].style;
            }
        }
        return nullptr;
    }
    
    // Apply style by name to existing style (merges properties)
    bool applyStyle(const char* name, Style& target) const {
        const Style* source = getStyle(name);
        if (!source) return false;
        target = *source;  // Simple copy for now
        return true;
    }
};

} // namespace OledUI
