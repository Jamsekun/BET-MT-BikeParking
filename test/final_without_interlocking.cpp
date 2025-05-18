#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>
#include <LoRa.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>

// Pin definitions
#define RST_PIN 16     // RFID reset pin
#define SS_PIN  17  // RFID slave chip select pin
#define LORA_CS 10    // LoRa chip select
#define LORA_RST 14   // LoRa reset
#define LORA_DIO0 15  // LoRa DIO0
#define BUZZER_PIN 40 // Buzzer pin
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

// RFID UIDs
const String UID_SPACE1 = "26 07 93 3D";
const String UID_SPACE2 = "F6516F3D";

void updateDisplay(String message);
void manageBuzzer();

// Initialize components
MFRC522 rfid(SS_PIN, RST_PIN);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

class ParkingSpace {
public:
    int spaceNumber;
    String status;        // "occupied", "available", "stolen"
    bool occupied;        // True if both IR sensors are ON
    bool locked;          // True if actuators are extended
    String associatedUID; // UID when locked
    int A_IR_pin;
    int B_IR_pin;
    int A_LA_pin;
    int B_LA_pin;
    bool lockingInProgress;
    bool unlockingInProgress;
    unsigned long startTime;
    bool stolenAlertSent; // To prevent repeated alerts

    ParkingSpace(int num, int a_ir, int b_ir, int a_la, int b_la) {
        spaceNumber = num;
        A_IR_pin = a_ir;
        B_IR_pin = b_ir;
        A_LA_pin = a_la;
        B_LA_pin = b_la;
        occupied = false;
        locked = false;
        associatedUID = "";
        lockingInProgress = false;
        unlockingInProgress = false;
        stolenAlertSent = false;
        status = "available";
        pinMode(A_IR_pin, INPUT);
        pinMode(B_IR_pin, INPUT);
        pinMode(A_LA_pin, OUTPUT);
        pinMode(B_LA_pin, OUTPUT);
        digitalWrite(A_LA_pin, LOW); // Retracted
        digitalWrite(B_LA_pin, LOW);
    }

    void checkIR() {
        bool a_ir = digitalRead(A_IR_pin) == LOW; // LOW when bike is present
        bool b_ir = digitalRead(B_IR_pin) == LOW;
        occupied = a_ir && b_ir;
    }

    void startLocking() {
        digitalWrite(A_LA_pin, HIGH); // Extend actuators
        digitalWrite(B_LA_pin, HIGH);
        lockingInProgress = true;
        startTime = millis();
        updateDisplay("Locking Bike Please wait");
    }

    void startUnlocking() {
        digitalWrite(A_LA_pin, LOW); // Retract actuators
        digitalWrite(B_LA_pin, LOW);
        unlockingInProgress = true;
        startTime = millis();
        updateDisplay("Unlocking Bike Please wait");
    }

    void update() {
        if (lockingInProgress && millis() - startTime >= 8000) {
            locked = true;
            lockingInProgress = false;
            status = occupied ? "occupied" : "stolen";
            sendStatus();
            updateDisplay("Sending Info");
        }
        if (unlockingInProgress && millis() - startTime >= 8000) {
            locked = false;
            unlockingInProgress = false;
            associatedUID = "";
            status = "available";
            sendStatus();
            updateDisplay("Sending Info");
        }
    }

    void associateUID(String uid) {
        associatedUID = uid;
    }

    void sendStatus() {
        LoRa.beginPacket();
        LoRa.print("Space ");
        LoRa.print(spaceNumber);
        LoRa.print(": ");
        LoRa.print(status);
        LoRa.endPacket();
    }
};

// Parking space instances
ParkingSpace space1(1, 1, 2, 39, 38);  // Space 1: IR on 1,2; Actuators on 4,5
ParkingSpace space2(2, 42, 41, 37, 36);  // Space 2: IR on 42,41; Actuators on 8,9

// Buzzer control
unsigned long buzzerStartTime = 0;
int buzzerCount = 0;
bool buzzerActive = false;
enum BuzzerState { OFF, CARD_DETECTED, STOLEN };
BuzzerState buzzerState = OFF;

void setup() {
    Serial.begin(115200);
    SPI.begin();
    rfid.PCD_Init();
    LoRa.setPins(LORA_CS, LORA_RST, LORA_DIO0);
    if (!LoRa.begin(433E6)) {
        Serial.println("LoRa init failed!");
        while (1);
    }
    LoRa.setSyncWord(0xF1);
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println("OLED init failed!");
        while (1);
    }
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);
    updateDisplay("System Ready");
}

void updateDisplay(String message) {
    display.clearDisplay();
    display.setCursor(0, 0);
    int availableSpaces = (!space1.locked) + (!space2.locked);
    display.print("Available Spaces: ");
    display.println(availableSpaces);
    if (space1.status == "stolen") {
        display.print("Bike stolen in Space 1");
    }
    if (space2.status == "stolen") {
        display.print("Bike stolen in Space 2");
    }
    display.setCursor(0, 20);
    display.println(message);
    display.display();
}

void manageBuzzer() {
    unsigned long currentTime = millis();
    if (!buzzerActive) return;

    switch (buzzerState) {
        case CARD_DETECTED:
            if (currentTime - buzzerStartTime < 100) {
                digitalWrite(BUZZER_PIN, HIGH);
            } else {
                digitalWrite(BUZZER_PIN, LOW);
                buzzerActive = false;
            }
            break;
        case STOLEN: { // Add curly brace to create a scope
            if (buzzerCount >= 5) {
                digitalWrite(BUZZER_PIN, LOW);
                buzzerActive = false;
                break;
            }
            unsigned long elapsed = currentTime - buzzerStartTime; // Now within a block
            if (elapsed % 2000 < 1000) {
                digitalWrite(BUZZER_PIN, HIGH);
            } else if (elapsed % 2000 >= 1000) {
                digitalWrite(BUZZER_PIN, LOW);
                if (elapsed >= (buzzerCount + 1) * 2000) {
                    buzzerCount++;
                }
            }
            break;
        } // Close the scope here
        case OFF:
            buzzerActive = false;
            break;
    }
}

void loop() {
    // Update parking spaces
    space1.checkIR();
    space1.update();
    space2.checkIR();
    space2.update();

    // Check for stolen bikes
    if (space1.locked && !space1.occupied && !space1.unlockingInProgress && !space1.stolenAlertSent) {
        space1.status = "stolen";
        space1.sendStatus();
        buzzerState = STOLEN;
        buzzerStartTime = millis();
        buzzerCount = 0;
        buzzerActive = true;
        space1.stolenAlertSent = true;
        updateDisplay("Bike stolen in Space 1");
    }
    if (space2.locked && !space2.occupied && !space2.unlockingInProgress && !space2.stolenAlertSent) {
        space2.status = "stolen";
        space2.sendStatus();
        buzzerState = STOLEN;
        buzzerStartTime = millis();
        buzzerCount = 0;
        buzzerActive = true;
        space2.stolenAlertSent = true;
        updateDisplay("Bike stolen in Space 2");
    }

    // Manage buzzer
    manageBuzzer();

    // Check RFID
    if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
        String uid = "";
        for (byte i = 0; i < rfid.uid.size; i++) {
            uid += String(rfid.uid.uidByte[i] < 0x10 ? "0" : "");
            uid += String(rfid.uid.uidByte[i], HEX);
        }
        uid.toUpperCase();
        rfid.PICC_HaltA();

        // Beep buzzer for card detection
        buzzerState = CARD_DETECTED;
        buzzerStartTime = millis();
        buzzerActive = true;

        updateDisplay("UID: " + uid);
        bool actionTaken = false;

        // Check for unlocking
        if (space1.locked && space1.associatedUID == uid) {
            space1.startUnlocking();
            actionTaken = true;
        } else if (space2.locked && space2.associatedUID == uid) {
            space2.startUnlocking();
            actionTaken = true;
        }

        // Check for locking if no unlock action
        if (!actionTaken) {
            if (space1.occupied && !space1.locked && !space1.lockingInProgress) {
                space1.associateUID(uid);
                space1.startLocking();
                actionTaken = true;
            } else if (space2.occupied && !space2.locked && !space2.lockingInProgress) {
                space2.associateUID(uid);
                space2.startLocking();
                actionTaken = true;
            } else {
                // Check IR conditions for feedback
                space1.checkIR();
                space2.checkIR();
                bool oneIRSpace1 = digitalRead(space1.A_IR_pin) == LOW || digitalRead(space1.B_IR_pin) == LOW;
                bool oneIRSpace2 = digitalRead(space2.A_IR_pin) == LOW || digitalRead(space2.B_IR_pin) == LOW;
                if ((oneIRSpace1 && !space1.locked) || (oneIRSpace2 && !space2.locked)) {
                    updateDisplay("Please readjust bike");
                } else {
                    updateDisplay("No bike detected");
                }
            }
        }
        if (!actionTaken && uid != UID_SPACE1 && uid != UID_SPACE2) {
            updateDisplay("The RFID card is unknown");
        }
    }
}


