#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>
#include <LoRa.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>

// WiFi credentials
const char* ssid = "OPPO_A78";
const char* password = "s7dg6zmy";

byte final_x, final_y;
int16_t  x, y;
uint16_t w, h; //values

// Warning icon bitmap (8x8)
static const uint8_t PROGMEM WARNING_ICON[] = {
  0b00111100,
  0b01111110,
  0b11111111,
  0b11111111,
  0b11111111,
  0b01111110,
  0b00111100,
  0b00000000
};

// Static IP configuration
IPAddress local_IP(192, 168, 1, 100);
IPAddress gateway(192, 168, 248, 164);
IPAddress subnet(255, 255, 255, 0);

// Pin definitions
#define RST_PIN 16     // RFID reset pin
#define SS_PIN  17     // RFID slave chip select pin
#define LORA_CS 10     // LoRa chip select
#define LORA_RST 14    // LoRa reset
#define LORA_DIO0 15   // LoRa DIO0
#define BUZZER_PIN 40  // Buzzer pin
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_SDA 8
#define OLED_SCL 9
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
void processUID(String uid);

// Initialize components
MFRC522 rfid(SS_PIN, RST_PIN);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
WiFiServer server(80);

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

    // Configure static IP
    // if (!WiFi.config(local_IP, gateway, subnet)) {
    //     Serial.println("Static IP configuration failed!");
    // }

    // Connect to WiFi
    WiFi.begin(ssid, password);
    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();
    Serial.print("Connected, IP address: ");
    Serial.println(WiFi.localIP());
    server.begin();

    SPI.begin();
    delay(500);
    Wire.begin(OLED_SDA, OLED_SCL);
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println("OLED init failed!");
        while (1);
    }
    delay(500);

    Serial.println("OLED initialized");
    display.clearDisplay();
    display.setCursor(0,0);
    display.println("System Initializing");
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    delay(500);

    rfid.PCD_Init();

    // Check firmware version
    byte version = rfid.PCD_ReadRegister(rfid.VersionReg);
    Serial.print("MFRC522 Version: 0x");
    Serial.println(version, HEX);
    if (version == 0x00 || version == 0xFF) {
        Serial.println("ERROR: No RC522 detected or communication issue!");
        byte testReg = rfid.PCD_ReadRegister(rfid.CommandReg);
        Serial.print("Command Register: 0x");
        Serial.println(testReg, HEX);
    }
    delay(1000);
    Serial.println("RFID initialized");
    delay(3000);

    LoRa.setPins(LORA_CS, LORA_RST, LORA_DIO0);
    if (!LoRa.begin(433E6)) {
        Serial.println("LoRa init failed!");
        while (1);
    }

    delay(500);
    LoRa.setSyncWord(0xF1);
    Serial.println("LoRa initialized");

    pinMode(BUZZER_PIN, OUTPUT);
    tone(BUZZER_PIN, 550, 1000); // Test beep on startup
    delay(1000);
    noTone(BUZZER_PIN);
    Serial.println("Buzzer initialized");
    updateDisplay("System Ready");
    Serial.println("System setup complete");

    space1.sendStatus();
    space2.sendStatus();
    Serial.println("Initial statuses sent via LoRa");
}

// Helper function for centered text
void drawCenteredText(String text, int y) {
  int16_t x, y1;
  uint16_t w, h;
  display.getTextBounds(text, 0, 0, &x, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w)/2, y);
  display.print(text);
}

// Stolen status alert with icon and background
void drawStatusAlert(String text, int y) {
  // Draw warning icon
  display.drawBitmap(10, y, WARNING_ICON, 8, 8, SSD1306_WHITE);
  
  // Draw inverted text background
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  display.fillRect(22, y, w + 4, h + 2, SSD1306_WHITE);
  
  // Draw alert text
  display.setTextColor(SSD1306_BLACK);
  display.setCursor(24, y + 1);
  display.print(text);
  display.setTextColor(SSD1306_WHITE);
}

//I want my stolen bike to display some design, improve the design of my updateDisplay OLED handler, i use the Adafruit_SSD1306 OLED
void updateDisplay(String message) {
  display.clearDisplay();
  
  // Draw header
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  drawCenteredText("Bike Tracker", 0);

  // Draw available spaces with large number
  int availableSpaces = (!space1.locked) + (!space2.locked);
  String availableStr = String(availableSpaces);
  display.setTextSize(2);
  drawCenteredText(availableStr, 16);
  display.setTextSize(1);

  // Draw status indicators
  int statusY = 32;
  if (space1.status == "stolen") {
    drawStatusAlert("SPACE 1 STOLEN!", statusY);
    statusY += 16;
  }
  if (space2.status == "stolen") {
    drawStatusAlert("SPACE 2 STOLEN!", statusY);
    statusY += 16;
  }

  // Draw footer message
  drawCenteredText(message, SCREEN_HEIGHT - 10);

  display.display();
  
  // Serial logging
  Serial.print("Available: ");
  Serial.print(availableSpaces);
  Serial.print(" | Stolen: ");
  if (space1.status == "stolen") Serial.print("1 ");
  if (space2.status == "stolen") Serial.print("2 ");
  Serial.print(" | Message: ");
  Serial.println(message);
}

void manageBuzzer() {
    unsigned long currentTime = millis();
    if (!buzzerActive) return;

    switch (buzzerState) {
        case CARD_DETECTED:
            if (currentTime - buzzerStartTime < 100) {
                digitalWrite(BUZZER_PIN, HIGH);
                tone(BUZZER_PIN, 550, 1000); // Test beep on startup
                delay(1000);
            } else {
                digitalWrite(BUZZER_PIN, LOW);
                buzzerActive = false;
                Serial.println("Buzzer: Card detected beep finished");
            }
            break;
        case STOLEN: { // Add opening brace to scope this case
            if (buzzerCount >= 5) {
                digitalWrite(BUZZER_PIN, LOW);
                buzzerActive = false;
                Serial.println("Buzzer: Stolen alert finished");
                break;
            }
            unsigned long elapsed = currentTime - buzzerStartTime; // Now scoped
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
        } // Add closing brace to end the scoped case
        case OFF:
            buzzerActive = false;
            break;
    }
}

void processUID(String uid) {
    uid.toUpperCase();
    Serial.print("Processing UID=");
    Serial.println(uid);

    buzzerState = CARD_DETECTED;
    buzzerStartTime = millis();
    buzzerActive = true;
    Serial.println("Buzzer: Card detected beep started");

    updateDisplay("UID: " + uid);

    // Validate UID
    if (uid != UID_SPACE1 && uid != UID_SPACE2 && uid != UID_RESERVE1 && uid != UID_RESERVE2 && uid != UID_RESET) {
        updateDisplay("The RFID card is unknown");
        Serial.println("UID: Unknown card");
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
                Serial.println("UID: Please readjust bike");
            } else if (!space1.occupied && !space2.occupied) {
                updateDisplay("No bike detected");
                Serial.println("UID: No bike detected");
            }
        }
    }
}

void handleWiFiClient() {
    WiFiClient client = server.available();
    if (!client) return;

    String request = client.readStringUntil('\r');
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/html");
    client.println("Connection: close");
    client.println();

    // Space 1: Control both actuators A and B
    if (request.indexOf("/space1/forward") != -1) {
        space1.stopActuators();
        delay(10);
        digitalWrite(SPACE1_A_LA_FORWARD, HIGH);
        digitalWrite(SPACE1_B_LA_FORWARD, HIGH);
        digitalWrite(SPACE1_A_LA_REVERSE, LOW);
        digitalWrite(SPACE1_B_LA_REVERSE, LOW);
        Serial.println("WiFi: Space 1 Forward");
        updateDisplay("Space 1 Forward");
    } else if (request.indexOf("/space1/reverse") != -1) {
        space1.stopActuators();
        delay(10);
        digitalWrite(SPACE1_A_LA_REVERSE, HIGH);
        digitalWrite(SPACE1_B_LA_REVERSE, HIGH);
        digitalWrite(SPACE1_A_LA_FORWARD, LOW);
        digitalWrite(SPACE1_B_LA_FORWARD, LOW);
        Serial.println("WiFi: Space 1 Reverse");
        updateDisplay("Space 1 Reverse");
    } else if (request.indexOf("/space1/off") != -1) {
        space1.stopActuators();
        Serial.println("WiFi: Space 1 Off");
        updateDisplay("Space 1 Off");
    }

    // Space 2: Control both actuators A and B
    if (request.indexOf("/space2/forward") != -1) {
        space2.stopActuators();
        delay(10);
        digitalWrite(SPACE2_A_LA_FORWARD, HIGH);
        digitalWrite(SPACE2_B_LA_FORWARD, HIGH);
        digitalWrite(SPACE2_A_LA_REVERSE, LOW);
        digitalWrite(SPACE2_B_LA_REVERSE, LOW);
        Serial.println("WiFi: Space 2 Forward");
        updateDisplay("Space 2 Forward");
    } else if (request.indexOf("/space2/reverse") != -1) {
        space2.stopActuators();
        delay(10);
        digitalWrite(SPACE2_A_LA_REVERSE, HIGH);
        digitalWrite(SPACE2_B_LA_REVERSE, HIGH);
        digitalWrite(SPACE2_A_LA_FORWARD, LOW);
        digitalWrite(SPACE2_B_LA_FORWARD, LOW);
        Serial.println("WiFi: Space 2 Reverse");
        updateDisplay("Space 2 Reverse");
    } else if (request.indexOf("/space2/off") != -1) {
        space2.stopActuators();
        Serial.println("WiFi: Space 2 Off");
        updateDisplay("Space 2 Off");
    }

    // UID buttons
    if (request.indexOf("/uid/space1") != -1) {
        processUID(UID_SPACE1);
        Serial.println("WiFi: Space 1 UID button clicked");
    } else if (request.indexOf("/uid/space2") != -1) {
        processUID(UID_SPACE2);
        Serial.println("WiFi: Space 2 UID button clicked");
    } else if (request.indexOf("/uid/reserve1") != -1) {
        processUID(UID_RESERVE1);
        Serial.println("WiFi: Reserve 1 UID button clicked");
    } else if (request.indexOf("/uid/reserve2") != -1) {
        processUID(UID_RESERVE2);
        Serial.println("WiFi: Reserve 2 UID button clicked");
    } else if ( !request.indexOf("/uid/reset") != -1) {
        processUID(UID_RESET);
        Serial.println("WiFi: Reset UID button clicked");
    }

    // Serve HTML page
    client.println("<!DOCTYPE html><html><head><title>Actuator Control</title>");
    client.println("<link rel='stylesheet' href='https://cdnjs.cloudflare.com/ajax/libs/bulma/0.9.3/css/bulma.min.css'>");
    client.println("<style>body { padding: 20px; } .control-group { margin-bottom: 20px; } .button { margin: 5px; }</style>");
    client.println("</head><body><div class='container'><h1 class='title'>Actuator and RFID Control</h1>");

    // Space 1 controls
    client.println("<div class='control-group'><h2 class='subtitle'>Space 1 Control</h2>");
    client.println("<a class='button is-primary' href='/space1/forward'>Forward</a>");
    client.println("<a class='button is-warning' href='/space1/reverse'>Reverse</a>");
    client.println("<a class='button is-danger' href='/space1/off'>Off</a></div>");

    // Space 2 controls
    client.println("<div class='control-group'><h2 class='subtitle'>Space 2 Control</h2>");
    client.println("<a class='button is-primary' href='/space2/forward'>Forward</a>");
    client.println("<a class='button is-warning' href='/space2/reverse'>Reverse</a>");
    client.println("<a class='button is-danger' href='/space2/off'>Off</a></div>");

    // UID controls
    client.println("<div class='control-group'><h2 class='subtitle'>RFID Card Simulation</h2>");
    client.println("<a class='button is-info' href='/uid/space1'>Space 1 Card</a>"); // UID: 3CA0FFE2
    client.println("<a class='button is-info' href='/uid/space2'>Space 2 Card</a>"); // UID: AA2B0C7D
    client.println("<a class='button is-info' href='/uid/reserve1'>Reserve 1 Card</a>"); // UID: BBBBBBBB
    client.println("<a class='button is-info' href='/uid/reserve2'>Reserve 2 Card</a>"); // UID: CCCCCCCC
    client.println("<a class='button is-info' href='/uid/reset'>Reset Card</a></div>"); // UID: DDDDDDDD

    client.println("</div></body></html>");
    client.stop();
}

void handleSerialCommand() {
    if (Serial.available()) {
        String command = Serial.readStringUntil('\n');
        command.trim();
        if (command.startsWith("RUN ")) {
            String actuator = command.substring(4);
            if (actuator == "SPACE1_FORWARD") {
                space1.stopActuators();
                delay(10);
                digitalWrite(SPACE1_A_LA_FORWARD, HIGH);
                digitalWrite(SPACE1_B_LA_FORWARD, HIGH);
                digitalWrite(SPACE1_A_LA_REVERSE, LOW);
                digitalWrite(SPACE1_B_LA_REVERSE, LOW);
                Serial.println("Override: SPACE1 Forward");
            } else if (actuator == "SPACE1_REVERSE") {
                space1.stopActuators();
                delay(10);
                digitalWrite(SPACE1_A_LA_REVERSE, HIGH);
                digitalWrite(SPACE1_B_LA_REVERSE, HIGH);
                digitalWrite(SPACE1_A_LA_FORWARD, LOW);
                digitalWrite(SPACE1_B_LA_FORWARD, LOW);
                Serial.println("Override: SPACE1 Reverse");
            } else if (actuator == "SPACE1_OFF") {
                space1.stopActuators();
                Serial.println("Override: SPACE1 Off");
            } else if (actuator == "SPACE2_FORWARD") {
                space2.stopActuators();
                delay(10);
                digitalWrite(SPACE2_A_LA_FORWARD, HIGH);
                digitalWrite(SPACE2_B_LA_FORWARD, HIGH);
                digitalWrite(SPACE2_A_LA_REVERSE, LOW);
                digitalWrite(SPACE2_B_LA_REVERSE, LOW);
                Serial.println("Override: SPACE2 Forward");
            } else if (actuator == "SPACE2_REVERSE") {
                space2.stopActuators();
                delay(10);
                digitalWrite(SPACE2_A_LA_REVERSE, HIGH);
                digitalWrite(SPACE2_B_LA_REVERSE, HIGH);
                digitalWrite(SPACE2_A_LA_FORWARD, LOW);
                digitalWrite(SPACE2_B_LA_FORWARD, LOW);
                Serial.println("Override: SPACE2 Reverse");
            } else if (actuator == "SPACE2_OFF") {
                space2.stopActuators();
                Serial.println("Override: SPACE2 Off");
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
    handleWiFiClient();
    manageBuzzer();

    if (space1.locked && !space1.occupied && !space1.unlockingInProgress && !space1.stolenAlertSent) {
        space1.status = "stolen";
        space1.sendStatus();
        buzzerState = STOLEN;
        buzzerStartTime = millis();
        buzzerCount = 0;
        buzzerActive = true;
        space1.stolenAlertSent = true;
        Serial.println("Space 1: Bike stolen detected");
        updateDisplay("SPACE 1: STOLEN");
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
        updateDisplay("Space 2: STOLEN");
    }

    if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
        String uid = "";
        for (byte i = 0; i < rfid.uid.size; i++) {
            uid += String(rfid.uid.uidByte[i] < 0x10 ? "0" : "");
            uid += String(rfid.uid.uidByte[i], HEX);
        }
        processUID(uid);
        rfid.PICC_HaltA();
    }
}