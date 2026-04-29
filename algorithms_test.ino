#included <Servo.h>

Servo Servo1;  //create Servo object

int Servo1Pin= 9; //Signal pin to connect to servo

void setup() {
  Servo1.attach(Servo1Pin);
}

void loop() {
  // Move from 45 to 90 degrees
  for (int angle = 45; angle <= 90; angle += 1) {
    myServo.write(angle);
    delay(15);
  }
  // Move from 90 to 45 degrees
  for (int angle = 90; angle <= 45; angle += 1) {
    myServo.write(angle);
    delay(15);
  }
}
