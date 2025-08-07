#include <Arduino.h>

// Data lines
#define HUB75_R0 7
#define HUB75_G0 15
#define HUB75_B0 16
#define HUB75_R1 17
#define HUB75_G1 18
#define HUB75_B1 8

// Address lines
#define HUB75_A  41
#define HUB75_B  40
#define HUB75_C  39
#define HUB75_D  38
#define HUB75_E  42

// Control lines
#define HUB75_LAT 36
#define HUB75_OE0 35
#define HUB75_OE1 6
#define HUB75_CLK 37

void Row_Select(int row){
    digitalWrite(HUB75_A, (row >> 0) & 0x01);
    digitalWrite(HUB75_B, (row >> 1) & 0x01);
    digitalWrite(HUB75_C, (row >> 2) & 0x01);
    digitalWrite(HUB75_D, (row >> 3) & 0x01);
    digitalWrite(HUB75_E, (row >> 4) & 0x01);
}

void Clock(){
    digitalWrite(HUB75_CLK, HIGH);
    digitalWrite(HUB75_CLK, LOW);
}

void setup(){
    pinMode(HUB75_R0 , OUTPUT);
    pinMode(HUB75_G0 , OUTPUT);
    pinMode(HUB75_B0 , OUTPUT);
    pinMode(HUB75_R1 , OUTPUT);
    pinMode(HUB75_G1 , OUTPUT);
    pinMode(HUB75_B1 , OUTPUT);
    pinMode(HUB75_A  , OUTPUT);
    pinMode(HUB75_B  , OUTPUT);
    pinMode(HUB75_C  , OUTPUT);
    pinMode(HUB75_D  , OUTPUT);
    pinMode(HUB75_E  , OUTPUT);
    pinMode(HUB75_LAT, OUTPUT);
    pinMode(HUB75_OE0, OUTPUT);
    pinMode(HUB75_OE1, OUTPUT);
    pinMode(HUB75_CLK, OUTPUT);

    Serial.begin(115200);
}

void loop(){
    digitalWrite(HUB75_R0, HIGH);
    digitalWrite(HUB75_G0, HIGH);
    digitalWrite(HUB75_B0, LOW );
    digitalWrite(HUB75_R1, HIGH);
    digitalWrite(HUB75_G1, HIGH);
    digitalWrite(HUB75_B1, LOW );

    for(size_t i = 0; i < 32; i++){
        Row_Select(i);

        for(size_t j = 0; j < 64; j++){
            Clock();
        }

        digitalWrite(HUB75_LAT, HIGH);
        digitalWrite(HUB75_LAT, LOW);
        delayMicroseconds(20);
        digitalWrite(HUB75_OE0, HIGH);
        delayMicroseconds(20);
        digitalWrite(HUB75_OE0, LOW);
    }

    digitalWrite(HUB75_R0, LOW );
    digitalWrite(HUB75_G0, HIGH);
    digitalWrite(HUB75_B0, HIGH);
    digitalWrite(HUB75_R1, LOW );
    digitalWrite(HUB75_G1, HIGH);
    digitalWrite(HUB75_B1, HIGH);

    for(size_t i = 0; i < 32; i++){
        Row_Select(i);

        for(size_t j = 0; j < 64; j++){
            Clock();
        }

        digitalWrite(HUB75_LAT, HIGH);
        digitalWrite(HUB75_LAT, LOW);
        delayMicroseconds(20);
        digitalWrite(HUB75_OE1, HIGH);
        delayMicroseconds(20);
        digitalWrite(HUB75_OE1, LOW);
    }

}