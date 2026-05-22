#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Hardware settings
const int BUTTON_PIN = 9;   // INPUT_PULLUP: pressed = LOW
const int BUZZER_PIN = 8;
const int LCD_ADDR = 0x27;

LiquidCrystal_I2C lcd(LCD_ADDR, 16, 2);

enum GameState {
  STATE_WAIT,
  STATE_GO,
  STATE_RESULT,
  STATE_MISS
};

enum SoundType {
  SOUND_CUE,
  SOUND_MISS,
  SOUND_BEST
};

GameState currentState = STATE_WAIT;

// Time management
unsigned long stateEnteredMs = 0;
unsigned long waitStartedMs = 0;
unsigned long randomWaitMs = 0;
unsigned long goSignalUs = 0;

// Score data
unsigned long reactionTimeUs = 0;
unsigned long bestTimeUs = 4294967295UL;
unsigned long totalReactionUs = 0;
unsigned long playCount = 0;

// Button debounce
bool stableButtonLevel = HIGH;
bool lastRawLevel = HIGH;
unsigned long lastDebounceMs = 0;
const unsigned long DEBOUNCE_MS = 50;
const unsigned long GO_TIMEOUT_MS = 3000;

void setup();
void loop();
bool readButton();
unsigned long measureReaction();
bool waitRandom();
void displayResult(unsigned long reactionUs);
bool updateBest(unsigned long reactionUs);
bool checkFalseStart();
void playSound(int soundType);
void displayAverage(unsigned long avgUs);
void enterState(GameState next);

void setup() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);

  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("READY");

  randomSeed(analogRead(A0));
  enterState(STATE_WAIT);
}

void loop() {
  switch (currentState) {
    case STATE_WAIT:
      if (checkFalseStart()) {
        enterState(STATE_MISS);
        break;
      }

      if (waitRandom()) {
        goSignalUs = micros();
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("PUSH!");
        playSound(SOUND_CUE);
        enterState(STATE_GO);
      }
      break;

    case STATE_GO:
      if (readButton()) {
        reactionTimeUs = measureReaction();
        bool isBest = updateBest(reactionTimeUs);
        displayResult(reactionTimeUs);
        if (isBest) {
          playSound(SOUND_BEST);
        }
        enterState(STATE_RESULT);
      } else if (millis() - stateEnteredMs >= GO_TIMEOUT_MS) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("TIMEOUT");
        lcd.setCursor(0, 1);
        lcd.print("Try again");
        enterState(STATE_RESULT);
      }
      break;

    case STATE_RESULT:
      if (millis() - stateEnteredMs >= 2500) {
        enterState(STATE_WAIT);
      }
      break;

    case STATE_MISS:
      if (millis() - stateEnteredMs >= 1500) {
        enterState(STATE_WAIT);
      }
      break;
  }
}

bool readButton() {
  bool raw = digitalRead(BUTTON_PIN);

  if (raw != lastRawLevel) {
    lastDebounceMs = millis();
    lastRawLevel = raw;
  }

  if (millis() - lastDebounceMs >= DEBOUNCE_MS && raw != stableButtonLevel) {
    stableButtonLevel = raw;
    if (stableButtonLevel == LOW) {
      return true;
    }
  }

  return false;
}

unsigned long measureReaction() {
  return micros() - goSignalUs;
}

bool waitRandom() {
  return (millis() - waitStartedMs) >= randomWaitMs;
}

void displayResult(unsigned long reactionUs) {
  unsigned long reactionMs = reactionUs / 1000;
  unsigned long bestMs = bestTimeUs / 1000;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Time:");
  lcd.print(reactionMs);
  lcd.print("ms");

  lcd.setCursor(0, 1);
  lcd.print("Best:");
  lcd.print(bestMs);
  lcd.print("ms");
}

bool updateBest(unsigned long reactionUs) {
  bool isBest = reactionUs < bestTimeUs;

  if (isBest) {
    bestTimeUs = reactionUs;
  }

  totalReactionUs += reactionUs;
  playCount++;

  return isBest;
}

bool checkFalseStart() {
  return readButton();
}

void playSound(int soundType) {
  if (soundType == SOUND_CUE) {
    tone(BUZZER_PIN, 1760, 120);
  } else if (soundType == SOUND_MISS) {
    tone(BUZZER_PIN, 330, 250);
  } else if (soundType == SOUND_BEST) {
    tone(BUZZER_PIN, 1319, 120);
  }
}

void displayAverage(unsigned long avgUs) {
  unsigned long avgMs = avgUs / 1000;
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Average:");
  lcd.setCursor(0, 1);
  lcd.print(avgMs);
  lcd.print("ms");
}

void enterState(GameState next) {
  currentState = next;
  stateEnteredMs = millis();

  if (next == STATE_WAIT) {
    waitStartedMs = millis();
    randomWaitMs = random(2000, 5001);

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WAIT...");

    lcd.setCursor(0, 1);
    lcd.print("Best:");
    if (bestTimeUs == 4294967295UL) {
      lcd.print("----");
    } else {
      lcd.print(bestTimeUs / 1000);
      lcd.print("ms");
    }
  } else if (next == STATE_MISS) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("MISS");
    playSound(SOUND_MISS);
  }
}
