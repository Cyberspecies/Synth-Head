/*****************************************************************
 * File:      ImageConverterExample.cpp
 * Category:  examples
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Example demonstrating SD card image conversion to BMP format
 *    using streaming approach without loading entire images into RAM.
 *****************************************************************/

#include <Arduino.h>
#include "../include/ImageConverter.h"
#include "../include/ImageConverter.impl.hpp"

using namespace arcos::abstraction::utilities;

// Global image converter instance
ImageConverter image_converter;

void setup(){
  // Initialize serial for logging
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n=== Image Converter Example ===\n");
  
  // Configure SD card and image converter
  ImageConverterConfig config;
  config.cs_pin = 14;
  config.mosi_pin = 3;
  config.miso_pin = 48;
  config.clk_pin = 47;
  config.spi_frequency = 40000000; // 40 MHz
  config.chunk_size = 1024; // Process 1KB at a time
  
  // Initialize the converter
  if(!image_converter.init(config)){
    Serial.println("Failed to initialize image converter!");
    return;
  }
  
  Serial.println("Image converter initialized successfully\n");
  
  // Option 1: Convert all images in root directory
  Serial.println("Converting all images on SD card...");
  int converted = image_converter.convertAllImages("/");
  Serial.printf("Total images converted: %d\n", converted);
  
  // Option 2: Convert a specific image
  // Serial.println("\nConverting specific image...");
  // if(image_converter.convertImage("/photo.jpg", "/photo.bmp")){
  //   Serial.println("Image converted successfully!");
  // }else{
  //   Serial.println("Failed to convert image");
  // }
}

void loop(){
  // Nothing to do in loop
  delay(1000);
}
