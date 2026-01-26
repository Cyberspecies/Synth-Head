/**
 * @file OledUI.hpp
 * @brief Main aggregator header for OLED UI Framework
 * 
 * Include this single header to get access to the entire OLED UI framework.
 * 
 * @example Basic Usage:
 * @code
 * #include "FrameworkAPI/OledUI/OledUI.hpp"
 * 
 * using namespace OledUI;
 * 
 * // Create a simple page
 * auto homePage = PageBuilder("home")
 *     .title("Home")
 *     .content(
 *         Column(4)
 *             ->add(Title("Welcome"))
 *             ->add(Text("Hello World!"))
 *             ->add(Button("Settings", []() { 
 *                 // Navigate to settings 
 *             }))
 *     )
 *     .build();
 * 
 * // Create navigator
 * Navigator nav;
 * nav.registerPage(homePage);
 * nav.navigate("home");
 * 
 * // In update loop:
 * nav.update();
 * nav.render(&gpu);
 * @endcode
 * 
 * @example Menu Page:
 * @code
 * auto menuPage = PageBuilder("menu")
 *     .title("Settings")
 *     .content(
 *         CreateMenu("Settings")
 *             ->addItem(MenuItem(Icon::WIFI, "WiFi", []() { }))
 *             ->addItem(MenuItem(Icon::BLUETOOTH, "Bluetooth", []() { }))
 *             ->addItem(MenuItem::Toggle("Sound", true))
 *             ->addSeparator()
 *             ->addItem(MenuItem("About", "about"))
 *     )
 *     .build();
 * @endcode
 */

#pragma once

//-----------------------------------------------------------------------------
// Core Components
//-----------------------------------------------------------------------------
#include "Core/Types.hpp"
#include "Core/Style.hpp"
#include "Core/Element.hpp"
#include "Core/Page.hpp"
#include "Core/Navigator.hpp"

//-----------------------------------------------------------------------------
// UI Elements
//-----------------------------------------------------------------------------
#include "Elements/TextElement.hpp"
#include "Elements/IconElement.hpp"
#include "Elements/ButtonElement.hpp"
#include "Elements/ContainerElement.hpp"
#include "Elements/ListElement.hpp"
#include "Elements/ProgressElement.hpp"
#include "Elements/DividerElement.hpp"
#include "Elements/SpacerElement.hpp"

//-----------------------------------------------------------------------------
// Widgets
//-----------------------------------------------------------------------------
#include "Widgets/StatusBar.hpp"
#include "Widgets/Menu.hpp"
#include "Widgets/Dialog.hpp"
#include "Widgets/Toast.hpp"

//-----------------------------------------------------------------------------
// Rendering (include in ONE .cpp file only)
//-----------------------------------------------------------------------------
// Note: Include "Rendering/Renderer.hpp" separately in your implementation file

namespace OledUI {

/**
 * @brief Quick-start helper to create a basic app structure
 * 
 * Creates a navigator with a status bar and content area.
 */
class OledApp {
private:
    Navigator navigator_;
    std::shared_ptr<StatusBar> statusBar_;
    std::shared_ptr<Dialog> dialog_;
    std::shared_ptr<Toast> toast_;
    GpuCommands* gpu_ = nullptr;
    
public:
    OledApp() {
        statusBar_ = CreateStatusBar();
        dialog_ = CreateDialog();
        toast_ = CreateToast();
    }
    
    //-------------------------------------------------------------------------
    // Setup
    //-------------------------------------------------------------------------
    OledApp& setGpu(GpuCommands* gpu) { gpu_ = gpu; return *this; }
    
    //-------------------------------------------------------------------------
    // Pages
    //-------------------------------------------------------------------------
    OledApp& addPage(std::shared_ptr<Page> page) {
        navigator_.registerPage(page);
        return *this;
    }
    
    OledApp& setHomePage(const std::string& id) {
        navigator_.setHomePage(id);
        return *this;
    }
    
    OledApp& navigate(const std::string& pageId) {
        navigator_.navigate(pageId);
        return *this;
    }
    
    OledApp& back() {
        navigator_.back();
        return *this;
    }
    
    OledApp& home() {
        navigator_.home();
        return *this;
    }
    
    Navigator& navigator() { return navigator_; }
    
    //-------------------------------------------------------------------------
    // Status Bar
    //-------------------------------------------------------------------------
    StatusBar& statusBar() { return *statusBar_; }
    
    OledApp& setTitle(const std::string& title) {
        statusBar_->setTitle(title);
        return *this;
    }
    
    OledApp& showStatusBar(bool show = true) {
        statusBar_->setVisible(show);
        return *this;
    }
    
    //-------------------------------------------------------------------------
    // Dialog
    //-------------------------------------------------------------------------
    Dialog& dialog() { return *dialog_; }
    
    OledApp& showInfo(const std::string& title, const std::string& msg) {
        dialog_->info(title, msg).show();
        return *this;
    }
    
    OledApp& showConfirm(const std::string& title, const std::string& msg,
                        Callback onYes, Callback onNo = nullptr) {
        dialog_->confirm(title, msg, onYes, onNo).show();
        return *this;
    }
    
    OledApp& hideDialog() {
        dialog_->hide();
        return *this;
    }
    
    //-------------------------------------------------------------------------
    // Toast
    //-------------------------------------------------------------------------
    Toast& toast() { return *toast_; }
    
    OledApp& showToast(const std::string& msg) {
        toast_->show(msg);
        return *this;
    }
    
    OledApp& showToast(const std::string& msg, Icon icon) {
        toast_->show(msg, icon);
        return *this;
    }
    
    //-------------------------------------------------------------------------
    // Input Handling
    //-------------------------------------------------------------------------
    bool handleInput(InputEvent event) {
        // Dialog takes priority
        if (dialog_->isShowing()) {
            return dialog_->handleInput(event);
        }
        
        // Then current page
        return navigator_.handleInput(event);
    }
    
    //-------------------------------------------------------------------------
    // Update & Render
    //-------------------------------------------------------------------------
    void update(uint32_t currentTimeMs = 0) {
        navigator_.update();
        
        if (currentTimeMs > 0) {
            dialog_->tick(0.016f);  // ~60fps
            toast_->tick(currentTimeMs);
        }
    }
    
    void render() {
        if (!gpu_) return;
        
        // Clear screen
        gpu_->oledClear();
        
        // Calculate content area (below status bar if visible)
        int16_t contentY = 0;
        int16_t contentH = OLED_HEIGHT;
        
        if (statusBar_->isVisible()) {
            // Layout and render status bar
            Rect sbBounds{0, 0, OLED_WIDTH, 12};
            statusBar_->layout(sbBounds);
            statusBar_->render(gpu_);
            
            contentY = 12;
            contentH -= 12;
        }
        
        // Render current page
        navigator_.render(gpu_, 0, contentY, OLED_WIDTH, contentH);
        
        // Render toast on top
        if (toast_->isShowing()) {
            toast_->layout({0, 0, OLED_WIDTH, OLED_HEIGHT});
            toast_->render(gpu_);
        }
        
        // Render dialog on top of everything
        if (dialog_->isShowing()) {
            dialog_->layout({0, 0, OLED_WIDTH, OLED_HEIGHT});
            dialog_->render(gpu_);
        }
        
        // Present to display
        gpu_->oledPresent();
    }
};

} // namespace OledUI
