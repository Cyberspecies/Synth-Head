/**
 * @file Navigator.hpp
 * @brief Navigation system for OLED UI (like JS Router)
 * 
 * The Navigator manages page navigation, history, and transitions.
 * Similar to a web router or mobile navigation stack.
 */

#pragma once

#include "Types.hpp"
#include "Page.hpp"
#include <vector>
#include <stack>
#include <unordered_map>
#include <memory>
#include <functional>

// Forward declaration
class GpuCommands;

namespace OledUI {

//=============================================================================
// Navigator - Page navigation and routing (like JS Router)
//=============================================================================
class Navigator {
public:
    // Navigation callback types
    using BeforeNavigateCallback = std::function<bool(const std::string& from, const std::string& to)>;
    using AfterNavigateCallback = std::function<void(const std::string& pageId)>;
    
private:
    // Page registry
    std::unordered_map<std::string, Page::Ptr> pages_;
    
    // Navigation state
    Page::Ptr currentPage_ = nullptr;
    std::stack<std::string> history_;
    std::string homePage_;
    
    // Transition state
    Transition currentTransition_ = Transition::NONE;
    float transitionProgress_ = 0.0f;
    Page::Ptr transitionFromPage_ = nullptr;
    
    // Callbacks
    BeforeNavigateCallback beforeNavigate_;
    AfterNavigateCallback afterNavigate_;
    
    // Input state
    InputEvent lastInput_ = InputEvent::NONE;
    
public:
    //-------------------------------------------------------------------------
    // Constructor
    //-------------------------------------------------------------------------
    Navigator() = default;
    
    //-------------------------------------------------------------------------
    // Page Registration
    //-------------------------------------------------------------------------
    
    /**
     * Register a page with the navigator
     * @param page Page to register
     * @param isHome Set as home page
     */
    Navigator& registerPage(Page::Ptr page, bool isHome = false) {
        if (page) {
            pages_[page->getId()] = page;
            if (isHome || homePage_.empty()) {
                homePage_ = page->getId();
            }
        }
        return *this;
    }
    
    /**
     * Register multiple pages
     */
    Navigator& registerPages(std::initializer_list<Page::Ptr> pageList) {
        for (auto& page : pageList) {
            registerPage(page);
        }
        return *this;
    }
    
    /**
     * Get registered page by ID
     */
    Page::Ptr getPage(const std::string& id) const {
        auto it = pages_.find(id);
        return (it != pages_.end()) ? it->second : nullptr;
    }
    
    /**
     * Get current active page
     */
    Page::Ptr getCurrentPage() const { return currentPage_; }
    
    /**
     * Set home page
     */
    Navigator& setHomePage(const std::string& id) {
        homePage_ = id;
        return *this;
    }
    
    //-------------------------------------------------------------------------
    // Navigation
    //-------------------------------------------------------------------------
    
    /**
     * Navigate to a page by ID
     * @param pageId Page ID to navigate to
     * @param transition Transition animation
     * @param addToHistory Add to navigation history
     * @return true if navigation succeeded
     */
    bool navigate(const std::string& pageId, 
                  Transition transition = Transition::NONE,
                  bool addToHistory = true) {
        
        // Find page
        auto it = pages_.find(pageId);
        if (it == pages_.end()) {
            return false;  // Page not found
        }
        
        Page::Ptr newPage = it->second;
        
        // Check before navigate callback
        if (beforeNavigate_) {
            std::string fromId = currentPage_ ? currentPage_->getId() : "";
            if (!beforeNavigate_(fromId, pageId)) {
                return false;  // Navigation cancelled
            }
        }
        
        // Exit current page
        if (currentPage_) {
            currentPage_->triggerExit();
            
            if (addToHistory) {
                history_.push(currentPage_->getId());
            }
        }
        
        // Start transition
        currentTransition_ = transition;
        transitionProgress_ = 0.0f;
        transitionFromPage_ = currentPage_;
        
        // Enter new page
        currentPage_ = newPage;
        currentPage_->triggerEnter();
        
        // After navigate callback
        if (afterNavigate_) {
            afterNavigate_(pageId);
        }
        
        return true;
    }
    
    /**
     * Navigate to home page
     */
    bool home(Transition transition = Transition::NONE) {
        // Clear history
        while (!history_.empty()) history_.pop();
        return navigate(homePage_, transition, false);
    }
    
    /**
     * Navigate back in history
     */
    bool back(Transition transition = Transition::SLIDE_RIGHT) {
        if (history_.empty()) {
            return false;  // No history
        }
        
        std::string prevId = history_.top();
        history_.pop();
        return navigate(prevId, transition, false);
    }
    
    /**
     * Check if can go back
     */
    bool canGoBack() const { return !history_.empty(); }
    
    /**
     * Get history depth
     */
    size_t getHistoryDepth() const { return history_.size(); }
    
    /**
     * Clear navigation history
     */
    void clearHistory() {
        while (!history_.empty()) history_.pop();
    }
    
    //-------------------------------------------------------------------------
    // Callbacks
    //-------------------------------------------------------------------------
    
    Navigator& onBeforeNavigate(BeforeNavigateCallback cb) {
        beforeNavigate_ = cb;
        return *this;
    }
    
    Navigator& onAfterNavigate(AfterNavigateCallback cb) {
        afterNavigate_ = cb;
        return *this;
    }
    
    //-------------------------------------------------------------------------
    // Input Handling
    //-------------------------------------------------------------------------
    
    /**
     * Process input event
     * @param event Input event
     * @return true if event was handled
     */
    bool handleInput(InputEvent event) {
        lastInput_ = event;
        
        // Handle back button globally
        if (event == InputEvent::BACK) {
            if (canGoBack()) {
                back();
                return true;
            }
        }
        
        // Pass to current page
        if (currentPage_) {
            return currentPage_->handleInput(event);
        }
        
        return false;
    }
    
    //-------------------------------------------------------------------------
    // Update & Render
    //-------------------------------------------------------------------------
    
    /**
     * Update navigation state
     * @param deltaMs Time since last update in milliseconds
     */
    void update(uint32_t deltaMs) {
        // Update transition
        if (currentTransition_ != Transition::NONE) {
            transitionProgress_ += deltaMs / 200.0f;  // 200ms transition
            if (transitionProgress_ >= 1.0f) {
                transitionProgress_ = 1.0f;
                currentTransition_ = Transition::NONE;
                transitionFromPage_ = nullptr;
            }
        }
        
        // Update current page
        if (currentPage_) {
            currentPage_->triggerUpdate();
        }
    }
    
    /**
     * Render current page
     * @param gpu GPU command interface
     */
    void render(GpuCommands* gpu) {
        if (!gpu) return;
        
        // Handle transition rendering
        if (currentTransition_ != Transition::NONE && transitionFromPage_) {
            renderTransition(gpu);
        } else if (currentPage_) {
            currentPage_->render(gpu);
        }
    }
    
private:
    void renderTransition(GpuCommands* gpu) {
        // Simple transition rendering
        // For now, just render the new page
        // TODO: Implement actual transition animations
        if (currentPage_) {
            currentPage_->render(gpu);
        }
    }
};

//=============================================================================
// Route - Named route with optional parameters (like URL routes)
//=============================================================================
struct Route {
    std::string path;           // Route path (e.g., "/settings/wifi")
    std::string pageId;         // Target page ID
    std::unordered_map<std::string, std::string> params;  // Route parameters
    
    Route() = default;
    Route(const std::string& p, const std::string& pid) 
        : path(p), pageId(pid) {}
};

//=============================================================================
// Router - URL-like routing (advanced navigation)
//=============================================================================
class Router {
private:
    Navigator& navigator_;
    std::vector<Route> routes_;
    
public:
    Router(Navigator& nav) : navigator_(nav) {}
    
    /**
     * Add a route
     */
    Router& addRoute(const std::string& path, const std::string& pageId) {
        routes_.push_back(Route(path, pageId));
        return *this;
    }
    
    /**
     * Navigate by path
     */
    bool push(const std::string& path, Transition transition = Transition::SLIDE_LEFT) {
        // Find matching route
        for (const auto& route : routes_) {
            if (route.path == path) {
                return navigator_.navigate(route.pageId, transition);
            }
        }
        return false;
    }
    
    /**
     * Go back
     */
    bool pop() {
        return navigator_.back();
    }
    
    /**
     * Go to home
     */
    bool popToRoot() {
        return navigator_.home();
    }
};

} // namespace OledUI
