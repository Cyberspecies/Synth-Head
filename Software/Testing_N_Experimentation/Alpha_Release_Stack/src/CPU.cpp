/*****************************************************************
 * File:      uart_cpu_sensor_display.cpp
 * Category:  communication/applications
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    CPU-side bidirectional application:
 *    - Sends sensor data (IMU, BME280, GPS, Mic) + buttons to GPU at 60Hz
 *    - Receives LED RGBW data from GPU and drives 4 LED strips
 * 
 * Hardware:
 *    - ESP32-S3 (CPU)
 *    - ICM20948 IMU (I2C: SDA=GPIO9, SCL=GPIO10)
 *    - BME280 Environmental Sensor (I2C: SDA=GPIO9, SCL=GPIO10)
 *    - NEO-8M GPS (UART: TX=GPIO43, RX=GPIO44)
 *    - INMP441 Microphone (I2S)
 *    - 4 Buttons: A=GPIO5, B=GPIO6, C=GPIO7, D=GPIO15
 *    - LED Strips: Strip1=GPIO18, Strip2=GPIO8, Strip4=GPIO38, Strip5=GPIO37
 * 
 * Communication:
 *    - UART to GPU: RX=GPIO11, TX=GPIO12
 *    - Baud Rate: 2 Mbps
 *    - TX: Sensor data at 60Hz
 *    - RX: LED data from GPU
 * 
 * Debug Messages (Inter-Core Data Flow):
 *    [CORE0-SENSOR] - Sensor read task writes to double buffer
 *    [CORE1-UART-TX] - UART send task reads from double buffer (60Hz)
 *    [CORE1-WEB] - Web server task reads from double buffer
 *    [PORTAL-UPDATE] - Captive portal receives data via mutex
 *    [PORTAL-JSON] - Captive portal reads internal storage to generate JSON
 *    [WEB-API] - Web interface polls /api/sensors endpoint
 *****************************************************************/

#include <Arduino.h>
#include <atomic>
#include <Adafruit_NeoPixel.h>
#include "Drivers/UART Comms/CpuUartBidirectional.hpp"
#include "Drivers/Sensors/SensorManager.h"
#include "Manager/CaptivePortalManager.hpp"

using namespace arcos::communication;
using namespace sensors;
using namespace arcos::manager;

// ============== Pin Definitions ==============
constexpr uint8_t BUTTON_A_PIN = 5;
constexpr uint8_t BUTTON_B_PIN = 6;
constexpr uint8_t BUTTON_C_PIN = 7;
constexpr uint8_t BUTTON_D_PIN = 15;

// LED Strip GPIO Pins
constexpr int LED_PIN_STRIP1 = 18;  // Left Fin
constexpr int LED_PIN_STRIP2 = 8;   // Tongue
constexpr int LED_PIN_STRIP4 = 38;  // Right Fin
constexpr int LED_PIN_STRIP5 = 37;  // Scale

// Fan PWM Configuration
constexpr int FAN_PIN = 17;         // Fan 1 on GPIO 17
constexpr int FAN_PWM_CHANNEL = 0;  // LEDC channel 0
constexpr int FAN_PWM_FREQ = 40000; // 40kHz PWM frequency (above human hearing, eliminates whine)
constexpr int FAN_PWM_RESOLUTION = 8; // 8-bit resolution (0-255)

// ============== Timing Configuration ==============
constexpr uint32_t CPU_TARGET_FPS = 60;
constexpr uint32_t FRAME_TIME_US = 1000000 / CPU_TARGET_FPS;  // 16666 microseconds
constexpr int LED_BRIGHTNESS = 255;

// ============== Global Instances ==============
SensorManager sensor_manager;
CpuUartBidirectional uart_comm;
CaptivePortalManager captive_portal;

// Adafruit NeoPixel strips (RGBW order for SK6812)
Adafruit_NeoPixel strip1(LED_COUNT_LEFT_FIN, LED_PIN_STRIP1, NEO_RGBW + NEO_KHZ800);
Adafruit_NeoPixel strip2(LED_COUNT_TONGUE, LED_PIN_STRIP2, NEO_RGBW + NEO_KHZ800);
Adafruit_NeoPixel strip4(LED_COUNT_RIGHT_FIN, LED_PIN_STRIP4, NEO_RGBW + NEO_KHZ800);
Adafruit_NeoPixel strip5(LED_COUNT_SCALE, LED_PIN_STRIP5, NEO_RGBW + NEO_KHZ800);

// ============== Dual-Core Task Handles ==============
TaskHandle_t sensor_task_handle = NULL;
TaskHandle_t uart_send_task_handle = NULL;
TaskHandle_t uart_receive_task_handle = NULL;
TaskHandle_t led_display_task_handle = NULL;
TaskHandle_t web_server_task_handle = NULL;

// ============== Double-Buffered Shared Data (Lock-Free) ==============
// Two buffers: one for writing (Core 0), one for reading (Core 1)
// Atomic index swap ensures cores never access same buffer simultaneously
SensorDataPayload sensor_data_buffers[2];
std::atomic<uint8_t> active_buffer_index{0};  // Which buffer is ready for reading

SemaphoreHandle_t led_data_mutex;
LedDataPayload shared_led_data;
bool led_data_received = false;
uint32_t last_led_data_time = 0;

// ============== Statistics ==============
struct Statistics{
  uint32_t sensor_frames_sent = 0;
  uint32_t led_frames_received = 0;
  uint32_t sensor_reads = 0;
  uint32_t leds_updated = 0;
  uint32_t last_report_time = 0;
  uint32_t sensor_fps = 0;
  uint32_t led_fps = 0;
  // Last received LED color for display
  uint8_t last_led_r = 0;
  uint8_t last_led_g = 0;
  uint8_t last_led_b = 0;
  uint8_t last_led_w = 0;
  uint8_t fan_speed = 0;  // Current fan speed (0-255)
};
Statistics stats;

/**
 * @brief Test all LED strips at startup
 */
void testLedStrips(){
  Serial.println("CPU: Testing LED strips...");
  
  // Test Strip 1 (Left Fin) - Red
  Serial.println("CPU: Testing Strip 1 (Left Fin) - RED");
  for(int i = 0; i < LED_COUNT_LEFT_FIN; i++){
    strip1.setPixelColor(i, strip1.Color(255, 0, 0));
  }
  strip1.show();
  delay(1000);
  strip1.clear();
  strip1.show();
  
  // Test Strip 2 (Tongue) - Green
  Serial.println("CPU: Testing Strip 2 (Tongue) - GREEN");
  for(int i = 0; i < LED_COUNT_TONGUE; i++){
    strip2.setPixelColor(i, strip2.Color(0, 255, 0));
  }
  strip2.show();
  delay(1000);
  strip2.clear();
  strip2.show();
  
  // Test Strip 4 (Right Fin) - Blue
  Serial.println("CPU: Testing Strip 4 (Right Fin) - BLUE");
  for(int i = 0; i < LED_COUNT_RIGHT_FIN; i++){
    strip4.setPixelColor(i, strip4.Color(0, 0, 255));
  }
  strip4.show();
  delay(1000);
  strip4.clear();
  strip4.show();
  
  // Test Strip 5 (Scale) - White
  Serial.println("CPU: Testing Strip 5 (Scale) - WHITE");
  for(int i = 0; i < LED_COUNT_SCALE; i++){
    strip5.setPixelColor(i, strip5.Color(255, 255, 255));
  }
  strip5.show();
  delay(1000);
  strip5.clear();
  strip5.show();
  
  // All strips together - Rainbow
  Serial.println("CPU: All strips - RAINBOW");
  for(int i = 0; i < LED_COUNT_LEFT_FIN; i++) strip1.setPixelColor(i, strip1.Color(255, 0, 0));
  for(int i = 0; i < LED_COUNT_TONGUE; i++) strip2.setPixelColor(i, strip2.Color(0, 255, 0));
  for(int i = 0; i < LED_COUNT_RIGHT_FIN; i++) strip4.setPixelColor(i, strip4.Color(0, 0, 255));
  for(int i = 0; i < LED_COUNT_SCALE; i++) strip5.setPixelColor(i, strip5.Color(255, 255, 0));
  strip1.show();
  strip2.show();
  strip4.show();
  strip5.show();
  delay(1000);
  
  // Clear all
  strip1.clear();
  strip2.clear();
  strip4.clear();
  strip5.clear();
  strip1.show();
  strip2.show();
  strip4.show();
  strip5.show();
  
  Serial.println("CPU: LED strip test complete!");
}

/**
 * @brief Initialize button GPIOs with internal pull-ups
 */
void initializeButtons(){
  pinMode(BUTTON_A_PIN, INPUT_PULLUP);
  pinMode(BUTTON_B_PIN, INPUT_PULLUP);
  pinMode(BUTTON_C_PIN, INPUT_PULLUP);
  pinMode(BUTTON_D_PIN, INPUT_PULLUP);
  
  Serial.println("CPU: Buttons initialized (A=GPIO5, B=GPIO6, C=GPIO7, D=GPIO15)");
}

/**
 * @brief Read button states (active LOW with pull-ups)
 * @return Button states (0=released, 1=pressed)
 */
void readButtons(uint8_t& btn_a, uint8_t& btn_b, uint8_t& btn_c, uint8_t& btn_d){
  // Buttons are active LOW (pressed = LOW)
  btn_a = !digitalRead(BUTTON_A_PIN);
  btn_b = !digitalRead(BUTTON_B_PIN);
  btn_c = !digitalRead(BUTTON_C_PIN);
  btn_d = !digitalRead(BUTTON_D_PIN);
}

/**
 * @brief Core 0 Task: Read sensors and update inactive buffer
 * Lock-free double buffering: write to inactive buffer, then atomically swap
 */
void sensorReadTask(void* parameter){
  Serial.println("CPU: Sensor read task started on Core 0");
  Serial.println("DEBUG [CORE0-SENSOR]: Will write sensor data to double buffers");
  
  static uint32_t debug_counter = 0;
  
  while(true){
    // Update sensor manager (reads all sensors)
    sensor_manager.update();
    
    // Debug: Print sensor validity every 1000 reads
    if(++debug_counter % 1000 == 0){
      Serial.printf("SENSOR: IMU Valid: %d | Env Valid: %d | GPS Valid: %d | Mic Valid: %d\n",
        sensor_manager.isImuValid(), sensor_manager.isEnvironmentalValid(),
        sensor_manager.isGpsValid(), sensor_manager.isMicrophoneValid());
    }
    
    // Get the INACTIVE buffer (the one NOT being read by other core)
    uint8_t current_active = active_buffer_index.load(std::memory_order_acquire);
    uint8_t write_index = 1 - current_active;  // Write to the OTHER buffer
    SensorDataPayload& write_buffer = sensor_data_buffers[write_index];
    
    // Debug: Track buffer switching
    if(debug_counter % 1000 == 0){
      Serial.printf("DEBUG [CORE0-SENSOR]: Writing to buffer[%u], active is buffer[%u]\n",
        write_index, current_active);
    }
    
    // Write to inactive buffer WITHOUT any locks - other core reads from active buffer
    
    // Read IMU data
    if(sensor_manager.isImuValid()){
      const Icm20948Data& imu = sensor_manager.getImuData();
      
      // Debug: Print actual IMU values every 1000 reads
      if(debug_counter % 1000 == 0){
        Serial.printf("SENSOR: Raw IMU - Accel: %.2f, %.2f, %.2f | Gyro: %.1f, %.1f, %.1f\n",
          imu.accel_x, imu.accel_y, imu.accel_z,
          imu.gyro_x, imu.gyro_y, imu.gyro_z);
      }
      
      write_buffer.accel_x = imu.accel_x;
      write_buffer.accel_y = imu.accel_y;
      write_buffer.accel_z = imu.accel_z;
      write_buffer.gyro_x = imu.gyro_x;
      write_buffer.gyro_y = imu.gyro_y;
      write_buffer.gyro_z = imu.gyro_z;
      write_buffer.mag_x = imu.mag_x;
      write_buffer.mag_y = imu.mag_y;
      write_buffer.mag_z = imu.mag_z;
      write_buffer.setImuValid(true);
    }else{
      write_buffer.setImuValid(false);
    }
    
    // Read environmental data
    if(sensor_manager.isEnvironmentalValid()){
      const Bme280Data& env = sensor_manager.getEnvironmentalData();
      
      // Debug: Print environmental values every 1000 reads
      if(debug_counter % 1000 == 0){
        Serial.printf("SENSOR: Raw ENV - Temp: %.1f | Humidity: %.1f | Pressure: %.0f\n",
          env.temperature, env.humidity, env.pressure);
      }
      
      write_buffer.temperature = env.temperature;
      write_buffer.humidity = env.humidity;
      write_buffer.pressure = env.pressure;
      write_buffer.setEnvValid(true);
    }else{
      write_buffer.setEnvValid(false);
    }
    
    // Read GPS data
    if(sensor_manager.isGpsValid()){
      const Neo8mGpsData& gps = sensor_manager.getGpsData();
      write_buffer.latitude = gps.latitude;
      write_buffer.longitude = gps.longitude;
      write_buffer.altitude = gps.altitude;
      write_buffer.speed_knots = gps.speed_knots;
      write_buffer.course = gps.course;
      write_buffer.setGpsFixQuality(static_cast<uint8_t>(gps.fix_quality));
      write_buffer.gps_satellites = gps.satellites;
      write_buffer.gps_hour = gps.hour;
      write_buffer.gps_minute = gps.minute;
      write_buffer.gps_second = gps.second;
      write_buffer.setGpsValid(gps.valid);
      write_buffer.setGpsValidFlag(true);
    }else{
      write_buffer.setGpsValidFlag(false);
    }
    
    // Read microphone data
    if(sensor_manager.isMicrophoneValid()){
      const Inmp441AudioData& mic = sensor_manager.getMicrophoneData();
      write_buffer.mic_current_sample = mic.current_sample;
      write_buffer.mic_peak_amplitude = mic.peak_amplitude;
      write_buffer.mic_db_level = mic.db_level;
      write_buffer.setMicClipping(mic.clipping);
      write_buffer.setMicValid(true);
    }else{
      write_buffer.setMicValid(false);
    }
    
    // Read button states (physical buttons)
    uint8_t btn_a, btn_b, btn_c, btn_d;
    readButtons(btn_a, btn_b, btn_c, btn_d);
    
    // Get web button states from captive portal
    SensorDataPayload web_buttons;
    captive_portal.getSensorData(web_buttons);
    
    // Merge physical and web button states (OR logic - either source can activate)
    write_buffer.setButtonA(btn_a || web_buttons.getButtonA());
    write_buffer.setButtonB(btn_b || web_buttons.getButtonB());
    write_buffer.setButtonC(btn_c || web_buttons.getButtonC());
    write_buffer.setButtonD(btn_d || web_buttons.getButtonD());
    
    // Update WiFi credentials from captive portal
    String ssid = captive_portal.getSSID();
    String password = captive_portal.getPassword();
    strncpy(write_buffer.wifi_ssid, ssid.c_str(), 32);
    write_buffer.wifi_ssid[32] = '\0';
    strncpy(write_buffer.wifi_password, password.c_str(), 31);
    write_buffer.wifi_password[31] = '\0';
    
    stats.sensor_reads++;
    
    // ATOMIC SWAP: Make this buffer active (other core will now read from it)
    active_buffer_index.store(write_index, std::memory_order_release);
    
    // Debug: Confirm buffer swap and show sample data
    if(debug_counter % 1000 == 0){
      Serial.printf("DEBUG [CORE0-SENSOR]: Swapped to buffer[%u] - Sample data: Temp=%.1f°C, Accel=(%.2f,%.2f,%.2f)\n",
        write_index, write_buffer.temperature, 
        write_buffer.accel_x, write_buffer.accel_y, write_buffer.accel_z);
    }
    
    // Small delay to prevent task starvation
    vTaskDelay(1);
  }
}

/**
 * @brief Core 1 Task: Package and send sensor data via UART at 60Hz
 * Maintains precise 60Hz timing using high-resolution timer
 */
void uartSendTask(void* parameter){
  Serial.println("CPU: UART send task started on Core 1");
  Serial.println("DEBUG [CORE1-UART-TX]: Will read sensor data from double buffers at 60Hz");
  
  uint32_t last_frame_time = micros();
  SensorDataPayload local_copy;
  static uint32_t send_count = 0;
  
  while(true){
    uint32_t current_time = micros();
    uint32_t elapsed = current_time - last_frame_time;
    
    // Wait until frame time elapsed (60Hz = 16666 microseconds)
    if(elapsed >= FRAME_TIME_US){
      last_frame_time = current_time;
      
      // LOCK-FREE READ: Copy from active buffer (no mutex needed)
      uint8_t read_index = active_buffer_index.load(std::memory_order_acquire);
      memcpy(&local_copy, &sensor_data_buffers[read_index], sizeof(SensorDataPayload));
      
      // Debug: Verify data being sent
      if(++send_count % 60 == 0){  // Every 1 second at 60Hz
        Serial.printf("DEBUG [CORE1-UART-TX]: Read from buffer[%u] - Sending Temp=%.1f°C, Accel=(%.2f,%.2f,%.2f)\n",
          read_index, local_copy.temperature,
          local_copy.accel_x, local_copy.accel_y, local_copy.accel_z);
      }
      
      // Send sensor data packet via UART
      if(uart_comm.sendPacket(
        MessageType::SENSOR_DATA,
        reinterpret_cast<const uint8_t*>(&local_copy),
        sizeof(SensorDataPayload)
      )){
        stats.sensor_frames_sent++;
      }
      
      // Print statistics every second
      if(current_time - stats.last_report_time >= 1000000){
        stats.sensor_fps = stats.sensor_frames_sent;
        stats.led_fps = stats.led_frames_received;
        
        Serial.printf("CPU Stats: Sensor TX: %u fps | LED RX: %u fps | LEDs: %u upd/s | Sensors: %u/s | LED[0]: R=%d G=%d B=%d W=%d | Fan: %d%%\n",
          stats.sensor_fps,
          stats.led_fps,
          stats.leds_updated,
          stats.sensor_reads,
          stats.last_led_r,
          stats.last_led_g,
          stats.last_led_b,
          stats.last_led_w,
          (stats.fan_speed * 100) / 255
        );
        
        stats.sensor_frames_sent = 0;
        stats.led_frames_received = 0;
        stats.leds_updated = 0;
        stats.sensor_reads = 0;
        stats.last_report_time = current_time;
      }
    }else{
      // Not time to send yet - small delay to prevent busy-waiting
      delayMicroseconds(100);
    }
  }
}

/**
 * @brief UART Receive Task - receives LED data from GPU
 */
void uartReceiveTask(void* parameter){
  Serial.println("CPU: UART receive task started on Core 0");
  
  UartPacket packet;
  uint32_t last_debug_time = 0;
  
  while(true){
    if(uart_comm.receivePacket(packet)){
      if(packet.message_type == MessageType::LED_DATA){
        if(packet.payload_length == sizeof(LedDataPayload)){
          // Copy LED data to shared buffer
          if(xSemaphoreTake(led_data_mutex, pdMS_TO_TICKS(5)) == pdTRUE){
            memcpy(&shared_led_data, packet.payload, sizeof(LedDataPayload));
            led_data_received = true;
            last_led_data_time = millis();
            stats.led_frames_received++;
            
            // Store first LED color and fan speed for stats display
            const RgbwColor& first_led = shared_led_data.leds[0];
            stats.last_led_r = first_led.r;
            stats.last_led_g = first_led.g;
            stats.last_led_b = first_led.b;
            stats.last_led_w = first_led.w;
            stats.fan_speed = shared_led_data.fan_speed;
            
            // Update fan speed immediately
            ledcWrite(FAN_PWM_CHANNEL, shared_led_data.fan_speed);
            
            xSemaphoreGive(led_data_mutex);
          }
        }else{
          Serial.printf("CPU: ERROR - Invalid LED payload size: %d (expected %d)\n",
                       packet.payload_length, sizeof(LedDataPayload));
        }
      }
    }
    
    // Small delay
    vTaskDelay(1);
  }
}

/**
 * @brief LED Display Task - updates physical LED strips from received data
 */
void ledDisplayTask(void* parameter){
  Serial.println("CPU: LED display task started on Core 0");
  
  LedDataPayload local_led_data;
  bool have_led_data = false;
  uint32_t last_status_print = 0;
  uint32_t loop_count = 0;
  
  while(true){
    loop_count++;
    
    // Copy shared LED data to local buffer
    if(xSemaphoreTake(led_data_mutex, pdMS_TO_TICKS(5)) == pdTRUE){
      bool data_available = led_data_received;
      if(data_available){
        memcpy(&local_led_data, &shared_led_data, sizeof(LedDataPayload));
        have_led_data = true;
      }
      xSemaphoreGive(led_data_mutex);
    }
    
    // Always update LEDs if we have received data at least once
    if(have_led_data){
      // Strip 1 (Left Fin) - using RGBW
      for(int i = 0; i < LED_COUNT_LEFT_FIN; i++){
        const RgbwColor& color = local_led_data.leds[LED_OFFSET_LEFT_FIN + i];
        strip1.setPixelColor(i, color.r, color.g, color.b, color.w);
      }
      
      // Strip 2 (Tongue) - using RGBW
      for(int i = 0; i < LED_COUNT_TONGUE; i++){
        const RgbwColor& color = local_led_data.leds[LED_OFFSET_TONGUE + i];
        strip2.setPixelColor(i, color.r, color.g, color.b, color.w);
      }
      
      // Strip 4 (Right Fin) - using RGBW
      for(int i = 0; i < LED_COUNT_RIGHT_FIN; i++){
        const RgbwColor& color = local_led_data.leds[LED_OFFSET_RIGHT_FIN + i];
        strip4.setPixelColor(i, color.r, color.g, color.b, color.w);
      }
      
      // Strip 5 (Scale) - using RGBW
      for(int i = 0; i < LED_COUNT_SCALE; i++){
        const RgbwColor& color = local_led_data.leds[LED_OFFSET_SCALE + i];
        strip5.setPixelColor(i, color.r, color.g, color.b, color.w);
      }
      
      // Update all strips
      strip1.show();
      strip2.show();
      strip4.show();
      strip5.show();
      stats.leds_updated++;
    }else{
      // No data received yet - show dim red waiting pattern
      for(int i = 0; i < LED_COUNT_LEFT_FIN; i++) strip1.setPixelColor(i, 5, 0, 0, 0);
      for(int i = 0; i < LED_COUNT_TONGUE; i++) strip2.setPixelColor(i, 5, 0, 0, 0);
      for(int i = 0; i < LED_COUNT_RIGHT_FIN; i++) strip4.setPixelColor(i, 5, 0, 0, 0);
      for(int i = 0; i < LED_COUNT_SCALE; i++) strip5.setPixelColor(i, 5, 0, 0, 0);
      strip1.show();
      strip2.show();
      strip4.show();
      strip5.show();
    }
    
    vTaskDelay(pdMS_TO_TICKS(20));  // Update at ~50Hz
  }
}

/**
 * @brief Core 1 Task: Web server and captive portal processing
 * Handles DNS requests and updates web interface with sensor data
 */
void webServerTask(void* parameter){
  // CRITICAL: First thing - print to serial to confirm task is running
  delay(10);  // Brief delay to let serial stabilize
  Serial.println("========================================");
  Serial.println("DEBUG [CORE1-WEB]: TASK STARTING!");
  Serial.println("========================================");
  Serial.flush();
  delay(100);
  
  // Give other tasks time to initialize
  Serial.println("DEBUG [CORE1-WEB]: Delaying 1 second...");
  Serial.flush();
  vTaskDelay(pdMS_TO_TICKS(1000));
  
  Serial.println("DEBUG [CORE1-WEB]: After 1 second delay, entering main loop NOW");
  Serial.flush();
  delay(100);
  
  uint32_t web_debug_count = 0;
  uint32_t last_alive_print = millis();
  uint32_t last_update_print = millis();
  
  Serial.println("DEBUG [CORE1-WEB]: Variables initialized, entering while loop");
  Serial.flush();
  
  while(true){
    web_debug_count++;
    
    // HEARTBEAT: Print immediately every loop for first 10 iterations
    if(web_debug_count <= 10){
      Serial.printf("DEBUG [CORE1-WEB]: HEARTBEAT #%u\n", web_debug_count);
      Serial.flush();
    }
    
    // Print alive message every 2 seconds for debugging
    uint32_t current_time = millis();
    if(current_time - last_alive_print >= 2000){
      Serial.printf("DEBUG [CORE1-WEB]: *** ALIVE *** Loop count=%u, Active buffer=%u\n", 
        web_debug_count, active_buffer_index.load(std::memory_order_acquire));
      Serial.flush();
      last_alive_print = current_time;
    }
    
    // LOCK-FREE READ: Get active buffer index and copy data
    // No mutex needed - sensor task writes to OTHER buffer
    uint8_t read_index = active_buffer_index.load(std::memory_order_acquire);
    SensorDataPayload sensor_copy;
    memcpy(&sensor_copy, &sensor_data_buffers[read_index], sizeof(sensor_copy));
    
    // Debug: Print update every 2 seconds
    if(current_time - last_update_print >= 2000){
      Serial.printf("DEBUG [CORE1-WEB]: Read from buffer[%u] - Temp=%.1f°C, Accel=(%.2f,%.2f,%.2f)\n", 
        read_index, sensor_copy.temperature,
        sensor_copy.accel_x, sensor_copy.accel_y, sensor_copy.accel_z);
      Serial.printf("DEBUG [CORE1-WEB]: About to call captive_portal.updateSensorData()...\n");
      Serial.flush();
      last_update_print = current_time;
    }
    
    // Update portal with fresh data (portal has its own internal mutex)
    captive_portal.updateSensorData(sensor_copy);
    
    // Debug confirmation after update
    if((current_time - last_alive_print) >= 2000 && (current_time - last_alive_print) < 2100){
      Serial.println("DEBUG [CORE1-WEB]: captive_portal.updateSensorData() completed");
      Serial.flush();
    }
    
    // Process DNS requests for captive portal (can block briefly)
    captive_portal.update();
    
    // Small delay to keep loop responsive for 250ms web polling
    vTaskDelay(pdMS_TO_TICKS(5));  // 5ms delay = up to 200 updates/sec
  }
}

/**
 * @brief Arduino setup function
 */
void setup(){
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n");
  Serial.println("========================================================");
  Serial.println("  CPU Bidirectional: Sensors TX + LED RX System");
  Serial.println("========================================================");
  Serial.println();
  
  // Initialize buttons
  initializeButtons();
  
  // Initialize NeoPixel strips
  Serial.println("CPU: Initializing LED strips...");
  strip1.begin();
  strip2.begin();
  strip4.begin();
  strip5.begin();
  strip1.setBrightness(LED_BRIGHTNESS);
  strip2.setBrightness(LED_BRIGHTNESS);
  strip4.setBrightness(LED_BRIGHTNESS);
  strip5.setBrightness(LED_BRIGHTNESS);
  strip1.clear();
  strip2.clear();
  strip4.clear();
  strip5.clear();
  strip1.show();
  strip2.show();
  strip4.show();
  strip5.show();
  Serial.printf("CPU: LED strips initialized (Total: %d LEDs)\n", LED_COUNT_TOTAL);
  
  // Test LED strips
  testLedStrips();
  
  // Initialize fan PWM
  Serial.println("CPU: Initializing fan control...");
  ledcSetup(FAN_PWM_CHANNEL, FAN_PWM_FREQ, FAN_PWM_RESOLUTION);
  ledcAttachPin(FAN_PIN, FAN_PWM_CHANNEL);
  ledcWrite(FAN_PWM_CHANNEL, 0);  // Start with fan off
  Serial.printf("CPU: Fan initialized on GPIO %d (PWM channel %d, %d Hz)\n", 
                FAN_PIN, FAN_PWM_CHANNEL, FAN_PWM_FREQ);
  
  // Initialize sensor manager
  Serial.println("CPU: Initializing sensors...");
  if(!sensor_manager.init()){
    Serial.println("CPU: [ERROR] Sensor manager initialization failed!");
    Serial.println("CPU: System halted. Check sensor wiring.");
    while(1){
      delay(1000);
    }
  }
  Serial.println("CPU: Sensors initialized successfully");
  
  // Initialize UART communication
  Serial.println("CPU: Initializing UART communication...");
  if(!uart_comm.init()){
    Serial.println("CPU: [ERROR] UART initialization failed!");
    Serial.println("CPU: System halted. Check UART wiring.");
    while(1){
      delay(1000);
    }
  }
  Serial.println("CPU: UART initialized (2 Mbps, RX=GPIO11, TX=GPIO12)");
  
  // Create mutex for LED data (still needed for LED display)
  led_data_mutex = xSemaphoreCreateMutex();
  if(led_data_mutex == NULL){
    Serial.println("CPU: [ERROR] Failed to create LED mutex!");
    while(1){
      delay(1000);
    }
  }
  
  // Initialize double buffers (lock-free for sensor data)
  memset(&sensor_data_buffers[0], 0, sizeof(SensorDataPayload));
  memset(&sensor_data_buffers[1], 0, sizeof(SensorDataPayload));
  active_buffer_index.store(0, std::memory_order_release);
  
  // Initialize LED data
  memset(&shared_led_data, 0, sizeof(LedDataPayload));
  
  // Initialize captive portal (WiFi AP + web server)
  Serial.println();
  Serial.println("CPU: Initializing captive portal...");
  if(!captive_portal.initialize()){
    Serial.println("CPU: [WARNING] Captive portal initialization failed!");
    Serial.println("CPU: Continuing without web interface...");
  }else{
    Serial.println("CPU: Captive portal ready!");
    Serial.printf("CPU: Connect to: %s\n", captive_portal.getSSID().c_str());
    Serial.printf("CPU: Password: %s\n", captive_portal.getPassword().c_str());
  }
  
  Serial.println();
  Serial.println("CPU: Creating tasks on both cores...");
  
  // Core 0 tasks
  xTaskCreatePinnedToCore(
    sensorReadTask,
    "sensor_read",
    8192,
    NULL,
    2,
    &sensor_task_handle,
    0  // Core 0
  );
  
  xTaskCreatePinnedToCore(
    uartReceiveTask,
    "uart_receive",
    4096,
    NULL,
    2,
    &uart_receive_task_handle,
    0  // Core 0
  );
  
  // Create LED display task BEFORE high-priority tasks
  // This prevents setup() from being starved by uart_send task on Core 1
  Serial.println("CPU: About to create LED display task...");
  Serial.flush();
  
  BaseType_t result = xTaskCreatePinnedToCore(
    ledDisplayTask,
    "led_display",
    4096,  // Moderate stack for LED updates
    NULL,
    1,     // Lower priority than other tasks
    &led_display_task_handle,
    0  // Core 0 - same as receive task for efficient data sharing
  );
  
  Serial.printf("CPU: xTaskCreatePinnedToCore returned: %d (pdPASS=%d)\n", result, pdPASS);
  Serial.flush();
  
  if(result != pdPASS){
    Serial.println("CPU: ERROR - Failed to create LED display task!");
  }else{
    Serial.println("CPU: LED display task created successfully");
  }
  
  // Core 1 tasks - created LAST because uart_send has high priority
  xTaskCreatePinnedToCore(
    uartSendTask,
    "uart_send",
    8192,
    NULL,
    3,  // Higher priority for timing-critical
    &uart_send_task_handle,
    1  // Core 1
  );
  
  // Web server task on Core 1 (lower priority)
  Serial.println("CPU: About to create web server task...");
  Serial.flush();
  delay(100);
  
  BaseType_t web_result = xTaskCreatePinnedToCore(
    webServerTask,
    "web_server",
    16384,  // DOUBLED stack size - was 8192, now 16KB for safety
    NULL,
    1,     // Lower priority than UART
    &web_server_task_handle,
    1  // Core 1
  );
  
  Serial.printf("CPU: xTaskCreatePinnedToCore (web) returned: %d (pdPASS=%d)\n", web_result, pdPASS);
  Serial.flush();
  
  if(web_result != pdPASS){
    Serial.println("CPU: ERROR - Failed to create web server task!");
  }else{
    Serial.println("CPU: Web server task created successfully");
  }
  
  Serial.println("CPU: All tasks created!");
  Serial.println("CPU: Core 0 - Sensor reading + UART RX (LED data) + LED display");
  Serial.println("CPU: Core 1 - UART TX (Sensor @ 60Hz) + Web Server");
  Serial.println();
  Serial.println("========================================================");
  Serial.println();
}

/**
 * @brief Arduino loop function (runs on Core 1)
 * Main loop is not used - all work done in FreeRTOS tasks
 */
void loop(){
  // Main loop idle - all work done in tasks
  vTaskDelay(pdMS_TO_TICKS(1000));
}
