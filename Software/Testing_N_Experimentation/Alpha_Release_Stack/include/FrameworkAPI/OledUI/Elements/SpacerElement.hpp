/**
 * @file SpacerElement.hpp
 * @brief Flexible spacing element for OLED UI (flex spacer)
 */

#pragma once

#include "../Core/Element.hpp"

namespace OledUI {

//=============================================================================
// SpacerElement - Flexible space filler
//=============================================================================
class SpacerElement : public Element {
public:
    SpacerElement() : Element("spacer") {
        style_.flex = 1;  // Default flex to fill available space
    }
    
    explicit SpacerElement(int16_t fixedSize, bool horizontal = true) 
        : Element("spacer") {
        if (horizontal) {
            style_.width = fixedSize;
        } else {
            style_.height = fixedSize;
        }
        style_.flex = 0;  // Fixed size, no flex
    }
    
    //-------------------------------------------------------------------------
    // Configuration
    //-------------------------------------------------------------------------
    SpacerElement& setFlex(int16_t f) { style_.flex = f; return *this; }
    SpacerElement& setFixed(int16_t size, bool horizontal = true) {
        if (horizontal) {
            style_.width = size;
            style_.height = -1;
        } else {
            style_.height = size;
            style_.width = -1;
        }
        style_.flex = 0;
        return *this;
    }
    
    //-------------------------------------------------------------------------
    // Layout
    //-------------------------------------------------------------------------
    Rect measure(int16_t availableWidth, int16_t availableHeight) override {
        Rect preferred;
        preferred.width = style_.width >= 0 ? style_.width : 0;
        preferred.height = style_.height >= 0 ? style_.height : 0;
        return preferred;
    }
    
    bool isFocusable() const override { return false; }
    
protected:
    void renderContent(GpuCommands* gpu) override {
        // Spacer doesn't render anything
    }
};

//=============================================================================
// Factory Functions
//=============================================================================

// Flexible spacer (fills available space)
inline std::shared_ptr<SpacerElement> Spacer(int16_t flex = 1) {
    auto s = std::make_shared<SpacerElement>();
    s->setFlex(flex);
    return s;
}

// Fixed horizontal spacer
inline std::shared_ptr<SpacerElement> HSpacer(int16_t size) {
    return std::make_shared<SpacerElement>(size, true);
}

// Fixed vertical spacer
inline std::shared_ptr<SpacerElement> VSpacer(int16_t size) {
    return std::make_shared<SpacerElement>(size, false);
}

} // namespace OledUI
