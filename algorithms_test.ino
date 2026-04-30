#include <Servo.h>

Servo servo;

// ------------------ CONFIG ------------------
const int SERVO_PIN  = 9;
const int BUTTON_PIN = 2;

const int MIN_ANGLE = 45;
const int MAX_ANGLE = 135;
const int CENTER    = 90;

// Deterministic scripted positions (appear random)
const int scriptedPositions[] = {
  52, 128, 67, 111, 59, 134, 76,
  98, 121, 63, 110, 85
};
const int NUM_SCRIPTED = sizeof(scriptedPositions) / sizeof(scriptedPositions[0]);
// --------------------------------------------

// State
bool testRunning = false;
unsigned long testStartTime = 0;
unsigned long segmentStartTime = 0;

int currentAngle = MIN_ANGLE;
int lastButtonState = LOW;

enum Segment {
  SEG1,
  SEG2,
  SEG3,
  SEG4
};

Segment currentSegment = SEG1;

// ------------------------------------------------

void setup() {
  pinMode(BUTTON_PIN, INPUT);   // external pull-down
  servo.attach(SERVO_PIN);

  // Wake up at 45°
  servo.write(MIN_ANGLE);
  currentAngle = MIN_ANGLE;

  Serial.begin(9600);
  Serial.println("Ready. Servo parked at 45°. Press button to start.");
}

void loop() {
  int buttonState = digitalRead(BUTTON_PIN);

  // Start test
  if (buttonState == HIGH && lastButtonState == LOW && !testRunning) {
    Serial.println("Starting 60-second motion test");
    testRunning = true;
    testStartTime = millis();
    segmentStartTime = millis();
    currentSegment = SEG1;

    servo.write(MIN_ANGLE);  // ensure exact start
    currentAngle = MIN_ANGLE;
  }
  lastButtonState = buttonState;

  if (!testRunning) return;

  unsigned long elapsed = millis() - testStartTime;

  if (elapsed >= 60000UL) {
    Serial.println("Test complete");
    testRunning = false;
    servo.write(MIN_ANGLE);
    return;
  }

  // Segment switching
  if (elapsed < 15000UL) {
    setSegment(SEG1);
    smoothSweep(15);      // slow
  }
  else if (elapsed < 30000UL) {
    setSegment(SEG2);
    smoothSweep(6);       // faster
  }
  else if (elapsed < 45000UL) {
    setSegment(SEG3);
    scriptedJumpAndHold();
  }
  else {
    setSegment(SEG4);
    centeredJitter();
  }
}

// ------------------------------------------------
// Segment handling
void setSegment(Segment seg) {
  if (seg != currentSegment) {
    currentSegment = seg;
    segmentStartTime = millis();

    // Force clean transitions
    if (seg == SEG1 || seg == SEG2) {
      servo.write(MIN_ANGLE);
      currentAngle = MIN_ANGLE;
    }
    if (seg == SEG4) {
      servo.write(CENTER);
      currentAngle = CENTER;
    }
  }
}

// ------------------------------------------------
// SEGMENT 1 & 2 – Smooth sweep, different speeds
void smoothSweep(int stepDelay) {
  static unsigned long lastUpdate = 0;
  static int direction = 1;

  if (millis() - lastUpdate < stepDelay) return;
  lastUpdate = millis();

  currentAngle += direction;

  if (currentAngle >= MAX_ANGLE) {
    direction = -1;
  }
  if (currentAngle <= MIN_ANGLE) {
    currentAngle = MIN_ANGLE;
    direction = 1;

    // End sweep cleanly at 45°
    servo.write(MIN_ANGLE);
    return;
  }

  servo.write(currentAngle);
}

// ------------------------------------------------
// SEGMENT 3 – Deterministic random-looking jumps
void scriptedJumpAndHold() {
  static unsigned long lastMove = 0;
  static int idx = 0;

  if (millis() - lastMove < 1000) return;
  lastMove = millis();

  currentAngle = scriptedPositions[idx];
  servo.write(currentAngle);

  idx = (idx + 1) % NUM_SCRIPTED;
}

// ------------------------------------------------
// SEGMENT 4 – Jitter centered at 90°
void centeredJitter() {
  static unsigned long lastUpdate = 0;
  static int idx = 0;

  if (millis() - lastUpdate < 70) return;
  lastUpdate = millis();

  const int jitterPattern[] = { -6, 4, -3, 5, -2, 3 };
  const int jpLen = sizeof(jitterPattern) / sizeof(jitterPattern[0]);

  int target = CENTER + jitterPattern[idx];
  target = constrain(target, CENTER - 15, CENTER + 15);

  servo.write(target);
  currentAngle = target;

  idx = (idx + 1) % jpLen;
}
