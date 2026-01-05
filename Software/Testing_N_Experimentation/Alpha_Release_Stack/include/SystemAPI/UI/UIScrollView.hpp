/*****************************************************************
 * @file UIScrollView.hpp
 * @brief UI Framework ScrollView - Scrollable content container
 * 
 * @author ARCOS
 * @version 1.0
 *****************************************************************/

#pragma once

#include "UIContainer.hpp"

namespace SystemAPI {
namespace UI {

/**
 * @brief Scroll direction
 */
enum class ScrollDirection : uint8_t {
  NONE,
  VERTICAL,
  HORIZONTAL,
  BOTH
};

/**
 * @brief Scrollable container
 * 
 * @example
 * ```cpp
 * auto scroll = new UIScrollView();
 * scroll->setSize(120, 100);
 * scroll->setScrollDirection(ScrollDirection::VERTICAL);
 * 
 * // Add content that exceeds view size
 * for (int i = 0; i < 20; i++) {
 *   scroll->addChild(new UIText(("Item " + std::to_string(i)).c_str()));
 * }
 * 
 * // Scroll to position
 * scroll->scrollToY(50);
 * ```
 */
class UIScrollView : public UIContainer {
public:
  UIScrollView() {
    setOverflow(Overflow::SCROLL);
    setLayoutMode(LayoutMode::FLEX);
    setFlexDirection(FlexDirection::COLUMN);
  }
  
  const char* getTypeName() const override { return "UIScrollView"; }
  
  // ---- Scroll Direction ----
  
  void setScrollDirection(ScrollDirection dir) { scrollDir_ = dir; markDirty(); }
  ScrollDirection getScrollDirection() const { return scrollDir_; }
  
  // ---- Scroll Position ----
  
  void scrollToX(int16_t x) { 
    scrollX_ = clamp(x, (int16_t)0, (int16_t)(contentSize_.width - bounds_.width)); 
    markDirty(); 
  }
  
  void scrollToY(int16_t y) { 
    scrollY_ = clamp(y, (int16_t)0, (int16_t)(contentSize_.height - bounds_.height)); 
    markDirty(); 
  }
  
  void scrollTo(int16_t x, int16_t y) {
    scrollToX(x);
    scrollToY(y);
  }
  
  /**
   * @brief Scroll to make element visible
   */
  void scrollToElement(UIElement* element) {
    if (!element) return;
    
    Rect elemBounds = element->getBounds();
    Rect content = style_.contentRect(bounds_);
    
    // Vertical
    if (scrollDir_ == ScrollDirection::VERTICAL || scrollDir_ == ScrollDirection::BOTH) {
      if (elemBounds.y < scrollY_) {
        scrollToY(elemBounds.y);
      } else if (elemBounds.bottom() > scrollY_ + content.height) {
        scrollToY(elemBounds.bottom() - content.height);
      }
    }
    
    // Horizontal
    if (scrollDir_ == ScrollDirection::HORIZONTAL || scrollDir_ == ScrollDirection::BOTH) {
      if (elemBounds.x < scrollX_) {
        scrollToX(elemBounds.x);
      } else if (elemBounds.right() > scrollX_ + content.width) {
        scrollToX(elemBounds.right() - content.width);
      }
    }
  }
  
  /**
   * @brief Scroll to top
   */
  void scrollToTop() { scrollToY(0); }
  
  /**
   * @brief Scroll to bottom
   */
  void scrollToBottom() { 
    scrollToY(contentSize_.height - bounds_.height); 
  }
  
  // ---- Scroll Metrics ----
  
  bool canScrollUp() const { return scrollY_ > 0; }
  bool canScrollDown() const { return scrollY_ < contentSize_.height - bounds_.height; }
  bool canScrollLeft() const { return scrollX_ > 0; }
  bool canScrollRight() const { return scrollX_ < contentSize_.width - bounds_.width; }
  
  float getVerticalScrollRatio() const {
    if (contentSize_.height <= bounds_.height) return 0.0f;
    return (float)scrollY_ / (contentSize_.height - bounds_.height);
  }
  
  float getHorizontalScrollRatio() const {
    if (contentSize_.width <= bounds_.width) return 0.0f;
    return (float)scrollX_ / (contentSize_.width - bounds_.width);
  }
  
  // ---- Scrollbar ----
  
  void setShowScrollbar(bool show) { showScrollbar_ = show; markDirty(); }
  bool getShowScrollbar() const { return showScrollbar_; }
  
  void setScrollbarWidth(uint8_t width) { scrollbarWidth_ = width; markDirty(); }
  uint8_t getScrollbarWidth() const { return scrollbarWidth_; }
  
  // ---- Smooth Scrolling ----
  
  void setSmooth(bool smooth) { smooth_ = smooth; }
  bool isSmooth() const { return smooth_; }
  
  void animateScrollTo(int16_t x, int16_t y) {
    targetScrollX_ = x;
    targetScrollY_ = y;
    animating_ = true;
  }
  
  void update(uint32_t deltaMs) override {
    UIContainer::update(deltaMs);
    
    if (animating_ && smooth_) {
      float factor = deltaMs * 0.01f;
      scrollX_ += (targetScrollX_ - scrollX_) * factor;
      scrollY_ += (targetScrollY_ - scrollY_) * factor;
      
      if (abs(scrollX_ - targetScrollX_) < 1 && abs(scrollY_ - targetScrollY_) < 1) {
        scrollX_ = targetScrollX_;
        scrollY_ = targetScrollY_;
        animating_ = false;
      }
      markDirty();
    }
  }
  
  // ---- Input Handling ----
  
  bool handleInput(InputEvent& event) override {
    // Let children handle first
    if (UIContainer::handleInput(event)) {
      return true;
    }
    
    // Handle scroll
    if (event.type == InputEvent::BUTTON && 
        (event.btn.event == ButtonEvent::PRESSED || event.btn.event == ButtonEvent::REPEATED)) {
      int16_t scrollAmount = 16;
      
      if (scrollDir_ == ScrollDirection::VERTICAL || scrollDir_ == ScrollDirection::BOTH) {
        if (event.btn.button == Button::UP || event.btn.button == Button::ENCODER_CCW) {
          scrollBy(0, -scrollAmount);
          event.consumed = true;
          return true;
        } else if (event.btn.button == Button::DOWN || event.btn.button == Button::ENCODER_CW) {
          scrollBy(0, scrollAmount);
          event.consumed = true;
          return true;
        }
      }
      
      if (scrollDir_ == ScrollDirection::HORIZONTAL || scrollDir_ == ScrollDirection::BOTH) {
        if (event.btn.button == Button::LEFT) {
          scrollBy(-scrollAmount, 0);
          event.consumed = true;
          return true;
        } else if (event.btn.button == Button::RIGHT) {
          scrollBy(scrollAmount, 0);
          event.consumed = true;
          return true;
        }
      }
    }
    
    return false;
  }
  
  // ---- Rendering ----
  
  void render(UIRenderer& renderer) override;  // Implemented in UIRenderer.hpp
  
protected:
  ScrollDirection scrollDir_ = ScrollDirection::VERTICAL;
  bool showScrollbar_ = true;
  uint8_t scrollbarWidth_ = 4;
  bool smooth_ = true;
  
  int16_t targetScrollX_ = 0;
  int16_t targetScrollY_ = 0;
  bool animating_ = false;
};

/**
 * @brief List view (scrollable list of items)
 */
class UIListView : public UIScrollView {
public:
  UIListView() {
    setScrollDirection(ScrollDirection::VERTICAL);
  }
  
  const char* getTypeName() const override { return "UIListView"; }
  
  // ---- Selection ----
  
  void setSelectedIndex(int index) {
    if (index >= 0 && index < getChildCount()) {
      if (selectedIndex_ >= 0 && selectedIndex_ < getChildCount()) {
        getChild(selectedIndex_)->blur();
      }
      selectedIndex_ = index;
      getChild(selectedIndex_)->focus();
      scrollToElement(getChild(selectedIndex_));
      markDirty();
    }
  }
  
  int getSelectedIndex() const { return selectedIndex_; }
  
  void selectNext() {
    setSelectedIndex(std::min(selectedIndex_ + 1, getChildCount() - 1));
  }
  
  void selectPrev() {
    setSelectedIndex(std::max(selectedIndex_ - 1, 0));
  }
  
  bool handleInput(InputEvent& event) override {
    if (event.type == InputEvent::BUTTON && event.btn.event == ButtonEvent::PRESSED) {
      if (event.btn.button == Button::UP || event.btn.button == Button::ENCODER_CCW) {
        selectPrev();
        event.consumed = true;
        return true;
      } else if (event.btn.button == Button::DOWN || event.btn.button == Button::ENCODER_CW) {
        selectNext();
        event.consumed = true;
        return true;
      }
    }
    return UIScrollView::handleInput(event);
  }
  
protected:
  int selectedIndex_ = 0;
};

} // namespace UI
} // namespace SystemAPI
