#include <Servo.h>

Servo Servo1;

int Servo1Pin = 9;   // Signal pin for servo
int ButtonPin = A7;  // using A7 as input for button

void setup() {
  Servo1.attach(Servo1Pin);
  pinMode(ButtonPin, INPUT);  // external pulldown resistor
}

void loop() {
  int ButtonState = digitalRead(ButtonPin);

  // Print the raw button state
  Serial.print("Button state: ");
  Serial.println(ButtonState);

  // Optional: small delay so the monitor is readable
  delay(100);

  if (ButtonState == HIGH) {
    for (int angle = 45; angle <= 90; angle++) {
      Servo1.write(angle);
      delay(15);
    }

    for (int angle = 90; angle >= 45; angle--) {
      Servo1.write(angle);
      delay(15);
    }
  }
}
