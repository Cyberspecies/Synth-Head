/*****************************************************************
 * File:      ImageConverter.h
 * Category:  abstraction/utilities
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    SD card image converter that converts various image formats
 *    to BMP format without loading entire images into RAM.
 *    Uses streaming approach for memory-efficient conversion.
 *****************************************************************/

#ifndef ARCOS_ABSTRACTION_UTILITIES_IMAGECONVERTER_H_
#define ARCOS_ABSTRACTION_UTILITIES_IMAGECONVERTER_H_

#include <stdio.h>
#include <stdint.h>
#include <dirent.h>

namespace arcos::abstraction::utilities{

/** Image conversion configuration */
struct ImageConverterConfig{
  uint8_t cs_pin = 14;
  uint8_t mosi_pin = 3;
  uint8_t miso_pin = 48;
  uint8_t clk_pin = 47;
  uint32_t spi_frequency = 40000000; // 40 MHz
  size_t chunk_size = 1024; // Bytes to process at a time
};

/** Image converter for SD card files */
class ImageConverter{
public:
  ImageConverter();
  ~ImageConverter();
  
  /** Initialize SD card with SPI configuration
   * @param config SD card and conversion configuration
   * @return true if initialization successful
   */
  bool init(const ImageConverterConfig& config = ImageConverterConfig());
  
  /** Search for images on SD card and convert to BMP
   * @param directory Root directory to search (default: "/sdcard")
   * @return Number of images successfully converted
   */
  int convertAllImages(const char* directory = "/sdcard");
  
  /** Convert a single image file to BMP format
   * @param source_path Path to source image file
   * @param dest_path Path for output BMP file (optional, auto-generated if nullptr)
   * @return true if conversion successful
   */
  bool convertImage(const char* source_path, const char* dest_path = nullptr);
  
  /** Check if initialized */
  bool isInitialized() const { return initialized_; }
  
private:
  /** SD card initialization state */
  bool initialized_;
  
  /** Configuration */
  ImageConverterConfig config_;
  
  /** Working buffer for chunk processing */
  uint8_t* chunk_buffer_;
  
  /** Recursively search directory for image files
   * @param dir_path Directory path to search
   * @param converted_count Counter for converted images
   */
  void searchAndConvert(const char* dir_path, int& converted_count);
  
  /** Check if file is a supported image format
   * @param filename File name to check
   * @return true if file is an image
   */
  bool isImageFile(const char* filename);
  
  /** Generate output BMP filename from source path
   * @param source_path Source image path
   * @param output_buffer Buffer to store output path
   * @param buffer_size Size of output buffer
   */
  void generateBmpPath(const char* source_path, char* output_buffer, size_t buffer_size);
  
  /** Convert various image formats to BMP
   * Streaming conversion - reads and writes in chunks
   */
  bool convertToBmp(const char* source_path, const char* dest_path);
  
  /** Write BMP file header
   * @param dest_file Output file pointer
   * @param width Image width
   * @param height Image height
   * @param bits_per_pixel Bits per pixel (24 for RGB)
   * @return true if successful
   */
  bool writeBmpHeader(FILE* dest_file, uint32_t width, uint32_t height, uint16_t bits_per_pixel);
  
  /** Parse image header to get dimensions
   * @param source_file Source image file pointer
   * @param width Output width
   * @param height Output height
   * @param format Output format type
   * @return true if header parsed successfully
   */
  bool parseImageHeader(FILE* source_file, uint32_t& width, uint32_t& height, uint8_t& format);
};

} // namespace arcos::abstraction::utilities

#endif // ARCOS_ABSTRACTION_UTILITIES_IMAGECONVERTER_H_
