/*****************************************************************
 * File:      uart_gpu_sensor_display.cpp
 * Category:  communication/applications
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    GPU-side application that receives sensor data from CPU via
 *    UART at 60Hz, displays it on OLED SH1107 with page-based
 *    navigation using buttons A and B.
 * 
 * Hardware:
 *    - ESP32-S3 (GPU)
 *    - OLED SH1107 128x128 display (I2C: SDA=GPIO2, SCL=GPIO1)
 *    - UART from CPU: RX=GPIO13, TX=GPIO12
 * 
 * Display Layout:
 *    - Page 0: IMU Data (Accelerometer, Gyroscope, Magnetometer)
 *    - Page 1: Environmental Data (Temperature, Humidity, Pressure)
 *    - Page 2: GPS Data (Position, Satellites, Time)
 *    - Page 3: Microphone Data (Audio levels, dB)
 *    - Page 4: System Info (FPS, Button states)
 * 
 * Controls:
 *    - Button A: Previous page
 *    - Button B: Next page
 *****************************************************************/

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "Drivers/UART Comms/GpuUartBidirectional.hpp"
#include "abstraction/drivers/components/OLED/driver_oled_sh1107.hpp"

using namespace arcos::communication;
using namespace arcos::abstraction;

static const char* TAG = "GPU_SENSOR_DISPLAY";

// ============== Display Configuration ==============
constexpr int DISPLAY_WIDTH = 128;
constexpr int DISPLAY_HEIGHT = 128;
constexpr int LINE_HEIGHT = 10;
constexpr int LINES_PER_PAGE = 12;
constexpr int TOTAL_PAGES = 5;

// ============== Microphone Graph Configuration ==============
constexpr float MIC_GRAPH_DURATION_SEC = 1.5f;  // Graph width in seconds (adjustable)
constexpr int MIC_GRAPH_WIDTH = 120;            // Graph width in pixels
constexpr int MIC_GRAPH_HEIGHT = 40;            // Graph height in pixels
constexpr int MIC_GRAPH_SAMPLES = MIC_GRAPH_WIDTH;  // One sample per pixel
constexpr float MIC_DB_MIN = -60.0f;            // Minimum dB for graph
constexpr float MIC_DB_MAX = 0.0f;              // Maximum dB for graph

// ============== Global Instances ==============
GpuUartBidirectional uart_comm;
DRIVER_OLED_SH1107 oled_display;

// ============== Shared Data (Protected by Mutex) ==============
SemaphoreHandle_t data_mutex;
SensorDataPayload current_sensor_data;
bool data_received = false;
uint32_t last_data_time = 0;

// ============== Display State ==============
int current_page = 0;
bool button_a_prev = false;
bool button_b_prev = false;

// ============== Microphone Graph State ==============
float mic_history[MIC_GRAPH_SAMPLES];          // Circular buffer for dB levels
int mic_history_index = 0;                     // Current write position
uint32_t last_mic_sample_time = 0;             // Last time we added a sample
uint32_t mic_sample_interval_ms = 0;           // Calculated from graph duration

// ============== Statistics ==============
struct DisplayStats{
  uint32_t frames_received = 0;
  uint32_t display_updates = 0;
  uint32_t last_report_time = 0;
  uint32_t fps = 0;
};
DisplayStats stats;

/**
 * @brief Initialize OLED display
 */
bool initializeDisplay(){
  ESP_LOGI(TAG, "Initializing OLED SH1107 display...");
  
  // Initialize I2C bus: bus_id=0, SDA=GPIO2, SCL=GPIO1, 400kHz
  HalResult i2c_result = ESP32S3_I2C::Initialize(0, 2, 1, 400000);
  if(i2c_result != HalResult::Success){
    ESP_LOGE(TAG, "Failed to initialize I2C bus!");
    return false;
  }
  
  // Initialize OLED with custom configuration
  OLEDConfig config;
  config.contrast = 0xCF;
  config.flip_horizontal = true;
  config.flip_vertical = true;
  
  if(!oled_display.initialize(config)){
    ESP_LOGE(TAG, "Failed to initialize OLED display!");
    return false;
  }
  
  // Flip display upside down
  if(!oled_display.setUpsideDown(true)){
    ESP_LOGW(TAG, "Warning: Failed to set display upside down");
  }
  
  ESP_LOGI(TAG, "OLED display initialized successfully");
  return true;
}

/**
 * @brief Draw text at specified position
 */
void drawText(int x, int y, const char* text){
  oled_display.drawString(x, y, text, true);
}

/**
 * @brief Clear display buffer
 */
void clearDisplay(){
  oled_display.clearBuffer();
}

/**
 * @brief Update display (flush buffer)
 */
void updateDisplay(){
  oled_display.updateDisplay();
}

/**
 * @brief Display Page 0: IMU Data
 */
void displayImuPage(const SensorDataPayload& data){
  clearDisplay();
  
  drawText(0, 0, "===== IMU DATA =====");
  
  if(data.getImuValid()){
    char buf[32];
    
    drawText(0, 12, "Accel (g):");
    snprintf(buf, sizeof(buf), " X:%.2f", data.accel_x);
    drawText(0, 22, buf);
    snprintf(buf, sizeof(buf), " Y:%.2f", data.accel_y);
    drawText(0, 32, buf);
    snprintf(buf, sizeof(buf), " Z:%.2f", data.accel_z);
    drawText(0, 42, buf);
    
    drawText(0, 54, "Gyro (dps):");
    snprintf(buf, sizeof(buf), " X:%.1f", data.gyro_x);
    drawText(0, 64, buf);
    snprintf(buf, sizeof(buf), " Y:%.1f", data.gyro_y);
    drawText(0, 74, buf);
    snprintf(buf, sizeof(buf), " Z:%.1f", data.gyro_z);
    drawText(0, 84, buf);
    
    drawText(0, 96, "Mag (uT):");
    snprintf(buf, sizeof(buf), " X:%.1f", data.mag_x);
    drawText(0, 106, buf);
    snprintf(buf, sizeof(buf), " Y:%.1f Z:%.1f", data.mag_y, data.mag_z);
    drawText(0, 116, buf);
  }else{
    drawText(10, 60, "NO IMU DATA");
  }
  
  updateDisplay();
}

/**
 * @brief Display Page 1: Environmental Data
 */
void displayEnvironmentalPage(const SensorDataPayload& data){
  clearDisplay();
  
  drawText(0, 0, "=== ENVIRONMENT ===");
  
  if(data.getEnvValid()){
    char buf[32];
    
    drawText(0, 20, "Temperature:");
    snprintf(buf, sizeof(buf), "  %.2f C", data.temperature);
    drawText(0, 32, buf);
    
    drawText(0, 50, "Humidity:");
    snprintf(buf, sizeof(buf), "  %.1f %%", data.humidity);
    drawText(0, 62, buf);
    
    drawText(0, 80, "Pressure:");
    snprintf(buf, sizeof(buf), "  %.0f Pa", data.pressure);
    drawText(0, 92, buf);
    snprintf(buf, sizeof(buf), "  %.2f hPa", data.pressure / 100.0f);
    drawText(0, 104, buf);
  }else{
    drawText(10, 60, "NO ENV DATA");
  }
  
  updateDisplay();
}

/**
 * @brief Display Page 2: GPS Data
 */
void displayGpsPage(const SensorDataPayload& data){
  clearDisplay();
  
  drawText(0, 0, "===== GPS DATA =====");
  
  if(data.getGpsValid()){
    char buf[32];
    
    drawText(0, 12, "Position:");
    snprintf(buf, sizeof(buf), " Lat:%.5f", data.latitude);
    drawText(0, 22, buf);
    snprintf(buf, sizeof(buf), " Lon:%.5f", data.longitude);
    drawText(0, 32, buf);
    snprintf(buf, sizeof(buf), " Alt:%.1fm", data.altitude);
    drawText(0, 42, buf);
    
    drawText(0, 54, "Navigation:");
    snprintf(buf, sizeof(buf), " Spd:%.1fkn", data.speed_knots);
    drawText(0, 64, buf);
    snprintf(buf, sizeof(buf), " Crs:%.1fdeg", data.course);
    drawText(0, 74, buf);
    
    drawText(0, 86, "Status:");
    snprintf(buf, sizeof(buf), " Sats:%u Fix:%u", data.gps_satellites, data.getGpsFixQuality());
    drawText(0, 96, buf);
    
    snprintf(buf, sizeof(buf), "Time: %02u:%02u:%02u", 
      data.gps_hour, data.gps_minute, data.gps_second);
    drawText(0, 108, buf);
  }else{
    drawText(10, 60, "NO GPS FIX");
  }
  
  updateDisplay();
}

/**
 * @brief Add microphone sample to history buffer
 */
void addMicSample(float db_level){
  mic_history[mic_history_index] = db_level;
  mic_history_index = (mic_history_index + 1) % MIC_GRAPH_SAMPLES;
}

/**
 * @brief Draw microphone waveform graph
 */
void drawMicGraph(){
  const int graph_x = 4;
  const int graph_y = 70;
  
  // Draw graph border
  oled_display.drawRect(graph_x - 1, graph_y - 1, MIC_GRAPH_WIDTH + 2, MIC_GRAPH_HEIGHT + 2, false, true);
  
  // Draw center line (for reference)
  int mid_y = graph_y + MIC_GRAPH_HEIGHT / 2;
  for(int x = 0; x < MIC_GRAPH_WIDTH; x += 4){
    oled_display.setPixel(graph_x + x, mid_y, true);
  }
  
  // Draw waveform
  for(int i = 0; i < MIC_GRAPH_SAMPLES - 1; i++){
    // Get samples (oldest to newest, scrolling left)
    int idx1 = (mic_history_index + i) % MIC_GRAPH_SAMPLES;
    int idx2 = (mic_history_index + i + 1) % MIC_GRAPH_SAMPLES;
    
    float db1 = mic_history[idx1];
    float db2 = mic_history[idx2];
    
    // Clamp to range
    if(db1 < MIC_DB_MIN) db1 = MIC_DB_MIN;
    if(db1 > MIC_DB_MAX) db1 = MIC_DB_MAX;
    if(db2 < MIC_DB_MIN) db2 = MIC_DB_MIN;
    if(db2 > MIC_DB_MAX) db2 = MIC_DB_MAX;
    
    // Map dB to pixel Y coordinate (inverted - higher dB = lower Y)
    int y1 = graph_y + MIC_GRAPH_HEIGHT - 1 - (int)((db1 - MIC_DB_MIN) / (MIC_DB_MAX - MIC_DB_MIN) * (MIC_GRAPH_HEIGHT - 1));
    int y2 = graph_y + MIC_GRAPH_HEIGHT - 1 - (int)((db2 - MIC_DB_MIN) / (MIC_DB_MAX - MIC_DB_MIN) * (MIC_GRAPH_HEIGHT - 1));
    
    // Draw line between consecutive points
    oled_display.drawLine(graph_x + i, y1, graph_x + i + 1, y2, true);
  }
}

/**
 * @brief Display Page 3: Microphone Data with Waveform Graph
 */
void displayMicrophonePage(const SensorDataPayload& data){
  clearDisplay();
  
  drawText(0, 0, "==== MIC DATA =====");
  
  if(data.getMicValid()){
    char buf[32];
    
    // Current dB level
    drawText(0, 12, "Level:");
    snprintf(buf, sizeof(buf), " %.1f dB", data.mic_db_level);
    drawText(42, 12, buf);
    
    // Clipping indicator
    if(data.getMicClipping()){
      drawText(90, 12, "[CLIP]");
    }
    
    // Peak amplitude
    drawText(0, 24, "Peak:");
    snprintf(buf, sizeof(buf), " %ld", data.mic_peak_amplitude);
    drawText(36, 24, buf);
    
    // Graph title and range
    drawText(0, 38, "Waveform:");
    snprintf(buf, sizeof(buf), "%.1fs", MIC_GRAPH_DURATION_SEC);
    drawText(60, 38, buf);
    
    // dB range labels
    drawText(0, 52, "-60dB");
    drawText(100, 52, "0dB");
    
    // Draw the waveform graph
    drawMicGraph();
    
    // Footer info
    drawText(0, 118, "Graph scrolls left");
  }else{
    drawText(10, 60, "NO MIC DATA");
  }
  
  updateDisplay();
}

/**
 * @brief Display Page 4: System Info
 */
void displaySystemPage(const SensorDataPayload& data){
  clearDisplay();
  
  drawText(0, 0, "==== SYSTEM INFO ====");
  
  char buf[32];
  
  drawText(0, 15, "Data Rate:");
  snprintf(buf, sizeof(buf), " %lu FPS", stats.fps);
  drawText(0, 27, buf);
  
  drawText(0, 43, "Buttons:");
  snprintf(buf, sizeof(buf), " A:%u B:%u C:%u D:%u",
    data.getButtonA(), data.getButtonB(), data.getButtonC(), data.getButtonD());
  drawText(0, 55, buf);
  
  drawText(0, 71, "Sensors:");
  snprintf(buf, sizeof(buf), " IMU:%u ENV:%u",
    data.getImuValid(), data.getEnvValid());
  drawText(0, 83, buf);
  snprintf(buf, sizeof(buf), " GPS:%u MIC:%u",
    data.getGpsValidFlag(), data.getMicValid());
  drawText(0, 95, buf);
  
  drawText(0, 111, "Page: ");
  snprintf(buf, sizeof(buf), "%d/%d", current_page + 1, TOTAL_PAGES);
  drawText(48, 111, buf);
  
  updateDisplay();
}

/**
 * @brief Handle page navigation based on button states
 */
void handlePageNavigation(const SensorDataPayload& data){
  bool button_a = data.getButtonA();
  bool button_b = data.getButtonB();
  
  // Button A: Previous page (rising edge detection)
  if(button_a && !button_a_prev){
    current_page--;
    if(current_page < 0){
      current_page = TOTAL_PAGES - 1;
    }
    ESP_LOGI(TAG, "Page changed to %d", current_page);
  }
  
  // Button B: Next page (rising edge detection)
  if(button_b && !button_b_prev){
    current_page++;
    if(current_page >= TOTAL_PAGES){
      current_page = 0;
    }
    ESP_LOGI(TAG, "Page changed to %d", current_page);
  }
  
  button_a_prev = button_a;
  button_b_prev = button_b;
}

/**
 * @brief Display current page based on page number
 */
void displayCurrentPage(const SensorDataPayload& data){
  switch(current_page){
    case 0:
      displayImuPage(data);
      break;
    case 1:
      displayEnvironmentalPage(data);
      break;
    case 2:
      displayGpsPage(data);
      break;
    case 3:
      displayMicrophonePage(data);
      break;
    case 4:
      displaySystemPage(data);
      break;
    default:
      current_page = 0;
      displayImuPage(data);
      break;
  }
}

/**
 * @brief Core 0 Task: Receive UART data and update shared buffer
 */
void uartReceiveTask(void* parameter){
  ESP_LOGI(TAG, "UART receive task started on Core 0");
  
  UartPacket packet;
  
  while(true){
    // Check for received packets (non-blocking)
    if(uart_comm.receivePacket(packet)){
      if(packet.message_type == MessageType::SENSOR_DATA){
        // Parse sensor data payload
        if(packet.payload_length == sizeof(SensorDataPayload)){
          if(xSemaphoreTake(data_mutex, pdMS_TO_TICKS(5)) == pdTRUE){
            memcpy(&current_sensor_data, packet.payload, sizeof(SensorDataPayload));
            data_received = true;
            last_data_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
            stats.frames_received++;
            xSemaphoreGive(data_mutex);
          }
        }
      }
    }
    
    // Small delay to prevent task starvation
    vTaskDelay(1);
  }
}

/**
 * @brief Core 1 Task: Update display based on received data
 */
void displayUpdateTask(void* parameter){
  ESP_LOGI(TAG, "Display update task started on Core 1");
  
  // Initialize microphone graph
  mic_sample_interval_ms = (uint32_t)((MIC_GRAPH_DURATION_SEC * 1000.0f) / (float)MIC_GRAPH_SAMPLES);
  last_mic_sample_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
  for(int i = 0; i < MIC_GRAPH_SAMPLES; i++){
    mic_history[i] = MIC_DB_MIN;
  }
  ESP_LOGI(TAG, "Microphone graph: %.1fs window, %d samples, %ums interval", 
           MIC_GRAPH_DURATION_SEC, MIC_GRAPH_SAMPLES, mic_sample_interval_ms);
  
  SensorDataPayload local_copy;
  bool have_data = false;
  
  while(true){
    uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    // Copy shared data to local buffer
    if(xSemaphoreTake(data_mutex, pdMS_TO_TICKS(5)) == pdTRUE){
      if(data_received){
        memcpy(&local_copy, &current_sensor_data, sizeof(SensorDataPayload));
        have_data = true;
      }
      xSemaphoreGive(data_mutex);
    }
    
    // Update microphone history buffer at calculated interval
    if(have_data && local_copy.getMicValid()){
      if(current_time - last_mic_sample_time >= mic_sample_interval_ms){
        addMicSample(local_copy.mic_db_level);
        last_mic_sample_time = current_time;
      }
    }
    
    // Update display if we have data
    if(have_data){
      // Handle button navigation
      handlePageNavigation(local_copy);
      
      // Display current page
      displayCurrentPage(local_copy);
      stats.display_updates++;
    }else{
      // No data received yet - show waiting message
      clearDisplay();
      drawText(10, 50, "Waiting for");
      drawText(10, 62, "sensor data...");
      updateDisplay();
    }
    
    // Print statistics every second
    if(current_time - stats.last_report_time >= 1000){
      stats.fps = stats.frames_received;
      
      ESP_LOGI(TAG, "Stats: %lu fps | Display updates: %lu | Page: %d",
        stats.fps, stats.display_updates, current_page);
      
      stats.frames_received = 0;
      stats.display_updates = 0;
      stats.last_report_time = current_time;
    }
    
    // Update at ~20Hz to avoid flickering
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

/**
 * @brief Application entry point
 */
extern "C" void app_main(void){
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "================================================");
  ESP_LOGI(TAG, "    GPU Sensor Display System with OLED        ");
  ESP_LOGI(TAG, "================================================");
  ESP_LOGI(TAG, "");
  
  // Initialize OLED display
  if(!initializeDisplay()){
    ESP_LOGE(TAG, "FATAL: Display initialization failed!");
    ESP_LOGE(TAG, "System halted.");
    return;
  }
  
  // Show startup message
  clearDisplay();
  drawText(10, 40, "GPU System");
  drawText(10, 52, "Initializing...");
  updateDisplay();
  vTaskDelay(pdMS_TO_TICKS(1000));
  
  // Initialize UART communication
  ESP_LOGI(TAG, "Initializing UART communication...");
  if(!uart_comm.init()){
    ESP_LOGE(TAG, "FATAL: UART initialization failed!");
    ESP_LOGE(TAG, "System halted.");
    
    clearDisplay();
    drawText(10, 50, "UART INIT");
    drawText(10, 62, "FAILED!");
    updateDisplay();
    return;
  }
  ESP_LOGI(TAG, "UART initialized (2 Mbps, RX=GPIO13, TX=GPIO12)");
  
  // Create mutex for shared data protection
  data_mutex = xSemaphoreCreateMutex();
  if(data_mutex == NULL){
    ESP_LOGE(TAG, "FATAL: Failed to create mutex!");
    return;
  }
  
  // Initialize shared sensor data
  memset(&current_sensor_data, 0, sizeof(SensorDataPayload));
  
  ESP_LOGI(TAG, "Creating dual-core tasks...");
  
  // Create UART receive task on Core 0
  xTaskCreatePinnedToCore(
    uartReceiveTask,
    "uart_receive",
    8192,
    NULL,
    3,                 // High priority for data reception
    NULL,
    0                  // Core 0
  );
  
  // Create display update task on Core 1
  xTaskCreatePinnedToCore(
    displayUpdateTask,
    "display_update",
    8192,
    NULL,
    2,                 // Normal priority
    NULL,
    1                  // Core 1
  );
  
  ESP_LOGI(TAG, "Dual-core system active!");
  ESP_LOGI(TAG, "Core 0 - UART reception @ 60Hz");
  ESP_LOGI(TAG, "Core 1 - Display updates");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "Controls:");
  ESP_LOGI(TAG, "  Button A - Previous page");
  ESP_LOGI(TAG, "  Button B - Next page");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "Pages:");
  ESP_LOGI(TAG, "  0 - IMU (Accel/Gyro/Mag)");
  ESP_LOGI(TAG, "  1 - Environment (Temp/Humidity/Pressure)");
  ESP_LOGI(TAG, "  2 - GPS (Position/Navigation)");
  ESP_LOGI(TAG, "  3 - Microphone (Audio levels)");
  ESP_LOGI(TAG, "  4 - System Info (FPS/Buttons/Status)");
  ESP_LOGI(TAG, "================================================");
  ESP_LOGI(TAG, "");
}
