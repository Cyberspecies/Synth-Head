/**
 * @file Element.hpp
 * @brief Base element class for OLED UI (like HTML Element)
 * 
 * This is the base class for all UI elements. It provides:
 * - Style management (CSS-like)
 * - Layout calculation (measure/layout passes)
 * - Rendering interface
 * - Event handling
 * - Parent/child relationships
 */

#pragma once

#include "Types.hpp"
#include "Style.hpp"
#include <vector>
#include <memory>

// Forward declaration
class GpuCommands;

namespace OledUI {

// Forward declarations
class Page;
class Navigator;

//=============================================================================
// Element - Base class for all UI elements (like HTML Element)
//=============================================================================
class Element {
public:
    //-------------------------------------------------------------------------
    // Types
    //-------------------------------------------------------------------------
    using Ptr = std::shared_ptr<Element>;
    using WeakPtr = std::weak_ptr<Element>;
    
protected:
    //-------------------------------------------------------------------------
    // Core Properties
    //-------------------------------------------------------------------------
    ElementId id_;
    std::string tag_;           // Element type tag (like HTML tag)
    
    Style style_;               // CSS-like style
    Rect bounds_;               // Calculated bounds after layout
    Rect contentBounds_;        // Content area (bounds - padding)
    
    Element* parent_ = nullptr;
    std::vector<Ptr> children_;
    
    bool focused_ = false;
    bool dirty_ = true;         // Needs re-render
    
    //-------------------------------------------------------------------------
    // Event Callbacks
    //-------------------------------------------------------------------------
    OnClickCallback onClick_;
    OnFocusCallback onFocus_;
    
    //-------------------------------------------------------------------------
    // Static ID counter
    //-------------------------------------------------------------------------
    static ElementId nextId_;
    
public:
    //-------------------------------------------------------------------------
    // Constructor/Destructor
    //-------------------------------------------------------------------------
    Element(const std::string& tag = "div") 
        : id_(nextId_++), tag_(tag) {}
    
    virtual ~Element() = default;
    
    //-------------------------------------------------------------------------
    // Identity
    //-------------------------------------------------------------------------
    ElementId getId() const { return id_; }
    const std::string& getTag() const { return tag_; }
    
    //-------------------------------------------------------------------------
    // Style Management
    //-------------------------------------------------------------------------
    Style& style() { return style_; }
    const Style& style() const { return style_; }
    
    Element& setStyle(const Style& s) { 
        style_ = s; 
        markDirty();
        return *this; 
    }
    
    // Convenience style methods
    Element& setSize(int16_t w, int16_t h) { style_.setSize(w, h); markDirty(); return *this; }
    Element& setMargin(int16_t m) { style_.setMargin(m); markDirty(); return *this; }
    Element& setPadding(int16_t p) { style_.setPadding(p); markDirty(); return *this; }
    Element& setVisible(bool v) { style_.setVisible(v); markDirty(); return *this; }
    Element& setEnabled(bool e) { style_.setEnabled(e); markDirty(); return *this; }
    
    //-------------------------------------------------------------------------
    // Bounds
    //-------------------------------------------------------------------------
    const Rect& getBounds() const { return bounds_; }
    const Rect& getContentBounds() const { return contentBounds_; }
    
    //-------------------------------------------------------------------------
    // Parent/Child Management
    //-------------------------------------------------------------------------
    Element* getParent() const { return parent_; }
    const std::vector<Ptr>& getChildren() const { return children_; }
    
    Element& addChild(Ptr child) {
        if (child) {
            child->parent_ = this;
            children_.push_back(child);
            markDirty();
        }
        return *this;
    }
    
    Element& removeChild(ElementId id) {
        children_.erase(
            std::remove_if(children_.begin(), children_.end(),
                [id](const Ptr& c) { return c->getId() == id; }),
            children_.end()
        );
        markDirty();
        return *this;
    }
    
    void clearChildren() {
        children_.clear();
        markDirty();
    }
    
    // Find child by ID (recursive)
    Element* findById(ElementId id) {
        if (id_ == id) return this;
        for (auto& child : children_) {
            Element* found = child->findById(id);
            if (found) return found;
        }
        return nullptr;
    }
    
    //-------------------------------------------------------------------------
    // Focus Management
    //-------------------------------------------------------------------------
    bool isFocused() const { return focused_; }
    
    virtual void setFocused(bool f) {
        if (focused_ != f) {
            focused_ = f;
            markDirty();
            if (onFocus_) onFocus_(f);
        }
    }
    
    // Find next focusable element
    virtual Element* getNextFocusable() {
        // Default: return first focusable child
        for (auto& child : children_) {
            if (child->isFocusable()) return child.get();
            Element* next = child->getNextFocusable();
            if (next) return next;
        }
        return nullptr;
    }
    
    virtual bool isFocusable() const { 
        return style_.enabled && style_.visible; 
    }
    
    //-------------------------------------------------------------------------
    // Dirty State
    //-------------------------------------------------------------------------
    bool isDirty() const { return dirty_; }
    
    void markDirty() {
        dirty_ = true;
        // Propagate up
        if (parent_) parent_->markDirty();
    }
    
    void markClean() { dirty_ = false; }
    
    //-------------------------------------------------------------------------
    // Event Handling
    //-------------------------------------------------------------------------
    Element& onClick(OnClickCallback cb) { onClick_ = cb; return *this; }
    Element& onFocusChange(OnFocusCallback cb) { onFocus_ = cb; return *this; }
    
    virtual bool handleInput(InputEvent event) {
        if (!style_.enabled || !style_.visible) return false;
        
        if (event == InputEvent::SELECT && onClick_) {
            onClick_();
            return true;
        }
        
        // Pass to children
        for (auto& child : children_) {
            if (child->handleInput(event)) return true;
        }
        return false;
    }
    
    //-------------------------------------------------------------------------
    // Layout System (like CSS layout)
    //-------------------------------------------------------------------------
    
    /**
     * Measure pass - calculate preferred size
     * @param availableWidth Available width from parent
     * @param availableHeight Available height from parent
     * @return Preferred size (width, height)
     */
    virtual Rect measure(int16_t availableWidth, int16_t availableHeight) {
        Rect preferred;
        
        // Use explicit size if set
        preferred.width = (style_.width >= 0) ? style_.width : 0;
        preferred.height = (style_.height >= 0) ? style_.height : 0;
        
        // Add margin to preferred size
        preferred.width += style_.margin.horizontal();
        preferred.height += style_.margin.vertical();
        
        return preferred;
    }
    
    /**
     * Layout pass - position element and children
     * @param x Left position
     * @param y Top position
     * @param width Available width
     * @param height Available height
     */
    virtual void layout(int16_t x, int16_t y, int16_t width, int16_t height) {
        // Apply margin
        bounds_.x = x + style_.margin.left;
        bounds_.y = y + style_.margin.top;
        bounds_.width = width - style_.margin.horizontal();
        bounds_.height = height - style_.margin.vertical();
        
        // Constrain to min/max
        if (style_.minWidth > 0 && bounds_.width < style_.minWidth)
            bounds_.width = style_.minWidth;
        if (style_.minHeight > 0 && bounds_.height < style_.minHeight)
            bounds_.height = style_.minHeight;
        if (style_.maxWidth > 0 && bounds_.width > style_.maxWidth)
            bounds_.width = style_.maxWidth;
        if (style_.maxHeight > 0 && bounds_.height > style_.maxHeight)
            bounds_.height = style_.maxHeight;
        
        // Calculate content bounds (bounds - padding)
        contentBounds_.x = bounds_.x + style_.padding.left;
        contentBounds_.y = bounds_.y + style_.padding.top;
        contentBounds_.width = bounds_.width - style_.padding.horizontal();
        contentBounds_.height = bounds_.height - style_.padding.vertical();
    }
    
    //-------------------------------------------------------------------------
    // Rendering
    //-------------------------------------------------------------------------
    
    /**
     * Render the element to GPU
     * @param gpu GPU command interface
     */
    virtual void render(GpuCommands* gpu) {
        if (!style_.visible || !gpu) return;
        
        // Render background
        renderBackground(gpu);
        
        // Render content (override in subclasses)
        renderContent(gpu);
        
        // Render children
        for (auto& child : children_) {
            child->render(gpu);
        }
        
        // Render border
        if (style_.border) {
            renderBorder(gpu);
        }
        
        // Render focus indicator
        if (focused_) {
            renderFocus(gpu);
        }
        
        markClean();
    }
    
protected:
    virtual void renderBackground(GpuCommands* gpu);
    virtual void renderContent(GpuCommands* gpu) {} // Override in subclasses
    virtual void renderBorder(GpuCommands* gpu);
    virtual void renderFocus(GpuCommands* gpu);
};

// Static member initialization
inline ElementId Element::nextId_ = 0;

} // namespace OledUI
