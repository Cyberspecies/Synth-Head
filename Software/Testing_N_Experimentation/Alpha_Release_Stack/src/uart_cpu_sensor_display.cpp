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
 *****************************************************************/

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "Drivers/UART Comms/CpuUartBidirectional.hpp"
#include "Drivers/Sensors/SensorManager.h"

using namespace arcos::communication;
using namespace sensors;

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

// ============== Timing Configuration ==============
constexpr uint32_t CPU_TARGET_FPS = 60;
constexpr uint32_t FRAME_TIME_US = 1000000 / CPU_TARGET_FPS;  // 16666 microseconds
constexpr int LED_BRIGHTNESS = 255;

// ============== Global Instances ==============
SensorManager sensor_manager;
CpuUartBidirectional uart_comm;

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

// ============== Shared Data (Protected by Mutexes) ==============
SemaphoreHandle_t sensor_data_mutex;
SensorDataPayload shared_sensor_data;

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
 * @brief Core 0 Task: Read sensors and update shared data structure
 * Runs at maximum speed, continuously updating sensor readings
 */
void sensorReadTask(void* parameter){
  Serial.println("CPU: Sensor read task started on Core 0");
  
  while(true){
    // Update sensor manager (reads all sensors)
    sensor_manager.update();
    
    // Acquire mutex to update shared data
    if(xSemaphoreTake(sensor_data_mutex, pdMS_TO_TICKS(5)) == pdTRUE){
      // Read IMU data
      if(sensor_manager.isImuValid()){
        const Icm20948Data& imu = sensor_manager.getImuData();
        shared_sensor_data.accel_x = imu.accel_x;
        shared_sensor_data.accel_y = imu.accel_y;
        shared_sensor_data.accel_z = imu.accel_z;
        shared_sensor_data.gyro_x = imu.gyro_x;
        shared_sensor_data.gyro_y = imu.gyro_y;
        shared_sensor_data.gyro_z = imu.gyro_z;
        shared_sensor_data.mag_x = imu.mag_x;
        shared_sensor_data.mag_y = imu.mag_y;
        shared_sensor_data.mag_z = imu.mag_z;
        shared_sensor_data.setImuValid(true);
      }else{
        shared_sensor_data.setImuValid(false);
      }
      
      // Read environmental data
      if(sensor_manager.isEnvironmentalValid()){
        const Bme280Data& env = sensor_manager.getEnvironmentalData();
        shared_sensor_data.temperature = env.temperature;
        shared_sensor_data.humidity = env.humidity;
        shared_sensor_data.pressure = env.pressure;
        shared_sensor_data.setEnvValid(true);
      }else{
        shared_sensor_data.setEnvValid(false);
      }
      
      // Read GPS data
      if(sensor_manager.isGpsValid()){
        const Neo8mGpsData& gps = sensor_manager.getGpsData();
        shared_sensor_data.latitude = gps.latitude;
        shared_sensor_data.longitude = gps.longitude;
        shared_sensor_data.altitude = gps.altitude;
        shared_sensor_data.speed_knots = gps.speed_knots;
        shared_sensor_data.course = gps.course;
        shared_sensor_data.setGpsFixQuality(static_cast<uint8_t>(gps.fix_quality));
        shared_sensor_data.gps_satellites = gps.satellites;
        shared_sensor_data.gps_hour = gps.hour;
        shared_sensor_data.gps_minute = gps.minute;
        shared_sensor_data.gps_second = gps.second;
        shared_sensor_data.setGpsValid(gps.valid);
        shared_sensor_data.setGpsValidFlag(true);
      }else{
        shared_sensor_data.setGpsValidFlag(false);
      }
      
      // Read microphone data
      if(sensor_manager.isMicrophoneValid()){
        const Inmp441AudioData& mic = sensor_manager.getMicrophoneData();
        shared_sensor_data.mic_current_sample = mic.current_sample;
        shared_sensor_data.mic_peak_amplitude = mic.peak_amplitude;
        shared_sensor_data.mic_db_level = mic.db_level;
        shared_sensor_data.setMicClipping(mic.clipping);
        shared_sensor_data.setMicValid(true);
      }else{
        shared_sensor_data.setMicValid(false);
      }
      
      // Read button states
      uint8_t btn_a, btn_b, btn_c, btn_d;
      readButtons(btn_a, btn_b, btn_c, btn_d);
      shared_sensor_data.setButtonA(btn_a);
      shared_sensor_data.setButtonB(btn_b);
      shared_sensor_data.setButtonC(btn_c);
      shared_sensor_data.setButtonD(btn_d);
      
      stats.sensor_reads++;
      
      xSemaphoreGive(sensor_data_mutex);
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
  
  uint32_t last_frame_time = micros();
  SensorDataPayload local_copy;
  
  while(true){
    uint32_t current_time = micros();
    uint32_t elapsed = current_time - last_frame_time;
    
    // Wait until frame time elapsed (60Hz = 16666 microseconds)
    if(elapsed >= FRAME_TIME_US){
      last_frame_time = current_time;
      
      // Copy shared data to local buffer
      if(xSemaphoreTake(sensor_data_mutex, pdMS_TO_TICKS(2)) == pdTRUE){
        memcpy(&local_copy, &shared_sensor_data, sizeof(SensorDataPayload));
        xSemaphoreGive(sensor_data_mutex);
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
        
        Serial.printf("CPU Stats: Sensor TX: %u fps | LED RX: %u fps | LEDs: %u upd/s | Sensors: %u/s\n",
          stats.sensor_fps,
          stats.led_fps,
          stats.leds_updated,
          stats.sensor_reads
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
            xSemaphoreGive(led_data_mutex);
            
            // Debug: Print first LED color every 2 seconds
            if(millis() - last_debug_time > 2000){
              const RgbwColor& first_led = shared_led_data.leds[0];
              Serial.printf("CPU: LED RX - First LED: R=%d G=%d B=%d W=%d\n", 
                           first_led.r, first_led.g, first_led.b, first_led.w);
              last_debug_time = millis();
            }
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
    
    // Debug: Print loop counter every second to prove task is running
    if(millis() - last_status_print > 1000){
      Serial.printf("CPU: *** LED DISPLAY LOOP %lu *** have_data=%d\n", 
                   loop_count, have_led_data);
      last_status_print = millis();
      loop_count = 0;
    }
    
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
  
  // Create mutexes for shared data protection
  sensor_data_mutex = xSemaphoreCreateMutex();
  led_data_mutex = xSemaphoreCreateMutex();
  if(sensor_data_mutex == NULL || led_data_mutex == NULL){
    Serial.println("CPU: [ERROR] Failed to create mutexes!");
    while(1){
      delay(1000);
    }
  }
  
  // Initialize shared data
  memset(&shared_sensor_data, 0, sizeof(SensorDataPayload));
  memset(&shared_led_data, 0, sizeof(LedDataPayload));
  
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
  
  Serial.printf("CPU: xTaskCreatePinnedToCore returned: %d (pdPASS=%d)\n", result, pdPASS);
  Serial.flush();
  
  if(result != pdPASS){
    Serial.println("CPU: ERROR - Failed to create LED display task!");
  }else{
    Serial.println("CPU: LED display task created successfully");
  }
  
  Serial.println("CPU: All tasks created!");
  Serial.println("CPU: Core 0 - Sensor reading + UART RX (LED data) + LED display");
  Serial.println("CPU: Core 1 - UART TX (Sensor @ 60Hz)");
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
