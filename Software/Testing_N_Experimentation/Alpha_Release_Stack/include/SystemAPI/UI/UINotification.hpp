/*****************************************************************
 * @file UINotification.hpp
 * @brief UI Framework Notification - Toast and notification system
 * 
 * @author ARCOS
 * @version 1.0
 *****************************************************************/

#pragma once

#include "UIElement.hpp"
#include "UIIcon.hpp"
#include <vector>
#include <queue>

namespace SystemAPI {
namespace UI {

/**
 * @brief Notification type (affects styling)
 */
enum class NotificationType : uint8_t {
  INFO,
  SUCCESS,
  WARNING,
  ERROR
};

/**
 * @brief Notification position on screen
 */
enum class NotificationPosition : uint8_t {
  TOP,
  TOP_LEFT,
  TOP_RIGHT,
  BOTTOM,
  BOTTOM_LEFT,
  BOTTOM_RIGHT,
  CENTER
};

/**
 * @brief Single notification/toast
 */
class UINotification : public UIElement {
public:
  UINotification() {
    style_ = Styles::notification();
  }
  
  UINotification(const char* message, NotificationType type = NotificationType::INFO)
    : UINotification() {
    setMessage(message);
    setType(type);
  }
  
  const char* getTypeName() const override { return "UINotification"; }
  
  // ---- Content ----
  
  void setMessage(const char* msg) {
    if (msg) {
      strncpy(message_, msg, sizeof(message_) - 1);
    }
    markDirty();
  }
  
  const char* getMessage() const { return message_; }
  
  void setTitle(const char* title) {
    if (title) {
      strncpy(title_, title, sizeof(title_) - 1);
    }
    markDirty();
  }
  
  const char* getTitle() const { return title_; }
  
  void setType(NotificationType type) { 
    type_ = type;
    // Update style based on type
    switch (type) {
      case NotificationType::SUCCESS:
        style_.borderColor(Colors::Success);
        icon_ = IconType::SUCCESS;
        break;
      case NotificationType::WARNING:
        style_.borderColor(Colors::Warning);
        icon_ = IconType::WARNING;
        break;
      case NotificationType::ERROR:
        style_.borderColor(Colors::Danger);
        icon_ = IconType::ERROR;
        break;
      default:
        style_.borderColor(Colors::Primary);
        icon_ = IconType::INFO;
        break;
    }
    markDirty();
  }
  
  NotificationType getType() const { return type_; }
  
  void setIcon(IconType icon) { icon_ = icon; markDirty(); }
  IconType getIcon() const { return icon_; }
  
  // ---- Timing ----
  
  void setDuration(uint32_t ms) { duration_ = ms; }
  uint32_t getDuration() const { return duration_; }
  
  void setAutoHide(bool autoHide) { autoHide_ = autoHide; }
  bool getAutoHide() const { return autoHide_; }
  
  bool isExpired() const { return autoHide_ && elapsed_ >= duration_; }
  
  // ---- Animation ----
  
  void show() {
    setVisible(true);
    elapsed_ = 0;
    animProgress_ = 0.0f;
    animState_ = AnimState::ENTER;
  }
  
  void dismiss() {
    animState_ = AnimState::EXIT;
  }
  
  float getAnimProgress() const { return animProgress_; }
  
  void update(uint32_t deltaMs) override {
    UIElement::update(deltaMs);
    
    // Animation
    switch (animState_) {
      case AnimState::ENTER:
        animProgress_ += deltaMs * 0.005f;
        if (animProgress_ >= 1.0f) {
          animProgress_ = 1.0f;
          animState_ = AnimState::VISIBLE;
        }
        markDirty();
        break;
        
      case AnimState::VISIBLE:
        if (autoHide_) {
          elapsed_ += deltaMs;
          if (elapsed_ >= duration_) {
            animState_ = AnimState::EXIT;
          }
        }
        break;
        
      case AnimState::EXIT:
        animProgress_ -= deltaMs * 0.005f;
        if (animProgress_ <= 0.0f) {
          animProgress_ = 0.0f;
          animState_ = AnimState::HIDDEN;
          setVisible(false);
        }
        markDirty();
        break;
        
      default:
        break;
    }
  }
  
  // ---- Layout ----
  
  Size getPreferredSize() const override {
    FontInfo font = getFontInfo(style_.getFontSize());
    uint16_t textW = std::max(
      title_[0] ? textWidth(title_, FontSize::MEDIUM) : 0,
      textWidth(message_, style_.getFontSize())
    );
    uint16_t iconW = (icon_ != IconType::NONE) ? 16 : 0;
    
    return Size(
      std::max(style_.getMinWidth(), (uint16_t)(textW + iconW + style_.horizontalSpace())),
      std::max(style_.getMinHeight(), (uint16_t)(
        (title_[0] ? 12 : 0) + font.charHeight + style_.verticalSpace()
      ))
    );
  }
  
  // ---- Rendering ----
  
  void render(UIRenderer& renderer) override;  // Implemented in UIRenderer.hpp
  
protected:
  char message_[64] = "";
  char title_[32] = "";
  NotificationType type_ = NotificationType::INFO;
  IconType icon_ = IconType::INFO;
  
  uint32_t duration_ = 3000;  // 3 seconds
  bool autoHide_ = true;
  uint32_t elapsed_ = 0;
  
  enum class AnimState { HIDDEN, ENTER, VISIBLE, EXIT } animState_ = AnimState::HIDDEN;
  float animProgress_ = 0.0f;
};

/**
 * @brief Notification manager - handles multiple notifications
 * 
 * @example
 * ```cpp
 * auto& notifs = NotificationManager::instance();
 * notifs.setPosition(NotificationPosition::TOP_RIGHT);
 * 
 * // Show notifications
 * notifs.info("Download complete");
 * notifs.success("File saved successfully");
 * notifs.warning("Low battery");
 * notifs.error("Connection failed");
 * 
 * // Custom notification
 * notifs.show("Custom", NotificationType::INFO, 5000);
 * ```
 */
class NotificationManager {
public:
  static NotificationManager& instance() {
    static NotificationManager inst;
    return inst;
  }
  
  // ---- Configuration ----
  
  void setPosition(NotificationPosition pos) { position_ = pos; }
  NotificationPosition getPosition() const { return position_; }
  
  void setMaxVisible(int max) { maxVisible_ = max; }
  int getMaxVisible() const { return maxVisible_; }
  
  void setSpacing(uint8_t spacing) { spacing_ = spacing; }
  uint8_t getSpacing() const { return spacing_; }
  
  void setScreenSize(uint16_t w, uint16_t h) { screenW_ = w; screenH_ = h; }
  
  // ---- Show Notifications ----
  
  UINotification* show(const char* message, NotificationType type = NotificationType::INFO, 
                       uint32_t duration = 3000) {
    auto* notif = new UINotification(message, type);
    notif->setDuration(duration);
    notif->show();
    
    // Add to queue
    if ((int)active_.size() >= maxVisible_) {
      // Remove oldest
      delete active_.front();
      active_.erase(active_.begin());
    }
    active_.push_back(notif);
    
    layoutNotifications();
    return notif;
  }
  
  UINotification* info(const char* message, uint32_t duration = 3000) {
    return show(message, NotificationType::INFO, duration);
  }
  
  UINotification* success(const char* message, uint32_t duration = 3000) {
    return show(message, NotificationType::SUCCESS, duration);
  }
  
  UINotification* warning(const char* message, uint32_t duration = 3000) {
    return show(message, NotificationType::WARNING, duration);
  }
  
  UINotification* error(const char* message, uint32_t duration = 5000) {
    return show(message, NotificationType::ERROR, duration);
  }
  
  // ---- Dismiss ----
  
  void dismissAll() {
    for (auto* notif : active_) {
      notif->dismiss();
    }
  }
  
  // ---- Update ----
  
  void update(uint32_t deltaMs) {
    for (int i = active_.size() - 1; i >= 0; i--) {
      active_[i]->update(deltaMs);
      
      if (!active_[i]->isVisible()) {
        delete active_[i];
        active_.erase(active_.begin() + i);
        layoutNotifications();
      }
    }
  }
  
  // ---- Render ----
  
  void render(UIRenderer& renderer) {
    for (auto* notif : active_) {
      if (notif->isVisible()) {
        notif->render(renderer);
      }
    }
  }
  
  // ---- Access ----
  
  const std::vector<UINotification*>& getActive() const { return active_; }
  int getActiveCount() const { return active_.size(); }
  
private:
  NotificationManager() = default;
  
  void layoutNotifications() {
    int16_t y;
    int16_t x;
    
    // Calculate starting position based on position_
    switch (position_) {
      case NotificationPosition::TOP:
      case NotificationPosition::TOP_LEFT:
      case NotificationPosition::TOP_RIGHT:
        y = 4;
        break;
      case NotificationPosition::CENTER:
        y = screenH_ / 2;
        break;
      default:
        y = screenH_ - 4;
        break;
    }
    
    for (auto* notif : active_) {
      Size size = notif->getPreferredSize();
      notif->setSize(size.width, size.height);
      
      // Calculate X position
      switch (position_) {
        case NotificationPosition::TOP_LEFT:
        case NotificationPosition::BOTTOM_LEFT:
          x = 4;
          break;
        case NotificationPosition::TOP_RIGHT:
        case NotificationPosition::BOTTOM_RIGHT:
          x = screenW_ - size.width - 4;
          break;
        default:
          x = (screenW_ - size.width) / 2;
          break;
      }
      
      notif->setPosition(x, y);
      
      // Move to next position
      if (position_ == NotificationPosition::BOTTOM ||
          position_ == NotificationPosition::BOTTOM_LEFT ||
          position_ == NotificationPosition::BOTTOM_RIGHT) {
        y -= size.height + spacing_;
      } else {
        y += size.height + spacing_;
      }
    }
  }
  
  std::vector<UINotification*> active_;
  NotificationPosition position_ = NotificationPosition::TOP;
  int maxVisible_ = 3;
  uint8_t spacing_ = 4;
  uint16_t screenW_ = 128;
  uint16_t screenH_ = 128;
};

} // namespace UI
} // namespace SystemAPI
