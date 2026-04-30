#include <Servo.h>

Servo servo;

// ------------------ CONFIG ------------------
const int SERVO_PIN  = 9;
const int BUTTON_PIN = 2;

const int MIN_ANGLE = 45;
const int MAX_ANGLE = 135;
const int CENTER    = 90;

// Deterministic, hand-chosen "random-looking" positions
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

  // Wake servo up at 45° immediately
  servo.write(MIN_ANGLE);
  currentAngle = MIN_ANGLE;

  Serial.begin(9600);
  Serial.println("Ready. Servo parked at 45°. Press button to start.");
}

void loop() {
  int buttonState = digitalRead(BUTTON_PIN);

  // Start test on button press
  if (buttonState == HIGH && lastButtonState == LOW && !testRunning) {
    Serial.println("Starting 60-second motion test");
    testRunning = true;
    testStartTime = millis();
    segmentStartTime = millis();
    currentSegment = SEG1;

    servo.write(MIN_ANGLE);
    currentAngle = MIN_ANGLE;
  }
  lastButtonState = buttonState;

  if (!testRunning) return;

  unsigned long elapsed = millis() - testStartTime;

  // Stop after 60 seconds
  if (elapsed >= 60000UL) {
    Serial.println("Test complete");
    testRunning = false;
    servo.write(MIN_ANGLE);
    return;
  }

  // Segment selection
  if (elapsed < 15000UL) {
    setSegment(SEG1);
    timeBasedSweep(15000UL, 2);   // Section 1: 4 rotations (slow)
  }
  else if (elapsed < 30000UL) {
    setSegment(SEG2);
    timeBasedSweep(15000UL, 4);   // Section 2: same motion, faster
  }
  else if (elapsed < 45000UL) {
    setSegment(SEG3);
    scriptedJumpAndHold();        // Deterministic jumps with holds
  }
  else {
    setSegment(SEG4);
    centeredJitter();             // Jitter centered at 90°
  }
}

// ------------------------------------------------
// Segment transition handling
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
// Time-based sweep with guaranteed endpoints
void timeBasedSweep(unsigned long sectionDurationMs, int numCycles) {
  unsigned long t = millis() - segmentStartTime;

  if (t >= sectionDurationMs) {
    servo.write(MIN_ANGLE);
    currentAngle = MIN_ANGLE;
    return;
  }

  float progress = (float)t / sectionDurationMs;

  float span = MAX_ANGLE - MIN_ANGLE;
  float totalTravel = numCycles * 2.0 * span;  // back-and-forth motion

  float traveled = progress * totalTravel;
  float phase = fmod(traveled, 2.0 * span);

  int angle;
  if (phase <= span) {
    angle = MIN_ANGLE + phase;          // ascending
  } else {
    angle = MAX_ANGLE - (phase - span); // descending
  }

  servo.write(angle);
  currentAngle = angle;
}

// ------------------------------------------------
// Section 3: deterministic "random-looking" jumps
void scriptedJumpAndHold() {
  static unsigned long lastMove = 0;
  static int idx = 0;

  if (millis() - lastMove < 1000) return; // 1 second hold
  lastMove = millis();

  currentAngle = scriptedPositions[idx];
  servo.write(currentAngle);

  idx = (idx + 1) % NUM_SCRIPTED;
}

// ------------------------------------------------
// Section 4: jitter tightly centered around 90°
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
