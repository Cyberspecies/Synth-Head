/*****************************************************************
 * @file OledUI.hpp
 * @brief OLED HUD User Interface Implementation
 * 
 * Complete UI implementation for the 128x128 OLED display using
 * the SystemAPI UI framework. Syncs with web captive portal.
 * 
 * @author ARCOS
 * @version 1.0
 *****************************************************************/

#pragma once

#include "SystemAPI/UI/UIManager.hpp"
#include "SystemAPI/UI/UIRenderer.hpp"
#include "SystemAPI/UI/UIContainer.hpp"
#include "SystemAPI/UI/UIText.hpp"
#include "SystemAPI/UI/UIButton.hpp"
#include "SystemAPI/UI/UISlider.hpp"
#include "SystemAPI/UI/UICheckbox.hpp"
#include "SystemAPI/UI/UIProgressBar.hpp"
#include "SystemAPI/UI/UIIcon.hpp"
#include "SystemAPI/UI/UIGrid.hpp"
#include "SystemAPI/UI/UIDropdown.hpp"
#include "SystemAPI/SyncState.hpp"

#include <functional>

namespace SystemAPI {
namespace UI {

/**
 * @brief OLED UI Manager - Creates and manages all UI scenes
 */
class OledUI {
public:
  static OledUI& instance() {
    static OledUI inst;
    return inst;
  }
  
  /**
   * @brief Initialize the OLED UI
   * @param width Display width (default 128)
   * @param height Display height (default 128)
   */
  bool init(uint16_t width = 128, uint16_t height = 128) {
    width_ = width;
    height_ = height;
    
    // Initialize UI Manager
    if (!UIManager::instance().init(width, height, BufferFormat::MONO_1BPP)) {
      return false;
    }
    
    // Create all scenes
    createMainScene();
    createStatusScene();
    createControlsScene();
    createSensorsScene();
    createSettingsScene();
    
    // Start with main scene
    UIManager::instance().pushScene(mainScene_);
    
    initialized_ = true;
    return true;
  }
  
  /**
   * @brief Update UI from sync state
   */
  void syncFromState() {
    auto& state = SYNC_STATE.state();
    
    // Update controls scene
    if (slider1_) slider1_->setValue(state.slider1Value);
    if (slider2_) slider2_->setValue(state.slider2Value);
    if (slider3_) slider3_->setValue(state.slider3Value);
    
    if (toggle1_) toggle1_->setOn(state.toggle1);
    if (toggle2_) toggle2_->setOn(state.toggle2);
    if (toggle3_) toggle3_->setOn(state.toggle3);
    
    if (brightnessSlider_) brightnessSlider_->setValue(state.brightness);
    if (fanSlider_) fanSlider_->setValue(state.fanSpeed);
    
    // Update status texts
    updateStatusDisplay();
    updateSensorDisplay();
  }
  
  /**
   * @brief Update the UI (call each frame)
   */
  void update(float dt) {
    if (!initialized_) return;
    
    UIManager::instance().update(dt);
    
    // Periodic state sync
    syncTimer_ += dt;
    if (syncTimer_ >= 0.1f) {  // 10 Hz sync
      syncFromState();
      syncTimer_ = 0;
    }
  }
  
  /**
   * @brief Render the UI
   */
  void render() {
    if (!initialized_) return;
    UIManager::instance().render();
  }
  
  /**
   * @brief Get the frame buffer for display output
   */
  const uint8_t* getBuffer() const {
    return UIManager::instance().getBuffer();
  }
  
  size_t getBufferSize() const {
    return UIManager::instance().getBufferSize();
  }
  
  // ---- Input handling ----
  
  void navigateUp() {
    UIManager::instance().focusDirection(0, -1);
  }
  
  void navigateDown() {
    UIManager::instance().focusDirection(0, 1);
  }
  
  void navigateLeft() {
    UIManager::instance().focusDirection(-1, 0);
  }
  
  void navigateRight() {
    UIManager::instance().focusDirection(1, 0);
  }
  
  void select() {
    UIManager::instance().pressKey(KeyCode::ENTER);
  }
  
  void back() {
    UIManager::instance().pressKey(KeyCode::ESC);
  }
  
  void encoderRotate(int8_t delta) {
    UIManager::instance().encoderRotate(delta);
  }
  
  // ---- Scene navigation ----
  
  void showMain() {
    UIManager::instance().setScene(mainScene_, TransitionType::FADE);
  }
  
  void showStatus() {
    UIManager::instance().setScene(statusScene_, TransitionType::SLIDE_LEFT);
  }
  
  void showControls() {
    UIManager::instance().setScene(controlsScene_, TransitionType::SLIDE_LEFT);
  }
  
  void showSensors() {
    UIManager::instance().setScene(sensorsScene_, TransitionType::SLIDE_LEFT);
  }
  
  void showSettings() {
    UIManager::instance().setScene(settingsScene_, TransitionType::SLIDE_LEFT);
  }
  
  void goBack() {
    UIManager::instance().popScene(TransitionType::SLIDE_RIGHT);
  }
  
private:
  OledUI() = default;
  
  // ---- Scene Creation ----
  
  void createMainScene() {
    mainScene_ = UIManager::instance().createScene("main");
    auto& root = mainScene_->getRoot();
    root.setStyle(UIStyle().backgroundColor(Colors::Black));
    
    // Header
    auto* header = new UIContainer();
    header->setPosition(0, 0);
    header->setSize(width_, 16);
    header->setStyle(UIStyle().backgroundColor(Color(30)));
    root.addChild(header);
    
    auto* title = new UIText("SynthHead");
    title->setPosition(4, 2);
    title->setStyle(UIStyle().textColor(Colors::White).fontSize(FontSize::SMALL));
    header->addChild(title);
    
    // WiFi icon
    auto* wifiIcon = new UIIcon(IconType::WIFI);
    wifiIcon->setPosition(width_ - 24, 4);
    header->addChild(wifiIcon);
    
    // Battery icon
    auto* battIcon = new UIIcon(IconType::BATTERY_FULL);
    battIcon->setPosition(width_ - 12, 4);
    header->addChild(battIcon);
    
    // Menu items
    const char* menuItems[] = {"Status", "Controls", "Sensors", "Settings"};
    IconType menuIcons[] = {IconType::INFO, IconType::SLIDER, IconType::CHART, IconType::SETTINGS};
    std::function<void()> menuActions[] = {
      [this]() { showStatus(); },
      [this]() { showControls(); },
      [this]() { showSensors(); },
      [this]() { showSettings(); }
    };
    
    for (int i = 0; i < 4; i++) {
      auto* btn = new UIButton(menuItems[i]);
      btn->setPosition(8, 24 + i * 24);
      btn->setSize(width_ - 16, 20);
      btn->setIcon(menuIcons[i]);
      btn->setStyle(UIStyle()
        .backgroundColor(Color(40))
        .backgroundColorHover(Color(60))
        .backgroundColorPressed(Color(80))
        .borderRadius(4)
        .textColor(Colors::White)
        .padding(4));
      btn->onClick(menuActions[i]);
      root.addChild(btn);
      menuButtons_[i] = btn;
    }
    
    // Footer status
    statusText_ = new UIText("Ready");
    statusText_->setPosition(4, height_ - 12);
    statusText_->setStyle(UIStyle().textColor(Colors::Gray).fontSize(FontSize::TINY));
    root.addChild(statusText_);
  }
  
  void createStatusScene() {
    statusScene_ = UIManager::instance().createScene("status");
    auto& root = statusScene_->getRoot();
    root.setStyle(UIStyle().backgroundColor(Colors::Black));
    
    // Back button header
    addBackHeader(root, "Status");
    
    // Status info
    int y = 20;
    
    // Mode
    auto* modeLabel = new UIText("Mode:");
    modeLabel->setPosition(4, y);
    modeLabel->setStyle(UIStyle().textColor(Colors::Gray).fontSize(FontSize::TINY));
    root.addChild(modeLabel);
    
    modeText_ = new UIText("IDLE");
    modeText_->setPosition(50, y);
    modeText_->setStyle(UIStyle().textColor(Colors::Green).fontSize(FontSize::TINY));
    root.addChild(modeText_);
    y += 14;
    
    // Uptime
    auto* uptimeLabel = new UIText("Uptime:");
    uptimeLabel->setPosition(4, y);
    uptimeLabel->setStyle(UIStyle().textColor(Colors::Gray).fontSize(FontSize::TINY));
    root.addChild(uptimeLabel);
    
    uptimeText_ = new UIText("00:00:00");
    uptimeText_->setPosition(50, y);
    uptimeText_->setStyle(UIStyle().textColor(Colors::White).fontSize(FontSize::TINY));
    root.addChild(uptimeText_);
    y += 14;
    
    // CPU
    auto* cpuLabel = new UIText("CPU:");
    cpuLabel->setPosition(4, y);
    cpuLabel->setStyle(UIStyle().textColor(Colors::Gray).fontSize(FontSize::TINY));
    root.addChild(cpuLabel);
    
    cpuBar_ = new UIProgressBar(0.0f);
    cpuBar_->setPosition(50, y);
    cpuBar_->setSize(70, 8);
    root.addChild(cpuBar_);
    y += 14;
    
    // Memory
    auto* memLabel = new UIText("Heap:");
    memLabel->setPosition(4, y);
    memLabel->setStyle(UIStyle().textColor(Colors::Gray).fontSize(FontSize::TINY));
    root.addChild(memLabel);
    
    heapText_ = new UIText("0 KB");
    heapText_->setPosition(50, y);
    heapText_->setStyle(UIStyle().textColor(Colors::White).fontSize(FontSize::TINY));
    root.addChild(heapText_);
    y += 14;
    
    // FPS
    auto* fpsLabel = new UIText("FPS:");
    fpsLabel->setPosition(4, y);
    fpsLabel->setStyle(UIStyle().textColor(Colors::Gray).fontSize(FontSize::TINY));
    root.addChild(fpsLabel);
    
    fpsText_ = new UIText("0");
    fpsText_->setPosition(50, y);
    fpsText_->setStyle(UIStyle().textColor(Colors::White).fontSize(FontSize::TINY));
    root.addChild(fpsText_);
    y += 14;
    
    // WiFi info
    y += 6;
    auto* wifiLabel = new UIText("WiFi:");
    wifiLabel->setPosition(4, y);
    wifiLabel->setStyle(UIStyle().textColor(Colors::Cyan).fontSize(FontSize::TINY));
    root.addChild(wifiLabel);
    y += 12;
    
    ssidText_ = new UIText("SynthHead-AP");
    ssidText_->setPosition(8, y);
    ssidText_->setStyle(UIStyle().textColor(Colors::White).fontSize(FontSize::TINY));
    root.addChild(ssidText_);
    y += 12;
    
    ipText_ = new UIText("192.168.4.1");
    ipText_->setPosition(8, y);
    ipText_->setStyle(UIStyle().textColor(Colors::White).fontSize(FontSize::TINY));
    root.addChild(ipText_);
    y += 12;
    
    clientsText_ = new UIText("Clients: 0");
    clientsText_->setPosition(8, y);
    clientsText_->setStyle(UIStyle().textColor(Colors::White).fontSize(FontSize::TINY));
    root.addChild(clientsText_);
  }
  
  void createControlsScene() {
    controlsScene_ = UIManager::instance().createScene("controls");
    auto& root = controlsScene_->getRoot();
    root.setStyle(UIStyle().backgroundColor(Colors::Black));
    
    addBackHeader(root, "Controls");
    
    int y = 22;
    
    // Slider 1 - Brightness
    auto* lbl1 = new UIText("Brightness");
    lbl1->setPosition(4, y);
    lbl1->setStyle(UIStyle().textColor(Colors::Gray).fontSize(FontSize::TINY));
    root.addChild(lbl1);
    y += 10;
    
    brightnessSlider_ = new UISlider(0, 255, 128);
    brightnessSlider_->setPosition(4, y);
    brightnessSlider_->setWidth(width_ - 8);
    brightnessSlider_->onChange([](int val) {
      SYNC_STATE.setBrightness(val);
    });
    root.addChild(brightnessSlider_);
    y += 18;
    
    // Slider 2 - Fan Speed
    auto* lbl2 = new UIText("Fan Speed");
    lbl2->setPosition(4, y);
    lbl2->setStyle(UIStyle().textColor(Colors::Gray).fontSize(FontSize::TINY));
    root.addChild(lbl2);
    y += 10;
    
    fanSlider_ = new UISlider(0, 100, 0);
    fanSlider_->setPosition(4, y);
    fanSlider_->setWidth(width_ - 8);
    fanSlider_->onChange([](int val) {
      SYNC_STATE.setFanSpeed(val);
    });
    root.addChild(fanSlider_);
    y += 18;
    
    // Custom sliders
    auto* lbl3 = new UIText("Slider 1");
    lbl3->setPosition(4, y);
    lbl3->setStyle(UIStyle().textColor(Colors::Gray).fontSize(FontSize::TINY));
    root.addChild(lbl3);
    y += 10;
    
    slider1_ = new UISlider(0, 100, 50);
    slider1_->setPosition(4, y);
    slider1_->setWidth(width_ - 8);
    slider1_->onChange([](int val) {
      SYNC_STATE.setSlider1(val);
    });
    root.addChild(slider1_);
    y += 20;
    
    // Toggles
    toggle1_ = new UIToggle("LED Enable", false);
    toggle1_->setPosition(4, y);
    toggle1_->setSize(width_ - 8, 14);
    toggle1_->onChange([](bool val) {
      SYNC_STATE.setLedEnabled(val);
      SYNC_STATE.setToggle1(val);
    });
    root.addChild(toggle1_);
    y += 18;
    
    toggle2_ = new UIToggle("Display", true);
    toggle2_->setPosition(4, y);
    toggle2_->setSize(width_ - 8, 14);
    toggle2_->onChange([](bool val) {
      auto& state = SYNC_STATE.state();
      state.displayEnabled = val;
      SYNC_STATE.setToggle2(val);
    });
    root.addChild(toggle2_);
    y += 18;
    
    toggle3_ = new UIToggle("Auto Mode", false);
    toggle3_->setPosition(4, y);
    toggle3_->setSize(width_ - 8, 14);
    toggle3_->onChange([](bool val) {
      SYNC_STATE.setToggle3(val);
    });
    root.addChild(toggle3_);
  }
  
  void createSensorsScene() {
    sensorsScene_ = UIManager::instance().createScene("sensors");
    auto& root = sensorsScene_->getRoot();
    root.setStyle(UIStyle().backgroundColor(Colors::Black));
    
    addBackHeader(root, "Sensors");
    
    int y = 20;
    
    // Environmental
    auto* envLabel = new UIText("Environment");
    envLabel->setPosition(4, y);
    envLabel->setStyle(UIStyle().textColor(Colors::Cyan).fontSize(FontSize::TINY));
    root.addChild(envLabel);
    y += 12;
    
    tempText_ = new UIText("Temp: --.-°C");
    tempText_->setPosition(8, y);
    tempText_->setStyle(UIStyle().textColor(Colors::White).fontSize(FontSize::TINY));
    root.addChild(tempText_);
    y += 10;
    
    humText_ = new UIText("Hum: --.-%");
    humText_->setPosition(8, y);
    humText_->setStyle(UIStyle().textColor(Colors::White).fontSize(FontSize::TINY));
    root.addChild(humText_);
    y += 10;
    
    presText_ = new UIText("Pres: ---- hPa");
    presText_->setPosition(8, y);
    presText_->setStyle(UIStyle().textColor(Colors::White).fontSize(FontSize::TINY));
    root.addChild(presText_);
    y += 14;
    
    // IMU
    auto* imuLabel = new UIText("IMU");
    imuLabel->setPosition(4, y);
    imuLabel->setStyle(UIStyle().textColor(Colors::Cyan).fontSize(FontSize::TINY));
    root.addChild(imuLabel);
    y += 12;
    
    accelText_ = new UIText("Acc: 0,0,0");
    accelText_->setPosition(8, y);
    accelText_->setStyle(UIStyle().textColor(Colors::White).fontSize(FontSize::TINY));
    root.addChild(accelText_);
    y += 10;
    
    gyroText_ = new UIText("Gyr: 0,0,0");
    gyroText_->setPosition(8, y);
    gyroText_->setStyle(UIStyle().textColor(Colors::White).fontSize(FontSize::TINY));
    root.addChild(gyroText_);
    y += 14;
    
    // GPS
    auto* gpsLabel = new UIText("GPS");
    gpsLabel->setPosition(4, y);
    gpsLabel->setStyle(UIStyle().textColor(Colors::Cyan).fontSize(FontSize::TINY));
    root.addChild(gpsLabel);
    y += 12;
    
    gpsStatusText_ = new UIText("No Fix");
    gpsStatusText_->setPosition(8, y);
    gpsStatusText_->setStyle(UIStyle().textColor(Colors::Red).fontSize(FontSize::TINY));
    root.addChild(gpsStatusText_);
    y += 10;
    
    latText_ = new UIText("Lat: --.------");
    latText_->setPosition(8, y);
    latText_->setStyle(UIStyle().textColor(Colors::White).fontSize(FontSize::TINY));
    root.addChild(latText_);
    y += 10;
    
    lonText_ = new UIText("Lon: --.------");
    lonText_->setPosition(8, y);
    lonText_->setStyle(UIStyle().textColor(Colors::White).fontSize(FontSize::TINY));
    root.addChild(lonText_);
    y += 10;
    
    altText_ = new UIText("Alt: ---m");
    altText_->setPosition(8, y);
    altText_->setStyle(UIStyle().textColor(Colors::White).fontSize(FontSize::TINY));
    root.addChild(altText_);
  }
  
  void createSettingsScene() {
    settingsScene_ = UIManager::instance().createScene("settings");
    auto& root = settingsScene_->getRoot();
    root.setStyle(UIStyle().backgroundColor(Colors::Black));
    
    addBackHeader(root, "Settings");
    
    int y = 22;
    
    // LED Color dropdown
    auto* ledLabel = new UIText("LED Color");
    ledLabel->setPosition(4, y);
    ledLabel->setStyle(UIStyle().textColor(Colors::Gray).fontSize(FontSize::TINY));
    root.addChild(ledLabel);
    y += 12;
    
    const char* colors[] = {"Off", "Red", "Green", "Blue", "White"};
    ledColorDropdown_ = new UIDropdown();
    ledColorDropdown_->setPosition(4, y);
    ledColorDropdown_->setSize(width_ - 8, 16);
    for (int i = 0; i < 5; i++) {
      ledColorDropdown_->addItem(colors[i]);
    }
    ledColorDropdown_->setSelectedIndex(0);
    ledColorDropdown_->onChange([](int idx) {
      SYNC_STATE.setLedColor(idx);
    });
    root.addChild(ledColorDropdown_);
    y += 24;
    
    // Animation speed slider
    auto* animLabel = new UIText("Anim Speed");
    animLabel->setPosition(4, y);
    animLabel->setStyle(UIStyle().textColor(Colors::Gray).fontSize(FontSize::TINY));
    root.addChild(animLabel);
    y += 10;
    
    slider2_ = new UISlider(0, 100, 50);
    slider2_->setPosition(4, y);
    slider2_->setWidth(width_ - 8);
    slider2_->onChange([](int val) {
      SYNC_STATE.setSlider2(val);
    });
    root.addChild(slider2_);
    y += 20;
    
    // Misc slider
    auto* miscLabel = new UIText("Misc Value");
    miscLabel->setPosition(4, y);
    miscLabel->setStyle(UIStyle().textColor(Colors::Gray).fontSize(FontSize::TINY));
    root.addChild(miscLabel);
    y += 10;
    
    slider3_ = new UISlider(0, 100, 50);
    slider3_->setPosition(4, y);
    slider3_->setWidth(width_ - 8);
    slider3_->onChange([](int val) {
      SYNC_STATE.setSlider3(val);
    });
    root.addChild(slider3_);
    y += 24;
    
    // Reset button
    auto* resetBtn = new UIButton("Factory Reset");
    resetBtn->setPosition(4, y);
    resetBtn->setSize(width_ - 8, 18);
    resetBtn->setStyle(UIStyle()
      .backgroundColor(Color(80, 20, 20))
      .backgroundColorHover(Color(120, 30, 30))
      .borderRadius(4)
      .textColor(Colors::White));
    resetBtn->onClick([]() {
      // Factory reset action
    });
    root.addChild(resetBtn);
  }
  
  // ---- Helper Methods ----
  
  void addBackHeader(UIContainer& root, const char* title) {
    auto* header = new UIContainer();
    header->setPosition(0, 0);
    header->setSize(width_, 18);
    header->setStyle(UIStyle().backgroundColor(Color(30)));
    root.addChild(header);
    
    auto* backBtn = new UIButton("<");
    backBtn->setPosition(2, 2);
    backBtn->setSize(14, 14);
    backBtn->setStyle(UIStyle()
      .backgroundColor(Color(50))
      .backgroundColorHover(Color(70))
      .borderRadius(2)
      .textColor(Colors::White));
    backBtn->onClick([this]() { goBack(); });
    header->addChild(backBtn);
    
    auto* titleText = new UIText(title);
    titleText->setPosition(22, 3);
    titleText->setStyle(UIStyle().textColor(Colors::White).fontSize(FontSize::SMALL));
    header->addChild(titleText);
  }
  
  void updateStatusDisplay() {
    auto& state = SYNC_STATE.state();
    char buf[32];
    
    // Mode
    if (modeText_) {
      const char* modes[] = {"IDLE", "RUNNING", "PAUSED", "ERROR"};
      modeText_->setText(modes[(int)state.mode]);
    }
    
    // Uptime
    if (uptimeText_) {
      uint32_t secs = state.uptime;
      snprintf(buf, sizeof(buf), "%02lu:%02lu:%02lu", 
               secs / 3600, (secs / 60) % 60, secs % 60);
      uptimeText_->setText(buf);
    }
    
    // CPU
    if (cpuBar_) {
      cpuBar_->setValue(state.cpuUsage / 100.0f);
    }
    
    // Heap
    if (heapText_) {
      snprintf(buf, sizeof(buf), "%lu KB", state.freeHeap / 1024);
      heapText_->setText(buf);
    }
    
    // FPS
    if (fpsText_) {
      snprintf(buf, sizeof(buf), "%.1f", state.fps);
      fpsText_->setText(buf);
    }
    
    // WiFi
    if (ssidText_) ssidText_->setText(state.ssid);
    if (ipText_) ipText_->setText(state.ipAddress);
    if (clientsText_) {
      snprintf(buf, sizeof(buf), "Clients: %d", state.wifiClients);
      clientsText_->setText(buf);
    }
    
    // Main screen status
    if (statusText_) {
      statusText_->setText(state.statusText);
    }
  }
  
  void updateSensorDisplay() {
    auto& state = SYNC_STATE.state();
    char buf[32];
    
    // Environment
    if (tempText_) {
      snprintf(buf, sizeof(buf), "Temp: %.1f C", state.temperature);
      tempText_->setText(buf);
    }
    if (humText_) {
      snprintf(buf, sizeof(buf), "Hum: %.1f%%", state.humidity);
      humText_->setText(buf);
    }
    if (presText_) {
      snprintf(buf, sizeof(buf), "Pres: %.0f hPa", state.pressure);
      presText_->setText(buf);
    }
    
    // IMU
    if (accelText_) {
      snprintf(buf, sizeof(buf), "Acc:%d,%d,%d", 
               state.accelX, state.accelY, state.accelZ);
      accelText_->setText(buf);
    }
    if (gyroText_) {
      snprintf(buf, sizeof(buf), "Gyr:%d,%d,%d",
               state.gyroX, state.gyroY, state.gyroZ);
      gyroText_->setText(buf);
    }
    
    // GPS
    if (gpsStatusText_) {
      if (state.gpsValid) {
        gpsStatusText_->setText("Fix OK");
        gpsStatusText_->setStyle(UIStyle().textColor(Colors::Green).fontSize(FontSize::TINY));
      } else {
        gpsStatusText_->setText("No Fix");
        gpsStatusText_->setStyle(UIStyle().textColor(Colors::Red).fontSize(FontSize::TINY));
      }
    }
    if (latText_) {
      snprintf(buf, sizeof(buf), "Lat: %.6f", state.latitude);
      latText_->setText(buf);
    }
    if (lonText_) {
      snprintf(buf, sizeof(buf), "Lon: %.6f", state.longitude);
      lonText_->setText(buf);
    }
    if (altText_) {
      snprintf(buf, sizeof(buf), "Alt: %.0fm", state.altitude);
      altText_->setText(buf);
    }
  }
  
  // State
  bool initialized_ = false;
  uint16_t width_ = 128;
  uint16_t height_ = 128;
  float syncTimer_ = 0;
  
  // Scenes
  UIScene* mainScene_ = nullptr;
  UIScene* statusScene_ = nullptr;
  UIScene* controlsScene_ = nullptr;
  UIScene* sensorsScene_ = nullptr;
  UIScene* settingsScene_ = nullptr;
  
  // Main scene elements
  UIButton* menuButtons_[4] = {};
  UIText* statusText_ = nullptr;
  
  // Status scene elements
  UIText* modeText_ = nullptr;
  UIText* uptimeText_ = nullptr;
  UIProgressBar* cpuBar_ = nullptr;
  UIText* heapText_ = nullptr;
  UIText* fpsText_ = nullptr;
  UIText* ssidText_ = nullptr;
  UIText* ipText_ = nullptr;
  UIText* clientsText_ = nullptr;
  
  // Controls scene elements
  UISlider* brightnessSlider_ = nullptr;
  UISlider* fanSlider_ = nullptr;
  UISlider* slider1_ = nullptr;
  UIToggle* toggle1_ = nullptr;
  UIToggle* toggle2_ = nullptr;
  UIToggle* toggle3_ = nullptr;
  
  // Sensors scene elements
  UIText* tempText_ = nullptr;
  UIText* humText_ = nullptr;
  UIText* presText_ = nullptr;
  UIText* accelText_ = nullptr;
  UIText* gyroText_ = nullptr;
  UIText* gpsStatusText_ = nullptr;
  UIText* latText_ = nullptr;
  UIText* lonText_ = nullptr;
  UIText* altText_ = nullptr;
  
  // Settings scene elements
  UIDropdown* ledColorDropdown_ = nullptr;
  UISlider* slider2_ = nullptr;
  UISlider* slider3_ = nullptr;
};

// Convenience macro
#define OLED_UI OledUI::instance()

} // namespace UI
} // namespace SystemAPI
