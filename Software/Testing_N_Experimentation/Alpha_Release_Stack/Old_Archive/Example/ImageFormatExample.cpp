/*****************************************************************
 * File:      ImageFormatExample.cpp
 * Category:  Example/Tools
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Example showing how to create image files for custom sprites
 *    that can be transferred from CPU to GPU for HUB75 display.
 * 
 * Image Format:
 *    - Width (2 bytes, little-endian)
 *    - Height (2 bytes, little-endian)
 *    - RGB pixel data (width * height * 3 bytes)
 *    - Each pixel: R, G, B (8-bit values, 0-255)
 * 
 * Example Usage:
 *    This file demonstrates how to create a simple image buffer
 *    that can be sent via file transfer from CPU to GPU.
 *****************************************************************/

#include <stdint.h>
#include <cstring>

/**
 * @brief Create a simple test pattern image
 * @param buffer Output buffer (must be pre-allocated)
 * @param width Image width (max 64 pixels recommended)
 * @param height Image height (max 32 pixels recommended)
 * @return Total size of image data in bytes
 */
uint32_t createTestImage(uint8_t* buffer, uint16_t width, uint16_t height){
  // Write header: width and height (little-endian)
  buffer[0] = width & 0xFF;
  buffer[1] = (width >> 8) & 0xFF;
  buffer[2] = height & 0xFF;
  buffer[3] = (height >> 8) & 0xFF;
  
  // Write RGB pixel data
  uint8_t* pixel_data = buffer + 4;
  
  for(int y = 0; y < height; y++){
    for(int x = 0; x < width; x++){
      int pixel_index = (y * width + x) * 3;
      
      // Create gradient pattern
      // Red increases left to right
      pixel_data[pixel_index + 0] = (x * 255) / width;
      // Green increases top to bottom
      pixel_data[pixel_index + 1] = (y * 255) / height;
      // Blue is constant
      pixel_data[pixel_index + 2] = 128;
    }
  }
  
  return 4 + (width * height * 3);
}

/**
 * @brief Create a simple smiley face image (16x16)
 * @param buffer Output buffer (must be at least 772 bytes)
 * @return Total size of image data in bytes
 */
uint32_t createSmileyFace(uint8_t* buffer){
  const uint16_t width = 16;
  const uint16_t height = 16;
  
  // Write header
  buffer[0] = width & 0xFF;
  buffer[1] = (width >> 8) & 0xFF;
  buffer[2] = height & 0xFF;
  buffer[3] = (height >> 8) & 0xFF;
  
  uint8_t* pixel_data = buffer + 4;
  
  // Clear to black
  memset(pixel_data, 0, width * height * 3);
  
  // Helper to set a pixel
  auto setPixel = [&](int x, int y, uint8_t r, uint8_t g, uint8_t b){
    if(x >= 0 && x < width && y >= 0 && y < height){
      int idx = (y * width + x) * 3;
      pixel_data[idx + 0] = r;
      pixel_data[idx + 1] = g;
      pixel_data[idx + 2] = b;
    }
  };
  
  // Draw circle outline (yellow)
  const int cx = 8;
  const int cy = 8;
  const int radius = 7;
  
  for(int angle = 0; angle < 360; angle += 10){
    float rad = angle * 3.14159f / 180.0f;
    int x = cx + static_cast<int>(radius * cosf(rad));
    int y = cy + static_cast<int>(radius * sinf(rad));
    setPixel(x, y, 255, 255, 0);
  }
  
  // Draw eyes (white)
  setPixel(5, 6, 255, 255, 255);
  setPixel(11, 6, 255, 255, 255);
  
  // Draw smile (white)
  setPixel(5, 10, 255, 255, 255);
  setPixel(6, 11, 255, 255, 255);
  setPixel(7, 11, 255, 255, 255);
  setPixel(8, 11, 255, 255, 255);
  setPixel(9, 11, 255, 255, 255);
  setPixel(10, 11, 255, 255, 255);
  setPixel(11, 10, 255, 255, 255);
  
  return 4 + (width * height * 3);
}

/**
 * @brief Example of how to send an image from CPU to GPU
 * 
 * In your CPU code, you would:
 * 1. Create image buffer
 * 2. Generate or load image data
 * 3. Use FileTransferManager to send to GPU
 */
void exampleSendImage(){
  // Example: Create a 16x16 test image
  const uint16_t width = 16;
  const uint16_t height = 16;
  const uint32_t buffer_size = 4 + (width * height * 3);  // Header + RGB data
  
  uint8_t* image_buffer = new uint8_t[buffer_size];
  
  // Generate test pattern or smiley face
  // uint32_t size = createTestImage(image_buffer, width, height);
  uint32_t size = createSmileyFace(image_buffer);
  
  // In CPU code, use FileTransferManager:
  // file_transfer.sendFile(image_buffer, size, "sprite.img");
  
  delete[] image_buffer;
}

/**
 * @brief Convert PNG/BMP image to custom format (pseudo-code)
 * 
 * You would need to:
 * 1. Load PNG/BMP using a library (lodepng, stb_image, etc.)
 * 2. Convert to RGB888 format
 * 3. Write custom format header
 * 4. Copy RGB pixel data
 */
void exampleConvertImage(){
  // Pseudo-code for converting existing images:
  
  // 1. Load image using lodepng or similar
  // unsigned char* pixels;
  // unsigned width, height;
  // lodepng_decode24_file(&pixels, &width, &height, "input.png");
  
  // 2. Create output buffer
  // uint32_t output_size = 4 + (width * height * 3);
  // uint8_t* output = new uint8_t[output_size];
  
  // 3. Write header
  // output[0] = width & 0xFF;
  // output[1] = (width >> 8) & 0xFF;
  // output[2] = height & 0xFF;
  // output[3] = (height >> 8) & 0xFF;
  
  // 4. Copy RGB data
  // memcpy(output + 4, pixels, width * height * 3);
  
  // 5. Send to GPU via file transfer
  // file_transfer.sendFile(output, output_size, "custom.img");
}

/**
 * @brief Recommended image sizes for HUB75 display
 * 
 * The dual HUB75 display is 128x32 pixels total (two 64x32 panels)
 * Each panel displays the sprite centered at (32, 16) and (96, 16)
 * 
 * Recommended sizes:
 * - 16x16: Small icon, good detail
 * - 24x24: Medium size, balanced
 * - 32x32: Full panel height (will be clipped on sides if > 64 width)
 * - 64x32: Full panel size
 * 
 * Keep in mind:
 * - Larger images use more memory
 * - File transfer takes longer for larger images
 * - Both panels show the same sprite (mirrored display)
 */
