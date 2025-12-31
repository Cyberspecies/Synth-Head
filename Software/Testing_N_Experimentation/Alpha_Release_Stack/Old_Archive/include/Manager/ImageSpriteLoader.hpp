/*****************************************************************
 * File:      ImageSpriteLoader.hpp
 * Category:  Manager
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Loads and renders custom sprite images transferred from CPU
 *    for display on HUB75 LED matrix. Supports simple RGB format
 *    and centers sprite on each panel.
 *****************************************************************/

#ifndef ARCOS_MANAGER_IMAGE_SPRITE_LOADER_HPP_
#define ARCOS_MANAGER_IMAGE_SPRITE_LOADER_HPP_

#include <cstdint>
#include <cstring>
#include "Manager/HUB75DisplayManager.hpp"

namespace arcos::manager{

/**
 * @brief Simple image sprite loader for HUB75 displays
 * 
 * Expected image format:
 * - Width (2 bytes, little-endian)
 * - Height (2 bytes, little-endian)
 * - RGB pixel data (width * height * 3 bytes)
 * - Each pixel: R, G, B (8-bit values)
 */
class ImageSpriteLoader{
public:
  ImageSpriteLoader()
    : image_data_(nullptr)
    , image_width_(0)
    , image_height_(0)
    , data_size_(0)
    , loaded_(false)
  {
  }
  
  ~ImageSpriteLoader(){
    clearImage();
  }
  
  /**
   * @brief Load image from raw data buffer
   * @param data Raw image data (includes width, height, RGB pixels)
   * @param size Total size of data buffer
   * @return true if loaded successfully
   */
  bool loadImage(const uint8_t* data, uint32_t size){
    if(!data || size < 4){
      return false;
    }
    
    // Parse header: width (2 bytes) + height (2 bytes)
    uint16_t width = data[0] | (data[1] << 8);
    uint16_t height = data[2] | (data[3] << 8);
    
    // Validate dimensions (allow up to 128x64 for flexibility)
    if(width == 0 || height == 0 || width > 128 || height > 64){
      return false;
    }
    
    // Calculate expected size: 4 bytes header + width * height * 3 bytes RGB
    uint32_t expected_size = 4 + (width * height * 3);
    if(size < expected_size){
      return false;
    }
    
    // Clear previous image
    clearImage();
    
    // Allocate new buffer
    data_size_ = size;
    image_data_ = new uint8_t[data_size_];
    if(!image_data_){
      return false;
    }
    
    // Copy data
    memcpy(image_data_, data, data_size_);
    
    image_width_ = width;
    image_height_ = height;
    loaded_ = true;
    
    return true;
  }
  
  /**
   * @brief Clear loaded image and free memory
   */
  void clearImage(){
    if(image_data_){
      delete[] image_data_;
      image_data_ = nullptr;
    }
    image_width_ = 0;
    image_height_ = 0;
    data_size_ = 0;
    loaded_ = false;
  }
  
  /**
   * @brief Check if an image is loaded
   * @return true if image is loaded
   */
  bool isLoaded() const{
    return loaded_;
  }
  
  /**
   * @brief Get image width
   * @return Image width in pixels
   */
  uint16_t getWidth() const{
    return image_width_;
  }
  
  /**
   * @brief Get image height
   * @return Image height in pixels
   */
  uint16_t getHeight() const{
    return image_height_;
  }
  
  /**
   * @brief Get pointer to pixel data (RGB array)
   * @return Pointer to RGB pixel data (after 4-byte header)
   */
  const uint8_t* getData() const{
    return loaded_ && image_data_ ? (image_data_ + 4) : nullptr;
  }
  
  /**
   * @brief Bicubic interpolation helper - cubic function
   */
  float cubicInterpolate(float p0, float p1, float p2, float p3, float t) const{
    float a = -0.5f * p0 + 1.5f * p1 - 1.5f * p2 + 0.5f * p3;
    float b = p0 - 2.5f * p1 + 2.0f * p2 - 0.5f * p3;
    float c = -0.5f * p0 + 0.5f * p2;
    float d = p1;
    return a * t * t * t + b * t * t + c * t + d;
  }

  /**
   * @brief Get pixel value with bicubic interpolation
   * @param u Normalized U coordinate (0-1)
   * @param v Normalized V coordinate (0-1)
   * @param channel Channel index (0=B, 1=R, 2=G based on file format)
   * @return Interpolated channel value
   */
  float sampleBicubic(float u, float v, int channel) const{
    if(!loaded_ || !image_data_) return 0.0f;
    
    const uint8_t* pixel_data = image_data_ + 4;
    
    // Convert normalized coords to pixel space
    float px = u * (image_width_ - 1);
    float py = v * (image_height_ - 1);
    
    int x = static_cast<int>(px);
    int y = static_cast<int>(py);
    
    float fx = px - x;
    float fy = py - y;
    
    // Sample 4x4 grid for bicubic interpolation
    float values[4];
    for(int j = 0; j < 4; j++){
      float row_values[4];
      for(int i = 0; i < 4; i++){
        int sx = x + i - 1;
        int sy = y + j - 1;
        
        // Clamp to image bounds
        sx = sx < 0 ? 0 : (sx >= image_width_ ? image_width_ - 1 : sx);
        sy = sy < 0 ? 0 : (sy >= image_height_ ? image_height_ - 1 : sy);
        
        int pixel_index = (sy * image_width_ + sx) * 3;
        row_values[i] = pixel_data[pixel_index + channel];
      }
      values[j] = cubicInterpolate(row_values[0], row_values[1], row_values[2], row_values[3], fx);
    }
    
    float result = cubicInterpolate(values[0], values[1], values[2], values[3], fy);
    
    // Clamp to valid range
    return result < 0.0f ? 0.0f : (result > 255.0f ? 255.0f : result);
  }

  /**
   * @brief Render sprite with transformation (translation, rotation, scale)
   * @param display HUB75 display manager
   * @param center_x X coordinate of center position (can be fractional)
   * @param center_y Y coordinate of center position (can be fractional)
   * @param rotation Rotation angle in radians (default 0)
   * @param scale_x Scale factor X (default 1.0)
   * @param scale_y Scale factor Y (default 1.0)
   */
  void renderTransformed(HUB75DisplayManager& display, float center_x, float center_y,
                        float rotation = 0.0f, float scale_x = 1.0f, float scale_y = 1.0f){
    if(!loaded_ || !image_data_){
      return;
    }
    
    float cos_angle = cosf(rotation);
    float sin_angle = sinf(rotation);
    
    // Calculate sprite bounds in screen space
    float half_w = (image_width_ * scale_x) * 0.5f;
    float half_h = (image_height_ * scale_y) * 0.5f;
    
    // Determine screen bounds to render
    int min_x = static_cast<int>(center_x - half_w - 2);
    int max_x = static_cast<int>(center_x + half_w + 2);
    int min_y = static_cast<int>(center_y - half_h - 2);
    int max_y = static_cast<int>(center_y + half_h + 2);
    
    // Clamp to display bounds
    min_x = min_x < 0 ? 0 : min_x;
    max_x = max_x >= display.getWidth() ? display.getWidth() - 1 : max_x;
    min_y = min_y < 0 ? 0 : min_y;
    max_y = max_y >= display.getHeight() ? display.getHeight() - 1 : max_y;
    
    // Render each screen pixel
    for(int screen_y = min_y; screen_y <= max_y; screen_y++){
      for(int screen_x = min_x; screen_x <= max_x; screen_x++){
        // Transform screen coords to sprite space
        float dx = screen_x - center_x;
        float dy = screen_y - center_y;
        
        // Inverse rotation
        float rotated_x = dx * cos_angle + dy * sin_angle;
        float rotated_y = -dx * sin_angle + dy * cos_angle;
        
        // Inverse scale
        float sprite_x = rotated_x / scale_x + image_width_ * 0.5f;
        float sprite_y = rotated_y / scale_y + image_height_ * 0.5f;
        
        // Check if pixel is within sprite bounds
        if(sprite_x < 0 || sprite_x >= image_width_ || 
           sprite_y < 0 || sprite_y >= image_height_){
          continue;
        }
        
        // Normalized coordinates for bicubic sampling
        float u = sprite_x / (image_width_ - 1);
        float v = sprite_y / (image_height_ - 1);
        
        // Sample with bicubic interpolation (channel order: B,R,G)
        float b = sampleBicubic(u, v, 0);
        float r = sampleBicubic(u, v, 1);
        float g = sampleBicubic(u, v, 2);
        
        // Set pixel
        display.setPixel(screen_x, screen_y, 
                        RGB(static_cast<uint8_t>(b), 
                            static_cast<uint8_t>(g), 
                            static_cast<uint8_t>(r)));
      }
    }
  }

  /**
   * @brief Render sprite centered at specified position (with bicubic interpolation)
   * @param display HUB75 display manager
   * @param center_x X coordinate of center position
   * @param center_y Y coordinate of center position
   */
  void renderCentered(HUB75DisplayManager& display, int center_x, int center_y){
    // Use transformed rendering with no rotation/scale but with bicubic interpolation
    renderTransformed(display, static_cast<float>(center_x), static_cast<float>(center_y), 
                     0.0f, 1.0f, 1.0f);
  }
  
  /**
   * @brief Render sprite on both panels (left and right)
   * @param display HUB75 display manager
   * 
   * Assumes dual 64x32 panels arranged horizontally (128x32 total)
   * Centers sprite at (32, 16) for left panel and (96, 16) for right panel
   */
  void renderOnBothPanels(HUB75DisplayManager& display){
    if(!loaded_){
      return;
    }
    
    // Render on left panel (center at x=32, y=16)
    renderCentered(display, 32, 16);
    
    // Render on right panel (center at x=96, y=16)
    renderCentered(display, 96, 16);
  }
  
private:
  uint8_t* image_data_;    // Raw image data buffer
  uint16_t image_width_;   // Image width in pixels
  uint16_t image_height_;  // Image height in pixels
  uint32_t data_size_;     // Total size of data buffer
  bool loaded_;            // Whether image is loaded
};

} // namespace arcos::manager

#endif // ARCOS_MANAGER_IMAGE_SPRITE_LOADER_HPP_
