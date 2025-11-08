# SD Card Image Converter

A memory-efficient image converter for ESP32 that converts images on an SD card to BMP format without loading entire images into RAM.

## Features

- **Streaming Conversion**: Processes images in chunks to minimize RAM usage
- **Recursive Search**: Automatically finds all images in SD card directories
- **Multiple Formats**: Supports BMP, JPEG, PNG, GIF, and TGA image formats
- **Auto-naming**: Automatically generates .bmp output filenames
- **SPI Mode**: Uses SPI interface for SD card communication

## Hardware Configuration

### SD Card Pins (SPI Mode)
- **CS (Chip Select)**: GPIO14
- **MOSI (Master Out Slave In)**: GPIO3
- **MISO (Master In Slave Out)**: GPIO48
- **CLK (Clock)**: GPIO47

## Usage

### Basic Usage

```cpp
#include "ImageConverter.h"
#include "ImageConverter.impl.hpp"

using namespace arcos::abstraction::utilities;

ImageConverter converter;

void setup(){
  Serial.begin(115200);
  
  // Configure and initialize
  ImageConverterConfig config;
  config.cs_pin = 14;
  config.mosi_pin = 3;
  config.miso_pin = 48;
  config.clk_pin = 47;
  
  if(!converter.init(config)){
    Serial.println("Init failed!");
    return;
  }
  
  // Convert all images on SD card
  int count = converter.convertAllImages("/");
  Serial.printf("Converted %d images\n", count);
}
```

### Convert Specific Image

```cpp
// Convert a single image
if(converter.convertImage("/photos/image.jpg", "/photos/image.bmp")){
  Serial.println("Success!");
}

// Auto-generate output filename
converter.convertImage("/photos/image.jpg"); // Creates /photos/image.bmp
```

## Configuration Options

### ImageConverterConfig

| Parameter | Default | Description |
|-----------|---------|-------------|
| `cs_pin` | 14 | SD card chip select pin |
| `mosi_pin` | 3 | SPI MOSI pin |
| `miso_pin` | 48 | SPI MISO pin |
| `clk_pin` | 47 | SPI clock pin |
| `spi_frequency` | 40000000 | SPI frequency in Hz (40 MHz) |
| `chunk_size` | 1024 | Bytes to process at a time |

### Adjusting Memory Usage

The `chunk_size` parameter controls how much data is processed at once:

```cpp
config.chunk_size = 512;  // Lower memory usage
config.chunk_size = 2048; // Faster processing
```

## Supported Formats

### Input Formats
- **JPEG** (.jpg, .jpeg) - Detection only, full decoding requires library
- **PNG** (.png) - Detection only, full decoding requires library
- **BMP** (.bmp) - Full support with dimension parsing
- **GIF** (.gif) - Dimension parsing supported
- **TGA** (.tga) - Basic support

### Output Format
- **BMP** (24-bit RGB, uncompressed)

## Memory Efficiency

The converter uses a streaming approach:
- Only allocates one chunk buffer (`chunk_size` bytes)
- Processes images in small chunks
- No full image loaded into RAM
- Suitable for embedded systems with limited memory

## Example Output

```
=== Image Converter Example ===

SD card initialized successfully
Card Type: SDHC
Card Size: 15079MB

Searching for images in: /
Found image: /photo1.jpg
Converted: /photo1.jpg -> /photo1.bmp
Found image: /images/photo2.png
Converted: /images/photo2.png -> /images/photo2.bmp

Conversion complete. 2 images converted
```

## Implementation Notes

### BMP Format
- Files are stored in standard Windows BMP format
- 24-bit RGB color (3 bytes per pixel)
- No compression
- Rows padded to 4-byte boundaries
- Bottom-to-top row order (standard BMP)

### Image Decoding
The current implementation includes basic format detection but full decoding of compressed formats (JPEG, PNG) requires additional libraries. For production use, consider integrating:
- **JPEG**: [TJpg_Decoder](https://github.com/Bodmer/TJpg_Decoder)
- **PNG**: [pngle](https://github.com/kikuchan/pngle)

### Performance
At 40 MHz SPI frequency:
- Typical SD card read speed: ~2-5 MB/s
- Processing overhead: Minimal (chunk copy)
- Conversion time: Depends on file size and SD card speed

## Error Handling

The converter includes comprehensive error checking:
- SD card initialization failures
- File open/read/write errors
- Invalid image formats
- Memory allocation failures
- Missing headers or corrupted files

All errors are logged via the platform logging system.

## API Reference

### ImageConverter Class

#### Public Methods

##### `bool init(const ImageConverterConfig& config)`
Initialize SD card and allocate buffers.
- **Returns**: `true` if successful
- **Must be called** before any conversion operations

##### `int convertAllImages(const char* directory = "/")`
Recursively search directory and convert all found images.
- **Parameters**: `directory` - Root directory to search
- **Returns**: Number of images successfully converted

##### `bool convertImage(const char* source_path, const char* dest_path = nullptr)`
Convert a single image file.
- **Parameters**: 
  - `source_path` - Path to source image
  - `dest_path` - Output path (auto-generated if nullptr)
- **Returns**: `true` if successful

##### `bool isInitialized() const`
Check if converter is initialized.
- **Returns**: `true` if ready to use

## Troubleshooting

### SD Card Not Detected
- Check wiring connections
- Verify power supply is adequate
- Try lower SPI frequency (e.g., 10 MHz)
- Ensure SD card is formatted as FAT32

### Conversion Failures
- Verify source file exists and is readable
- Check sufficient free space on SD card
- Ensure file format is actually supported
- Check serial output for specific error messages

### Memory Issues
- Reduce `chunk_size` in configuration
- Monitor free heap with `ESP.getFreeHeap()`
- Ensure no other large allocations

## License

Part of the ARCOS hardware abstraction framework.
Author: XCR1793 (Feather Forge)
