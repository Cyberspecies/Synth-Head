/*****************************************************************
 * @file UIElement.hpp
 * @brief UI Framework Element - Base class for all UI elements
 * 
 * UIElement is the base class for all UI components, similar to
 * HTMLElement in web development. It provides:
 * - Positioning and sizing
 * - Style management
 * - Event handling
 * - Parent/child relationships
 * - Focus management
 * - Visibility control
 * 
 * @author ARCOS
 * @version 1.0
 *****************************************************************/

#pragma once

#include "UICore.hpp"
#include "UIStyle.hpp"

namespace SystemAPI {
namespace UI {

// Forward declarations
class UIContainer;
class UIRenderer;

// ============================================================
// UIElement Base Class
// ============================================================

/**
 * @brief Base class for all UI elements
 * 
 * Like an HTML element, this provides the foundation for all UI components.
 * 
 * @example
 * ```cpp
 * // Elements have position, size, and style
 * element->setPosition(10, 20);
 * element->setSize(100, 30);
 * element->setStyle(Styles::buttonPrimary());
 * 
 * // Visibility control
 * element->setVisible(true);
 * element->hide();
 * element->show();
 * 
 * // Focus
 * element->setFocusable(true);
 * element->focus();
 * 
 * // Event callbacks
 * element->onPress([](UIElement* e) {
 *   printf("Pressed!\n");
 * });
 * ```
 */
class UIElement {
public:
  // ---- Constructors ----
  
  UIElement() : id_(generateElementID()) {}
  virtual ~UIElement() = default;
  
  // ---- Identity ----
  
  ElementID getID() const { return id_; }
  
  void setTag(const char* tag) { strncpy(tag_, tag, sizeof(tag_) - 1); }
  const char* getTag() const { return tag_; }
  
  // ---- Type Info ----
  
  /**
   * @brief Get element type name (for debugging)
   */
  virtual const char* getTypeName() const { return "UIElement"; }
  
  /**
   * @brief Check if element is a container
   */
  virtual bool isContainer() const { return false; }
  
  /**
   * @brief Check if element is interactive
   */
  virtual bool isInteractive() const { return focusable_; }
  
  // ---- Geometry ----
  
  void setPosition(int16_t x, int16_t y) { 
    bounds_.x = x; 
    bounds_.y = y; 
    markDirty(); 
  }
  
  void setPosition(const Point& pos) { setPosition(pos.x, pos.y); }
  
  void setSize(uint16_t w, uint16_t h) { 
    bounds_.width = w; 
    bounds_.height = h; 
    markDirty(); 
  }
  
  void setSize(const Size& size) { setSize(size.width, size.height); }
  
  void setBounds(const Rect& r) { 
    bounds_ = r; 
    markDirty(); 
  }
  
  void setBounds(int16_t x, int16_t y, uint16_t w, uint16_t h) {
    bounds_ = Rect(x, y, w, h);
    markDirty();
  }
  
  const Rect& getBounds() const { return bounds_; }
  Point getPosition() const { return bounds_.position(); }
  Size getSize() const { return bounds_.size(); }
  
  int16_t getX() const { return bounds_.x; }
  int16_t getY() const { return bounds_.y; }
  uint16_t getWidth() const { return bounds_.width; }
  uint16_t getHeight() const { return bounds_.height; }
  
  /**
   * @brief Get bounds in screen coordinates
   */
  Rect getScreenBounds() const {
    if (parent_) {
      Point parentPos = parent_->getScreenPosition();
      Rect parentContent = style_.contentRect(parent_->getBounds());
      return Rect(
        parentContent.x + bounds_.x,
        parentContent.y + bounds_.y,
        bounds_.width,
        bounds_.height
      );
    }
    return bounds_;
  }
  
  /**
   * @brief Get position in screen coordinates
   */
  Point getScreenPosition() const {
    Rect screen = getScreenBounds();
    return screen.position();
  }
  
  // ---- Style ----
  
  void setStyle(const UIStyle& style) { 
    style_ = style; 
    markDirty(); 
  }
  
  UIStyle& getStyle() { return style_; }
  const UIStyle& getStyle() const { return style_; }
  
  /**
   * @brief Get current style state based on element state
   */
  StyleState getStyleState() const {
    if (!enabled_) return StyleState::DISABLED;
    if (pressed_) return StyleState::PRESSED;
    if (focused_) return StyleState::FOCUSED;
    return StyleState::NORMAL;
  }
  
  // ---- Visibility ----
  
  void setVisibility(Visibility v) { 
    visibility_ = v; 
    markDirty(); 
  }
  
  Visibility getVisibility() const { return visibility_; }
  
  void setVisible(bool visible) { 
    setVisibility(visible ? Visibility::VISIBLE : Visibility::HIDDEN); 
  }
  
  bool isVisible() const { return visibility_ == Visibility::VISIBLE; }
  
  void show() { setVisible(true); }
  void hide() { setVisible(false); }
  
  // ---- Enabled State ----
  
  void setEnabled(bool e) { 
    enabled_ = e; 
    markDirty(); 
  }
  
  bool isEnabled() const { return enabled_; }
  
  void enable() { setEnabled(true); }
  void disable() { setEnabled(false); }
  
  // ---- Focus ----
  
  void setFocusable(bool f) { focusable_ = f; }
  bool isFocusable() const { return focusable_ && enabled_ && isVisible(); }
  
  bool isFocused() const { return focused_; }
  
  /**
   * @brief Request focus for this element
   */
  virtual void focus() {
    if (isFocusable() && !focused_) {
      focused_ = true;
      markDirty();
      if (onFocus_) onFocus_(this);
    }
  }
  
  /**
   * @brief Remove focus from this element
   */
  virtual void blur() {
    if (focused_) {
      focused_ = false;
      markDirty();
      if (onBlur_) onBlur_(this);
    }
  }
  
  // ---- Pressed State ----
  
  bool isPressed() const { return pressed_; }
  
  void setPressed(bool p) { 
    if (pressed_ != p) {
      pressed_ = p;
      markDirty();
    }
  }
  
  // ---- Parent/Child Hierarchy ----
  
  UIContainer* getParent() const { return parent_; }
  void setParent(UIContainer* p) { parent_ = p; }
  
  /**
   * @brief Remove this element from its parent
   */
  void removeFromParent();  // Implemented after UIContainer is defined
  
  // ---- Dirty/Redraw Management ----
  
  bool isDirty() const { return dirty_; }
  void markDirty() { dirty_ = true; }
  void clearDirty() { dirty_ = false; }
  
  // ---- Event Callbacks ----
  
  void onPress(ElementCallback cb) { onPress_ = cb; }
  void onRelease(ElementCallback cb) { onRelease_ = cb; }
  void onClick(ElementCallback cb) { onClick_ = cb; }
  void onLongPress(ElementCallback cb) { onLongPress_ = cb; }
  void onFocus(ElementCallback cb) { onFocus_ = cb; }
  void onBlur(ElementCallback cb) { onBlur_ = cb; }
  void onChange(ElementCallback cb) { onChange_ = cb; }
  
  // ---- Event Handling ----
  
  /**
   * @brief Handle input event
   * @return true if event was consumed
   */
  virtual bool handleInput(InputEvent& event) {
    if (!isEnabled() || !isVisible()) return false;
    
    if (event.type == InputEvent::BUTTON) {
      if (event.btn.button == Button::SELECT) {
        if (event.btn.event == ButtonEvent::PRESSED) {
          setPressed(true);
          if (onPress_) onPress_(this);
          event.consumed = true;
          return true;
        } else if (event.btn.event == ButtonEvent::RELEASED) {
          setPressed(false);
          if (onRelease_) onRelease_(this);
          if (onClick_) onClick_(this);
          event.consumed = true;
          return true;
        } else if (event.btn.event == ButtonEvent::LONG_PRESS) {
          if (onLongPress_) onLongPress_(this);
          event.consumed = true;
          return true;
        }
      }
    } else if (event.type == InputEvent::TOUCH) {
      Rect screenBounds = getScreenBounds();
      bool inside = screenBounds.contains(event.touch.x, event.touch.y);
      
      if (inside) {
        if (event.touch.event == TouchEvent::DOWN) {
          setPressed(true);
          if (onPress_) onPress_(this);
          event.consumed = true;
          return true;
        } else if (event.touch.event == TouchEvent::UP) {
          if (pressed_) {
            setPressed(false);
            if (onRelease_) onRelease_(this);
            if (onClick_) onClick_(this);
          }
          event.consumed = true;
          return true;
        }
      } else if (pressed_ && event.touch.event == TouchEvent::UP) {
        setPressed(false);
        if (onRelease_) onRelease_(this);
      }
    }
    
    return false;
  }
  
  // ---- Lifecycle ----
  
  /**
   * @brief Called every frame to update element state
   */
  virtual void update(uint32_t deltaMs) {
    (void)deltaMs;  // Base implementation does nothing
  }
  
  /**
   * @brief Render the element
   */
  virtual void render(UIRenderer& renderer) = 0;
  
  // ---- Layout ----
  
  /**
   * @brief Calculate preferred size based on content
   */
  virtual Size getPreferredSize() const {
    return Size(
      style_.getMinWidth(),
      style_.getMinHeight()
    );
  }
  
  /**
   * @brief Perform layout (called by parent container)
   */
  virtual void layout() {
    // Base implementation does nothing
  }
  
  // ---- Hit Testing ----
  
  /**
   * @brief Test if point is inside element
   */
  virtual bool hitTest(int16_t x, int16_t y) const {
    return getScreenBounds().contains(x, y);
  }
  
  /**
   * @brief Find deepest element at point
   */
  virtual UIElement* elementAt(int16_t x, int16_t y) {
    if (!isVisible()) return nullptr;
    return hitTest(x, y) ? this : nullptr;
  }
  
protected:
  // Identity
  ElementID id_;
  char tag_[16] = "";
  
  // Geometry
  Rect bounds_;
  
  // Styling
  UIStyle style_;
  
  // State
  Visibility visibility_ = Visibility::VISIBLE;
  bool enabled_ = true;
  bool focusable_ = false;
  bool focused_ = false;
  bool pressed_ = false;
  bool dirty_ = true;
  
  // Hierarchy
  UIContainer* parent_ = nullptr;
  
  // Callbacks
  ElementCallback onPress_;
  ElementCallback onRelease_;
  ElementCallback onClick_;
  ElementCallback onLongPress_;
  ElementCallback onFocus_;
  ElementCallback onBlur_;
  ElementCallback onChange_;
};

} // namespace UI
} // namespace SystemAPI
