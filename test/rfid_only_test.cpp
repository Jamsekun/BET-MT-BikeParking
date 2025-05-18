#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>

// Pin definitions for your setup
#define RST_PIN 16  // GPIO 16
#define SS_PIN 17   // GPIO 17

// Create RFID instance with custom SPI pins
MFRC522 rfid(SS_PIN, RST_PIN); 

void setup() {
  Serial.begin(9600);
  SPI.begin(12, 13, 11, SS_PIN); // SCK=12, MISO=13, MOSI=11
  rfid.PCD_Init();
  Serial.println("RFID Reader Ready");
}

void loop() {
  // Reset the loop if no new card present
  if (!rfid.PICC_IsNewCardPresent()) {
    return;
  }

  // Verify card reading
  if (!rfid.PICC_ReadCardSerial()) {
    return;
  }

  // Print UID to serial monitor
  Serial.print("Card UID:");
  for (byte i = 0; i < rfid.uid.size; i++) {
    Serial.print(rfid.uid.uidByte[i] < 0x10 ? " 0" : " ");
    Serial.print(rfid.uid.uidByte[i], HEX);
  }
  Serial.println();

  // Stop encryption and put RFID to idle
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
  
  delay(1000); // Prevent rapid repeated reads
}