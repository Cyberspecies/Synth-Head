/**
 * @file ButtonElement.hpp
 * @brief Interactive button element for OLED UI (like HTML <button>)
 */

#pragma once

#include "../Core/Element.hpp"
#include "TextElement.hpp"
#include "IconElement.hpp"
#include <string>

namespace OledUI {

//=============================================================================
// ButtonElement - Clickable button (like HTML <button>)
//=============================================================================
class ButtonElement : public Element {
private:
    std::string label_;
    Icon icon_ = Icon::NONE;
    bool iconLeft_ = true;  // Icon on left side
    bool pressed_ = false;
    OnClickCallback onClick_;
    
public:
    ButtonElement(const std::string& label = "") 
        : Element("button"), label_(label) {
        // Buttons are focusable by default
        setStyle(Style::Button());
    }
    
    //-------------------------------------------------------------------------
    // Button Configuration
    //-------------------------------------------------------------------------
    const std::string& getLabel() const { return label_; }
    
    ButtonElement& setLabel(const std::string& l) {
        if (label_ != l) {
            label_ = l;
            markDirty();
        }
        return *this;
    }
    
    ButtonElement& setIcon(Icon i, bool leftSide = true) {
        icon_ = i;
        iconLeft_ = leftSide;
        markDirty();
        return *this;
    }
    
    ButtonElement& setOnClick(OnClickCallback cb) {
        onClick_ = cb;
        return *this;
    }
    
    ButtonElement& inverted(bool i = true) { style_.color.inverted = i; return *this; }
    
    bool isPressed() const { return pressed_; }
    
    //-------------------------------------------------------------------------
    // Layout
    //-------------------------------------------------------------------------
    Rect measure(int16_t availableWidth, int16_t availableHeight) override {
        Rect preferred = Element::measure(availableWidth, availableHeight);
        
        int scale = static_cast<int>(style_.textSize);
        int charWidth = 6 * scale;
        int charHeight = 8 * scale;
        
        if (style_.width < 0) {
            // Auto width: icon + label + padding
            int iconSpace = (icon_ != Icon::NONE) ? 10 : 0;
            preferred.width = label_.length() * charWidth + iconSpace + 
                              style_.padding.horizontal() + style_.margin.horizontal();
        }
        
        if (style_.height < 0) {
            // Auto height: text + padding
            preferred.height = charHeight + style_.padding.vertical() + style_.margin.vertical();
        }
        
        return preferred;
    }
    
    //-------------------------------------------------------------------------
    // Interaction
    //-------------------------------------------------------------------------
    bool handleInput(InputEvent event) override {
        if (!style_.enabled) return false;
        
        if (event == InputEvent::SELECT && onClick_) {
            pressed_ = true;
            markDirty();
            onClick_();
            pressed_ = false;
            markDirty();
            return true;
        }
        
        return Element::handleInput(event);
    }
    
    bool isFocusable() const override { return style_.enabled; }
    
protected:
    void renderContent(GpuCommands* gpu) override;
    void renderFocus(GpuCommands* gpu) override;
};

//=============================================================================
// Factory Functions
//=============================================================================

inline std::shared_ptr<ButtonElement> Button(const std::string& label, OnClickCallback onClick = nullptr) {
    auto b = std::make_shared<ButtonElement>(label);
    if (onClick) b->setOnClick(onClick);
    return b;
}

inline std::shared_ptr<ButtonElement> IconButton(Icon icon, OnClickCallback onClick = nullptr) {
    auto b = std::make_shared<ButtonElement>("");
    b->setIcon(icon);
    if (onClick) b->setOnClick(onClick);
    return b;
}

inline std::shared_ptr<ButtonElement> IconButton(Icon icon, const std::string& label, OnClickCallback onClick = nullptr) {
    auto b = std::make_shared<ButtonElement>(label);
    b->setIcon(icon);
    if (onClick) b->setOnClick(onClick);
    return b;
}

} // namespace OledUI
