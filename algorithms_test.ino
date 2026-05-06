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
const unsigned long SYNC_DURATION_MS       = 2000;  // original linear sync signal
const unsigned long SYNC_PAUSE_MS          = 1000;  // existing pause after sync
const unsigned long MOVE_TO_START_MS       = 1000;  // slowed back down to reduce tracking loss
const unsigned long INTER_SECTION_PAUSE_MS = 500;   // 0.5s still buffer before/after sections
const unsigned long SEG2_TO_SEG3_MS        = 500;   // smooth transition into first Section 3 hold target
const unsigned long END_RETURN_MS          = 500;   // smooth return to sync start after Section 4

const unsigned long SEG1_MS = 15000;  // slow sweep
const unsigned long SEG2_MS = 10000;  // moderate sweep
const unsigned long SEG3_MS = 15000;  // jump + hold
const unsigned long SEG4_MS = 8000;   // micro-adjustments

// Sweep cycles
const int SEG1_CYCLES = 2;
const int SEG2_CYCLES = 2;

// Segment 3 scripted positions
// 15 unique 1-second holds, ending at 90° so Section 4 can start there with no transition
const int scriptedPositions[] = {
  52, 100, 67, 111, 59, 95, 76,
  98, 121, 63, 78, 100,
  69, 82, 90
};
const int NUM_SCRIPTED = sizeof(scriptedPositions) / sizeof(scriptedPositions[0]);
const unsigned long JUMP_HOLD_MS = 1000;

// Segment 4 micro-adjustment pattern
const int microAdjustPattern[] = {
   0,  0,  1,  1,  2,  2,  3,  3,
   2,  2,  1,  1,  0,  0,
  -1, -1, -2, -2, -1, -1,
   0,  0,  1,  1,  2,  2,
   1,  1,  0,  0
};
const int MICRO_LEN = sizeof(microAdjustPattern) / sizeof(microAdjustPattern[0]);
const unsigned long MICRO_UPDATE_MS = 300;
// --------------------------------------------


// ------------------ STATE MACHINE ------------------
/*
Timeline after button press (ms):
0    - 2000   SYNC_SIGNAL
2000 - 3000   SYNC_PAUSE
3000 - 4000   MOVE_TO_START
4000 - 4500   PAUSE_BEFORE_SEG1
4500 - 19500  SEG1_SWEEP
19500- 20000  PAUSE_AFTER_SEG1
20000- 30000  SEG2_SWEEP
30000- 30500  PAUSE_AFTER_SEG2
30500- 31000  TRANSITION_TO_SEG3
31000- 31500  PAUSE_BEFORE_SEG3
31500- 46500  SEG3_JUMP_HOLD   (ends at 90°)
46500- 47000  PAUSE_AFTER_SEG3
47000- 55000  SEG4_MICRO_ADJUST (starts at 90°)
55000- 55500  PAUSE_AFTER_SEG4
55500- 56000  TRANSITION_TO_END
56000+        WAIT_FOR_BUTTON
Total after button press ≈ 56.0 seconds
*/

enum Phase {
  WAIT_FOR_BUTTON,
  SYNC_SIGNAL,
  SYNC_PAUSE,
  MOVE_TO_START,
  PAUSE_BEFORE_SEG1,
  SEG1_SWEEP,
  PAUSE_AFTER_SEG1,
  SEG2_SWEEP,
  PAUSE_AFTER_SEG2,
  TRANSITION_TO_SEG3,
  PAUSE_BEFORE_SEG3,
  SEG3_JUMP_HOLD,
  PAUSE_AFTER_SEG3,
  SEG4_MICRO_ADJUST,
  PAUSE_AFTER_SEG4,
  TRANSITION_TO_END,
  DONE
};

Phase phase = WAIT_FOR_BUTTON;
unsigned long phaseStart = 0;

int currentAngle = CENTER;
int transitionStartAngle = CENTER;
int lastButtonState = LOW;

// Segment 3 state
int seg3Index = 0;
unsigned long lastJumpChange = 0;

// Segment 4 state
int seg4Index = 0;
unsigned long lastMicroUpdate = 0;


// ------------------ HELPERS ------------------
void writeAngle(int a) {
  a = constrain(a, 0, 180);
  servo.write(a);
  currentAngle = a;
}

int rampAngle(int startA, int targetA, unsigned long elapsed, unsigned long durationMs) {
  if (elapsed >= durationMs) return targetA;
  float u = (float)elapsed / (float)durationMs;
  return (int)(startA + (targetA - startA) * u);
}

void enterPhase(Phase p) {
  phase = p;
  phaseStart = millis();

  // Capture current angle when starting transition-like phases
  if (p == MOVE_TO_START || p == TRANSITION_TO_SEG3 || p == TRANSITION_TO_END) {
    transitionStartAngle = currentAngle;
  }

  // Initialize Section 3 so it starts already holding the first target
  if (p == SEG3_JUMP_HOLD) {
    writeAngle(scriptedPositions[0]);
    seg3Index = 1;               // next jump goes to second target
    lastJumpChange = millis();   // hold first target for 1 second
  }

  // Initialize Section 4 centered and ready
  if (p == SEG4_MICRO_ADJUST) {
    writeAngle(CENTER);
    seg4Index = 0;
    lastMicroUpdate = millis();
  }

  Serial.print("PHASE: ");
  Serial.print((int)phase);
  Serial.print(" @ ");
  Serial.println(phaseStart);
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
    return (int)(MIN_ANGLE + phasePos);            // ascending
  } else {
    return (int)(MAX_ANGLE - (phasePos - span));   // descending
  }
}

void microAdjust() {
  if (millis() - lastMicroUpdate < MICRO_UPDATE_MS) return;
  lastMicroUpdate = millis();

  int target = CENTER + microAdjustPattern[seg4Index];
  target = constrain(target, CENTER - 5, CENTER + 5);
  writeAngle(target);

  seg4Index = (seg4Index + 1) % MICRO_LEN;
}


// ------------------ SETUP ------------------
void setup() {
  pinMode(BUTTON_PIN, INPUT); // external pulldown
  servo.attach(SERVO_PIN);

  Serial.begin(9600);
  Serial.println("Ready. Servo parked at signature start angle. Start HoloLens, then press button ~3s later.");

  // Park at sync start angle so repeated runs do not have a catch-up jump
  writeAngle(SYNC_START_ANGLE);
  enterPhase(WAIT_FOR_BUTTON);
}


// ------------------ MAIN LOOP ------------------
void loop() {
  int buttonState = digitalRead(BUTTON_PIN);

  // Rising edge starts the sequence
  if (phase == WAIT_FOR_BUTTON) {
    if (buttonState == HIGH && lastButtonState == LOW) {
      Serial.println("Button pressed — starting sequence.");

      // Reset helpers
      seg3Index = 0;
      lastJumpChange = 0;
      seg4Index = 0;
      lastMicroUpdate = 0;

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

      // Original linear sync signal: 110 -> 70 over ~2 seconds
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
      // Existing pause after sync
      writeAngle(SYNC_END_ANGLE);
      if (millis() - phaseStart >= SYNC_PAUSE_MS) {
        enterPhase(MOVE_TO_START);
      }
      break;
    }

    case MOVE_TO_START: {
      // Slower move to Section 1 start (45°), then pause before Section 1
      unsigned long t = millis() - phaseStart;
      if (t >= MOVE_TO_START_MS) {
        writeAngle(MIN_ANGLE);
        enterPhase(PAUSE_BEFORE_SEG1);
      } else {
        writeAngle(rampAngle(transitionStartAngle, MIN_ANGLE, t, MOVE_TO_START_MS));
      }
      break;
    }

    case PAUSE_BEFORE_SEG1: {
      writeAngle(MIN_ANGLE);
      if (millis() - phaseStart >= INTER_SECTION_PAUSE_MS) {
        enterPhase(SEG1_SWEEP);
      }
      break;
    }

    case SEG1_SWEEP: {
      writeAngle(sweepAngle(SEG1_MS, SEG1_CYCLES));
      if (millis() - phaseStart >= SEG1_MS) {
        writeAngle(MIN_ANGLE);  // Section 1 ends where Section 2 starts
        enterPhase(PAUSE_AFTER_SEG1);
      }
      break;
    }

    case PAUSE_AFTER_SEG1: {
      writeAngle(MIN_ANGLE);
      if (millis() - phaseStart >= INTER_SECTION_PAUSE_MS) {
        enterPhase(SEG2_SWEEP);
      }
      break;
    }

    case SEG2_SWEEP: {
      writeAngle(sweepAngle(SEG2_MS, SEG2_CYCLES));
      if (millis() - phaseStart >= SEG2_MS) {
        writeAngle(MIN_ANGLE);  // Section 2 ends at 45°
        enterPhase(PAUSE_AFTER_SEG2);
      }
      break;
    }

    case PAUSE_AFTER_SEG2: {
      writeAngle(MIN_ANGLE);
      if (millis() - phaseStart >= INTER_SECTION_PAUSE_MS) {
        enterPhase(TRANSITION_TO_SEG3);
      }
      break;
    }

    case TRANSITION_TO_SEG3: {
      // Smooth transition into first Section 3 hold target
      unsigned long t = millis() - phaseStart;
      int target = scriptedPositions[0];

      if (t >= SEG2_TO_SEG3_MS) {
        writeAngle(target);
        enterPhase(PAUSE_BEFORE_SEG3);
      } else {
        writeAngle(rampAngle(transitionStartAngle, target, t, SEG2_TO_SEG3_MS));
      }
      break;
    }

    case PAUSE_BEFORE_SEG3: {
      writeAngle(scriptedPositions[0]);
      if (millis() - phaseStart >= INTER_SECTION_PAUSE_MS) {
        enterPhase(SEG3_JUMP_HOLD);
      }
      break;
    }

    case SEG3_JUMP_HOLD: {
      unsigned long elapsed = millis() - phaseStart;

      if (elapsed >= SEG3_MS) {
        // Section 3 naturally ends at 90°, same as Section 4 start
        writeAngle(CENTER);
        enterPhase(PAUSE_AFTER_SEG3);
        break;
      }

      if (millis() - lastJumpChange >= JUMP_HOLD_MS) {
        lastJumpChange = millis();
        int target;
        if (seg3Index < NUM_SCRIPTED) {
          target = scriptedPositions[seg3Index];
          seg3Index++;
        } else {
          target = scriptedPositions[NUM_SCRIPTED - 1];  // stay at final target
        }
        writeAngle(target);
      }
      break;
    }

    case PAUSE_AFTER_SEG3: {
      // No transition to Section 4; hold at 90° then start Section 4
      writeAngle(CENTER);
      if (millis() - phaseStart >= INTER_SECTION_PAUSE_MS) {
        enterPhase(SEG4_MICRO_ADJUST);
      }
      break;
    }

    case SEG4_MICRO_ADJUST: {
      unsigned long elapsed = millis() - phaseStart;

      if (elapsed >= SEG4_MS) {
        enterPhase(PAUSE_AFTER_SEG4);
        break;
      }

      microAdjust();
      break;
    }

    case PAUSE_AFTER_SEG4: {
      // Hold final Section 4 position, then return to sync start
      writeAngle(currentAngle);
      if (millis() - phaseStart >= INTER_SECTION_PAUSE_MS) {
        enterPhase(TRANSITION_TO_END);
      }
      break;
    }

    case TRANSITION_TO_END: {
      // Smooth return to sync start after the post-Section-4 pause
      unsigned long t = millis() - phaseStart;
      if (t >= END_RETURN_MS) {
        writeAngle(SYNC_START_ANGLE);
        enterPhase(DONE);
      } else {
        writeAngle(rampAngle(transitionStartAngle, SYNC_START_ANGLE, t, END_RETURN_MS));
      }
      break;
    }

    case DONE: {
      Serial.println("Sequence complete. Parking at signature start angle.");
      writeAngle(SYNC_START_ANGLE);
      enterPhase(WAIT_FOR_BUTTON);
      break;
    }

    default:
      break;
  }
}
