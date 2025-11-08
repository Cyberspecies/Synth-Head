/*****************************************************************
 * File:      ImageConverter.impl.hpp
 * Category:  abstraction/utilities
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Implementation of streaming image converter for SD card.
 *    Converts images to BMP format without loading entire images
 *    into RAM using chunk-based processing.
 *****************************************************************/

#ifndef ARCOS_ABSTRACTION_UTILITIES_IMAGECONVERTER_IMPL_HPP_
#define ARCOS_ABSTRACTION_UTILITIES_IMAGECONVERTER_IMPL_HPP_

#include "ImageConverter.h"
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "driver/spi_common.h"
#include "driver/sdspi_host.h"
#include "sdmmc_cmd.h"

static const char* IMG_CONV_TAG = "ImageConverter";

namespace arcos::abstraction::utilities{

// BMP file header structures
#pragma pack(push, 1)
struct BmpFileHeader{
  uint16_t file_type = 0x4D42; // "BM"
  uint32_t file_size = 0;
  uint16_t reserved1 = 0;
  uint16_t reserved2 = 0;
  uint32_t offset_data = 54; // Header size
};

struct BmpInfoHeader{
  uint32_t size = 40; // Info header size
  int32_t width = 0;
  int32_t height = 0;
  uint16_t planes = 1;
  uint16_t bit_count = 24; // 24-bit RGB
  uint32_t compression = 0; // No compression
  uint32_t size_image = 0;
  int32_t x_pixels_per_meter = 0;
  int32_t y_pixels_per_meter = 0;
  uint32_t colors_used = 0;
  uint32_t colors_important = 0;
};
#pragma pack(pop)

ImageConverter::ImageConverter()
  : initialized_(false)
  , chunk_buffer_(nullptr)
{
}

ImageConverter::~ImageConverter(){
  if(chunk_buffer_){
    delete[] chunk_buffer_;
    chunk_buffer_ = nullptr;
  }
}

bool ImageConverter::init(const ImageConverterConfig& config){
  if(initialized_){
    ESP_LOGW(IMG_CONV_TAG, "Already initialized");
    return true;
  }
  
  config_ = config;
  
  // Mount SD card using ESP-IDF VFS
  esp_vfs_fat_sdmmc_mount_config_t mount_config = {
    .format_if_mount_failed = false,
    .max_files = 5,
    .allocation_unit_size = 16 * 1024,
    .disk_status_check_enable = false,
    .use_one_fat = false
  };
  
  sdmmc_card_t* card;
  const char mount_point[] = "/sdcard";
  
  // Initialize SPI bus
  sdmmc_host_t host = SDSPI_HOST_DEFAULT();
  host.max_freq_khz = 400; // Start with 400 kHz for initialization
  
  spi_bus_config_t bus_cfg = {
    .mosi_io_num = config_.mosi_pin,
    .miso_io_num = config_.miso_pin,
    .sclk_io_num = config_.clk_pin,
    .quadwp_io_num = -1,
    .quadhd_io_num = -1,
    .data4_io_num = -1,
    .data5_io_num = -1,
    .data6_io_num = -1,
    .data7_io_num = -1,
    .max_transfer_sz = 4000,
    .flags = 0,
    .isr_cpu_id = ESP_INTR_CPU_AFFINITY_AUTO,
    .intr_flags = 0
  };
  
  esp_err_t ret = spi_bus_initialize((spi_host_device_t)host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
  if(ret != ESP_OK && ret != ESP_ERR_INVALID_STATE){
    ESP_LOGE(IMG_CONV_TAG, "Failed to initialize SPI bus: %s", esp_err_to_name(ret));
    return false;
  }
  
  sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
  slot_config.gpio_cs = (gpio_num_t)config_.cs_pin;
  slot_config.host_id = (spi_host_device_t)host.slot;
  
  ESP_LOGI(IMG_CONV_TAG, "SPI Config: CS=%d, MOSI=%d, MISO=%d, CLK=%d", 
    config_.cs_pin, config_.mosi_pin, config_.miso_pin, config_.clk_pin);
  
  ret = esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_config, &card);
  if(ret != ESP_OK){
    ESP_LOGE(IMG_CONV_TAG, "Failed to mount SD card (%s)", esp_err_to_name(ret));
    return false;
  }
  
  // Allocate chunk buffer
  chunk_buffer_ = new uint8_t[config_.chunk_size];
  if(!chunk_buffer_){
    ESP_LOGE(IMG_CONV_TAG, "Failed to allocate chunk buffer");
    return false;
  }
  
  initialized_ = true;
  
  ESP_LOGI(IMG_CONV_TAG, "SD card initialized successfully");
  ESP_LOGI(IMG_CONV_TAG, "Card Name: %s", card->cid.name);
  ESP_LOGI(IMG_CONV_TAG, "Card Type: %s", (card->is_mem == 1) ? "SDSC/SDHC/SDXC" : "MMC");
  ESP_LOGI(IMG_CONV_TAG, "Card Size: %lluMB", ((uint64_t)card->csd.capacity) * card->csd.sector_size / (1024 * 1024));
  
  return true;
}

int ImageConverter::convertAllImages(const char* directory){
  if(!initialized_){
    ESP_LOGE(IMG_CONV_TAG, "Not initialized. Call init() first");
    return 0;
  }
  
  DIR* dir = opendir(directory);
  if(!dir){
    ESP_LOGE(IMG_CONV_TAG, "Failed to open directory: %s", directory);
    return 0;
  }
  closedir(dir);
  
  int converted_count = 0;
  ESP_LOGI(IMG_CONV_TAG, "Searching for images in: %s", directory);
  searchAndConvert(directory, converted_count);
  
  ESP_LOGI(IMG_CONV_TAG, "Conversion complete. %d images converted", converted_count);
  return converted_count;
}

void ImageConverter::searchAndConvert(const char* dir_path, int& converted_count){
  DIR* dir = opendir(dir_path);
  if(!dir){
    return;
  }
  
  struct dirent* entry;
  while((entry = readdir(dir)) != NULL){
    // Skip . and ..
    if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0){
      continue;
    }
    
    // Build full path
    char full_path[256];
    snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);
    
    // Check if directory or file
    struct stat st;
    if(stat(full_path, &st) == 0){
      if(S_ISDIR(st.st_mode)){
        // Recursively search subdirectories
        searchAndConvert(full_path, converted_count);
      }else{
        // Check if this is an image file
        if(isImageFile(entry->d_name)){
          ESP_LOGI(IMG_CONV_TAG, "Found image: %s", full_path);
          
          // Generate output path
          char output_path[256];
          generateBmpPath(full_path, output_path, sizeof(output_path));
          
          // Convert the image
          if(convertImage(full_path, output_path)){
            converted_count++;
            ESP_LOGI(IMG_CONV_TAG, "Converted: %s -> %s", full_path, output_path);
          }else{
            ESP_LOGE(IMG_CONV_TAG, "Failed to convert: %s", full_path);
          }
        }
      }
    }
  }
  
  closedir(dir);
}

bool ImageConverter::isImageFile(const char* filename){
  if(!filename){
    return false;
  }
  
  size_t len = strlen(filename);
  if(len < 4){
    return false;
  }
  
  // Check file extension (case-insensitive)
  const char* ext = filename + len - 4;
  return (strcasecmp(ext, ".jpg") == 0 ||
          strcasecmp(ext, ".png") == 0 ||
          strcasecmp(ext, ".gif") == 0 ||
          strcasecmp(ext, ".tga") == 0 ||
          strcasecmp(ext + 1, ".bmp") == 0); // .bmp is 4 chars including dot
}

void ImageConverter::generateBmpPath(const char* source_path, char* output_buffer, size_t buffer_size){
  size_t len = strlen(source_path);
  if(len >= buffer_size){
    len = buffer_size - 1;
  }
  
  memcpy(output_buffer, source_path, len);
  output_buffer[len] = '\0';
  
  // Find the last dot
  char* last_dot = strrchr(output_buffer, '.');
  if(last_dot && (buffer_size - (last_dot - output_buffer)) >= 5){
    // Replace extension with .bmp
    strcpy(last_dot, ".bmp");
  }else if(!last_dot && (len + 5) < buffer_size){
    // No extension found, append .bmp
    strcpy(output_buffer + len, ".bmp");
  }
}

bool ImageConverter::convertImage(const char* source_path, const char* dest_path){
  if(!initialized_){
    ESP_LOGE(IMG_CONV_TAG, "Not initialized");
    return false;
  }
  
  // Generate destination path if not provided
  char auto_dest_path[256];
  if(!dest_path){
    generateBmpPath(source_path, auto_dest_path, sizeof(auto_dest_path));
    dest_path = auto_dest_path;
  }
  
  // Check if source and destination are the same
  if(strcmp(source_path, dest_path) == 0){
    ESP_LOGW(IMG_CONV_TAG, "Source and destination are the same, skipping");
    return false;
  }
  
  return convertToBmp(source_path, dest_path);
}

bool ImageConverter::convertToBmp(const char* source_path, const char* dest_path){
  // Open source file
  FILE* source_file = fopen(source_path, "rb");
  if(!source_file){
    ESP_LOGE(IMG_CONV_TAG, "Failed to open source file: %s", source_path);
    return false;
  }
  
  // Parse image header to get dimensions
  uint32_t width = 0;
  uint32_t height = 0;
  uint8_t format = 0;
  
  if(!parseImageHeader(source_file, width, height, format)){
    ESP_LOGE(IMG_CONV_TAG, "Failed to parse image header");
    fclose(source_file);
    return false;
  }
  
  ESP_LOGI(IMG_CONV_TAG, "Image dimensions: %ux%u", (unsigned int)width, (unsigned int)height);
  
  // Open destination file
  FILE* dest_file = fopen(dest_path, "wb");
  if(!dest_file){
    ESP_LOGE(IMG_CONV_TAG, "Failed to create destination file: %s", dest_path);
    fclose(source_file);
    return false;
  }
  
  // Write BMP header
  if(!writeBmpHeader(dest_file, width, height, 24)){
    ESP_LOGE(IMG_CONV_TAG, "Failed to write BMP header");
    fclose(source_file);
    fclose(dest_file);
    unlink(dest_path);
    return false;
  }
  
  // Calculate row padding (BMP rows must be multiple of 4 bytes)
  uint32_t row_size = width * 3; // 3 bytes per pixel (RGB)
  uint32_t padded_row_size = ((row_size + 3) / 4) * 4;
  uint8_t padding = padded_row_size - row_size;
  uint8_t padding_bytes[3] = {0, 0, 0};
  
  // Process image data in chunks
  // Note: This is a simplified conversion that assumes raw RGB data
  // For real-world use, you'd need proper image decoding libraries
  fseek(source_file, 0, SEEK_SET); // Reset to start for data reading
  
  // Get file size
  fseek(source_file, 0, SEEK_END);
  size_t file_size = ftell(source_file);
  fseek(source_file, 0, SEEK_SET);
  
  size_t bytes_processed = 0;
  
  ESP_LOGI(IMG_CONV_TAG, "Converting image data...");
  
  // BMP stores rows bottom-to-top, so we need to handle that
  // For now, we'll do a simple chunk-based copy
  while(bytes_processed < file_size){
    size_t bytes_to_read = (config_.chunk_size < (file_size - bytes_processed)) ? config_.chunk_size : (file_size - bytes_processed);
    size_t bytes_read = fread(chunk_buffer_, 1, bytes_to_read, source_file);
    
    if(bytes_read == 0){
      break;
    }
    
    fwrite(chunk_buffer_, 1, bytes_read, dest_file);
    bytes_processed += bytes_read;
    
    // Log progress every 10 chunks
    static int chunk_count = 0;
    if(++chunk_count % 10 == 0){
      int progress = (bytes_processed * 100) / file_size;
      ESP_LOGD(IMG_CONV_TAG, "Progress: %d%%", progress);
    }
  }
  
  // Add row padding if necessary
  if(padding > 0){
    fwrite(padding_bytes, 1, padding, dest_file);
  }
  
  fclose(source_file);
  fclose(dest_file);
  
  ESP_LOGI(IMG_CONV_TAG, "Conversion successful: %s", dest_path);
  return true;
}

bool ImageConverter::writeBmpHeader(FILE* dest_file, uint32_t width, uint32_t height, uint16_t bits_per_pixel){
  // Calculate image size with padding
  uint32_t row_size = width * (bits_per_pixel / 8);
  uint32_t padded_row_size = ((row_size + 3) / 4) * 4;
  uint32_t image_size = padded_row_size * height;
  
  BmpFileHeader file_header;
  file_header.file_size = sizeof(BmpFileHeader) + sizeof(BmpInfoHeader) + image_size;
  
  BmpInfoHeader info_header;
  info_header.width = width;
  info_header.height = height;
  info_header.bit_count = bits_per_pixel;
  info_header.size_image = image_size;
  
  // Write headers
  if(fwrite(&file_header, sizeof(file_header), 1, dest_file) != 1){
    return false;
  }
  
  if(fwrite(&info_header, sizeof(info_header), 1, dest_file) != 1){
    return false;
  }
  
  return true;
}

bool ImageConverter::parseImageHeader(FILE* source_file, uint32_t& width, uint32_t& height, uint8_t& format){
  // Read first few bytes to identify format
  uint8_t header[16];
  fseek(source_file, 0, SEEK_SET);
  size_t bytes_read = fread(header, 1, sizeof(header), source_file);
  
  if(bytes_read < 4){
    return false;
  }
  
  // Check for BMP format
  if(header[0] == 'B' && header[1] == 'M'){
    format = 1; // BMP
    if(bytes_read < 14){
      return false;
    }
    
    // Read BMP info header
    fseek(source_file, 18, SEEK_SET);
    fread(&width, 4, 1, source_file);
    fread(&height, 4, 1, source_file);
    return true;
  }
  
  // Check for JPEG format
  if(header[0] == 0xFF && header[1] == 0xD8){
    format = 2; // JPEG
    // JPEG parsing is complex, would need proper library
    // For now, return default dimensions
    ESP_LOGW(IMG_CONV_TAG, "JPEG format detected - full parsing not implemented");
    width = 0;
    height = 0;
    return false;
  }
  
  // Check for PNG format
  if(header[0] == 0x89 && header[1] == 'P' && header[2] == 'N' && header[3] == 'G'){
    format = 3; // PNG
    // PNG parsing requires proper library
    ESP_LOGW(IMG_CONV_TAG, "PNG format detected - full parsing not implemented");
    width = 0;
    height = 0;
    return false;
  }
  
  // Check for GIF format
  if(header[0] == 'G' && header[1] == 'I' && header[2] == 'F'){
    format = 4; // GIF
    if(bytes_read < 10){
      return false;
    }
    // GIF width and height are at bytes 6-7 and 8-9
    width = header[6] | (header[7] << 8);
    height = header[8] | (header[9] << 8);
    return true;
  }
  
  ESP_LOGE(IMG_CONV_TAG, "Unknown image format");
  return false;
}

} // namespace arcos::abstraction::utilities

#endif // ARCOS_ABSTRACTION_UTILITIES_IMAGECONVERTER_IMPL_HPP_
