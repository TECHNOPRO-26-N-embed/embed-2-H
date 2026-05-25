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
uint8_t btnPins[3] = {4, 5, 6};
uint8_t buzzerPin = 9;
uint8_t displayClkPin = 2;
uint8_t displayDioPin = 3;

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
uint8_t isDebouncedPress(int pin);

int8_t getButtonIndexByPin(int pin);
uint8_t clampDigit(uint8_t value);
void playReachCue();
void playWinSound();

void setup() {
  for (uint8_t i = 0; i < 3; ++i) {
    pinMode(btnPins[i], INPUT_PULLUP);
  }
  pinMode(buzzerPin, OUTPUT);

  randomSeed(analogRead(A0));

  for (uint8_t i = 0; i < 3; ++i) {
    slotDigits[i] = random(10);
  }
  renderDisplay();

  currentState = STATE_WAIT;
  stoppedCount = 0;
  isSpinning = 0;
}

void loop() {
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
  // C言語ではTM1637ライブラリを使わず、直接ピン操作で実装する必要があります。
  // ここでは簡略化して、スロット値をシリアル出力する例を示します。
  Serial.print("Slot: ");
  for (uint8_t i = 0; i < 3; ++i) {
    Serial.print(slotDigits[i]);
    if (i < 2) {
      Serial.print(" ");
    }
  }
  Serial.println();
}
