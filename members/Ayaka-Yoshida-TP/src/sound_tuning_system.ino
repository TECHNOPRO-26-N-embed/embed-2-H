#include <math.h>

#ifndef ARDUINO
// VS Code C/C++ extension may parse this file without Arduino core.
// These stubs are for editor diagnostics only and are ignored on Arduino builds.
#define HIGH 0x1
#define LOW  0x0
#define INPUT_PULLUP 0x2
#define OUTPUT 0x1
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
const unsigned long SOUND_INTERVAL_MS = 50;
const unsigned long POT_INTERVAL_MS = 50;
const unsigned long LED_INTERVAL_MS = 100;
const unsigned long LOOP_LOG_INTERVAL_MS = 500;
const unsigned long LONG_PRESS_MS = 1500;
const unsigned long IDLE_TIMEOUT_MS = 5000;

// Parameter constants
const int DEBOUNCE_DELAY = 50;
const int PITCH_MIN = 0;
const int PITCH_MAX = 24;
const int VOLUME_MIN = 0;
const int VOLUME_MAX = 10;
const int TIMBRE_MIN = 0;
const int TIMBRE_MAX = 2;
const int FREQ_MIN_HZ = 131;
const int FREQ_MAX_HZ = 1047;
const int BASE_FREQ_HZ = 262;

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

int pitchIndex = 12;
int volumeLevel = 8;
int timbreMode = 0;
int targetFreqHz = 523;

bool errorFlag = false;
int errorCode = 0;
unsigned long lastDebounceTimeMode = 0;
unsigned long lastDebounceTimeEnc = 0;
int lastButtonStateMode = HIGH;
int lastButtonStateEnc = HIGH;

bool ledStateGreen = false;
bool ledStateRed = false;

unsigned long lastInputMs = 0;
bool encoderJumpDetected = false;
int prevEncClk = HIGH;

// Function declarations
int readEncoder();
bool handleButton();
void updateSoundParams(int step);
void changeMelodyPattern();
void saveSoundPattern();
void loadSoundPattern();
void playCurrentSound();
bool checkError();
void showStatusLed(int state);
void adjustVolume();

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
  lastInputMs = millis();

  digitalWrite(PIN_LED_RED, LOW);
  digitalWrite(PIN_LED_GREEN, HIGH);
  delay(120);
  digitalWrite(PIN_LED_GREEN, LOW);

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
    Serial.print(" timbre=");
    Serial.println(timbreMode);
    lastLoopLog = nowMs;
  } else {
    TRACE_BRANCH("[COV] loop:periodic-log:false");
  }

  encoderStep = readEncoder();
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
      noTone(PIN_BUZZER);
      if (encoderStep != 0) {
        TRACE_BRANCH("[COV] loop:IDLE:encoderStep!=0:true");
        lastInputMs = nowMs;
        currentState = STATE_SETTING;
      } else if (modeButtonState) {
        TRACE_BRANCH("[COV] loop:IDLE:modeButtonState:true");
        lastInputMs = nowMs;
        changeMelodyPattern();
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
        changeMelodyPattern();
      } else {
        TRACE_BRANCH("[COV] loop:PLAYING:modeButtonState:false");
      }
      if (nowMs - lastInputMs > IDLE_TIMEOUT_MS) {
        TRACE_BRANCH("[COV] loop:PLAYING:idle-timeout:true");
        noTone(PIN_BUZZER);
        currentState = STATE_IDLE;
      } else {
        TRACE_BRANCH("[COV] loop:PLAYING:idle-timeout:false");
      }
      break;

    case STATE_ERROR:
      TRACE_BRANCH("[COV] loop:switch:STATE_ERROR");
      noTone(PIN_BUZZER);
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

  int clk = digitalRead(PIN_ENC_CLK);
  int dt = digitalRead(PIN_ENC_DT);
  int step = 0;

  // Count only on CLK edge to avoid double count.
  if (clk != prevEncClk) {
    TRACE_BRANCH("[COV] readEncoder:clk-changed:true");
    if (clk == LOW) {
      TRACE_BRANCH("[COV] readEncoder:clk-low:true");
      step = (dt != clk) ? 1 : -1;
    } else {
      TRACE_BRANCH("[COV] readEncoder:clk-low:false");
    }
    prevEncClk = clk;
  } else {
    TRACE_BRANCH("[COV] readEncoder:clk-changed:false");
  }

  if (step != 0) {
    TRACE_BRANCH("[COV] readEncoder:step!=0:true");
    Serial.print("[readEncoder] step=");
    Serial.println(step);
  } else {
    TRACE_BRANCH("[COV] readEncoder:step!=0:false");
  }

  return step;
}

bool handleButton() {
  bool switched = false;
  int reading = digitalRead(PIN_MODE_BUTTON);

  if (reading != lastButtonStateMode) {
    TRACE_BRANCH("[COV] handleButton:reading-changed:true");
    lastDebounceTimeMode = nowMs;
  } else {
    TRACE_BRANCH("[COV] handleButton:reading-changed:false");
  }

  if ((nowMs - lastDebounceTimeMode) > DEBOUNCE_DELAY) {
    TRACE_BRANCH("[COV] handleButton:debounce-ok:true");
    if (lastButtonStateMode == HIGH && reading == LOW) {
      TRACE_BRANCH("[COV] handleButton:falling-edge:true");
      timbreMode++;
      if (timbreMode > TIMBRE_MAX) {
        TRACE_BRANCH("[COV] handleButton:timbre-wrap:true");
        timbreMode = TIMBRE_MIN;
      } else {
        TRACE_BRANCH("[COV] handleButton:timbre-wrap:false");
      }
      switched = true;
      Serial.print("[handleButton] timbre switched to ");
      Serial.println(timbreMode);
    } else {
      TRACE_BRANCH("[COV] handleButton:falling-edge:false");
    }
  } else {
    TRACE_BRANCH("[COV] handleButton:debounce-ok:false");
  }

  lastButtonStateMode = reading;
  return switched;
}

void updateSoundParams(int step) {
  Serial.print("[updateSoundParams] step=");
  Serial.println(step);

  if (step == 0) {
    TRACE_BRANCH("[COV] updateSoundParams:step==0:true");
    return;
  }
  TRACE_BRANCH("[COV] updateSoundParams:step==0:false");

  pitchIndex += step;
  if (pitchIndex < PITCH_MIN) {
    TRACE_BRANCH("[COV] updateSoundParams:pitch<min:true");
    pitchIndex = PITCH_MIN;
  } else {
    TRACE_BRANCH("[COV] updateSoundParams:pitch<min:false");
  }
  if (pitchIndex > PITCH_MAX) {
    TRACE_BRANCH("[COV] updateSoundParams:pitch>max:true");
    pitchIndex = PITCH_MAX;
  } else {
    TRACE_BRANCH("[COV] updateSoundParams:pitch>max:false");
  }

  float ratio = pow(2.0, pitchIndex / 12.0);
  targetFreqHz = (int)(BASE_FREQ_HZ * ratio + 0.5);

  Serial.print("[updateSoundParams] pitchIndex=");
  Serial.print(pitchIndex);
  Serial.print(" targetFreqHz=");
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
  Serial.println("[changeMelodyPattern] apply timbre confirmation sound");

  if (timbreMode < TIMBRE_MIN || timbreMode > TIMBRE_MAX) {
    TRACE_BRANCH("[COV] changeMelodyPattern:timbre-invalid:true");
    timbreMode = TIMBRE_MIN;
    Serial.println("[changeMelodyPattern] invalid timbre fixed to 0");
  } else {
    TRACE_BRANCH("[COV] changeMelodyPattern:timbre-invalid:false");
  }

  tone(PIN_BUZZER, targetFreqHz, 60);

  Serial.print("[changeMelodyPattern] current timbreMode=");
  Serial.println(timbreMode);
}

void saveSoundPattern() {
  Serial.println("[saveSoundPattern] stub (not implemented in this version)");
}

void loadSoundPattern() {
  Serial.println("[loadSoundPattern] stub (not implemented in this version)");
}

void playCurrentSound() {
  static int arpeggioIndex = 0;
  static bool tremoloPhase = false;
  static int volumeGateCounter = 0;

  int freq = targetFreqHz;

  if (timbreMode == 1) {
    TRACE_BRANCH("[COV] playCurrentSound:timbre==1");
    tremoloPhase = !tremoloPhase;
    freq = targetFreqHz + (tremoloPhase ? 8 : -8);
  } else if (timbreMode == 2) {
    TRACE_BRANCH("[COV] playCurrentSound:timbre==2");
    const int intervals[3] = {0, 4, 7};
    float ratio = pow(2.0, intervals[arpeggioIndex] / 12.0);
    freq = (int)(targetFreqHz * ratio + 0.5);
    arpeggioIndex = (arpeggioIndex + 1) % 3;
  } else {
    TRACE_BRANCH("[COV] playCurrentSound:timbre-other");
  }

  if (freq < FREQ_MIN_HZ || freq > (FREQ_MAX_HZ * 2)) {
    TRACE_BRANCH("[COV] playCurrentSound:freq-invalid:true");
    Serial.println("[playCurrentSound] invalid frequency, stop tone");
    noTone(PIN_BUZZER);
    errorFlag = true;
    errorCode = 2;
    return;
  } else {
    TRACE_BRANCH("[COV] playCurrentSound:freq-invalid:false");
  }

  volumeGateCounter = (volumeGateCounter + 1) % (VOLUME_MAX + 1);
  if (volumeLevel <= 0 || volumeGateCounter > volumeLevel) {
    TRACE_BRANCH("[COV] playCurrentSound:gate-off:true");
    noTone(PIN_BUZZER);
  } else {
    TRACE_BRANCH("[COV] playCurrentSound:gate-off:false");
    tone(PIN_BUZZER, freq);
  }

  static unsigned long lastPlayLog = 0;
  if (nowMs - lastPlayLog >= 250) {
    TRACE_BRANCH("[COV] playCurrentSound:periodic-log:true");
    Serial.print("[playCurrentSound] freq=");
    Serial.print(freq);
    Serial.print(" volume=");
    Serial.print(volumeLevel);
    Serial.print(" timbre=");
    Serial.println(timbreMode);
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

  static bool blink = false;
  blink = !blink;

  switch (state) {
    case STATE_IDLE:
      TRACE_BRANCH("[COV] showStatusLed:switch:STATE_IDLE");
      ledStateGreen = false;
      ledStateRed = false;
      break;
    case STATE_SETTING:
      TRACE_BRANCH("[COV] showStatusLed:switch:STATE_SETTING");
      ledStateGreen = blink;
      ledStateRed = false;
      break;
    case STATE_PLAYING:
      TRACE_BRANCH("[COV] showStatusLed:switch:STATE_PLAYING");
      ledStateGreen = true;
      ledStateRed = false;
      break;
    case STATE_VOLUME_ADJUST:
      TRACE_BRANCH("[COV] showStatusLed:switch:STATE_VOLUME_ADJUST");
      ledStateGreen = blink;
      ledStateRed = false;
      break;
    case STATE_ERROR:
      TRACE_BRANCH("[COV] showStatusLed:switch:STATE_ERROR");
      ledStateGreen = false;
      ledStateRed = blink;
      break;
    default:
      TRACE_BRANCH("[COV] showStatusLed:switch:default");
      ledStateGreen = false;
      ledStateRed = blink;
      break;
  }

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

  static int smoothed = 0;
  potRaw = analogRead(PIN_POT);
  smoothed = (smoothed * 3 + potRaw) / 4;

  int mappedVolume = map(smoothed, 0, 1023, VOLUME_MIN, VOLUME_MAX);
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
    Serial.print("[adjustVolume] potRaw=");
    Serial.print(potRaw);
    Serial.print(" volumeLevel=");
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
}