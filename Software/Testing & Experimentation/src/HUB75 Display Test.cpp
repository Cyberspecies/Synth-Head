#include <Arduino.h>
#include <cstdint>
#include "driver/gpio.h"

// --- Display settings ---
#define DISPLAY_WIDTH 64
#define DISPLAY_HEIGHT 32

// --- Rectangle settings ---
#define RECT_WIDTH 5
#define RECT_HEIGHT 3
#define RECT_COLOR 255
#define MOVE_DELAY 50

// --- HUB75 pin definitions ---
#define HUB75_R0 7
#define HUB75_G0 15
#define HUB75_B0 16
#define HUB75_R1 17
#define HUB75_G1 18
#define HUB75_B1 8
#define HUB75_A  41
#define HUB75_B  40
#define HUB75_C  39
#define HUB75_D  38
#define HUB75_E  42
#define HUB75_LAT 36
#define HUB75_OE0 35 // Panel 0
#define HUB75_OE1 6  // Panel 1
#define HUB75_CLK 37

// --- Fast GPIO macros for ESP32 ---
#define SET_PIN(pin)   gpio_set_level((gpio_num_t)pin, 1)
#define CLEAR_PIN(pin) gpio_set_level((gpio_num_t)pin, 0)

// --- Display buffers ---
uint32_t* display0 = nullptr;
uint32_t* display1 = nullptr;

// --- Rectangle positions ---
int rect0X = DISPLAY_WIDTH / 2;
int rect0Y = DISPLAY_HEIGHT / 2;
int rect1X = DISPLAY_WIDTH / 2;
int rect1Y = DISPLAY_HEIGHT / 2;

// --- Helper functions ---
uint32_t rgb(uint8_t r, uint8_t g, uint8_t b) {
    return (r << 16) | (g << 8) | b;
}

uint32_t* generateDisplayBuffer(int width, int height) {
    return new uint32_t[width * height]{0};
}

void clearDisplay(uint32_t* buffer) {
    for (int i = 0; i < DISPLAY_WIDTH * DISPLAY_HEIGHT; ++i)
        buffer[i] = 0;
}

void drawRectangle(uint32_t* buffer, int centerX, int centerY, int width, int height, uint8_t r, uint8_t g, uint8_t b) {
    int halfW = width / 2;
    int halfH = height / 2;
    uint32_t color = rgb(r, g, b);

    for (int y = centerY - halfH; y <= centerY + halfH; ++y) {
        if (y < 0 || y >= DISPLAY_HEIGHT) continue;
        for (int x = centerX - halfW; x <= centerX + halfW; ++x) {
            if (x < 0 || x >= DISPLAY_WIDTH) continue;
            buffer[y * DISPLAY_WIDTH + x] = color;
        }
    }
}

// --- HUB75 helpers using W1TS/W1TC ---
void Row_Select(int row){
    CLEAR_PIN(HUB75_A); CLEAR_PIN(HUB75_B); CLEAR_PIN(HUB75_C);
    CLEAR_PIN(HUB75_D); CLEAR_PIN(HUB75_E);
    if(row & 0x01) SET_PIN(HUB75_A);
    if(row & 0x02) SET_PIN(HUB75_B);
    if(row & 0x04) SET_PIN(HUB75_C);
    if(row & 0x08) SET_PIN(HUB75_D);
    if(row & 0x10) SET_PIN(HUB75_E);
}

void Clock(){
    SET_PIN(HUB75_CLK);
    CLEAR_PIN(HUB75_CLK);
}

void SetRGBPins(uint8_t r0, uint8_t g0, uint8_t b0, uint8_t r1, uint8_t g1, uint8_t b1) {
    (r0 > 127) ? SET_PIN(HUB75_R0) : CLEAR_PIN(HUB75_R0);
    (g0 > 127) ? SET_PIN(HUB75_G0) : CLEAR_PIN(HUB75_G0);
    (b0 > 127) ? SET_PIN(HUB75_B0) : CLEAR_PIN(HUB75_B0);

    (r1 > 127) ? SET_PIN(HUB75_R1) : CLEAR_PIN(HUB75_R1);
    (g1 > 127) ? SET_PIN(HUB75_G1) : CLEAR_PIN(HUB75_G1);
    (b1 > 127) ? SET_PIN(HUB75_B1) : CLEAR_PIN(HUB75_B1);
}

// --- Drive panel independently ---
void DrivePanel(uint32_t* buffer, int width, int height, int oePin) {
    int halfHeight = height / 2;
    for (int row = 0; row < halfHeight; ++row) {
        Row_Select(row);
        for (int col = 0; col < width; ++col) {
            uint32_t topPixel = buffer[row * width + col];
            uint32_t bottomPixel = buffer[(row + halfHeight) * width + col];

            uint8_t r0 = (topPixel >> 16) & 0xFF;
            uint8_t g0 = (topPixel >> 8) & 0xFF;
            uint8_t b0 = topPixel & 0xFF;

            uint8_t r1 = (bottomPixel >> 16) & 0xFF;
            uint8_t g1 = (bottomPixel >> 8) & 0xFF;
            uint8_t b1 = bottomPixel & 0xFF;

            SetRGBPins(r0, g0, b0, r1, g1, b1);
            Clock();
        }
        // Latch and OE with minimal delay
        SET_PIN(HUB75_LAT); 
        delayMicroseconds(1); 
        CLEAR_PIN(HUB75_LAT);

        SET_PIN(oePin); 
        delayMicroseconds(1); 
        CLEAR_PIN(oePin);
    }
}

// --- Serial input ---
void handleSerialInput() {
    while (Serial.available() > 0) {
        int c = Serial.read();
        // Panel 1 arrow keys
        if(c == 27 && Serial.available() > 1 && Serial.read() == '[') {
            int dir = Serial.read();
            switch(dir){
                case 'A': rect1Y--; break;
                case 'B': rect1Y++; break;
                case 'C': rect1X++; break;
                case 'D': rect1X--; break;
            }
        }
        // Panel 0 WASD keys
        else {
            switch(c){
                case 'w': rect0Y--; break;
                case 's': rect0Y++; break;
                case 'a': rect0X--; break;
                case 'd': rect0X++; break;
            }
        }
        // Clamp positions
        rect0X = constrain(rect0X, 0, DISPLAY_WIDTH-1);
        rect0Y = constrain(rect0Y, 0, DISPLAY_HEIGHT-1);
        rect1X = constrain(rect1X, 0, DISPLAY_WIDTH-1);
        rect1Y = constrain(rect1Y, 0, DISPLAY_HEIGHT-1);
    }
}

// --- Setup ---
void setup() {
    // Set pins as output
    pinMode(HUB75_R0 , OUTPUT); pinMode(HUB75_G0 , OUTPUT); pinMode(HUB75_B0 , OUTPUT);
    pinMode(HUB75_R1 , OUTPUT); pinMode(HUB75_G1 , OUTPUT); pinMode(HUB75_B1 , OUTPUT);
    pinMode(HUB75_A  , OUTPUT); pinMode(HUB75_B  , OUTPUT); pinMode(HUB75_C  , OUTPUT);
    pinMode(HUB75_D  , OUTPUT); pinMode(HUB75_E  , OUTPUT);
    pinMode(HUB75_LAT, OUTPUT); pinMode(HUB75_OE0, OUTPUT); pinMode(HUB75_OE1, OUTPUT);
    pinMode(HUB75_CLK, OUTPUT);

    Serial.begin(115200);

    // Allocate buffers
    display0 = generateDisplayBuffer(DISPLAY_WIDTH, DISPLAY_HEIGHT);
    display1 = generateDisplayBuffer(DISPLAY_WIDTH, DISPLAY_HEIGHT);
}

// --- Loop ---
void loop() {
    handleSerialInput();

    // Clear and draw rectangles
    clearDisplay(display0);
    drawRectangle(display0, rect0X, rect0Y, RECT_WIDTH, RECT_HEIGHT, RECT_COLOR, 0, 0);

    clearDisplay(display1);
    drawRectangle(display1, rect1X, rect1Y, RECT_WIDTH, RECT_HEIGHT, 0, RECT_COLOR, RECT_COLOR);

    // Drive panels independently
    DrivePanel(display0, DISPLAY_WIDTH, DISPLAY_HEIGHT, HUB75_OE0);
    DrivePanel(display1, DISPLAY_WIDTH, DISPLAY_HEIGHT, HUB75_OE1);

    delay(MOVE_DELAY);
}
