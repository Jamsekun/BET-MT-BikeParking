#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>


int counter = 0;
String status;
int spaceNumber; 

#define LORA_CS 10
#define LORA_RST 14
#define LORA_DIO0 15

void sendStatus() {
  LoRa.beginPacket();
  LoRa.print("Space ");
  LoRa.print(spaceNumber);
  LoRa.print(": ");
  LoRa.print(status);
  LoRa.endPacket();
  Serial.print("Space ");
  Serial.print(spaceNumber);
  Serial.print(" sent LoRa status: ");
  Serial.println(status);
}

void handleSerialCommand() {
    if (Serial.available()) {
        String command = Serial.readStringUntil('\n');
        command.trim();
        Serial.print("Command: ");
        Serial.println(command);
        if (command.startsWith("SEND ")) {
            String sendermsg = command.substring(5);
            if (sendermsg == "1AVAILABLE") {
                delay(10);
                status = "available";
                spaceNumber = 1; 
                // send packet
                LoRa.beginPacket();
                LoRa.print("Space ");
                LoRa.print(spaceNumber);
                LoRa.print(": ");
                LoRa.print(status);
                LoRa.endPacket();
                Serial.print("Space ");
                Serial.print(spaceNumber);
                Serial.print(" sent LoRa status: ");
                Serial.println(status);
            }
            else if (sendermsg == "1OCCUPIED" ){
              delay(10);
              status = "occupied";
              spaceNumber = 1; 
                            LoRa.beginPacket();
  LoRa.print("Space ");
  LoRa.print(spaceNumber);
  LoRa.print(": ");
  LoRa.print(status);
  LoRa.endPacket();
  Serial.print("Space ");
  Serial.print(spaceNumber);
  Serial.print(" sent LoRa status: ");
  Serial.println(status);
            }
            else if (sendermsg == "1STOLEN" ){
              delay(10);
              status = "stolen";
              spaceNumber = 1; 
                            LoRa.beginPacket();
  LoRa.print("Space ");
  LoRa.print(spaceNumber);
  LoRa.print(": ");
  LoRa.print(status);
  LoRa.endPacket();
  Serial.print("Space ");
  Serial.print(spaceNumber);
  Serial.print(" sent LoRa status: ");
  Serial.println(status);
            }
            else if (sendermsg == "2AVAILABLE" ){
              delay(10);
              status = "occupied";
              spaceNumber = 2; 
                            LoRa.beginPacket();
  LoRa.print("Space ");
  LoRa.print(spaceNumber);
  LoRa.print(": ");
  LoRa.print(status);
  LoRa.endPacket();
  Serial.print("Space ");
  Serial.print(spaceNumber);
  Serial.print(" sent LoRa status: ");
  Serial.println(status);
            }
            else if (sendermsg == "2OCCUPIED" ){
              delay(10);
              status = "occupied";
              spaceNumber = 2;
                // send packet
                  LoRa.beginPacket();
  LoRa.print("Space ");
  LoRa.print(spaceNumber);
  LoRa.print(": ");
  LoRa.print(status);
  LoRa.endPacket();
  Serial.print("Space ");
  Serial.print(spaceNumber);
  Serial.print(" sent LoRa status: ");
  Serial.println(status);
            }
            else if (sendermsg == "2STOLEN" ){
              delay(10);
              status = "stolen";
              spaceNumber = 2; 
            // send packet
             LoRa.beginPacket();
            LoRa.print("Space ");
            LoRa.print(spaceNumber);
            LoRa.print(": ");
            LoRa.print(status);
            LoRa.endPacket();
            Serial.print("Space ");
            Serial.print(spaceNumber);
            Serial.print(" sent LoRa status: ");
            Serial.println(status);
            }

            } else {
                Serial.println("Invalid sendermsg command");
                counter++;
                Serial.print(counter);
            }
        }
    }

void setup() {
    Serial.begin(115200);
    while (!Serial);
    Serial.println("Testing LoRa Sender.");

    LoRa.setPins(LORA_CS, LORA_RST, LORA_DIO0);
    if (!LoRa.begin(433E6)) {
        Serial.println("LoRa init failed!");
        while (1);
    }
    LoRa.setSyncWord(0xF1);
    Serial.println("LoRa initialized");
}

void loop() {

  handleSerialCommand();
  delay(1000);
}