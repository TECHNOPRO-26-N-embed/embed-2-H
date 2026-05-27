// Detailed-design-based implementation for Arduino UNO R3.

#include <Keypad.h>
#include <pitches.h>

// const int TONES[NUM_TONES] = {
//     NOTE_C4, NOTE_C4, NOTE_G4, NOTE_G4, NOTE_A4, NOTE_A4, NOTE_G4, NOTE_EMPTY,
//     NOTE_F4, NOTE_F4, NOTE_E4, NOTE_E4, NOTE_D4, NOTE_D4, NOTE_C4, NOTE_EMPTY};
// const int TONES2[] = {
//     NOTE_C4, NOTE_D4, NOTE_E4, NOTE_C4, NOTE_D4, NOTE_E4, NOTE_G4, NOTE_E4, NOTE_D4, NOTE_C4, NOTE_D4, NOTE_E4, NOTE_D4,
//     NOTE_G4, NOTE_G4, NOTE_E4, NOTE_G4, NOTE_A4, NOTE_A4, NOTE_G4, NOTE_E4, NOTE_E4, NOTE_D4, NOTE_D4, NOTE_C4};

// 各部品が使用するピンの定義（2-9はキー、10はブザー、11は赤LED、12はモード切替ボタン）
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
const int PIN_ORIGINAL_END_BUTTON = 12;

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
const int TONE_HIGH_DO = NOTE_C5;

// Keypadイベント定義
const int KEY_NONE = 0;
const int KEY_TURN_SWITCH = 9; // '0' キーをターン切替に使う

// 1-3 State constants and state variable
const int STATE_ORIGINAL_WAIT = 0;
const int STATE_ORIGINAL_RECORD = 1;
const int STATE_MIMIC_WAIT = 2;
const int STATE_COMPARE = 3;
const int STATE_CORRECT_NOTIFY = 4;
const int STATE_WRONG_NOTIFY = 5;
int currentState = STATE_ORIGINAL_WAIT;

// 1-5 Processing constants
const int MAX_SEQUENCE_LENGTH = 20;
const unsigned long MIMIC_TIMEOUT_MS = 3000;
const int INPUT_TONE_DURATION_MS = 100;
const int CORRECT_TONE_DURATION_MS = 500;
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
bool coverage[COV_MAX] = {false};

const unsigned long STATE_LOG_INTERVAL_MS = 1000;
unsigned long lastStateLogMs = 0;
int lastLoggedState = -1;

const char* getStateName(int state) {
  switch (state) {
    case STATE_ORIGINAL_WAIT: return "STATE_ORIGINAL_WAIT";
    case STATE_ORIGINAL_RECORD: return "STATE_ORIGINAL_RECORD";
    case STATE_MIMIC_WAIT: return "STATE_MIMIC_WAIT";
    case STATE_COMPARE: return "STATE_COMPARE";
    case STATE_CORRECT_NOTIFY: return "STATE_CORRECT_NOTIFY";
    case STATE_WRONG_NOTIFY: return "STATE_WRONG_NOTIFY";
    default: return "STATE_UNKNOWN";
  }
}

void logStateAction(const char* action) {
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
}

// カバレッジ回数を表示　ターン終了時に出力してますか
void printCoverageReport() {
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
}

void resetGameState() {
  originalLength = 0;
  compareIndex = 0;
  mimicCount = 0;
  missDetected = false;
  isAllMatch = false;
  currentMode = MODE_ORIGINAL_INPUT;
  currentState = STATE_ORIGINAL_WAIT;
}

void blinkWrongLed() {
  for (int i = 0; i < WRONG_LED_BLINK_COUNT; i++) {
    digitalWrite(PIN_LED_RED, HIGH);
    delay(WRONG_LED_BLINK_MS);
    digitalWrite(PIN_LED_RED, LOW);
    delay(WRONG_LED_BLINK_MS);
  }
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
  coverage[COV_DEFAULT_MODE_PREPARE] = true;
}

void playDefaultSequence() {
  for (int i = 0; i < originalLength; i++) {
    playMappedTone(originalSeq[i]);
    delay(INPUT_TONE_DURATION_MS + 60);
  }
  noTone(PIN_BUZZER);
}

// 初期設定
void setup() {
  coverage[COV_SETUP] = true;

  // ボタンのピンとモード設定　INPUT_PULLUPは内部プルアップ抵抗とは
  pinMode(PIN_ORIGINAL_END_BUTTON, INPUT_PULLUP);

  // パッシブブザーとLEDはOUTPUT
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_LED_RED, OUTPUT);
  digitalWrite(PIN_LED_RED, LOW); // LEDを消灯状態に

  // ゲームの初期化（状態、オリジナル音数、ミミック入力カウント、比較インデックス、全一致フラグ、ミス検出フラグ）
  currentState = STATE_ORIGINAL_WAIT;
  originalLength = 0;
  mimicCount = 0;
  compareIndex = 0;
  isAllMatch = false;
  missDetected = false;

  // シリアル通信の開始と初期化完了ログ
  Serial.begin(115200);
  Serial.println("[CONFIG] keypad mode=Keypad library");
  logStateAction("initialization complete");
}

void loop() {
  coverage[COV_LOOP] = true;

  // 入力とその時間の読み取り
  inputKey = readInputKey();
  unsigned long now = millis();

  // 該当キーと音をマッピング
  if (inputKey >= 1 && inputKey <= 8) {
    playMappedTone(inputKey);
  }

  switch (currentState) {
    case STATE_ORIGINAL_WAIT:
      coverage[COV_STATE_ORIGINAL_WAIT] = true;
      logStateAction("waiting original first key input");

      if (readOriginalEndButton()) {
        Serial.println("[ACTION] switch to default song mode");
        currentMode = MODE_DEFAULT_SONG;
        prepareDefaultSequence();
        playDefaultSequence();
        lastMimicInputMs = millis();
        currentState = STATE_MIMIC_WAIT;
      } else if (inputKey >= 1 && inputKey <= 8) {
        Serial.println("[ACTION] transition to STATE_ORIGINAL_RECORD and record first original key");
        currentMode = MODE_ORIGINAL_INPUT;
        currentState = STATE_ORIGINAL_RECORD;
        recordAndCompare(inputKey, ORIGINAL_MODE);
      }
      break;

    case STATE_ORIGINAL_RECORD:
      coverage[COV_STATE_ORIGINAL_RECORD] = true;
      logStateAction("recording original sequence");

      if (inputKey >= 1 && inputKey <= 8) {
        Serial.println("[ACTION] record original key");
        recordAndCompare(inputKey, ORIGINAL_MODE);
      }

      // ターン切替は keypad の '0' キーを使う
      if (inputKey == KEY_TURN_SWITCH && originalLength > 0) {
        Serial.println("[ACTION] original input finalized by keypad '0', move to STATE_MIMIC_WAIT");
        compareIndex = 0;
        mimicCount = 0;
        missDetected = false;
        isAllMatch = false;
        lastMimicInputMs = now;
        currentState = STATE_MIMIC_WAIT;
      }
      break;

    case STATE_MIMIC_WAIT:
      coverage[COV_STATE_MIMIC_WAIT] = true;
      logStateAction("waiting mimic first key input");

      if (inputKey >= 1 && inputKey <= 8) {
        Serial.println("[ACTION] transition to STATE_COMPARE and compare first mimic key");
        lastMimicInputMs = now;
        currentState = STATE_COMPARE;
        recordAndCompare(inputKey, MIMIC_MODE);

        int result = judgeGameResult();
        if (result == RESULT_CORRECT) {
          Serial.println("[ACTION] judge result=CORRECT, move to STATE_CORRECT_NOTIFY");
          currentState = STATE_CORRECT_NOTIFY;
        } else if (result == RESULT_WRONG) {
          Serial.println("[ACTION] judge result=WRONG, move to STATE_WRONG_NOTIFY");
          currentState = STATE_WRONG_NOTIFY;
        }
      }
      break;

    case STATE_COMPARE:
      coverage[COV_STATE_COMPARE] = true;
      logStateAction("comparing mimic input against original sequence");

      if (inputKey >= 1 && inputKey <= 8) {
        Serial.println("[ACTION] compare next mimic key");
        recordAndCompare(inputKey, MIMIC_MODE);
      }

      {
        int result = judgeGameResult();
        if (result == RESULT_CORRECT) {
          Serial.println("[ACTION] judge result=CORRECT, move to STATE_CORRECT_NOTIFY");
          currentState = STATE_CORRECT_NOTIFY;
        } else if (result == RESULT_WRONG) {
          Serial.println("[ACTION] judge result=WRONG, move to STATE_WRONG_NOTIFY");
          currentState = STATE_WRONG_NOTIFY;
        }
      }
      break;

    case STATE_CORRECT_NOTIFY:
      coverage[COV_STATE_CORRECT_NOTIFY] = true;
      logStateAction("playing correct notification tone and resetting turn");
      tone(PIN_BUZZER, TONE_HIGH_DO, CORRECT_TONE_DURATION_MS);
      delay(CORRECT_TONE_DURATION_MS);
      resetGameState();
      Serial.println("[ACTION] reset complete, move to STATE_ORIGINAL_WAIT");
      printCoverageReport();
      break;

    case STATE_WRONG_NOTIFY:
      coverage[COV_STATE_WRONG_NOTIFY] = true;
      logStateAction("blinking wrong notification LED and resetting turn");
      blinkWrongLed();
      resetGameState();
      Serial.println("[ACTION] reset complete, move to STATE_ORIGINAL_WAIT");
      printCoverageReport();
      break;

    default:
      Serial.println("[ACTION] unknown state detected, force STATE_ORIGINAL_WAIT");
      currentState = STATE_ORIGINAL_WAIT;
      break;
  }
}

int readInputKey() {
  char customKey = customKeypad.getKey();
  int rawKey = KEY_NONE;

  if (customKey >= '1' && customKey <= '8') {
    rawKey = customKey - '0';
  } else if (customKey == '0') {
    rawKey = KEY_TURN_SWITCH;
    coverage[COV_TURN_SWITCH_PRESS] = true;
  } else if (customKey == NO_KEY) {
    return KEY_NONE;
  } else {
    coverage[COV_READ_INPUT_INVALID] = true;
    return KEY_NONE;
  }

  unsigned long now = millis();
  if (now - lastDebounceTimeKey < DEBOUNCE_DELAY_MS) {
    return KEY_NONE;
  }

  lastStableInputKey = rawKey;
  lastDebounceTimeKey = now;

  if (rawKey >= 1 && rawKey <= 8) {
    coverage[COV_READ_INPUT_VALID] = true;
    Serial.print("[INPUT] key=");
    Serial.println(rawKey);
  } else if (rawKey == KEY_TURN_SWITCH) {
    Serial.println("[INPUT] turn_switch=0");
  }

  return rawKey;
}

bool readOriginalEndButton() {
  bool rawPressed = (digitalRead(PIN_ORIGINAL_END_BUTTON) == LOW);
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

  Serial.print("[INPUT] mode_button=");
  Serial.println(rawPressed ? "PRESSED" : "RELEASED");

  // 未押下 -> 押下のみ有効
  if (!previousStable && rawPressed) {
    coverage[COV_END_BUTTON_PRESS] = true;
    return true;
  }

  return false;
}

void playMappedTone(int key) {
  int frequency = 0;

  switch (key) {
    case 1: frequency = TONE_DO; break;
    case 2: frequency = TONE_RE; break;
    case 3: frequency = TONE_MI; break;
    case 4: frequency = TONE_FA; break;
    case 5: frequency = TONE_SO; break;
    case 6: frequency = TONE_LA; break;
    case 7: frequency = TONE_SI; break;
    case 8: frequency = TONE_HIGH_DO; break;
    default: break;
  }

  if (frequency > 0) {
    tone(PIN_BUZZER, frequency, INPUT_TONE_DURATION_MS);
  }
}

bool recordAndCompare(int key, int mode) {
  if (key < 1 || key > 8) {
    return false;
  }

  if (mode == ORIGINAL_MODE) {
    if (originalLength < MAX_SEQUENCE_LENGTH) {
      originalSeq[originalLength] = key;
      originalLength++;
      coverage[COV_RECORD_ORIGINAL] = true;
      return true;
    }

    // 上限に達した場合は追加しないが、動作継続扱いにする
    coverage[COV_RECORD_ORIGINAL_FULL] = true;
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
      coverage[COV_COMPARE_MATCH] = true;
      return true;
    }

    missDetected = true;
    coverage[COV_COMPARE_MISS] = true;
    return false;
  }

  return false;
}

int judgeGameResult() {
  if (missDetected) {
    coverage[COV_JUDGE_WRONG_MISS] = true;
    return RESULT_WRONG;
  }
  // 全て一致した場合
  if (originalLength > 0 && compareIndex == originalLength) {
    isAllMatch = true;
    coverage[COV_JUDGE_CORRECT] = true;
    return RESULT_CORRECT;
  }
  // タイムアウト判定（STATE_MIMIC_WAITも含める）
  if (currentState == STATE_COMPARE || currentState == STATE_MIMIC_WAIT) {
    unsigned long now = millis();
    if (now - lastMimicInputMs >= MIMIC_TIMEOUT_MS) {
      missDetected = true;
      coverage[COV_JUDGE_WRONG_TIMEOUT] = true;
      return RESULT_WRONG;
    }
  }

  coverage[COV_JUDGE_CONTINUE] = true;
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
