#include <Arduino.h>
#include <stdint.h>

// --- 状態定義 ---
#define STATE_WAIT 0              // 待機中
#define STATE_SPINNING 1          // 回転中
#define STATE_LEFT_STOP 2         // 左停止
#define STATE_CENTER_STOP 3       // 中央停止
#define STATE_RIGHT_STOP 4        // 右停止
#define STATE_JUDGE 5             // 判定中
#define STATE_REACH 6             // リーチ演出中
#define STATE_WIN_SOUND 7         // 当たり音再生中
#define STATE_RESULT_WAIT 8       // 結果待ち

uint8_t currentState = STATE_WAIT; // 現在の状態

// --- ピン定義 ---
constexpr uint8_t btnPins[3] = {4, 5, 6};      // スロット停止ボタンのピン
constexpr uint8_t buzzerPin = 9;               // ブザー出力ピン
constexpr uint8_t displayDataPin = 2;          // シフトレジスタ データピン
constexpr uint8_t displayClockPin = 3;         // シフトレジスタ クロックピン
constexpr uint8_t displayLatchPin = 7;         // シフトレジスタ ラッチピン
constexpr uint8_t digitPins[4] = {8, 10, A0, A1}; // 7セグ桁選択ピン

// --- 7セグ数字パターン ---
constexpr uint8_t SEGMENT_PATTERNS[10] = {
  0b00111111, // 0
  0b00000110, // 1
  0b01011011, // 2
  0b01001111, // 3
  0b01100110, // 4
  0b01101101, // 5
  0b01111101, // 6
  0b00000111, // 7
  0b01111111, // 8
  0b01101111  // 9
};

// --- 表示制御定数 ---
constexpr uint8_t DIGIT_ON_LEVEL = HIGH;        // 桁ON時の論理レベル
constexpr uint8_t DIGIT_OFF_LEVEL = LOW;        // 桁OFF時の論理レベル
constexpr uint8_t BLANK_DIGIT = 0xFF;           // 空白表示用
constexpr uint16_t DISPLAY_MUX_INTERVAL_MS = 2; // 多重化表示の切替間隔[ms]

// --- ゲーム進行用変数 ---
uint8_t isSpinning = 0;         // 回転中フラグ
uint8_t stoppedCount = 0;       // 停止済みスロット数
uint8_t isReachSpinActive = 0;  // リーチ時の回転中フラグ

// --- タイミング管理 ---
uint32_t lastSpinUpdateMs = 0;  // 最後にスロット数字を更新した時刻
#define SPIN_INTERVAL_MS 50      // スロット数字更新間隔[ms]
uint32_t lastDebounceMs[3] = {0, 0, 0}; // ボタンデバウンス用時刻
#define DEBOUNCE_INTERVAL_MS 50  // デバウンス判定間隔[ms]

// --- ボタン状態管理 ---
uint8_t prevButtonLevel[3] = {HIGH, HIGH, HIGH};   // 前回読み取り値
uint8_t stableButtonLevel[3] = {HIGH, HIGH, HIGH}; // 安定した値

// --- スロット・表示用 ---
uint8_t slotDigits[3] = {0, 0, 0};        // 各スロットの数字
uint8_t slotStopped[3] = {0, 0, 0};       // 各スロット停止フラグ
uint8_t displayDigits[4] = {BLANK_DIGIT, 0, 0, 0}; // 4桁表示バッファ
uint8_t currentDisplayIndex = 0;          // 現在点灯中の桁インデックス
uint32_t lastDisplayMuxMs = 0;            // 最後に桁切替した時刻

// --- リーチ・当たり管理 ---
uint8_t reachEnabled = 1;     // リーチ演出有効フラグ
uint8_t isReach = 0;          // リーチ中フラグ
int8_t reachSlotIndex = -1;   // リーチ対象スロット番号
uint8_t hitFlag = 0;          // 当たりフラグ
uint16_t hitCount = 0;        // 累計当たり回数
uint16_t playCount = 0;       // 累計プレイ回数

// スロットの回転を開始する
void startSpin();
// 回転中のスロット数字を更新する
void updateSpinningDigits();
// ボタン押下を検出し、状態遷移や停止処理を行う
void handleButtonPress();
// 揃ったか判定し、当たり音を鳴らす
uint8_t checkHitAndPlaySound();
// リーチか判定し、リーチ音を鳴らす
uint8_t checkReachAndPlayCue();
// リーチ時の最後のスロットを回す
void spinLastSlotForReach();
// 表示用バッファにスロット数字をセットする
void renderDisplay();
// 4桁7セグの多重化表示を1桁ずつ実行する
void refreshDisplayTick();
// シフトレジスタ経由でセグメントパターンを出力する
void outputSegments(uint8_t pattern);
// 全桁を消灯する
void disableAllDigits();
// ノイズ除去付きでボタン押下を検出する
uint8_t isDebouncedPress(int pin);

// ピン番号からボタンインデックスを取得
int8_t getButtonIndexByPin(int pin);
// 0-9の範囲に丸める
uint8_t clampDigit(uint8_t value);
// リーチ音を鳴らす
void playReachCue();
// 当たり音を鳴らす
void playWinSound();
// リーチ音（短い単音）
// リーチ時の短い音を鳴らす
void playReachCue() {
  // 1000Hzの音を200ms鳴らす
  tone(buzzerPin, 1000, 200);
  delay(220); // 音が鳴り終わるまで少し待つ
  noTone(buzzerPin);
}

// アタリ音（簡単なメロディ）
// 当たり時のメロディを鳴らす
void playWinSound() {
  int melody[] = { 1319, 1568, 1760, 2093 }; // E6, G6, A6, C7
  int noteDurations[] = { 150, 150, 150, 300 };
  for (int i = 0; i < 4; i++) {
    tone(buzzerPin, melody[i], noteDurations[i]);
    delay(noteDurations[i] + 30);
  }
  noTone(buzzerPin);
}

// 初期化処理（ピン設定・乱数初期化・初期表示など）
void setup() {
  Serial.begin(9600);
  Serial.println("[setup] 開始");
  for (uint8_t i = 0; i < 3; ++i) {
    pinMode(btnPins[i], INPUT_PULLUP);
  }
  pinMode(buzzerPin, OUTPUT);
  pinMode(displayDataPin, OUTPUT);
  pinMode(displayClockPin, OUTPUT);
  pinMode(displayLatchPin, OUTPUT);
  for (uint8_t i = 0; i < 4; ++i) {
    pinMode(digitPins[i], OUTPUT);
  }
  disableAllDigits();
  outputSegments(0x00);

  randomSeed(analogRead(A2));

  for (uint8_t i = 0; i < 3; ++i) {
    slotDigits[i] = random(10);
  }
  renderDisplay();

  currentState = STATE_WAIT;
  stoppedCount = 0;
  isSpinning = 0;
  Serial.println("[setup] 完了");
}

// メインループ（表示更新・状態遷移・ゲーム進行）
void loop() {
  Serial.print("[loop] state: "); Serial.println(currentState);
  refreshDisplayTick();

  if (currentState != STATE_REACH) {
    Serial.println("[loop] handleButtonPress呼び出し");
    handleButtonPress();
  }

  switch (currentState) {
    case STATE_WAIT:
      Serial.println("[loop] STATE_WAIT");
      break;

    case STATE_SPINNING:
      Serial.println("[loop] STATE_SPINNING");
      updateSpinningDigits();
      break;
    case STATE_LEFT_STOP:
      Serial.println("[loop] STATE_LEFT_STOP");
      updateSpinningDigits();
      break;
    case STATE_CENTER_STOP:
      Serial.println("[loop] STATE_CENTER_STOP");
      updateSpinningDigits();
      break;
    case STATE_RIGHT_STOP:
      Serial.println("[loop] STATE_RIGHT_STOP");
      updateSpinningDigits();
      break;

    case STATE_JUDGE:
      Serial.println("[loop] STATE_JUDGE");
      if (checkHitAndPlaySound()) {
        Serial.println("[loop] 当たり判定: HIT");
      } else if (checkReachAndPlayCue()) {
        Serial.println("[loop] リーチ判定: REACH");
      } else {
        Serial.println("[loop] 判定: ハズレ");
        currentState = STATE_RESULT_WAIT;
      }
      break;

    case STATE_REACH:
      Serial.println("[loop] STATE_REACH");
      spinLastSlotForReach();
      break;

    case STATE_WIN_SOUND:
      Serial.println("[loop] STATE_WIN_SOUND");
      playWinSound();
      currentState = STATE_RESULT_WAIT;
      break;

    case STATE_RESULT_WAIT:
      Serial.println("[loop] STATE_RESULT_WAIT");
      break;
  }
}

// スロットの回転を開始し、状態を初期化
void startSpin() {
  Serial.println("[startSpin] 呼び出し");
  for (uint8_t i = 0; i < 3; ++i) {
    slotStopped[i] = 0;
  }

  stoppedCount = 0;
  isSpinning = 1;
  isReach = 0;
  isReachSpinActive = 0;
  reachSlotIndex = -1;
  hitFlag = 0;

  lastSpinUpdateMs = millis();
  currentState = STATE_SPINNING;
  ++playCount;
  Serial.println("[startSpin] 状態初期化完了");
}

// 回転中のスロット数字をランダムで更新
void updateSpinningDigits() {
  Serial.println("[updateSpinningDigits] 呼び出し");
  uint32_t now = millis();
  if (now - lastSpinUpdateMs < SPIN_INTERVAL_MS) {
    Serial.println("[updateSpinningDigits] 50ms未満: スキップ");
    return;
  }

  lastSpinUpdateMs = now;

  for (uint8_t i = 0; i < 3; ++i) {
    if (!slotStopped[i]) {
      slotDigits[i] = random(10);
      Serial.print("[updateSpinningDigits] slotDigits[");
      Serial.print(i);
      Serial.print("] = ");
      Serial.println(slotDigits[i]);
    }
  }

  renderDisplay();
  Serial.println("[updateSpinningDigits] 表示更新");
}

// ボタン押下を検出し、スロット停止や回転開始を制御
void handleButtonPress() {
  Serial.println("[handleButtonPress] 呼び出し");
  int8_t pressedIndex = -1;
  Serial.print("[handleButtonPress] currentState: ");
  Serial.println(currentState);

  for (uint8_t i = 0; i < 3; ++i) {
    if (isDebouncedPress(btnPins[i])) {
      Serial.print("[handleButtonPress] ボタン押下: index=");
      Serial.println(i);
      pressedIndex = i;
      break;
    }
  }

  if (pressedIndex < 0) {
    Serial.println("[handleButtonPress] ボタン押下なし");
    return;
  }

  if (currentState == STATE_WAIT || currentState == STATE_RESULT_WAIT) {
    Serial.println("[handleButtonPress] startSpin呼び出し");
    startSpin();
    return;
  }

  if (currentState != STATE_SPINNING &&
      currentState != STATE_LEFT_STOP &&
      currentState != STATE_CENTER_STOP &&
      currentState != STATE_RIGHT_STOP) {
    Serial.println("[handleButtonPress] 回転系状態でないため無視");
    return;
  }

  if (slotStopped[pressedIndex]) {
    Serial.println("[handleButtonPress] 既に停止済みのスロット");
    return;
  }

  slotStopped[pressedIndex] = 1;
  ++stoppedCount;
  Serial.print("[handleButtonPress] stoppedCount: ");
  Serial.println(stoppedCount);

  if (stoppedCount == 1) {
    if (pressedIndex == 0) {
      currentState = STATE_LEFT_STOP;
      Serial.println("[handleButtonPress] STATE_LEFT_STOP");
    } else if (pressedIndex == 1) {
      currentState = STATE_CENTER_STOP;
      Serial.println("[handleButtonPress] STATE_CENTER_STOP");
    } else {
      currentState = STATE_RIGHT_STOP;
      Serial.println("[handleButtonPress] STATE_RIGHT_STOP");
    }
  } else if (stoppedCount == 2) {
    currentState = STATE_SPINNING;
    Serial.println("[handleButtonPress] 残り1桁回転中");
  } else {
    stoppedCount = 3;
    isSpinning = 0;
    currentState = STATE_JUDGE;
    Serial.println("[handleButtonPress] 全停止・判定へ");
  }

  renderDisplay();
}

// ノイズ除去付きでボタン押下を検出
uint8_t isDebouncedPress(int pin) {
  int8_t idx = getButtonIndexByPin(pin);
  if (idx < 0) {
    return 0;
  }

  uint8_t reading = digitalRead(pin);

  if (reading != prevButtonLevel[idx]) {
    lastDebounceMs[idx] = millis();
    prevButtonLevel[idx] = reading;
  }

  if ((millis() - lastDebounceMs[idx]) >= DEBOUNCE_INTERVAL_MS && reading != stableButtonLevel[idx]) {
    stableButtonLevel[idx] = reading;
    if (stableButtonLevel[idx] == LOW) {
      return 1;
    }
  }

  return 0;
}

// ピン番号からボタンインデックスを取得
int8_t getButtonIndexByPin(int pin) {
  for (uint8_t i = 0; i < 3; ++i) {
    if (btnPins[i] == pin) {
      return i;
    }
  }
  return -1;
}

// 表示用バッファにスロット数字をセット
void renderDisplay() {
  Serial.print("[renderDisplay] displayDigits: ");
  // 4桁の左端は空白、右3桁にスロット値を表示する。
  displayDigits[0] = BLANK_DIGIT;
  displayDigits[1] = clampDigit(slotDigits[0]);
  displayDigits[2] = clampDigit(slotDigits[1]);
  displayDigits[3] = clampDigit(slotDigits[2]);
  for (int i = 0; i < 4; ++i) {
    Serial.print(displayDigits[i]);
    Serial.print(",");
  }
  Serial.println();
}

// 4桁7セグの多重化表示を1桁ずつ実行
void refreshDisplayTick() {
  Serial.print("[refreshDisplayTick] index=");
  Serial.print(currentDisplayIndex);
  Serial.print(" value=");
  Serial.println(displayDigits[currentDisplayIndex]);
  uint32_t now = millis();
  if (now - lastDisplayMuxMs < DISPLAY_MUX_INTERVAL_MS) {
    return;
  }
  lastDisplayMuxMs = now;

  disableAllDigits();

  uint8_t value = displayDigits[currentDisplayIndex];
  uint8_t pattern = 0x00;
  if (value <= 9) {
    pattern = SEGMENT_PATTERNS[value];
  }
  outputSegments(pattern);

  digitalWrite(digitPins[currentDisplayIndex], DIGIT_ON_LEVEL);
  currentDisplayIndex = (currentDisplayIndex + 1) & 0x03;
}

// シフトレジスタ経由でセグメントパターンを出力
void outputSegments(uint8_t pattern) {
  Serial.print("[outputSegments] pattern=0b");
  for (int i = 7; i >= 0; --i) {
    Serial.print((pattern >> i) & 1);
  }
  Serial.println();
  digitalWrite(displayLatchPin, LOW);
  shiftOut(displayDataPin, displayClockPin, LSBFIRST, pattern);
  digitalWrite(displayLatchPin, HIGH);
}

// 全桁を消灯
void disableAllDigits() {
  Serial.println("[disableAllDigits] 全桁消灯");
  for (uint8_t i = 0; i < 4; ++i) {
    digitalWrite(digitPins[i], DIGIT_OFF_LEVEL);
  }
}

// 0-9の範囲に丸める
uint8_t clampDigit(uint8_t value) {
  if (value <= 9) {
    Serial.print("[clampDigit] 正常: ");
    Serial.println(value);
    return value;
  }
  Serial.print("[clampDigit] 異常値: ");
  Serial.println(value);
  return 9;
}

// 揃ったか判定し、当たり音を鳴らす
uint8_t checkHitAndPlaySound() {
  Serial.println("[checkHitAndPlaySound] 呼び出し");
  slotDigits[0] = clampDigit(slotDigits[0]);
  slotDigits[1] = clampDigit(slotDigits[1]);
  slotDigits[2] = clampDigit(slotDigits[2]);

  if (slotDigits[0] == slotDigits[1] && slotDigits[1] == slotDigits[2]) {
    hitFlag = 1;
    ++hitCount;
    currentState = STATE_WIN_SOUND;
    Serial.println("[checkHitAndPlaySound] HIT");
    return 1;
  }

  hitFlag = 0;
  Serial.println("[checkHitAndPlaySound] MISS");
  return 0;
}

// リーチか判定し、リーチ音を鳴らす
uint8_t checkReachAndPlayCue() {
  Serial.println("[checkReachAndPlayCue] 呼び出し");
  if (!reachEnabled) {
    Serial.println("[checkReachAndPlayCue] reachEnabled==false");
    isReach = 0;
    isReachSpinActive = 0;
    reachSlotIndex = -1;
    return 0;
  }

  slotDigits[0] = clampDigit(slotDigits[0]);
  slotDigits[1] = clampDigit(slotDigits[1]);
  slotDigits[2] = clampDigit(slotDigits[2]);

  if (slotDigits[0] == slotDigits[1] && slotDigits[1] == slotDigits[2]) {
    Serial.println("[checkReachAndPlayCue] 3桁一致→リーチなし");
    isReach = 0;
    isReachSpinActive = 0;
    reachSlotIndex = -1;
    return 0;
  }

  int8_t unmatched = -1;
  if (slotDigits[0] == slotDigits[1] && slotDigits[1] != slotDigits[2]) {
    unmatched = 2;
  } else if (slotDigits[0] == slotDigits[2] && slotDigits[0] != slotDigits[1]) {
    unmatched = 1;
  } else if (slotDigits[1] == slotDigits[2] && slotDigits[0] != slotDigits[1]) {
    unmatched = 0;
  }

  if (unmatched < 0) {
    Serial.println("[checkReachAndPlayCue] 2桁一致なし");
    isReach = 0;
    isReachSpinActive = 0;
    reachSlotIndex = -1;
    return 0;
  }

  isReach = 1;
  isReachSpinActive = 0;
  reachSlotIndex = unmatched;
  currentState = STATE_REACH;
  Serial.print("[checkReachAndPlayCue] REACH: reachSlotIndex=");
  Serial.println(reachSlotIndex);
  playReachCue();
  return 1;
}

// リーチ時の最後のスロットを回す
void spinLastSlotForReach() {
  Serial.println("[spinLastSlotForReach] 呼び出し");
  if (reachSlotIndex < 0 || reachSlotIndex > 2) {
    Serial.println("[spinLastSlotForReach] reachSlotIndex異常");
    isReach = 0;
    isReachSpinActive = 0;
    isSpinning = 0;
    currentState = STATE_RESULT_WAIT;
    return;
  }

  if (!isReachSpinActive) {
    for (uint8_t i = 0; i < 3; ++i) {
      if (isDebouncedPress(btnPins[i])) {
        Serial.print("[spinLastSlotForReach] リーチ回転開始: index=");
        Serial.println(i);
        isReachSpinActive = 1;
        slotStopped[reachSlotIndex] = 0;
        stoppedCount = 2;
        isSpinning = 1;
        lastSpinUpdateMs = millis();
        return;
      }
    }
    return;
  }

  uint32_t now = millis();
  if (now - lastSpinUpdateMs >= SPIN_INTERVAL_MS) {
    lastSpinUpdateMs = now;
    slotDigits[reachSlotIndex] = random(10);
    Serial.print("[spinLastSlotForReach] リーチ回転中: digit=");
    Serial.print(reachSlotIndex);
    Serial.print(" value=");
    Serial.println(slotDigits[reachSlotIndex]);
    renderDisplay();
  }

  if (isDebouncedPress(btnPins[reachSlotIndex])) {
    Serial.println("[spinLastSlotForReach] リーチ停止");
    slotStopped[reachSlotIndex] = 1;
    stoppedCount = 3;
    isReach = 0;
    isReachSpinActive = 0;
    isSpinning = 0;
    currentState = STATE_JUDGE;
    renderDisplay();
  }
}
