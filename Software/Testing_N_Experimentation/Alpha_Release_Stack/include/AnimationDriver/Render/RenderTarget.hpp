/*****************************************************************
 * @file RenderTarget.hpp
 * @brief Target displays for animation rendering
 * 
 * Manages rendering to HUB75 LED matrix and OLED display.
 *****************************************************************/

#pragma once

#include "../Core/Types.hpp"
#include "../Core/Color.hpp"
#include "../Shaders/ShaderBase.hpp"

namespace AnimationDriver {

// ============================================================
// Display Dimensions
// ============================================================

struct DisplayDimensions {
    int width;
    int height;
    
    DisplayDimensions(int w = 128, int h = 32) : width(w), height(h) {}
    
    int getPixelCount() const { return width * height; }
    
    // Standard display presets
    static DisplayDimensions HUB75() { return DisplayDimensions(128, 32); }
    static DisplayDimensions OLED()  { return DisplayDimensions(128, 128); }
};

// ============================================================
// Frame Buffer Interface
// ============================================================

class FrameBuffer {
public:
    virtual ~FrameBuffer() = default;
    
    // Set pixel color
    virtual void setPixel(int x, int y, const RGB& color) = 0;
    
    // Get pixel color
    virtual RGB getPixel(int x, int y) const = 0;
    
    // Clear with color
    virtual void clear(const RGB& color = RGB::Black()) = 0;
    
    // Fill entire buffer
    virtual void fill(const RGB& color) = 0;
    
    // Get dimensions
    virtual int getWidth() const = 0;
    virtual int getHeight() const = 0;
    
    // Get raw buffer pointer (for GPU commands)
    virtual uint8_t* getData() = 0;
    virtual const uint8_t* getData() const = 0;
    virtual size_t getDataSize() const = 0;
};

// ============================================================
// RGB Frame Buffer (for HUB75)
// ============================================================

class RGBFrameBuffer : public FrameBuffer {
public:
    static constexpr int MAX_PIXELS = 128 * 64;  // Support up to 128x64
    
    RGBFrameBuffer(int width = 128, int height = 32) 
        : width_(width), height_(height) {
        clear();
    }
    
    void setPixel(int x, int y, const RGB& color) override {
        if (x >= 0 && x < width_ && y >= 0 && y < height_) {
            int idx = (y * width_ + x) * 3;
            buffer_[idx + 0] = color.r;
            buffer_[idx + 1] = color.g;
            buffer_[idx + 2] = color.b;
        }
    }
    
    RGB getPixel(int x, int y) const override {
        if (x >= 0 && x < width_ && y >= 0 && y < height_) {
            int idx = (y * width_ + x) * 3;
            return RGB(buffer_[idx], buffer_[idx + 1], buffer_[idx + 2]);
        }
        return RGB::Black();
    }
    
    void clear(const RGB& color = RGB::Black()) override {
        fill(color);
    }
    
    void fill(const RGB& color) override {
        for (int i = 0; i < width_ * height_; i++) {
            buffer_[i * 3 + 0] = color.r;
            buffer_[i * 3 + 1] = color.g;
            buffer_[i * 3 + 2] = color.b;
        }
    }
    
    int getWidth() const override { return width_; }
    int getHeight() const override { return height_; }
    
    uint8_t* getData() override { return buffer_; }
    const uint8_t* getData() const override { return buffer_; }
    size_t getDataSize() const override { return width_ * height_ * 3; }
    
    // Resize buffer
    void resize(int width, int height) {
        width_ = std::min(width, static_cast<int>(MAX_PIXELS / height));
        height_ = height;
        clear();
    }
    
private:
    uint8_t buffer_[MAX_PIXELS * 3];
    int width_;
    int height_;
};

// ============================================================
// Mono Frame Buffer (for OLED)
// ============================================================

class MonoFrameBuffer : public FrameBuffer {
public:
    static constexpr int MAX_PIXELS = 128 * 128;
    
    MonoFrameBuffer(int width = 128, int height = 128) 
        : width_(width), height_(height) {
        clear();
    }
    
    void setPixel(int x, int y, const RGB& color) override {
        if (x >= 0 && x < width_ && y >= 0 && y < height_) {
            // Convert to grayscale
            uint8_t gray = static_cast<uint8_t>(
                color.r * 0.299f + color.g * 0.587f + color.b * 0.114f
            );
            buffer_[y * width_ + x] = gray;
        }
    }
    
    // Set pixel with grayscale directly
    void setPixelMono(int x, int y, uint8_t value) {
        if (x >= 0 && x < width_ && y >= 0 && y < height_) {
            buffer_[y * width_ + x] = value;
        }
    }
    
    RGB getPixel(int x, int y) const override {
        if (x >= 0 && x < width_ && y >= 0 && y < height_) {
            uint8_t v = buffer_[y * width_ + x];
            return RGB(v, v, v);
        }
        return RGB::Black();
    }
    
    uint8_t getPixelMono(int x, int y) const {
        if (x >= 0 && x < width_ && y >= 0 && y < height_) {
            return buffer_[y * width_ + x];
        }
        return 0;
    }
    
    void clear(const RGB& color = RGB::Black()) override {
        uint8_t gray = static_cast<uint8_t>(
            color.r * 0.299f + color.g * 0.587f + color.b * 0.114f
        );
        memset(buffer_, gray, width_ * height_);
    }
    
    void fill(const RGB& color) override {
        clear(color);
    }
    
    int getWidth() const override { return width_; }
    int getHeight() const override { return height_; }
    
    uint8_t* getData() override { return buffer_; }
    const uint8_t* getData() const override { return buffer_; }
    size_t getDataSize() const override { return width_ * height_; }
    
private:
    uint8_t buffer_[MAX_PIXELS];
    int width_;
    int height_;
};

// ============================================================
// Render Target - Combines buffer with context
// ============================================================

class RenderTarget {
public:
    RenderTarget(DisplayTarget target = DisplayTarget::HUB75) 
        : target_(target), frame_(0) {
        if (target == DisplayTarget::HUB75) {
            rgbBuffer_ = new RGBFrameBuffer(128, 32);
            buffer_ = rgbBuffer_;
            isRGB_ = true;
        } else {
            monoBuffer_ = new MonoFrameBuffer(128, 128);
            buffer_ = monoBuffer_;
            isRGB_ = false;
        }
        updateContext();
    }
    
    ~RenderTarget() {
        if (rgbBuffer_) delete rgbBuffer_;
        if (monoBuffer_) delete monoBuffer_;
    }
    
    // Render shader to entire buffer
    void renderShader(ShaderBase* shader) {
        if (!shader || !buffer_) return;
        
        int w = buffer_->getWidth();
        int h = buffer_->getHeight();
        
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                context_.setPixel(x, y, w, h);
                RGB color = shader->render(context_);
                buffer_->setPixel(x, y, color);
            }
        }
    }
    
    // Render shader to region
    void renderShader(ShaderBase* shader, int x1, int y1, int x2, int y2) {
        if (!shader || !buffer_) return;
        
        int w = buffer_->getWidth();
        int h = buffer_->getHeight();
        
        x1 = std::max(0, std::min(x1, w - 1));
        y1 = std::max(0, std::min(y1, h - 1));
        x2 = std::max(0, std::min(x2, w));
        y2 = std::max(0, std::min(y2, h));
        
        for (int y = y1; y < y2; y++) {
            for (int x = x1; x < x2; x++) {
                context_.setPixel(x, y, w, h);
                RGB color = shader->render(context_);
                buffer_->setPixel(x, y, color);
            }
        }
    }
    
    // Update time and frame
    void update(float deltaTime) {
        context_.deltaTime = deltaTime;
        context_.time += deltaTime;
        context_.frame = ++frame_;
    }
    
    // Reset time
    void resetTime() {
        context_.time = 0.0f;
        frame_ = 0;
    }
    
    // Access buffer
    FrameBuffer* getBuffer() { return buffer_; }
    const FrameBuffer* getBuffer() const { return buffer_; }
    
    // Access context
    ShaderContext& getContext() { return context_; }
    const ShaderContext& getContext() const { return context_; }
    
    // Get buffer data for GPU commands
    const uint8_t* getData() const { return buffer_ ? buffer_->getData() : nullptr; }
    size_t getDataSize() const { return buffer_ ? buffer_->getDataSize() : 0; }
    
    // Clear buffer
    void clear(const RGB& color = RGB::Black()) {
        if (buffer_) buffer_->clear(color);
    }
    
    // Get target type
    DisplayTarget getTarget() const { return target_; }
    bool isRGB() const { return isRGB_; }
    
    int getWidth() const { return buffer_ ? buffer_->getWidth() : 0; }
    int getHeight() const { return buffer_ ? buffer_->getHeight() : 0; }
    
private:
    void updateContext() {
        if (buffer_) {
            context_.width = buffer_->getWidth();
            context_.height = buffer_->getHeight();
        }
    }
    
    DisplayTarget target_;
    FrameBuffer* buffer_ = nullptr;
    RGBFrameBuffer* rgbBuffer_ = nullptr;
    MonoFrameBuffer* monoBuffer_ = nullptr;
    ShaderContext context_;
    uint32_t frame_;
    bool isRGB_;
};

} // namespace AnimationDriver
