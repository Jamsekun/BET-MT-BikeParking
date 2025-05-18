#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>
#include <LoRa.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>

// Pin definitions
#define RST_PIN 16     // RFID reset pin
#define SS_PIN  17     // RFID slave chip select pin
#define LORA_CS 10     // LoRa chip select
#define LORA_RST 14    // LoRa reset
#define LORA_DIO0 15   // LoRa DIO0
#define BUZZER_PIN 40  // Buzzer pin
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

// Actuator pins
#define SPACE1_A_LA_FORWARD 39  // Parking Space 1, Actuator A forward
#define SPACE1_B_LA_FORWARD 38  // Parking Space 1, Actuator B forward
#define SPACE1_A_LA_REVERSE 48  // Parking Space 1, Actuator A reverse
#define SPACE1_B_LA_REVERSE 47  // Parking Space 1, Actuator B reverse
#define SPACE2_A_LA_FORWARD 37  // Parking Space 2, Actuator A forward
#define SPACE2_B_LA_FORWARD 36  // Parking Space 2, Actuator B forward
#define SPACE2_A_LA_REVERSE 21  // Parking Space 2, Actuator A reverse
#define SPACE2_B_LA_REVERSE 20  // Parking Space 2, Actuator B reverse

// RFID UIDs
const String UID_SPACE1 = "3CA0FFE2";   // Parking Space #1
const String UID_SPACE2 = "AA2B0C7D";   // Parking Space #2
const String UID_RESERVE1 = "BBBBBBBB"; // Reserve Card #1
const String UID_RESERVE2 = "CCCCCCCC"; // Reserve Card #2
const String UID_RESET = "DDDDDDDD";    // Reset Card

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
    int A_LA_forward_pin;
    int B_LA_forward_pin;
    int A_LA_reverse_pin;
    int B_LA_reverse_pin;
    bool lockingInProgress;
    bool unlockingInProgress;
    unsigned long startTime;
    bool stolenAlertSent; // To prevent repeated alerts

    ParkingSpace(int num, int a_ir, int b_ir, int a_la_fwd, int b_la_fwd, int a_la_rev, int b_la_rev) {
        spaceNumber = num;
        A_IR_pin = a_ir;
        B_IR_pin = b_ir;
        A_LA_forward_pin = a_la_fwd;
        B_LA_forward_pin = b_la_fwd;
        A_LA_reverse_pin = a_la_rev;
        B_LA_reverse_pin = b_la_rev;
        occupied = false;
        locked = false;
        associatedUID = "";
        lockingInProgress = false;
        unlockingInProgress = false;
        stolenAlertSent = false;
        status = "available";

        pinMode(A_IR_pin, INPUT);
        pinMode(B_IR_pin, INPUT);
        pinMode(A_LA_forward_pin, OUTPUT);
        pinMode(B_LA_forward_pin, OUTPUT);
        pinMode(A_LA_reverse_pin, OUTPUT);
        pinMode(B_LA_reverse_pin, OUTPUT);
        stopActuators();
        Serial.print("ParkingSpace ");
        Serial.print(spaceNumber);
        Serial.println(" initialized");
    }

    void checkIR() {
        bool a_ir = digitalRead(A_IR_pin) == LOW;
        bool b_ir = digitalRead(B_IR_pin) == LOW;
        bool prevOccupied = occupied;
        occupied = a_ir && b_ir;
        if (occupied != prevOccupied) {
            Serial.print("Space ");
            Serial.print(spaceNumber);
            Serial.print(" IR status: A_IR=");
            Serial.print(a_ir ? "ON" : "OFF");
            Serial.print(", B_IR=");
            Serial.print(b_ir ? "ON" : "OFF");
            Serial.print(", Occupied=");
            Serial.println(occupied ? "True" : "False");
        }
    }

    void startLocking() {
        stopActuators();
        delay(10);
        digitalWrite(A_LA_forward_pin, HIGH);
        digitalWrite(B_LA_forward_pin, HIGH);
        digitalWrite(A_LA_reverse_pin, LOW);
        digitalWrite(B_LA_reverse_pin, LOW);
        lockingInProgress = true;
        startTime = millis();
        Serial.print("Space ");
        Serial.print(spaceNumber);
        Serial.println(" starting locking (forward)");
        updateDisplay("Locking Bike Please wait");
    }

    void startUnlocking() {
        stopActuators();
        delay(10);
        digitalWrite(A_LA_reverse_pin, HIGH);
        digitalWrite(B_LA_reverse_pin, HIGH);
        digitalWrite(A_LA_forward_pin, LOW);
        digitalWrite(B_LA_forward_pin, LOW);
        unlockingInProgress = true;
        startTime = millis();
        Serial.print("Space ");
        Serial.print(spaceNumber);
        Serial.println(" starting unlocking (reverse)");
        updateDisplay("Unlocking Bike Please wait");
    }

    void stopActuators() {
        digitalWrite(A_LA_forward_pin, LOW);
        digitalWrite(B_LA_forward_pin, LOW);
        digitalWrite(A_LA_reverse_pin, LOW);
        digitalWrite(B_LA_reverse_pin, LOW);
        Serial.print("Space ");
        Serial.print(spaceNumber);
        Serial.println(" actuators stopped");
    }

    void update() {
        if (lockingInProgress && millis() - startTime >= 8000) {
            stopActuators();
            locked = true;
            lockingInProgress = false;
            status = occupied ? "occupied" : "stolen";
            Serial.print("Space ");
            Serial.print(spaceNumber);
            Serial.print(" locked, Status=");
            Serial.println(status);
            sendStatus();
            updateDisplay("Sending Info");
        }
        if (unlockingInProgress && millis() - startTime >= 8000) {
            stopActuators();
            locked = false;
            unlockingInProgress = false;
            associatedUID = "";
            status = "available";
            Serial.print("Space ");
            Serial.print(spaceNumber);
            Serial.print(" unlocked, Status=");
            Serial.println(status);
            sendStatus();
            updateDisplay("Sending Info");
        }
    }

    void associateUID(String uid) {
        associatedUID = uid;
        Serial.print("Space ");
        Serial.print(spaceNumber);
        Serial.print(" associated with UID: ");
        Serial.println(uid);
    }

    void sendStatus(String overrideStatus = "") {
        String sendStatus = (overrideStatus != "") ? overrideStatus : status;
        LoRa.beginPacket();
        LoRa.print("Space ");
        LoRa.print(spaceNumber);
        LoRa.print(": ");
        LoRa.print(sendStatus);
        LoRa.endPacket();
        Serial.print("Space ");
        Serial.print(spaceNumber);
        Serial.print(" sent LoRa status: ");
        Serial.println(sendStatus);
    }
};

ParkingSpace space1(1, 1, 2, SPACE1_A_LA_FORWARD, SPACE1_B_LA_FORWARD, SPACE1_A_LA_REVERSE, SPACE1_B_LA_REVERSE);
ParkingSpace space2(2, 42, 41, SPACE2_A_LA_FORWARD, SPACE2_B_LA_FORWARD, SPACE2_A_LA_REVERSE, SPACE2_B_LA_REVERSE);

// Buzzer control
unsigned long buzzerStartTime = 0;
int buzzerCount = 0;
bool buzzerActive = false;
enum BuzzerState { OFF, CARD_DETECTED, STOLEN };
BuzzerState buzzerState = OFF;

void setup() {
    Serial.begin(115200);
    Serial.println("System starting...");
    SPI.begin();
    rfid.PCD_Init();
    Serial.println("RFID initialized");
    LoRa.setPins(LORA_CS, LORA_RST, LORA_DIO0);
    if (!LoRa.begin(433E6)) {
        Serial.println("LoRa init failed!");
        while (1);
    }
    LoRa.setSyncWord(0xF1);
    Serial.println("LoRa initialized");
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println("OLED init failed!");
        while (1);
    }
    Serial.println("OLED initialized");
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);
    Serial.println("Buzzer initialized");
    updateDisplay("System Ready");
    Serial.println("System setup complete");

    space1.sendStatus();
    space2.sendStatus();
    Serial.println("Initial statuses sent via LoRa");
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
    Serial.print("OLED Display: ");
    Serial.print("Available Spaces: ");
    Serial.print(availableSpaces);
    Serial.print(", Message: ");
    Serial.println(message);
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
                Serial.println("Buzzer: Card detected beep finished");
            }
            break;
        case STOLEN:
            if (buzzerCount >= 5) {
                digitalWrite(BUZZER_PIN, LOW);
                buzzerActive = false;
                Serial.println("Buzzer: Stolen alert finished");
                break;
            }
            unsigned long elapsed = currentTime - buzzerStartTime;
            if (elapsed % 2000 < 1000) {
                digitalWrite(BUZZER_PIN, HIGH);
            } else if (elapsed % 2000 >= 1000) {
                digitalWrite(BUZZER_PIN, LOW);
                if (elapsed >= (buzzerCount + 1) * 2000) {
                    buzzerCount++;
                    Serial.print("Buzzer: Stolen beep #");
                    Serial.println(buzzerCount);
                }
            }
            break;
        case OFF:
            buzzerActive = false;
            break;
    }
}

void handleSerialCommand() {
    if (Serial.available()) {
        String command = Serial.readStringUntil('\n');
        command.trim();
        if (command.startsWith("RUN ")) {
            String actuator = command.substring(4);
            if (actuator == "SPACE1_A_LA_FORWARD") {
                space1.stopActuators();
                delay(10);
                digitalWrite(SPACE1_A_LA_FORWARD, HIGH);
                digitalWrite(SPACE1_A_LA_REVERSE, LOW);
                Serial.println("Override: SPACE1_A_LA_FORWARD ON");
            } else if (actuator == "SPACE1_A_LA_REVERSE") {
                space1.stopActuators();
                delay(10);
                digitalWrite(SPACE1_A_LA_REVERSE, HIGH);
                digitalWrite(SPACE1_A_LA_FORWARD, LOW);
                Serial.println("Override: SPACE1_A_LA_REVERSE ON");
            } else if (actuator == "SPACE1_B_LA_FORWARD") {
                space1.stopActuators();
                delay(10);
                digitalWrite(SPACE1_B_LA_FORWARD, HIGH);
                digitalWrite(SPACE1_B_LA_REVERSE, LOW);
                Serial.println("Override: SPACE1_B_LA_FORWARD ON");
            } else if (actuator == "SPACE1_B_LA_REVERSE") {
                space1.stopActuators();
                delay(10);
                digitalWrite(SPACE1_B_LA_REVERSE, HIGH);
                digitalWrite(SPACE1_B_LA_FORWARD, LOW);
                Serial.println("Override: SPACE1_B_LA_REVERSE ON");
            } else if (actuator == "SPACE2_A_LA_FORWARD") {
                space2.stopActuators();
                delay(10);
                digitalWrite(SPACE2_A_LA_FORWARD, HIGH);
                digitalWrite(SPACE2_A_LA_REVERSE, LOW);
                Serial.println("Override: SPACE2_A_LA_FORWARD ON");
            } else if (actuator == "SPACE2_A_LA_REVERSE") {
                space2.stopActuators();
                delay(10);
                digitalWrite(SPACE2_A_LA_REVERSE, HIGH);
                digitalWrite(SPACE2_A_LA_FORWARD, LOW);
                Serial.println("Override: SPACE2_A_LA_REVERSE ON");
            } else if (actuator == "SPACE2_B_LA_FORWARD") {
                space2.stopActuators();
                delay(10);
                digitalWrite(SPACE2_B_LA_FORWARD, HIGH);
                digitalWrite(SPACE2_B_LA_REVERSE, LOW);
                Serial.println("Override: SPACE2_B_LA_FORWARD ON");
            } else if (actuator == "SPACE2_B_LA_REVERSE") {
                space2.stopActuators();
                delay(10);
                digitalWrite(SPACE2_B_LA_REVERSE, HIGH);
                digitalWrite(SPACE2_B_LA_FORWARD, LOW);
                Serial.println("Override: SPACE2_B_LA_REVERSE ON");
            } else {
                Serial.println("Invalid actuator command");
            }
        } else if (command.startsWith("SEND ")) {
            String sendermsg = command.substring(5);
            if (sendermsg == "1AVAILABLE") {
                space1.sendStatus("available");
            } else if (sendermsg == "1OCCUPIED") {
                space1.sendStatus("occupied");
            } else if (sendermsg == "1STOLEN") {
                space1.sendStatus("stolen");
            } else if (sendermsg == "2AVAILABLE") {
                space2.sendStatus("available");
            } else if (sendermsg == "2OCCUPIED") {
                space2.sendStatus("occupied");
            } else if (sendermsg == "2STOLEN") {
                space2.sendStatus("stolen");
            } else {
                Serial.println("Invalid SEND command");
            }
        } else {
            Serial.println("Invalid command");
        }
    }
}

void loop() {
    space1.checkIR();
    space1.update();
    space2.checkIR();
    space2.update();

    handleSerialCommand();

    if (space1.locked && !space1.occupied && !space1.unlockingInProgress && !space1.stolenAlertSent) {
        space1.status = "stolen";
        space1.sendStatus();
        buzzerState = STOLEN;
        buzzerStartTime = millis();
        buzzerCount = 0;
        buzzerActive = true;
        space1.stolenAlertSent = true;
        Serial.println("Space 1: Bike stolen detected");
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
        Serial.println("Space 2: Bike stolen detected");
        updateDisplay("Bike stolen in Space 2");
    }

    manageBuzzer();

    if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
        String uid = "";
        for (byte i = 0; i < rfid.uid.size; i++) {
            uid += String(rfid.uid.uidByte[i] < 0x10 ? "0" : "");
            uid += String(rfid.uid.uidByte[i], HEX);
        }
        uid.toUpperCase();
        rfid.PICC_HaltA();
        Serial.print("RFID: Card detected, UID=");
        Serial.println(uid);

        buzzerState = CARD_DETECTED;
        buzzerStartTime = millis();
        buzzerActive = true;
        Serial.println("Buzzer: Card detected beep started");

        updateDisplay("UID: " + uid);

        // Validate UID
        if (uid != UID_SPACE1 && uid != UID_SPACE2 && uid != UID_RESERVE1 && uid != UID_RESERVE2 && uid != UID_RESET) {
            updateDisplay("The RFID card is unknown");
            Serial.println("RFID: Unknown card");
            return;
        }

        // Handle reset card
        if (uid == UID_RESET) {
            bool resetDone = false;
            if (space1.status == "stolen") {
                space1.status = "available";
                space1.stolenAlertSent = false;
                space1.sendStatus();
                Serial.println("Space 1 reset to available");
                resetDone = true;
            }
            if (space2.status == "stolen") {
                space2.status = "available";
                space2.stolenAlertSent = false;
                space2.sendStatus();
                Serial.println("Space 2 reset to available");
                resetDone = true;
            }
            updateDisplay(resetDone ? "Reset card used" : "No stolen spaces");
            return;
        }

        bool actionTaken = false;

        // Handle unlocking
        if (space1.locked && (space1.associatedUID == uid || uid == UID_RESERVE1 || uid == UID_RESERVE2)) {
            space1.startUnlocking();
            actionTaken = true;
            Serial.println("Space 1: Unlocking initiated");
        } else if (space2.locked && (space2.associatedUID == uid || uid == UID_RESERVE1 || uid == UID_RESERVE2)) {
            space2.startUnlocking();
            actionTaken = true;
            Serial.println("Space 2: Unlocking initiated");
        }

        // Handle locking (exclude reserve cards)
        if (!actionTaken && uid != UID_RESERVE1 && uid != UID_RESERVE2) {
            if (uid == UID_SPACE1 && space1.occupied && !space1.locked && !space1.lockingInProgress) {
                space1.associateUID(uid);
                space1.startLocking();
                actionTaken = true;
                Serial.println("Space 1: Locking initiated");
            } else if (uid == UID_SPACE2 && space2.occupied && !space2.locked && !space2.lockingInProgress) {
                space2.associateUID(uid);
                space2.startLocking();
                actionTaken = true;
                Serial.println("Space 2: Locking initiated");
            } else {
                space1.checkIR();
                space2.checkIR();
                bool oneIRSpace1 = digitalRead(space1.A_IR_pin) == LOW || digitalRead(space1.B_IR_pin) == LOW;
                bool oneIRSpace2 = digitalRead(space2.A_IR_pin) == LOW || digitalRead(space2.B_IR_pin) == LOW;
                if ((oneIRSpace1 && !space1.locked) || (oneIRSpace2 && !space2.locked)) {
                    updateDisplay("Please readjust bike");
                    Serial.println("RFID: Please readjust bike");
                } else if (!space1.occupied && !space2.occupied) {
                    updateDisplay("No bike detected");
                    Serial.println("RFID: No bike detected");
                }
            }
        }
    }
}