#include <Servo.h>
#include <math.h>   // for fmod()

Servo servo;

// ------------------ CONFIG ------------------
const int SERVO_PIN    = 9;
const int BUTTON1_PIN  = 2;   // normal button: Section 2
const int BUTTON2_PIN  = 5;   // second button: Section 3

// Button mode selection
// - BUTTON1 only        -> Section 2 (sweep)
// - BUTTON2 only        -> Section 3 (jump + hold)
// - BUTTON1 + BUTTON2   -> Section 4 (small corrections)
//
// To make "both buttons" easier to register, the sketch waits briefly after the
// first press, then samples both buttons together.
const unsigned long SELECT_WINDOW_MS = 150;

// Angles
const int MIN_ANGLE = 45;
const int MAX_ANGLE = 135;
const int CENTER    = 90;
const int HOME_ANGLE = CENTER;   // where the rig returns after each section-only run

// Timing (ms)
const unsigned long PREP_MOVE_MS   = 1000;  // smooth move into section start position
const unsigned long PREP_SETTLE_MS = 500;   // still buffer before section begins
const unsigned long POST_HOLD_MS   = 500;   // still buffer after section ends
const unsigned long RETURN_HOME_MS = 1000;  // smooth return to HOME_ANGLE

const unsigned long SEG2_MS = 10000;  // moderate sweep only
const unsigned long SEG3_MS = 15000;  // jump + hold only
const unsigned long SEG4_MS = 8000;   // micro-adjustments only

// Sweep cycles for Section 2
const int SEG2_CYCLES = 2;

// Section 3 scripted positions: 15 unique holds ending at 90°
const int scriptedPositions[] = {
  52, 100, 67, 111, 59, 95, 76,
  98, 121, 63, 78, 100,
  69, 82, 90
};
const int NUM_SCRIPTED = sizeof(scriptedPositions) / sizeof(scriptedPositions[0]);
const unsigned long JUMP_HOLD_MS = 1000;

// Section 4 micro-adjustment pattern
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


// ------------------ TIMING ROADMAP ------------------
/*
SECTION 2 ONLY (BUTTON1)
0.0  - 1.0   PREP_MOVE to 45°
1.0  - 1.5   PREP_SETTLE at 45°
1.5  - 11.5  SECTION 2 sweep (10.0 s)
11.5 - 12.0  POST_HOLD at final angle
12.0 - 13.0  RETURN_HOME to 90°

SECTION 3 ONLY (BUTTON2)
0.0  - 1.0   PREP_MOVE to first hold target (52°)
1.0  - 1.5   PREP_SETTLE at 52°
1.5  - 16.5  SECTION 3 jump + hold (15.0 s)
16.5 - 17.0  POST_HOLD at final angle (ends at 90°)
17.0 - 18.0  RETURN_HOME to 90° (may already be there)

SECTION 4 ONLY (BUTTON1 + BUTTON2 together)
0.0  - 1.0   PREP_MOVE to 90°
1.0  - 1.5   PREP_SETTLE at 90°
1.5  - 9.5   SECTION 4 micro-adjustments (8.0 s)
9.5  - 10.0  POST_HOLD at final angle
10.0 - 11.0  RETURN_HOME to 90°
*/
// --------------------------------------------


enum Mode {
  MODE_NONE,
  MODE_SEG2,
  MODE_SEG3,
  MODE_SEG4
};

Mode selectedMode = MODE_NONE;

// State machine
// WAIT_FOR_SELECTION -> SELECT_WINDOW -> PREP_MOVE -> PREP_SETTLE -> RUN_SECTION -> POST_HOLD -> RETURN_HOME -> WAIT_FOR_SELECTION

enum Phase {
  WAIT_FOR_SELECTION,
  SELECT_WINDOW,
  PREP_MOVE,
  PREP_SETTLE,
  RUN_SEG2,
  RUN_SEG3,
  RUN_SEG4,
  POST_HOLD,
  RETURN_HOME
};

Phase phase = WAIT_FOR_SELECTION;
unsigned long phaseStart = 0;
unsigned long selectWindowStart = 0;

int currentAngle = HOME_ANGLE;
int transitionStartAngle = HOME_ANGLE;
int sectionStartAngle = HOME_ANGLE;
int postHoldAngle = HOME_ANGLE;

int lastB1 = LOW;
int lastB2 = LOW;

// Section 3 state
int seg3Index = 0;
unsigned long lastJumpChange = 0;

// Section 4 state
int seg4Index = 0;
unsigned long lastMicroUpdate = 0;


// ---------- Helpers ----------
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

void microAdjust() {
  if (millis() - lastMicroUpdate < MICRO_UPDATE_MS) return;
  lastMicroUpdate = millis();

  int target = CENTER + microAdjustPattern[seg4Index];
  target = constrain(target, CENTER - 5, CENTER + 5);
  writeAngle(target);

  seg4Index = (seg4Index + 1) % MICRO_LEN;
}

void enterPhase(Phase p) {
  phase = p;
  phaseStart = millis();

  if (p == PREP_MOVE || p == RETURN_HOME) {
    transitionStartAngle = currentAngle;
  }

  if (p == RUN_SEG3) {
    writeAngle(scriptedPositions[0]);
    seg3Index = 1;
    lastJumpChange = millis();
  }

  if (p == RUN_SEG4) {
    writeAngle(CENTER);
    seg4Index = 0;
    lastMicroUpdate = millis();
  }

  Serial.print("PHASE: ");
  Serial.print((int)phase);
  Serial.print(" @ ");
  Serial.println(phaseStart);
}

void startSelectedMode() {
  switch (selectedMode) {
    case MODE_SEG2:
      sectionStartAngle = MIN_ANGLE;
      Serial.println("Selected: SECTION 2");
      break;
    case MODE_SEG3:
      sectionStartAngle = scriptedPositions[0];
      Serial.println("Selected: SECTION 3");
      break;
    case MODE_SEG4:
      sectionStartAngle = CENTER;
      Serial.println("Selected: SECTION 4");
      break;
    default:
      sectionStartAngle = HOME_ANGLE;
      break;
  }
  enterPhase(PREP_MOVE);
}


// ---------- Setup ----------
void setup() {
  pinMode(BUTTON1_PIN, INPUT); // assumes external pulldown
  pinMode(BUTTON2_PIN, INPUT); // assumes external pulldown
  servo.attach(SERVO_PIN);

  Serial.begin(9600);
  Serial.println("Ready.");
  Serial.println("Press BUTTON1 for Section 2, BUTTON2 for Section 3, or both together for Section 4.");

  writeAngle(HOME_ANGLE);
  enterPhase(WAIT_FOR_SELECTION);
}


// ---------- Main Loop ----------
void loop() {
  int b1 = digitalRead(BUTTON1_PIN);
  int b2 = digitalRead(BUTTON2_PIN);

  switch (phase) {

    case WAIT_FOR_SELECTION: {
      // Start a short selection window when either button is first pressed
      if ((b1 == HIGH && lastB1 == LOW) || (b2 == HIGH && lastB2 == LOW)) {
        selectWindowStart = millis();
        enterPhase(SELECT_WINDOW);
      }
      break;
    }

    case SELECT_WINDOW: {
      // Wait briefly so pressing both buttons is easy to register
      if (millis() - selectWindowStart >= SELECT_WINDOW_MS) {
        if (b1 == HIGH && b2 == HIGH) {
          selectedMode = MODE_SEG4;
          startSelectedMode();
        } else if (b1 == HIGH && b2 == LOW) {
          selectedMode = MODE_SEG2;
          startSelectedMode();
        } else if (b1 == LOW && b2 == HIGH) {
          selectedMode = MODE_SEG3;
          startSelectedMode();
        } else {
          // buttons released / ambiguous -> go back to waiting
          selectedMode = MODE_NONE;
          enterPhase(WAIT_FOR_SELECTION);
        }
      }
      break;
    }

    case PREP_MOVE: {
      unsigned long t = millis() - phaseStart;
      if (t >= PREP_MOVE_MS) {
        writeAngle(sectionStartAngle);
        enterPhase(PREP_SETTLE);
      } else {
        writeAngle(rampAngle(transitionStartAngle, sectionStartAngle, t, PREP_MOVE_MS));
      }
      break;
    }

    case PREP_SETTLE: {
      writeAngle(sectionStartAngle);
      if (millis() - phaseStart >= PREP_SETTLE_MS) {
        if (selectedMode == MODE_SEG2) enterPhase(RUN_SEG2);
        else if (selectedMode == MODE_SEG3) enterPhase(RUN_SEG3);
        else if (selectedMode == MODE_SEG4) enterPhase(RUN_SEG4);
      }
      break;
    }

    case RUN_SEG2: {
      int a = sweepAngle(SEG2_MS, SEG2_CYCLES);
      writeAngle(a);
      if (millis() - phaseStart >= SEG2_MS) {
        postHoldAngle = currentAngle;
        enterPhase(POST_HOLD);
      }
      break;
    }

    case RUN_SEG3: {
      unsigned long elapsed = millis() - phaseStart;
      if (elapsed >= SEG3_MS) {
        postHoldAngle = currentAngle;
        enterPhase(POST_HOLD);
        break;
      }

      if (millis() - lastJumpChange >= JUMP_HOLD_MS) {
        lastJumpChange = millis();
        int target;
        if (seg3Index < NUM_SCRIPTED) {
          target = scriptedPositions[seg3Index];
          seg3Index++;
        } else {
          target = scriptedPositions[NUM_SCRIPTED - 1];
        }
        writeAngle(target);
      }
      break;
    }

    case RUN_SEG4: {
      unsigned long elapsed = millis() - phaseStart;
      if (elapsed >= SEG4_MS) {
        postHoldAngle = currentAngle;
        enterPhase(POST_HOLD);
        break;
      }
      microAdjust();
      break;
    }

    case POST_HOLD: {
      writeAngle(postHoldAngle);
      if (millis() - phaseStart >= POST_HOLD_MS) {
        enterPhase(RETURN_HOME);
      }
      break;
    }

    case RETURN_HOME: {
      unsigned long t = millis() - phaseStart;
      if (t >= RETURN_HOME_MS) {
        writeAngle(HOME_ANGLE);
        selectedMode = MODE_NONE;
        enterPhase(WAIT_FOR_SELECTION);
      } else {
        writeAngle(rampAngle(transitionStartAngle, HOME_ANGLE, t, RETURN_HOME_MS));
      }
      break;
    }

    default:
      break;
  }

  lastB1 = b1;
  lastB2 = b2;
}
