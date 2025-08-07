#include "Arduino.h"
#include <Wire.h>
#include <Adafruit_ICM20948.h>
#include <Adafruit_BME280.h>

// I2C pins
#define SDA_PIN 9
#define SCL_PIN 10

Adafruit_ICM20948 icm;
Adafruit_BME280 bme;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Booting sensors...");

  Wire.begin(SDA_PIN, SCL_PIN);

  // Initialize ICM20948
  if (!icm.begin_I2C(0x68, &Wire)) {
    Serial.println("❌ ICM20948 not found at 0x68");
    while (true);
  }
  Serial.println("✅ ICM20948 OK");

  // Initialize BME280
  if (!bme.begin(0x76, &Wire)) {
    Serial.println("❌ BME280 not found at 0x76");
    while (true);
  }
  Serial.println("✅ BME280 OK");
  Serial.println("Setup complete.");
}

void loop() {
  sensors_event_t accel, gyro, mag, tempICM;
  icm.getEvent(&accel, &gyro, &tempICM, &mag);

  float tempBME = bme.readTemperature();
  float pressure = bme.readPressure() / 100.0F; // Convert to hPa
  float humidity = bme.readHumidity();

  // Output one-liner
  Serial.printf(
    "ICM_T=%.2f Accel[%.2f %.2f %.2f] Gyro[%.2f %.2f %.2f] Mag[%.2f %.2f %.2f] | "
    "BME_T=%.2f P=%.2f H=%.2f\n",
    tempICM.temperature,
    accel.acceleration.x, accel.acceleration.y, accel.acceleration.z,
    gyro.gyro.x, gyro.gyro.y, gyro.gyro.z,
    mag.magnetic.x, mag.magnetic.y, mag.magnetic.z,
    tempBME, pressure, humidity
  );

  delay(500); // 0.5 seconds
}
