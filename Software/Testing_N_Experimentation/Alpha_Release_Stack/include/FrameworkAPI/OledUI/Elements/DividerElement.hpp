/**
 * @file DividerElement.hpp
 * @brief Horizontal/vertical divider element for OLED UI (like HTML <hr>)
 */

#pragma once

#include "../Core/Element.hpp"

namespace OledUI {

//=============================================================================
// DividerElement - Line separator
//=============================================================================
class DividerElement : public Element {
private:
    bool horizontal_ = true;
    bool dashed_ = false;
    int16_t thickness_ = 1;
    
public:
    DividerElement(bool horizontal = true) 
        : Element("hr"), horizontal_(horizontal) {}
    
    //-------------------------------------------------------------------------
    // Configuration
    //-------------------------------------------------------------------------
    DividerElement& setHorizontal(bool h) { horizontal_ = h; markDirty(); return *this; }
    DividerElement& setDashed(bool d) { dashed_ = d; markDirty(); return *this; }
    DividerElement& setThickness(int16_t t) { thickness_ = t; markDirty(); return *this; }
    
    bool isHorizontal() const { return horizontal_; }
    bool isDashed() const { return dashed_; }
    int16_t getThickness() const { return thickness_; }
    
    //-------------------------------------------------------------------------
    // Layout
    //-------------------------------------------------------------------------
    Rect measure(int16_t availableWidth, int16_t availableHeight) override {
        Rect preferred;
        
        if (horizontal_) {
            preferred.width = style_.width >= 0 ? style_.width : availableWidth;
            preferred.height = thickness_ + style_.margin.vertical();
        } else {
            preferred.width = thickness_ + style_.margin.horizontal();
            preferred.height = style_.height >= 0 ? style_.height : availableHeight;
        }
        
        return preferred;
    }
    
    bool isFocusable() const override { return false; }
    
protected:
    void renderContent(GpuCommands* gpu) override;
};

//=============================================================================
// Factory Functions
//=============================================================================

inline std::shared_ptr<DividerElement> Divider() {
    return std::make_shared<DividerElement>(true);
}

inline std::shared_ptr<DividerElement> HDivider() {
    return std::make_shared<DividerElement>(true);
}

inline std::shared_ptr<DividerElement> VDivider() {
    return std::make_shared<DividerElement>(false);
}

} // namespace OledUI
