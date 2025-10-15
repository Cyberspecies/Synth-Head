#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <ICM20948_WE.h>
#include <Wire.h>
#include <Adafruit_BME280.h>

// Pin Definitions
// I2C Bus
#define SDA_PIN 9
#define SCL_PIN 10

// GPS UART (NEO M8) - VERIFIED WORKING PINS!
#define GPS_TX_PIN 43  // ESP32 TX to GPS RX (TXD2)
#define GPS_RX_PIN 44  // ESP32 RX from GPS TX (RXD2)
#define GPS_BAUD 9600

// LED Strips
#define LED_STRIP_0 16
#define LED_STRIP_1 18  // Left Fin - 13 LEDs
#define LED_STRIP_2 8   // Fixed: was 5 (conflicted with Button A)
#define LED_STRIP_3 39
#define LED_STRIP_4 38  // Right Fin - 13 LEDs
#define LED_STRIP_5 37  // Scale LEDs - 14 LEDs

// Buttons
// NOTE: Button D is GPIO 15 on the hardware (was 10) per latest mapping
#define BUTTON_A 5
#define BUTTON_B 6
#define BUTTON_C 7
#define BUTTON_D 15

// LED Strip Configuration
#define NUM_LEDS_STRIP_0 10
#define NUM_LEDS_STRIP_1 13  // Left Fin
#define NUM_LEDS_STRIP_2 10
#define NUM_LEDS_STRIP_3 10
#define NUM_LEDS_STRIP_4 13  // Right Fin
#define NUM_LEDS_STRIP_5 14  // Scale LEDs

// NeoPixel Objects (WRGB / RGBW strips)
// Use NEO_RGBW flag for LED strips that have a white channel (WRGB). If your strips
// are actually GRB(W) reorder, change the order accordingly.
Adafruit_NeoPixel strip0(NUM_LEDS_STRIP_0, LED_STRIP_0, NEO_RGBW + NEO_KHZ800);
Adafruit_NeoPixel strip1(NUM_LEDS_STRIP_1, LED_STRIP_1, NEO_RGBW + NEO_KHZ800);
Adafruit_NeoPixel strip2(NUM_LEDS_STRIP_2, LED_STRIP_2, NEO_RGBW + NEO_KHZ800);
Adafruit_NeoPixel strip3(NUM_LEDS_STRIP_3, LED_STRIP_3, NEO_RGBW + NEO_KHZ800);
Adafruit_NeoPixel strip4(NUM_LEDS_STRIP_4, LED_STRIP_4, NEO_RGBW + NEO_KHZ800);
Adafruit_NeoPixel strip5(NUM_LEDS_STRIP_5, LED_STRIP_5, NEO_RGBW + NEO_KHZ800);

// Sensor Objects
ICM20948_WE icm_sensor = ICM20948_WE(0x68);
Adafruit_BME280 bme_sensor;

// GPS UART Serial (using HardwareSerial 2)
HardwareSerial gps_serial(2);

// GPS NMEA Parser Variables
String nmea_sentence = "";
double gps_latitude = 0.0;
double gps_longitude = 0.0;
uint8_t gps_satellites = 0;
uint8_t gps_fix_quality = 0;

// Timing Variables
unsigned long last_print_time = 0;
const unsigned long PRINT_INTERVAL = 1000;  // 1 second

// LED Animation Variables
uint8_t led_hue = 0;
uint8_t animation_step = 0;

// GPS Diagnostics
unsigned long last_gps_byte_time = 0;
int gps_byte_count = 0;

// Parse NMEA GPGGA sentence: $GPGGA,time,lat,N/S,lon,E/W,fix,sats,hdop,alt,M,...
// Example: $GPGGA,135025.00,3343.61042,S,15055.10503,E,1,07,1.17,79.1,M,19.6,M,,*75
void parseGpgga(String sentence){
  int field_index = 0;
  int start_pos = 0;
  String fields[15];
  
  // Split by commas
  for(int i = 0; i < sentence.length(); i++){
    if(sentence[i] == ',' || sentence[i] == '*'){
      fields[field_index++] = sentence.substring(start_pos, i);
      start_pos = i + 1;
      if(field_index >= 15) break;
    }
  }
  
  // Field 0: $GPGGA
  // Field 1: Time
  // Field 2: Latitude (DDMM.MMMMM)
  // Field 3: N/S
  // Field 4: Longitude (DDDMM.MMMMM)
  // Field 5: E/W
  // Field 6: Fix quality (0=none, 1=GPS, 2=DGPS)
  // Field 7: Number of satellites
  
  if(field_index >= 8 && fields[0] == "$GPGGA"){
    // Parse fix quality
    gps_fix_quality = fields[6].toInt();
    
    // Parse satellites
    gps_satellites = fields[7].toInt();
    
    // Parse latitude: DDMM.MMMMM -> DD.DDDDDD
    if(fields[2].length() > 0 && fields[3].length() > 0){
      double lat_raw = fields[2].toDouble();
      int lat_degrees = (int)(lat_raw / 100);
      double lat_minutes = lat_raw - (lat_degrees * 100);
      gps_latitude = lat_degrees + (lat_minutes / 60.0);
      if(fields[3] == "S") gps_latitude = -gps_latitude;
    }
    
    // Parse longitude: DDDMM.MMMMM -> DDD.DDDDDD
    if(fields[4].length() > 0 && fields[5].length() > 0){
      double lon_raw = fields[4].toDouble();
      int lon_degrees = (int)(lon_raw / 100);
      double lon_minutes = lon_raw - (lon_degrees * 100);
      gps_longitude = lon_degrees + (lon_minutes / 60.0);
      if(fields[5] == "W") gps_longitude = -gps_longitude;
    }
  }
}

// Process incoming GPS data
void processGpsData(){
  while(gps_serial.available() > 0){
    char c = gps_serial.read();
    gps_byte_count++;
    last_gps_byte_time = millis();
    
    // Build NMEA sentence
    if(c == '$'){
      nmea_sentence = "$";  // Start new sentence
    }else if(c == '\n'){
      // End of sentence - parse if it's GPGGA
      if(nmea_sentence.startsWith("$GPGGA")){
        parseGpgga(nmea_sentence);
      }
      nmea_sentence = "";
    }else if(nmea_sentence.length() < 100){  // Prevent overflow
      nmea_sentence += c;
    }
  }
}

void initializeLedStrips(){
  strip0.begin();
  strip0.setBrightness(50);
  strip0.show();
  
  strip1.begin();
  strip1.setBrightness(50);
  strip1.show();
  
  strip2.begin();
  strip2.setBrightness(50);
  strip2.show();
  
  strip3.begin();
  strip3.setBrightness(50);
  strip3.show();
  
  strip4.begin();
  strip4.setBrightness(50);
  strip4.show();
  
  strip5.begin();
  strip5.setBrightness(50);
  strip5.show();
}

void initializeButtons(){
  pinMode(BUTTON_A, INPUT_PULLUP);
  pinMode(BUTTON_B, INPUT_PULLUP);
  pinMode(BUTTON_C, INPUT_PULLUP);
  pinMode(BUTTON_D, INPUT_PULLUP);
}

bool initializeSensors(){
  Wire.begin(SDA_PIN, SCL_PIN);
  
  // Initialize ICM20948
  bool icm_ok = icm_sensor.init();
  if(icm_ok){
    icm_sensor.autoOffsets();
    icm_sensor.setAccRange(ICM20948_ACC_RANGE_2G);
    icm_sensor.setGyrRange(ICM20948_GYRO_RANGE_250);
    icm_sensor.setAccDLPF(ICM20948_DLPF_6);
    icm_sensor.setGyrDLPF(ICM20948_DLPF_6);
  }
  
  // Initialize BME280
  bool bme_ok = bme_sensor.begin(0x76);
  
  // Initialize GPS using simple HardwareSerial
  Serial.println("Initializing GPS (NEO-M8)...");
  Serial.print("  Config: TX=GPIO");
  Serial.print(GPS_TX_PIN);
  Serial.print(" RX=GPIO");
  Serial.print(GPS_RX_PIN);
  Serial.print(" @ ");
  Serial.print(GPS_BAUD);
  Serial.println(" baud");
  
  // Start GPS serial
  gps_serial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  Serial.println("  GPS serial started");
  
  // Check for data
  Serial.print("  Waiting for NMEA data (3 sec)...");
  unsigned long start = millis();
  int bytes_received = 0;
  while(millis() - start < 3000){
    if(gps_serial.available()){
      gps_serial.read();
      bytes_received++;
    }
    delay(1);
  }
  
  if(bytes_received > 0){
    Serial.print(" ✓ Received ");
    Serial.print(bytes_received);
    Serial.println(" bytes");
  }else{
    Serial.println(" ✗ No data");
  }
  
  Serial.println("  GPS initialization complete\n");
  
  return icm_ok && bme_ok;
}

void rainbowCycle(Adafruit_NeoPixel &strip, uint8_t offset){
  for(int i = 0; i < strip.numPixels(); i++){
    uint8_t pixel_hue = led_hue + (i * 256 / strip.numPixels()) + offset;
    strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(pixel_hue * 256)));
  }
  strip.show();
}

void updateLedAnimations(){
  rainbowCycle(strip0, 0);
  rainbowCycle(strip1, 42);
  rainbowCycle(strip2, 84);
  rainbowCycle(strip3, 126);
  rainbowCycle(strip4, 168);
  rainbowCycle(strip5, 210);
  
  led_hue += 2;
}

void readAndPrintSensors(){
  // Read Button States
  bool btn_a = !digitalRead(BUTTON_A);  // Inverted because of pullup
  bool btn_b = !digitalRead(BUTTON_B);
  bool btn_c = !digitalRead(BUTTON_C);
  bool btn_d = !digitalRead(BUTTON_D);
  
  // Read ICM20948 Data
  xyzFloat gyro_data;
  xyzFloat accel_data;
  xyzFloat mag_data;
  
  icm_sensor.readSensor();
  icm_sensor.getGyrValues(&gyro_data);
  icm_sensor.getAccRawValues(&accel_data);
  icm_sensor.getMagValues(&mag_data);

  // If magnetometer reads all zeros, log diagnostic and suggest a hardware check
  static bool mag_diag_reported = false;
  if(mag_data.x == 0.0f && mag_data.y == 0.0f && mag_data.z == 0.0f && !mag_diag_reported){
    Serial.println("ICM MAG: zeros detected — check magnetometer wiring, power, and I2C address");
    mag_diag_reported = true;
  }
  
  // Read BME280 Data
  float temperature = bme_sensor.readTemperature();
  float pressure = bme_sensor.readPressure() / 100.0F;  // Convert to hPa
  float humidity = bme_sensor.readHumidity();
  
  // Process GPS data
  processGpsData();
  
  // Print everything in one line
  Serial.printf("BTN[A:%d B:%d C:%d D:%d] | ICM[G(%.1f,%.1f,%.1f) A(%.1f,%.1f,%.1f)] | BME[T:%.1fC P:%.1fhPa H:%.1f%%] | GPS[Fix:%d Sat:%d Lat:%.6f Lon:%.6f]\n",
    btn_a, btn_b, btn_c, btn_d,
    gyro_data.x, gyro_data.y, gyro_data.z,
    accel_data.x, accel_data.y, accel_data.z,
    temperature, pressure, humidity,
    gps_fix_quality, gps_satellites, gps_latitude, gps_longitude
  );
}

void setup(){
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n=== Synth-Head Test Program ===");
  Serial.println("Initializing...\n");
  
  // Initialize Buttons
  Serial.print("Buttons... ");
  initializeButtons();
  Serial.println("OK");
  
  // Initialize LED Strips
  Serial.print("LED Strips... ");
  initializeLedStrips();
  Serial.println("OK");
  
  // Initialize Sensors
  Serial.print("Sensors (ICM20948, BME280 & NEO-M8)... ");
  if(initializeSensors()){
    Serial.println("OK");
  }else{
    Serial.println("FAILED (check connections)");
  }
  
  Serial.println("\n=== Test Running ===");
  Serial.println("Format: BTN[A B C D] | ICM[Gyro Accel Mag] | BME[Temp Press Humidity] | GPS[Sat Lat Lon]\n");
  
  last_print_time = millis();
}

void loop(){
  // Update LED animations continuously
  updateLedAnimations();
  
  // Print sensor data every 1 second
  unsigned long current_time = millis();
  if(current_time - last_print_time >= PRINT_INTERVAL){
    readAndPrintSensors();
    last_print_time = current_time;
  }
  
  delay(20);  // Small delay for smooth animations
}
