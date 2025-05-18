#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>

#define RST_PIN 16     // RFID Reset
#define SS_PIN 17      // RFID Slave Select
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_SDA 8
#define OLED_SCL 9

MFRC522 rfid(SS_PIN, RST_PIN);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

void setup() {
  Serial.begin(115200);
  SPI.begin();
  Wire.begin(OLED_SDA, OLED_SCL);
  rfid.PCD_Init();
  
  // Initialize OLED
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("OLED allocation failed"));
    while(1);
  }
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.println("RFID Scanner Ready");
  display.display();
  delay(2000);
  display.clearDisplay();
}

void loop() {
  // Reset display content
  display.clearDisplay();
  display.setCursor(0,0);
  
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    display.println("Scan your card");
    display.display();
    delay(200);
    return;
  }

  // Read UID
  String uid = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    uid += String(rfid.uid.uidByte[i] < 0x10 ? "0" : "");
    uid += String(rfid.uid.uidByte[i], HEX);
  }
  uid.toUpperCase();

  // Display UID
  display.clearDisplay();
  display.setCursor(0,0);
  display.println("Card UID:");
  display.setCursor(0,20);
  display.setTextSize(2);
  display.println(uid);
  display.display();
  
  Serial.print("Card UID: ");
  Serial.println(uid);

  // Halt PICC and stop crypto
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
  
  delay(3000); // Display for 3 seconds before next scan
}