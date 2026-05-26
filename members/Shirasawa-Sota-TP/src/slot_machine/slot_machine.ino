#include <Arduino.h>
#include <stdint.h>

#define STATE_WAIT 0
#define STATE_SPINNING 1
#define STATE_LEFT_STOP 2
#define STATE_CENTER_STOP 3
#define STATE_RIGHT_STOP 4
#define STATE_JUDGE 5
#define STATE_REACH 6
#define STATE_WIN_SOUND 7
#define STATE_RESULT_WAIT 8

uint8_t currentState = STATE_WAIT;
constexpr uint8_t btnPins[3] = {4, 5, 6};
constexpr uint8_t buzzerPin = 9;
constexpr uint8_t displayDataPin = 2;
constexpr uint8_t displayClockPin = 3;
constexpr uint8_t displayLatchPin = 7;
constexpr uint8_t digitPins[4] = {8, 10, A0, A1};

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

constexpr uint8_t DIGIT_ON_LEVEL = HIGH;
constexpr uint8_t DIGIT_OFF_LEVEL = LOW;
constexpr uint8_t BLANK_DIGIT = 0xFF;
constexpr uint16_t DISPLAY_MUX_INTERVAL_MS = 2;

uint8_t isSpinning = 0;
uint8_t stoppedCount = 0;
uint8_t isReachSpinActive = 0;

uint32_t lastSpinUpdateMs = 0;
#define SPIN_INTERVAL_MS 50
uint32_t lastDebounceMs[3] = {0, 0, 0};
#define DEBOUNCE_INTERVAL_MS 50

uint8_t prevButtonLevel[3] = {HIGH, HIGH, HIGH};
uint8_t stableButtonLevel[3] = {HIGH, HIGH, HIGH};

uint8_t slotDigits[3] = {0, 0, 0};
uint8_t slotStopped[3] = {0, 0, 0};
uint8_t displayDigits[4] = {BLANK_DIGIT, 0, 0, 0};
uint8_t currentDisplayIndex = 0;
uint32_t lastDisplayMuxMs = 0;
uint8_t reachEnabled = 1;
uint8_t isReach = 0;
int8_t reachSlotIndex = -1;
uint8_t hitFlag = 0;
uint16_t hitCount = 0;
uint16_t playCount = 0;

void startSpin();
void updateSpinningDigits();
void handleButtonPress();
uint8_t checkHitAndPlaySound();
uint8_t checkReachAndPlayCue();
void spinLastSlotForReach();
void renderDisplay();
void refreshDisplayTick();
void outputSegments(uint8_t pattern);
void disableAllDigits();
uint8_t isDebouncedPress(int pin);

int8_t getButtonIndexByPin(int pin);
uint8_t clampDigit(uint8_t value);
void playReachCue();
void playWinSound();
// リーチ音（短い単音）
void playReachCue() {
  // 1000Hzの音を200ms鳴らす
  tone(buzzerPin, 1000, 200);
  delay(220); // 音が鳴り終わるまで少し待つ
  noTone(buzzerPin);
}

// アタリ音（簡単なメロディ）
void playWinSound() {
  int melody[] = { 1319, 1568, 1760, 2093 }; // E6, G6, A6, C7
  int noteDurations[] = { 150, 150, 150, 300 };
  for (int i = 0; i < 4; i++) {
    tone(buzzerPin, melody[i], noteDurations[i]);
    delay(noteDurations[i] + 30);
  }
  noTone(buzzerPin);
}

void setup() {
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
}

void loop() {
  refreshDisplayTick();

  if (currentState != STATE_REACH) {
    handleButtonPress();
  }

  switch (currentState) {
    case STATE_WAIT:
      break;

    case STATE_SPINNING:
    case STATE_LEFT_STOP:
    case STATE_CENTER_STOP:
    case STATE_RIGHT_STOP:
      updateSpinningDigits();
      break;

    case STATE_JUDGE:
      if (checkHitAndPlaySound()) {
        // 状態遷移は関数内で処理
      } else if (checkReachAndPlayCue()) {
        // 状態遷移は関数内で処理
      } else {
        currentState = STATE_RESULT_WAIT;
      }
      break;

    case STATE_REACH:
      spinLastSlotForReach();
      break;

    case STATE_WIN_SOUND:
      playWinSound();
      currentState = STATE_RESULT_WAIT;
      break;

    case STATE_RESULT_WAIT:
      break;
  }
}

void startSpin() {
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
}

void updateSpinningDigits() {
  uint32_t now = millis();
  if (now - lastSpinUpdateMs < SPIN_INTERVAL_MS) {
    return;
  }

  lastSpinUpdateMs = now;

  for (uint8_t i = 0; i < 3; ++i) {
    if (!slotStopped[i]) {
      slotDigits[i] = random(10);
    }
  }

  renderDisplay();
}

void handleButtonPress() {
  int8_t pressedIndex = -1;

  for (uint8_t i = 0; i < 3; ++i) {
    if (isDebouncedPress(btnPins[i])) {
      pressedIndex = i;
      break;
    }
  }

  if (pressedIndex < 0) {
    return;
  }

  if (currentState == STATE_WAIT || currentState == STATE_RESULT_WAIT) {
    startSpin();
    return;
  }

  if (currentState != STATE_SPINNING &&
      currentState != STATE_LEFT_STOP &&
      currentState != STATE_CENTER_STOP &&
      currentState != STATE_RIGHT_STOP) {
    return;
  }

  if (slotStopped[pressedIndex]) {
    return;
  }

  slotStopped[pressedIndex] = 1;
  ++stoppedCount;

  if (stoppedCount == 1) {
    if (pressedIndex == 0) {
      currentState = STATE_LEFT_STOP;
    } else if (pressedIndex == 1) {
      currentState = STATE_CENTER_STOP;
    } else {
      currentState = STATE_RIGHT_STOP;
    }
  } else if (stoppedCount == 2) {
    currentState = STATE_SPINNING;
  } else {
    stoppedCount = 3;
    isSpinning = 0;
    currentState = STATE_JUDGE;
  }

  renderDisplay();
}

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

int8_t getButtonIndexByPin(int pin) {
  for (uint8_t i = 0; i < 3; ++i) {
    if (btnPins[i] == pin) {
      return i;
    }
  }
  return -1;
}

void renderDisplay() {
  // 4桁の左端は空白、右3桁にスロット値を表示する。
  displayDigits[0] = BLANK_DIGIT;
  displayDigits[1] = clampDigit(slotDigits[0]);
  displayDigits[2] = clampDigit(slotDigits[1]);
  displayDigits[3] = clampDigit(slotDigits[2]);
}

void refreshDisplayTick() {
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

void outputSegments(uint8_t pattern) {
  digitalWrite(displayLatchPin, LOW);
  shiftOut(displayDataPin, displayClockPin, LSBFIRST, pattern);
  digitalWrite(displayLatchPin, HIGH);
}

void disableAllDigits() {
  for (uint8_t i = 0; i < 4; ++i) {
    digitalWrite(digitPins[i], DIGIT_OFF_LEVEL);
  }
}

uint8_t clampDigit(uint8_t value) {
  if (value <= 9) {
    return value;
  }
  return 9;
}

uint8_t checkHitAndPlaySound() {
  slotDigits[0] = clampDigit(slotDigits[0]);
  slotDigits[1] = clampDigit(slotDigits[1]);
  slotDigits[2] = clampDigit(slotDigits[2]);

  if (slotDigits[0] == slotDigits[1] && slotDigits[1] == slotDigits[2]) {
    hitFlag = 1;
    ++hitCount;
    currentState = STATE_WIN_SOUND;
    return 1;
  }

  hitFlag = 0;
  return 0;
}

uint8_t checkReachAndPlayCue() {
  if (!reachEnabled) {
    isReach = 0;
    isReachSpinActive = 0;
    reachSlotIndex = -1;
    return 0;
  }

  slotDigits[0] = clampDigit(slotDigits[0]);
  slotDigits[1] = clampDigit(slotDigits[1]);
  slotDigits[2] = clampDigit(slotDigits[2]);

  if (slotDigits[0] == slotDigits[1] && slotDigits[1] == slotDigits[2]) {
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
    isReach = 0;
    isReachSpinActive = 0;
    reachSlotIndex = -1;
    return 0;
  }

  isReach = 1;
  isReachSpinActive = 0;
  reachSlotIndex = unmatched;
  currentState = STATE_REACH;
  playReachCue();
  return 1;
}

void spinLastSlotForReach() {
  if (reachSlotIndex < 0 || reachSlotIndex > 2) {
    isReach = 0;
    isReachSpinActive = 0;
    isSpinning = 0;
    currentState = STATE_RESULT_WAIT;
    return;
  }

  if (!isReachSpinActive) {
    for (uint8_t i = 0; i < 3; ++i) {
      if (isDebouncedPress(btnPins[i])) {
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
    renderDisplay();
  }

  if (isDebouncedPress(btnPins[reachSlotIndex])) {
    slotStopped[reachSlotIndex] = 1;
    stoppedCount = 3;
    isReach = 0;
    isReachSpinActive = 0;
    isSpinning = 0;
    currentState = STATE_JUDGE;
    renderDisplay();
  }
}
