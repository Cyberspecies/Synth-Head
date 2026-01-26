/**
 * @file Page.hpp
 * @brief Page container for OLED UI (like HTML document)
 * 
 * A Page represents a single screen/view in the UI. It contains:
 * - Root element tree
 * - Page lifecycle callbacks
 * - Layout management
 * - Focus tracking
 */

#pragma once

#include "Types.hpp"
#include "Style.hpp"
#include "Element.hpp"
#include <string>
#include <functional>
#include <memory>

namespace OledUI {

//=============================================================================
// Page - A single screen/view (like HTML document)
//=============================================================================
class Page {
public:
    using Ptr = std::shared_ptr<Page>;
    
    // Page lifecycle callbacks
    using LifecycleCallback = std::function<void(Page&)>;
    
private:
    std::string id_;            // Unique page identifier (like URL)
    std::string title_;         // Page title
    
    Element::Ptr root_;         // Root element tree
    Element* focusedElement_ = nullptr;
    
    bool dirty_ = true;
    bool mounted_ = false;      // Has been shown at least once
    
    // Lifecycle callbacks
    LifecycleCallback onMount_;     // Called first time page is shown
    LifecycleCallback onEnter_;     // Called each time page becomes active
    LifecycleCallback onExit_;      // Called when navigating away
    LifecycleCallback onUpdate_;    // Called each frame while active
    
    // Page-level style (background, etc.)
    Style pageStyle_;
    
public:
    //-------------------------------------------------------------------------
    // Constructor
    //-------------------------------------------------------------------------
    Page(const std::string& id, const std::string& title = "")
        : id_(id), title_(title.empty() ? id : title) {
        
        // Create root container
        root_ = std::make_shared<Element>("page-root");
        root_->style()
            .setSize(OLED_WIDTH, OLED_HEIGHT)
            .setPadding(0);
    }
    
    //-------------------------------------------------------------------------
    // Identity
    //-------------------------------------------------------------------------
    const std::string& getId() const { return id_; }
    const std::string& getTitle() const { return title_; }
    void setTitle(const std::string& t) { title_ = t; }
    
    //-------------------------------------------------------------------------
    // Root Element
    //-------------------------------------------------------------------------
    Element::Ptr getRoot() const { return root_; }
    
    // Add element to page root
    Page& add(Element::Ptr element) {
        root_->addChild(element);
        markDirty();
        return *this;
    }
    
    // Remove element from page
    Page& remove(ElementId id) {
        root_->removeChild(id);
        markDirty();
        return *this;
    }
    
    // Clear all elements
    Page& clear() {
        root_->clearChildren();
        focusedElement_ = nullptr;
        markDirty();
        return *this;
    }
    
    // Find element by ID
    Element* findById(ElementId id) {
        return root_->findById(id);
    }
    
    //-------------------------------------------------------------------------
    // Page Style
    //-------------------------------------------------------------------------
    Style& style() { return pageStyle_; }
    const Style& style() const { return pageStyle_; }
    
    //-------------------------------------------------------------------------
    // Focus Management
    //-------------------------------------------------------------------------
    Element* getFocusedElement() const { return focusedElement_; }
    
    void setFocus(Element* element) {
        if (focusedElement_ == element) return;
        
        if (focusedElement_) {
            focusedElement_->setFocused(false);
        }
        focusedElement_ = element;
        if (focusedElement_) {
            focusedElement_->setFocused(true);
        }
        markDirty();
    }
    
    void focusFirst() {
        Element* first = root_->getNextFocusable();
        setFocus(first);
    }
    
    void focusNext() {
        // Simple linear focus navigation
        // TODO: Implement proper 2D focus navigation
        if (!focusedElement_) {
            focusFirst();
            return;
        }
        
        Element* next = focusedElement_->getNextFocusable();
        if (next) {
            setFocus(next);
        }
    }
    
    void focusPrev() {
        // TODO: Implement reverse focus navigation
        focusFirst();
    }
    
    //-------------------------------------------------------------------------
    // Lifecycle Callbacks
    //-------------------------------------------------------------------------
    Page& onMount(LifecycleCallback cb) { onMount_ = cb; return *this; }
    Page& onEnter(LifecycleCallback cb) { onEnter_ = cb; return *this; }
    Page& onExit(LifecycleCallback cb) { onExit_ = cb; return *this; }
    Page& onUpdate(LifecycleCallback cb) { onUpdate_ = cb; return *this; }
    
    // Internal lifecycle triggers
    void triggerMount() {
        if (!mounted_) {
            mounted_ = true;
            if (onMount_) onMount_(*this);
        }
    }
    
    void triggerEnter() {
        triggerMount();  // Ensure mount is called first
        if (onEnter_) onEnter_(*this);
        focusFirst();    // Auto-focus first element
    }
    
    void triggerExit() {
        if (onExit_) onExit_(*this);
    }
    
    void triggerUpdate() {
        if (onUpdate_) onUpdate_(*this);
    }
    
    //-------------------------------------------------------------------------
    // Dirty State
    //-------------------------------------------------------------------------
    bool isDirty() const { return dirty_ || root_->isDirty(); }
    void markDirty() { dirty_ = true; }
    void markClean() { dirty_ = false; root_->markClean(); }
    
    //-------------------------------------------------------------------------
    // Input Handling
    //-------------------------------------------------------------------------
    bool handleInput(InputEvent event) {
        // Navigation events
        switch (event) {
            case InputEvent::DOWN:
                focusNext();
                return true;
            case InputEvent::UP:
                focusPrev();
                return true;
            default:
                break;
        }
        
        // Pass to focused element
        if (focusedElement_) {
            if (focusedElement_->handleInput(event)) {
                return true;
            }
        }
        
        // Pass to root
        return root_->handleInput(event);
    }
    
    //-------------------------------------------------------------------------
    // Layout
    //-------------------------------------------------------------------------
    void layout() {
        root_->layout(0, 0, OLED_WIDTH, OLED_HEIGHT);
    }
    
    //-------------------------------------------------------------------------
    // Rendering
    //-------------------------------------------------------------------------
    void render(GpuCommands* gpu) {
        if (!gpu) return;
        
        // Layout if needed
        if (isDirty()) {
            layout();
        }
        
        // Render root tree
        root_->render(gpu);
        
        markClean();
    }
};

//=============================================================================
// PageBuilder - Fluent API for building pages
//=============================================================================
class PageBuilder {
private:
    Page::Ptr page_;
    
public:
    PageBuilder(const std::string& id, const std::string& title = "")
        : page_(std::make_shared<Page>(id, title)) {}
    
    PageBuilder& add(Element::Ptr element) {
        page_->add(element);
        return *this;
    }
    
    PageBuilder& onMount(Page::LifecycleCallback cb) {
        page_->onMount(cb);
        return *this;
    }
    
    PageBuilder& onEnter(Page::LifecycleCallback cb) {
        page_->onEnter(cb);
        return *this;
    }
    
    PageBuilder& onExit(Page::LifecycleCallback cb) {
        page_->onExit(cb);
        return *this;
    }
    
    PageBuilder& onUpdate(Page::LifecycleCallback cb) {
        page_->onUpdate(cb);
        return *this;
    }
    
    Page::Ptr build() { return page_; }
    
    // Implicit conversion to Page::Ptr
    operator Page::Ptr() { return page_; }
};

} // namespace OledUI
