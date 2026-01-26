/**
 * @file OledUI_Example.hpp
 * @brief Example usage of the OLED UI Framework
 * 
 * This file demonstrates how to create pages, menus, and handle navigation
 * using the HTML/CSS/JS-like OLED UI framework.
 */

#pragma once

#include "OledUI.hpp"
#include "Rendering/Renderer.hpp"  // Include ONCE for rendering implementations
#include "GpuDriver/GpuCommands.hpp"

namespace OledUI {
namespace Examples {

/**
 * @brief Create a simple home page with title and menu
 */
inline std::shared_ptr<Page> createHomePage(Navigator& nav) {
    return PageBuilder("home")
        .title("Home")
        .content(
            Column(4)
                ->add(Title("Welcome!"))
                ->add(Text("OLED UI Demo"))
                ->add(Divider())
                ->add(Spacer())
                ->add(Button("Settings", [&nav]() {
                    nav.navigate("settings");
                }))
                ->add(Button("About", [&nav]() {
                    nav.navigate("about");
                }))
        )
        .build();
}

/**
 * @brief Create a settings menu page
 */
inline std::shared_ptr<Page> createSettingsPage(Navigator& nav) {
    auto menu = CreateMenu("Settings");
    
    // Add menu items
    menu->addItem(MenuItem(Icon::WIFI, "WiFi", [&nav]() {
        nav.navigate("wifi");
    }));
    
    menu->addItem(MenuItem(Icon::BLUETOOTH, "Bluetooth", [&nav]() {
        nav.navigate("bluetooth");
    }));
    
    menu->addSeparator();
    
    // Toggle item
    static bool soundEnabled = true;
    menu->addItem(MenuItem::Toggle("Sound", soundEnabled, [](bool value) {
        soundEnabled = value;
        // Apply sound setting
    }));
    
    // Choice item  
    static int brightnessLevel = 1;
    menu->addItem(MenuItem::Choice("Brightness", 
        {"Low", "Medium", "High"}, 
        brightnessLevel,
        [](int index) {
            brightnessLevel = index;
            // Apply brightness
        }
    ));
    
    menu->addSeparator();
    menu->addItem(MenuItem("About", "about"));
    
    menu->setOnBack([&nav]() {
        nav.back();
    });
    
    menu->setOnNavigate([&nav](const std::string& pageId) {
        nav.navigate(pageId);
    });
    
    return PageBuilder("settings")
        .title("Settings")
        .content(menu)
        .build();
}

/**
 * @brief Create an about page
 */
inline std::shared_ptr<Page> createAboutPage(Navigator& nav) {
    return PageBuilder("about")
        .title("About")
        .content(
            Column(2)
                ->add(Text("OLED UI Framework"))
                ->add(Caption("v1.0.0"))
                ->add(Spacer())
                ->add(Text("A modular UI system"))
                ->add(Text("for OLED displays"))
                ->add(Spacer())
                ->add(Button("Back", [&nav]() {
                    nav.back();
                }))
        )
        .onBack([&nav]() {
            nav.back();
        })
        .build();
}

/**
 * @brief Complete example app setup
 */
inline void setupExampleApp(OledApp& app, GpuCommands& gpu) {
    app.setGpu(&gpu);
    
    // Create navigation
    Navigator& nav = app.navigator();
    
    // Register pages
    app.addPage(createHomePage(nav));
    app.addPage(createSettingsPage(nav));
    app.addPage(createAboutPage(nav));
    
    // Set home page
    app.setHomePage("home");
    app.navigate("home");
    
    // Setup status bar
    app.statusBar()
        .setTitle("Demo App")
        .setWifi(true, 75)
        .setBattery(85);
}

/**
 * @brief Example main loop handler
 */
inline void exampleLoop(OledApp& app, uint32_t currentTimeMs) {
    // Handle input (in real app, read from buttons/encoder)
    // Example: app.handleInput(InputEvent::DOWN);
    // Example: app.handleInput(InputEvent::SELECT);
    
    // Update state
    app.update(currentTimeMs);
    
    // Render to display
    app.render();
}

/**
 * @brief Minimal example - just show text
 */
inline void minimalExample(GpuCommands& gpu) {
    // Create a single page app
    OledApp app;
    app.setGpu(&gpu);
    
    // Create simple page
    auto page = PageBuilder("main")
        .content(
            Center()
                ->add(Column(8)
                    ->add(Title("Hello!"))
                    ->add(Text("OLED UI"))
                )
        )
        .build();
    
    app.addPage(page);
    app.navigate("main");
    
    // Render once
    app.update(0);
    app.render();
}

/**
 * @brief Progress indicator example
 */
inline void progressExample(GpuCommands& gpu) {
    OledApp app;
    app.setGpu(&gpu);
    
    auto progress = ProgressBar(0.5f);
    auto spinner = Spinner();
    
    auto page = PageBuilder("progress")
        .content(
            Column(8)
                ->add(Title("Loading..."))
                ->add(progress)
                ->add(Text("50%"))
                ->add(Divider())
                ->add(Row(4)
                    ->add(Text("Working"))
                    ->add(spinner)
                )
        )
        .build();
    
    app.addPage(page);
    app.navigate("progress");
    
    // In update loop:
    // progress->setValue(newValue);
    // spinner->tick();
    app.render();
}

/**
 * @brief List example
 */
inline void listExample(GpuCommands& gpu, std::function<void(int)> onSelect) {
    OledApp app;
    app.setGpu(&gpu);
    
    auto list = List({
        ListItem(Icon::PLAY, "Play"),
        ListItem(Icon::PAUSE, "Pause"),
        ListItem(Icon::STOP, "Stop"),
        ListItem(Icon::SETTINGS, "Settings"),
    });
    
    list->setOnSelect([onSelect](int index, const ListItem& item) {
        onSelect(index);
    });
    
    auto page = PageBuilder("playlist")
        .title("Playlist")
        .content(list)
        .build();
    
    app.addPage(page);
    app.navigate("playlist");
    app.render();
}

/**
 * @brief Dialog example
 */
inline void dialogExample(OledApp& app) {
    // Show confirmation dialog
    app.showConfirm(
        "Delete?",
        "Delete this item?",
        []() {
            // User pressed Yes
        },
        []() {
            // User pressed No
        }
    );
}

/**
 * @brief Toast notification example
 */
inline void toastExample(OledApp& app) {
    // Show various toast types
    app.toast().info("Item saved");
    app.toast().success("Upload complete");
    app.toast().warning("Low battery");
    app.toast().error("Connection failed");
}

} // namespace Examples
} // namespace OledUI
