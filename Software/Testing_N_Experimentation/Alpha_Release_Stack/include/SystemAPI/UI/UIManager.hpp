/*****************************************************************
 * @file UIManager.hpp
 * @brief UI Framework Manager - Scene & Element Management
 * 
 * The UIManager handles:
 * - Root element/scene management
 * - Focus navigation (tab order, directional)
 * - Global input routing
 * - Animation updates
 * - Frame rendering coordination
 * 
 * @author ARCOS
 * @version 1.0
 *****************************************************************/

#pragma once

#include "UICore.hpp"
#include "UIRenderer.hpp"
#include "UIContainer.hpp"
#include "UINotification.hpp"
#include "UIDialog.hpp"
#include <vector>
#include <functional>

namespace SystemAPI {
namespace UI {

/**
 * @brief Navigation mode for focus movement
 */
enum class NavMode : uint8_t {
  TAB_ORDER,      // Sequential tab order
  DIRECTIONAL,   // D-pad style (up/down/left/right)
  BOTH           // Support both
};

/**
 * @brief Input source type
 */
enum class InputSource : uint8_t {
  ENCODER,       // Rotary encoder
  BUTTONS,       // Physical buttons
  TOUCH,         // Touch screen
  DPAD,          // Directional pad
  KEYBOARD       // USB keyboard
};

/**
 * @brief Scene - a logical page/screen of UI
 */
class UIScene {
public:
  explicit UIScene(const char* name = "scene") {
    strncpy(name_, name, sizeof(name_) - 1);
    root_.setSize(128, 128);  // Default OLED size
  }
  
  const char* getName() const { return name_; }
  UIContainer& getRoot() { return root_; }
  const UIContainer& getRoot() const { return root_; }
  
  // Lifecycle callbacks
  void setOnEnter(std::function<void()> callback) { onEnter_ = callback; }
  void setOnExit(std::function<void()> callback) { onExit_ = callback; }
  void setOnUpdate(std::function<void(float)> callback) { onUpdate_ = callback; }
  
  void enter() { if (onEnter_) onEnter_(); }
  void exit() { if (onExit_) onExit_(); }
  void update(float dt) { 
    root_.update(dt);
    if (onUpdate_) onUpdate_(dt); 
  }
  
  void render(UIRenderer& renderer) {
    root_.render(renderer);
  }
  
  // Focus management within scene
  void setInitialFocus(UIElement* element) { initialFocus_ = element; }
  UIElement* getInitialFocus() const { return initialFocus_; }
  
private:
  char name_[32] = {};
  UIContainer root_;
  UIElement* initialFocus_ = nullptr;
  std::function<void()> onEnter_;
  std::function<void()> onExit_;
  std::function<void(float)> onUpdate_;
};

/**
 * @brief Scene transition animation type
 */
enum class TransitionType : uint8_t {
  NONE,
  FADE,
  SLIDE_LEFT,
  SLIDE_RIGHT,
  SLIDE_UP,
  SLIDE_DOWN,
  ZOOM_IN,
  ZOOM_OUT
};

/**
 * @brief UI Manager - Global UI management singleton
 * 
 * Initialize with ui.init(128, 128), create scenes, add elements,
 * then call ui.update() and ui.render() in your main loop.
 */
class UIManager {
public:
  static UIManager& instance() {
    static UIManager inst;
    return inst;
  }
  
  // ---- Initialization ----
  
  bool init(uint16_t width, uint16_t height, BufferFormat format = BufferFormat::MONO_1BPP) {
    screenWidth_ = width;
    screenHeight_ = height;
    
    if (!renderer_.init(width, height, format)) {
      return false;
    }
    
    initialized_ = true;
    return true;
  }
  
  bool isInitialized() const { return initialized_; }
  
  // ---- Scene Management ----
  
  UIScene* createScene(const char* name) {
    auto* scene = new UIScene(name);
    scene->getRoot().setSize(screenWidth_, screenHeight_);
    scenes_.push_back(scene);
    return scene;
  }
  
  void destroyScene(UIScene* scene) {
    for (auto it = scenes_.begin(); it != scenes_.end(); ++it) {
      if (*it == scene) {
        scenes_.erase(it);
        delete scene;
        return;
      }
    }
  }
  
  UIScene* getScene(const char* name) {
    for (auto* scene : scenes_) {
      if (strcmp(scene->getName(), name) == 0) {
        return scene;
      }
    }
    return nullptr;
  }
  
  void setScene(UIScene* scene, TransitionType transition = TransitionType::NONE) {
    if (currentScene_) {
      currentScene_->exit();
    }
    
    previousScene_ = currentScene_;
    currentScene_ = scene;
    transitionType_ = transition;
    
    if (transition != TransitionType::NONE) {
      transitioning_ = true;
      transitionProgress_ = 0.0f;
    }
    
    if (currentScene_) {
      currentScene_->enter();
      
      // Set initial focus
      if (currentScene_->getInitialFocus()) {
        setFocus(currentScene_->getInitialFocus());
      } else {
        // Find first focusable element
        autoFocus(&currentScene_->getRoot());
      }
    }
  }
  
  void pushScene(UIScene* scene, TransitionType transition = TransitionType::NONE) {
    if (currentScene_) {
      sceneStack_.push_back(currentScene_);
    }
    setScene(scene, transition);
  }
  
  void popScene(TransitionType transition = TransitionType::NONE) {
    if (!sceneStack_.empty()) {
      UIScene* prev = sceneStack_.back();
      sceneStack_.pop_back();
      setScene(prev, transition);
    }
  }
  
  UIScene* getCurrentScene() { return currentScene_; }
  
  // ---- Focus Management ----
  
  void setFocus(UIElement* element) {
    if (focusedElement_ == element) return;
    
    if (focusedElement_) {
      focusedElement_->setFocused(false);
    }
    
    focusedElement_ = element;
    
    if (focusedElement_) {
      focusedElement_->setFocused(true);
    }
  }
  
  UIElement* getFocus() { return focusedElement_; }
  
  void focusNext() {
    if (!currentScene_) return;
    
    std::vector<UIElement*> focusable;
    collectFocusable(&currentScene_->getRoot(), focusable);
    
    if (focusable.empty()) return;
    
    int currentIdx = -1;
    for (int i = 0; i < (int)focusable.size(); i++) {
      if (focusable[i] == focusedElement_) {
        currentIdx = i;
        break;
      }
    }
    
    int nextIdx = (currentIdx + 1) % focusable.size();
    setFocus(focusable[nextIdx]);
  }
  
  void focusPrevious() {
    if (!currentScene_) return;
    
    std::vector<UIElement*> focusable;
    collectFocusable(&currentScene_->getRoot(), focusable);
    
    if (focusable.empty()) return;
    
    int currentIdx = 0;
    for (int i = 0; i < (int)focusable.size(); i++) {
      if (focusable[i] == focusedElement_) {
        currentIdx = i;
        break;
      }
    }
    
    int prevIdx = (currentIdx - 1 + focusable.size()) % focusable.size();
    setFocus(focusable[prevIdx]);
  }
  
  void focusDirection(int16_t dx, int16_t dy) {
    if (!focusedElement_ || !currentScene_) return;
    
    std::vector<UIElement*> focusable;
    collectFocusable(&currentScene_->getRoot(), focusable);
    
    if (focusable.size() <= 1) return;
    
    // Get current focus center
    Rect currentBounds = focusedElement_->getBounds();
    Point currentCenter(
      currentBounds.x + currentBounds.width / 2,
      currentBounds.y + currentBounds.height / 2
    );
    
    UIElement* best = nullptr;
    int bestScore = INT32_MAX;
    
    for (auto* elem : focusable) {
      if (elem == focusedElement_) continue;
      
      Rect bounds = elem->getBounds();
      Point center(bounds.x + bounds.width / 2, bounds.y + bounds.height / 2);
      
      int ddx = center.x - currentCenter.x;
      int ddy = center.y - currentCenter.y;
      
      // Check if element is in the right direction
      bool valid = false;
      if (dx > 0 && ddx > 0) valid = true;
      if (dx < 0 && ddx < 0) valid = true;
      if (dy > 0 && ddy > 0) valid = true;
      if (dy < 0 && ddy < 0) valid = true;
      
      if (!valid) continue;
      
      // Score by distance
      int dist = abs(ddx) + abs(ddy);
      if (dist < bestScore) {
        bestScore = dist;
        best = elem;
      }
    }
    
    if (best) {
      setFocus(best);
    }
  }
  
  // ---- Input Handling ----
  
  void setNavMode(NavMode mode) { navMode_ = mode; }
  NavMode getNavMode() const { return navMode_; }
  
  /**
   * @brief Process input event
   * @return true if event was consumed
   */
  bool handleInput(const InputEvent& event) {
    // Dialog has priority
    if (!activeDialogs_.empty()) {
      auto* dialog = activeDialogs_.back();
      if (dialog->handleInput(event)) {
        return true;
      }
    }
    
    // Then focused element
    if (focusedElement_ && focusedElement_->handleInput(event)) {
      return true;
    }
    
    // Handle navigation
    if (event.type == EventType::KEY_PRESS) {
      switch (event.key) {
        case KeyCode::TAB:
          if (event.modifiers & (uint8_t)Modifiers::SHIFT) {
            focusPrevious();
          } else {
            focusNext();
          }
          return true;
          
        case KeyCode::UP:
          if (navMode_ != NavMode::TAB_ORDER) {
            focusDirection(0, -1);
            return true;
          }
          break;
          
        case KeyCode::DOWN:
          if (navMode_ != NavMode::TAB_ORDER) {
            focusDirection(0, 1);
            return true;
          }
          break;
          
        case KeyCode::LEFT:
          if (navMode_ != NavMode::TAB_ORDER) {
            focusDirection(-1, 0);
            return true;
          }
          break;
          
        case KeyCode::RIGHT:
          if (navMode_ != NavMode::TAB_ORDER) {
            focusDirection(1, 0);
            return true;
          }
          break;
          
        default:
          break;
      }
    }
    
    return false;
  }
  
  // Convenience input methods
  void pressKey(KeyCode key) {
    InputEvent event;
    event.type = EventType::KEY_PRESS;
    event.key = key;
    handleInput(event);
  }
  
  void releaseKey(KeyCode key) {
    InputEvent event;
    event.type = EventType::KEY_RELEASE;
    event.key = key;
    handleInput(event);
  }
  
  void encoderRotate(int8_t delta) {
    InputEvent event;
    event.type = EventType::ENCODER_ROTATE;
    event.encoderDelta = delta;
    handleInput(event);
  }
  
  void encoderPress() {
    pressKey(KeyCode::ENTER);
  }
  
  void touch(int16_t x, int16_t y) {
    InputEvent event;
    event.type = EventType::TOUCH_START;
    event.touchX = x;
    event.touchY = y;
    handleInput(event);
  }
  
  // ---- Dialog Management ----
  
  void showDialog(UIDialog* dialog) {
    activeDialogs_.push_back(dialog);
    dialog->show();
    
    // Store previous focus
    dialogPreviousFocus_ = focusedElement_;
    
    // Focus dialog
    setFocus(dialog);
  }
  
  void hideDialog(UIDialog* dialog) {
    for (auto it = activeDialogs_.begin(); it != activeDialogs_.end(); ++it) {
      if (*it == dialog) {
        activeDialogs_.erase(it);
        dialog->hide();
        
        // Restore focus
        if (dialogPreviousFocus_) {
          setFocus(dialogPreviousFocus_);
          dialogPreviousFocus_ = nullptr;
        }
        return;
      }
    }
  }
  
  // ---- Notification Integration ----
  
  NotificationManager& notifications() {
    return NotificationManager::instance();
  }
  
  void showToast(const char* message, NotificationType type = NotificationType::INFO) {
    notifications().show(message, "", type, 3000);
  }
  
  // ---- Update & Render ----
  
  void update(float dt) {
    // Update transition
    if (transitioning_) {
      transitionProgress_ += dt / transitionDuration_;
      if (transitionProgress_ >= 1.0f) {
        transitionProgress_ = 1.0f;
        transitioning_ = false;
        previousScene_ = nullptr;
      }
    }
    
    // Update current scene
    if (currentScene_) {
      currentScene_->update(dt);
    }
    
    // Update dialogs
    for (auto* dialog : activeDialogs_) {
      dialog->update(dt);
    }
    
    // Update notifications
    notifications().update(dt);
    
    frameTime_ = dt;
    totalTime_ += dt;
  }
  
  void render() {
    renderer_.beginFrame();
    renderer_.clear();
    
    // Render transition
    if (transitioning_ && previousScene_) {
      renderTransition();
    } else if (currentScene_) {
      currentScene_->render(renderer_);
    }
    
    // Render dialogs
    for (auto* dialog : activeDialogs_) {
      dialog->render(renderer_);
    }
    
    // Render notifications
    notifications().render(renderer_);
    
    // Render debug overlay
    if (showDebugOverlay_) {
      renderDebugOverlay();
    }
    
    renderer_.endFrame();
  }
  
  // ---- Buffer Access ----
  
  UIRenderer& getRenderer() { return renderer_; }
  uint8_t* getBuffer() { return renderer_.getBuffer(); }
  const uint8_t* getBuffer() const { return renderer_.getBuffer(); }
  size_t getBufferSize() const { return renderer_.getBufferSize(); }
  
  // ---- Debug ----
  
  void setDebugOverlay(bool show) { showDebugOverlay_ = show; }
  bool getDebugOverlay() const { return showDebugOverlay_; }
  
  float getFPS() const { return frameTime_ > 0 ? 1.0f / frameTime_ : 0; }
  float getTotalTime() const { return totalTime_; }
  
  // ---- Screen Properties ----
  
  uint16_t getScreenWidth() const { return screenWidth_; }
  uint16_t getScreenHeight() const { return screenHeight_; }
  
private:
  UIManager() = default;
  ~UIManager() {
    for (auto* scene : scenes_) {
      delete scene;
    }
  }
  
  // Non-copyable
  UIManager(const UIManager&) = delete;
  UIManager& operator=(const UIManager&) = delete;
  
  void autoFocus(UIContainer* container) {
    for (size_t i = 0; i < container->getChildCount(); i++) {
      auto* child = container->getChild(i);
      if (child->isFocusable() && child->isEnabled()) {
        setFocus(child);
        return;
      }
      // Check nested containers
      if (auto* childContainer = dynamic_cast<UIContainer*>(child)) {
        autoFocus(childContainer);
        if (focusedElement_) return;
      }
    }
  }
  
  void collectFocusable(UIElement* element, std::vector<UIElement*>& list) {
    if (!element->isVisible()) return;
    
    if (element->isFocusable() && element->isEnabled()) {
      list.push_back(element);
    }
    
    if (auto* container = dynamic_cast<UIContainer*>(element)) {
      for (size_t i = 0; i < container->getChildCount(); i++) {
        collectFocusable(container->getChild(i), list);
      }
    }
  }
  
  void renderTransition() {
    float t = transitionProgress_;
    
    switch (transitionType_) {
      case TransitionType::FADE: {
        // Draw current with fade
        if (currentScene_) {
          currentScene_->render(renderer_);
        }
        break;
      }
      
      case TransitionType::SLIDE_LEFT: {
        int16_t offset = (int16_t)((1.0f - t) * screenWidth_);
        if (previousScene_) {
          renderer_.pushTranslation(-screenWidth_ + offset, 0);
          previousScene_->render(renderer_);
          renderer_.popTranslation();
        }
        if (currentScene_) {
          renderer_.pushTranslation(offset, 0);
          currentScene_->render(renderer_);
          renderer_.popTranslation();
        }
        break;
      }
      
      case TransitionType::SLIDE_RIGHT: {
        int16_t offset = (int16_t)((1.0f - t) * screenWidth_);
        if (previousScene_) {
          renderer_.pushTranslation(screenWidth_ - offset, 0);
          previousScene_->render(renderer_);
          renderer_.popTranslation();
        }
        if (currentScene_) {
          renderer_.pushTranslation(-offset, 0);
          currentScene_->render(renderer_);
          renderer_.popTranslation();
        }
        break;
      }
      
      case TransitionType::SLIDE_UP: {
        int16_t offset = (int16_t)((1.0f - t) * screenHeight_);
        if (previousScene_) {
          renderer_.pushTranslation(0, -screenHeight_ + offset);
          previousScene_->render(renderer_);
          renderer_.popTranslation();
        }
        if (currentScene_) {
          renderer_.pushTranslation(0, offset);
          currentScene_->render(renderer_);
          renderer_.popTranslation();
        }
        break;
      }
      
      case TransitionType::SLIDE_DOWN: {
        int16_t offset = (int16_t)((1.0f - t) * screenHeight_);
        if (previousScene_) {
          renderer_.pushTranslation(0, screenHeight_ - offset);
          previousScene_->render(renderer_);
          renderer_.popTranslation();
        }
        if (currentScene_) {
          renderer_.pushTranslation(0, -offset);
          currentScene_->render(renderer_);
          renderer_.popTranslation();
        }
        break;
      }
      
      default:
        if (currentScene_) {
          currentScene_->render(renderer_);
        }
        break;
    }
  }
  
  void renderDebugOverlay() {
    // FPS counter
    char fps[16];
    snprintf(fps, sizeof(fps), "%.1f FPS", getFPS());
    renderer_.drawText(2, screenHeight_ - 10, fps, Colors::Yellow, FontSize::TINY);
    
    // Memory info (placeholder)
    renderer_.drawText(60, screenHeight_ - 10, "MEM:?", Colors::Yellow, FontSize::TINY);
    
    // Focus indicator
    if (focusedElement_) {
      Rect bounds = focusedElement_->getBounds();
      renderer_.drawRect(bounds.inset(-1), Colors::Cyan);
    }
  }
  
  // State
  bool initialized_ = false;
  UIRenderer renderer_;
  uint16_t screenWidth_ = 128;
  uint16_t screenHeight_ = 128;
  
  // Scenes
  std::vector<UIScene*> scenes_;
  std::vector<UIScene*> sceneStack_;
  UIScene* currentScene_ = nullptr;
  UIScene* previousScene_ = nullptr;
  
  // Transitions
  bool transitioning_ = false;
  TransitionType transitionType_ = TransitionType::NONE;
  float transitionProgress_ = 0.0f;
  float transitionDuration_ = 0.3f;
  
  // Focus
  UIElement* focusedElement_ = nullptr;
  UIElement* dialogPreviousFocus_ = nullptr;
  NavMode navMode_ = NavMode::BOTH;
  
  // Dialogs
  std::vector<UIDialog*> activeDialogs_;
  
  // Debug
  bool showDebugOverlay_ = false;
  float frameTime_ = 0.016f;
  float totalTime_ = 0.0f;
};

// ============================================================
// Convenience Macros for Building UIs
// ============================================================

/**
 * @brief Quick UI builder macros for creating scenes and adding elements
 */

#define UI_MANAGER UIManager::instance()
#define UI_SCENE(name) UIManager::instance().createScene(name)
#define UI_SHOW(scene) UIManager::instance().pushScene(scene)

// Helper to add elements
template<typename T, typename... Args>
T& ui_add(UIContainer* parent, Args&&... args) {
  auto* elem = new T(std::forward<Args>(args)...);
  parent->addChild(elem);
  return *elem;
}

#define UI_ADD(parent, Type, ...) ui_add<Type>(&(parent)->getRoot(), ##__VA_ARGS__)
#define UI_ADD_TO(container, Type, ...) ui_add<Type>(container, ##__VA_ARGS__)

// ============================================================
// Quick UI Builder - Fluent Interface
// ============================================================

/**
 * @brief Fluent UI builder for creating UIs quickly with chained method calls
 */
class UIBuilder {
public:
  explicit UIBuilder(UIScene* scene) : scene_(scene) {
    containerStack_.push_back(&scene->getRoot());
  }
  
  // Container operations
  UIBuilder& container(int16_t x, int16_t y, uint16_t w, uint16_t h) {
    auto* c = new UIContainer();
    c->setPosition(x, y);
    c->setSize(w, h);
    current()->addChild(c);
    containerStack_.push_back(c);
    return *this;
  }
  
  UIBuilder& end() {
    if (containerStack_.size() > 1) {
      containerStack_.pop_back();
    }
    return *this;
  }
  
  // Styling
  UIBuilder& style(const UIStyle& s) {
    if (!containerStack_.empty()) {
      containerStack_.back()->setStyle(s);
    }
    return *this;
  }
  
  // Elements
  UIBuilder& text(const char* str, int16_t x, int16_t y) {
    auto* t = new UIText(str);
    t->setPosition(x, y);
    current()->addChild(t);
    lastElement_ = t;
    return *this;
  }
  
  UIBuilder& icon(IconType type, int16_t x, int16_t y, const Color& color = Colors::White) {
    auto* i = new UIIcon(type);
    i->setPosition(x, y);
    i->setColor(color);
    current()->addChild(i);
    lastElement_ = i;
    return *this;
  }
  
  UIBuilder& button(const char* label, int16_t x, int16_t y) {
    auto* b = new UIButton(label);
    b->setPosition(x, y);
    current()->addChild(b);
    lastElement_ = b;
    return *this;
  }
  
  UIBuilder& checkbox(const char* label, int16_t x, int16_t y, bool checked = false) {
    auto* c = new UICheckbox(label, checked);
    c->setPosition(x, y);
    current()->addChild(c);
    lastElement_ = c;
    return *this;
  }
  
  UIBuilder& slider(int16_t x, int16_t y, uint16_t w, int min, int max, int value) {
    auto* s = new UISlider(min, max, value);
    s->setPosition(x, y);
    s->setWidth(w);
    current()->addChild(s);
    lastElement_ = s;
    return *this;
  }
  
  UIBuilder& progress(int16_t x, int16_t y, uint16_t w, float value = 0) {
    auto* p = new UIProgressBar(value);
    p->setPosition(x, y);
    p->setSize(w, 8);
    current()->addChild(p);
    lastElement_ = p;
    return *this;
  }
  
  // Dividers
  UIBuilder& hline(int16_t y, const Color& color = Colors::DarkGray) {
    auto* c = new UIContainer();
    c->setPosition(0, y);
    c->setSize(current()->getWidth(), 1);
    c->setStyle(UIStyle().backgroundColor(color));
    current()->addChild(c);
    return *this;
  }
  
  UIBuilder& vline(int16_t x, const Color& color = Colors::DarkGray) {
    auto* c = new UIContainer();
    c->setPosition(x, 0);
    c->setSize(1, current()->getHeight());
    c->setStyle(UIStyle().backgroundColor(color));
    current()->addChild(c);
    return *this;
  }
  
  // Spacing
  UIBuilder& spacer(uint16_t height) {
    cursorY_ += height;
    return *this;
  }
  
  // Event handlers (applied to last element)
  template<typename F>
  UIBuilder& onClick(F&& callback) {
    if (auto* btn = dynamic_cast<UIButton*>(lastElement_)) {
      btn->onClick(std::forward<F>(callback));
    }
    return *this;
  }
  
  template<typename F>
  UIBuilder& onChange(F&& callback) {
    if (auto* cb = dynamic_cast<UICheckbox*>(lastElement_)) {
      cb->onChange(std::forward<F>(callback));
    } else if (auto* sl = dynamic_cast<UISlider*>(lastElement_)) {
      sl->onChange(std::forward<F>(callback));
    }
    return *this;
  }
  
  // Get last created element
  UIElement* last() { return lastElement_; }
  
  template<typename T>
  T* lastAs() { return dynamic_cast<T*>(lastElement_); }
  
private:
  UIContainer* current() { return containerStack_.back(); }
  
  UIScene* scene_;
  std::vector<UIContainer*> containerStack_;
  UIElement* lastElement_ = nullptr;
  int16_t cursorY_ = 0;
};

} // namespace UI
} // namespace SystemAPI
