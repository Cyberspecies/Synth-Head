/*****************************************************************
 * File:      main.cpp
 * Category:  application
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Main application for SD card image converter. Lists all
 *    images with metadata, converts them to BMP format with
 *    progress tracking, then lists the converted bitmaps.
 *****************************************************************/

#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "driver/spi_common.h"
#include "driver/sdspi_host.h"
#include "sdmmc_cmd.h"
#include "ImageConverter.h"
#include "ImageConverter.impl.hpp"

using namespace arcos::abstraction::utilities;

static const char* MAIN_TAG = "main";

// Global image converter instance
ImageConverter image_converter;

/** List all image files on SD card with metadata
 * @param directory Directory to search
 * @param file_count Output parameter for total files found
 */
void listImageFiles(const char* directory, int& file_count);

/** List all BMP files on SD card
 * @param directory Directory to search
 */
void listBmpFiles(const char* directory);

/** Recursive helper to list images with metadata */
void listImagesRecursive(const char* dir_path, int& file_count);

/** Recursive helper to list BMP files */
void listBmpsRecursive(const char* dir_path);

/** Check if file is an image */
bool isImageFile(const char* filename);

/** Get image dimensions and bit depth from file */
bool getImageMetadata(const char* filepath, uint32_t& width, uint32_t& height, uint16_t& bit_depth);

/** Format file size as human-readable string */
void formatFileSize(size_t size, char* buffer, size_t buffer_size);

/** Convert all images with progress tracking */
int convertAllImagesWithProgress(const char* directory);

/** Recursive conversion with progress */
void convertRecursiveWithProgress(const char* dir_path, int& converted_count, int& total_count, int total_images);

extern "C" void app_main(void){
  // Wait 3 seconds at startup
  vTaskDelay(pdMS_TO_TICKS(3000));
  
  ESP_LOGI(MAIN_TAG, "\n========================================");
  ESP_LOGI(MAIN_TAG, "    SD Card Image Converter v1.0");
  ESP_LOGI(MAIN_TAG, "========================================\n");
  
  // Configure SD card and image converter
  ImageConverterConfig config;
  config.cs_pin = 14;
  config.mosi_pin = 21;
  config.miso_pin = 48;
  config.clk_pin = 47;
  config.spi_frequency = 40000000; // 40 MHz
  config.chunk_size = 2048; // Process 2KB at a time for better performance
  
  // Initialize the converter
  ESP_LOGI(MAIN_TAG, "Initializing SD card...");
  if(!image_converter.init(config)){
    ESP_LOGE(MAIN_TAG, "\n[ERROR] Failed to initialize image converter!");
    ESP_LOGE(MAIN_TAG, "Please check SD card connection and try again.");
    return;
  }
  
  ESP_LOGI(MAIN_TAG, "SD card initialized successfully!\n");
  
  // Step 1: List all image files with metadata
  ESP_LOGI(MAIN_TAG, "========================================");
  ESP_LOGI(MAIN_TAG, "  STEP 1: Scanning for Image Files");
  ESP_LOGI(MAIN_TAG, "========================================\n");
  
  int file_count = 0;
  listImageFiles("/sdcard", file_count);
  
  if(file_count == 0){
    ESP_LOGI(MAIN_TAG, "\nNo image files found on SD card.");
    return;
  }
  
  ESP_LOGI(MAIN_TAG, "\nTotal images found: %d\n", file_count);
  
  // Step 2: Convert all images with progress tracking
  ESP_LOGI(MAIN_TAG, "========================================");
  ESP_LOGI(MAIN_TAG, "  STEP 2: Converting Images to BMP");
  ESP_LOGI(MAIN_TAG, "========================================\n");
  
  int converted = convertAllImagesWithProgress("/sdcard");
  
  ESP_LOGI(MAIN_TAG, "\n========================================");
  ESP_LOGI(MAIN_TAG, "  Conversion Complete: %d/%d images", converted, file_count);
  ESP_LOGI(MAIN_TAG, "========================================\n");
  
  // Step 3: List all BMP files
  ESP_LOGI(MAIN_TAG, "========================================");
  ESP_LOGI(MAIN_TAG, "  STEP 3: Listing BMP Files");
  ESP_LOGI(MAIN_TAG, "========================================\n");
  
  listBmpFiles("/sdcard");
  
  ESP_LOGI(MAIN_TAG, "\n========================================");
  ESP_LOGI(MAIN_TAG, "  All operations completed!");
  ESP_LOGI(MAIN_TAG, "========================================\n");
  
  // Keep task alive
  while(1){
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void listImageFiles(const char* directory, int& file_count){
  DIR* dir = opendir(directory);
  if(!dir){
    ESP_LOGE(MAIN_TAG, "[ERROR] Failed to open directory: %s", directory);
    return;
  }
  
  file_count = 0;
  printf("Filename                        | Size      | Dimensions  | Bit Depth\n");
  printf("----------------------------------------------------------------\n");
  
  listImagesRecursive(directory, file_count);
  closedir(dir);
}

void listImagesRecursive(const char* dir_path, int& file_count){
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
        // Recursively list subdirectories
        listImagesRecursive(full_path, file_count);
      }else{
        if(isImageFile(entry->d_name)){
          file_count++;
          
          // Get file size
          size_t file_size = st.st_size;
          char size_str[16];
          formatFileSize(file_size, size_str, sizeof(size_str));
          
          // Get image metadata
          uint32_t width = 0;
          uint32_t height = 0;
          uint16_t bit_depth = 0;
          bool has_metadata = getImageMetadata(full_path, width, height, bit_depth);
          
          // Print file information
          printf("%-30s | %-9s | ", full_path, size_str);
          
          if(has_metadata && width > 0 && height > 0){
            printf("%4lu x %-4lu | %u-bit\n", (unsigned long)width, (unsigned long)height, (unsigned int)bit_depth);
          }else{
            printf("Unknown     | Unknown\n");
          }
        }
      }
    }
  }
  
  closedir(dir);
}

void listBmpFiles(const char* directory){
  DIR* dir = opendir(directory);
  if(!dir){
    ESP_LOGE(MAIN_TAG, "[ERROR] Failed to open directory: %s", directory);
    return;
  }
  
  printf("BMP Filename                    | Size      | Dimensions  | Bit Depth\n");
  printf("----------------------------------------------------------------\n");
  
  listBmpsRecursive(directory);
  closedir(dir);
}

void listBmpsRecursive(const char* dir_path){
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
        // Recursively list subdirectories
        listBmpsRecursive(full_path);
      }else{
        size_t len = strlen(entry->d_name);
        
        // Check if file is .bmp
        if(len > 4 && strcasecmp(entry->d_name + len - 4, ".bmp") == 0){
          // Get file size
          size_t file_size = st.st_size;
          char size_str[16];
          formatFileSize(file_size, size_str, sizeof(size_str));
          
          // Get BMP metadata
          uint32_t width = 0;
          uint32_t height = 0;
          uint16_t bit_depth = 0;
          bool has_metadata = getImageMetadata(full_path, width, height, bit_depth);
          
          // Print BMP information
          printf("%-30s | %-9s | ", full_path, size_str);
          
          if(has_metadata && width > 0 && height > 0){
            printf("%4lu x %-4lu | %u-bit\n", (unsigned long)width, (unsigned long)height, (unsigned int)bit_depth);
          }else{
            printf("Unknown     | Unknown\n");
          }
        }
      }
    }
  }
  
  closedir(dir);
}

bool isImageFile(const char* filename){
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
          (len > 5 && strcasecmp(filename + len - 5, ".jpeg") == 0));
}

bool getImageMetadata(const char* filepath, uint32_t& width, uint32_t& height, uint16_t& bit_depth){
  FILE* file = fopen(filepath, "rb");
  if(!file){
    return false;
  }
  
  uint8_t header[32];
  size_t bytes_read = fread(header, 1, sizeof(header), file);
  
  if(bytes_read < 4){
    fclose(file);
    return false;
  }
  
  // Check for BMP format
  if(header[0] == 'B' && header[1] == 'M'){
    if(bytes_read < 30){
      fclose(file);
      return false;
    }
    
    // BMP dimensions are at offset 18
    width = *((uint32_t*)(header + 18));
    height = *((uint32_t*)(header + 22));
    bit_depth = *((uint16_t*)(header + 28));
    
    fclose(file);
    return true;
  }
  
  // Check for GIF format
  if(header[0] == 'G' && header[1] == 'I' && header[2] == 'F'){
    if(bytes_read < 10){
      fclose(file);
      return false;
    }
    
    // GIF width and height are at bytes 6-7 and 8-9 (little-endian)
    width = header[6] | (header[7] << 8);
    height = header[8] | (header[9] << 8);
    bit_depth = ((header[10] & 0x07) + 1); // Color depth from packed field
    
    fclose(file);
    return true;
  }
  
  // Check for JPEG format
  if(header[0] == 0xFF && header[1] == 0xD8){
    // JPEG parsing requires scanning for SOF marker
    bit_depth = 24; // Typically 24-bit
    fclose(file);
    return false; // Dimension parsing not implemented
  }
  
  // Check for PNG format
  if(header[0] == 0x89 && header[1] == 'P' && header[2] == 'N' && header[3] == 'G'){
    if(bytes_read < 24){
      fclose(file);
      return false;
    }
    
    // PNG IHDR chunk at offset 16 (width and height are big-endian)
    width = (header[16] << 24) | (header[17] << 16) | (header[18] << 8) | header[19];
    height = (header[20] << 24) | (header[21] << 16) | (header[22] << 8) | header[23];
    bit_depth = header[24]; // Bit depth
    
    fclose(file);
    return true;
  }
  
  fclose(file);
  return false;
}

void formatFileSize(size_t size, char* buffer, size_t buffer_size){
  if(size < 1024){
    snprintf(buffer, buffer_size, "%zu B", size);
  }else if(size < 1024 * 1024){
    snprintf(buffer, buffer_size, "%.1f KB", size / 1024.0);
  }else{
    snprintf(buffer, buffer_size, "%.1f MB", size / (1024.0 * 1024.0));
  }
}

int convertAllImagesWithProgress(const char* directory){
  DIR* dir = opendir(directory);
  if(!dir){
    ESP_LOGE(MAIN_TAG, "[ERROR] Failed to open directory: %s", directory);
    return 0;
  }
  closedir(dir);
  
  // First pass: count total images
  int total_images = 0;
  listImagesRecursive(directory, total_images);
  
  // Second pass: convert with progress
  int converted_count = 0;
  int processed_count = 0;
  convertRecursiveWithProgress(directory, converted_count, processed_count, total_images);
  
  return converted_count;
}

void convertRecursiveWithProgress(const char* dir_path, int& converted_count, int& total_count, int total_images){
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
        // Recursively convert subdirectories
        convertRecursiveWithProgress(full_path, converted_count, total_count, total_images);
      }else{
        if(isImageFile(entry->d_name)){
          total_count++;
          
          // Calculate progress
          int progress = (total_count * 100) / total_images;
          
          printf("[%3d%%] Converting: %s\n", progress, full_path);
          
          // Generate output path
          char output_path[256];
          strncpy(output_path, full_path, sizeof(output_path) - 1);
          output_path[sizeof(output_path) - 1] = '\0';
          
          char* last_dot = strrchr(output_path, '.');
          if(last_dot){
            strncpy(last_dot, ".bmp", sizeof(output_path) - (last_dot - output_path));
          }else{
            strncat(output_path, ".bmp", sizeof(output_path) - strlen(output_path) - 1);
          }
          
          // Check if output already exists and is same as input (skip .bmp files)
          if(strcmp(full_path, output_path) == 0){
            printf("       -> Skipped (already BMP format)\n");
            continue;
          }
          
          // Convert the image
          if(image_converter.convertImage(full_path, output_path)){
            converted_count++;
            printf("       -> Success: %s\n", output_path);
          }else{
            printf("       -> Failed\n");
          }
        }
      }
    }
  }
  
  closedir(dir);
}
