#include <Servo.h>

Servo servo;

// ------------------ CONFIG ------------------
const int SERVO_PIN  = 9;
const int BUTTON_PIN = 2;

// Angles
const int MIN_ANGLE = 45;
const int MAX_ANGLE = 135;
const int CENTER    = 90;

// Timing (ms)
const unsigned long SYNC_DURATION_MS   = 2000;
const unsigned long SYNC_PAUSE_MS      = 1000;
const unsigned long MOVE_TO_START_MS   = 1000;

const unsigned long SEG1_MS = 15000;  // slow sweep
const unsigned long SEG2_MS = 10000;  // faster sweep
const unsigned long SEG3_MS = 15000;  // jump + hold
const unsigned long SEG4_MS = 8000;   // jitter

// Sweep cycles (back-and-forth counts)
const int SEG1_CYCLES = 2;   // adjust if you want more/less rotations
const int SEG2_CYCLES = 4;

// Segment 3 scripted “random-looking” positions
const int scriptedPositions[] = {
  52, 128, 67, 111, 59, 134, 76,
  98, 121, 63, 110, 85
};
const int NUM_SCRIPTED = sizeof(scriptedPositions) / sizeof(scriptedPositions[0]);
const unsigned long JUMP_HOLD_MS = 1000; // hold each jump target

// Segment 4 jitter pattern
const int jitterPattern[] = { -6, 4, -3, 5, -2, 3 };
const int JITTER_LEN = sizeof(jitterPattern) / sizeof(jitterPattern[0]);
const unsigned long JITTER_UPDATE_MS = 70;
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
  SEG4_JITTER,
  DONE
};

Phase phase = WAIT_FOR_BUTTON;
unsigned long phaseStart = 0;

int currentAngle = CENTER;
int lastButtonState = LOW;

// For SEG3 + SEG4 indexing
int seg3Index = 0;
unsigned long lastJumpChange = 0;

int seg4Index = 0;
unsigned long lastJitterUpdate = 0;

// For sync signal pattern
int syncIndex = 0;
unsigned long lastSyncStep = 0;
const unsigned long SYNC_STEP_MS = 150;
// “Distinct” sync pattern centered around 90
const int syncPattern[] = { 90, 120, 60, 110, 70, 90 };
const int SYNC_LEN = sizeof(syncPattern) / sizeof(syncPattern[0]);

// ---------- Helpers ----------
void enterPhase(Phase p) {
  phase = p;
  phaseStart = millis();

  // Optional debug prints (helps validate timing)
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

void rampToAngle(int target, unsigned long durationMs) {
  // Linear interpolation based on elapsed time in this phase
  unsigned long t = millis() - phaseStart;
  if (t >= durationMs) {
    writeAngle(target);
    return;
  }
  float u = (float)t / (float)durationMs;
  int a = (int)(currentAngle + (target - currentAngle) * u);
  writeAngle(a);
}

int sweepAngle(unsigned long sectionDurationMs, int numCycles) {
  unsigned long t = millis() - phaseStart;
  if (t >= sectionDurationMs) return MIN_ANGLE;

  float progress = (float)t / (float)sectionDurationMs;

  float span = (float)(MAX_ANGLE - MIN_ANGLE);
  float totalTravel = numCycles * 2.0f * span;  // back-and-forth distance
  float traveled = progress * totalTravel;

  float phasePos = fmod(traveled, 2.0f * span);

  if (phasePos <= span) {
    return (int)(MIN_ANGLE + phasePos);            // ascending
  } else {
    return (int)(MAX_ANGLE - (phasePos - span));   // descending
  }
}

// ---------- Arduino Setup ----------
void setup() {
  pinMode(BUTTON_PIN, INPUT); // external pulldown
  servo.attach(SERVO_PIN);

  Serial.begin(9600);
  Serial.println("Ready. Servo parked at 90°. Start HoloLens, then press button ~3s later.");

  writeAngle(CENTER); // park at 90 so sync signature is obvious
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
      syncIndex = 0;
      lastSyncStep = 0;
      seg3Index = 0;
      lastJumpChange = 0;
      seg4Index = 0;
      lastJitterUpdate = 0;

      // Start sync immediately
      enterPhase(SYNC_SIGNAL);
    }
    lastButtonState = buttonState;
    return;
  }

  lastButtonState = buttonState;

  // Phase machine
  switch (phase) {

    case SYNC_SIGNAL: {
      unsigned long elapsed = millis() - phaseStart;

      // step through pattern at fixed cadence
      if (millis() - lastSyncStep >= SYNC_STEP_MS) {
        lastSyncStep = millis();
        writeAngle(syncPattern[syncIndex]);
        syncIndex = (syncIndex + 1) % SYNC_LEN;
      }

      if (elapsed >= SYNC_DURATION_MS) {
        writeAngle(CENTER);
        enterPhase(SYNC_PAUSE);
      }
      break;
    }

    case SYNC_PAUSE: {
      // hold at 90 for 1s
      writeAngle(CENTER);
      if (millis() - phaseStart >= SYNC_PAUSE_MS) {
        // begin transition toward 45
        // NOTE: capture starting angle for ramp: we set currentAngle to 90 above
        enterPhase(MOVE_TO_START);
      }
      break;
    }

    case MOVE_TO_START: {
      // Smooth deterministic ramp to 45 over 1s budget
      // (Servo may physically lag; this is still a consistent command schedule)
      unsigned long t = millis() - phaseStart;
      if (t >= MOVE_TO_START_MS) {
        writeAngle(MIN_ANGLE);
        enterPhase(SEG1_SWEEP);
      } else {
        // linearly ramp from 90 to 45
        float u = (float)t / (float)MOVE_TO_START_MS;
        int a = (int)(CENTER + (MIN_ANGLE - CENTER) * u);
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
        enterPhase(SEG4_JITTER);
        break;
      }

      // change target every 1s
      if (millis() - lastJumpChange >= JUMP_HOLD_MS) {
        lastJumpChange = millis();
        int target = scriptedPositions[seg3Index];
        writeAngle(target);
        seg3Index = (seg3Index + 1) % NUM_SCRIPTED;
      }
      break;
    }

    case SEG4_JITTER: {
      unsigned long elapsed = millis() - phaseStart;

      if (elapsed >= SEG4_MS) {
        enterPhase(DONE);
        break;
      }

      if (millis() - lastJitterUpdate >= JITTER_UPDATE_MS) {
        lastJitterUpdate = millis();
        int target = CENTER + jitterPattern[seg4Index];
        target = constrain(target, CENTER - 15, CENTER + 15);
        writeAngle(target);
        seg4Index = (seg4Index + 1) % JITTER_LEN;
      }
      break;
    }

    case DONE: {
      Serial.println("Sequence complete. Parking at 90°.");
      writeAngle(CENTER);
      enterPhase(WAIT_FOR_BUTTON);
      break;
    }

    default:
      break;
  }
}
