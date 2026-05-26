// Detailed-design-based implementation for Arduino UNO R3.

#include <Keypad.h>
#include <pitches.h>

const int TONES[NUM_TONES] = {
    NOTE_C4, NOTE_C4, NOTE_G4, NOTE_G4, NOTE_A4, NOTE_A4, NOTE_G4, NOTE_EMPTY,
    NOTE_F4, NOTE_F4, NOTE_E4, NOTE_E4, NOTE_D4, NOTE_D4, NOTE_C4, NOTE_EMPTY};
const int TONES2[] = {
    NOTE_C4, NOTE_D4, NOTE_E4, NOTE_C4, NOTE_D4, NOTE_E4, NOTE_G4, NOTE_E4, NOTE_D4, NOTE_C4, NOTE_D4, NOTE_E4, NOTE_D4,
    NOTE_G4, NOTE_G4, NOTE_E4, NOTE_G4, NOTE_A4, NOTE_A4, NOTE_G4, NOTE_E4, NOTE_E4, NOTE_D4, NOTE_D4, NOTE_C4};

// 各部品が使用するピンの定義（2-9はキー、10はブザー、11は赤LED、12はオリジナル終了ボタン）
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

// 音のヘルツ定数は、include <pitches.h>から取得したものを用いてください。   
const int TONE_DO = 262;
const int TONE_RE = 294;
const int TONE_MI = 330;
const int TONE_FA = 349;
const int TONE_SO = 392;
const int TONE_LA = 440;
const int TONE_SI = 494;
const int TONE_HIGH_DO = 523;

// 1-3 State constants and state variable
const int STATE_ORIGINAL_WAIT = 0;
const int STATE_ORIGINAL_RECORD = 1;
const int STATE_MIMIC_WAIT = 2;
const int STATE_COMPARE = 3;
const int STATE_CORRECT_NOTIFY = 4;
const int STATE_WRONG_NOTIFY = 5;
int currentState = STATE_ORIGINAL_WAIT; // 現在の状態をオリジナル入力で初期化

// 1-5 Processing constants
const int MAX_SEQUENCE_LENGTH = 20; // 最大保存音数
const unsigned long MIMIC_TIMEOUT_MS = 3000; // ミミック入力のタイムアウト時間（ms）
const int INPUT_TONE_DURATION_MS = 100; // 入力音の再生時間（ms）
const int CORRECT_TONE_DURATION_MS = 500; // 正解音の再生時間（ms）
const int WRONG_LED_ON_MS = 1000; // 間違い時のLED点灯時間（ms）
const int DEBOUNCE_DELAY_MS = 50; // デバウンス時間（ms）
const int ORIGINAL_MODE = 0;
const int MIMIC_MODE = 1;
const int RESULT_CORRECT = 1;
const int RESULT_WRONG = -1;
const int RESULT_CONTINUE = 0;

// 1-4 Input and judge variables
int inputKey = 0; // 現在の入力キー（1-8、0は無効）
int originalSeq[20]; // オリジナル音を保存する配列
int expectedKey = 0; // ミミック入力で期待されるキー
int originalLength = 0; // オリジナル音の長さ
int mimicCount = 0; // ミミック入力のカウント
int compareIndex = 0; // 比較インデックス
unsigned long lastMimicInputMs = 0; // 最後のミミック入力時間
bool isAllMatch = false; // 全て一致したかどうか
bool missDetected = false; // ミスが検出されたかどうか
unsigned long lastDebounceTimeKey = 0; // キーのデバウンス時間
unsigned long lastDebounceTimeButton = 0; // ボタンのデバウンス時間
int lastStableInputKey = 0; // 最後に安定した入力キー
bool lastButtonStableState = false; // 最後に安定したボタン状態

// Coverage counters for manual test verification.
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
  COV_END_BUTTON_PRESS,
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
unsigned long coverage[COV_MAX] = {0};
const unsigned long STATE_LOG_INTERVAL_MS = 1000; // 状態ログの表示間隔
unsigned long lastStateLogMs = 0; // 最後に状態ログを表示した時間
int lastLoggedState = -1; // 最後にログを表示した状態

// 状態に対応した文章を返す関数
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
  Serial.print("setup="); Serial.println(coverage[COV_SETUP]);
  Serial.print("loop="); Serial.println(coverage[COV_LOOP]);
  Serial.print("state_original_wait="); Serial.println(coverage[COV_STATE_ORIGINAL_WAIT]);
  Serial.print("state_original_record="); Serial.println(coverage[COV_STATE_ORIGINAL_RECORD]);
  Serial.print("state_mimic_wait="); Serial.println(coverage[COV_STATE_MIMIC_WAIT]);
  Serial.print("state_compare="); Serial.println(coverage[COV_STATE_COMPARE]);
  Serial.print("state_correct_notify="); Serial.println(coverage[COV_STATE_CORRECT_NOTIFY]);
  Serial.print("state_wrong_notify="); Serial.println(coverage[COV_STATE_WRONG_NOTIFY]);
  Serial.print("read_input_valid="); Serial.println(coverage[COV_READ_INPUT_VALID]);
  Serial.print("read_input_invalid="); Serial.println(coverage[COV_READ_INPUT_INVALID]);
  Serial.print("end_button_press="); Serial.println(coverage[COV_END_BUTTON_PRESS]);
  Serial.print("record_original="); Serial.println(coverage[COV_RECORD_ORIGINAL]);
  Serial.print("record_original_full="); Serial.println(coverage[COV_RECORD_ORIGINAL_FULL]);
  Serial.print("compare_match="); Serial.println(coverage[COV_COMPARE_MATCH]);
  Serial.print("compare_miss="); Serial.println(coverage[COV_COMPARE_MISS]);
  Serial.print("judge_correct="); Serial.println(coverage[COV_JUDGE_CORRECT]);
  Serial.print("judge_wrong_miss="); Serial.println(coverage[COV_JUDGE_WRONG_MISS]);
  Serial.print("judge_wrong_timeout="); Serial.println(coverage[COV_JUDGE_WRONG_TIMEOUT]);
  Serial.print("judge_continue="); Serial.println(coverage[COV_JUDGE_CONTINUE]);
}

// 初期設定
void setup() {
  coverage[COV_SETUP]++;

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
  coverage[COV_LOOP]++;

  // 入力とその時間の読み取り
  inputKey = readInputKey();
  unsigned long now = millis();

  // 該当キーと音をマッピング
  if (inputKey >= 1 && inputKey <= 8) {
    playMappedTone(inputKey);
  }

  // オリジナル記録入力数を入力時に加算していないため、上限の管理ができていない気がします。
  switch (currentState) {
    case STATE_ORIGINAL_WAIT:
      coverage[COV_STATE_ORIGINAL_WAIT]++;
      logStateAction("waiting original first key input"); // logStateActionは定義した関数で、状態とアクションをシリアル出力する
      // 1-8のキーが入力されたら、記録状態に遷移し、音を配列に格納（初回のみ別実行）
      if (inputKey >= 1 && inputKey <= 8) {
        Serial.println("[ACTION] transition to STATE_ORIGINAL_RECORD and record first original key");
        currentState = STATE_ORIGINAL_RECORD;
        recordAndCompare(inputKey, ORIGINAL_MODE);
      }
      break;

    case STATE_ORIGINAL_RECORD:
      coverage[COV_STATE_ORIGINAL_RECORD]++;
      logStateAction("recording original sequence");
      if (inputKey >= 1 && inputKey <= 8) {
        Serial.println("[ACTION] record original key");
        recordAndCompare(inputKey, ORIGINAL_MODE);
      }

      // ボタンが押されたらオリジナルの入力を終了し、ミミックの入力状態に遷移
      if (readOriginalEndButton()) {
        Serial.println("[ACTION] original input finalized, move to STATE_MIMIC_WAIT");
        compareIndex = 0;
        mimicCount = 0;
        missDetected = false;
        isAllMatch = false;
        lastMimicInputMs = now;
        currentState = STATE_MIMIC_WAIT;
      }
      break;

    case STATE_MIMIC_WAIT:
      coverage[COV_STATE_MIMIC_WAIT]++;
      logStateAction("waiting mimic first key input");
      if (inputKey >= 1 && inputKey <= 8) {
        Serial.println("[ACTION] transition to STATE_COMPARE and compare first mimic key");
        lastMimicInputMs = now;
        currentState = STATE_COMPARE;
        recordAndCompare(inputKey, MIMIC_MODE);

        int result = judgeGameResult(); // 引数にinputKeyが渡されていないのに、どのように判定するのか。
        // resultによる状態変化は1音ずつ行われているのではないか。
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
      coverage[COV_STATE_COMPARE]++;
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
      coverage[COV_STATE_CORRECT_NOTIFY]++;
      logStateAction("playing correct notification tone and resetting turn");
      tone(PIN_BUZZER, TONE_HIGH_DO, CORRECT_TONE_DURATION_MS);
      delay(CORRECT_TONE_DURATION_MS);


        // 共通の初期化処理を関数化
          resetGameState();
          Serial.println("[ACTION] reset complete, move to STATE_ORIGINAL_WAIT");
          printCoverageReport();
          break;

        case STATE_WRONG_NOTIFY:
          coverage[COV_STATE_WRONG_NOTIFY]++;
          logStateAction("lighting wrong notification LED and resetting turn");
          digitalWrite(PIN_LED_RED, HIGH);
          delay(WRONG_LED_ON_MS);
          digitalWrite(PIN_LED_RED, LOW);
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
  // Keypadライブラリによる入力読み込み
  char customKey = customKeypad.getKey();
  int rawKey = 0;

  if (customKey >= '1' && customKey <= '8') {
    rawKey = customKey - '0'; // 文字から数値に変換
  } else if (customKey != NO_KEY) {
    coverage[COV_READ_INPUT_INVALID]++;
    return 0;
  }

  unsigned long now = millis();
  // デバウンスタイム内はすべて無効
  if (now - lastDebounceTimeKey < DEBOUNCE_DELAY_MS) {
    return 0;
  }

  // 他のキーが押された場合、無効にする
  if (rawKey == 0) {
    return 0;
  }

  // 安定キーを確定し、その時間を更新
  lastStableInputKey = rawKey;
  lastDebounceTimeKey = now;

  if (lastStableInputKey >= 1 && lastStableInputKey <= 8) {
    coverage[COV_READ_INPUT_VALID]++;
    Serial.print("[INPUT] key=");
    Serial.println(lastStableInputKey);
    return lastStableInputKey;
  }

  return 0;
}


bool readOriginalEndButton() {
  bool rawPressed = (digitalRead(PIN_ORIGINAL_END_BUTTON) == LOW);
  unsigned long now = millis();
  // デバウンスタイム内はすべて無効
  if (now - lastDebounceTimeButton < DEBOUNCE_DELAY_MS) {
    return false;
  }
  bool previousStable = lastButtonStableState;
  lastButtonStableState = rawPressed;
  lastDebounceTimeButton = now;

  Serial.print("[INPUT] original_end_button=");
  Serial.println(rawPressed ? "PRESSED" : "RELEASED");

  // 立ち上がりエッジのみ検出
  if (!previousStable && rawPressed) {
    coverage[COV_END_BUTTON_PRESS]++;
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
      coverage[COV_RECORD_ORIGINAL]++;
      return true;
    } else {
      // 上限に達した場合は記録せず、フルカバレッジ記録
      coverage[COV_RECORD_ORIGINAL_FULL]++;
      return false;
    }
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
      coverage[COV_COMPARE_MATCH]++;
      return true;
    } else {
      missDetected = true;
      coverage[COV_COMPARE_MISS]++;
      return false;
    }
  }
  return false;
}


int judgeGameResult() {
  // ミスが検出された場合
  if (missDetected) {
    coverage[COV_JUDGE_WRONG_MISS]++;
    return RESULT_WRONG;
  }
  // 全て一致した場合
  if (originalLength > 0 && compareIndex == originalLength) {
    isAllMatch = true;
    coverage[COV_JUDGE_CORRECT]++;
    return RESULT_CORRECT;
  }
  // タイムアウト判定（STATE_MIMIC_WAITも含める）
  if (currentState == STATE_COMPARE || currentState == STATE_MIMIC_WAIT) {
    unsigned long now = millis();
    if (now - lastMimicInputMs >= MIMIC_TIMEOUT_MS) {
      missDetected = true;
      coverage[COV_JUDGE_WRONG_TIMEOUT]++;
      return RESULT_WRONG;
    }
  }
  coverage[COV_JUDGE_CONTINUE]++;
  return RESULT_CONTINUE;
}
// ゲーム状態の初期化処理（正解・不正解後の共通化）
void resetGameState() {
  originalLength = 0;
  compareIndex = 0;
  mimicCount = 0;
  missDetected = false;
  isAllMatch = false;
  currentState = STATE_ORIGINAL_WAIT;
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
