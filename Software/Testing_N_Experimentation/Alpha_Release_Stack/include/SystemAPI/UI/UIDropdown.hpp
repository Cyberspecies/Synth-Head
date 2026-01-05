/*****************************************************************
 * @file UIDropdown.hpp
 * @brief UI Framework Dropdown - Dropdown selection control
 * 
 * @author ARCOS
 * @version 1.0
 *****************************************************************/

#pragma once

#include "UIElement.hpp"
#include "UIContainer.hpp"
#include "UIIcon.hpp"
#include <vector>

namespace SystemAPI {
namespace UI {

/**
 * @brief Dropdown selection element
 * 
 * @example
 * ```cpp
 * auto dropdown = new UIDropdown();
 * dropdown->addItem("Option 1");
 * dropdown->addItem("Option 2");
 * dropdown->addItem("Option 3");
 * dropdown->setSelectedIndex(0);
 * 
 * dropdown->onSelect([](int index, const char* item) {
 *   printf("Selected: %d - %s\n", index, item);
 * });
 * ```
 */
class UIDropdown : public UIElement {
public:
  UIDropdown() {
    focusable_ = true;
    style_ = Styles::dropdown();
  }
  
  const char* getTypeName() const override { return "UIDropdown"; }
  bool isInteractive() const override { return true; }
  
  // ---- Items ----
  
  void addItem(const char* item) {
    if (itemCount_ < MAX_ITEMS) {
      strncpy(items_[itemCount_], item, MAX_ITEM_LEN - 1);
      items_[itemCount_][MAX_ITEM_LEN - 1] = '\0';
      itemCount_++;
      markDirty();
    }
  }
  
  void clearItems() {
    itemCount_ = 0;
    selectedIndex_ = -1;
    markDirty();
  }
  
  int getItemCount() const { return itemCount_; }
  
  const char* getItem(int index) const {
    if (index >= 0 && index < itemCount_) {
      return items_[index];
    }
    return nullptr;
  }
  
  // ---- Selection ----
  
  void setSelectedIndex(int index) {
    if (index >= -1 && index < itemCount_ && index != selectedIndex_) {
      selectedIndex_ = index;
      markDirty();
      if (onSelect_ && selectedIndex_ >= 0) {
        onSelect_(selectedIndex_, items_[selectedIndex_]);
      }
      if (onChange_) onChange_(this);
    }
  }
  
  int getSelectedIndex() const { return selectedIndex_; }
  
  const char* getSelectedItem() const {
    return getItem(selectedIndex_);
  }
  
  void selectNext() {
    if (itemCount_ > 0) {
      setSelectedIndex((selectedIndex_ + 1) % itemCount_);
    }
  }
  
  void selectPrev() {
    if (itemCount_ > 0) {
      setSelectedIndex((selectedIndex_ - 1 + itemCount_) % itemCount_);
    }
  }
  
  // ---- Open/Close ----
  
  bool isOpen() const { return open_; }
  
  void open() { 
    open_ = true; 
    highlightedIndex_ = selectedIndex_ >= 0 ? selectedIndex_ : 0;
    markDirty(); 
  }
  
  void close() { 
    open_ = false; 
    markDirty(); 
  }
  
  void toggle() { 
    if (open_) close(); 
    else open(); 
  }
  
  // ---- Placeholder ----
  
  void setPlaceholder(const char* text) {
    strncpy(placeholder_, text, sizeof(placeholder_) - 1);
    markDirty();
  }
  
  const char* getPlaceholder() const { return placeholder_; }
  
  // ---- Appearance ----
  
  void setMaxVisibleItems(int max) { maxVisibleItems_ = max; }
  int getMaxVisibleItems() const { return maxVisibleItems_; }
  
  int getHighlightedIndex() const { return highlightedIndex_; }
  
  // ---- Callback ----
  
  void onSelect(SelectCallback cb) { onSelect_ = cb; }
  
  // ---- Input Handling ----
  
  bool handleInput(InputEvent& event) override {
    if (!isEnabled() || !isVisible()) return false;
    
    if (event.type == InputEvent::BUTTON && event.btn.event == ButtonEvent::PRESSED) {
      if (event.btn.button == Button::SELECT) {
        if (open_) {
          setSelectedIndex(highlightedIndex_);
          close();
        } else {
          open();
        }
        event.consumed = true;
        return true;
      }
      
      if (open_) {
        if (event.btn.button == Button::UP || event.btn.button == Button::ENCODER_CCW) {
          highlightedIndex_ = (highlightedIndex_ - 1 + itemCount_) % itemCount_;
          markDirty();
          event.consumed = true;
          return true;
        } else if (event.btn.button == Button::DOWN || event.btn.button == Button::ENCODER_CW) {
          highlightedIndex_ = (highlightedIndex_ + 1) % itemCount_;
          markDirty();
          event.consumed = true;
          return true;
        } else if (event.btn.button == Button::BACK) {
          close();
          event.consumed = true;
          return true;
        }
      }
    }
    
    return UIElement::handleInput(event);
  }
  
  // ---- Layout ----
  
  Size getPreferredSize() const override {
    FontInfo font = getFontInfo(style_.getFontSize());
    
    // Find widest item
    uint16_t maxWidth = textWidth(placeholder_, style_.getFontSize());
    for (int i = 0; i < itemCount_; i++) {
      maxWidth = std::max(maxWidth, textWidth(items_[i], style_.getFontSize()));
    }
    
    return Size(
      std::max(style_.getMinWidth(), (uint16_t)(maxWidth + 16 + style_.horizontalSpace())),  // +16 for arrow
      std::max(style_.getMinHeight(), (uint16_t)(font.charHeight + style_.verticalSpace()))
    );
  }
  
  /**
   * @brief Get size of dropdown list when open
   */
  Size getOpenSize() const {
    FontInfo font = getFontInfo(style_.getFontSize());
    int visibleItems = std::min(itemCount_, maxVisibleItems_);
    uint16_t listHeight = visibleItems * (font.charHeight + 4);
    
    return Size(bounds_.width, listHeight);
  }
  
  // ---- Rendering ----
  
  void render(UIRenderer& renderer) override;  // Implemented in UIRenderer.hpp
  
protected:
  static constexpr int MAX_ITEMS = 16;
  static constexpr int MAX_ITEM_LEN = 32;
  
  char items_[MAX_ITEMS][MAX_ITEM_LEN];
  int itemCount_ = 0;
  int selectedIndex_ = -1;
  int highlightedIndex_ = 0;
  
  bool open_ = false;
  char placeholder_[32] = "Select...";
  int maxVisibleItems_ = 5;
  
  SelectCallback onSelect_;
};

/**
 * @brief Combo box (editable dropdown)
 */
class UIComboBox : public UIDropdown {
public:
  UIComboBox() : UIDropdown() {
    editable_ = true;
  }
  
  const char* getTypeName() const override { return "UIComboBox"; }
  
  void setText(const char* text) {
    strncpy(inputText_, text, sizeof(inputText_) - 1);
    markDirty();
  }
  
  const char* getText() const { return inputText_; }
  
protected:
  bool editable_ = true;
  char inputText_[32] = "";
};

} // namespace UI
} // namespace SystemAPI
