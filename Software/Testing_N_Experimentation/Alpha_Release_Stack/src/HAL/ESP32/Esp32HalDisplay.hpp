/*****************************************************************
 * File:      Esp32HalDisplay.hpp
 * Category:  src/HAL/ESP32
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    ESP32 implementation of Display HAL interfaces.
 *    Includes HUB75 LED matrix and OLED (SH1107/SSD1306) support.
 *****************************************************************/

#ifndef ARCOS_SRC_HAL_ESP32_HAL_DISPLAY_HPP_
#define ARCOS_SRC_HAL_ESP32_HAL_DISPLAY_HPP_

#include "HAL/IHalDisplay.hpp"
#include "HAL/IHalI2c.hpp"
#include "HAL/IHalLog.hpp"
#include <Arduino.h>

// Include ESP32-HUB75-MatrixPanel-I2S-DMA library if available
#ifdef ESP32_HUB75_MATRIXPANEL_I2S_DMA
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#endif

namespace arcos::hal::esp32{

// ============================================================
// HUB75 Display Implementation
// ============================================================

/** ESP32 HUB75 Matrix Display Implementation
 * 
 * Uses the ESP32-HUB75-MatrixPanel-I2S-DMA library for driving
 * HUB75 LED matrix panels via I2S DMA.
 */
class Esp32HalHub75Display : public IHalHub75Display{
private:
  static constexpr const char* TAG = "HUB75";
  
  IHalLog* log_ = nullptr;
  Hub75Config config_;
  bool initialized_ = false;
  uint8_t brightness_ = 128;
  
#ifdef ESP32_HUB75_MATRIXPANEL_I2S_DMA
  MatrixPanel_I2S_DMA* matrix_ = nullptr;
#else
  // Stub for when library is not available
  void* matrix_ = nullptr;
#endif
  
  // Frame buffer for software fallback
  RGB* frame_buffer_ = nullptr;
  uint16_t width_ = 0;
  uint16_t height_ = 0;

public:
  explicit Esp32HalHub75Display(IHalLog* log = nullptr) : log_(log){}
  
  ~Esp32HalHub75Display() override{
    deinit();
  }
  
  HalResult init(const Hub75Config& config) override{
    if(initialized_){
      if(log_) log_->warn(TAG, "HUB75 already initialized");
      return HalResult::ALREADY_INITIALIZED;
    }
    
    config_ = config;
    width_ = config.width * config.chain_length;
    height_ = config.height;
    
#ifdef ESP32_HUB75_MATRIXPANEL_I2S_DMA
    // Configure HUB75 panel
    HUB75_I2S_CFG::i2s_pins pins = {
      .r1 = 25, .g1 = 26, .b1 = 27,
      .r2 = 14, .g2 = 12, .b2 = 13,
      .a = 23, .b = 19, .c = 5, .d = 17, .e = -1,
      .lat = 4, .oe = 15, .clk = 16
    };
    
    HUB75_I2S_CFG mxconfig(
      config.width,
      config.height,
      config.chain_length,
      pins
    );
    
    mxconfig.double_buff = config.double_buffer;
    mxconfig.clkphase = false;
    
    matrix_ = new MatrixPanel_I2S_DMA(mxconfig);
    if(!matrix_->begin()){
      delete matrix_;
      matrix_ = nullptr;
      if(log_) log_->error(TAG, "Failed to initialize HUB75 matrix");
      return HalResult::HARDWARE_FAULT;
    }
    
    matrix_->setBrightness8(brightness_);
    matrix_->clearScreen();
#else
    // Allocate software frame buffer as fallback
    frame_buffer_ = new RGB[width_ * height_];
    if(!frame_buffer_){
      if(log_) log_->error(TAG, "Failed to allocate frame buffer");
      return HalResult::NO_MEMORY;
    }
    memset(frame_buffer_, 0, width_ * height_ * sizeof(RGB));
    
    if(log_) log_->warn(TAG, "HUB75 library not available, using software framebuffer");
#endif
    
    initialized_ = true;
    if(log_) log_->info(TAG, "HUB75 display initialized: %dx%d, chains=%d",
                        config.width, config.height, config.chain_length);
    return HalResult::OK;
  }
  
  HalResult deinit() override{
    if(!initialized_) return HalResult::NOT_INITIALIZED;
    
#ifdef ESP32_HUB75_MATRIXPANEL_I2S_DMA
    if(matrix_){
      delete matrix_;
      matrix_ = nullptr;
    }
#endif
    
    if(frame_buffer_){
      delete[] frame_buffer_;
      frame_buffer_ = nullptr;
    }
    
    initialized_ = false;
    if(log_) log_->info(TAG, "HUB75 display deinitialized");
    return HalResult::OK;
  }
  
  bool isInitialized() const override{
    return initialized_;
  }
  
  HalResult setPixel(int16_t x, int16_t y, const RGB& color) override{
    if(!initialized_) return HalResult::NOT_INITIALIZED;
    if(x < 0 || x >= width_ || y < 0 || y >= height_) return HalResult::INVALID_PARAM;
    
#ifdef ESP32_HUB75_MATRIXPANEL_I2S_DMA
    matrix_->drawPixelRGB888(x, y, color.r, color.g, color.b);
#else
    frame_buffer_[y * width_ + x] = color;
#endif
    
    return HalResult::OK;
  }
  
  HalResult getPixel(int16_t x, int16_t y, RGB& color) const override{
    if(!initialized_) return HalResult::NOT_INITIALIZED;
    if(x < 0 || x >= width_ || y < 0 || y >= height_) return HalResult::INVALID_PARAM;
    
#ifdef ESP32_HUB75_MATRIXPANEL_I2S_DMA
    // HUB75 library doesn't support reading pixels easily
    color = {0, 0, 0};
#else
    color = frame_buffer_[y * width_ + x];
#endif
    
    return HalResult::OK;
  }
  
  HalResult fill(const RGB& color) override{
    if(!initialized_) return HalResult::NOT_INITIALIZED;
    
#ifdef ESP32_HUB75_MATRIXPANEL_I2S_DMA
    matrix_->fillScreenRGB888(color.r, color.g, color.b);
#else
    for(int i = 0; i < width_ * height_; i++){
      frame_buffer_[i] = color;
    }
#endif
    
    return HalResult::OK;
  }
  
  HalResult clear() override{
    return fill({0, 0, 0});
  }
  
  HalResult show() override{
    if(!initialized_) return HalResult::NOT_INITIALIZED;
    
#ifdef ESP32_HUB75_MATRIXPANEL_I2S_DMA
    if(config_.double_buffer){
      matrix_->flipDMABuffer();
    }
#endif
    
    return HalResult::OK;
  }
  
  uint16_t getWidth() const override{
    return width_;
  }
  
  uint16_t getHeight() const override{
    return height_;
  }
  
  HalResult setBrightness(uint8_t brightness) override{
    if(!initialized_) return HalResult::NOT_INITIALIZED;
    
    brightness_ = brightness;
    
#ifdef ESP32_HUB75_MATRIXPANEL_I2S_DMA
    matrix_->setBrightness8(brightness);
#endif
    
    if(log_) log_->debug(TAG, "Brightness set to %d", brightness);
    return HalResult::OK;
  }
  
  uint8_t getBrightness() const override{
    return brightness_;
  }
};

// ============================================================
// OLED Display Implementation (SH1107/SSD1306)
// ============================================================

/** ESP32 OLED Display Implementation
 * 
 * Direct I2C driver for SH1107 (128x128) and SSD1306 (128x64) OLED displays.
 * Uses a software frame buffer and direct I2C commands.
 */
class Esp32HalOledDisplay : public IHalOledDisplay{
private:
  static constexpr const char* TAG = "OLED";
  
  // Common OLED commands
  static constexpr uint8_t CMD_DISPLAY_OFF = 0xAE;
  static constexpr uint8_t CMD_DISPLAY_ON = 0xAF;
  static constexpr uint8_t CMD_SET_CONTRAST = 0x81;
  static constexpr uint8_t CMD_NORMAL_DISPLAY = 0xA6;
  static constexpr uint8_t CMD_INVERT_DISPLAY = 0xA7;
  static constexpr uint8_t CMD_SET_PAGE_ADDR = 0xB0;
  static constexpr uint8_t CMD_SET_COL_LOW = 0x00;
  static constexpr uint8_t CMD_SET_COL_HIGH = 0x10;
  
  IHalLog* log_ = nullptr;
  IHalI2c* i2c_ = nullptr;
  OledConfig config_;
  bool initialized_ = false;
  
  // Frame buffer (1 bit per pixel)
  uint8_t* frame_buffer_ = nullptr;
  size_t buffer_size_ = 0;
  uint8_t contrast_ = 0xCF;
  
  HalResult sendCommand(uint8_t cmd){
    uint8_t data[2] = {0x00, cmd};  // Co=0, D/C#=0 (command)
    return i2c_->write(config_.address, data, 2);
  }
  
  HalResult sendCommands(const uint8_t* cmds, size_t length){
    for(size_t i = 0; i < length; i++){
      HalResult result = sendCommand(cmds[i]);
      if(result != HalResult::OK) return result;
    }
    return HalResult::OK;
  }

public:
  Esp32HalOledDisplay(IHalI2c* i2c, IHalLog* log = nullptr) 
    : log_(log), i2c_(i2c){}
  
  ~Esp32HalOledDisplay() override{
    deinit();
  }
  
  HalResult init(const OledConfig& config) override{
    if(initialized_){
      if(log_) log_->warn(TAG, "OLED already initialized");
      return HalResult::ALREADY_INITIALIZED;
    }
    
    if(!i2c_ || !i2c_->isInitialized()){
      if(log_) log_->error(TAG, "I2C not initialized");
      return HalResult::NOT_INITIALIZED;
    }
    
    config_ = config;
    contrast_ = config.contrast;
    
    // Allocate frame buffer (1 bit per pixel, organized as pages of 8 rows)
    size_t pages = (config_.height + 7) / 8;
    buffer_size_ = config_.width * pages;
    frame_buffer_ = new uint8_t[buffer_size_];
    if(!frame_buffer_){
      if(log_) log_->error(TAG, "Failed to allocate frame buffer");
      return HalResult::NO_MEMORY;
    }
    memset(frame_buffer_, 0, buffer_size_);
    
    // Probe for OLED
    if(i2c_->probe(config_.address) != HalResult::OK){
      delete[] frame_buffer_;
      frame_buffer_ = nullptr;
      if(log_) log_->error(TAG, "OLED not found at 0x%02X", config_.address);
      return HalResult::DEVICE_NOT_FOUND;
    }
    
    // Initialize display
    const uint8_t init_cmds[] = {
      CMD_DISPLAY_OFF,
      0xD5, 0x80,           // Set display clock
      0xA8, (uint8_t)(config_.height - 1),  // Set multiplex ratio
      0xD3, 0x00,           // Set display offset
      0x40,                 // Set start line
      0x8D, 0x14,           // Enable charge pump
      0x20, 0x00,           // Memory mode: horizontal
      0xA1,                 // Segment remap
      0xC8,                 // COM scan direction
      0xDA, (config_.height == 64) ? (uint8_t)0x12 : (uint8_t)0x02,  // COM pins
      CMD_SET_CONTRAST, contrast_,
      0xD9, 0xF1,           // Pre-charge period
      0xDB, 0x40,           // VCOMH deselect
      0xA4,                 // Output follows RAM
      CMD_NORMAL_DISPLAY,
      CMD_DISPLAY_ON
    };
    
    HalResult result = sendCommands(init_cmds, sizeof(init_cmds));
    if(result != HalResult::OK){
      delete[] frame_buffer_;
      frame_buffer_ = nullptr;
      if(log_) log_->error(TAG, "Failed to initialize OLED");
      return result;
    }
    
    initialized_ = true;
    if(log_) log_->info(TAG, "OLED initialized: %dx%d at 0x%02X",
                        config_.width, config_.height, config_.address);
    return HalResult::OK;
  }
  
  HalResult deinit() override{
    if(!initialized_) return HalResult::NOT_INITIALIZED;
    
    sendCommand(CMD_DISPLAY_OFF);
    
    if(frame_buffer_){
      delete[] frame_buffer_;
      frame_buffer_ = nullptr;
    }
    
    initialized_ = false;
    if(log_) log_->info(TAG, "OLED deinitialized");
    return HalResult::OK;
  }
  
  bool isInitialized() const override{
    return initialized_;
  }
  
  HalResult setPixel(int16_t x, int16_t y, bool on) override{
    if(!initialized_) return HalResult::NOT_INITIALIZED;
    if(x < 0 || x >= config_.width || y < 0 || y >= config_.height){
      return HalResult::INVALID_PARAM;
    }
    
    size_t page = y / 8;
    uint8_t bit = y % 8;
    size_t index = x + (page * config_.width);
    
    if(on){
      frame_buffer_[index] |= (1 << bit);
    } else{
      frame_buffer_[index] &= ~(1 << bit);
    }
    
    return HalResult::OK;
  }
  
  bool getPixel(int16_t x, int16_t y) const override{
    if(!initialized_) return false;
    if(x < 0 || x >= config_.width || y < 0 || y >= config_.height){
      return false;
    }
    
    size_t page = y / 8;
    uint8_t bit = y % 8;
    size_t index = x + (page * config_.width);
    
    return (frame_buffer_[index] & (1 << bit)) != 0;
  }
  
  HalResult fill(bool on) override{
    if(!initialized_) return HalResult::NOT_INITIALIZED;
    memset(frame_buffer_, on ? 0xFF : 0x00, buffer_size_);
    return HalResult::OK;
  }
  
  HalResult clear() override{
    return fill(false);
  }
  
  HalResult show() override{
    if(!initialized_) return HalResult::NOT_INITIALIZED;
    
    size_t pages = (config_.height + 7) / 8;
    
    for(size_t page = 0; page < pages; page++){
      // Set page address
      sendCommand(CMD_SET_PAGE_ADDR | page);
      sendCommand(CMD_SET_COL_LOW);
      sendCommand(CMD_SET_COL_HIGH);
      
      // Send page data
      uint8_t* page_data = frame_buffer_ + (page * config_.width);
      
      // Send data with D/C# bit set
      uint8_t buffer[config_.width + 1];
      buffer[0] = 0x40;  // Co=0, D/C#=1 (data)
      memcpy(buffer + 1, page_data, config_.width);
      i2c_->write(config_.address, buffer, config_.width + 1);
    }
    
    return HalResult::OK;
  }
  
  uint16_t getWidth() const override{
    return config_.width;
  }
  
  uint16_t getHeight() const override{
    return config_.height;
  }
  
  HalResult setContrast(uint8_t contrast) override{
    if(!initialized_) return HalResult::NOT_INITIALIZED;
    
    contrast_ = contrast;
    sendCommand(CMD_SET_CONTRAST);
    sendCommand(contrast);
    
    if(log_) log_->debug(TAG, "Contrast set to %d", contrast);
    return HalResult::OK;
  }
  
  uint8_t getContrast() const override{
    return contrast_;
  }
  
  HalResult setDisplayOn(bool on) override{
    if(!initialized_) return HalResult::NOT_INITIALIZED;
    return sendCommand(on ? CMD_DISPLAY_ON : CMD_DISPLAY_OFF);
  }
  
  HalResult setInverted(bool invert) override{
    if(!initialized_) return HalResult::NOT_INITIALIZED;
    return sendCommand(invert ? CMD_INVERT_DISPLAY : CMD_NORMAL_DISPLAY);
  }
};

} // namespace arcos::hal::esp32

#endif // ARCOS_SRC_HAL_ESP32_HAL_DISPLAY_HPP_
