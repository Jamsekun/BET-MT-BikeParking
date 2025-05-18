#include <Arduino.h>

#define BUZZER_PIN 38 // Buzzer for stolen alert

void setup() {
    pinMode(BUZZER_PIN, OUTPUT);
    Serial.begin(115200);
    while (!Serial);
    Serial.println("Testing BUzzer...");
    Serial.println("Buzzer initialized");
}


void loop () {
    Serial.println("Buzzer is ON");
    digitalWrite(BUZZER_PIN, HIGH);
    delay(1000);
    Serial.println("Buzzer is OFF");
    digitalWrite(BUZZER_PIN, LOW);
    delay(1000);
}