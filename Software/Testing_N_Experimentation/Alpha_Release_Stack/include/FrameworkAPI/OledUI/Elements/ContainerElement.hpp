/**
 * @file ContainerElement.hpp
 * @brief Layout container element for OLED UI (like HTML <div> with flexbox)
 */

#pragma once

#include "../Core/Element.hpp"
#include <algorithm>

namespace OledUI {

//=============================================================================
// ContainerElement - Layout container (like HTML <div> with flexbox)
//=============================================================================
class ContainerElement : public Element {
private:
    bool scrollable_ = false;
    int16_t scrollOffset_ = 0;
    int16_t contentHeight_ = 0;
    
public:
    ContainerElement() : Element("div") {}
    
    //-------------------------------------------------------------------------
    // Container Configuration
    //-------------------------------------------------------------------------
    ContainerElement& setDirection(FlexDirection d) { 
        style_.flexDirection = d; 
        markDirty();
        return *this;
    }
    
    ContainerElement& row() { return setDirection(FlexDirection::ROW); }
    ContainerElement& column() { return setDirection(FlexDirection::COLUMN); }
    
    ContainerElement& setJustify(Justify j) { 
        style_.justify = j; 
        markDirty();
        return *this;
    }
    
    ContainerElement& setAlign(Align a) { 
        style_.align = a; 
        markDirty();
        return *this;
    }
    
    ContainerElement& setGap(int16_t g) { 
        style_.gap = g; 
        markDirty();
        return *this;
    }
    
    ContainerElement& setScrollable(bool s) {
        scrollable_ = s;
        markDirty();
        return *this;
    }
    
    // Add child element (fluent interface)
    ContainerElement* add(std::shared_ptr<Element> child) {
        addChild(child);
        return this;
    }
    
    bool isScrollable() const { return scrollable_; }
    int16_t getScrollOffset() const { return scrollOffset_; }
    
    void scrollTo(int16_t offset) {
        int16_t maxScroll = std::max(0, contentHeight_ - bounds_.height);
        scrollOffset_ = std::clamp(offset, (int16_t)0, maxScroll);
        markDirty();
    }
    
    void scrollBy(int16_t delta) {
        scrollTo(scrollOffset_ + delta);
    }
    
    //-------------------------------------------------------------------------
    // Layout
    //-------------------------------------------------------------------------
    Rect measure(int16_t availableWidth, int16_t availableHeight) override {
        Rect preferred = Element::measure(availableWidth, availableHeight);
        
        // If no explicit size, calculate from children
        if (style_.width < 0 || style_.height < 0) {
            int16_t totalWidth = 0;
            int16_t totalHeight = 0;
            int16_t maxWidth = 0;
            int16_t maxHeight = 0;
            
            int16_t childAvailW = (style_.width >= 0) ? 
                (style_.width - style_.padding.horizontal()) : availableWidth;
            int16_t childAvailH = (style_.height >= 0) ? 
                (style_.height - style_.padding.vertical()) : availableHeight;
            
            for (auto& child : children_) {
                if (!child->style().visible) continue;
                
                Rect childSize = child->measure(childAvailW, childAvailH);
                
                if (style_.flexDirection == FlexDirection::COLUMN) {
                    totalHeight += childSize.height + style_.gap;
                    maxWidth = std::max(maxWidth, childSize.width);
                } else {
                    totalWidth += childSize.width + style_.gap;
                    maxHeight = std::max(maxHeight, childSize.height);
                }
            }
            
            // Remove trailing gap
            if (!children_.empty()) {
                if (style_.flexDirection == FlexDirection::COLUMN) totalHeight -= style_.gap;
                else totalWidth -= style_.gap;
            }
            
            if (style_.width < 0) {
                if (style_.flexDirection == FlexDirection::COLUMN) {
                    preferred.width = maxWidth + style_.padding.horizontal() + style_.margin.horizontal();
                } else {
                    preferred.width = totalWidth + style_.padding.horizontal() + style_.margin.horizontal();
                }
            }
            
            if (style_.height < 0) {
                if (style_.flexDirection == FlexDirection::COLUMN) {
                    preferred.height = totalHeight + style_.padding.vertical() + style_.margin.vertical();
                } else {
                    preferred.height = maxHeight + style_.padding.vertical() + style_.margin.vertical();
                }
            }
        }
        
        return preferred;
    }
    
    void layout(int16_t x, int16_t y, int16_t width, int16_t height) override {
        Element::layout(x, y, width, height);
        
        // Calculate content area
        int16_t contentX = bounds_.x + style_.padding.left;
        int16_t contentY = bounds_.y + style_.padding.top;
        int16_t contentW = bounds_.width - style_.padding.horizontal();
        int16_t contentH = bounds_.height - style_.padding.vertical();
        
        if (scrollable_) {
            contentY -= scrollOffset_;
        }
        
        // Layout children based on flex direction
        layoutChildren(contentX, contentY, contentW, contentH);
    }
    
    bool isFocusable() const override { return false; }  // Container itself not focusable
    
protected:
    void layoutChildren(int16_t x, int16_t y, int16_t w, int16_t h) {
        if (children_.empty()) return;
        
        // First pass: measure all children
        struct ChildInfo {
            std::shared_ptr<Element> elem;
            Rect size;
            int16_t flex;
        };
        std::vector<ChildInfo> visibleChildren;
        int16_t totalFixed = 0;
        int16_t totalFlex = 0;
        
        for (auto& child : children_) {
            if (!child->style().visible) continue;
            
            ChildInfo info;
            info.elem = child;
            info.size = child->measure(w, h);
            info.flex = child->style().flex;
            
            if (info.flex > 0) {
                totalFlex += info.flex;
            } else {
                if (style_.flexDirection == FlexDirection::COLUMN) {
                    totalFixed += info.size.height + style_.gap;
                } else {
                    totalFixed += info.size.width + style_.gap;
                }
            }
            
            visibleChildren.push_back(info);
        }
        
        if (visibleChildren.empty()) return;
        
        // Remove trailing gap from fixed
        if (style_.flexDirection == FlexDirection::COLUMN) {
            totalFixed -= style_.gap;
            contentHeight_ = totalFixed;
        } else {
            totalFixed -= style_.gap;
        }
        
        // Calculate flex space
        int16_t availableSpace = (style_.flexDirection == FlexDirection::COLUMN) ? h : w;
        int16_t flexSpace = std::max((int16_t)0, (int16_t)(availableSpace - totalFixed));
        
        // Second pass: layout each child
        int16_t currentX = x;
        int16_t currentY = y;
        
        // Handle justify for non-flex layouts
        if (totalFlex == 0) {
            int16_t totalSize = totalFixed;
            int16_t remaining = availableSpace - totalSize;
            
            switch (style_.justify) {
                case Justify::CENTER:
                    if (style_.flexDirection == FlexDirection::COLUMN) currentY += remaining / 2;
                    else currentX += remaining / 2;
                    break;
                case Justify::END:
                    if (style_.flexDirection == FlexDirection::COLUMN) currentY += remaining;
                    else currentX += remaining;
                    break;
                case Justify::SPACE_BETWEEN:
                    // Gap is recalculated between items
                    if (visibleChildren.size() > 1) {
                        // Handled per-item below
                    }
                    break;
                case Justify::SPACE_AROUND:
                    // Equal space around each item
                    if (visibleChildren.size() > 0) {
                        int16_t space = remaining / (visibleChildren.size() * 2);
                        if (style_.flexDirection == FlexDirection::COLUMN) currentY += space;
                        else currentX += space;
                    }
                    break;
                default:
                    break;
            }
        }
        
        for (size_t i = 0; i < visibleChildren.size(); i++) {
            auto& info = visibleChildren[i];
            Rect childBounds;
            
            if (style_.flexDirection == FlexDirection::COLUMN) {
                childBounds.x = x;
                childBounds.y = currentY;
                childBounds.width = w;
                
                if (info.flex > 0) {
                    childBounds.height = (flexSpace * info.flex) / totalFlex;
                } else {
                    childBounds.height = info.size.height;
                }
                
                // Handle cross-axis alignment
                switch (style_.align) {
                    case Align::CENTER:
                        childBounds.x = x + (w - info.size.width) / 2;
                        childBounds.width = info.size.width;
                        break;
                    case Align::END:
                        childBounds.x = x + w - info.size.width;
                        childBounds.width = info.size.width;
                        break;
                    default: // STRETCH
                        break;
                }
                
                info.elem->layout(childBounds.x, childBounds.y, childBounds.width, childBounds.height);
                currentY += childBounds.height + style_.gap;
            } else {
                childBounds.x = currentX;
                childBounds.y = y;
                childBounds.height = h;
                
                if (info.flex > 0) {
                    childBounds.width = (flexSpace * info.flex) / totalFlex;
                } else {
                    childBounds.width = info.size.width;
                }
                
                // Handle cross-axis alignment
                switch (style_.align) {
                    case Align::CENTER:
                        childBounds.y = y + (h - info.size.height) / 2;
                        childBounds.height = info.size.height;
                        break;
                    case Align::END:
                        childBounds.y = y + h - info.size.height;
                        childBounds.height = info.size.height;
                        break;
                    default: // STRETCH
                        break;
                }
                
                info.elem->layout(childBounds.x, childBounds.y, childBounds.width, childBounds.height);
                currentX += childBounds.width + style_.gap;
            }
        }
    }
    
    void renderContent(GpuCommands* gpu) override {
        // Container renders its children (already handled by base render())
    }
};

//=============================================================================
// Factory Functions
//=============================================================================

inline std::shared_ptr<ContainerElement> Container() {
    return std::make_shared<ContainerElement>();
}

inline std::shared_ptr<ContainerElement> Row(int16_t gap = 4) {
    auto c = std::make_shared<ContainerElement>();
    c->row().setGap(gap);
    return c;
}

inline std::shared_ptr<ContainerElement> Column(int16_t gap = 4) {
    auto c = std::make_shared<ContainerElement>();
    c->column().setGap(gap);
    return c;
}

inline std::shared_ptr<ContainerElement> Center() {
    auto c = std::make_shared<ContainerElement>();
    c->setStyle(Style::Centered());
    return c;
}

inline std::shared_ptr<ContainerElement> Card() {
    auto c = std::make_shared<ContainerElement>();
    c->setStyle(Style::Card());
    c->column();
    return c;
}

inline std::shared_ptr<ContainerElement> ScrollView() {
    auto c = std::make_shared<ContainerElement>();
    c->column().setScrollable(true);
    return c;
}

} // namespace OledUI
