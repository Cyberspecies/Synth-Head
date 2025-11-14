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
   * @brief Render sprite centered at specified position
   * @param display HUB75 display manager
   * @param center_x X coordinate of center position
   * @param center_y Y coordinate of center position
   */
  void renderCentered(HUB75DisplayManager& display, int center_x, int center_y){
    if(!loaded_ || !image_data_){
      return;
    }
    
    // Calculate top-left corner for centering
    int start_x = center_x - (image_width_ / 2);
    int start_y = center_y - (image_height_ / 2);
    
    // Pixel data starts after 4-byte header
    const uint8_t* pixel_data = image_data_ + 4;
    
    // Render each pixel
    for(int y = 0; y < image_height_; y++){
      for(int x = 0; x < image_width_; x++){
        int screen_x = start_x + x;
        int screen_y = start_y + y;
        
        // Skip pixels outside display bounds
        if(screen_x < 0 || screen_x >= display.getWidth() ||
           screen_y < 0 || screen_y >= display.getHeight()){
          continue;
        }
        
        // Get RGB values from pixel data
        // NOTE: CPU sends RGB, but display expects BGR - swap R and B
        int pixel_index = (y * image_width_ + x) * 3;
        uint8_t r = pixel_data[pixel_index + 0];
        uint8_t g = pixel_data[pixel_index + 1];
        uint8_t b = pixel_data[pixel_index + 2];
        
        // Set pixel on display (swap R and B channels)
        display.setPixel(screen_x, screen_y, RGB(b, g, r));
      }
    }
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
