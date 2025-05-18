#include <Arduino.h>

// IR Sensor Pins
const int A_IR_pin = 1;  // Parking Space 1 - IR Sensor A
const int B_IR_pin = 35;  // Parking Space 1 - IR Sensor B

// Variables to track previous states
bool prevAState = HIGH;
bool prevBState = HIGH;

void setup() {
  Serial.begin(115200);
  
  // Initialize IR sensor pins as inputs
  pinMode(A_IR_pin, INPUT);  // Enable internal pull-up resistor
  pinMode(B_IR_pin, INPUT);
  
  Serial.println("IR Sensor Monitoring Started");
  Serial.println("---------------------------");
  Serial.println("Format: SensorA | SensorB");
  Serial.println("---------------------------");
}

void loop() {
  // Read current states
  bool currentAState = digitalRead(A_IR_pin);
  bool currentBState = digitalRead(B_IR_pin);
  
  // Print only when state changes
  Serial.print("IR 1: ");
  Serial.println(currentAState);
//   Serial.print("IR 2: ");
//   Serial.println(currentBState);
  
  delay(500); // Small delay to debounce
}