/**
 * @file ListElement.hpp
 * @brief Scrollable list element for OLED UI (like HTML <ul>/<ol> with scroll)
 */

#pragma once

#include "../Core/Element.hpp"
#include <vector>
#include <functional>
#include <any>

namespace OledUI {

//=============================================================================
// ListItem - Individual item in a list
//=============================================================================
struct ListItem {
    std::string text;
    std::string secondaryText;
    Icon icon = Icon::NONE;
    bool enabled = true;
    std::any data;  // User data attached to item
    
    ListItem() = default;
    ListItem(const std::string& t) : text(t) {}
    ListItem(const std::string& t, const std::string& s) : text(t), secondaryText(s) {}
    ListItem(Icon i, const std::string& t) : text(t), icon(i) {}
    ListItem(Icon i, const std::string& t, const std::string& s) : text(t), secondaryText(s), icon(i) {}
};

//=============================================================================
// ListElement - Scrollable list (like HTML <ul> with virtual scrolling)
//=============================================================================
class ListElement : public Element {
public:
    using SelectCallback = std::function<void(int index, const ListItem& item)>;
    
private:
    std::vector<ListItem> items_;
    int selectedIndex_ = 0;
    int scrollOffset_ = 0;      // First visible item index
    int itemHeight_ = 12;       // Height per item
    int visibleCount_ = 0;      // Number of visible items (calculated)
    bool showScrollbar_ = true;
    bool wrapAround_ = false;   // Wrap selection at ends
    SelectCallback onSelect_;
    SelectCallback onChange_;   // Called when selection changes
    
public:
    ListElement() : Element("list") {
        style_.flex = 1;  // Lists typically fill available space
    }
    
    //-------------------------------------------------------------------------
    // Item Management
    //-------------------------------------------------------------------------
    const std::vector<ListItem>& getItems() const { return items_; }
    
    ListElement& setItems(const std::vector<ListItem>& items) {
        items_ = items;
        selectedIndex_ = 0;
        scrollOffset_ = 0;
        markDirty();
        return *this;
    }
    
    ListElement& addItem(const ListItem& item) {
        items_.push_back(item);
        markDirty();
        return *this;
    }
    
    ListElement& addItem(const std::string& text) {
        items_.push_back(ListItem(text));
        markDirty();
        return *this;
    }
    
    ListElement& addItem(Icon icon, const std::string& text) {
        items_.push_back(ListItem(icon, text));
        markDirty();
        return *this;
    }
    
    ListElement& clearItems() {
        items_.clear();
        selectedIndex_ = 0;
        scrollOffset_ = 0;
        markDirty();
        return *this;
    }
    
    ListElement& removeItem(int index) {
        if (index >= 0 && index < (int)items_.size()) {
            items_.erase(items_.begin() + index);
            if (selectedIndex_ >= (int)items_.size()) {
                selectedIndex_ = items_.size() - 1;
            }
            markDirty();
        }
        return *this;
    }
    
    size_t itemCount() const { return items_.size(); }
    
    //-------------------------------------------------------------------------
    // Selection
    //-------------------------------------------------------------------------
    int getSelectedIndex() const { return selectedIndex_; }
    
    const ListItem* getSelectedItem() const {
        if (selectedIndex_ >= 0 && selectedIndex_ < (int)items_.size()) {
            return &items_[selectedIndex_];
        }
        return nullptr;
    }
    
    ListElement& setSelectedIndex(int index) {
        if (index != selectedIndex_ && index >= 0 && index < (int)items_.size()) {
            selectedIndex_ = index;
            ensureVisible(selectedIndex_);
            markDirty();
            if (onChange_) {
                onChange_(selectedIndex_, items_[selectedIndex_]);
            }
        }
        return *this;
    }
    
    ListElement& selectNext() {
        if (items_.empty()) return *this;
        
        int next = selectedIndex_ + 1;
        if (next >= (int)items_.size()) {
            next = wrapAround_ ? 0 : items_.size() - 1;
        }
        
        // Skip disabled items
        int startIndex = next;
        while (!items_[next].enabled) {
            next++;
            if (next >= (int)items_.size()) {
                next = wrapAround_ ? 0 : items_.size() - 1;
            }
            if (next == startIndex) break;  // All items disabled
        }
        
        setSelectedIndex(next);
        return *this;
    }
    
    ListElement& selectPrev() {
        if (items_.empty()) return *this;
        
        int prev = selectedIndex_ - 1;
        if (prev < 0) {
            prev = wrapAround_ ? items_.size() - 1 : 0;
        }
        
        // Skip disabled items
        int startIndex = prev;
        while (!items_[prev].enabled) {
            prev--;
            if (prev < 0) {
                prev = wrapAround_ ? items_.size() - 1 : 0;
            }
            if (prev == startIndex) break;  // All items disabled
        }
        
        setSelectedIndex(prev);
        return *this;
    }
    
    //-------------------------------------------------------------------------
    // Configuration
    //-------------------------------------------------------------------------
    ListElement& setItemHeight(int h) { itemHeight_ = h; markDirty(); return *this; }
    ListElement& setShowScrollbar(bool s) { showScrollbar_ = s; markDirty(); return *this; }
    ListElement& setWrapAround(bool w) { wrapAround_ = w; return *this; }
    
    ListElement& setOnSelect(SelectCallback cb) { onSelect_ = cb; return *this; }
    ListElement& setOnChange(SelectCallback cb) { onChange_ = cb; return *this; }
    
    //-------------------------------------------------------------------------
    // Layout
    //-------------------------------------------------------------------------
    Rect measure(int16_t availableWidth, int16_t availableHeight) override {
        Rect preferred = Element::measure(availableWidth, availableHeight);
        
        if (style_.height < 0) {
            // Auto height: show all items up to available
            int totalHeight = items_.size() * itemHeight_ + style_.margin.vertical();
            preferred.height = std::min((int)totalHeight, (int)availableHeight);
        }
        
        return preferred;
    }
    
    void layout(int16_t x, int16_t y, int16_t width, int16_t height) override {
        Element::layout(x, y, width, height);
        
        // Calculate visible count
        int contentHeight = bounds_.height - style_.padding.vertical();
        visibleCount_ = contentHeight / itemHeight_;
        
        // Ensure selected item is visible
        ensureVisible(selectedIndex_);
    }
    
    //-------------------------------------------------------------------------
    // Interaction
    //-------------------------------------------------------------------------
    bool handleInput(InputEvent event) override {
        if (!style_.enabled || items_.empty()) return false;
        
        switch (event) {
            case InputEvent::UP:
                selectPrev();
                return true;
                
            case InputEvent::DOWN:
                selectNext();
                return true;
                
            case InputEvent::SELECT:
                if (onSelect_ && selectedIndex_ >= 0 && items_[selectedIndex_].enabled) {
                    onSelect_(selectedIndex_, items_[selectedIndex_]);
                }
                return true;
                
            default:
                break;
        }
        
        return Element::handleInput(event);
    }
    
    bool isFocusable() const override { return style_.enabled && !items_.empty(); }
    
protected:
    void ensureVisible(int index) {
        if (index < scrollOffset_) {
            scrollOffset_ = index;
        } else if (index >= scrollOffset_ + visibleCount_) {
            scrollOffset_ = index - visibleCount_ + 1;
        }
        scrollOffset_ = std::max(0, std::min(scrollOffset_, 
            (int)items_.size() - visibleCount_));
    }
    
    void renderContent(GpuCommands* gpu) override;
    void renderFocus(GpuCommands* gpu) override;
};

//=============================================================================
// Factory Functions
//=============================================================================

inline std::shared_ptr<ListElement> List() {
    return std::make_shared<ListElement>();
}

inline std::shared_ptr<ListElement> List(const std::vector<std::string>& items) {
    auto l = std::make_shared<ListElement>();
    for (const auto& item : items) {
        l->addItem(item);
    }
    return l;
}

inline std::shared_ptr<ListElement> List(const std::vector<ListItem>& items) {
    auto l = std::make_shared<ListElement>();
    l->setItems(items);
    return l;
}

} // namespace OledUI
