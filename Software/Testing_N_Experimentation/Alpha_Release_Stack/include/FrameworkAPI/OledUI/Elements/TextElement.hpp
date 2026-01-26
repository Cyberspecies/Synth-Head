/**
 * @file TextElement.hpp
 * @brief Text display element for OLED UI (like HTML <span>, <p>, <h1>)
 */

#pragma once

#include "../Core/Element.hpp"
#include <string>
#include <cstring>

namespace OledUI {

//=============================================================================
// TextElement - Display text (like HTML text nodes)
//=============================================================================
class TextElement : public Element {
private:
    std::string text_;
    
public:
    TextElement(const std::string& text = "") 
        : Element("text"), text_(text) {}
    
    //-------------------------------------------------------------------------
    // Text Management
    //-------------------------------------------------------------------------
    const std::string& getText() const { return text_; }
    
    TextElement& setText(const std::string& t) {
        if (text_ != t) {
            text_ = t;
            markDirty();
        }
        return *this;
    }
    
    //-------------------------------------------------------------------------
    // Convenience Setters
    //-------------------------------------------------------------------------
    TextElement& size(TextSize s) { style_.textSize = s; return *this; }
    TextElement& align(Align a) { style_.textAlign = a; return *this; }
    TextElement& wrap(bool w) { style_.textWrap = w; return *this; }
    TextElement& inverted(bool i = true) { style_.color.inverted = i; return *this; }
    
    //-------------------------------------------------------------------------
    // Layout
    //-------------------------------------------------------------------------
    Rect measure(int16_t availableWidth, int16_t availableHeight) override {
        Rect preferred = Element::measure(availableWidth, availableHeight);
        
        // Calculate text size
        int scale = static_cast<int>(style_.textSize);
        int charWidth = 6 * scale;  // 5 pixels + 1 spacing
        int charHeight = 8 * scale; // 7 pixels + 1 spacing
        
        if (style_.width < 0) {
            // Auto width based on text
            preferred.width = text_.length() * charWidth + style_.margin.horizontal();
        }
        
        if (style_.height < 0) {
            // Auto height based on font size
            if (style_.textWrap && availableWidth > 0) {
                // Calculate wrapped height
                int charsPerLine = (availableWidth - style_.margin.horizontal()) / charWidth;
                if (charsPerLine > 0) {
                    int lines = (text_.length() + charsPerLine - 1) / charsPerLine;
                    preferred.height = lines * charHeight + style_.margin.vertical();
                }
            } else {
                preferred.height = charHeight + style_.margin.vertical();
            }
        }
        
        return preferred;
    }
    
    //-------------------------------------------------------------------------
    // Rendering
    //-------------------------------------------------------------------------
    bool isFocusable() const override { return false; }  // Text is not focusable
    
protected:
    void renderContent(GpuCommands* gpu) override;
};

//=============================================================================
// Factory Functions for common text styles
//=============================================================================

inline std::shared_ptr<TextElement> Text(const std::string& text) {
    return std::make_shared<TextElement>(text);
}

inline std::shared_ptr<TextElement> Title(const std::string& text) {
    auto t = std::make_shared<TextElement>(text);
    t->setStyle(Style::Title());
    return t;
}

inline std::shared_ptr<TextElement> Subtitle(const std::string& text) {
    auto t = std::make_shared<TextElement>(text);
    t->setStyle(Style::Subtitle());
    return t;
}

inline std::shared_ptr<TextElement> Caption(const std::string& text) {
    auto t = std::make_shared<TextElement>(text);
    t->setStyle(Style::Caption());
    return t;
}

inline std::shared_ptr<TextElement> Label(const std::string& text) {
    auto t = std::make_shared<TextElement>(text);
    t->style().setPadding(1);
    return t;
}

} // namespace OledUI
