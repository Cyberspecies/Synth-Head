/*****************************************************************
 * @file UIDialog.hpp
 * @brief UI Framework Dialog - Modal dialog system
 * 
 * @author ARCOS
 * @version 1.0
 *****************************************************************/

#pragma once

#include "UIContainer.hpp"
#include "UIButton.hpp"
#include "UIText.hpp"
#include <functional>

namespace SystemAPI {
namespace UI {

/**
 * @brief Dialog result
 */
enum class DialogResult : uint8_t {
  NONE,
  OK,
  CANCEL,
  YES,
  NO,
  CUSTOM
};

/**
 * @brief Dialog buttons configuration
 */
enum class DialogButtons : uint8_t {
  OK,
  OK_CANCEL,
  YES_NO,
  YES_NO_CANCEL,
  CUSTOM
};

/**
 * @brief Modal dialog
 * 
 * @example
 * ```cpp
 * auto dialog = new UIDialog("Confirm", "Are you sure?", DialogButtons::YES_NO);
 * dialog->onResult([](DialogResult result) {
 *   if (result == DialogResult::YES) {
 *     // Do something
 *   }
 * });
 * dialog->show();
 * 
 * // Input dialog
 * auto input = UIDialog::input("Enter Name", "", [](const char* text) {
 *   printf("Entered: %s\n", text);
 * });
 * ```
 */
class UIDialog : public UIContainer {
public:
  UIDialog() {
    style_ = Styles::dialog();
    setLayoutMode(LayoutMode::FLEX);
    setFlexDirection(FlexDirection::COLUMN);
  }
  
  UIDialog(const char* title, const char* message, DialogButtons buttons = DialogButtons::OK)
    : UIDialog() {
    setTitle(title);
    setMessage(message);
    setButtons(buttons);
  }
  
  const char* getTypeName() const override { return "UIDialog"; }
  
  // ---- Content ----
  
  void setTitle(const char* title) {
    strncpy(title_, title, sizeof(title_) - 1);
    markDirty();
  }
  
  const char* getTitle() const { return title_; }
  
  void setMessage(const char* message) {
    strncpy(message_, message, sizeof(message_) - 1);
    markDirty();
  }
  
  const char* getMessage() const { return message_; }
  
  // ---- Buttons ----
  
  void setButtons(DialogButtons buttons) {
    buttons_ = buttons;
    rebuildButtons();
  }
  
  DialogButtons getButtons() const { return buttons_; }
  
  void setCustomButton(int index, const char* label) {
    if (index >= 0 && index < (int)buttonLabels_.size()) {
      buttonLabels_[index] = label;
      rebuildButtons();
    }
  }
  
  // ---- Result ----
  
  void onResult(std::function<void(DialogResult)> cb) { onResult_ = cb; }
  
  DialogResult getResult() const { return result_; }
  
  // ---- Show/Hide ----
  
  void show() {
    setVisible(true);
    result_ = DialogResult::NONE;
    // Focus first button
    if (!buttonElements_.empty()) {
      buttonElements_[0]->focus();
      focusedButton_ = 0;
    }
  }
  
  void close(DialogResult result = DialogResult::CANCEL) {
    result_ = result;
    setVisible(false);
    if (onResult_) onResult_(result_);
  }
  
  // ---- Modal Overlay ----
  
  void setShowOverlay(bool show) { showOverlay_ = show; }
  bool getShowOverlay() const { return showOverlay_; }
  
  void setOverlayColor(const Color& color) { overlayColor_ = color; }
  Color getOverlayColor() const { return overlayColor_; }
  
  // ---- Input Handling ----
  
  bool handleInput(InputEvent& event) override {
    if (!isVisible()) return false;
    
    if (event.type == InputEvent::BUTTON && event.btn.event == ButtonEvent::PRESSED) {
      switch (event.btn.button) {
        case Button::LEFT:
          focusPrevButton();
          event.consumed = true;
          return true;
          
        case Button::RIGHT:
          focusNextButton();
          event.consumed = true;
          return true;
          
        case Button::SELECT:
          selectCurrentButton();
          event.consumed = true;
          return true;
          
        case Button::BACK:
          close(DialogResult::CANCEL);
          event.consumed = true;
          return true;
          
        default:
          break;
      }
    }
    
    return UIContainer::handleInput(event);
  }
  
  // ---- Layout ----
  
  void layout() override {
    // Center on screen
    Size pref = getPreferredSize();
    setBounds(
      (screenW_ - pref.width) / 2,
      (screenH_ - pref.height) / 2,
      pref.width,
      pref.height
    );
    UIContainer::layout();
  }
  
  Size getPreferredSize() const override {
    FontInfo titleFont = getFontInfo(FontSize::MEDIUM);
    FontInfo msgFont = getFontInfo(FontSize::SMALL);
    
    uint16_t titleW = textWidth(title_, FontSize::MEDIUM);
    uint16_t msgW = textWidth(message_, FontSize::SMALL);
    uint16_t buttonW = buttonElements_.size() * 40 + (buttonElements_.size() - 1) * 8;
    
    uint16_t maxW = std::max({titleW, msgW, buttonW});
    
    return Size(
      std::max((uint16_t)80, (uint16_t)(maxW + style_.horizontalSpace())),
      std::max((uint16_t)60, (uint16_t)(
        titleFont.charHeight + 4 +
        msgFont.charHeight + 12 +
        20 +  // Button height
        style_.verticalSpace()
      ))
    );
  }
  
  void setScreenSize(uint16_t w, uint16_t h) { screenW_ = w; screenH_ = h; }
  
  // ---- Factory Methods ----
  
  /**
   * @brief Create alert dialog
   */
  static UIDialog* alert(const char* title, const char* message) {
    return new UIDialog(title, message, DialogButtons::OK);
  }
  
  /**
   * @brief Create confirm dialog
   */
  static UIDialog* confirm(const char* title, const char* message,
                           std::function<void(bool)> callback) {
    auto* dialog = new UIDialog(title, message, DialogButtons::YES_NO);
    dialog->onResult([callback](DialogResult result) {
      if (callback) callback(result == DialogResult::YES);
    });
    return dialog;
  }
  
  // ---- Rendering ----
  
  void render(UIRenderer& renderer) override;  // Implemented in UIRenderer.hpp
  
protected:
  char title_[32] = "";
  char message_[128] = "";
  
  DialogButtons buttons_ = DialogButtons::OK;
  std::vector<const char*> buttonLabels_;
  std::vector<UIButton*> buttonElements_;
  int focusedButton_ = 0;
  
  DialogResult result_ = DialogResult::NONE;
  std::function<void(DialogResult)> onResult_;
  
  bool showOverlay_ = true;
  Color overlayColor_ = Color(0, 0, 0, 150);
  
  uint16_t screenW_ = 128;
  uint16_t screenH_ = 128;
  
  void rebuildButtons() {
    // Clear existing buttons
    for (auto* btn : buttonElements_) {
      removeChild(btn);
      delete btn;
    }
    buttonElements_.clear();
    buttonLabels_.clear();
    
    // Create buttons based on type
    switch (buttons_) {
      case DialogButtons::OK:
        addDialogButton("OK", DialogResult::OK);
        break;
        
      case DialogButtons::OK_CANCEL:
        addDialogButton("OK", DialogResult::OK);
        addDialogButton("Cancel", DialogResult::CANCEL);
        break;
        
      case DialogButtons::YES_NO:
        addDialogButton("Yes", DialogResult::YES);
        addDialogButton("No", DialogResult::NO);
        break;
        
      case DialogButtons::YES_NO_CANCEL:
        addDialogButton("Yes", DialogResult::YES);
        addDialogButton("No", DialogResult::NO);
        addDialogButton("Cancel", DialogResult::CANCEL);
        break;
        
      default:
        break;
    }
    
    markDirty();
  }
  
  void addDialogButton(const char* label, DialogResult result) {
    auto* btn = new UIButton(label);
    btn->onClick([this, result](UIElement*) {
      close(result);
    });
    buttonLabels_.push_back(label);
    buttonElements_.push_back(btn);
    addChild(btn);
  }
  
  void focusNextButton() {
    if (buttonElements_.empty()) return;
    buttonElements_[focusedButton_]->blur();
    focusedButton_ = (focusedButton_ + 1) % buttonElements_.size();
    buttonElements_[focusedButton_]->focus();
    markDirty();
  }
  
  void focusPrevButton() {
    if (buttonElements_.empty()) return;
    buttonElements_[focusedButton_]->blur();
    focusedButton_ = (focusedButton_ - 1 + buttonElements_.size()) % buttonElements_.size();
    buttonElements_[focusedButton_]->focus();
    markDirty();
  }
  
  void selectCurrentButton() {
    if (focusedButton_ >= 0 && focusedButton_ < (int)buttonElements_.size()) {
      // Trigger the button's click callback
      InputEvent clickEvent = InputEvent::buttonRelease(Button::SELECT);
      buttonElements_[focusedButton_]->handleInput(clickEvent);
    }
  }
};

/**
 * @brief Progress dialog
 */
class UIProgressDialog : public UIDialog {
public:
  UIProgressDialog(const char* title, const char* message = "Please wait...")
    : UIDialog() {
    setTitle(title);
    setMessage(message);
    buttons_ = DialogButtons::CUSTOM;  // No buttons
    cancellable_ = false;
  }
  
  const char* getTypeName() const override { return "UIProgressDialog"; }
  
  void setProgress(float progress) { progress_ = clamp(progress, 0.0f, 1.0f); markDirty(); }
  float getProgress() const { return progress_; }
  
  void setIndeterminate(bool ind) { indeterminate_ = ind; markDirty(); }
  bool isIndeterminate() const { return indeterminate_; }
  
  void setCancellable(bool can) { cancellable_ = can; rebuildButtons(); }
  bool isCancellable() const { return cancellable_; }
  
  void update(uint32_t deltaMs) override {
    UIDialog::update(deltaMs);
    if (indeterminate_) {
      animPhase_ += deltaMs * 0.002f;
      if (animPhase_ > 1.0f) animPhase_ -= 1.0f;
      markDirty();
    }
  }
  
  float getAnimPhase() const { return animPhase_; }
  
  void render(UIRenderer& renderer) override;  // Implemented in UIRenderer.hpp
  
protected:
  float progress_ = 0.0f;
  bool indeterminate_ = false;
  float animPhase_ = 0.0f;
  bool cancellable_ = false;
};

} // namespace UI
} // namespace SystemAPI
