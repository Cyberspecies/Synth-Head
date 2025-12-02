/*****************************************************************
 * File:      uart_loopback_test.cpp
 * Category:  testing/experimentation
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Simple UART loopback test. Connect TX to RX on same board.
 *****************************************************************/

#include <Arduino.h>

void setup(){
  Serial.begin(115200);
  while(!Serial){}
  Serial.println("=== UART Loopback Test ===");
  Serial.println("Connect GPIO 17 (TX) to GPIO 16 (RX) on THIS board");
  Serial.println("Starting in 3 seconds...");
  delay(3000);
  
  Serial2.begin(2000000, SERIAL_8N1, 16, 17); // RX=16, TX=17
  Serial.println("UART2 initialized at 2Mbaud");
}

void loop(){
  // Send test data
  const char* test_msg = "TEST123";
  Serial2.write(test_msg);
  Serial2.flush();
  
  delay(10);
  
  // Try to receive
  if(Serial2.available()){
    Serial.print("RECEIVED: ");
    while(Serial2.available()){
      char c = Serial2.read();
      Serial.print(c);
    }
    Serial.println(" [SUCCESS - UART WORKING]");
  }else{
    Serial.println("NO DATA RECEIVED [FAIL - Check wiring or UART pins]");
  }
  
  delay(1000);
}
