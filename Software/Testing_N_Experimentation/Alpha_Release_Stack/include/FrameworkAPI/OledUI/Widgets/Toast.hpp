/**
 * @file Toast.hpp
 * @brief Toast notification widget for OLED UI
 */

#pragma once

#include "../Core/Element.hpp"
#include <queue>

namespace OledUI {

//=============================================================================
// ToastPosition - Where toast appears
//=============================================================================
enum class ToastPosition {
    TOP,
    BOTTOM,
    CENTER
};

//=============================================================================
// Toast - Temporary notification message
//=============================================================================
class Toast : public Element {
private:
    struct ToastMessage {
        std::string text;
        Icon icon;
        uint32_t duration;
    };
    
    std::queue<ToastMessage> queue_;
    ToastMessage current_;
    bool showing_ = false;
    uint32_t showTime_ = 0;
    float animProgress_ = 0.0f;
    ToastPosition position_ = ToastPosition::BOTTOM;
    
    // Default duration in ms
    static constexpr uint32_t DEFAULT_DURATION = 2000;
    static constexpr uint32_t SHORT_DURATION = 1000;
    static constexpr uint32_t LONG_DURATION = 3500;
    
public:
    Toast() : Element("toast") {
        style_.width = OLED_WIDTH - 16;  // Some margin
        style_.height = 16;
    }
    
    //-------------------------------------------------------------------------
    // Show Toast
    //-------------------------------------------------------------------------
    Toast& show(const std::string& message, uint32_t durationMs = DEFAULT_DURATION) {
        return show(message, Icon::NONE, durationMs);
    }
    
    Toast& show(const std::string& message, Icon icon, uint32_t durationMs = DEFAULT_DURATION) {
        ToastMessage msg{message, icon, durationMs};
        
        if (showing_) {
            // Queue it
            queue_.push(msg);
        } else {
            showMessage(msg);
        }
        
        return *this;
    }
    
    // Convenience methods
    Toast& info(const std::string& msg) { return show(msg, Icon::INFO); }
    Toast& success(const std::string& msg) { return show(msg, Icon::CHECK); }
    Toast& warning(const std::string& msg) { return show(msg, Icon::WARNING); }
    Toast& error(const std::string& msg) { return show(msg, Icon::ERROR); }
    
    //-------------------------------------------------------------------------
    // Configuration
    //-------------------------------------------------------------------------
    Toast& setPosition(ToastPosition p) { position_ = p; return *this; }
    
    bool isShowing() const { return showing_; }
    
    //-------------------------------------------------------------------------
    // Update (call in loop)
    //-------------------------------------------------------------------------
    void tick(uint32_t currentTimeMs) {
        if (!showing_) return;
        
        uint32_t elapsed = currentTimeMs - showTime_;
        
        // Animation phases
        const uint32_t fadeIn = 200;
        const uint32_t fadeOut = 200;
        
        if (elapsed < fadeIn) {
            // Fade in
            animProgress_ = (float)elapsed / fadeIn;
        } else if (elapsed < current_.duration - fadeOut) {
            // Fully visible
            animProgress_ = 1.0f;
        } else if (elapsed < current_.duration) {
            // Fade out
            animProgress_ = 1.0f - (float)(elapsed - (current_.duration - fadeOut)) / fadeOut;
        } else {
            // Done
            showing_ = false;
            animProgress_ = 0.0f;
            
            // Show next in queue
            if (!queue_.empty()) {
                showMessage(queue_.front());
                queue_.pop();
            }
        }
        
        markDirty();
    }
    
    //-------------------------------------------------------------------------
    // Layout
    //-------------------------------------------------------------------------
    void layout(int16_t x, int16_t y, int16_t width, int16_t height) override {
        // Ignore incoming x,y since toast has fixed positions
        (void)x; (void)y; (void)width; (void)height;
        
        Rect bounds;
        bounds.width = style_.width;
        bounds.height = style_.height;
        
        // Center horizontally
        bounds.x = (OLED_WIDTH - bounds.width) / 2;
        
        // Vertical position based on setting
        switch (position_) {
            case ToastPosition::TOP:
                bounds.y = 4;
                break;
            case ToastPosition::CENTER:
                bounds.y = (OLED_HEIGHT - bounds.height) / 2;
                break;
            case ToastPosition::BOTTOM:
            default:
                bounds.y = OLED_HEIGHT - bounds.height - 4;
                break;
        }
        
        bounds_ = bounds;
    }
    
    bool isFocusable() const override { return false; }
    
    void render(GpuCommands* gpu) override {
        if (!showing_ || animProgress_ <= 0) return;
        Element::render(gpu);
    }
    
protected:
    void showMessage(const ToastMessage& msg) {
        current_ = msg;
        showing_ = true;
        showTime_ = millis();  // ESP32 millis()
        animProgress_ = 0.0f;
        markDirty();
    }
    
    void renderContent(GpuCommands* gpu) override;
};

//=============================================================================
// Factory Function
//=============================================================================

inline std::shared_ptr<Toast> CreateToast() {
    return std::make_shared<Toast>();
}

} // namespace OledUI
