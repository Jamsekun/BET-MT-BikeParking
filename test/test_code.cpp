
#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>

// Define SDA and SCL pins
#define SDA_PIN 8
#define SCL_PIN 9

// Initialize the U8g2 library for a specific display
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE, /* clock=*/ SCL_PIN, /* data=*/ SDA_PIN);

void setup() {
  // Initialize I2C
  Wire.begin(SDA_PIN, SCL_PIN);
  
  // Initialize the display
  u8g2.begin();
}

void loop() {
  // Clear the display buffer
  u8g2.clearBuffer();
  
  // Set the font
  u8g2.setFont(u8g2_font_ncenB14_tr);
  
  // Draw a string on the display
  u8g2.drawStr(0, 24, "Hello, World!");
  
  // Send the buffer to the display
  u8g2.sendBuffer();
  
  // Add a small delay
  delay(1000);
}
