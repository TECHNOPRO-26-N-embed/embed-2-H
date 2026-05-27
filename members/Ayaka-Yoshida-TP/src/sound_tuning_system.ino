#include <math.h>

#ifndef ARDUINO
// VS Code C/C++ extension may parse this file without Arduino core.
// These stubs are for editor diagnostics only and are ignored on Arduino builds.
#define HIGH 0x1
#define LOW  0x0
#define INPUT 0x0
#define INPUT_PULLUP 0x2
#define OUTPUT 0x1
#define CHANGE 0x2
#define A0 14

struct SerialStub {
  void begin(unsigned long) {}
  template <typename T>
  void print(const T&) {}
  template <typename T>
  void println(const T&) {}
  void println() {}
};

static SerialStub Serial;

inline void pinMode(int, int) {}
inline int digitalRead(int) { return HIGH; }
inline void digitalWrite(int, int) {}
inline int analogRead(int) { return 0; }
inline int digitalPinToInterrupt(int pin) { return pin; }
inline void attachInterrupt(int, void (*)(), int) {}
inline void delay(unsigned long) {}
inline unsigned long millis() {
  static unsigned long fake = 0;
  fake += 10;
  return fake;
}
inline long map(long x, long in_min, long in_max, long out_min, long out_max) {
  if (in_max == in_min) {
    return out_min;
  }
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}
inline void tone(int, unsigned int, unsigned long = 0) {}
inline void noTone(int) {}
#endif

#define COV_CAT2(a, b) a##b
#define COV_CAT(a, b) COV_CAT2(a, b)
#define TRACE_BRANCH(msg)                                                   \
  do {                                                                      \
    static bool COV_CAT(_cov_once_, __LINE__) = false;                     \
    if (!COV_CAT(_cov_once_, __LINE__)) {                                 \
      Serial.println(msg);                                                  \
      COV_CAT(_cov_once_, __LINE__) = true;                       \
    }                                                                       \
  } while (0)

// Pin definitions
const int PIN_ENC_CLK = 2;
const int PIN_ENC_DT = 3;
const int PIN_ENC_SW = 4;
const int PIN_MODE_BUTTON = 5;
const int PIN_LED_GREEN = 6;
const int PIN_LED_RED = 7;
const int PIN_BUZZER = 9;
const int PIN_POT = A0;

// State definitions
const int STATE_IDLE = 0;
const int STATE_SETTING = 1;
const int STATE_PLAYING = 2;
const int STATE_ERROR = 3;
const int STATE_VOLUME_ADJUST = 4;

// Timing constants
const unsigned long ENCODER_INTERVAL_MS = 10;
const unsigned long ENCODER_STEP_GUARD_MS = 25;
const unsigned long ENCODER_NOISE_GUARD_MS = 2;
const unsigned long SOUND_INTERVAL_MS = 50;
const unsigned long POT_INTERVAL_MS = 50;
const unsigned long LED_INTERVAL_MS = 100;
const unsigned long LOOP_LOG_INTERVAL_MS = 500;
const unsigned long LONG_PRESS_MS = 1500;
const unsigned long IDLE_TIMEOUT_MS = 5000;

// Parameter constants
const int DEBOUNCE_DELAY = 50;
const int PITCH_MIN = -10;
const int PITCH_MAX = 10;
const int VOLUME_MIN = 0;
const int VOLUME_MAX = 40;
const int MELODY_MIN = 0;
const int MELODY_MAX = 1;
const int FREQ_MIN_HZ = 131;
const int FREQ_MAX_HZ = 1047;
const int BASE_FREQ_HZ = 262;
const int MELODY_LEN = 5;
const int NOTE_HOLD_TICKS = 4;
const unsigned long BUTTON_BEEP_MS = 120;
const bool MODE_BUTTON_ACTIVE_LOW = true;

// Core globals from the detailed design
int currentState = STATE_IDLE;
int previousState = STATE_IDLE;

unsigned long nowMs = 0;
unsigned long lastMillis_Enc = 0;
unsigned long lastMillis_Btn = 0;
unsigned long lastMillis_Sound = 0;
unsigned long lastMillis_Pot = 0;
unsigned long lastMillis_Led = 0;

int encoderStep = 0;
bool modeButtonState = false;
int potRaw = 0;

int pitchIndex = 0;
int volumeLevel = 8;
int melodyMode = 0;
int targetFreqHz = BASE_FREQ_HZ;

bool errorFlag = false;
int errorCode = 0;
unsigned long lastDebounceTimeMode = 0;
unsigned long lastDebounceTimeEnc = 0;
int lastButtonStateMode = HIGH;
int lastButtonStateEnc = HIGH;
int buttonReadingMode = HIGH;

bool ledStateGreen = false;
bool ledStateRed = false;

unsigned long lastInputMs = 0;
unsigned long buttonGreenUntilMs = 0;
unsigned long buttonBeepUntilMs = 0;
bool buttonLedToggledOn = false;
bool encoderJumpDetected = false;
int prevEncClk = HIGH;
unsigned long lastEncoderAcceptedMs = 0;
unsigned long lastEncoderEdgeMs = 0;
int encoderBurst_count = 0;

bool isToneActive = false;
int melodyNoteIndex = 0;
int melodyTickCount = 0;

// --- Rotary Encoder Interrupt Logic ---
const int CLK_PIN = 2; // CLK をデジタルピン 2 に接続
const int DT_PIN = 3;  // DT をデジタルピン 3 に接続
volatile int encoderPosCount = 0;
volatile int prevState = 3;
int directionArray[4][4] = {
    {0, -1, 1, 0},
    {1, 0, 0, -1},
    {-1, 0, 0, 1},
    {0, 1, -1, 0}};
volatile int directionSum = 0;

void onChanged() {
  int clkVal = digitalRead(CLK_PIN);
  int dtVal = digitalRead(DT_PIN);
  int state = (clkVal << 1) | dtVal;
  directionSum += directionArray[prevState][state];
  if (directionSum == 4) {
    encoderPosCount++;
    directionSum = 0;
  }
  if (directionSum == -4) {
    encoderPosCount--;
    directionSum = 0;
  }
  prevState = state;
}

void setup() {
  Serial.begin(9600);
  Serial.println("[setup] start initialization");

  pinMode(PIN_ENC_SW, INPUT_PULLUP);
  pinMode(PIN_MODE_BUTTON, INPUT_PULLUP);
  pinMode(PIN_ENC_CLK, INPUT_PULLUP);
  pinMode(PIN_ENC_DT, INPUT_PULLUP);
  pinMode(PIN_LED_GREEN, OUTPUT);
  pinMode(PIN_LED_RED, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);

  currentState = STATE_IDLE;
  previousState = STATE_IDLE;
  errorFlag = false;
  errorCode = 0;
  prevEncClk = digitalRead(PIN_ENC_CLK);
  lastButtonStateMode = digitalRead(PIN_MODE_BUTTON);
  buttonReadingMode = lastButtonStateMode;
  lastEncoderEdgeMs = millis();
  lastEncoderAcceptedMs = lastEncoderEdgeMs;
  lastInputMs = millis();

  // Use pull-ups to avoid floating interrupt inputs.
  pinMode(CLK_PIN, INPUT_PULLUP);
  pinMode(DT_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(CLK_PIN), onChanged, CHANGE);
  attachInterrupt(digitalPinToInterrupt(DT_PIN), onChanged, CHANGE);

  digitalWrite(PIN_LED_RED, LOW);
  digitalWrite(PIN_LED_GREEN, HIGH);
  delay(120);
  digitalWrite(PIN_LED_GREEN, LOW);

  // --- 追加: 電源ON時にブザーを鳴らす ---
  tone(PIN_BUZZER, 440, 500); // 440Hzで0.5秒鳴らす（A音）

  Serial.println("[setup] complete");
}

void loop() {
  nowMs = millis();

  static unsigned long lastLoopLog = 0;
  if (nowMs - lastLoopLog >= LOOP_LOG_INTERVAL_MS) {
    TRACE_BRANCH("[COV] loop:periodic-log:true");
    Serial.print("[loop] state=");
    Serial.print(currentState);
    Serial.print(" pitch=");
    Serial.print(pitchIndex);
    Serial.print(" freq=");
    Serial.print(targetFreqHz);
    Serial.print(" vol=");
    Serial.print(volumeLevel);
    Serial.print(" melody=");
    Serial.print(melodyMode);
    Serial.print(" toneActive=");
    Serial.println(isToneActive ? 1 : 0);
    lastLoopLog = nowMs;
  } else {
    TRACE_BRANCH("[COV] loop:periodic-log:false");
  }

  static int previousEncoderPosCount = 0;
  if (previousEncoderPosCount != encoderPosCount) {
    int diff = encoderPosCount - previousEncoderPosCount;
    updateSoundParams(diff); // 1回の回転で1だけ加減
    previousEncoderPosCount = encoderPosCount;
  }

  modeButtonState = handleButton();
  adjustVolume();

  if (checkError()) {
    TRACE_BRANCH("[COV] loop:checkError:true");
    currentState = STATE_ERROR;
  } else {
    TRACE_BRANCH("[COV] loop:checkError:false");
  }

  if (currentState != previousState) {
    TRACE_BRANCH("[COV] loop:state-transition:true");
    Serial.print("[loop] state transition ");
    Serial.print(previousState);
    Serial.print(" -> ");
    Serial.println(currentState);
    previousState = currentState;
  } else {
    TRACE_BRANCH("[COV] loop:state-transition:false");
  }

  switch (currentState) {
    case STATE_IDLE:
      TRACE_BRANCH("[COV] loop:switch:STATE_IDLE");
      showStatusLed(STATE_IDLE);
      if (nowMs >= buttonBeepUntilMs) {
        noTone(PIN_BUZZER);
        isToneActive = false;
      } else {
        isToneActive = true;
      }
      if (encoderStep != 0) {
        TRACE_BRANCH("[COV] loop:IDLE:encoderStep!=0:true");
        lastInputMs = nowMs;
        currentState = STATE_SETTING;
      } else if (modeButtonState) {
        TRACE_BRANCH("[COV] loop:IDLE:modeButtonState:true");
        lastInputMs = nowMs;
        currentState = STATE_PLAYING;
      } else {
        TRACE_BRANCH("[COV] loop:IDLE:no-input");
      }
      break;

    case STATE_SETTING:
      TRACE_BRANCH("[COV] loop:switch:STATE_SETTING");
      updateSoundParams(encoderStep);
      currentState = STATE_PLAYING;
      break;

    case STATE_PLAYING:
      TRACE_BRANCH("[COV] loop:switch:STATE_PLAYING");
      showStatusLed(STATE_PLAYING);
      if (nowMs - lastMillis_Sound >= SOUND_INTERVAL_MS) {
        TRACE_BRANCH("[COV] loop:PLAYING:sound-interval:true");
        lastMillis_Sound = nowMs;
        playCurrentSound();
      } else {
        TRACE_BRANCH("[COV] loop:PLAYING:sound-interval:false");
      }
      if (modeButtonState) {
        TRACE_BRANCH("[COV] loop:PLAYING:modeButtonState:true");
        lastInputMs = nowMs;
      } else {
        TRACE_BRANCH("[COV] loop:PLAYING:modeButtonState:false");
      }
      if (nowMs - lastInputMs > IDLE_TIMEOUT_MS) {
        TRACE_BRANCH("[COV] loop:PLAYING:idle-timeout:true");
        noTone(PIN_BUZZER);
        isToneActive = false;
        currentState = STATE_IDLE;
      } else {
        TRACE_BRANCH("[COV] loop:PLAYING:idle-timeout:false");
      }
      break;

    case STATE_ERROR:
      TRACE_BRANCH("[COV] loop:switch:STATE_ERROR");
      noTone(PIN_BUZZER);
      isToneActive = false;
      showStatusLed(STATE_ERROR);
      {
        int encSw = digitalRead(PIN_ENC_SW);
        if (encSw == LOW && (nowMs - lastDebounceTimeEnc) > LONG_PRESS_MS) {
          TRACE_BRANCH("[COV] loop:ERROR:recover:true");
          Serial.println("[loop] error recovery by encoder switch long press");
          errorFlag = false;
          errorCode = 0;
          encoderJumpDetected = false;
          currentState = STATE_IDLE;
          lastDebounceTimeEnc = nowMs;
        } else {
          TRACE_BRANCH("[COV] loop:ERROR:recover:false");
        }
      }
      break;

    case STATE_VOLUME_ADJUST:
      TRACE_BRANCH("[COV] loop:switch:STATE_VOLUME_ADJUST");
      showStatusLed(STATE_VOLUME_ADJUST);
      if (nowMs - lastMillis_Sound >= SOUND_INTERVAL_MS) {
        TRACE_BRANCH("[COV] loop:VOL_ADJ:sound-interval:true");
        lastMillis_Sound = nowMs;
        playCurrentSound();
      } else {
        TRACE_BRANCH("[COV] loop:VOL_ADJ:sound-interval:false");
      }
      currentState = STATE_PLAYING;
      break;

    default:
      TRACE_BRANCH("[COV] loop:switch:default");
      Serial.println("[loop] invalid state fallback to error");
      currentState = STATE_ERROR;
      break;
  }
}

int readEncoder() {
  if (nowMs - lastMillis_Enc < ENCODER_INTERVAL_MS) {
    TRACE_BRANCH("[COV] readEncoder:interval-skip:true");
    return 0;
  }
  TRACE_BRANCH("[COV] readEncoder:interval-skip:false");
  lastMillis_Enc = nowMs;

  static int lastClk = HIGH;
  int clk = digitalRead(PIN_ENC_CLK);
  int dt = digitalRead(PIN_ENC_DT);
  int step = 0;

  // Only count on falling edge of CLK
  if (lastClk == HIGH && clk == LOW) {
    if (dt == HIGH) {
      step = 1;
    } else {
      step = -1;
    }
    TRACE_BRANCH("[COV] readEncoder:detent:true");
    Serial.print("[readEncoder] step=");
    Serial.println(step);
  } else {
    TRACE_BRANCH("[COV] readEncoder:detent:false");
  }
  lastClk = clk;
  return step;
}

bool handleButton() {
  bool switched = false;
  int reading = digitalRead(PIN_MODE_BUTTON);
  int pressedLevel = MODE_BUTTON_ACTIVE_LOW ? LOW : HIGH;

  // Debounce based on raw reading changes first.
  if (reading != buttonReadingMode) {
    TRACE_BRANCH("[COV] handleButton:reading-changed:true");
    lastDebounceTimeMode = nowMs;
    buttonReadingMode = reading;
  } else {
    TRACE_BRANCH("[COV] handleButton:reading-changed:false");
  }

  if ((nowMs - lastDebounceTimeMode) > DEBOUNCE_DELAY) {
    TRACE_BRANCH("[COV] handleButton:debounce-ok:true");
    // Update stable state only after debounce window.
    if (buttonReadingMode != lastButtonStateMode) {
      // Trigger only when stable state moves into pressed level.
      if (lastButtonStateMode != pressedLevel && buttonReadingMode == pressedLevel) {
        TRACE_BRANCH("[COV] handleButton:falling-edge:true");
        changeMelodyPattern();
        switched = true;
        buttonLedToggledOn = !buttonLedToggledOn;
        buttonGreenUntilMs = nowMs + 300;
        buttonBeepUntilMs = nowMs + BUTTON_BEEP_MS;
        tone(PIN_BUZZER, targetFreqHz, BUTTON_BEEP_MS);
        isToneActive = true;
        Serial.println(5555);
        Serial.println("音色が変更されました"); // Display message in Japanese
        Serial.print("melodyMode=");
        Serial.println(melodyMode);
      } else {
        TRACE_BRANCH("[COV] handleButton:falling-edge:false");
      }
      lastButtonStateMode = buttonReadingMode;
    } else {
      TRACE_BRANCH("[COV] handleButton:stable-nochange");
    }
  } else {
    TRACE_BRANCH("[COV] handleButton:debounce-ok:false");
  }
  return switched;
}

void updateSoundParams(int step) {
  Serial.print("[updateSoundParams] step=");
  Serial.println(step);

  if (step == 0) {
    TRACE_BRANCH("[COV] updateSoundParams:step==0:true");
    return;
  } else {
    TRACE_BRANCH("[COV] updateSoundParams:step==0:false");
  }

  // Prevent incrementing above max, but allow decrementing from max
  if (step > 0 && pitchIndex >= PITCH_MAX) {
    TRACE_BRANCH("[COV] updateSoundParams:inc-at-max:true");
    return;
  }
  // Prevent decrementing below min, but allow incrementing from min
  if (step < 0 && pitchIndex <= PITCH_MIN) {
    TRACE_BRANCH("[COV] updateSoundParams:dec-at-min:true");
    return;
  }

  pitchIndex += step;

  float ratio = pow(2.0, pitchIndex / 12.0);
  targetFreqHz = (int)(BASE_FREQ_HZ * ratio + 0.5);
  Serial.print("[updateSoundParams] targetFreqHz=");
  Serial.println(targetFreqHz);

  if (targetFreqHz < FREQ_MIN_HZ || targetFreqHz > FREQ_MAX_HZ) {
    TRACE_BRANCH("[COV] updateSoundParams:freq-out:true");
    errorFlag = true;
    errorCode = 1;
    Serial.println("[updateSoundParams] frequency out of range");
  } else {
    TRACE_BRANCH("[COV] updateSoundParams:freq-out:false");
  }
  lastInputMs = nowMs;
}

void changeMelodyPattern() {
  Serial.println("[changeMelodyPattern] toggle melody pattern");

  melodyMode++;
  if (melodyMode > MELODY_MAX) {
    TRACE_BRANCH("[COV] changeMelodyPattern:melody-wrap:true");
    melodyMode = MELODY_MIN;
  } else {
    TRACE_BRANCH("[COV] changeMelodyPattern:melody-wrap:false");
  }
  melodyNoteIndex = 0;
  melodyTickCount = 0;

  Serial.print("[changeMelodyPattern] current melodyMode=");
  Serial.println(melodyMode);
}

void saveSoundPattern() {
  Serial.println("[saveSoundPattern] stub (not implemented in this version)");
}

void loadSoundPattern() {
  Serial.println("[loadSoundPattern] stub (not implemented in this version)");
}

void playCurrentSound() {
  if (melodyMode == 0) {
    TRACE_BRANCH("[COV] playCurrentSound:melodyMode==0:true");
    tone(PIN_BUZZER, targetFreqHz);
    digitalWrite(PIN_LED_GREEN, HIGH); // Turn on green LED
  } else if (melodyMode == 1) {
    TRACE_BRANCH("[COV] playCurrentSound:melodyMode==1:true");
    if (melodyNoteIndex < MELODY_LEN) {
      TRACE_BRANCH("[COV] playCurrentSound:melodyNoteIndex<MELODY_LEN:true");
      tone(PIN_BUZZER, targetFreqHz, NOTE_HOLD_TICKS * SOUND_INTERVAL_MS);
      digitalWrite(PIN_LED_GREEN, HIGH); // Turn on green LED
      melodyTickCount++;
      if (melodyTickCount >= NOTE_HOLD_TICKS) {
        TRACE_BRANCH("[COV] playCurrentSound:melodyTickCount>=NOTE_HOLD_TICKS:true");
        melodyTickCount = 0;
        melodyNoteIndex++;
      } else {
        TRACE_BRANCH("[COV] playCurrentSound:melodyTickCount>=NOTE_HOLD_TICKS:false");
      }
    } else {
      TRACE_BRANCH("[COV] playCurrentSound:melodyNoteIndex<MELODY_LEN:false");
      noTone(PIN_BUZZER);
      digitalWrite(PIN_LED_GREEN, LOW); // Turn off green LED
      melodyNoteIndex = 0;
    }
  } else {
    TRACE_BRANCH("[COV] playCurrentSound:melodyMode==0:false");
    noTone(PIN_BUZZER);
    digitalWrite(PIN_LED_GREEN, LOW); // Turn off green LED
  }

  static unsigned long lastPlayLog = 0;
  if (nowMs - lastPlayLog >= 250) {
    TRACE_BRANCH("[COV] playCurrentSound:periodic-log:true");
    Serial.print("[playCurrentSound] freq=");
    Serial.print(targetFreqHz);
    Serial.print(" note=");
    Serial.print(melodyNoteIndex);
    Serial.print(" volume=");
    Serial.print(volumeLevel);
    Serial.print(" melody=");
    Serial.println(melodyMode);
    lastPlayLog = nowMs;
  } else {
    TRACE_BRANCH("[COV] playCurrentSound:periodic-log:false");
  }
}

bool checkError() {
  bool detected = false;

  if (pitchIndex < PITCH_MIN || pitchIndex > PITCH_MAX) {
    TRACE_BRANCH("[COV] checkError:pitch-out:true");
    errorCode = 10;
    detected = true;
  } else {
    TRACE_BRANCH("[COV] checkError:pitch-out:false");
  }
  if (volumeLevel < VOLUME_MIN || volumeLevel > VOLUME_MAX) {
    TRACE_BRANCH("[COV] checkError:volume-out:true");
    errorCode = 11;
    detected = true;
  } else {
    TRACE_BRANCH("[COV] checkError:volume-out:false");
  }
  if (targetFreqHz < FREQ_MIN_HZ || targetFreqHz > FREQ_MAX_HZ) {
    TRACE_BRANCH("[COV] checkError:freq-out:true");
    errorCode = 12;
    detected = true;
  } else {
    TRACE_BRANCH("[COV] checkError:freq-out:false");
  }
  if (encoderJumpDetected) {
    TRACE_BRANCH("[COV] checkError:encoder-jump:true");
    errorCode = 13;
    detected = true;
    encoderJumpDetected = false;
  } else {
    TRACE_BRANCH("[COV] checkError:encoder-jump:false");
  }
  if (melodyMode < MELODY_MIN || melodyMode > MELODY_MAX) {
    TRACE_BRANCH("[COV] checkError:melody-out:true");
    errorCode = 14;
    detected = true;
  } else {
    TRACE_BRANCH("[COV] checkError:melody-out:false");
  }

  if (detected) {
    TRACE_BRANCH("[COV] checkError:detected:true");
    errorFlag = true;
    Serial.print("[checkError] error detected code=");
    Serial.println(errorCode);
  } else {
    TRACE_BRANCH("[COV] checkError:detected:false");
  }

  if (detected || errorFlag) {
    TRACE_BRANCH("[COV] checkError:return:true");
    return true;
  }
  TRACE_BRANCH("[COV] checkError:return:false");
  return false;
}

void showStatusLed(int state) {
  if (nowMs - lastMillis_Led < LED_INTERVAL_MS) {
    TRACE_BRANCH("[COV] showStatusLed:interval-skip:true");
    return;
  }
  TRACE_BRANCH("[COV] showStatusLed:interval-skip:false");
  lastMillis_Led = nowMs;

  switch (state) {
    case STATE_IDLE:
      TRACE_BRANCH("[COV] showStatusLed:switch:STATE_IDLE");
      ledStateGreen = isToneActive;
      ledStateRed = !isToneActive;
      break;
    case STATE_SETTING:
      TRACE_BRANCH("[COV] showStatusLed:switch:STATE_SETTING");
      ledStateGreen = isToneActive;
      ledStateRed = !isToneActive;
      break;
    case STATE_PLAYING:
      TRACE_BRANCH("[COV] showStatusLed:switch:STATE_PLAYING");
      ledStateGreen = isToneActive;
      ledStateRed = !isToneActive;
      break;
    case STATE_VOLUME_ADJUST:
      TRACE_BRANCH("[COV] showStatusLed:switch:STATE_VOLUME_ADJUST");
      ledStateGreen = isToneActive;
      ledStateRed = !isToneActive;
      break;
    case STATE_ERROR:
      TRACE_BRANCH("[COV] showStatusLed:switch:STATE_ERROR");
      ledStateGreen = false;
      ledStateRed = true;
      break;
    default:
      TRACE_BRANCH("[COV] showStatusLed:switch:default");
      ledStateGreen = false;
      ledStateRed = true;
      break;
  }

  // Follow button toggle state: press once ON, press again OFF.
  ledStateGreen = buttonLedToggledOn;
  ledStateRed = false;

  digitalWrite(PIN_LED_GREEN, ledStateGreen ? HIGH : LOW);
  digitalWrite(PIN_LED_RED, ledStateRed ? HIGH : LOW);

  static int lastLoggedState = -1;
  if (lastLoggedState != state) {
    TRACE_BRANCH("[COV] showStatusLed:state-changed:true");
    Serial.print("[showStatusLed] state=");
    Serial.println(state);
    lastLoggedState = state;
  } else {
    TRACE_BRANCH("[COV] showStatusLed:state-changed:false");
  }
}

void adjustVolume() {
  if (nowMs - lastMillis_Pot < POT_INTERVAL_MS) {
    TRACE_BRANCH("[COV] adjustVolume:interval-skip:true");
    return;
  }
  TRACE_BRANCH("[COV] adjustVolume:interval-skip:false");
  lastMillis_Pot = nowMs;

  static int lastMappedVolume = 0;
  potRaw = analogRead(PIN_POT);
  int mappedVolume = map(potRaw, 0, 1023, VOLUME_MIN, VOLUME_MAX);

  // Ensure the volume changes by 1 step per significant movement
  if (mappedVolume > lastMappedVolume) {
    mappedVolume = lastMappedVolume + 1;
  } else if (mappedVolume < lastMappedVolume) {
    mappedVolume = lastMappedVolume - 1;
  }

  if (mappedVolume < VOLUME_MIN) {
    TRACE_BRANCH("[COV] adjustVolume:mapped<min:true");
    mappedVolume = VOLUME_MIN;
  } else {
    TRACE_BRANCH("[COV] adjustVolume:mapped<min:false");
  }
  if (mappedVolume > VOLUME_MAX) {
    TRACE_BRANCH("[COV] adjustVolume:mapped>max:true");
    mappedVolume = VOLUME_MAX;
  } else {
    TRACE_BRANCH("[COV] adjustVolume:mapped>max:false");
  }

  if (mappedVolume != volumeLevel) {
    TRACE_BRANCH("[COV] adjustVolume:value-changed:true");
    volumeLevel = mappedVolume;
    lastMappedVolume = mappedVolume;
    Serial.print("volume：");
    Serial.println(volumeLevel);

    if (currentState != STATE_ERROR) {
      TRACE_BRANCH("[COV] adjustVolume:not-error-state:true");
      currentState = STATE_VOLUME_ADJUST;
      lastInputMs = nowMs;
    } else {
      TRACE_BRANCH("[COV] adjustVolume:not-error-state:false");
    }
  } else {
    TRACE_BRANCH("[COV] adjustVolume:value-changed:false");
  }

  static unsigned long lastPotLog = 0;
  if (nowMs - lastPotLog >= 1000) {
    TRACE_BRANCH("[COV] adjustVolume:periodic-log:true");
    Serial.print("volume：");
    Serial.println(volumeLevel);
    lastPotLog = nowMs;
  } else {
    TRACE_BRANCH("[COV] adjustVolume:periodic-log:false");
  }
}