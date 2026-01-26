/**
 * @file IconElement.hpp
 * @brief Icon display element for OLED UI
 */

#pragma once

#include "../Core/Element.hpp"

namespace OledUI {

//=============================================================================
// IconElement - Display icons (like HTML <i> with font-awesome)
//=============================================================================
class IconElement : public Element {
private:
    Icon icon_;
    int16_t size_ = 16;  // Icon size in pixels
    
public:
    IconElement(Icon icon = Icon::NONE) 
        : Element("icon"), icon_(icon) {}
    
    //-------------------------------------------------------------------------
    // Icon Management
    //-------------------------------------------------------------------------
    Icon getIcon() const { return icon_; }
    
    IconElement& setIcon(Icon i) {
        if (icon_ != i) {
            icon_ = i;
            markDirty();
        }
        return *this;
    }
    
    IconElement& setSize(int16_t s) {
        if (size_ != s) {
            size_ = s;
            markDirty();
        }
        return *this;
    }
    
    IconElement& inverted(bool i = true) { style_.color.inverted = i; return *this; }
    
    //-------------------------------------------------------------------------
    // Layout
    //-------------------------------------------------------------------------
    Rect measure(int16_t availableWidth, int16_t availableHeight) override {
        Rect preferred;
        preferred.width = style_.width >= 0 ? style_.width : size_ + style_.margin.horizontal();
        preferred.height = style_.height >= 0 ? style_.height : size_ + style_.margin.vertical();
        return preferred;
    }
    
    bool isFocusable() const override { return false; }  // Icons not focusable by default
    
protected:
    void renderContent(GpuCommands* gpu) override;
};

//=============================================================================
// Factory Function
//=============================================================================

inline std::shared_ptr<IconElement> IconWidget(Icon icon, int16_t size = 16) {
    auto i = std::make_shared<IconElement>(icon);
    i->setSize(size);
    return i;
}

} // namespace OledUI
