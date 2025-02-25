#include <Arduino.h>

#include <Keypad.h>

#include <U8g2lib.h>

#include <Wire.h>


#define LED 21


const uint8_t ROWS = 4;
const uint8_t COLS = 4;
char keys[ROWS][COLS] = {
  { '1', '2', '3', 'A' },
  { '4', '5', '6', 'B' },
  { '7', '8', '9', 'C' },
  { '*', '0', '#', 'D' }
};

uint8_t colPins[COLS] = { 39, 40, 41, 42 }; // Pins connected to C1, C2, C3, C4
uint8_t rowPins[ROWS] = { 35, 36, 37, 38 }; // Pins connected to R1, R2, R3, R4

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);


class OLED {
  private:
      // U8g2 object for SSD1306 display via I2C
      U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;
      // SDA and SCL pins
      int sdaPin;
      int sclPin;
  
  public:
      // Constructor to initialize SDA and SCL pins
      OLED(int sda, int scl) : u8g2(U8G2_R0, U8X8_PIN_NONE, scl, sda), sdaPin(sda), sclPin(scl) {}
  
      // Initialize the display and I2C
      void begin() {
          Wire.begin(sdaPin, sclPin);  // Initialize I2C with specified SDA and SCL
          u8g2.begin();                // Initialize the display
      }
  
      // Clear the display buffer
      void clear() {
          u8g2.clearBuffer();
      }
  
      // Set the font for drawing text
      void setFont(const uint8_t* font) {
          u8g2.setFont(font);
      }
  
      // Draw a string at the specified (x, y) position
      void drawString(int x, int y, const char* str) {
          u8g2.drawStr(x, y, str);
      }
  
      // Send the buffer to the display
      void update() {
          u8g2.sendBuffer();
      }

  };

//------------------------------------------//

#define MAX_QR_LENGTH 20

//-------------------------------------//
class ParkingSpace {
  public:
      ParkingSpace(int pin, int number) : pin(pin), number(number), occupied(false) {
          pinMode(pin, INPUT_PULLUP); // Initialize limit switch pin
      }
  
      void check() {
          bool currentState = digitalRead(pin) == LOW; // LOW when pressed
          if (currentState != occupied) {
              occupied = currentState;
              Serial.print("Parking Space ");
              Serial.print(number);
              Serial.print(": ");
              Serial.println(occupied ? "Occupied" : "Empty");
              // Update LCD here or notify an LCDManager
          }
      }
  
      bool isOccupied() const {
          return occupied;
      }
  
      void setQRData(const String& data) {
          qrData = data;
      }
  
      String getQRData() const {
          return qrData;
      }
  
      int getNumber() const {
          return number;
      }
  
  private:
      int pin;
      int number;
      bool occupied;
      String qrData;
  };

//------------------------------------------------//


class QRScanner {
  public:
      //constructor
      QRScanner(OLED& display) : oled(display) {}

      // Update method to check for incoming serial data
      String update();
      
      // Method to handle the received command (can be overridden or modified)
      virtual void handleOLED(String command);

  
  private:
      String commandBuffer;  // Buffer to accumulate incoming data
      OLED& oled;  // Reference to the OLED instance
  };
  
  // Update method to read serial data
  String QRScanner::update() {
    static unsigned long lastReadTime = 0;
    const unsigned long timeout = 500;  // 100ms timeout

    while (Serial.available()) {
      char c = Serial.read();
      commandBuffer += c;
      lastReadTime = millis();
    }

    if (commandBuffer.length() > 0 && millis() - lastReadTime >= timeout) { //if nagtime out
      handleOLED(commandBuffer);
      commandBuffer = "";
    }

    return commandBuffer;
  }
  
  // Default command handler (can be overridden in derived classes)
  void QRScanner::handleOLED(String command) {
      // Implement default behavior or leave empty for subclasses to override
      //dito ung ishoshow sa OLED
      Serial.println("Received command: " + command);
      oled.clear();
      oled.setFont(u8g2_font_t0_11_mf);
      oled.drawString(0, 24, command.c_str());
      oled.update();
    
  }

ParkingSpace spaces[] = {
  ParkingSpace(18, 1),
  ParkingSpace(17, 2),
  ParkingSpace(16, 3),
  ParkingSpace(15, 4),
  ParkingSpace(7, 5),
  ParkingSpace(6, 6),
  ParkingSpace(5, 7),
  ParkingSpace(4, 8),
};

const String password = "1234"; // change your password here
String input_password;

OLED oled(8, 9);  // Assuming SDA_PIN = 8, SCL_PIN = 9
QRScanner qrScanner(oled);

void setup() {
  pinMode(LED, OUTPUT);
  Serial.begin(115200);
  Serial.println("Data received from UART chip:");
  oled.begin();  // Initialize the OLED display with .wire and u8g2 begin


}


void loop() {
  // digitalWrite(LED, HIGH);
  // delay(500);
  // digitalWrite(LED, LOW);
  // delay(500);

  // while (Serial.available()) {
  //   Serial.print((char)Serial.read());
  // }

  String qrCode = qrScanner.update();
  
  
  

  char key = keypad.getKey();

  if (key){
    Serial.println(key);

    if(key == '*') {
      input_password = ""; // clear input password
    } else if(key == '#') {
      if(password == input_password) {
        Serial.println("password is correct, here num: ");
        // DO YOUR WORK HERE
        int numbah = spaces[1].getNumber();
        Serial.println(String(numbah));
        
      } else {
        Serial.println("password is incorrect, try again");
      }

      input_password = ""; // clear input password
    } else {
      input_password += key; // append new character to input password string
    }
  }

  for (auto& space : spaces) { //needs improvement, dont check all the time
    space.check();
  }
}
