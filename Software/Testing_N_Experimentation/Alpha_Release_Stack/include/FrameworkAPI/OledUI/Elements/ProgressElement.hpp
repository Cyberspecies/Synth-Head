/**
 * @file ProgressElement.hpp
 * @brief Progress bar/indicator element for OLED UI
 */

#pragma once

#include "../Core/Element.hpp"
#include <algorithm>

namespace OledUI {

//=============================================================================
// ProgressType - Visual style of progress indicator
//=============================================================================
enum class ProgressType {
    BAR,            // Horizontal bar
    BAR_VERTICAL,   // Vertical bar
    SPINNER,        // Rotating spinner (indeterminate)
    DOTS,           // Animated dots (indeterminate)
    CIRCLE          // Circular progress
};

//=============================================================================
// ProgressElement - Progress indicator
//=============================================================================
class ProgressElement : public Element {
private:
    float value_ = 0.0f;        // 0.0 to 1.0 (or -1 for indeterminate)
    float minValue_ = 0.0f;
    float maxValue_ = 1.0f;
    ProgressType type_ = ProgressType::BAR;
    bool showLabel_ = false;
    std::string labelFormat_ = "%d%%";  // printf format
    int animFrame_ = 0;         // For animated types
    
public:
    ProgressElement() : Element("progress") {
        style_.height = 8;  // Default bar height
    }
    
    //-------------------------------------------------------------------------
    // Value Management
    //-------------------------------------------------------------------------
    float getValue() const { return value_; }
    float getProgress() const { 
        if (maxValue_ == minValue_) return 0.0f;
        return (value_ - minValue_) / (maxValue_ - minValue_);
    }
    int getPercent() const { return (int)(getProgress() * 100.0f); }
    
    ProgressElement& setValue(float v) {
        float clamped = std::clamp(v, minValue_, maxValue_);
        if (value_ != clamped) {
            value_ = clamped;
            markDirty();
        }
        return *this;
    }
    
    ProgressElement& setRange(float min, float max) {
        minValue_ = min;
        maxValue_ = max;
        markDirty();
        return *this;
    }
    
    ProgressElement& setIndeterminate() {
        value_ = -1.0f;
        markDirty();
        return *this;
    }
    
    bool isIndeterminate() const { return value_ < 0; }
    
    //-------------------------------------------------------------------------
    // Configuration
    //-------------------------------------------------------------------------
    ProgressElement& setType(ProgressType t) { type_ = t; markDirty(); return *this; }
    ProgressElement& setShowLabel(bool s) { showLabel_ = s; markDirty(); return *this; }
    ProgressElement& setLabelFormat(const std::string& f) { labelFormat_ = f; return *this; }
    
    // Convenience setters for type
    ProgressElement& bar() { return setType(ProgressType::BAR); }
    ProgressElement& verticalBar() { return setType(ProgressType::BAR_VERTICAL); }
    ProgressElement& spinner() { return setType(ProgressType::SPINNER).setIndeterminate(); }
    ProgressElement& dots() { return setType(ProgressType::DOTS).setIndeterminate(); }
    ProgressElement& circle() { return setType(ProgressType::CIRCLE); }
    
    //-------------------------------------------------------------------------
    // Animation (call in update loop for indeterminate)
    //-------------------------------------------------------------------------
    void tick() {
        if (isIndeterminate()) {
            animFrame_++;
            markDirty();
        }
    }
    
    //-------------------------------------------------------------------------
    // Layout
    //-------------------------------------------------------------------------
    Rect measure(int16_t availableWidth, int16_t availableHeight) override {
        Rect preferred = Element::measure(availableWidth, availableHeight);
        
        switch (type_) {
            case ProgressType::BAR:
                if (style_.width < 0) preferred.width = availableWidth;
                if (style_.height < 0) preferred.height = 8;
                break;
                
            case ProgressType::BAR_VERTICAL:
                if (style_.width < 0) preferred.width = 8;
                if (style_.height < 0) preferred.height = availableHeight;
                break;
                
            case ProgressType::SPINNER:
            case ProgressType::CIRCLE:
                if (style_.width < 0) preferred.width = 16;
                if (style_.height < 0) preferred.height = 16;
                break;
                
            case ProgressType::DOTS:
                if (style_.width < 0) preferred.width = 24;  // 3 dots
                if (style_.height < 0) preferred.height = 8;
                break;
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

inline std::shared_ptr<ProgressElement> ProgressBar(float value = 0.0f) {
    auto p = std::make_shared<ProgressElement>();
    p->setValue(value);
    return p;
}

inline std::shared_ptr<ProgressElement> Spinner() {
    auto p = std::make_shared<ProgressElement>();
    p->spinner();
    return p;
}

inline std::shared_ptr<ProgressElement> CircleProgress(float value = 0.0f) {
    auto p = std::make_shared<ProgressElement>();
    p->circle().setValue(value);
    return p;
}

} // namespace OledUI
