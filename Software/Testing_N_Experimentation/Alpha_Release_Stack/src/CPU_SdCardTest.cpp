/**
 * @file CPU_SdCardTest.cpp
 * @brief SD Card Read/Write Test - Tests text, BMP, and PNG file operations
 * 
 * This test creates, verifies, reads, and deletes files on the SD card
 * to diagnose file system issues.
 */

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "sdmmc_cmd.h"

static const char* TAG = "SDTest";

// SD Card pins (from PIN_MAPPING_CPU.md)
#define SD_MISO     GPIO_NUM_14
#define SD_MOSI     GPIO_NUM_47
#define SD_CLK      GPIO_NUM_21
#define SD_CS       GPIO_NUM_48

#define MOUNT_POINT "/sdcard"
#define TEST_DIR    "/sdcard/sdtest"

static sdmmc_card_t* card = nullptr;

// ============================================================================
// SD Card Mount/Unmount
// ============================================================================

bool mountSDCard() {
    ESP_LOGI(TAG, "Mounting SD card...");
    
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };
    
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.max_freq_khz = 4000; // Start slow for reliability
    
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = SD_MOSI,
        .miso_io_num = SD_MISO,
        .sclk_io_num = SD_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };
    
    esp_err_t ret = spi_bus_initialize((spi_host_device_t)host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus: %s", esp_err_to_name(ret));
        return false;
    }
    
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = SD_CS;
    slot_config.host_id = (spi_host_device_t)host.slot;
    
    ret = esp_vfs_fat_sdspi_mount(MOUNT_POINT, &host, &slot_config, &mount_config, &card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount SD card: %s", esp_err_to_name(ret));
        return false;
    }
    
    ESP_LOGI(TAG, "SD card mounted successfully!");
    sdmmc_card_print_info(stdout, card);
    return true;
}

void unmountSDCard() {
    if (card) {
        esp_vfs_fat_sdcard_unmount(MOUNT_POINT, card);
        ESP_LOGI(TAG, "SD card unmounted");
        card = nullptr;
    }
}

// ============================================================================
// Helper Functions
// ============================================================================

void listDirectory(const char* path) {
    DIR* dir = opendir(path);
    if (!dir) {
        ESP_LOGW(TAG, "Cannot open directory: %s (errno=%d)", path, errno);
        return;
    }
    
    int count = 0;
    struct dirent* entry;
    ESP_LOGI(TAG, "Contents of %s:", path);
    while ((entry = readdir(dir)) != nullptr) {
        ESP_LOGI(TAG, "  [%d] %s", count++, entry->d_name);
    }
    ESP_LOGI(TAG, "Total entries: %d", count);
    closedir(dir);
}

bool ensureDirectory(const char* path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        return true; // Already exists
    }
    
    if (mkdir(path, 0755) != 0) {
        ESP_LOGE(TAG, "Failed to create directory %s (errno=%d: %s)", path, errno, strerror(errno));
        return false;
    }
    ESP_LOGI(TAG, "Created directory: %s", path);
    return true;
}

bool deleteFile(const char* path) {
    if (remove(path) != 0) {
        ESP_LOGE(TAG, "Failed to delete %s (errno=%d: %s)", path, errno, strerror(errno));
        return false;
    }
    ESP_LOGI(TAG, "Deleted: %s", path);
    return true;
}

bool fileExists(const char* path) {
    struct stat st;
    return stat(path, &st) == 0;
}

size_t getFileSize(const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return st.st_size;
}

// ============================================================================
// Test: Write File with Multiple Methods
// ============================================================================

bool writeFileTest(const char* path, const uint8_t* data, size_t size, const char* description) {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "=== WRITE TEST: %s ===", description);
    ESP_LOGI(TAG, "Path: %s", path);
    ESP_LOGI(TAG, "Size: %u bytes", (unsigned)size);
    
    // List directory before write
    const char* dirPath = TEST_DIR;
    listDirectory(dirPath);
    
    // Method 1: Standard fopen/fwrite
    ESP_LOGI(TAG, "Trying fopen(\"wb\")...");
    FILE* f = fopen(path, "wb");
    if (f) {
        size_t written = fwrite(data, 1, size, f);
        fflush(f);
        fsync(fileno(f));
        fclose(f);
        
        if (written == size) {
            ESP_LOGI(TAG, "SUCCESS: fopen wrote %u bytes", (unsigned)written);
            return true;
        } else {
            ESP_LOGE(TAG, "PARTIAL: fopen wrote only %u/%u bytes", (unsigned)written, (unsigned)size);
            deleteFile(path);
        }
    } else {
        ESP_LOGE(TAG, "fopen FAILED (errno=%d: %s)", errno, strerror(errno));
    }
    
    // Method 2: Low-level open/write
    ESP_LOGI(TAG, "Trying low-level open()...");
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) {
        ssize_t written = write(fd, data, size);
        fsync(fd);
        close(fd);
        
        if (written == (ssize_t)size) {
            ESP_LOGI(TAG, "SUCCESS: open() wrote %d bytes", (int)written);
            return true;
        } else {
            ESP_LOGE(TAG, "PARTIAL: open() wrote only %d/%u bytes", (int)written, (unsigned)size);
            deleteFile(path);
        }
    } else {
        ESP_LOGE(TAG, "open() FAILED (errno=%d: %s)", errno, strerror(errno));
    }
    
    // Method 3: Try with shorter filename (8.3 format test)
    ESP_LOGW(TAG, "Both methods failed. This might be a filename/filesystem issue.");
    
    return false;
}

// ============================================================================
// Test: Read and Verify File
// ============================================================================

bool readAndVerifyFile(const char* path, const uint8_t* expectedData, size_t expectedSize, const char* description) {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "=== READ & VERIFY: %s ===", description);
    
    if (!fileExists(path)) {
        ESP_LOGE(TAG, "File does not exist: %s", path);
        return false;
    }
    
    size_t actualSize = getFileSize(path);
    ESP_LOGI(TAG, "File size: %u bytes (expected: %u)", (unsigned)actualSize, (unsigned)expectedSize);
    
    if (actualSize != expectedSize) {
        ESP_LOGE(TAG, "Size mismatch!");
        return false;
    }
    
    FILE* f = fopen(path, "rb");
    if (!f) {
        ESP_LOGE(TAG, "Cannot open for reading (errno=%d: %s)", errno, strerror(errno));
        return false;
    }
    
    std::vector<uint8_t> buffer(actualSize);
    size_t bytesRead = fread(buffer.data(), 1, actualSize, f);
    fclose(f);
    
    if (bytesRead != actualSize) {
        ESP_LOGE(TAG, "Read only %u/%u bytes", (unsigned)bytesRead, (unsigned)actualSize);
        return false;
    }
    
    // Verify content
    if (memcmp(buffer.data(), expectedData, expectedSize) == 0) {
        ESP_LOGI(TAG, "SUCCESS: Content verified!");
        return true;
    } else {
        ESP_LOGE(TAG, "FAILED: Content mismatch!");
        // Show first few bytes
        ESP_LOGE(TAG, "Expected: %02X %02X %02X %02X...", 
                 expectedData[0], expectedData[1], expectedData[2], expectedData[3]);
        ESP_LOGE(TAG, "Got:      %02X %02X %02X %02X...", 
                 buffer[0], buffer[1], buffer[2], buffer[3]);
        return false;
    }
}

// ============================================================================
// Generate Test Data
// ============================================================================

std::vector<uint8_t> generateTextData(size_t lines) {
    std::vector<uint8_t> data;
    for (size_t i = 0; i < lines; i++) {
        char line[64];
        int len = snprintf(line, sizeof(line), "Test line %u: Hello SD Card!\n", (unsigned)i);
        data.insert(data.end(), line, line + len);
    }
    return data;
}

// Simple BMP header + pixel data (uncompressed 24-bit)
std::vector<uint8_t> generateBMPData(int width, int height) {
    int rowSize = ((width * 3 + 3) / 4) * 4; // Rows must be 4-byte aligned
    int pixelDataSize = rowSize * height;
    int fileSize = 54 + pixelDataSize;
    
    std::vector<uint8_t> bmp(fileSize, 0);
    
    // BMP Header (14 bytes)
    bmp[0] = 'B'; bmp[1] = 'M';
    bmp[2] = fileSize & 0xFF;
    bmp[3] = (fileSize >> 8) & 0xFF;
    bmp[4] = (fileSize >> 16) & 0xFF;
    bmp[5] = (fileSize >> 24) & 0xFF;
    bmp[10] = 54; // Pixel data offset
    
    // DIB Header (40 bytes)
    bmp[14] = 40; // Header size
    bmp[18] = width & 0xFF;
    bmp[19] = (width >> 8) & 0xFF;
    bmp[22] = height & 0xFF;
    bmp[23] = (height >> 8) & 0xFF;
    bmp[26] = 1;  // Color planes
    bmp[28] = 24; // Bits per pixel
    
    // Pixel data (simple gradient)
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int offset = 54 + y * rowSize + x * 3;
            bmp[offset + 0] = (x * 255 / width) & 0xFF;     // Blue
            bmp[offset + 1] = (y * 255 / height) & 0xFF;    // Green
            bmp[offset + 2] = 128;                           // Red
        }
    }
    
    return bmp;
}

// Simple raw binary data (simulating PNG - actual PNG is complex)
std::vector<uint8_t> generateRawImageData(int width, int height) {
    std::vector<uint8_t> data;
    
    // PNG signature
    uint8_t pngSig[] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    data.insert(data.end(), pngSig, pngSig + 8);
    
    // Just fill with pattern data (not a valid PNG, but tests binary writes)
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            data.push_back((x + y) & 0xFF);
            data.push_back((x * y) & 0xFF);
            data.push_back((x ^ y) & 0xFF);
            data.push_back(0xFF); // Alpha
        }
    }
    
    return data;
}

// ============================================================================
// Run Single File Test Cycle
// ============================================================================

bool runFileTestCycle(const char* filename, const uint8_t* data, size_t size, const char* description) {
    char path[128];
    snprintf(path, sizeof(path), "%s/%s", TEST_DIR, filename);
    
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "############################################################");
    ESP_LOGI(TAG, "# TEST CYCLE: %s", description);
    ESP_LOGI(TAG, "# File: %s", filename);
    ESP_LOGI(TAG, "# Size: %u bytes", (unsigned)size);
    ESP_LOGI(TAG, "############################################################");
    
    bool success = true;
    
    // Step 1: Write
    if (!writeFileTest(path, data, size, description)) {
        ESP_LOGE(TAG, "WRITE FAILED - aborting test cycle");
        return false;
    }
    
    vTaskDelay(pdMS_TO_TICKS(100)); // Small delay
    
    // Step 2: Verify
    if (!readAndVerifyFile(path, data, size, description)) {
        ESP_LOGE(TAG, "VERIFY FAILED");
        success = false;
    }
    
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Step 3: Delete
    if (!deleteFile(path)) {
        ESP_LOGE(TAG, "DELETE FAILED");
        success = false;
    }
    
    // Verify deletion
    if (fileExists(path)) {
        ESP_LOGE(TAG, "File still exists after deletion!");
        success = false;
    } else {
        ESP_LOGI(TAG, "File successfully deleted");
    }
    
    if (success) {
        ESP_LOGI(TAG, ">>> TEST CYCLE PASSED: %s <<<", description);
    } else {
        ESP_LOGE(TAG, ">>> TEST CYCLE FAILED: %s <<<", description);
    }
    
    return success;
}

// ============================================================================
// Test: Filename Length Limits
// ============================================================================

void testFilenameLimits() {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "############################################################");
    ESP_LOGI(TAG, "# FILENAME LENGTH TESTS");
    ESP_LOGI(TAG, "############################################################");
    
    const char* testData = "Test data for filename test\n";
    size_t dataLen = strlen(testData);
    
    // Test various filename lengths
    const char* filenames[] = {
        "a.txt",           // 1 char name
        "ab.txt",          // 2 char name
        "abcdefgh.txt",    // 8 char name (8.3 limit)
        "abcdefghi.txt",   // 9 char name (exceeds 8.3)
        "sprite_1.bin",    // Like our sprite files
        "sprite_10.bin",   // 9 chars before dot
        "preview_1.txt",   // 9 chars before dot
        "preview_10.txt",  // 10 chars before dot
        "idx.dat",         // Short name
        "index.dat",       // 5 chars
        "longfilename.txt", // Long name
        "very_long_filename_test.txt", // Very long
    };
    
    for (size_t i = 0; i < sizeof(filenames) / sizeof(filenames[0]); i++) {
        char path[128];
        snprintf(path, sizeof(path), "%s/%s", TEST_DIR, filenames[i]);
        
        ESP_LOGI(TAG, "");
        ESP_LOGI(TAG, "Testing filename: '%s' (length before dot: %d)", 
                 filenames[i], (int)(strchr(filenames[i], '.') - filenames[i]));
        
        // Try to write
        FILE* f = fopen(path, "wb");
        if (f) {
            fwrite(testData, 1, dataLen, f);
            fclose(f);
            ESP_LOGI(TAG, "  WRITE: SUCCESS");
            
            // Verify it exists
            if (fileExists(path)) {
                ESP_LOGI(TAG, "  EXISTS: YES");
                deleteFile(path);
            } else {
                ESP_LOGW(TAG, "  EXISTS: NO (written but not found!)");
            }
        } else {
            ESP_LOGE(TAG, "  WRITE: FAILED (errno=%d: %s)", errno, strerror(errno));
        }
    }
}

// ============================================================================
// Test: Multiple Files (stress test directory entries)
// ============================================================================

void testMultipleFiles(int count) {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "############################################################");
    ESP_LOGI(TAG, "# MULTIPLE FILES TEST: Creating %d files", count);
    ESP_LOGI(TAG, "############################################################");
    
    const char* testData = "Test file content\n";
    size_t dataLen = strlen(testData);
    int successCount = 0;
    int failCount = 0;
    
    // Create files
    for (int i = 0; i < count; i++) {
        char path[128];
        snprintf(path, sizeof(path), "%s/file_%d.txt", TEST_DIR, i);
        
        FILE* f = fopen(path, "wb");
        if (f) {
            fwrite(testData, 1, dataLen, f);
            fclose(f);
            successCount++;
            ESP_LOGI(TAG, "Created file_%d.txt (total: %d)", i, successCount);
        } else {
            failCount++;
            ESP_LOGE(TAG, "FAILED file_%d.txt (errno=%d: %s) - Total failures: %d", 
                     i, errno, strerror(errno), failCount);
        }
        
        // List directory periodically
        if ((i + 1) % 5 == 0 || f == nullptr) {
            listDirectory(TEST_DIR);
        }
        
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "RESULTS: %d succeeded, %d failed out of %d", successCount, failCount, count);
    
    // Cleanup
    ESP_LOGI(TAG, "Cleaning up test files...");
    for (int i = 0; i < count; i++) {
        char path[128];
        snprintf(path, sizeof(path), "%s/file_%d.txt", TEST_DIR, i);
        remove(path); // Don't care if it fails
    }
    
    listDirectory(TEST_DIR);
}

// ============================================================================
// Main Test Runner
// ============================================================================

void runAllTests() {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "============================================================");
    ESP_LOGI(TAG, "        SD CARD FILE SYSTEM TEST SUITE");
    ESP_LOGI(TAG, "============================================================");
    ESP_LOGI(TAG, "");
    
    // Ensure test directory exists
    if (!ensureDirectory(TEST_DIR)) {
        ESP_LOGE(TAG, "Cannot create test directory - aborting tests");
        return;
    }
    
    listDirectory(TEST_DIR);
    
    int passed = 0;
    int failed = 0;
    
    // ========================================
    // Test 1: Simple text file
    // ========================================
    {
        auto data = generateTextData(10);
        if (runFileTestCycle("test.txt", data.data(), data.size(), "Simple Text File")) {
            passed++;
        } else {
            failed++;
        }
    }
    
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // ========================================
    // Test 2: Small BMP (10x10)
    // ========================================
    {
        auto data = generateBMPData(10, 10);
        if (runFileTestCycle("small.bmp", data.data(), data.size(), "Small BMP 10x10")) {
            passed++;
        } else {
            failed++;
        }
    }
    
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // ========================================
    // Test 3: Small raw image (10x10)
    // ========================================
    {
        auto data = generateRawImageData(10, 10);
        if (runFileTestCycle("small.raw", data.data(), data.size(), "Small RAW 10x10")) {
            passed++;
        } else {
            failed++;
        }
    }
    
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // ========================================
    // Test 4: Large BMP (100x100)
    // ========================================
    {
        auto data = generateBMPData(100, 100);
        if (runFileTestCycle("large.bmp", data.data(), data.size(), "Large BMP 100x100")) {
            passed++;
        } else {
            failed++;
        }
    }
    
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // ========================================
    // Test 5: Large raw image (100x100)
    // ========================================
    {
        auto data = generateRawImageData(100, 100);
        if (runFileTestCycle("large.raw", data.data(), data.size(), "Large RAW 100x100")) {
            passed++;
        } else {
            failed++;
        }
    }
    
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // ========================================
    // Test 6: Filename length tests
    // ========================================
    testFilenameLimits();
    
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // ========================================
    // Test 7: Multiple files (stress test)
    // ========================================
    testMultipleFiles(15);
    
    // ========================================
    // Summary
    // ========================================
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "============================================================");
    ESP_LOGI(TAG, "        TEST SUITE COMPLETE");
    ESP_LOGI(TAG, "============================================================");
    ESP_LOGI(TAG, "  Passed: %d", passed);
    ESP_LOGI(TAG, "  Failed: %d", failed);
    ESP_LOGI(TAG, "============================================================");
    
    // Cleanup test directory
    ESP_LOGI(TAG, "Removing test directory...");
    rmdir(TEST_DIR);
}

// ============================================================================
// Main Entry Point
// ============================================================================

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  SD Card Test Application Starting");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "");
    
    vTaskDelay(pdMS_TO_TICKS(1000)); // Wait for serial monitor
    
    if (!mountSDCard()) {
        ESP_LOGE(TAG, "Failed to mount SD card - cannot run tests");
        while (1) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
    
    runAllTests();
    
    unmountSDCard();
    
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Tests complete. Entering idle loop.");
    
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
