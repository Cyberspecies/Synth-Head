#ifndef UART_CONTROLLER_IMPL_HPP
#define UART_CONTROLLER_IMPL_HPP

#include "UartController.h"

// Constructor
UartController::UartController()
  : uart_serial(nullptr), buffer_index(0), last_frame_counter(0), 
    total_frames_received(0), frames_skipped(0), frames_corrupted(0),
    sync_failures(0), total_bytes_received(0){
  memset(receive_buffer, 0, TOTAL_BUFFER_SIZE);
  memset(button_state, 0, sizeof(button_state));
  memset(last_button_state, 0, sizeof(last_button_state));
}

// Destructor
UartController::~UartController(){
  if(uart_serial != nullptr){
    uart_serial->end();
  }
}

// Initialize UART and buttons
bool UartController::initialize(){
  // Initialize UART on Serial1 with custom pins
  uart_serial = &Serial1;
  
  Serial.println("Initializing UART...");
  Serial.printf("  RX Pin: GPIO %d\n", UART_RX_PIN);
  Serial.printf("  TX Pin: GPIO %d\n", UART_TX_PIN);
  Serial.printf("  Baud Rate: %d\n", UART_BAUD_RATE);
  Serial.printf("  RX Buffer Size: %d bytes\n", UART_RX_BUFFER_SIZE);
  
  // Set large RX buffer BEFORE calling begin()
  uart_serial->setRxBufferSize(UART_RX_BUFFER_SIZE);
  uart_serial->begin(UART_BAUD_RATE, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);
  
  if(!uart_serial){
    Serial.println("ERROR: Failed to initialize UART");
    return false;
  }
  
  delay(100); // Allow UART to stabilize
  
  // Test if UART is responsive
  Serial.printf("UART initialized. Testing RX pin...\n");
  Serial.printf("  Initial available bytes: %d\n", uart_serial->available());
  
  // Initialize buttons
  initializeButtons();
  
  Serial.println("UartController initialized successfully");
  Serial.printf("Expected data size: %d bytes (%d LEDs x %d bytes)\n", 
                TOTAL_BUFFER_SIZE, TOTAL_LED_COUNT, BYTES_PER_LED);
  Serial.println("Waiting for UART data from GPU...");
  
  return true;
}

// Initialize button pins
void UartController::initializeButtons(){
  pinMode(BUTTON_A_PIN, INPUT_PULLUP);
  pinMode(BUTTON_B_PIN, INPUT_PULLUP);
  pinMode(BUTTON_C_PIN, INPUT_PULLUP);
  pinMode(BUTTON_D_PIN, INPUT_PULLUP);
  
  Serial.println("Buttons initialized (Active LOW with pullup)");
  Serial.printf("  Button A: GPIO %d\n", BUTTON_A_PIN);
  Serial.printf("  Button B: GPIO %d\n", BUTTON_B_PIN);
  Serial.printf("  Button C: GPIO %d\n", BUTTON_C_PIN);
  Serial.printf("  Button D: GPIO %d (WARNING: Shared with I2C SCL)\n", BUTTON_D_PIN);
  
  // Test initial pin states
  delay(100);
  Serial.println("Initial pin states (raw digital reads):");
  Serial.printf("  A(GPIO%d)=%d, B(GPIO%d)=%d, C(GPIO%d)=%d, D(GPIO%d)=%d\n",
                BUTTON_A_PIN, digitalRead(BUTTON_A_PIN),
                BUTTON_B_PIN, digitalRead(BUTTON_B_PIN),
                BUTTON_C_PIN, digitalRead(BUTTON_C_PIN),
                BUTTON_D_PIN, digitalRead(BUTTON_D_PIN));
}

// Read button states
void UartController::readButtons(){
  // Store previous state
  memcpy(last_button_state, button_state, sizeof(button_state));
  
  // Read current state (buttons are active LOW with pullup)
  button_state[0] = !digitalRead(BUTTON_A_PIN);
  button_state[1] = !digitalRead(BUTTON_B_PIN);
  button_state[2] = !digitalRead(BUTTON_C_PIN);
  button_state[3] = !digitalRead(BUTTON_D_PIN);
}

// Send button state over UART
void UartController::sendButtonState(){
  if(!uart_serial){
    return;
  }
  
  // Send 4 bytes: one for each button state (0x00 or 0x01)
  uint8_t button_data[4];
  button_data[0] = button_state[0] ? 0x01 : 0x00;
  button_data[1] = button_state[1] ? 0x01 : 0x00;
  button_data[2] = button_state[2] ? 0x01 : 0x00;
  button_data[3] = button_state[3] ? 0x01 : 0x00;
  
  uart_serial->write(button_data, 4);
}

// Calculate CRC8 checksum
uint8_t UartController::calculateCRC8(const uint8_t* data, size_t length){
  uint8_t crc = 0x00;
  for(size_t i = 0; i < length; i++){
    crc ^= data[i];
    for(int j = 0; j < 8; j++){
      if(crc & 0x80){
        crc = (crc << 1) ^ 0x07; // CRC-8 polynomial
      }else{
        crc <<= 1;
      }
    }
  }
  return crc;
}

// Find sync marker in UART stream
bool UartController::findSyncMarker(){
  const int MAX_SEARCH_BYTES = 400; // Don't search forever
  int bytes_searched = 0;
  
  while(uart_serial->available() >= 2 && bytes_searched < MAX_SEARCH_BYTES){
    if(uart_serial->peek() == SYNC_BYTE_1){
      uint8_t first_byte = uart_serial->read();
      total_bytes_received++;
      
      if(uart_serial->available() > 0 && uart_serial->peek() == SYNC_BYTE_2){
        uint8_t second_byte = uart_serial->read();
        total_bytes_received++;
        
        // Found sync marker!
        receive_buffer[0] = first_byte;
        receive_buffer[1] = second_byte;
        return true;
      }
    }else{
      uart_serial->read(); // Discard byte
      total_bytes_received++;
      bytes_searched++;
    }
  }
  
  sync_failures++;
  return false; // Sync not found
}

// Receive LED data from UART with sync detection and CRC validation
bool UartController::receiveData(){
  if(!uart_serial){
    return false;
  }
  
  // Need enough bytes for a complete frame
  if(uart_serial->available() < TOTAL_BUFFER_SIZE){
    return false;
  }
  
  // Find sync marker
  if(!findSyncMarker()){
    return false; // No sync found
  }
  
  // Read rest of frame (198 bytes: LED data + counter + CRC)
  int remaining = TOTAL_BUFFER_SIZE - SYNC_BYTES;
  int bytes_read = uart_serial->readBytes(&receive_buffer[SYNC_BYTES], remaining);
  total_bytes_received += bytes_read;
  
  if(bytes_read != remaining){
    return false; // Incomplete frame
  }
  
  // Validate CRC (over all bytes except last CRC byte)
  uint8_t received_crc = receive_buffer[TOTAL_BUFFER_SIZE - 1];
  uint8_t calculated_crc = calculateCRC8(receive_buffer, TOTAL_BUFFER_SIZE - CRC_BYTES);
  
  if(received_crc != calculated_crc){
    frames_corrupted++;
    return false; // CRC mismatch, frame corrupted
  }
  
  // Extract frame counter (at byte 198)
  uint8_t current_frame_counter = receive_buffer[SYNC_BYTES + LED_DATA_BYTES];
  
  // Detect frame skipping (frame counter cycles 1-60)
  if(total_frames_received > 0){
    uint8_t expected_counter = last_frame_counter + 1;
    if(expected_counter > 60){
      expected_counter = 1;
    }
    
    // Check if we skipped frames
    if(current_frame_counter != expected_counter){
      // Calculate how many frames were skipped
      int skipped;
      if(current_frame_counter > expected_counter){
        skipped = current_frame_counter - expected_counter;
      }else{
        // Wrapped around (e.g., expected 59, got 2)
        skipped = (60 - expected_counter) + current_frame_counter;
      }
      frames_skipped += skipped;
    }
  }
  
  last_frame_counter = current_frame_counter;
  total_frames_received++;
  buffer_index = 0;
  
  return true; // Valid frame received
}

// Check if new data is available
bool UartController::hasNewData() const{
  return uart_serial && uart_serial->available() >= TOTAL_BUFFER_SIZE;
}

// Get left fin LED data (first 13 LEDs)
// LED data starts at offset 2 (after sync bytes)
const uint8_t* UartController::getLeftFinData() const{
  return &receive_buffer[SYNC_BYTES];
}

// Get right fin LED data (next 13 LEDs)
const uint8_t* UartController::getRightFinData() const{
  return &receive_buffer[SYNC_BYTES + (LEFT_FIN_LED_COUNT * BYTES_PER_LED)];
}

// Get tongue LED data (next 9 LEDs)
const uint8_t* UartController::getTongueData() const{
  return &receive_buffer[SYNC_BYTES + ((LEFT_FIN_LED_COUNT + RIGHT_FIN_LED_COUNT) * BYTES_PER_LED)];
}

// Get scale LED data (last 14 LEDs)
const uint8_t* UartController::getScaleData() const{
  return &receive_buffer[SYNC_BYTES + ((LEFT_FIN_LED_COUNT + RIGHT_FIN_LED_COUNT + TONGUE_LED_COUNT) * BYTES_PER_LED)];
}

// Get individual LED RGBW values
void UartController::getLedRgbw(int led_index, uint8_t& r, uint8_t& g, uint8_t& b, uint8_t& w) const{
  if(led_index < 0 || led_index >= TOTAL_LED_COUNT){
    r = g = b = w = 0;
    return;
  }
  
  int base_index = led_index * BYTES_PER_LED;
  r = receive_buffer[base_index];
  g = receive_buffer[base_index + 1];
  b = receive_buffer[base_index + 2];
  w = receive_buffer[base_index + 3];
}

// Get number of frames skipped
uint32_t UartController::getFramesSkipped() const{
  return frames_skipped;
}

// Get number of corrupted frames
uint32_t UartController::getFramesCorrupted() const{
  return frames_corrupted;
}

// Get number of sync failures
uint32_t UartController::getSyncFailures() const{
  return sync_failures;
}

// Get button state
bool UartController::getButtonState(int button_index) const{
  if(button_index < 0 || button_index >= 4){
    return false;
  }
  return button_state[button_index];
}

// Get button pressed event (rising edge detection)
bool UartController::getButtonPressed(int button_index) const{
  if(button_index < 0 || button_index >= 4){
    return false;
  }
  return button_state[button_index] && !last_button_state[button_index];
}

// Clear receive buffer
void UartController::clearBuffer(){
  memset(receive_buffer, 0, TOTAL_BUFFER_SIZE);
  buffer_index = 0;
}

// Validate received data (basic check)
bool UartController::validateReceivedData(){
  // Add custom validation logic here if needed
  // For example, check for start/end markers, checksums, etc.
  return true;
}

// Main update loop
void UartController::update(){
  // Read button states
  readButtons();
  
  // Send button state over UART
  sendButtonState();
  
  // Receive LED data
  receiveData();
}

// Print received LED data for debugging
void UartController::printReceivedData() const{
  Serial.println("\n=== Received LED Data ===");
  
  // Left Fin (13 LEDs)
  Serial.println("Left Fin (13 LEDs):");
  for(int i = 0; i < LEFT_FIN_LED_COUNT; i++){
    int base = i * BYTES_PER_LED;
    Serial.printf("  LED %2d: R=%3d G=%3d B=%3d W=%3d\n", 
                  i, receive_buffer[base], receive_buffer[base+1], 
                  receive_buffer[base+2], receive_buffer[base+3]);
  }
  
  // Right Fin (13 LEDs)
  Serial.println("Right Fin (13 LEDs):");
  int offset = LEFT_FIN_LED_COUNT * BYTES_PER_LED;
  for(int i = 0; i < RIGHT_FIN_LED_COUNT; i++){
    int base = offset + (i * BYTES_PER_LED);
    Serial.printf("  LED %2d: R=%3d G=%3d B=%3d W=%3d\n", 
                  i, receive_buffer[base], receive_buffer[base+1], 
                  receive_buffer[base+2], receive_buffer[base+3]);
  }
  
  // Tongue (9 LEDs)
  Serial.println("Tongue (9 LEDs):");
  offset = (LEFT_FIN_LED_COUNT + RIGHT_FIN_LED_COUNT) * BYTES_PER_LED;
  for(int i = 0; i < TONGUE_LED_COUNT; i++){
    int base = offset + (i * BYTES_PER_LED);
    Serial.printf("  LED %2d: R=%3d G=%3d B=%3d W=%3d\n", 
                  i, receive_buffer[base], receive_buffer[base+1], 
                  receive_buffer[base+2], receive_buffer[base+3]);
  }
  
  // Scale (14 LEDs)
  Serial.println("Scale (14 LEDs):");
  offset = (LEFT_FIN_LED_COUNT + RIGHT_FIN_LED_COUNT + TONGUE_LED_COUNT) * BYTES_PER_LED;
  for(int i = 0; i < SCALE_LED_COUNT; i++){
    int base = offset + (i * BYTES_PER_LED);
    Serial.printf("  LED %2d: R=%3d G=%3d B=%3d W=%3d\n", 
                  i, receive_buffer[base], receive_buffer[base+1], 
                  receive_buffer[base+2], receive_buffer[base+3]);
  }
  
  Serial.println("========================\n");
}

// Get frame counter from last received frame
uint8_t UartController::getFrameCounter() const{
  return last_frame_counter;
}

// Get total frames received
uint32_t UartController::getTotalFramesReceived() const{
  return total_frames_received;
}

// Print button states for debugging
void UartController::printButtonStates() const{
  // Print processed button states
  Serial.printf("Button States: A=%d B=%d C=%d D=%d | ",
                button_state[0], button_state[1], 
                button_state[2], button_state[3]);
  
  // Print raw pin reads
  Serial.printf("Raw Pins: A(GPIO%d)=%d B(GPIO%d)=%d C(GPIO%d)=%d D(GPIO%d)=%d\n",
                BUTTON_A_PIN, digitalRead(BUTTON_A_PIN),
                BUTTON_B_PIN, digitalRead(BUTTON_B_PIN),
                BUTTON_C_PIN, digitalRead(BUTTON_C_PIN),
                BUTTON_D_PIN, digitalRead(BUTTON_D_PIN));
}

#endif // UART_CONTROLLER_IMPL_HPP
