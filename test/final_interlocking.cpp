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
const String UID_SPACE1 = "2607933D";
const String UID_SPACE2 = "F6516F3D";
const String UID_MASTER = "8669793D";

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

        // Initialize pins
        pinMode(A_IR_pin, INPUT);
        pinMode(B_IR_pin, INPUT);
        pinMode(A_LA_forward_pin, OUTPUT);
        pinMode(B_LA_forward_pin, OUTPUT);
        pinMode(A_LA_reverse_pin, OUTPUT);
        pinMode(B_LA_reverse_pin, OUTPUT);
        stopActuators(); // Ensure all actuators are off at startup
        Serial.print("ParkingSpace ");
        Serial.print(spaceNumber);
        Serial.println(" initialized");
    }

    void checkIR() {
        bool a_ir = digitalRead(A_IR_pin) == LOW; // LOW when bike is present
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
        stopActuators(); // Ensure all pins are LOW before changing state
        delay(10); // Brief delay for interlocking safety
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
        stopActuators(); // Ensure all pins are LOW before changing state
        delay(10); // Brief delay for interlocking safety
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
};

// Parking space instances
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
        case STOLEN: {
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
        }
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
        Serial.print("RFID: Card detected, UID=");
        Serial.println(uid);

        // Beep buzzer for card detection
        buzzerState = CARD_DETECTED;
        buzzerStartTime = millis();
        buzzerActive = true;
        Serial.println("Buzzer: Card detected beep started");

        updateDisplay("UID: " + uid);
        bool actionTaken = false;

        // Check for unlocking
        if (space1.locked && space1.associatedUID == uid) {
            space1.startUnlocking();
            actionTaken = true;
            Serial.println("Space 1: Unlocking initiated");
        } else if (space2.locked && space2.associatedUID == uid) {
            space2.startUnlocking();
            actionTaken = true;
            Serial.println("Space 2: Unlocking initiated");
        }

        // Check for locking if no unlock action
        if (!actionTaken) {
            if (space1.occupied && !space1.locked && !space1.lockingInProgress) {
                space1.associateUID(uid);
                space1.startLocking();
                actionTaken = true;
                Serial.println("Space 1: Locking initiated");
            } else if (space2.occupied && !space2.locked && !space2.lockingInProgress) {
                space2.associateUID(uid);
                space2.startLocking();
                actionTaken = true;
                Serial.println("Space 2: Locking initiated");
            } else {
                // Check IR conditions for feedback
                space1.checkIR();
                space2.checkIR();
                bool oneIRSpace1 = digitalRead(space1.A_IR_pin) == LOW || digitalRead(space1.B_IR_pin) == LOW;
                bool oneIRSpace2 = digitalRead(space2.A_IR_pin) == LOW || digitalRead(space2.B_IR_pin) == LOW;
                if ((oneIRSpace1 && !space1.locked) || (oneIRSpace2 && !space2.locked)) {
                    updateDisplay("Please readjust bike");
                    Serial.println("RFID: Please readjust bike");
                } else {
                    updateDisplay("No bike detected");
                    Serial.println("RFID: No bike detected");
                }
            }
        }
        if (!actionTaken && uid != UID_SPACE1 && uid != UID_SPACE2) {
            updateDisplay("The RFID card is unknown");
            Serial.println("RFID: Unknown card");
        }
    }
}