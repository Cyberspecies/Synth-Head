/*****************************************************************
 * @file UIContainer.hpp
 * @brief UI Framework Container - Layout containers like HTML div
 * 
 * Containers hold and manage child elements. Like HTML div/section,
 * they handle layout (flexbox-style), clipping, and scrolling.
 * 
 * Supports:
 * - Flex layout (row/column, justify, align)
 * - Absolute positioning
 * - Scrolling (with scrollbars)
 * - Clipping
 * - Z-ordering
 * 
 * @author ARCOS
 * @version 1.0
 *****************************************************************/

#pragma once

#include "UIElement.hpp"
#include <vector>
#include <memory>
#include <algorithm>

namespace SystemAPI {
namespace UI {

// ============================================================
// Layout Types
// ============================================================

/**
 * @brief Layout mode for container
 */
enum class LayoutMode : uint8_t {
  NONE,       // No automatic layout (absolute positioning)
  FLEX,       // Flexbox-style layout
  GRID,       // Grid layout (simplified)
  STACK       // Stack children on top of each other
};

// ============================================================
// UIContainer Class
// ============================================================

/**
 * @brief Container that holds child elements
 * 
 * Like HTML div, containers manage layout and child elements.
 * 
 * @example
 * ```cpp
 * // Create a vertical container
 * auto container = new UIContainer();
 * container->setLayoutMode(LayoutMode::FLEX);
 * container->setFlexDirection(FlexDirection::COLUMN);
 * container->setJustifyContent(JustifyContent::SPACE_BETWEEN);
 * container->setPadding(8);
 * 
 * // Add children
 * container->addChild(new UIText("Header", Styles::heading()));
 * container->addChild(new UIButton("Click Me"));
 * container->addChild(new UIText("Footer"));
 * 
 * // Children are automatically laid out
 * container->layout();
 * ```
 */
class UIContainer : public UIElement {
public:
  UIContainer() = default;
  virtual ~UIContainer() {
    clearChildren();
  }
  
  // ---- Type Info ----
  
  const char* getTypeName() const override { return "UIContainer"; }
  bool isContainer() const override { return true; }
  
  // ---- Layout Mode ----
  
  void setLayoutMode(LayoutMode mode) { layoutMode_ = mode; markDirty(); }
  LayoutMode getLayoutMode() const { return layoutMode_; }
  
  // ---- Flex Layout Properties ----
  
  void setFlexDirection(FlexDirection dir) { flexDirection_ = dir; markDirty(); }
  FlexDirection getFlexDirection() const { return flexDirection_; }
  
  void setJustifyContent(JustifyContent justify) { justifyContent_ = justify; markDirty(); }
  JustifyContent getJustifyContent() const { return justifyContent_; }
  
  void setAlignItems(AlignItems align) { alignItems_ = align; markDirty(); }
  AlignItems getAlignItems() const { return alignItems_; }
  
  void setGap(uint8_t gap) { gap_ = gap; markDirty(); }
  uint8_t getGap() const { return gap_; }
  
  // ---- Overflow & Scrolling ----
  
  void setOverflow(Overflow overflow) { overflow_ = overflow; markDirty(); }
  Overflow getOverflow() const { return overflow_; }
  
  void setScrollX(int16_t x) { scrollX_ = x; markDirty(); }
  void setScrollY(int16_t y) { scrollY_ = y; markDirty(); }
  int16_t getScrollX() const { return scrollX_; }
  int16_t getScrollY() const { return scrollY_; }
  
  void scrollTo(int16_t x, int16_t y) { scrollX_ = x; scrollY_ = y; markDirty(); }
  void scrollBy(int16_t dx, int16_t dy) { scrollX_ += dx; scrollY_ += dy; markDirty(); }
  
  Size getContentSize() const { return contentSize_; }
  
  // ---- Child Management ----
  
  /**
   * @brief Add child element
   */
  UIContainer& addChild(UIElement* child) {
    if (child) {
      child->setParent(this);
      children_.push_back(child);
      markDirty();
    }
    return *this;
  }
  
  /**
   * @brief Add child with shared_ptr
   */
  UIContainer& addChild(std::shared_ptr<UIElement> child) {
    if (child) {
      child->setParent(this);
      sharedChildren_.push_back(child);
      children_.push_back(child.get());
      markDirty();
    }
    return *this;
  }
  
  /**
   * @brief Insert child at index
   */
  UIContainer& insertChild(int index, UIElement* child) {
    if (child && index >= 0 && index <= (int)children_.size()) {
      child->setParent(this);
      children_.insert(children_.begin() + index, child);
      markDirty();
    }
    return *this;
  }
  
  /**
   * @brief Remove child
   */
  bool removeChild(UIElement* child) {
    auto it = std::find(children_.begin(), children_.end(), child);
    if (it != children_.end()) {
      (*it)->setParent(nullptr);
      children_.erase(it);
      markDirty();
      return true;
    }
    return false;
  }
  
  /**
   * @brief Remove child at index
   */
  bool removeChildAt(int index) {
    if (index >= 0 && index < (int)children_.size()) {
      children_[index]->setParent(nullptr);
      children_.erase(children_.begin() + index);
      markDirty();
      return true;
    }
    return false;
  }
  
  /**
   * @brief Remove all children
   */
  void clearChildren() {
    for (auto* child : children_) {
      child->setParent(nullptr);
    }
    children_.clear();
    sharedChildren_.clear();
    markDirty();
  }
  
  /**
   * @brief Get child count
   */
  int getChildCount() const { return children_.size(); }
  
  /**
   * @brief Get child at index
   */
  UIElement* getChild(int index) const {
    if (index >= 0 && index < (int)children_.size()) {
      return children_[index];
    }
    return nullptr;
  }
  
  /**
   * @brief Get all children
   */
  const std::vector<UIElement*>& getChildren() const { return children_; }
  
  /**
   * @brief Find child by tag
   */
  UIElement* findByTag(const char* tag) const {
    for (auto* child : children_) {
      if (strcmp(child->getTag(), tag) == 0) {
        return child;
      }
      if (child->isContainer()) {
        UIElement* found = static_cast<UIContainer*>(child)->findByTag(tag);
        if (found) return found;
      }
    }
    return nullptr;
  }
  
  /**
   * @brief Find child by ID
   */
  UIElement* findByID(ElementID id) const {
    for (auto* child : children_) {
      if (child->getID() == id) return child;
      if (child->isContainer()) {
        UIElement* found = static_cast<UIContainer*>(child)->findByID(id);
        if (found) return found;
      }
    }
    return nullptr;
  }
  
  // ---- Layout ----
  
  void layout() override {
    switch (layoutMode_) {
      case LayoutMode::FLEX:
        layoutFlex();
        break;
      case LayoutMode::GRID:
        layoutGrid();
        break;
      case LayoutMode::STACK:
        layoutStack();
        break;
      case LayoutMode::NONE:
      default:
        // Absolute positioning - just layout children
        for (auto* child : children_) {
          child->layout();
        }
        break;
    }
    
    // Calculate content size
    calculateContentSize();
  }
  
  Size getPreferredSize() const override {
    Size pref = UIElement::getPreferredSize();
    
    // Calculate based on children
    if (layoutMode_ == LayoutMode::FLEX) {
      bool horizontal = (flexDirection_ == FlexDirection::ROW || 
                        flexDirection_ == FlexDirection::ROW_REVERSE);
      
      uint16_t totalMain = 0;
      uint16_t maxCross = 0;
      
      for (auto* child : children_) {
        if (child->getVisibility() == Visibility::GONE) continue;
        
        Size childPref = child->getPreferredSize();
        const Edges& margin = child->getStyle().getMargin();
        
        if (horizontal) {
          totalMain += childPref.width + margin.horizontal();
          maxCross = std::max(maxCross, (uint16_t)(childPref.height + margin.vertical()));
        } else {
          totalMain += childPref.height + margin.vertical();
          maxCross = std::max(maxCross, (uint16_t)(childPref.width + margin.horizontal()));
        }
      }
      
      // Add gaps
      if (children_.size() > 1) {
        totalMain += gap_ * (children_.size() - 1);
      }
      
      // Add padding
      const Edges& padding = style_.getPadding();
      
      if (horizontal) {
        pref.width = std::max(pref.width, (uint16_t)(totalMain + padding.horizontal()));
        pref.height = std::max(pref.height, (uint16_t)(maxCross + padding.vertical()));
      } else {
        pref.width = std::max(pref.width, (uint16_t)(maxCross + padding.horizontal()));
        pref.height = std::max(pref.height, (uint16_t)(totalMain + padding.vertical()));
      }
    }
    
    return pref;
  }
  
  // ---- Update ----
  
  void update(uint32_t deltaMs) override {
    UIElement::update(deltaMs);
    for (auto* child : children_) {
      child->update(deltaMs);
    }
  }
  
  // ---- Rendering ----
  
  void render(UIRenderer& renderer) override;  // Implemented in UIRenderer.hpp
  
  // ---- Input Handling ----
  
  bool handleInput(InputEvent& event) override {
    if (!isEnabled() || !isVisible()) return false;
    
    // First, let children handle
    // Process in reverse order (top elements first)
    for (int i = children_.size() - 1; i >= 0; i--) {
      if (children_[i]->handleInput(event)) {
        return true;
      }
    }
    
    // Handle scrolling
    if (overflow_ == Overflow::SCROLL) {
      if (event.type == InputEvent::BUTTON) {
        if (event.btn.button == Button::UP || event.btn.button == Button::ENCODER_CCW) {
          scrollBy(0, -16);
          return true;
        } else if (event.btn.button == Button::DOWN || event.btn.button == Button::ENCODER_CW) {
          scrollBy(0, 16);
          return true;
        }
      }
    }
    
    // Then handle ourselves
    return UIElement::handleInput(event);
  }
  
  // ---- Hit Testing ----
  
  UIElement* elementAt(int16_t x, int16_t y) override {
    if (!isVisible() || !hitTest(x, y)) return nullptr;
    
    // Adjust for scroll
    int16_t localX = x + scrollX_;
    int16_t localY = y + scrollY_;
    
    // Check children in reverse order (top first)
    for (int i = children_.size() - 1; i >= 0; i--) {
      UIElement* hit = children_[i]->elementAt(localX, localY);
      if (hit) return hit;
    }
    
    return this;
  }
  
  // ---- Focus Navigation ----
  
  /**
   * @brief Get first focusable child
   */
  UIElement* getFirstFocusable() const {
    for (auto* child : children_) {
      if (child->isFocusable()) return child;
      if (child->isContainer()) {
        UIElement* found = static_cast<UIContainer*>(child)->getFirstFocusable();
        if (found) return found;
      }
    }
    return nullptr;
  }
  
  /**
   * @brief Get last focusable child
   */
  UIElement* getLastFocusable() const {
    for (int i = children_.size() - 1; i >= 0; i--) {
      if (children_[i]->isFocusable()) return children_[i];
      if (children_[i]->isContainer()) {
        UIElement* found = static_cast<UIContainer*>(children_[i])->getLastFocusable();
        if (found) return found;
      }
    }
    return nullptr;
  }
  
  /**
   * @brief Get next focusable element after given element
   */
  UIElement* getNextFocusable(UIElement* current) const {
    bool found = false;
    for (auto* child : children_) {
      if (found) {
        if (child->isFocusable()) return child;
        if (child->isContainer()) {
          UIElement* next = static_cast<UIContainer*>(child)->getFirstFocusable();
          if (next) return next;
        }
      }
      if (child == current) found = true;
      else if (child->isContainer()) {
        UIElement* next = static_cast<UIContainer*>(child)->getNextFocusable(current);
        if (next) return next;
        // Check if current was in this container
        if (static_cast<UIContainer*>(child)->findByID(current->getID())) {
          found = true;
        }
      }
    }
    return nullptr;
  }
  
  /**
   * @brief Get previous focusable element before given element
   */
  UIElement* getPrevFocusable(UIElement* current) const {
    UIElement* prev = nullptr;
    for (auto* child : children_) {
      if (child == current) return prev;
      if (child->isFocusable()) prev = child;
      if (child->isContainer()) {
        if (static_cast<UIContainer*>(child)->findByID(current->getID())) {
          return static_cast<UIContainer*>(child)->getPrevFocusable(current);
        }
        UIElement* last = static_cast<UIContainer*>(child)->getLastFocusable();
        if (last) prev = last;
      }
    }
    return nullptr;
  }
  
protected:
  // Layout
  LayoutMode layoutMode_ = LayoutMode::NONE;
  FlexDirection flexDirection_ = FlexDirection::COLUMN;
  JustifyContent justifyContent_ = JustifyContent::START;
  AlignItems alignItems_ = AlignItems::STRETCH;
  uint8_t gap_ = 0;
  
  // Scrolling
  Overflow overflow_ = Overflow::HIDDEN;
  int16_t scrollX_ = 0;
  int16_t scrollY_ = 0;
  Size contentSize_;
  
  // Children
  std::vector<UIElement*> children_;
  std::vector<std::shared_ptr<UIElement>> sharedChildren_;  // For owned children
  
  // ---- Layout Methods ----
  
  void layoutFlex() {
    Rect content = style_.contentRect(bounds_);
    bool horizontal = (flexDirection_ == FlexDirection::ROW || 
                      flexDirection_ == FlexDirection::ROW_REVERSE);
    bool reversed = (flexDirection_ == FlexDirection::ROW_REVERSE || 
                    flexDirection_ == FlexDirection::COLUMN_REVERSE);
    
    // Collect visible children
    std::vector<UIElement*> visible;
    for (auto* child : children_) {
      if (child->getVisibility() != Visibility::GONE) {
        visible.push_back(child);
      }
    }
    
    if (visible.empty()) return;
    
    // Calculate total size and flex factors
    uint16_t totalFixed = 0;
    float totalGrow = 0;
    
    for (auto* child : visible) {
      Size pref = child->getPreferredSize();
      const Edges& margin = child->getStyle().getMargin();
      
      if (horizontal) {
        totalFixed += pref.width + margin.horizontal();
      } else {
        totalFixed += pref.height + margin.vertical();
      }
      
      totalGrow += child->getStyle().getFlexGrow();
    }
    
    totalFixed += gap_ * (visible.size() - 1);
    
    // Calculate available space for flex items
    uint16_t mainAxis = horizontal ? content.width : content.height;
    int16_t freeSpace = mainAxis - totalFixed;
    
    // Calculate positions based on justify-content
    int16_t startOffset = 0;
    int16_t itemGap = gap_;
    
    if (freeSpace > 0 && totalGrow == 0) {
      switch (justifyContent_) {
        case JustifyContent::END:
          startOffset = freeSpace;
          break;
        case JustifyContent::CENTER:
          startOffset = freeSpace / 2;
          break;
        case JustifyContent::SPACE_BETWEEN:
          if (visible.size() > 1) {
            itemGap = gap_ + freeSpace / (visible.size() - 1);
          }
          break;
        case JustifyContent::SPACE_AROUND:
          itemGap = gap_ + freeSpace / visible.size();
          startOffset = itemGap / 2;
          break;
        case JustifyContent::SPACE_EVENLY:
          itemGap = gap_ + freeSpace / (visible.size() + 1);
          startOffset = itemGap;
          break;
        default:
          break;
      }
    }
    
    // Layout children
    int16_t pos = startOffset;
    if (reversed) {
      pos = mainAxis - startOffset;
    }
    
    for (size_t i = 0; i < visible.size(); i++) {
      UIElement* child = reversed ? visible[visible.size() - 1 - i] : visible[i];
      Size pref = child->getPreferredSize();
      const Edges& margin = child->getStyle().getMargin();
      
      // Calculate flex size
      uint16_t mainSize = horizontal ? pref.width : pref.height;
      if (totalGrow > 0 && child->getStyle().getFlexGrow() > 0 && freeSpace > 0) {
        mainSize += (freeSpace * child->getStyle().getFlexGrow()) / totalGrow;
      }
      
      // Calculate cross-axis size and position
      uint16_t crossAxis = horizontal ? content.height : content.width;
      uint16_t crossSize = horizontal ? pref.height : pref.width;
      int16_t crossPos = 0;
      
      switch (alignItems_) {
        case AlignItems::END:
          crossPos = crossAxis - crossSize - (horizontal ? margin.vertical() : margin.horizontal());
          break;
        case AlignItems::CENTER:
          crossPos = (crossAxis - crossSize) / 2;
          break;
        case AlignItems::STRETCH:
          crossSize = crossAxis - (horizontal ? margin.vertical() : margin.horizontal());
          break;
        default:
          crossPos = horizontal ? margin.top : margin.left;
          break;
      }
      
      // Set bounds
      if (horizontal) {
        if (reversed) {
          child->setBounds(pos - mainSize - margin.right, crossPos, mainSize, crossSize);
          pos -= mainSize + margin.horizontal() + itemGap;
        } else {
          child->setBounds(pos + margin.left, crossPos, mainSize, crossSize);
          pos += mainSize + margin.horizontal() + itemGap;
        }
      } else {
        if (reversed) {
          child->setBounds(crossPos, pos - mainSize - margin.bottom, crossSize, mainSize);
          pos -= mainSize + margin.vertical() + itemGap;
        } else {
          child->setBounds(crossPos, pos + margin.top, crossSize, mainSize);
          pos += mainSize + margin.vertical() + itemGap;
        }
      }
      
      // Layout child
      child->layout();
    }
  }
  
  void layoutGrid() {
    // Simplified grid - just arrange in rows
    Rect content = style_.contentRect(bounds_);
    int cols = std::max(1, (int)(content.width / 32));  // Assume min cell size
    
    int16_t x = 0;
    int16_t y = 0;
    int16_t rowHeight = 0;
    int col = 0;
    
    for (auto* child : children_) {
      if (child->getVisibility() == Visibility::GONE) continue;
      
      Size pref = child->getPreferredSize();
      uint16_t cellWidth = content.width / cols;
      
      if (col >= cols) {
        col = 0;
        x = 0;
        y += rowHeight + gap_;
        rowHeight = 0;
      }
      
      child->setBounds(x, y, cellWidth - gap_, pref.height);
      child->layout();
      
      x += cellWidth;
      rowHeight = std::max(rowHeight, (int16_t)pref.height);
      col++;
    }
  }
  
  void layoutStack() {
    // All children occupy full content area
    Rect content = style_.contentRect(bounds_);
    
    for (auto* child : children_) {
      if (child->getVisibility() == Visibility::GONE) continue;
      child->setBounds(0, 0, content.width, content.height);
      child->layout();
    }
  }
  
  void calculateContentSize() {
    contentSize_ = Size(0, 0);
    
    for (auto* child : children_) {
      if (child->getVisibility() == Visibility::GONE) continue;
      
      Rect childBounds = child->getBounds();
      contentSize_.width = std::max(contentSize_.width, 
                                   (uint16_t)(childBounds.right()));
      contentSize_.height = std::max(contentSize_.height, 
                                    (uint16_t)(childBounds.bottom()));
    }
  }
};

// ---- UIElement method that needs UIContainer ----

inline void UIElement::removeFromParent() {
  if (parent_) {
    parent_->removeChild(this);
  }
}

} // namespace UI
} // namespace SystemAPI
