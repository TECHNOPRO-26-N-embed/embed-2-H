// Detailed-design-based implementation for Arduino UNO R3.

#include <Keypad.h>
#include <pitches.h>

// UNO R3向け軽量化スイッチ（0で無効化）
#define ENABLE_DIAGNOSTICS 0
#define ENABLE_COVERAGE 0
#define ENABLE_STATE_TRANSITION_LOG 1

#if ENABLE_DIAGNOSTICS
#define DBG_BEGIN(baud) Serial.begin(baud)
#define DBG_PRINT(x) Serial.print(x)
#define DBG_PRINTLN(x) Serial.println(x)
#else
#define DBG_BEGIN(baud) do {} while (0)
#define DBG_PRINT(x) do {} while (0)
#define DBG_PRINTLN(x) do {} while (0)
#endif

#if ENABLE_STATE_TRANSITION_LOG
#define STLOG_BEGIN(baud) Serial.begin(baud)
#define STLOG_PRINT(x) Serial.print(x)
#define STLOG_PRINTLN(x) Serial.println(x)
#else
#define STLOG_BEGIN(baud) do {} while (0)
#define STLOG_PRINT(x) do {} while (0)
#define STLOG_PRINTLN(x) do {} while (0)
#endif

// const int TONES[NUM_TONES] = {
//     NOTE_C4, NOTE_C4, NOTE_G4, NOTE_G4, NOTE_A4, NOTE_A4, NOTE_G4, NOTE_EMPTY,
//     NOTE_F4, NOTE_F4, NOTE_E4, NOTE_E4, NOTE_D4, NOTE_D4, NOTE_C4, NOTE_EMPTY};
// const int TONES2[] = {
//     NOTE_C4, NOTE_D4, NOTE_E4, NOTE_C4, NOTE_D4, NOTE_E4, NOTE_G4, NOTE_E4, NOTE_D4, NOTE_C4, NOTE_D4, NOTE_E4, NOTE_D4,
//     NOTE_G4, NOTE_G4, NOTE_E4, NOTE_G4, NOTE_A4, NOTE_A4, NOTE_G4, NOTE_E4, NOTE_E4, NOTE_D4, NOTE_D4, NOTE_C4};

// 各部品が使用するピンの定義（2-9はキー、10はブザー、11は赤LED、12は確定ボタン）
const int PIN_KEY_1 = 9;
const int PIN_KEY_2 = 8;
const int PIN_KEY_3 = 7;
const int PIN_KEY_4 = 6;
const int PIN_KEY_5 = 5;
const int PIN_KEY_6 = 4;
const int PIN_KEY_7 = 3;
const int PIN_KEY_8 = 2;
const int PIN_BUZZER = 10;
const int PIN_LED_RED = 11;
const int PIN_CONFIRM_BUTTON = 12;

// Keypadライブラリによるマトリックス入力
const byte ROWS = 4;
const byte COLS = 4;
char hexaKeys[ROWS][COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};
byte rowPins[ROWS] = {9, 8, 7, 6};
byte colPins[COLS] = {5, 4, 3, 2};
Keypad customKeypad = Keypad(makeKeymap(hexaKeys), rowPins, colPins, ROWS, COLS);

// 音定義は pitches.h の定数を使う
const int TONE_DO = NOTE_C4;
const int TONE_RE = NOTE_D4;
const int TONE_MI = NOTE_E4;
const int TONE_FA = NOTE_F4;
const int TONE_SO = NOTE_G4;
const int TONE_LA = NOTE_A4;
const int TONE_SI = NOTE_B4;

// Keypadイベント定義
const int KEY_NONE = 0;
const int KEY_TURN_SWITCH = 9; // '0' キーをモード切替に使う
const int KEY_OCTAVE_DOWN = 10; // '*'
const int KEY_OCTAVE_UP = 11;   // '#'
const int KEY_DIFFICULTY_A = 12;
const int KEY_DIFFICULTY_B = 13;
const int KEY_DIFFICULTY_C = 14;

// 1-3 State constants and state variable
const int STATE_MODE_SELECTION = -2;
const int STATE_DIFFICULTY_SELECTION = -1;
const int STATE_ORIGINAL_WAIT = 0;
const int STATE_ORIGINAL_RECORD = 1;
const int STATE_DEFAULT_PLAYBACK = 2;
const int STATE_MIMIC_WAIT = 3;
const int STATE_CORRECT_NOTIFY = 4;
const int STATE_WRONG_NOTIFY = 5;
int currentState = STATE_MODE_SELECTION;

// 1-5 Processing constants
const int MAX_SEQUENCE_LENGTH = 20;
const unsigned long MIMIC_TIMEOUT_BASE_MS = 3000;
const int INPUT_TONE_DURATION_MS = 100;
const int DEFAULT_SONG_TONE_DURATION_MS = 180;
const int DEFAULT_SONG_GAP_MS = 80;
const int CLEAR_TUNE_GAP_MS = 90;
const int WRONG_LED_BLINK_MS = 200;
const int WRONG_LED_BLINK_COUNT = 3;
const int DEBOUNCE_DELAY_MS = 50;
const int ORIGINAL_MODE = 0;
const int MIMIC_MODE = 1;
const int MODE_ORIGINAL_INPUT = 0;
const int MODE_DEFAULT_SONG = 1;
const int RESULT_CORRECT = 1;
const int RESULT_WRONG = -1;
const int RESULT_CONTINUE = 0;
const int MIN_NOTE_KEY = 1;
const int MAX_NOTE_KEY = 7;
const int MIN_OCTAVE_SHIFT = -1;
const int MAX_OCTAVE_SHIFT = 1;

const int DIFFICULTY_DISTANCE_A = 30;
const int DIFFICULTY_DISTANCE_B = 20;
const int DIFFICULTY_DISTANCE_C = 10;
const unsigned long DIFFICULTY_TIMEOUT_A_MS = 4000;
const unsigned long DIFFICULTY_TIMEOUT_B_MS = 3000;
const unsigned long DIFFICULTY_TIMEOUT_C_MS = 2200;

// 1-4 Input and judge variables
int inputKey = KEY_NONE;
int originalSeq[MAX_SEQUENCE_LENGTH];
int expectedKey = 0;
int originalLength = 0;
int mimicCount = 0;
int compareIndex = 0;
unsigned long lastMimicInputMs = 0;
bool isAllMatch = false;
bool missDetected = false;
unsigned long lastDebounceTimeKey = 0;
unsigned long lastDebounceTimeButton = 0;
int lastStableInputKey = KEY_NONE;
bool lastButtonStableState = false;
int currentMode = MODE_ORIGINAL_INPUT;
int octaveShift = 0;
int difficultyLevel = 1; // 0:A(Easy), 1:B(Normal), 2:C(Hard)
int distanceValue = DIFFICULTY_DISTANCE_B;
unsigned long mimicTimeoutMs = MIMIC_TIMEOUT_BASE_MS;
bool isPlaybackLocked = false;
bool modeSelected = false;

// ボタン切替時に使う固定曲（キー番号列）
const int DEFAULT_SEQUENCE[] = {1, 1, 5, 5, 6, 6, 5, 4, 4, 3, 3, 2, 2, 1};
const int DEFAULT_SEQUENCE_LENGTH = sizeof(DEFAULT_SEQUENCE) / sizeof(DEFAULT_SEQUENCE[0]);

// Coverage flags for manual test verification.
enum CoverageId {
  COV_SETUP = 0,
  COV_LOOP,
  COV_STATE_ORIGINAL_WAIT,
  COV_STATE_ORIGINAL_RECORD,
  COV_STATE_MIMIC_WAIT,
  COV_STATE_COMPARE,
  COV_STATE_CORRECT_NOTIFY,
  COV_STATE_WRONG_NOTIFY,
  COV_READ_INPUT_VALID,
  COV_READ_INPUT_INVALID,
  COV_TURN_SWITCH_PRESS,
  COV_END_BUTTON_PRESS,
  COV_DEFAULT_MODE_PREPARE,
  COV_RECORD_ORIGINAL,
  COV_RECORD_ORIGINAL_FULL,
  COV_COMPARE_MATCH,
  COV_COMPARE_MISS,
  COV_JUDGE_CORRECT,
  COV_JUDGE_WRONG_MISS,
  COV_JUDGE_WRONG_TIMEOUT,
  COV_JUDGE_CONTINUE,
  COV_MAX
};

#if ENABLE_COVERAGE
bool coverage[COV_MAX] = {false};
#endif

#if ENABLE_COVERAGE
#define COVERAGE_HIT(id) do { coverage[id] = true; } while (0)
#else
#define COVERAGE_HIT(id) do {} while (0)
#endif

const unsigned long STATE_LOG_INTERVAL_MS = 1000;
unsigned long lastStateLogMs = 0;
int lastLoggedState = -1;

const __FlashStringHelper* getStateLabel(int state) {
  switch (state) {
    case STATE_MODE_SELECTION: return F("STATE_MODE_SELECTION");
    case STATE_DIFFICULTY_SELECTION: return F("STATE_DIFFICULTY_SELECTION");
    case STATE_ORIGINAL_WAIT: return F("STATE_ORIGINAL_WAIT");
    case STATE_ORIGINAL_RECORD: return F("STATE_ORIGINAL_RECORD");
    case STATE_DEFAULT_PLAYBACK: return F("STATE_DEFAULT_PLAYBACK");
    case STATE_MIMIC_WAIT: return F("STATE_MIMIC_WAIT");
    case STATE_CORRECT_NOTIFY: return F("STATE_CORRECT_NOTIFY");
    case STATE_WRONG_NOTIFY: return F("STATE_WRONG_NOTIFY");
    default: return F("STATE_UNKNOWN");
  }
}

void transitionToState(int nextState, const __FlashStringHelper* reason) {
  if (nextState == currentState) {
    return;
  }

  STLOG_PRINT(F("[TRANSITION] "));
  STLOG_PRINT(getStateLabel(currentState));
  STLOG_PRINT(F(" -> "));
  STLOG_PRINT(getStateLabel(nextState));
  STLOG_PRINT(F(" | reason="));
  STLOG_PRINTLN(reason);

  currentState = nextState;
}

#if ENABLE_DIAGNOSTICS
const char* getStateName(int state) {
  switch (state) {
    case STATE_MODE_SELECTION: return "STATE_MODE_SELECTION";
    case STATE_DIFFICULTY_SELECTION: return "STATE_DIFFICULTY_SELECTION";
    case STATE_ORIGINAL_WAIT: return "STATE_ORIGINAL_WAIT";
    case STATE_ORIGINAL_RECORD: return "STATE_ORIGINAL_RECORD";
    case STATE_DEFAULT_PLAYBACK: return "STATE_DEFAULT_PLAYBACK";
    case STATE_MIMIC_WAIT: return "STATE_MIMIC_WAIT";
    case STATE_CORRECT_NOTIFY: return "STATE_CORRECT_NOTIFY";
    case STATE_WRONG_NOTIFY: return "STATE_WRONG_NOTIFY";
    default: return "STATE_UNKNOWN";
  }
}
#endif

void logStateAction(const char* action) {
#if ENABLE_DIAGNOSTICS
  unsigned long now = millis();
  // 同じ状態を短い時間に複数回表示させない（一定時間経過後出力）
  if (currentState == lastLoggedState && now - lastStateLogMs < STATE_LOG_INTERVAL_MS) {
    return;
  }

  Serial.print("[STATE] ");
  Serial.print(getStateName(currentState));
  Serial.print(" | action=");
  Serial.println(action);

  lastStateLogMs = now;
  lastLoggedState = currentState;
#else
  (void)action;
#endif
}

// カバレッジ回数を表示　ターン終了時に出力してますか
void printCoverageReport() {
#if ENABLE_COVERAGE
  Serial.println("[COVERAGE]");
  Serial.print("setup="); Serial.println(coverage[COV_SETUP] ? "HIT" : "MISS");
  Serial.print("loop="); Serial.println(coverage[COV_LOOP] ? "HIT" : "MISS");
  Serial.print("state_original_wait="); Serial.println(coverage[COV_STATE_ORIGINAL_WAIT] ? "HIT" : "MISS");
  Serial.print("state_original_record="); Serial.println(coverage[COV_STATE_ORIGINAL_RECORD] ? "HIT" : "MISS");
  Serial.print("state_mimic_wait="); Serial.println(coverage[COV_STATE_MIMIC_WAIT] ? "HIT" : "MISS");
  Serial.print("state_compare="); Serial.println(coverage[COV_STATE_COMPARE] ? "HIT" : "MISS");
  Serial.print("state_correct_notify="); Serial.println(coverage[COV_STATE_CORRECT_NOTIFY] ? "HIT" : "MISS");
  Serial.print("state_wrong_notify="); Serial.println(coverage[COV_STATE_WRONG_NOTIFY] ? "HIT" : "MISS");
  Serial.print("read_input_valid="); Serial.println(coverage[COV_READ_INPUT_VALID] ? "HIT" : "MISS");
  Serial.print("read_input_invalid="); Serial.println(coverage[COV_READ_INPUT_INVALID] ? "HIT" : "MISS");
  Serial.print("turn_switch_press="); Serial.println(coverage[COV_TURN_SWITCH_PRESS] ? "HIT" : "MISS");
  Serial.print("end_button_press="); Serial.println(coverage[COV_END_BUTTON_PRESS] ? "HIT" : "MISS");
  Serial.print("default_mode_prepare="); Serial.println(coverage[COV_DEFAULT_MODE_PREPARE] ? "HIT" : "MISS");
  Serial.print("record_original="); Serial.println(coverage[COV_RECORD_ORIGINAL] ? "HIT" : "MISS");
  Serial.print("record_original_full="); Serial.println(coverage[COV_RECORD_ORIGINAL_FULL] ? "HIT" : "MISS");
  Serial.print("compare_match="); Serial.println(coverage[COV_COMPARE_MATCH] ? "HIT" : "MISS");
  Serial.print("compare_miss="); Serial.println(coverage[COV_COMPARE_MISS] ? "HIT" : "MISS");
  Serial.print("judge_correct="); Serial.println(coverage[COV_JUDGE_CORRECT] ? "HIT" : "MISS");
  Serial.print("judge_wrong_miss="); Serial.println(coverage[COV_JUDGE_WRONG_MISS] ? "HIT" : "MISS");
  Serial.print("judge_wrong_timeout="); Serial.println(coverage[COV_JUDGE_WRONG_TIMEOUT] ? "HIT" : "MISS");
  Serial.print("judge_continue="); Serial.println(coverage[COV_JUDGE_CONTINUE] ? "HIT" : "MISS");
#endif
}

void resetGameState() {
  originalLength = 0;
  compareIndex = 0;
  mimicCount = 0;
  missDetected = false;
  isAllMatch = false;
  currentMode = MODE_ORIGINAL_INPUT;
  modeSelected = false;
  octaveShift = 0;
  difficultyLevel = 1;
  distanceValue = DIFFICULTY_DISTANCE_B;
  mimicTimeoutMs = MIMIC_TIMEOUT_BASE_MS;
  currentMode = MODE_ORIGINAL_INPUT;
  transitionToState(STATE_MODE_SELECTION, F("reset_game_state"));
}

const __FlashStringHelper* getModeLabel(int mode) {
  if (mode == MODE_DEFAULT_SONG) {
    return F("DEFAULT_SONG");
  }
  return F("ORIGINAL");
}

char getDifficultyLabel() {
  if (difficultyLevel == 0) {
    return 'A';
  }
  if (difficultyLevel == 2) {
    return 'C';
  }
  return 'B';
}

void printCurrentMode() {
  STLOG_PRINT(F("[MODE] current_mode="));
  STLOG_PRINTLN(getModeLabel(currentMode));
}

void printCurrentDifficulty() {
  STLOG_PRINT(F("[DIFFICULTY] current="));
  STLOG_PRINT(getDifficultyLabel());
  STLOG_PRINT(F(" timeout_ms="));
  STLOG_PRINTLN(mimicTimeoutMs);
}

void toggleModeByZeroKey() {
  if (currentMode == MODE_ORIGINAL_INPUT) {
    currentMode = MODE_DEFAULT_SONG;
  } else {
    currentMode = MODE_ORIGINAL_INPUT;
  }

  printCurrentMode();
}

void blinkWrongLed() {
  for (int i = 0; i < WRONG_LED_BLINK_COUNT; i++) {
    digitalWrite(PIN_LED_RED, HIGH);
    delay(WRONG_LED_BLINK_MS);
    digitalWrite(PIN_LED_RED, LOW);
    delay(WRONG_LED_BLINK_MS);
  }
}

bool isPlayableInputState() {
  return currentState == STATE_ORIGINAL_RECORD ||
         currentState == STATE_MIMIC_WAIT;
}

void applyDifficultyByKey(int difficultyKey) {
  if (difficultyKey == KEY_DIFFICULTY_A) {
    difficultyLevel = 0;
    distanceValue = DIFFICULTY_DISTANCE_A;
    mimicTimeoutMs = DIFFICULTY_TIMEOUT_A_MS;
  } else if (difficultyKey == KEY_DIFFICULTY_B) {
    difficultyLevel = 1;
    distanceValue = DIFFICULTY_DISTANCE_B;
    mimicTimeoutMs = DIFFICULTY_TIMEOUT_B_MS;
  } else if (difficultyKey == KEY_DIFFICULTY_C) {
    difficultyLevel = 2;
    distanceValue = DIFFICULTY_DISTANCE_C;
    mimicTimeoutMs = DIFFICULTY_TIMEOUT_C_MS;
  }

  DBG_PRINT(F("[DIFFICULTY] Difficulty set to "));
  DBG_PRINT(difficultyLevel);
  DBG_PRINT(F(", Timeout: "));
  DBG_PRINTLN(mimicTimeoutMs);

  printCurrentDifficulty();
}

int mapKeyToFrequency(int key) {
  int baseFrequency = 0;
  switch (key) {
    case 1: baseFrequency = TONE_DO; break;
    case 2: baseFrequency = TONE_RE; break;
    case 3: baseFrequency = TONE_MI; break;
    case 4: baseFrequency = TONE_FA; break;
    case 5: baseFrequency = TONE_SO; break;
    case 6: baseFrequency = TONE_LA; break;
    case 7: baseFrequency = TONE_SI; break;
    default: return 0;
  }

  if (octaveShift == -1) {
    return baseFrequency / 2;
  }
  if (octaveShift == 1) {
    return baseFrequency * 2;
  }

  return baseFrequency;
}

void playToneWithGap(int frequency, int toneDurationMs, int gapMs) {
  if (frequency > 0) {
    tone(PIN_BUZZER, frequency, toneDurationMs);
  }
  delay(toneDurationMs);
  noTone(PIN_BUZZER);
  delay(gapMs);
}

void prepareDefaultSequence() {
  originalLength = 0;
  for (int i = 0; i < DEFAULT_SEQUENCE_LENGTH && i < MAX_SEQUENCE_LENGTH; i++) {
    originalSeq[originalLength] = DEFAULT_SEQUENCE[i];
    originalLength++;
  }

  compareIndex = 0;
  mimicCount = 0;
  missDetected = false;
  isAllMatch = false;
  COVERAGE_HIT(COV_DEFAULT_MODE_PREPARE);
}

void playDefaultSequence() {
  isPlaybackLocked = true;
  for (int i = 0; i < originalLength; i++) {
    int frequency = mapKeyToFrequency(originalSeq[i]);
    playToneWithGap(frequency, DEFAULT_SONG_TONE_DURATION_MS, DEFAULT_SONG_GAP_MS);
  }
  noTone(PIN_BUZZER);
  isPlaybackLocked = false;
}

void playClearTune() {
  const int clearNotes[] = {NOTE_C5, NOTE_E5, NOTE_G5, NOTE_E5, NOTE_G5, NOTE_C6};
  const int clearDurations[] = {130, 130, 130, 130, 130, 260};
  const int clearLength = sizeof(clearNotes) / sizeof(clearNotes[0]);

  isPlaybackLocked = true;
  for (int i = 0; i < clearLength; i++) {
    playToneWithGap(clearNotes[i], clearDurations[i], CLEAR_TUNE_GAP_MS);
  }
  noTone(PIN_BUZZER);
  isPlaybackLocked = false;
}

// 初期設定
void setup() {
  COVERAGE_HIT(COV_SETUP);

  // ボタンのピンとモード設定　INPUT_PULLUPは内部プルアップ抵抗とは
  pinMode(PIN_CONFIRM_BUTTON, INPUT_PULLUP);

  // パッシブブザーとLEDはOUTPUT
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_LED_RED, OUTPUT);
  digitalWrite(PIN_LED_RED, LOW); // LEDを消灯状態に

  // ゲームの初期化（状態、オリジナル音数、ミミック入力カウント、比較インデックス、全一致フラグ、ミス検出フラグ）
  currentState = STATE_MODE_SELECTION;
  originalLength = 0;
  mimicCount = 0;
  compareIndex = 0;
  isAllMatch = false;
  missDetected = false;

  // シリアル通信の開始と初期化完了ログ
  STLOG_BEGIN(115200);
  DBG_BEGIN(115200);
  DBG_PRINTLN(F("[CONFIG] keypad mode=Keypad library"));
  printCurrentMode();
  printCurrentDifficulty();
  logStateAction("initialization complete");
}

void loop() {
  COVERAGE_HIT(COV_LOOP);

  // 入力とその時間の読み取り
  inputKey = readInputKey();
  unsigned long now = millis();

  // オクターブ変更はオリジナル/ミミック入力中のみ受理する
  if (inputKey == KEY_OCTAVE_DOWN && isPlayableInputState()) {
    if (octaveShift > MIN_OCTAVE_SHIFT) {
      octaveShift--;
    }
    DBG_PRINT(F("[CONFIG] octave_shift="));
    DBG_PRINTLN(octaveShift);
  } else if (inputKey == KEY_OCTAVE_UP && isPlayableInputState()) {
    if (octaveShift < MAX_OCTAVE_SHIFT) {
      octaveShift++;
    }
    DBG_PRINT(F("[CONFIG] octave_shift="));
    DBG_PRINTLN(octaveShift);
  }

  // 該当キーと音をマッピング
    if (inputKey >= MIN_NOTE_KEY && inputKey <= MAX_NOTE_KEY && isPlayableInputState()) {
      playMappedTone(inputKey); // 音を鳴らす条件を修正
  }

  switch (currentState) {
    case STATE_MODE_SELECTION:
      logStateAction("select mode by key '0', press button to confirm");

      if (inputKey == KEY_TURN_SWITCH) {
        toggleModeByZeroKey();
      }

      if (readOriginalEndButton()) {
        modeSelected = true;
        printCurrentMode();
        transitionToState(STATE_DIFFICULTY_SELECTION, F("mode_confirmed_by_button"));
      }
      break;

    case STATE_DIFFICULTY_SELECTION:
      logStateAction("select difficulty by A/B/C, press button to confirm");

      if (inputKey == KEY_DIFFICULTY_A || inputKey == KEY_DIFFICULTY_B || inputKey == KEY_DIFFICULTY_C) {
        applyDifficultyByKey(inputKey);
      }

      if (readOriginalEndButton()) {
        if (currentMode == MODE_DEFAULT_SONG) {
          transitionToState(STATE_DEFAULT_PLAYBACK, F("difficulty_confirmed_default_mode"));
        } else {
          transitionToState(STATE_ORIGINAL_WAIT, F("difficulty_confirmed_original_mode"));
        }
      }
      break;

    case STATE_ORIGINAL_WAIT:
      COVERAGE_HIT(COV_STATE_ORIGINAL_WAIT);
      logStateAction("press button to start original input");

      if (readOriginalEndButton()) {
        originalLength = 0;
        compareIndex = 0;
        mimicCount = 0;
        missDetected = false;
        isAllMatch = false;
        octaveShift = 0;
        transitionToState(STATE_ORIGINAL_RECORD, F("original_start_by_button"));
      }
      break;

    case STATE_ORIGINAL_RECORD:
      COVERAGE_HIT(COV_STATE_ORIGINAL_RECORD);
      logStateAction("recording original sequence");

      if (inputKey >= MIN_NOTE_KEY && inputKey <= MAX_NOTE_KEY) {
        DBG_PRINTLN(F("[ACTION] record original key"));
        recordAndCompare(inputKey, ORIGINAL_MODE);
      }

      // ボタン押下でオリジナル入力を確定してミミック開始
      if (readOriginalEndButton() && originalLength > 0) {
        DBG_PRINTLN(F("[ACTION] original input finalized by button, move to STATE_MIMIC_WAIT"));
        compareIndex = 0;
        mimicCount = 0;
        missDetected = false;
        isAllMatch = false;
        octaveShift = 0;
        DBG_PRINTLN(F("[CONFIG] mimic_start_octave=0"));
        lastMimicInputMs = now;
        transitionToState(STATE_MIMIC_WAIT, F("original_confirmed_by_button"));
      }
      break;

    case STATE_DEFAULT_PLAYBACK:
      logStateAction("playing default sequence (inputs locked)");
      prepareDefaultSequence();
      playDefaultSequence();
      octaveShift = 0;
      DBG_PRINTLN(F("[CONFIG] mimic_start_octave=0"));
      lastMimicInputMs = millis();
      transitionToState(STATE_MIMIC_WAIT, F("default_song_finished"));
      break;

    case STATE_MIMIC_WAIT:
      COVERAGE_HIT(COV_STATE_MIMIC_WAIT);
      logStateAction("waiting mimic input and comparing immediately");

      if (inputKey >= MIN_NOTE_KEY && inputKey <= MAX_NOTE_KEY) {
        DBG_PRINTLN(F("[ACTION] compare mimic key"));
        lastMimicInputMs = now;
        recordAndCompare(inputKey, MIMIC_MODE);
      }

      {
        int result = judgeGameResult();
        if (result == RESULT_CORRECT) {
          DBG_PRINTLN(F("[ACTION] judge result=CORRECT, move to STATE_CORRECT_NOTIFY"));
          transitionToState(STATE_CORRECT_NOTIFY, F("mimic_complete"));
        } else if (result == RESULT_WRONG) {
          DBG_PRINTLN(F("[ACTION] judge result=WRONG, move to STATE_WRONG_NOTIFY"));
          transitionToState(STATE_WRONG_NOTIFY, F("mimic_failed"));
        }
      }
      break;

    case STATE_CORRECT_NOTIFY:
      COVERAGE_HIT(COV_STATE_CORRECT_NOTIFY);
      logStateAction("playing correct notification tone and resetting turn");
      playClearTune();
      resetGameState();
      DBG_PRINTLN(F("[ACTION] reset complete, move to STATE_ORIGINAL_WAIT"));
      printCoverageReport();
      break;

    case STATE_WRONG_NOTIFY:
      COVERAGE_HIT(COV_STATE_WRONG_NOTIFY);
      logStateAction("blinking wrong notification LED and resetting turn");
      blinkWrongLed();
      resetGameState();
      DBG_PRINTLN(F("[ACTION] reset complete, move to STATE_ORIGINAL_WAIT"));
      printCoverageReport();
      break;

    default:
      DBG_PRINTLN(F("[ACTION] unknown state detected, force STATE_ORIGINAL_WAIT"));
      transitionToState(STATE_MODE_SELECTION, F("unknown_state_guard"));
      break;
  }
}

int readInputKey() {
  if (isPlaybackLocked) {
    return KEY_NONE;
  }

  char customKey = customKeypad.getKey();
  int rawKey = KEY_NONE;

  if (customKey >= '1' && customKey <= '7') {
    rawKey = customKey - '0';
  } else if (customKey == '0') {
    rawKey = KEY_TURN_SWITCH;
    COVERAGE_HIT(COV_TURN_SWITCH_PRESS);
  } else if (customKey == '*' && isPlayableInputState()) {
    rawKey = KEY_OCTAVE_DOWN;
  } else if (customKey == '#' && isPlayableInputState()) {
    rawKey = KEY_OCTAVE_UP;
  } else if (customKey == 'A') {
    rawKey = KEY_DIFFICULTY_A;
  } else if (customKey == 'B') {
    rawKey = KEY_DIFFICULTY_B;
  } else if (customKey == 'C') {
    rawKey = KEY_DIFFICULTY_C;
  } else if (customKey == NO_KEY) {
    return KEY_NONE;
  } else {
    COVERAGE_HIT(COV_READ_INPUT_INVALID);
    return KEY_NONE;
  }

  unsigned long now = millis();
  if (now - lastDebounceTimeKey < DEBOUNCE_DELAY_MS) {
    return KEY_NONE;
  }

  lastStableInputKey = rawKey;
  lastDebounceTimeKey = now;

  if (rawKey >= MIN_NOTE_KEY && rawKey <= MAX_NOTE_KEY) {
    COVERAGE_HIT(COV_READ_INPUT_VALID);
    DBG_PRINT(F("[INPUT] key="));
    DBG_PRINTLN(rawKey);
  } else if (rawKey == KEY_TURN_SWITCH) {
    DBG_PRINTLN(F("[INPUT] turn_switch=0"));
  } else if (rawKey == KEY_OCTAVE_DOWN) {
    DBG_PRINTLN(F("[INPUT] octave_down=*"));
  } else if (rawKey == KEY_OCTAVE_UP) {
    DBG_PRINTLN(F("[INPUT] octave_up=#"));
  } else if (rawKey == KEY_DIFFICULTY_A || rawKey == KEY_DIFFICULTY_B || rawKey == KEY_DIFFICULTY_C) {
    DBG_PRINT(F("[INPUT] difficulty_key="));
    if (rawKey == KEY_DIFFICULTY_A) {
      DBG_PRINTLN(F("A"));
    } else if (rawKey == KEY_DIFFICULTY_B) {
      DBG_PRINTLN(F("B"));
    } else {
      DBG_PRINTLN(F("C"));
    }
  }

  return rawKey;
}

bool readOriginalEndButton() {
  if (isPlaybackLocked) {
    return false;
  }

  bool rawPressed = (digitalRead(PIN_CONFIRM_BUTTON) == LOW);
  bool previousStable = lastButtonStableState;
  unsigned long now = millis();

  if (rawPressed == previousStable) {
    return false;
  }

  if (now - lastDebounceTimeButton < DEBOUNCE_DELAY_MS) {
    return false;
  }

  lastButtonStableState = rawPressed;
  lastDebounceTimeButton = now;

  DBG_PRINT(F("[INPUT] mode_button="));
  DBG_PRINTLN(rawPressed ? F("PRESSED") : F("RELEASED"));

  // 未押下 -> 押下のみ有効
  if (!previousStable && rawPressed) {
    COVERAGE_HIT(COV_END_BUTTON_PRESS);
    return true;
  }

  return false;
}

void playMappedTone(int key) {
  int frequency = mapKeyToFrequency(key);

  if (frequency > 0) {
    tone(PIN_BUZZER, frequency, INPUT_TONE_DURATION_MS);
  }
}

bool recordAndCompare(int key, int mode) {
  if (key < MIN_NOTE_KEY || key > MAX_NOTE_KEY) {
    return false;
  }

  if (mode == ORIGINAL_MODE) {
    if (originalLength < MAX_SEQUENCE_LENGTH) {
      originalSeq[originalLength] = key;
      originalLength++;
      COVERAGE_HIT(COV_RECORD_ORIGINAL);
      return true;
    }

    // 上限に達した場合は追加しないが、動作継続扱いにする
    COVERAGE_HIT(COV_RECORD_ORIGINAL_FULL);
    return true;
  }

  if (mode == MIMIC_MODE) {
    if (originalLength == 0) {
      return false;
    }
    if (compareIndex < 0 || compareIndex >= originalLength) {
      return false;
    }

    expectedKey = originalSeq[compareIndex];
    if (key == expectedKey) {
      mimicCount++;
      compareIndex++;
      lastMimicInputMs = millis();
      COVERAGE_HIT(COV_COMPARE_MATCH);
      return true;
    }

    missDetected = true;
    COVERAGE_HIT(COV_COMPARE_MISS);
    return false;
  }

  return false;
}

int judgeGameResult() {
  if (missDetected) {
    COVERAGE_HIT(COV_JUDGE_WRONG_MISS);
    return RESULT_WRONG;
  }
  // 全て一致した場合
  if (originalLength > 0 && compareIndex == originalLength) {
    isAllMatch = true;
    COVERAGE_HIT(COV_JUDGE_CORRECT);
    return RESULT_CORRECT;
  }
  // タイムアウト判定（ミミック入力待機中に判定）
  if (currentState == STATE_MIMIC_WAIT) {
    unsigned long now = millis();
    if (now - lastMimicInputMs >= mimicTimeoutMs) {
      missDetected = true;
      COVERAGE_HIT(COV_JUDGE_WRONG_TIMEOUT);
      return RESULT_WRONG;
    }
  }

  COVERAGE_HIT(COV_JUDGE_CONTINUE);
  return RESULT_CONTINUE;
}

void updateLcdStatus(int turn, int index) {
  (void)turn;
  (void)index;
  // Optional feature: no processing when LCD is not connected.
}

int selectDifficulty() {
  // Optional feature: return default difficulty ID.
  return 0;
}

void updateAccuracyStats(bool turnResult) {
  (void)turnResult;
  // Optional feature: placeholder for last-5-turn statistics.
}
