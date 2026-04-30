#include <Servo.h>

Servo servo;

// ------------------ CONFIG ------------------
const int SERVO_PIN = 9;
const int BUTTON_PIN = 2;

// Safe mechanical limits
const int MIN_ANGLE = 45;
const int MAX_ANGLE = 135;

// Deterministic "random-looking" positions
const int scriptedPositions[] = {
  52, 128, 67, 111, 59, 134, 76,
  98, 121, 63, 140, 85, 110
};
const int NUM_SCRIPTED = sizeof(scriptedPositions) / sizeof(scriptedPositions[0]);
// --------------------------------------------

// State
bool testRunning = false;
unsigned long testStartTime = 0;
int currentAngle = 90;

// Button debounce
int lastButtonState = LOW;

void setup() {
  pinMode(BUTTON_PIN, INPUT); // external pull-down
  servo.attach(SERVO_PIN);
  servo.write(45);

  Serial.begin(9600);
  Serial.println("Ready. Press button to start test.");
}

void loop() {
  int buttonState = digitalRead(BUTTON_PIN);

  // Start test on button press
  if (buttonState == HIGH && lastButtonState == LOW && !testRunning) {
    Serial.println("Starting 60-second motion test");
    testRunning = true;
    testStartTime = millis();
  }
  lastButtonState = buttonState;

  if (!testRunning) return;

  unsigned long elapsed = millis() - testStartTime;

  // Stop after 60 seconds
  if (elapsed >= 60000UL) {
    Serial.println("Test completed");
    testRunning = false;
    servo.write(90);
    return;
  }

  // Select motion segment
  if (elapsed < 15000UL) {
    smoothSweep();
  } 
  else if (elapsed < 30000UL) {
    fastDeterministicSteps();
  } 
  else if (elapsed < 45000UL) {
    scriptedJumpAndHold();
  } 
  else {
    jitterBurst();
  }
}

// ---------- MOTION SEGMENTS ----------

// 0–15 s: smooth, low-jerk sweep
void smoothSweep() {
  static int direction = 1;
  static unsigned long lastUpdate = 0;

  if (millis() - lastUpdate < 15) return;
  lastUpdate = millis();

  currentAngle += direction;
  if (currentAngle >= MAX_ANGLE || currentAngle <= MIN_ANGLE) {
    direction *= -1;
  }

  servo.write(currentAngle);
}

// 15–30 s: fast, sharp but deterministic steps
void fastDeterministicSteps() {
  static unsigned long lastStep = 0;
  static int idx = 0;

  if (millis() - lastStep < 300) return;
  lastStep = millis();

  currentAngle = scriptedPositions[idx];
  servo.write(currentAngle);

  idx = (idx + 1) % NUM_SCRIPTED;
}

// 30–45 s: jump to fixed positions, hold 1 second
void scriptedJumpAndHold() {
  static unsigned long lastMove = 0;
  static int idx = 0;

  if (millis() - lastMove < 1000) return; // 1s hold
  lastMove = millis();

  currentAngle = scriptedPositions[idx];
  servo.write(currentAngle);

  idx = (idx + 1) % NUM_SCRIPTED;
}

// 45–60 s: jitter torture (small, rapid disturbances)
void jitterBurst() {
  static unsigned long lastUpdate = 0;
  static int jitterIndex = 0;

  if (millis() - lastUpdate < 80) return;
  lastUpdate = millis();

  // Fixed jitter pattern (also deterministic)
  const int jitterPattern[] = { -4, 3, -2, 5, -3, 2 };
  const int jpLen = sizeof(jitterPattern) / sizeof(jitterPattern[0]);

  currentAngle = constrain(
    currentAngle + jitterPattern[jitterIndex],
    MIN_ANGLE,
    MAX_ANGLE
  );

  servo.write(currentAngle);
  jitterIndex = (jitterIndex + 1) % jpLen;
}
