/**
 * @file StatusBar.hpp
 * @brief Top status bar widget for OLED UI
 */

#pragma once

#include "../Core/Element.hpp"
#include "../Elements/TextElement.hpp"
#include "../Elements/IconElement.hpp"
#include "../Elements/ContainerElement.hpp"
#include <vector>

namespace OledUI {

//=============================================================================
// StatusItem - Individual status indicator
//=============================================================================
struct StatusItem {
    std::string id;
    Icon icon = Icon::NONE;
    std::string text;
    bool visible = true;
    
    StatusItem() = default;
    StatusItem(const std::string& i, Icon ic) : id(i), icon(ic) {}
    StatusItem(const std::string& i, const std::string& t) : id(i), text(t) {}
    StatusItem(const std::string& i, Icon ic, const std::string& t) : id(i), icon(ic), text(t) {}
};

//=============================================================================
// StatusBar - Top status bar (like phone status bar)
//=============================================================================
class StatusBar : public Element {
private:
    std::string title_;
    std::vector<StatusItem> leftItems_;
    std::vector<StatusItem> rightItems_;
    bool showDivider_ = true;
    
public:
    StatusBar() : Element("statusbar") {
        style_.height = 12;
        style_.width = OLED_WIDTH;
        style_.padding.set(1, 2);
    }
    
    //-------------------------------------------------------------------------
    // Title
    //-------------------------------------------------------------------------
    const std::string& getTitle() const { return title_; }
    
    StatusBar& setTitle(const std::string& t) {
        title_ = t;
        markDirty();
        return *this;
    }
    
    //-------------------------------------------------------------------------
    // Status Items
    //-------------------------------------------------------------------------
    StatusBar& addLeft(const StatusItem& item) {
        leftItems_.push_back(item);
        markDirty();
        return *this;
    }
    
    StatusBar& addRight(const StatusItem& item) {
        rightItems_.push_back(item);
        markDirty();
        return *this;
    }
    
    StatusBar& removeItem(const std::string& id) {
        leftItems_.erase(
            std::remove_if(leftItems_.begin(), leftItems_.end(),
                [&id](const StatusItem& i) { return i.id == id; }),
            leftItems_.end());
        rightItems_.erase(
            std::remove_if(rightItems_.begin(), rightItems_.end(),
                [&id](const StatusItem& i) { return i.id == id; }),
            rightItems_.end());
        markDirty();
        return *this;
    }
    
    StatusBar& updateItem(const std::string& id, const StatusItem& newItem) {
        for (auto& item : leftItems_) {
            if (item.id == id) { item = newItem; markDirty(); return *this; }
        }
        for (auto& item : rightItems_) {
            if (item.id == id) { item = newItem; markDirty(); return *this; }
        }
        return *this;
    }
    
    StatusBar& setItemVisible(const std::string& id, bool visible) {
        for (auto& item : leftItems_) {
            if (item.id == id) { item.visible = visible; markDirty(); return *this; }
        }
        for (auto& item : rightItems_) {
            if (item.id == id) { item.visible = visible; markDirty(); return *this; }
        }
        return *this;
    }
    
    //-------------------------------------------------------------------------
    // Common Status Updates
    //-------------------------------------------------------------------------
    StatusBar& setBattery(int percent) {
        Icon icon = Icon::BATTERY_EMPTY;
        if (percent >= 80) icon = Icon::BATTERY_FULL;
        else if (percent >= 50) icon = Icon::BATTERY_HALF;
        else if (percent >= 20) icon = Icon::BATTERY_LOW;
        
        StatusItem item("battery", icon, std::to_string(percent) + "%");
        
        // Update or add
        bool found = false;
        for (auto& i : rightItems_) {
            if (i.id == "battery") { i = item; found = true; break; }
        }
        if (!found) addRight(item);
        
        markDirty();
        return *this;
    }
    
    StatusBar& setWifi(bool connected, int strength = -1) {
        Icon icon = connected ? 
            (strength > 50 ? Icon::WIFI : Icon::WIFI_OFF) : Icon::WIFI_OFF;
        StatusItem item("wifi", icon);
        item.visible = true;
        
        bool found = false;
        for (auto& i : rightItems_) {
            if (i.id == "wifi") { i = item; found = true; break; }
        }
        if (!found) addRight(item);
        
        markDirty();
        return *this;
    }
    
    StatusBar& setBluetooth(bool connected) {
        StatusItem item("bluetooth", connected ? Icon::BLUETOOTH : Icon::NONE);
        item.visible = connected;
        
        bool found = false;
        for (auto& i : rightItems_) {
            if (i.id == "bluetooth") { i = item; found = true; break; }
        }
        if (!found) addRight(item);
        
        markDirty();
        return *this;
    }
    
    StatusBar& setTime(const std::string& time) {
        StatusItem item("time", time);
        
        bool found = false;
        for (auto& i : rightItems_) {
            if (i.id == "time") { i = item; found = true; break; }
        }
        if (!found) addRight(item);
        
        markDirty();
        return *this;
    }
    
    //-------------------------------------------------------------------------
    // Configuration
    //-------------------------------------------------------------------------
    StatusBar& setShowDivider(bool s) { showDivider_ = s; markDirty(); return *this; }
    
    //-------------------------------------------------------------------------
    // Layout
    //-------------------------------------------------------------------------
    Rect measure(int16_t availableWidth, int16_t availableHeight) override {
        Rect preferred;
        preferred.width = availableWidth;
        preferred.height = style_.height >= 0 ? style_.height : 12;
        return preferred;
    }
    
    bool isFocusable() const override { return false; }
    
protected:
    void renderContent(GpuCommands* gpu) override;
};

//=============================================================================
// Factory Function
//=============================================================================

inline std::shared_ptr<StatusBar> CreateStatusBar(const std::string& title = "") {
    auto s = std::make_shared<StatusBar>();
    if (!title.empty()) s->setTitle(title);
    return s;
}

} // namespace OledUI
