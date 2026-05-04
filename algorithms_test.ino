#include <Servo.h>
#include <math.h>   // for fmod()

Servo servo;

// ------------------ CONFIG ------------------
const int SERVO_PIN  = 9;
const int BUTTON_PIN = 2;

// Angles
const int MIN_ANGLE = 45;
const int MAX_ANGLE = 135;
const int CENTER    = 90;

// Signature angles
const int SYNC_START_ANGLE = 110;
const int SYNC_END_ANGLE   = 70;

// Timing (ms)
const unsigned long SYNC_DURATION_MS   = 2000;  // signature lasts ~2s
const unsigned long SYNC_PAUSE_MS      = 1000;
const unsigned long MOVE_TO_START_MS   = 1000;

const unsigned long SEG1_MS = 15000;  // slow sweep
const unsigned long SEG2_MS = 10000;  // moderate sweep (reduced difficulty)
const unsigned long SEG3_MS = 15000;  // jump + hold
const unsigned long SEG4_MS = 8000;   // micro-adjustments

// Sweep cycles
const int SEG1_CYCLES = 2;
const int SEG2_CYCLES = 2;   // CHANGED: was 4, now slower and more useful

// Segment 3 scripted “random-looking” positions
const int scriptedPositions[] = {
  52, 128, 67, 111, 59, 134, 76,
  98, 121, 63, 110, 85
};
const int NUM_SCRIPTED = sizeof(scriptedPositions) / sizeof(scriptedPositions[0]);
const unsigned long JUMP_HOLD_MS = 1000;

// Segment 4 micro-adjustment pattern
// Small, fast, tightly concentrated "needle line-up" corrections
const int microAdjustPattern[] = {
   0,  2,  4,  3,  5,  4,  2,  1,
   0, -1,  0,  1,  3,  2,  0, -2,
  -3, -1,  0,  2,  1,  0,  1,  3,
   2,  1,  0, -1,  0,  1
};
const int MICRO_LEN = sizeof(microAdjustPattern) / sizeof(microAdjustPattern[0]);
const unsigned long MICRO_UPDATE_MS = 120;
// --------------------------------------------

// State machine
enum Phase {
  WAIT_FOR_BUTTON,
  SYNC_SIGNAL,
  SYNC_PAUSE,
  MOVE_TO_START,
  SEG1_SWEEP,
  SEG2_SWEEP,
  SEG3_JUMP_HOLD,
  SEG4_MICRO_ADJUST,
  DONE
};

Phase phase = WAIT_FOR_BUTTON;
unsigned long phaseStart = 0;

int currentAngle = CENTER;
int moveStartAngle = CENTER;
int lastButtonState = LOW;

// For SEG3
int seg3Index = 0;
unsigned long lastJumpChange = 0;

// For SEG4
int seg4Index = 0;
unsigned long lastMicroUpdate = 0;

// ---------- Helpers ----------
void enterPhase(Phase p) {
  phase = p;
  phaseStart = millis();

  if (p == MOVE_TO_START) {
    moveStartAngle = currentAngle;
  }

  Serial.print("PHASE: ");
  Serial.print((int)phase);
  Serial.print(" @ ");
  Serial.println(phaseStart);
}

void writeAngle(int a) {
  a = constrain(a, 0, 180);
  servo.write(a);
  currentAngle = a;
}

// Time-based sweep with guaranteed endpoints
int sweepAngle(unsigned long sectionDurationMs, int numCycles) {
  unsigned long t = millis() - phaseStart;
  if (t >= sectionDurationMs) return MIN_ANGLE;

  float progress = (float)t / (float)sectionDurationMs;

  float span = (float)(MAX_ANGLE - MIN_ANGLE);
  float totalTravel = numCycles * 2.0f * span;
  float traveled = progress * totalTravel;

  float phasePos = fmod(traveled, 2.0f * span);

  if (phasePos <= span) {
    return (int)(MIN_ANGLE + phasePos);
  } else {
    return (int)(MAX_ANGLE - (phasePos - span));
  }
}

// Segment 4: simulated needle line-up micro adjustments
void microAdjust() {
  if (millis() - lastMicroUpdate < MICRO_UPDATE_MS) return;
  lastMicroUpdate = millis();

  int target = CENTER + microAdjustPattern[seg4Index];
  target = constrain(target, CENTER - 8, CENTER + 8);   // tightly concentrated
  writeAngle(target);

  seg4Index = (seg4Index + 1) % MICRO_LEN;
}

// ---------- Arduino Setup ----------
void setup() {
  pinMode(BUTTON_PIN, INPUT); // external pulldown
  servo.attach(SERVO_PIN);

  Serial.begin(9600);
  Serial.println("Ready. Servo parked at signature start angle. Start HoloLens, then press button ~3s later.");

  writeAngle(SYNC_START_ANGLE);   // important for repeatable starts
  enterPhase(WAIT_FOR_BUTTON);
}

// ---------- Main Loop ----------
void loop() {
  int buttonState = digitalRead(BUTTON_PIN);

  // Rising edge starts the sequence
  if (phase == WAIT_FOR_BUTTON) {
    if (buttonState == HIGH && lastButtonState == LOW) {
      Serial.println("Button pressed — starting sequence.");

      // reset helpers
      seg3Index = 0;
      lastJumpChange = 0;
      seg4Index = 0;
      lastMicroUpdate = 0;

      // Start signature immediately at 110°
      writeAngle(SYNC_START_ANGLE);
      enterPhase(SYNC_SIGNAL);
    }

    lastButtonState = buttonState;
    return;
  }

  lastButtonState = buttonState;

  switch (phase) {

    case SYNC_SIGNAL: {
      unsigned long elapsed = millis() - phaseStart;

      // Signature: slow, one-direction swivel 110 -> 70 over ~2s
      if (elapsed >= SYNC_DURATION_MS) {
        writeAngle(SYNC_END_ANGLE);
        enterPhase(SYNC_PAUSE);
        break;
      }

      float u = (float)elapsed / (float)SYNC_DURATION_MS;
      int angle = (int)(SYNC_START_ANGLE + (SYNC_END_ANGLE - SYNC_START_ANGLE) * u);
      writeAngle(angle);
      break;
    }

    case SYNC_PAUSE: {
      // Hold at end of signature (70°) for 1s
      writeAngle(SYNC_END_ANGLE);
      if (millis() - phaseStart >= SYNC_PAUSE_MS) {
        enterPhase(MOVE_TO_START);
      }
      break;
    }

    case MOVE_TO_START: {
      // Smooth ramp from current angle to 45° over 1s
      unsigned long t = millis() - phaseStart;

      if (t >= MOVE_TO_START_MS) {
        writeAngle(MIN_ANGLE);
        enterPhase(SEG1_SWEEP);
      } else {
        float u = (float)t / (float)MOVE_TO_START_MS;
        int a = (int)(moveStartAngle + (MIN_ANGLE - moveStartAngle) * u);
        writeAngle(a);
      }
      break;
    }

    case SEG1_SWEEP: {
      int a = sweepAngle(SEG1_MS, SEG1_CYCLES);
      writeAngle(a);

      if (millis() - phaseStart >= SEG1_MS) {
        writeAngle(MIN_ANGLE);
        enterPhase(SEG2_SWEEP);
      }
      break;
    }

    case SEG2_SWEEP: {
      int a = sweepAngle(SEG2_MS, SEG2_CYCLES);
      writeAngle(a);

      if (millis() - phaseStart >= SEG2_MS) {
        writeAngle(MIN_ANGLE);
        enterPhase(SEG3_JUMP_HOLD);
      }
      break;
    }

    case SEG3_JUMP_HOLD: {
      unsigned long elapsed = millis() - phaseStart;

      if (elapsed >= SEG3_MS) {
        enterPhase(SEG4_MICRO_ADJUST);
        break;
      }

      if (millis() - lastJumpChange >= JUMP_HOLD_MS) {
        lastJumpChange = millis();
        int target = scriptedPositions[seg3Index];
        writeAngle(target);
        seg3Index = (seg3Index + 1) % NUM_SCRIPTED;
      }
      break;
    }

    case SEG4_MICRO_ADJUST: {
      unsigned long elapsed = millis() - phaseStart;

      if (elapsed >= SEG4_MS) {
        enterPhase(DONE);
        break;
      }

      microAdjust();
      break;
    }

    case DONE: {
      Serial.println("Sequence complete. Parking at signature start angle.");
      writeAngle(SYNC_START_ANGLE);   // important for repeated runs
      enterPhase(WAIT_FOR_BUTTON);
      break;
    }

    default:
      break;
  }
}
