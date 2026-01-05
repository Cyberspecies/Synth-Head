/*****************************************************************
 * @file UIMenu.hpp
 * @brief UI Framework Menu - Menu trees and navigation
 * 
 * Full menu system with hierarchical items, submenus, and actions.
 * 
 * @author ARCOS
 * @version 1.0
 *****************************************************************/

#pragma once

#include "UIElement.hpp"
#include "UIContainer.hpp"
#include "UIIcon.hpp"
#include <vector>
#include <memory>
#include <functional>

namespace SystemAPI {
namespace UI {

// Forward declarations
class UIMenu;
class UIMenuItem;

// ============================================================
// Menu Item Types
// ============================================================

/**
 * @brief Type of menu item
 */
enum class MenuItemType : uint8_t {
  ACTION,     // Executes callback
  SUBMENU,    // Opens submenu
  CHECKBOX,   // Toggle item
  RADIO,      // Radio button (single selection in group)
  SEPARATOR,  // Visual separator
  HEADER      // Non-selectable header text
};

/**
 * @brief Menu item definition
 */
struct MenuItemDef {
  const char* label;
  MenuItemType type = MenuItemType::ACTION;
  IconType icon = IconType::NONE;
  std::function<void()> action;
  UIMenu* submenu = nullptr;
  bool checked = false;
  int radioGroup = 0;
  bool enabled = true;
};

// ============================================================
// UIMenuItem Class
// ============================================================

/**
 * @brief Individual menu item
 */
class UIMenuItem : public UIElement {
public:
  UIMenuItem() {
    focusable_ = true;
    style_ = Styles::menuItem();
  }
  
  UIMenuItem(const char* label, MenuItemType type = MenuItemType::ACTION)
    : UIMenuItem() {
    setLabel(label);
    type_ = type;
    if (type == MenuItemType::SEPARATOR || type == MenuItemType::HEADER) {
      focusable_ = false;
    }
  }
  
  const char* getTypeName() const override { return "UIMenuItem"; }
  bool isInteractive() const override { return type_ != MenuItemType::SEPARATOR && type_ != MenuItemType::HEADER; }
  
  // ---- Properties ----
  
  void setLabel(const char* label) {
    if (label) {
      strncpy(label_, label, sizeof(label_) - 1);
    }
    markDirty();
  }
  
  const char* getLabel() const { return label_; }
  
  void setIcon(IconType icon) { icon_ = icon; markDirty(); }
  IconType getIcon() const { return icon_; }
  
  void setType(MenuItemType type) { type_ = type; markDirty(); }
  MenuItemType getType() const { return type_; }
  
  void setShortcut(const char* shortcut) {
    if (shortcut) {
      strncpy(shortcut_, shortcut, sizeof(shortcut_) - 1);
    }
    markDirty();
  }
  
  const char* getShortcut() const { return shortcut_; }
  
  // ---- Checked State (for checkbox/radio) ----
  
  void setChecked(bool checked) { 
    if (checked_ != checked) {
      checked_ = checked; 
      markDirty();
      if (onChecked_) onChecked_(checked_);
    }
  }
  bool isChecked() const { return checked_; }
  
  void setRadioGroup(int group) { radioGroup_ = group; }
  int getRadioGroup() const { return radioGroup_; }
  
  // ---- Submenu ----
  
  void setSubmenu(UIMenu* submenu) { submenu_ = submenu; }
  UIMenu* getSubmenu() const { return submenu_; }
  bool hasSubmenu() const { return submenu_ != nullptr; }
  
  // ---- Callbacks ----
  
  void setAction(std::function<void()> action) { action_ = action; }
  void onCheckedChange(ValueCallback<bool> cb) { onChecked_ = cb; }
  
  // ---- Execute ----
  
  void execute() {
    if (!isEnabled()) return;
    
    switch (type_) {
      case MenuItemType::ACTION:
        if (action_) action_();
        break;
        
      case MenuItemType::CHECKBOX:
        setChecked(!checked_);
        if (action_) action_();
        break;
        
      case MenuItemType::RADIO:
        setChecked(true);
        if (action_) action_();
        break;
        
      default:
        break;
    }
  }
  
  // ---- Layout ----
  
  Size getPreferredSize() const override {
    if (type_ == MenuItemType::SEPARATOR) {
      return Size(bounds_.width, 5);
    }
    
    FontInfo font = getFontInfo(style_.getFontSize());
    uint16_t textW = textWidth(label_, style_.getFontSize());
    uint16_t shortcutW = shortcut_[0] ? textWidth(shortcut_, style_.getFontSize()) + 10 : 0;
    uint16_t iconW = (icon_ != IconType::NONE) ? 12 : 0;
    uint16_t checkW = (type_ == MenuItemType::CHECKBOX || type_ == MenuItemType::RADIO) ? 14 : 0;
    uint16_t arrowW = (submenu_ != nullptr) ? 10 : 0;
    
    return Size(
      iconW + checkW + textW + shortcutW + arrowW + style_.horizontalSpace() + 16,
      font.charHeight + style_.verticalSpace()
    );
  }
  
  // ---- Rendering ----
  
  void render(UIRenderer& renderer) override;  // Implemented in UIRenderer.hpp
  
protected:
  char label_[32] = "";
  char shortcut_[16] = "";
  IconType icon_ = IconType::NONE;
  MenuItemType type_ = MenuItemType::ACTION;
  
  bool checked_ = false;
  int radioGroup_ = 0;
  
  UIMenu* submenu_ = nullptr;
  
  std::function<void()> action_;
  ValueCallback<bool> onChecked_;
};

// ============================================================
// UIMenu Class
// ============================================================

/**
 * @brief Menu container with items
 * 
 * @example
 * auto menu = new UIMenu("Main Menu");
 * 
 * // Add items
 * menu->addAction("New", IconType::FILE, []() {});
 * menu->addAction("Open", IconType::FOLDER, []() {});
 * menu->addSeparator();
 * menu->addCheckbox("Auto Save", true, [](bool on) {});
 * 
 * // Submenu
 * auto settingsMenu = new UIMenu("Settings");
 * settingsMenu->addAction("General", []() {});
 * settingsMenu->addAction("Display", []() {});
 * menu->addSubmenu("Settings", IconType::SETTINGS, settingsMenu);
 * 
 * // Show menu
 * menu->show();
 */
class UIMenu : public UIContainer {
public:
  UIMenu() {
    setLayoutMode(LayoutMode::FLEX);
    setFlexDirection(FlexDirection::COLUMN);
    style_ = Styles::card();
  }
  
  UIMenu(const char* title) : UIMenu() {
    setTitle(title);
  }
  
  const char* getTypeName() const override { return "UIMenu"; }
  
  // ---- Title ----
  
  void setTitle(const char* title) {
    if (title) {
      strncpy(title_, title, sizeof(title_) - 1);
    }
    markDirty();
  }
  
  const char* getTitle() const { return title_; }
  
  // ---- Add Items ----
  
  UIMenuItem* addAction(const char* label, std::function<void()> action) {
    auto* item = new UIMenuItem(label, MenuItemType::ACTION);
    item->setAction(action);
    addChild(item);
    items_.push_back(item);
    return item;
  }
  
  UIMenuItem* addAction(const char* label, IconType icon, std::function<void()> action) {
    auto* item = addAction(label, action);
    item->setIcon(icon);
    return item;
  }
  
  UIMenuItem* addSubmenu(const char* label, UIMenu* submenu) {
    auto* item = new UIMenuItem(label, MenuItemType::SUBMENU);
    item->setSubmenu(submenu);
    addChild(item);
    items_.push_back(item);
    submenu->setParentMenu(this);
    return item;
  }
  
  UIMenuItem* addSubmenu(const char* label, IconType icon, UIMenu* submenu) {
    auto* item = addSubmenu(label, submenu);
    item->setIcon(icon);
    return item;
  }
  
  UIMenuItem* addCheckbox(const char* label, bool checked, ValueCallback<bool> onToggle) {
    auto* item = new UIMenuItem(label, MenuItemType::CHECKBOX);
    item->setChecked(checked);
    item->onCheckedChange(onToggle);
    item->setAction([item, onToggle]() {
      if (onToggle) onToggle(item->isChecked());
    });
    addChild(item);
    items_.push_back(item);
    return item;
  }
  
  UIMenuItem* addRadio(const char* label, int group, bool checked, std::function<void()> action) {
    auto* item = new UIMenuItem(label, MenuItemType::RADIO);
    item->setRadioGroup(group);
    item->setChecked(checked);
    item->setAction([this, item, group, action]() {
      // Uncheck other radios in group
      for (auto* other : items_) {
        if (other != item && other->getType() == MenuItemType::RADIO && 
            other->getRadioGroup() == group) {
          other->setChecked(false);
        }
      }
      if (action) action();
    });
    addChild(item);
    items_.push_back(item);
    return item;
  }
  
  void addSeparator() {
    auto* item = new UIMenuItem("", MenuItemType::SEPARATOR);
    addChild(item);
    items_.push_back(item);
  }
  
  void addHeader(const char* text) {
    auto* item = new UIMenuItem(text, MenuItemType::HEADER);
    item->setStyle(Styles::menuHeader());
    addChild(item);
    items_.push_back(item);
  }
  
  // ---- Item Access ----
  
  int getItemCount() const { return items_.size(); }
  UIMenuItem* getItem(int index) const { 
    return (index >= 0 && index < (int)items_.size()) ? items_[index] : nullptr; 
  }
  
  const std::vector<UIMenuItem*>& getItems() const { return items_; }
  
  // ---- Selection ----
  
  void setSelectedIndex(int index) {
    if (index >= 0 && index < (int)items_.size() && items_[index]->isInteractive()) {
      if (selectedIndex_ >= 0 && selectedIndex_ < (int)items_.size()) {
        items_[selectedIndex_]->blur();
      }
      selectedIndex_ = index;
      items_[selectedIndex_]->focus();
      markDirty();
    }
  }
  
  int getSelectedIndex() const { return selectedIndex_; }
  UIMenuItem* getSelectedItem() const { return getItem(selectedIndex_); }
  
  void selectNext() {
    int next = selectedIndex_;
    do {
      next = (next + 1) % items_.size();
    } while (!items_[next]->isInteractive() && next != selectedIndex_);
    setSelectedIndex(next);
  }
  
  void selectPrev() {
    int prev = selectedIndex_;
    do {
      prev = (prev - 1 + items_.size()) % items_.size();
    } while (!items_[prev]->isInteractive() && prev != selectedIndex_);
    setSelectedIndex(prev);
  }
  
  void executeSelected() {
    if (auto* item = getSelectedItem()) {
      if (item->hasSubmenu()) {
        openSubmenu(item->getSubmenu());
      } else {
        item->execute();
        if (closeOnSelect_ && !item->hasSubmenu()) {
          close();
        }
      }
    }
  }
  
  // ---- Submenu Navigation ----
  
  void setParentMenu(UIMenu* parent) { parentMenu_ = parent; }
  UIMenu* getParentMenu() const { return parentMenu_; }
  
  void openSubmenu(UIMenu* submenu) {
    if (submenu) {
      activeSubmenu_ = submenu;
      submenu->show();
      // Position submenu next to current item
      if (auto* item = getSelectedItem()) {
        Rect itemBounds = item->getScreenBounds();
        submenu->setPosition(itemBounds.right(), itemBounds.y);
      }
    }
  }
  
  void closeSubmenu() {
    if (activeSubmenu_) {
      activeSubmenu_->hide();
      activeSubmenu_ = nullptr;
    }
  }
  
  UIMenu* getActiveSubmenu() const { return activeSubmenu_; }
  
  // ---- Show/Hide ----
  
  void show() {
    setVisible(true);
    // Select first interactive item
    for (int i = 0; i < (int)items_.size(); i++) {
      if (items_[i]->isInteractive()) {
        setSelectedIndex(i);
        break;
      }
    }
  }
  
  void close() {
    closeSubmenu();
    setVisible(false);
    if (parentMenu_) {
      parentMenu_->closeSubmenu();
    }
  }
  
  // ---- Settings ----
  
  void setCloseOnSelect(bool close) { closeOnSelect_ = close; }
  bool getCloseOnSelect() const { return closeOnSelect_; }
  
  // ---- Input Handling ----
  
  bool handleInput(InputEvent& event) override {
    if (!isVisible()) return false;
    
    // Forward to active submenu first
    if (activeSubmenu_ && activeSubmenu_->handleInput(event)) {
      return true;
    }
    
    if (event.type == InputEvent::BUTTON && event.btn.event == ButtonEvent::PRESSED) {
      switch (event.btn.button) {
        case Button::UP:
        case Button::ENCODER_CCW:
          selectPrev();
          event.consumed = true;
          return true;
          
        case Button::DOWN:
        case Button::ENCODER_CW:
          selectNext();
          event.consumed = true;
          return true;
          
        case Button::SELECT:
        case Button::RIGHT:
          executeSelected();
          event.consumed = true;
          return true;
          
        case Button::BACK:
        case Button::LEFT:
          if (parentMenu_) {
            close();
          } else if (activeSubmenu_) {
            closeSubmenu();
          } else {
            close();
          }
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
    UIContainer::layout();
    
    // Calculate menu size based on items
    Size prefSize = getPreferredSize();
    setSize(prefSize.width, prefSize.height);
  }
  
  Size getPreferredSize() const override {
    uint16_t maxWidth = textWidth(title_, FontSize::MEDIUM) + 16;
    uint16_t totalHeight = title_[0] ? 20 : 0;  // Title bar height
    
    for (auto* item : items_) {
      Size itemSize = item->getPreferredSize();
      maxWidth = std::max(maxWidth, itemSize.width);
      totalHeight += itemSize.height;
    }
    
    return Size(
      maxWidth + style_.horizontalSpace(),
      totalHeight + style_.verticalSpace()
    );
  }
  
  // ---- Rendering ----
  
  void render(UIRenderer& renderer) override;  // Implemented in UIRenderer.hpp
  
protected:
  char title_[32] = "";
  std::vector<UIMenuItem*> items_;
  int selectedIndex_ = 0;
  
  UIMenu* parentMenu_ = nullptr;
  UIMenu* activeSubmenu_ = nullptr;
  
  bool closeOnSelect_ = true;
};

// ============================================================
// UIMenuBar Class
// ============================================================

/**
 * @brief Horizontal menu bar (like File, Edit, View...)
 */
class UIMenuBar : public UIContainer {
public:
  UIMenuBar() {
    setLayoutMode(LayoutMode::FLEX);
    setFlexDirection(FlexDirection::ROW);
    style_.backgroundColor(Color(30)).height(16);
  }
  
  const char* getTypeName() const override { return "UIMenuBar"; }
  
  void addMenu(const char* label, UIMenu* menu) {
    auto* item = new UIMenuItem(label, MenuItemType::SUBMENU);
    item->setSubmenu(menu);
    addChild(item);
    menus_.push_back({item, menu});
  }
  
  bool handleInput(InputEvent& event) override {
    if (event.type == InputEvent::BUTTON && event.btn.event == ButtonEvent::PRESSED) {
      if (event.btn.button == Button::LEFT) {
        selectPrev();
        return true;
      } else if (event.btn.button == Button::RIGHT) {
        selectNext();
        return true;
      } else if (event.btn.button == Button::SELECT || event.btn.button == Button::DOWN) {
        openSelected();
        return true;
      }
    }
    return UIContainer::handleInput(event);
  }
  
protected:
  struct MenuEntry {
    UIMenuItem* item;
    UIMenu* menu;
  };
  
  std::vector<MenuEntry> menus_;
  int selectedIndex_ = 0;
  
  void selectNext() {
    selectedIndex_ = (selectedIndex_ + 1) % menus_.size();
    markDirty();
  }
  
  void selectPrev() {
    selectedIndex_ = (selectedIndex_ - 1 + menus_.size()) % menus_.size();
    markDirty();
  }
  
  void openSelected() {
    if (selectedIndex_ >= 0 && selectedIndex_ < (int)menus_.size()) {
      menus_[selectedIndex_].menu->show();
    }
  }
};

} // namespace UI
} // namespace SystemAPI
