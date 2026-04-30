#include <Servo.h>

Servo Servo1;

int Servo1Pin = 9;

const int buttonPin = 2;
int buttonState = 0;
int lastButtonState = 0;

void setup() {
  pinMode(buttonPin, INPUT);  // External pull-down
  Servo1.attach(Servo1Pin);
  Serial.begin(9600);
}

void loop() {
  buttonState = digitalRead(buttonPin);

  // Detect LOW -> HIGH transition
  if (buttonState == HIGH && lastButtonState == LOW) {
    Serial.println("Button pressed (trigger once)");

    // Move from 45 to 90 degrees
    for (int angle = 45; angle <= 90; angle++) {
      Servo1.write(angle);
      delay(15);
    }

    // Move back from 90 to 45 degrees
    for (int angle = 90; angle >= 45; angle--) {
      Servo1.write(angle);
      delay(15);
    }
  }

  if (buttonState == LOW && lastButtonState == HIGH) {
    Serial.println("Button released");
  }

  // Save state for next loop
  lastButtonState = buttonState;

  delay(50); // small debounce delay
}
