/**
 * @file Dialog.hpp
 * @brief Modal dialog widget for OLED UI
 */

#pragma once

#include "../Core/Element.hpp"
#include "../Elements/TextElement.hpp"
#include "../Elements/ButtonElement.hpp"
#include "../Elements/ContainerElement.hpp"
#include <vector>

namespace OledUI {

//=============================================================================
// DialogButton - Button configuration for dialog
//=============================================================================
struct DialogButton {
    std::string label;
    Callback action;
    bool primary = false;  // Highlighted button
    
    DialogButton() = default;
    DialogButton(const std::string& l, Callback a = nullptr, bool p = false)
        : label(l), action(a), primary(p) {}
};

//=============================================================================
// DialogType - Preset dialog types
//=============================================================================
enum class DialogType {
    INFO,       // Info message with OK button
    WARNING,    // Warning with OK button
    ERROR,      // Error with OK button
    CONFIRM,    // Yes/No buttons
    INPUT,      // Text input (not implemented yet)
    PROGRESS,   // Progress dialog (not implemented yet)
    CUSTOM      // Custom buttons
};

//=============================================================================
// Dialog - Modal dialog overlay
//=============================================================================
class Dialog : public Element {
private:
    std::string title_;
    std::string message_;
    std::vector<DialogButton> buttons_;
    int selectedButton_ = 0;
    DialogType type_ = DialogType::INFO;
    Icon icon_ = Icon::NONE;
    bool visible_ = false;
    Callback onDismiss_;
    
    // Animation state
    float animProgress_ = 0.0f;
    bool animating_ = false;
    
public:
    Dialog() : Element("dialog") {
        // Dialogs cover the full screen
        style_.width = OLED_WIDTH;
        style_.height = OLED_HEIGHT;
    }
    
    //-------------------------------------------------------------------------
    // Content
    //-------------------------------------------------------------------------
    Dialog& setTitle(const std::string& t) { title_ = t; markDirty(); return *this; }
    Dialog& setMessage(const std::string& m) { message_ = m; markDirty(); return *this; }
    Dialog& setIcon(Icon i) { icon_ = i; markDirty(); return *this; }
    
    const std::string& getTitle() const { return title_; }
    const std::string& getMessage() const { return message_; }
    
    //-------------------------------------------------------------------------
    // Buttons
    //-------------------------------------------------------------------------
    Dialog& addButton(const DialogButton& btn) {
        buttons_.push_back(btn);
        markDirty();
        return *this;
    }
    
    Dialog& addButton(const std::string& label, Callback action = nullptr) {
        buttons_.push_back(DialogButton(label, action));
        markDirty();
        return *this;
    }
    
    Dialog& setButtons(const std::vector<DialogButton>& btns) {
        buttons_ = btns;
        selectedButton_ = 0;
        markDirty();
        return *this;
    }
    
    Dialog& clearButtons() {
        buttons_.clear();
        selectedButton_ = 0;
        markDirty();
        return *this;
    }
    
    //-------------------------------------------------------------------------
    // Preset Dialogs
    //-------------------------------------------------------------------------
    Dialog& info(const std::string& title, const std::string& msg) {
        type_ = DialogType::INFO;
        title_ = title;
        message_ = msg;
        icon_ = Icon::INFO;
        buttons_.clear();
        buttons_.push_back(DialogButton("OK", nullptr, true));
        markDirty();
        return *this;
    }
    
    Dialog& warning(const std::string& title, const std::string& msg) {
        type_ = DialogType::WARNING;
        title_ = title;
        message_ = msg;
        icon_ = Icon::WARNING;
        buttons_.clear();
        buttons_.push_back(DialogButton("OK", nullptr, true));
        markDirty();
        return *this;
    }
    
    Dialog& error(const std::string& title, const std::string& msg) {
        type_ = DialogType::ERROR;
        title_ = title;
        message_ = msg;
        icon_ = Icon::ERROR;
        buttons_.clear();
        buttons_.push_back(DialogButton("OK", nullptr, true));
        markDirty();
        return *this;
    }
    
    Dialog& confirm(const std::string& title, const std::string& msg,
                   Callback onYes, Callback onNo = nullptr) {
        type_ = DialogType::CONFIRM;
        title_ = title;
        message_ = msg;
        icon_ = Icon::QUESTION;
        buttons_.clear();
        buttons_.push_back(DialogButton("No", onNo));
        buttons_.push_back(DialogButton("Yes", onYes, true));
        selectedButton_ = 1;  // Default to Yes
        markDirty();
        return *this;
    }
    
    //-------------------------------------------------------------------------
    // Visibility
    //-------------------------------------------------------------------------
    Dialog& show() {
        visible_ = true;
        animating_ = true;
        animProgress_ = 0.0f;
        selectedButton_ = 0;
        // Find primary button
        for (size_t i = 0; i < buttons_.size(); i++) {
            if (buttons_[i].primary) {
                selectedButton_ = i;
                break;
            }
        }
        markDirty();
        return *this;
    }
    
    Dialog& hide() {
        visible_ = false;
        if (onDismiss_) onDismiss_();
        markDirty();
        return *this;
    }
    
    bool isShowing() const { return visible_; }
    
    Dialog& setOnDismiss(Callback cb) { onDismiss_ = cb; return *this; }
    
    //-------------------------------------------------------------------------
    // Selection
    //-------------------------------------------------------------------------
    void selectNext() {
        if (!buttons_.empty()) {
            selectedButton_ = (selectedButton_ + 1) % buttons_.size();
            markDirty();
        }
    }
    
    void selectPrev() {
        if (!buttons_.empty()) {
            selectedButton_--;
            if (selectedButton_ < 0) selectedButton_ = buttons_.size() - 1;
            markDirty();
        }
    }
    
    void selectButton() {
        if (selectedButton_ >= 0 && selectedButton_ < (int)buttons_.size()) {
            auto& btn = buttons_[selectedButton_];
            if (btn.action) btn.action();
        }
        hide();
    }
    
    //-------------------------------------------------------------------------
    // Animation
    //-------------------------------------------------------------------------
    void tick(float deltaTime) {
        if (animating_) {
            animProgress_ += deltaTime * 5.0f;  // Animation speed
            if (animProgress_ >= 1.0f) {
                animProgress_ = 1.0f;
                animating_ = false;
            }
            markDirty();
        }
    }
    
    //-------------------------------------------------------------------------
    // Interaction
    //-------------------------------------------------------------------------
    bool handleInput(InputEvent event) override {
        if (!visible_ || !style_.enabled) return false;
        
        switch (event) {
            case InputEvent::LEFT:
                selectPrev();
                return true;
                
            case InputEvent::RIGHT:
                selectNext();
                return true;
                
            case InputEvent::SELECT:
                selectButton();
                return true;
                
            case InputEvent::BACK:
                hide();
                return true;
                
            default:
                break;
        }
        
        return true;  // Dialog consumes all input when visible
    }
    
    bool isFocusable() const override { return visible_; }
    
    //-------------------------------------------------------------------------
    // Rendering
    //-------------------------------------------------------------------------
    void render(GpuCommands* gpu) override {
        if (!visible_) return;
        Element::render(gpu);
    }
    
protected:
    void renderContent(GpuCommands* gpu) override;
};

//=============================================================================
// Factory Functions
//=============================================================================

inline std::shared_ptr<Dialog> CreateDialog() {
    return std::make_shared<Dialog>();
}

inline std::shared_ptr<Dialog> InfoDialog(const std::string& title, 
                                          const std::string& message) {
    auto d = std::make_shared<Dialog>();
    d->info(title, message);
    return d;
}

inline std::shared_ptr<Dialog> ConfirmDialog(const std::string& title,
                                            const std::string& message,
                                            Callback onYes,
                                            Callback onNo = nullptr) {
    auto d = std::make_shared<Dialog>();
    d->confirm(title, message, onYes, onNo);
    return d;
}

} // namespace OledUI
