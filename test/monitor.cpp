#include <Arduino.h>
#include <LoRa.h>
#include <SPI.h>
#include <WiFi.h>

// Pin definitions
#define LORA_CS   10  // LoRa chip select
#define LORA_RST  14  // LoRa reset
#define LORA_DIO0 15  // LoRa DIO0
#define RED_LED   40  // Red LED for machine error
#define GREEN_LED 41  // Green LED for machine working
#define BUZZER_PIN 38 // Buzzer for stolen alert

// RGB LED pins for Parking Space 1 and 2
#define RGB1_RED   1  // Space 1 RGB Red
#define RGB1_GREEN 2  // Space 1 RGB Green
#define RGB1_BLUE  3  // Space 1 RGB Blue
#define RGB2_RED   4  // Space 2 RGB Red
#define RGB2_GREEN 5  // Space 2 RGB Green
#define RGB2_BLUE  6  // Space 2 RGB Blue

// WiFi credentials
const char* ssid = "OPPO_A78";
const char* password = "s7dg6zmy";

WiFiServer server(80);

class ParkingSpace {
public:
    int spaceNumber;
    String status;           // "occupied", "available", "stolen"
    int redPin, greenPin, bluePin;
    bool isBlinking;        // For stolen state blinking
    bool blinkState;        // Current state of blinking LED
    unsigned long lastBlinkTime;
    const unsigned long blinkInterval = 500; // Blink every 500ms
    bool buzzerOn;          // Buzzer ON/OFF state
    int buzzerCycleCount;   // Track number of ON/OFF cycles
    unsigned long buzzerStartTime; // Time of last buzzer state change
    bool buzzerOverride;    // Manual buzzer activation via Serial
    const unsigned long buzzerInterval = 1000; // 1s ON, 1s OFF

    ParkingSpace(int num, int rPin, int gPin, int bPin) {
        spaceNumber = num;
        redPin = rPin;
        greenPin = gPin;
        bluePin = bPin;
        status = "available";
        isBlinking = false;
        blinkState = false;
        lastBlinkTime = 0;
        buzzerOn = false;
        buzzerCycleCount = 0;
        buzzerStartTime = 0;
        buzzerOverride = false;

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
            // Reset buzzer state on status change
            buzzerCycleCount = 0;
            buzzerOn = false;
            buzzerOverride = false;
            digitalWrite(BUZZER_PIN, LOW);
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
        // Handle LED blinking for stolen state
        if (isBlinking && status == "stolen") {
            unsigned long currentTime = millis();
            if (currentTime - lastBlinkTime >= blinkInterval) {
                blinkState = !blinkState;
                digitalWrite(redPin, LOW);    // Orange: Red + Green
                digitalWrite(bluePin, blinkState ? HIGH : LOW);
                digitalWrite(greenPin, LOW);
                lastBlinkTime = currentTime;
                Serial.print("Space ");
                Serial.print(spaceNumber);
                Serial.print(" blink state: ");
                Serial.println(blinkState ? "ON" : "OFF");
            }
        }

        // Handle buzzer (stolen or override)
        if ((status == "stolen" || buzzerOverride) && buzzerCycleCount < 5) {
            unsigned long currentTime = millis();
            if (currentTime - buzzerStartTime >= buzzerInterval) {
                buzzerOn = !buzzerOn;
                digitalWrite(BUZZER_PIN, buzzerOn ? HIGH : LOW);
                delay(1000);
                Serial.print("Space ");
                Serial.print(spaceNumber);
                Serial.print(" buzzer: ");
                Serial.println(buzzerOn ? "ON" : "OFF");
                buzzerStartTime = currentTime;
                if (!buzzerOn) {
                    buzzerCycleCount++; // Increment after OFF state
                    Serial.print("Buzzer cycle count: ");
                    Serial.println(buzzerCycleCount);
                }
            }
            buzzerActive = true;
        } else {
            digitalWrite(BUZZER_PIN, LOW);
            buzzerActive = false;
        }
    }
};

// Parking space instances
ParkingSpace space1(1, RGB1_RED, RGB1_GREEN, RGB1_BLUE);
ParkingSpace space2(2, RGB2_RED, RGB2_GREEN, RGB2_BLUE);

void setup() {
    Serial.begin(115200);
    // while (!Serial) delay(10); // Wait for Serial
    Serial.println("LoRa Receiver starting...");

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

    // Initialize status LEDs and buzzer
    pinMode(RED_LED, OUTPUT);
    pinMode(GREEN_LED, OUTPUT);
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(RED_LED, LOW);
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(BUZZER_PIN, LOW);

    // Initialize LoRa
    Serial.println("Initializing LoRa...");
    LoRa.setPins(LORA_CS, LORA_RST, LORA_DIO0);
    if (!LoRa.begin(433E6)) {
        Serial.println("LoRa initialization failed!");
        digitalWrite(RED_LED, HIGH);
        while (1);
    }
    LoRa.setSyncWord(0xF1);
    Serial.println("LoRa initialized");
    digitalWrite(GREEN_LED, HIGH);
    Serial.println("System setup complete");
    Serial.println("Enter commands (e.g., 'ON RGB1RED', 'ON BUZZER')");
}

void handleSerialCommand() {
    if (Serial.available()) {
        Serial.println("Received Serial input...");
        String command = Serial.readStringUntil('\n');
        command.trim();
        Serial.print("Command: ");
        Serial.println(command);
        if (command.startsWith("ON ")) {
            String led = command.substring(3);
            if (led == "RGB1RED") {
                digitalWrite(RGB1_RED, HIGH);
                Serial.println("Override: RGB1_RED ON");
            } else if (led == "RGB1GREEN") {
                digitalWrite(RGB1_GREEN, HIGH);
                Serial.println("Override: RGB1_GREEN ON");
            } else if (led == "RGB1BLUE") {
                digitalWrite(RGB1_BLUE, HIGH);
                Serial.println("Override: RGB1_BLUE ON");
            } else if (led == "RGB2RED") {
                digitalWrite(RGB2_RED, HIGH);
                Serial.println("Override: RGB2_RED ON");
            } else if (led == "RGB2GREEN") {
                digitalWrite(RGB2_GREEN, HIGH);
                Serial.println("Override: RGB2_GREEN ON");
            } else if (led == "RGB2BLUE") {
                digitalWrite(RGB2_BLUE, HIGH);
                Serial.println("Override: RGB2_BLUE ON");
            } else if (led == "BUZZER") {
                space1.buzzerOverride = true;
                space1.buzzerCycleCount = 0; // Reset cycle count
                space1.buzzerOn = false;     // Start with OFF
                space1.buzzerStartTime = millis();
                Serial.println("Override: BUZZER ON (5 cycles)");
            } else {
                Serial.println("Invalid LED ON command");
            }
        } else if (command.startsWith("OFF ")) {
            String led = command.substring(4);
            if (led == "RGB1RED") {
                digitalWrite(RGB1_RED, LOW);
                Serial.println("Override: RGB1_RED OFF");
            } else if (led == "RGB1GREEN") {
                digitalWrite(RGB1_GREEN, LOW);
                Serial.println("Override: RGB1_GREEN OFF");
            } else if (led == "RGB1BLUE") {
                digitalWrite(RGB1_BLUE, LOW);
                Serial.println("Override: RGB1_BLUE OFF");
            } else if (led == "RGB2RED") {
                digitalWrite(RGB2_RED, LOW);
                Serial.println("Override: RGB2_RED OFF");
            } else if (led == "RGB2GREEN") {
                digitalWrite(RGB2_GREEN, LOW);
                Serial.println("Override: RGB2_GREEN OFF");
            } else if (led == "RGB2BLUE") {
                digitalWrite(RGB2_BLUE, LOW);
                Serial.println("Override: RGB2_BLUE OFF");
            } else if (led == "BUZZER") {
                space1.buzzerOverride = false;
                space1.buzzerCycleCount = 5; // Stop cycles
                digitalWrite(BUZZER_PIN, LOW);
                Serial.println("Override: BUZZER OFF");
            } else if (led == "RGB2") {
                digitalWrite(RGB2_RED, LOW);
                digitalWrite(RGB2_GREEN, LOW);
                digitalWrite(RGB2_BLUE, LOW);
                Serial.println("Override: RGB2 OFF");
            } else if (led == "RGB1") {
                digitalWrite(RGB1_RED, LOW);
                digitalWrite(RGB1_GREEN, LOW);
                digitalWrite(RGB1_BLUE, LOW);
                Serial.println("Override: RGB1 OFF");
            } else {
                Serial.println("Invalid LED OFF command");
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
    if (request.indexOf("/space1/available") != -1) {
        space1.updateStatus("available");

    }
    else if (request.indexOf("/space1/occupied") != -1){
        space1.updateStatus("occupied");
    }
    else if (request.indexOf("/space1/stolen") != -1){
        space1.updateStatus("stolen");
    }
    else if (request.indexOf("/space2/available") != -1) {
            space2.updateStatus("available");
    }
    else if (request.indexOf("/space2/occupied") != -1){
        space2.updateStatus("occupied");
    }
    else if (request.indexOf("/space2/stolen") != -1){
        space2.updateStatus("stolen");
    }

    // Serve HTML page
    client.println("<!DOCTYPE html><html><head><title>Monitoring Panel Control</title>");
    client.println("<link rel='stylesheet' href='https://cdnjs.cloudflare.com/ajax/libs/bulma/0.9.3/css/bulma.min.css'>");
    client.println("<style>body { padding: 40px; } .control-group { margin-bottom: 40px; } .button { margin: 15px; }</style>");
    client.println("</head><body><div class='container'><h1 class='title'>Monitoring Control</h1>");

    // Space 1 controls
    client.println("<div class='control-group'><h2 class='subtitle'>Space 1 Control</h2>");
    client.println("<a class='button is-primary' href='/space1/available'>AVAILABLE</a>");
    client.println("<a class='button is-warning' href='/space1/occupied'>OCCUPIED</a>");
    client.println("<a class='button is-danger' href='/space1/stolen'>STOLEN</a></div>");

    // Space 2 controls
    client.println("<div class='control-group'><h2 class='subtitle'>Space 2 Control</h2>");
    client.println("<a class='button is-primary' href='/space2/available'>AVAILABLE</a>");
    client.println("<a class='button is-warning' href='/space2/occupied'>OCCUPIED</a>");
    client.println("<a class='button is-danger' href='/space2/stolen'>STOLEN</a></div>");

    client.println("</div></body></html>");
    client.stop();
}

void loop() {
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        digitalWrite(RGB1_BLUE, HIGH);
    }

    // Track buzzer state
    bool buzzerActive = false;

    // Update LED blinking and buzzer for stolen state
    space1.update(buzzerActive);
    space2.update(buzzerActive);
    // handleSerialCommand();
    handleWiFiClient();

    // Control buzzer (only one space’s buzzer is active at a time)
    static bool lastBuzzerState = false;
    if (buzzerActive) {
        if (space1.buzzerOn || space2.buzzerOn) {
            bool currentBlinkState = (space1.buzzerOn ? space1.buzzerOn : space2.buzzerOn);
            if (currentBlinkState != lastBuzzerState) {
                Serial.print("Buzzer: Stolen/Override alert ");
                Serial.println(currentBlinkState ? "ON" : "OFF");
                lastBuzzerState = currentBlinkState;
            }
        }
    } else {
        digitalWrite(BUZZER_PIN, LOW);
        if (lastBuzzerState) {
            Serial.println("Buzzer: Stolen/Override alert OFF");
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