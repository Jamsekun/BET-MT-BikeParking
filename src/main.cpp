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
      ParkingSpace(int pin, int number) : pin(pin), number(number), occupied(false), locked(false), qrData("") {
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
          }
      }
  
      bool isOccupied() const {
          return occupied;
      }
  
      bool isLocked() const {
          return locked;
      }
  
      String getQRData() const {
          return qrData;
      }
  
      void setQRData(const String& data) {
          if (!locked) {
              qrData = data;
              locked = true;
              Serial.println("Locked QR code " + data + " to parking space " + String(number));
          } else {
              Serial.println("Error: Parking space " + String(number) + " is already locked.");
          }
      }
  
      bool unlock(const String& qrCode) {
          if (locked && qrData == qrCode) {
              locked = false;
              qrData = "";
              Serial.println("Unlocked parking space " + String(number) + " with QR code " + qrCode);
              return true;
          } else {
              Serial.println("Error: Cannot unlock parking space " + String(number) + ". Incorrect QR code.");
              return false;
          }
      }
  
      int getNumber() const {
          return number;
      }
  
  private:
      int pin;
      int number;
      bool occupied;
      bool locked;
      String qrData;
  };

//------------------------------------------------//


class QRScanner {
  public:
      QRScanner(OLED& display) : oled(display), lastQRCode("") {}
  
      String update() {
          static unsigned long lastReadTime = 0;
          const unsigned long timeout = 500;  // 500ms timeout
  
          while (Serial.available()) {
              char c = Serial.read();
              commandBuffer += c;
              lastReadTime = millis();
          }
  
          if (commandBuffer.length() > 0 && millis() - lastReadTime >= timeout) {
              lastQRCode = commandBuffer;
              commandBuffer = "";
              Serial.println("Received QR code: " + lastQRCode);
          }
  
          return lastQRCode;
      }
  
      String getLastQRCode() const {
          return lastQRCode;
      }
  
  private:
      String commandBuffer;
      String lastQRCode;
      OLED& oled;
  };

class ScreenManager {
  private:
    enum State { MAIN_SCREEN, LOCK_SCREEN, UNLOCK_SCREEN, CONFIRMATION_SCREEN, ERROR_SCREEN };
    State currentState;
    OLED& oled;
    QRScanner& qrScanner;
    ParkingSpace* spaces;
    String inputBuffer;
    unsigned long confirmationStartTime;
    const unsigned long confirmationDuration = 2000;  // 2 seconds
  
  public:
      ScreenManager(OLED& display, QRScanner& scanner, ParkingSpace* spacesArray)
          : oled(display), qrScanner(scanner), spaces(spacesArray), currentState(MAIN_SCREEN), inputBuffer("") {}
  
      void update() {
          char key = keypad.getKey();
          if (key) {
              handleKeypadInput(key);
          }
          handleStateTransitions();
          updateDisplay();
      }
  
  private:
  void handleKeypadInput(char key) {
    switch (currentState) {
        case MAIN_SCREEN:
            if (key == 'A') {
                currentState = LOCK_SCREEN;
                inputBuffer = "";
            } else if (key == 'B') {
                currentState = UNLOCK_SCREEN;
                inputBuffer = "";
            }
            break;

        case LOCK_SCREEN:
            if (key >= '0' && key <= '9') {
                inputBuffer += key;
            } else if (key == '#') {
                int parkingNumber = inputBuffer.toInt();
                if (parkingNumber >= 1 && parkingNumber <= 8) {
                    ParkingSpace& space = spaces[parkingNumber - 1];
                    if (space.isOccupied() && !space.isLocked()) {
                        String qrCode = qrScanner.getLastQRCode();
                        if (qrCode.length() > 0) {
                            space.setQRData(qrCode);
                            currentState = CONFIRMATION_SCREEN;
                            confirmationStartTime = millis();
                        } else {
                            Serial.println("No QR code available to lock.");
                            currentState = ERROR_SCREEN;
                            confirmationStartTime = millis();
                        }
                    } else {
                        Serial.println("Cannot lock: Space is either not occupied or already locked.");
                        currentState = ERROR_SCREEN;
                        confirmationStartTime = millis();
                    }
                } else {
                    Serial.println("Invalid parking space number.");
                    currentState = ERROR_SCREEN;
                    confirmationStartTime = millis();
                }
            } else if (key == '*') {
                currentState = MAIN_SCREEN;
            }
            break;

        case UNLOCK_SCREEN:
            if (key >= '0' && key <= '9') {
                inputBuffer += key;
            } else if (key == '#') {
                int parkingNumber = inputBuffer.toInt();
                if (parkingNumber >= 1 && parkingNumber <= 8) {
                    ParkingSpace& space = spaces[parkingNumber - 1];
                    String qrCode = qrScanner.getLastQRCode();
                    if (space.isLocked()) {
                        if (space.unlock(qrCode)) {
                            currentState = CONFIRMATION_SCREEN;
                            confirmationStartTime = millis();
                        } else {
                            currentState = ERROR_SCREEN;
                            confirmationStartTime = millis();
                        }
                    } else {
                        Serial.println("Error: Parking space " + String(parkingNumber) + " is not locked.");
                        currentState = ERROR_SCREEN;
                        confirmationStartTime = millis();
                    }
                } else {
                    Serial.println("Invalid parking space number.");
                    currentState = ERROR_SCREEN;
                    confirmationStartTime = millis();
                }
            } else if (key == '*') {
                currentState = MAIN_SCREEN;
            }
            break;
        }
      }
  
      void handleStateTransitions() {
          if (currentState == CONFIRMATION_SCREEN || currentState == ERROR_SCREEN) {
              if (millis() - confirmationStartTime >= confirmationDuration) {
                  currentState = MAIN_SCREEN;
              }
          }
      }
  
      void updateDisplay() {
        oled.clear();
        oled.setFont(u8g2_font_t0_11_mf);
        switch (currentState) {
            case MAIN_SCREEN:
                oled.drawString(0, 12, qrScanner.getLastQRCode().c_str());
                oled.drawString(0, 24, "[A] LOCK");
                oled.drawString(0, 36, "[B] UNLOCK");
                break;
    
            case LOCK_SCREEN:
                oled.drawString(0, 12, "Locking bike");
                oled.drawString(0, 24, "Enter parking #: ");
                oled.drawString(0, 36, inputBuffer.c_str());
                break;
    
            case UNLOCK_SCREEN:
                oled.drawString(0, 12, "Unlocking bike");
                oled.drawString(0, 24, "Enter parking #: ");
                oled.drawString(0, 36, inputBuffer.c_str());
                break;
    
            case CONFIRMATION_SCREEN:
                oled.drawString(0, 12, "Operation successful!");
                oled.drawString(0, 24, (currentState == UNLOCK_SCREEN) ? "Unlocked space" : "Locked to space");
                oled.drawString(0, 36, inputBuffer.c_str());
                break;
    
            case ERROR_SCREEN:
                oled.drawString(0, 24, "Error: Invalid");
                oled.drawString(0, 36, "or locked space");
                break;
        }
        oled.update();
    }
  };

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
ScreenManager screenManager(oled, qrScanner, spaces);

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
  screenManager.update();

  for (auto& space : spaces) { //needs improvement, dont check all the time
    space.check();
  }



  // char key = keypad.getKey();

  // if (key){
  //   Serial.println(key);

  //   if(key == '*') {
  //     input_password = ""; // clear input password
  //   } else if(key == '#') {
  //     if(password == input_password) {
  //       Serial.println("password is correct, here num: ");
  //       // DO YOUR WORK HERE
  //       int numbah = spaces[1].getNumber();
  //       Serial.println(String(numbah));
        
  //     } else {
  //       Serial.println("password is incorrect, try again");
  //     }

  //     input_password = ""; // clear input password
  //   } else {
  //     input_password += key; // append new character to input password string
  //   }
  // }
}
