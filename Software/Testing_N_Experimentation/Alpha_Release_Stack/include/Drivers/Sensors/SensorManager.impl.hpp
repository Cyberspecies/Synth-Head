/*****************************************************************
 * File:      SensorManager.impl.hpp
 * Category:  drivers/sensors
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Implementation file for unified sensor manager that
 *    registers sensors and caches their values for easy access.
 *****************************************************************/

#ifndef ALPHA_RELEASE_STACK_DRIVERS_SENSORS_SENSOR_MANAGER_IMPL_HPP_
#define ALPHA_RELEASE_STACK_DRIVERS_SENSORS_SENSOR_MANAGER_IMPL_HPP_

namespace sensors{

SensorManager::SensorManager()
  : imu_sensor_(nullptr),
    env_sensor_(nullptr),
    gps_sensor_(nullptr),
    mic_sensor_(nullptr),
    imu_valid_(false),
    env_valid_(false),
    gps_valid_(false),
    mic_valid_(false),
    initialized_(false){
}

SensorManager::~SensorManager(){
  if(imu_sensor_) delete imu_sensor_;
  if(env_sensor_) delete env_sensor_;
  if(gps_sensor_) delete gps_sensor_;
  if(mic_sensor_) delete mic_sensor_;
}

bool SensorManager::init(){
  return init(SensorManagerConfig());
}

bool SensorManager::init(const SensorManagerConfig& config){
  if(initialized_){
    Serial.println("[SensorManager] Already initialized");
    return true;
  }

  config_ = config;
  
  Serial.println("\n=======================================================");
  Serial.println("  Sensor Manager Initialization");
  Serial.println("=======================================================\n");
  
  Serial.printf("I2C Configuration:\n");
  Serial.printf("  SDA Pin: %d\n", config_.i2c_sda_pin);
  Serial.printf("  SCL Pin: %d\n", config_.i2c_scl_pin);
  Serial.printf("  ICM20948 Address: 0x%02X\n", config_.icm20948_address);
  Serial.printf("  BME280 Address: 0x%02X\n", config_.bme280_address);
  Serial.println();
  
  Serial.printf("UART Configuration (GPS):\n");
  Serial.printf("  TX Pin: %d\n", config_.gps_tx_pin);
  Serial.printf("  RX Pin: %d\n", config_.gps_rx_pin);
  Serial.println();
  
  Serial.printf("I2S Configuration (Microphone):\n");
  Serial.printf("  WS Pin: %d\n", config_.mic_ws_pin);
  Serial.printf("  SCK Pin: %d\n", config_.mic_sck_pin);
  Serial.printf("  SD Pin: %d\n", config_.mic_sd_pin);
  Serial.printf("  LR Pin: %d\n", config_.mic_lr_pin);
  Serial.println();

  bool all_success = true;

  // Initialize ICM20948 IMU
  Serial.println("--- Initializing ICM20948 (IMU) ---");
  imu_sensor_ = new Icm20948Sensor(config_.i2c_sda_pin, config_.i2c_scl_pin, 
                                    config_.icm20948_address);
  if(!imu_sensor_->init()){
    Serial.println("[ERROR] ICM20948 initialization failed!");
    all_success = false;
  }else{
    Serial.println("[SUCCESS] ICM20948 initialized\n");
  }

  // Initialize BME280 Environmental Sensor
  Serial.println("--- Initializing BME280 (Environmental) ---");
  env_sensor_ = new Bme280Sensor(config_.i2c_sda_pin, config_.i2c_scl_pin, 
                                  config_.bme280_address);
  if(!env_sensor_->init()){
    Serial.println("[ERROR] BME280 initialization failed!");
    all_success = false;
  }else{
    Serial.println("[SUCCESS] BME280 initialized\n");
  }

  // Initialize NEO-8M GPS
  Serial.println("--- Initializing NEO-8M GPS ---");
  gps_sensor_ = new Neo8mGps(config_.gps_tx_pin, config_.gps_rx_pin);
  if(!gps_sensor_->init()){
    Serial.println("[ERROR] NEO-8M GPS initialization failed!");
    all_success = false;
  }else{
    Serial.println("[SUCCESS] NEO-8M GPS initialized\n");
  }

  // Initialize INMP441 Microphone
  Serial.println("--- Initializing INMP441 Microphone ---");
  Inmp441Config mic_config;
  mic_config.ws_pin = config_.mic_ws_pin;
  mic_config.sck_pin = config_.mic_sck_pin;
  mic_config.sd_pin = config_.mic_sd_pin;
  mic_config.lr_select_pin = config_.mic_lr_pin;
  mic_sensor_ = new Inmp441Microphone(mic_config);
  if(!mic_sensor_->init()){
    Serial.println("[ERROR] INMP441 Microphone initialization failed!");
    all_success = false;
  }else{
    Serial.println("[SUCCESS] INMP441 Microphone initialized\n");
  }

  if(all_success){
    Serial.println("=======================================================");
    Serial.println("  All Sensors Initialized Successfully!");
    Serial.println("=======================================================\n");
    initialized_ = true;
  }else{
    Serial.println("=======================================================");
    Serial.println("  Warning: Some sensors failed to initialize");
    Serial.println("=======================================================\n");
    initialized_ = true; // Still mark as initialized to allow partial operation
  }

  return all_success;
}

void SensorManager::update(){
  if(!initialized_){
    return;
  }

  // Update GPS UART processing
  if(gps_sensor_ && gps_sensor_->isInitialized()){
    gps_sensor_->update();
  }

  // Update cached sensor values
  if(imu_sensor_ && imu_sensor_->isInitialized()){
    imu_valid_ = imu_sensor_->readData(imu_data_);
  }

  if(env_sensor_ && env_sensor_->isInitialized()){
    env_valid_ = env_sensor_->readData(env_data_);
  }

  if(gps_sensor_ && gps_sensor_->isInitialized()){
    gps_valid_ = gps_sensor_->getData(gps_data_);
  }

  if(mic_sensor_ && mic_sensor_->isInitialized()){
    mic_valid_ = mic_sensor_->update();
    if(mic_valid_){
      mic_data_ = mic_sensor_->getAudioData();
    }
  }
}

} // namespace sensors

#endif // ALPHA_RELEASE_STACK_DRIVERS_SENSORS_SENSOR_MANAGER_IMPL_HPP_
