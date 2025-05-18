#include <Arduino.h>
#include <LoRa.h>
#include <SPI.h>

// Pin definitions
#define LORA_CS   10  // LoRa chip select
#define LORA_RST  14  // LoRa reset
#define LORA_DIO0 15  // LoRa DIO0
#define RED_LED   40  // Red LED for machine error
#define GREEN_LED 41  // Green LED for machine working
#define BUZZER_PIN 45 // Buzzer for stolen alert

// RGB LED pins for Parking Space 1 and 2
#define RGB1_RED   1  // Space 1 RGB Red
#define RGB1_GREEN 2  // Space 1 RGB Green
#define RGB1_BLUE  3  // Space 1 RGB Blue
#define RGB2_RED   4  // Space 2 RGB Red
#define RGB2_GREEN 5  // Space 2 RGB Green
#define RGB2_BLUE  6  // Space 2 RGB Blue

class ParkingSpace {
public:
    int spaceNumber;
    String status;           // "occupied", "available", "stolen"
    int redPin, greenPin, bluePin;
    bool isBlinking;        // For stolen state blinking
    bool blinkState;        // Current state of blinking LED and buzzer
    unsigned long lastBlinkTime;
    const unsigned long blinkInterval = 500; // Blink and beep every 500ms

    ParkingSpace(int num, int rPin, int gPin, int bPin) {
        spaceNumber = num;
        redPin = rPin;
        greenPin = gPin;
        bluePin = bPin;
        status = "available";
        isBlinking = false;
        blinkState = false;
        lastBlinkTime = 0;

        // Initialize pins
        pinMode(redPin, OUTPUT);
        pinMode(greenPin, OUTPUT);
        pinMode(bluePin, OUTPUT);
        setLEDColor(); // Set initial LED state
        Serial.print("ParkingSpace ");
        Serial.print(spaceNumber);
        Serial.println(" initialized");
    }

    void updateStatus(String newStatus) {
        if (status != newStatus) {
            status = newStatus;
            Serial.print("Space ");
            Serial.print(spaceNumber);
            Serial.print(" status updated to: ");
            Serial.println(status);
            setLEDColor();
        }
    }

    void setLEDColor() {
        isBlinking = (status == "stolen");
        if (!isBlinking) {
            // Turn off all colors
            digitalWrite(redPin, LOW);
            digitalWrite(greenPin, LOW);
            digitalWrite(bluePin, LOW);

            // Set color based on status
            if (status == "occupied") {
                digitalWrite(redPin, HIGH); // Red
            } else if (status == "available") {
                digitalWrite(greenPin, HIGH); // Green
            }
        }
        // Blinking for stolen is handled in update()
    }

    void update(bool &buzzerActive) {
        if (isBlinking && status == "stolen") {
            unsigned long currentTime = millis();
            if (currentTime - lastBlinkTime >= blinkInterval) {
                blinkState = !blinkState;
                digitalWrite(redPin, blinkState ? HIGH : LOW);    // Orange: Red + Green
                digitalWrite(greenPin, blinkState ? HIGH : LOW);
                digitalWrite(bluePin, LOW);
                lastBlinkTime = currentTime;
                buzzerActive = true; // Signal buzzer to activate
                Serial.print("Space ");
                Serial.print(spaceNumber);
                Serial.print(" blink state: ");
                Serial.println(blinkState ? "ON" : "OFF");
            }
        } else {
            buzzerActive = false; // No buzzer if not stolen
        }
    }
};

// Parking space instances
ParkingSpace space1(1, RGB1_RED, RGB1_GREEN, RGB1_BLUE);
ParkingSpace space2(2, RGB2_RED, RGB2_GREEN, RGB2_BLUE);

void setup() {
    Serial.begin(115200);
    Serial.println("LoRa Receiver starting...");

    // Initialize status LEDs and buzzer
    pinMode(RED_LED, OUTPUT);
    pinMode(GREEN_LED, OUTPUT);
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(RED_LED, LOW);
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(BUZZER_PIN, LOW);

    // Initialize LoRa
    LoRa.setPins(LORA_CS, LORA_RST, LORA_DIO0);
    if (!LoRa.begin(433E6)) {
        Serial.println("LoRa initialization failed!");
        digitalWrite(RED_LED, HIGH); // Indicate error
        while (1);
    }
    LoRa.setSyncWord(0xF1);
    Serial.println("LoRa initialized");
    digitalWrite(GREEN_LED, HIGH); // Indicate system is working
    Serial.println("System setup complete");
}

void loop() {
    // Track buzzer state
    bool buzzerActive = false;

    // Update LED blinking and buzzer for stolen state
    space1.update(buzzerActive);
    space2.update(buzzerActive);

    // Control buzzer
    static bool lastBuzzerState = false;
    if (buzzerActive) {
        if (space1.isBlinking || space2.isBlinking) {
            bool currentBlinkState = (space1.isBlinking ? space1.blinkState : space2.blinkState);
            digitalWrite(BUZZER_PIN, currentBlinkState ? HIGH : LOW);
            if (currentBlinkState != lastBuzzerState) {
                Serial.print("Buzzer: Stolen alert ");
                Serial.println(currentBlinkState ? "ON" : "OFF");
                lastBuzzerState = currentBlinkState;
            }
        }
    } else {
        digitalWrite(BUZZER_PIN, LOW);
        if (lastBuzzerState) {
            Serial.println("Buzzer: Stolen alert OFF");
            lastBuzzerState = false;
        }
    }

    // Check for incoming LoRa packets
    int packetSize = LoRa.parsePacket();
    if (packetSize) {
        String message = "";
        while (LoRa.available()) {
            message += (char)LoRa.read();
        }
        Serial.print("Received LoRa message: ");
        Serial.println(message);

        // Parse message (expected format: "Space X: status")
        if (message.startsWith("Space ")) {
            int spaceNum = message.substring(6, 7).toInt();
            String status = message.substring(9); // Status after ": "
            status.trim(); // Remove any trailing whitespace

            // Update the appropriate parking space
            if (spaceNum == 1) {
                space1.updateStatus(status);
            } else if (spaceNum == 2) {
                space2.updateStatus(status);
            } else {
                Serial.println("Invalid space number in message");
            }
        } else {
            Serial.println("Invalid message format");
        }
    }
}