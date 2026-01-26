/**
 * @file Menu.hpp
 * @brief Menu widget for OLED UI
 */

#pragma once

#include "../Core/Element.hpp"
#include "../Core/Page.hpp"
#include "../Elements/ListElement.hpp"
#include <functional>
#include <vector>

namespace OledUI {

//=============================================================================
// MenuItem - Menu entry with optional submenu or action
//=============================================================================
struct MenuItem {
    std::string id;
    std::string label;
    Icon icon = Icon::NONE;
    bool enabled = true;
    bool separator = false;  // If true, render as separator line
    Callback action;         // Action when selected
    std::string submenuId;   // Navigate to submenu page
    
    // Value for toggle/choice items
    bool isToggle = false;
    bool toggleValue = false;
    
    bool isChoice = false;
    std::vector<std::string> choices;
    int choiceIndex = 0;
    
    MenuItem() = default;
    
    // Separator
    static MenuItem Separator() {
        MenuItem m;
        m.separator = true;
        return m;
    }
    
    // Action item
    MenuItem(const std::string& l, Callback a = nullptr) 
        : label(l), action(a) { id = l; }
    
    MenuItem(const std::string& i, const std::string& l, Callback a = nullptr)
        : id(i), label(l), action(a) {}
    
    MenuItem(Icon ic, const std::string& l, Callback a = nullptr)
        : label(l), icon(ic), action(a) { id = l; }
    
    // Submenu item
    MenuItem(const std::string& l, const std::string& submenu)
        : label(l), submenuId(submenu) { id = l; }
    
    // Toggle item
    static MenuItem Toggle(const std::string& label, bool value, 
                          std::function<void(bool)> onChange = nullptr) {
        MenuItem m;
        m.id = label;
        m.label = label;
        m.isToggle = true;
        m.toggleValue = value;
        if (onChange) {
            m.action = [&m, onChange]() {
                m.toggleValue = !m.toggleValue;
                onChange(m.toggleValue);
            };
        }
        return m;
    }
    
    // Choice item (cycles through options)
    static MenuItem Choice(const std::string& label, 
                          const std::vector<std::string>& options,
                          int selectedIndex = 0,
                          std::function<void(int)> onChange = nullptr) {
        MenuItem m;
        m.id = label;
        m.label = label;
        m.isChoice = true;
        m.choices = options;
        m.choiceIndex = selectedIndex;
        if (onChange) {
            m.action = [&m, onChange]() {
                m.choiceIndex = (m.choiceIndex + 1) % m.choices.size();
                onChange(m.choiceIndex);
            };
        }
        return m;
    }
};

//=============================================================================
// Menu - Full-featured menu widget
//=============================================================================
class Menu : public Element {
public:
    using NavigateCallback = std::function<void(const std::string&)>;
    
private:
    std::string title_;
    std::vector<MenuItem> items_;
    int selectedIndex_ = 0;
    int scrollOffset_ = 0;
    int itemHeight_ = 14;
    int visibleCount_ = 0;
    bool showTitle_ = true;
    bool wrapAround_ = true;
    Callback onBack_;
    NavigateCallback onNavigate_;
    
public:
    Menu() : Element("menu") {
        style_.flex = 1;
    }
    
    //-------------------------------------------------------------------------
    // Title
    //-------------------------------------------------------------------------
    Menu& setTitle(const std::string& t) { title_ = t; markDirty(); return *this; }
    Menu& setShowTitle(bool s) { showTitle_ = s; markDirty(); return *this; }
    const std::string& getTitle() const { return title_; }
    
    //-------------------------------------------------------------------------
    // Items
    //-------------------------------------------------------------------------
    Menu& addItem(const MenuItem& item) {
        items_.push_back(item);
        markDirty();
        return *this;
    }
    
    Menu& addSeparator() {
        items_.push_back(MenuItem::Separator());
        markDirty();
        return *this;
    }
    
    Menu& setItems(const std::vector<MenuItem>& items) {
        items_ = items;
        selectedIndex_ = 0;
        scrollOffset_ = 0;
        markDirty();
        return *this;
    }
    
    Menu& clearItems() {
        items_.clear();
        selectedIndex_ = 0;
        scrollOffset_ = 0;
        markDirty();
        return *this;
    }
    
    const std::vector<MenuItem>& getItems() const { return items_; }
    
    MenuItem* getItem(const std::string& id) {
        for (auto& item : items_) {
            if (item.id == id) return &item;
        }
        return nullptr;
    }
    
    //-------------------------------------------------------------------------
    // Selection
    //-------------------------------------------------------------------------
    int getSelectedIndex() const { return selectedIndex_; }
    
    MenuItem* getSelectedItem() {
        if (selectedIndex_ >= 0 && selectedIndex_ < (int)items_.size()) {
            return &items_[selectedIndex_];
        }
        return nullptr;
    }
    
    Menu& selectNext() {
        if (items_.empty()) return *this;
        
        int start = selectedIndex_;
        do {
            selectedIndex_++;
            if (selectedIndex_ >= (int)items_.size()) {
                selectedIndex_ = wrapAround_ ? 0 : items_.size() - 1;
            }
            // Skip separators and disabled items
            if (!items_[selectedIndex_].separator && 
                items_[selectedIndex_].enabled) break;
        } while (selectedIndex_ != start);
        
        ensureVisible();
        markDirty();
        return *this;
    }
    
    Menu& selectPrev() {
        if (items_.empty()) return *this;
        
        int start = selectedIndex_;
        do {
            selectedIndex_--;
            if (selectedIndex_ < 0) {
                selectedIndex_ = wrapAround_ ? items_.size() - 1 : 0;
            }
            // Skip separators and disabled items
            if (!items_[selectedIndex_].separator && 
                items_[selectedIndex_].enabled) break;
        } while (selectedIndex_ != start);
        
        ensureVisible();
        markDirty();
        return *this;
    }
    
    //-------------------------------------------------------------------------
    // Configuration
    //-------------------------------------------------------------------------
    Menu& setItemHeight(int h) { itemHeight_ = h; markDirty(); return *this; }
    Menu& setWrapAround(bool w) { wrapAround_ = w; return *this; }
    Menu& setOnBack(OnClickCallback cb) { onBack_ = cb; return *this; }
    Menu& setOnNavigate(NavigateCallback cb) { onNavigate_ = cb; return *this; }
    
    //-------------------------------------------------------------------------
    // Layout
    //-------------------------------------------------------------------------
    void layout(int16_t x, int16_t y, int16_t width, int16_t height) override {
        Element::layout(x, y, width, height);
        
        int titleHeight = showTitle_ && !title_.empty() ? 14 : 0;
        int contentHeight = bounds_.height - titleHeight - style_.padding.vertical();
        visibleCount_ = contentHeight / itemHeight_;
        
        ensureVisible();
    }
    
    //-------------------------------------------------------------------------
    // Interaction
    //-------------------------------------------------------------------------
    bool handleInput(InputEvent event) override {
        if (!style_.enabled) return false;
        
        switch (event) {
            case InputEvent::UP:
                selectPrev();
                return true;
                
            case InputEvent::DOWN:
                selectNext();
                return true;
                
            case InputEvent::SELECT: {
                auto item = getSelectedItem();
                if (item && !item->separator && item->enabled) {
                    if (!item->submenuId.empty() && onNavigate_) {
                        onNavigate_(item->submenuId);
                    }
                    if (item->action) {
                        item->action();
                        markDirty();
                    }
                }
                return true;
            }
            
            case InputEvent::BACK:
                if (onBack_) {
                    onBack_();
                    return true;
                }
                break;
                
            case InputEvent::LEFT:
                // For choice items, go prev
                if (auto item = getSelectedItem()) {
                    if (item->isChoice && item->choices.size() > 1) {
                        item->choiceIndex--;
                        if (item->choiceIndex < 0) 
                            item->choiceIndex = item->choices.size() - 1;
                        if (item->action) item->action();
                        markDirty();
                        return true;
                    }
                }
                break;
                
            case InputEvent::RIGHT:
                // For choice items, go next
                if (auto item = getSelectedItem()) {
                    if (item->isChoice && item->choices.size() > 1) {
                        item->choiceIndex = (item->choiceIndex + 1) % item->choices.size();
                        if (item->action) item->action();
                        markDirty();
                        return true;
                    }
                }
                break;
                
            default:
                break;
        }
        
        return Element::handleInput(event);
    }
    
    bool isFocusable() const override { return style_.enabled && !items_.empty(); }
    
protected:
    void ensureVisible() {
        if (selectedIndex_ < scrollOffset_) {
            scrollOffset_ = selectedIndex_;
        } else if (selectedIndex_ >= scrollOffset_ + visibleCount_) {
            scrollOffset_ = selectedIndex_ - visibleCount_ + 1;
        }
        scrollOffset_ = std::max(0, std::min(scrollOffset_,
            (int)items_.size() - visibleCount_));
    }
    
    void renderContent(GpuCommands* gpu) override;
};

//=============================================================================
// Factory Functions
//=============================================================================

inline std::shared_ptr<Menu> CreateMenu(const std::string& title = "") {
    auto m = std::make_shared<Menu>();
    if (!title.empty()) m->setTitle(title);
    return m;
}

inline std::shared_ptr<Menu> CreateMenu(const std::vector<MenuItem>& items) {
    auto m = std::make_shared<Menu>();
    m->setItems(items);
    return m;
}

} // namespace OledUI
