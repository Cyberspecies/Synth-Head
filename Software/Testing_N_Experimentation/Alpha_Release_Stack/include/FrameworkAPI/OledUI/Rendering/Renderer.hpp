/**
 * @file Renderer.hpp
 * @brief Rendering implementations for all OLED UI elements
 * 
 * This file contains the actual GPU rendering code for all elements.
 * Include this file ONCE in a .cpp file after including the main OledUI.hpp
 */

#pragma once

#include "../Core/Element.hpp"
#include "../Core/Page.hpp"
#include "../Core/Navigator.hpp"
#include "../Elements/TextElement.hpp"
#include "../Elements/IconElement.hpp"
#include "../Elements/ButtonElement.hpp"
#include "../Elements/ContainerElement.hpp"
#include "../Elements/ListElement.hpp"
#include "../Elements/ProgressElement.hpp"
#include "../Elements/DividerElement.hpp"
#include "../Widgets/StatusBar.hpp"
#include "../Widgets/Menu.hpp"
#include "../Widgets/Dialog.hpp"
#include "../Widgets/Toast.hpp"

namespace OledUI {

//=============================================================================
// Element Base - Background, Border, Focus
//=============================================================================

inline void Element::renderBackground(GpuCommands* gpu) {
    if (!gpu) return;
    
    if (style_.color.inverted) {
        // Filled white rectangle
        gpu->oledFill(bounds_.x, bounds_.y, bounds_.width, bounds_.height, true);
    }
}

inline void Element::renderBorder(GpuCommands* gpu) {
    if (!gpu) return;
    
    if (style_.borderWidth > 0) {
        bool on = !style_.color.inverted;
        
        // Draw border rectangle (outline)
        for (int i = 0; i < style_.borderWidth; i++) {
            gpu->oledRect(bounds_.x + i, bounds_.y + i,
                         bounds_.width - 2*i, bounds_.height - 2*i, on);
        }
    }
}

inline void Element::renderFocus(GpuCommands* gpu) {
    if (!gpu || !focused_) return;
    
    // Dotted/dashed focus indicator
    // For simplicity, draw corners
    uint8_t color = style_.color.inverted ? 0 : 1;
    int16_t x = bounds_.x;
    int16_t y = bounds_.y;
    int16_t w = bounds_.width;
    int16_t h = bounds_.height;
    
    // Top-left corner
    gpu->oledPixel(x, y, color);
    gpu->oledPixel(x+1, y, color);
    gpu->oledPixel(x, y+1, color);
    
    // Top-right corner
    gpu->oledPixel(x+w-1, y, color);
    gpu->oledPixel(x+w-2, y, color);
    gpu->oledPixel(x+w-1, y+1, color);
    
    // Bottom-left corner
    gpu->oledPixel(x, y+h-1, color);
    gpu->oledPixel(x+1, y+h-1, color);
    gpu->oledPixel(x, y+h-2, color);
    
    // Bottom-right corner
    gpu->oledPixel(x+w-1, y+h-1, color);
    gpu->oledPixel(x+w-2, y+h-1, color);
    gpu->oledPixel(x+w-1, y+h-2, color);
}

//=============================================================================
// TextElement
//=============================================================================

inline void TextElement::renderContent(GpuCommands* gpu) {
    if (!gpu || text_.empty()) return;
    
    int scale = static_cast<int>(style_.textSize);
    int charWidth = 6 * scale;
    int charHeight = 8 * scale;
    
    // Calculate text position
    int16_t textX = bounds_.x + style_.padding.left;
    int16_t textY = bounds_.y + style_.padding.top;
    
    // Handle alignment
    int textWidth = text_.length() * charWidth;
    int availableWidth = bounds_.width - style_.padding.horizontal();
    
    switch (style_.textAlign) {
        case Align::CENTER:
            textX += (availableWidth - textWidth) / 2;
            break;
        case Align::END:
            textX += availableWidth - textWidth;
            break;
        default:
            break;
    }
    
    // Invert color if background is filled
    uint8_t color = style_.color.inverted ? 0 : 1;
    
    // Use appropriate text size
    gpu->oledText(textX, textY, text_.c_str(), scale, color);
}

//=============================================================================
// IconElement
//=============================================================================

inline void IconElement::renderContent(GpuCommands* gpu) {
    if (!gpu || icon_ == Icon::NONE) return;
    
    uint8_t color = style_.color.inverted ? 0 : 1;
    
    // Center icon in bounds
    int16_t iconX = bounds_.x + (bounds_.width - size_) / 2;
    int16_t iconY = bounds_.y + (bounds_.height - size_) / 2;
    
    // Draw icon based on type (simplified - you'd have actual icon bitmaps)
    // For now, draw a placeholder based on icon type
    switch (icon_) {
        case Icon::CHECK:
            gpu->oledLine(iconX + 2, iconY + size_/2, iconX + size_/3, iconY + size_ - 2, color);
            gpu->oledLine(iconX + size_/3, iconY + size_ - 2, iconX + size_ - 2, iconY + 2, color);
            break;
            
        case Icon::CLOSE:
        case Icon::ERROR:
            gpu->oledLine(iconX + 2, iconY + 2, iconX + size_ - 2, iconY + size_ - 2, color);
            gpu->oledLine(iconX + size_ - 2, iconY + 2, iconX + 2, iconY + size_ - 2, color);
            break;
            
        case Icon::ARROW_UP:
            gpu->oledLine(iconX + size_/2, iconY + 2, iconX + 2, iconY + size_ - 2, color);
            gpu->oledLine(iconX + size_/2, iconY + 2, iconX + size_ - 2, iconY + size_ - 2, color);
            break;
            
        case Icon::ARROW_DOWN:
            gpu->oledLine(iconX + 2, iconY + 2, iconX + size_/2, iconY + size_ - 2, color);
            gpu->oledLine(iconX + size_ - 2, iconY + 2, iconX + size_/2, iconY + size_ - 2, color);
            break;
            
        case Icon::ARROW_LEFT:
            gpu->oledLine(iconX + 2, iconY + size_/2, iconX + size_ - 2, iconY + 2, color);
            gpu->oledLine(iconX + 2, iconY + size_/2, iconX + size_ - 2, iconY + size_ - 2, color);
            break;
            
        case Icon::ARROW_RIGHT:
            gpu->oledLine(iconX + size_ - 2, iconY + size_/2, iconX + 2, iconY + 2, color);
            gpu->oledLine(iconX + size_ - 2, iconY + size_/2, iconX + 2, iconY + size_ - 2, color);
            break;
            
        case Icon::PLUS:
            gpu->oledLine(iconX + size_/2, iconY + 2, iconX + size_/2, iconY + size_ - 2, color);
            gpu->oledLine(iconX + 2, iconY + size_/2, iconX + size_ - 2, iconY + size_/2, color);
            break;
            
        case Icon::MINUS:
            gpu->oledLine(iconX + 2, iconY + size_/2, iconX + size_ - 2, iconY + size_/2, color);
            break;
            
        case Icon::CIRCLE:
            // Approximate circle with rectangle for now
            gpu->oledRect(iconX + 2, iconY + 2, size_ - 4, size_ - 4, color);
            break;
            
        case Icon::CIRCLE_FILLED:
            gpu->oledFill(iconX + 2, iconY + 2, size_ - 4, size_ - 4, color);
            break;
            
        default:
            // Draw a small box as placeholder
            gpu->oledRect(iconX + 2, iconY + 2, size_ - 4, size_ - 4, color);
            break;
    }
}

//=============================================================================
// ButtonElement
//=============================================================================

inline void ButtonElement::renderContent(GpuCommands* gpu) {
    if (!gpu) return;
    
    // Draw button background/border
    bool bg = pressed_ || style_.color.inverted;
    bool fg = !(pressed_ || style_.color.inverted);
    
    // Background
    if (bg) {
        gpu->oledFill(bounds_.x, bounds_.y, bounds_.width, bounds_.height, true);
    }
    
    // Border
    gpu->oledRect(bounds_.x, bounds_.y, bounds_.width, bounds_.height, fg);
    
    // Content positioning
    int scale = static_cast<int>(style_.textSize);
    int charWidth = 6 * scale;
    int charHeight = 8 * scale;
    int iconSpace = (icon_ != Icon::NONE) ? 10 : 0;
    int labelWidth = label_.length() * charWidth;
    int totalWidth = iconSpace + labelWidth;
    
    int16_t contentX = bounds_.x + (bounds_.width - totalWidth) / 2;
    int16_t contentY = bounds_.y + (bounds_.height - charHeight) / 2;
    
    // Draw icon
    if (icon_ != Icon::NONE && iconLeft_) {
        // Simple icon rendering
        gpu->oledRect(contentX, contentY, 8, 8, fg);
        contentX += iconSpace;
    }
    
    // Draw label
    if (!label_.empty()) {
        gpu->oledText(contentX, contentY, label_.c_str(), scale, fg);
    }
    
    // Draw icon on right
    if (icon_ != Icon::NONE && !iconLeft_) {
        gpu->oledRect(contentX + labelWidth + 2, contentY, 8, 8, fg);
    }
}

inline void ButtonElement::renderFocus(GpuCommands* gpu) {
    if (!gpu || !focused_) return;
    
    // Invert the button when focused
    gpu->oledFill(bounds_.x, bounds_.y, bounds_.width, bounds_.height, true);
    
    // Redraw content inverted
    int scale = static_cast<int>(style_.textSize);
    int charWidth = 6 * scale;
    int charHeight = 8 * scale;
    int labelWidth = label_.length() * charWidth;
    
    int16_t contentX = bounds_.x + (bounds_.width - labelWidth) / 2;
    int16_t contentY = bounds_.y + (bounds_.height - charHeight) / 2;
    
    gpu->oledText(contentX, contentY, label_.c_str(), scale, 0);  // Black text on white
}

//=============================================================================
// ListElement
//=============================================================================

inline void ListElement::renderContent(GpuCommands* gpu) {
    if (!gpu || items_.empty()) return;
    
    int16_t x = bounds_.x + style_.padding.left;
    int16_t y = bounds_.y + style_.padding.top;
    int16_t w = bounds_.width - style_.padding.horizontal();
    
    // Draw visible items
    for (int i = 0; i < visibleCount_ && (scrollOffset_ + i) < (int)items_.size(); i++) {
        int idx = scrollOffset_ + i;
        const auto& item = items_[idx];
        
        int16_t itemY = y + i * itemHeight_;
        bool selected = (idx == selectedIndex_);
        
        // Selection highlight
        if (selected && focused_) {
            gpu->oledFill(x, itemY, w, itemHeight_, true);
        }
        
        bool textColor = !(selected && focused_);
        
        // Icon
        int16_t textX = x + 2;
        if (item.icon != Icon::NONE) {
            // Draw small icon indicator
            gpu->oledRect(textX, itemY + 2, 8, 8, textColor);
            textX += 10;
        }
        
        // Primary text
        gpu->oledText(textX, itemY + 2, item.text.c_str(), 1, textColor);
        
        // Secondary text (smaller, below)
        if (!item.secondaryText.empty() && itemHeight_ >= 20) {
            gpu->oledText(textX, itemY + 10, item.secondaryText.c_str(), 1, textColor);
        }
    }
    
    // Scrollbar
    if (showScrollbar_ && items_.size() > (size_t)visibleCount_) {
        int16_t sbX = bounds_.x + bounds_.width - 3;
        int16_t sbY = bounds_.y + style_.padding.top;
        int16_t sbH = bounds_.height - style_.padding.vertical();
        
        // Track
        gpu->oledLine(sbX + 1, sbY, sbX + 1, sbY + sbH, 1);
        
        // Thumb
        float ratio = (float)visibleCount_ / items_.size();
        int16_t thumbH = std::max(4, (int)(sbH * ratio));
        float scrollRatio = (float)scrollOffset_ / (items_.size() - visibleCount_);
        int16_t thumbY = sbY + (int)((sbH - thumbH) * scrollRatio);
        
        gpu->oledFill(sbX, thumbY, 3, thumbH, true);
    }
}

inline void ListElement::renderFocus(GpuCommands* gpu) {
    // Focus is shown via selection highlight, handled in renderContent
}

//=============================================================================
// ProgressElement
//=============================================================================

inline void ProgressElement::renderContent(GpuCommands* gpu) {
    if (!gpu) return;
    
    int16_t x = bounds_.x;
    int16_t y = bounds_.y;
    int16_t w = bounds_.width;
    int16_t h = bounds_.height;
    
    switch (type_) {
        case ProgressType::BAR: {
            // Outline
            gpu->oledRect(x, y, w, h, true);
            
            // Fill based on progress
            if (!isIndeterminate()) {
                int fillW = (int)((w - 2) * getProgress());
                if (fillW > 0) {
                    gpu->oledFill(x + 1, y + 1, fillW, h - 2, true);
                }
            } else {
                // Animated segment
                int segW = w / 4;
                int pos = (animFrame_ * 2) % (w + segW) - segW;
                if (pos < 0) pos = 0;
                int segEnd = std::min(pos + segW, w - 2);
                if (pos < w - 2) {
                    gpu->oledFill(x + 1 + pos, y + 1, segEnd - pos, h - 2, true);
                }
            }
            break;
        }
        
        case ProgressType::BAR_VERTICAL: {
            gpu->oledRect(x, y, w, h, true);
            
            if (!isIndeterminate()) {
                int fillH = (int)((h - 2) * getProgress());
                if (fillH > 0) {
                    gpu->oledFill(x + 1, y + h - 1 - fillH, w - 2, fillH, true);
                }
            }
            break;
        }
        
        case ProgressType::SPINNER: {
            // Simple rotating line
            int cx = x + w / 2;
            int cy = y + h / 2;
            int r = std::min(w, h) / 2 - 2;
            
            // Draw 8 spokes with varying intensity (using dots for simplicity)
            for (int i = 0; i < 8; i++) {
                int angle = (animFrame_ + i * 45) % 360;
                float rad = angle * 3.14159f / 180.0f;
                int ex = cx + (int)(r * cos(rad));
                int ey = cy + (int)(r * sin(rad));
                
                // Fade based on position in rotation
                if ((animFrame_ / 4) % 8 == i || ((animFrame_ / 4) % 8 + 1) % 8 == i) {
                    gpu->oledLine(cx, cy, ex, ey, 1);
                } else {
                    gpu->oledPixel(ex, ey, 1);
                }
            }
            break;
        }
        
        case ProgressType::DOTS: {
            // Three animated dots
            int dotR = 2;
            int spacing = 8;
            int baseX = x + (w - 3 * spacing) / 2;
            
            for (int i = 0; i < 3; i++) {
                int dotX = baseX + i * spacing;
                int dotY = y + h / 2;
                
                // Animate vertical position
                int phase = (animFrame_ / 8 + i) % 3;
                if (phase == 0) dotY -= 2;
                
                gpu->oledFill(dotX - dotR, dotY - dotR, dotR * 2, dotR * 2, true);
            }
            break;
        }
        
        case ProgressType::CIRCLE: {
            // Circular progress (approximate with arc)
            int cx = x + w / 2;
            int cy = y + h / 2;
            int r = std::min(w, h) / 2 - 2;
            
            // Draw circle outline
            gpu->oledCircle(cx, cy, r, true);
            
            if (!isIndeterminate()) {
                // Draw filled arc based on progress
                // Simplified: draw dots along progress
                int dots = (int)(16 * getProgress());
                for (int i = 0; i < dots; i++) {
                    float angle = -90 + (360.0f * i / 16);  // Start from top
                    float rad = angle * 3.14159f / 180.0f;
                    int px = cx + (int)((r - 2) * cos(rad));
                    int py = cy + (int)((r - 2) * sin(rad));
                    gpu->oledPixel(px, py, 1);
                }
            }
            break;
        }
    }
    
    // Label
    if (showLabel_ && type_ == ProgressType::BAR) {
        char buf[8];
        snprintf(buf, sizeof(buf), labelFormat_.c_str(), getPercent());
        
        int labelW = strlen(buf) * 6;
        int labelX = x + (w - labelW) / 2;
        int labelY = y + (h - 8) / 2;
        
        // Draw with XOR effect or after progress fill
        gpu->oledText(labelX, labelY, buf, 1, 1);
    }
}

//=============================================================================
// DividerElement
//=============================================================================

inline void DividerElement::renderContent(GpuCommands* gpu) {
    if (!gpu) return;
    
    int16_t x = bounds_.x + style_.margin.left;
    int16_t y = bounds_.y + style_.margin.top;
    int16_t w = bounds_.width - style_.margin.horizontal();
    int16_t h = bounds_.height - style_.margin.vertical();
    
    if (horizontal_) {
        int16_t lineY = y + h / 2;
        
        if (dashed_) {
            for (int16_t i = 0; i < w; i += 4) {
                int16_t segLen = std::min(2, w - i);
                gpu->oledLine(x + i, lineY, x + i + segLen, lineY, 1);
            }
        } else {
            for (int16_t t = 0; t < thickness_; t++) {
                gpu->oledLine(x, lineY + t, x + w - 1, lineY + t, 1);
            }
        }
    } else {
        int16_t lineX = x + w / 2;
        
        if (dashed_) {
            for (int16_t i = 0; i < h; i += 4) {
                int16_t segLen = std::min(2, h - i);
                gpu->oledLine(lineX, y + i, lineX, y + i + segLen, 1);
            }
        } else {
            for (int16_t t = 0; t < thickness_; t++) {
                gpu->oledLine(lineX + t, y, lineX + t, y + h - 1, 1);
            }
        }
    }
}

//=============================================================================
// StatusBar Widget
//=============================================================================

inline void StatusBar::renderContent(GpuCommands* gpu) {
    if (!gpu) return;
    
    int16_t x = bounds_.x + style_.padding.left;
    int16_t y = bounds_.y + style_.padding.top;
    int16_t w = bounds_.width - style_.padding.horizontal();
    int16_t h = bounds_.height - style_.padding.vertical();
    
    // Left items
    int16_t leftX = x;
    for (const auto& item : leftItems_) {
        if (!item.visible) continue;
        
        if (item.icon != Icon::NONE) {
            // Small icon
            gpu->oledRect(leftX, y + 1, 8, 8, true);
            leftX += 10;
        }
        
        if (!item.text.empty()) {
            gpu->oledText(leftX, y + 2, item.text.c_str(), 1, 1);
            leftX += item.text.length() * 6 + 2;
        }
    }
    
    // Title (centered)
    if (!title_.empty()) {
        int titleW = title_.length() * 6;
        int titleX = x + (w - titleW) / 2;
        gpu->oledText(titleX, y + 2, title_.c_str(), 1, 1);
    }
    
    // Right items (right-aligned)
    int16_t rightX = x + w;
    for (auto it = rightItems_.rbegin(); it != rightItems_.rend(); ++it) {
        if (!it->visible) continue;
        
        if (!it->text.empty()) {
            int textW = it->text.length() * 6;
            rightX -= textW;
            gpu->oledText(rightX, y + 2, it->text.c_str(), 1, 1);
            rightX -= 2;
        }
        
        if (it->icon != Icon::NONE) {
            rightX -= 8;
            gpu->oledRect(rightX, y + 1, 8, 8, true);
            rightX -= 2;
        }
    }
    
    // Divider line
    if (showDivider_) {
        gpu->oledLine(bounds_.x, bounds_.y + bounds_.height - 1,
                     bounds_.x + bounds_.width - 1, bounds_.y + bounds_.height - 1, 1);
    }
}

//=============================================================================
// Menu Widget
//=============================================================================

inline void Menu::renderContent(GpuCommands* gpu) {
    if (!gpu) return;
    
    int16_t x = bounds_.x + style_.padding.left;
    int16_t y = bounds_.y + style_.padding.top;
    int16_t w = bounds_.width - style_.padding.horizontal();
    
    // Title
    if (showTitle_ && !title_.empty()) {
        gpu->oledText(x, y, title_.c_str(), 1, 1);
        gpu->oledLine(x, y + 10, x + w, y + 10, 1);
        y += 14;
    }
    
    // Menu items
    for (int i = 0; i < visibleCount_ && (scrollOffset_ + i) < (int)items_.size(); i++) {
        int idx = scrollOffset_ + i;
        const auto& item = items_[idx];
        int16_t itemY = y + i * itemHeight_;
        
        if (item.separator) {
            // Draw separator line
            gpu->oledLine(x, itemY + itemHeight_/2, x + w, itemY + itemHeight_/2, 1);
            continue;
        }
        
        bool selected = (idx == selectedIndex_);
        
        // Selection highlight
        if (selected && focused_) {
            gpu->oledFill(x, itemY, w, itemHeight_, true);
        }
        
        bool textColor = !(selected && focused_);
        bool dimColor = (!item.enabled) ? true : textColor;  // Could use dithered for disabled
        
        int16_t textX = x + 2;
        
        // Icon
        if (item.icon != Icon::NONE) {
            gpu->oledRect(textX, itemY + 3, 8, 8, dimColor);
            textX += 10;
        }
        
        // Label
        gpu->oledText(textX, itemY + 3, item.label.c_str(), 1, dimColor);
        
        // Right side content
        int16_t rightX = x + w - 2;
        
        // Submenu indicator
        if (!item.submenuId.empty()) {
            gpu->oledText(rightX - 6, itemY + 3, ">", 1, textColor);
        }
        
        // Toggle indicator
        if (item.isToggle) {
            int16_t toggleX = rightX - 12;
            gpu->oledRect(toggleX, itemY + 4, 10, 6, textColor);
            if (item.toggleValue) {
                gpu->oledFill(toggleX + 5, itemY + 5, 4, 4, textColor);
            } else {
                gpu->oledFill(toggleX + 1, itemY + 5, 4, 4, textColor);
            }
        }
        
        // Choice value
        if (item.isChoice && !item.choices.empty()) {
            const auto& choice = item.choices[item.choiceIndex];
            int choiceW = choice.length() * 6;
            gpu->oledText(rightX - choiceW, itemY + 3, choice.c_str(), 1, textColor);
        }
    }
    
    // Scroll indicators
    if (items_.size() > (size_t)visibleCount_) {
        if (scrollOffset_ > 0) {
            // Up arrow
            gpu->oledText(x + w - 6, bounds_.y + style_.padding.top, "^", 1, 1);
        }
        if (scrollOffset_ + visibleCount_ < (int)items_.size()) {
            // Down arrow
            gpu->oledText(x + w - 6, bounds_.y + bounds_.height - 10, "v", 1, 1);
        }
    }
}

//=============================================================================
// Dialog Widget
//=============================================================================

inline void Dialog::renderContent(GpuCommands* gpu) {
    if (!gpu) return;
    
    // Semi-transparent overlay (dither pattern)
    // For OLED, just dim the background or skip
    
    // Dialog box dimensions
    int16_t dialogW = bounds_.width - 16;
    int16_t dialogH = 60;  // Fixed height for now
    int16_t dialogX = (bounds_.width - dialogW) / 2;
    int16_t dialogY = (bounds_.height - dialogH) / 2;
    
    // Apply animation
    if (animProgress_ < 1.0f) {
        float scale = 0.5f + 0.5f * animProgress_;
        int16_t animW = (int16_t)(dialogW * scale);
        int16_t animH = (int16_t)(dialogH * scale);
        dialogX = (bounds_.width - animW) / 2;
        dialogY = (bounds_.height - animH) / 2;
        dialogW = animW;
        dialogH = animH;
    }
    
    // Clear background
    gpu->oledFill(dialogX - 2, dialogY - 2, dialogW + 4, dialogH + 4, false);
    
    // Border
    gpu->oledRect(dialogX, dialogY, dialogW, dialogH, true);
    
    // Title bar
    if (!title_.empty()) {
        gpu->oledFill(dialogX, dialogY, dialogW, 12, true);
        
        // Icon
        int16_t titleX = dialogX + 2;
        if (icon_ != Icon::NONE) {
            gpu->oledRect(titleX, dialogY + 2, 8, 8, false);
            titleX += 10;
        }
        
        gpu->oledText(titleX, dialogY + 2, title_.c_str(), 1, 0);
    }
    
    // Message
    if (!message_.empty()) {
        int16_t msgY = dialogY + (title_.empty() ? 4 : 16);
        
        // Simple word wrap
        int16_t msgX = dialogX + 4;
        int16_t maxW = dialogW - 8;
        int charsPerLine = maxW / 6;
        
        std::string line;
        int16_t lineY = msgY;
        
        for (size_t i = 0; i < message_.length(); i++) {
            line += message_[i];
            
            if ((int)line.length() >= charsPerLine || i == message_.length() - 1) {
                gpu->oledText(msgX, lineY, line.c_str(), 1, 1);
                lineY += 10;
                line.clear();
            }
        }
    }
    
    // Buttons
    if (!buttons_.empty()) {
        int16_t btnY = dialogY + dialogH - 14;
        int16_t totalBtnW = 0;
        
        for (const auto& btn : buttons_) {
            totalBtnW += btn.label.length() * 6 + 8;
        }
        totalBtnW += (buttons_.size() - 1) * 4;  // Spacing
        
        int16_t btnX = dialogX + (dialogW - totalBtnW) / 2;
        
        for (size_t i = 0; i < buttons_.size(); i++) {
            const auto& btn = buttons_[i];
            int16_t btnW = btn.label.length() * 6 + 6;
            
            bool selected = ((int)i == selectedButton_);
            
            if (selected) {
                gpu->oledFill(btnX, btnY, btnW, 12, true);
                gpu->oledText(btnX + 3, btnY + 2, btn.label.c_str(), 1, false);
            } else {
                gpu->oledRect(btnX, btnY, btnW, 12, true);
                gpu->oledText(btnX + 3, btnY + 2, btn.label.c_str(), 1, true);
            }
            
            btnX += btnW + 4;
        }
    }
}

//=============================================================================
// Toast Widget
//=============================================================================

inline void Toast::renderContent(GpuCommands* gpu) {
    if (!gpu || !showing_) return;
    
    // Apply fade animation
    if (animProgress_ < 0.3f) return;  // Don't draw if too faded
    
    // Toast box
    gpu->oledFill(bounds_.x - 1, bounds_.y - 1, bounds_.width + 2, bounds_.height + 2, false);
    gpu->oledFill(bounds_.x, bounds_.y, bounds_.width, bounds_.height, true);
    
    // Content
    int16_t contentX = bounds_.x + 4;
    int16_t contentY = bounds_.y + (bounds_.height - 8) / 2;
    
    // Icon
    if (current_.icon != Icon::NONE) {
        gpu->oledRect(contentX, contentY, 8, 8, false);
        contentX += 10;
    }
    
    // Text
    gpu->oledText(contentX, contentY, current_.text.c_str(), 1, 0);
}

} // namespace OledUI
