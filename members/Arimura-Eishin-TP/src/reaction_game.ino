#include <LiquidCrystal.h>

// ハードウェア設定
const int BUTTON_PIN = 2;   // INPUT_PULLUP: 押下時 = LOW
const int BUZZER_PIN = 6;
const int LCD_RS_PIN = 7;
const int LCD_E_PIN = 8;
const int LCD_D4_PIN = 9;
const int LCD_D5_PIN = 10;
const int LCD_D6_PIN = 11;
const int LCD_D7_PIN = 12;

LiquidCrystal lcd(LCD_RS_PIN, LCD_E_PIN, LCD_D4_PIN, LCD_D5_PIN, LCD_D6_PIN, LCD_D7_PIN);

enum GameState {
  STATE_IDLE,
  STATE_READY,
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

GameState currentState = STATE_IDLE;

// 時間管理
unsigned long stateEnteredMs = 0;
unsigned long waitStartedMs = 0;
unsigned long randomWaitMs = 0;
unsigned long goSignalUs = 0;

// スコア管理
unsigned long reactionTimeUs = 0;
unsigned long bestTimeUs = 4294967295UL;
unsigned long totalReactionUs = 0;
unsigned long playCount = 0;
unsigned long averageUs = 0;
unsigned long lastResultReactionUs = 0;
unsigned long resultRefreshAtMs = 0;

bool showAverageInResult = false;
bool averageShown = false;
bool needsResultRefresh = false;

// ボタンのデバウンス
bool stableButtonLevel = HIGH;
bool lastRawLevel = HIGH;
unsigned long lastDebounceMs = 0;
const unsigned long DEBOUNCE_MS = 50;
const unsigned long GO_TIMEOUT_MS = 3000;
const unsigned long READY_DISPLAY_MS = 800;
const unsigned long RESULT_DISPLAY_MS = 2000;
const unsigned long AVERAGE_DISPLAY_MS = 2000;
const int RANKING_SIZE = 5;
const unsigned long EMPTY_RANK = 4294967295UL;

unsigned long rankingUs[RANKING_SIZE] = {
  EMPTY_RANK,
  EMPTY_RANK,
  EMPTY_RANK,
  EMPTY_RANK,
  EMPTY_RANK
};

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
void updateRanking(unsigned long reactionUs);
void printRanking();

// ハードウェアを初期化し、初期画面を表示する。
void setup() {
  Serial.begin(9600);

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);

  lcd.begin(16, 2);
  randomSeed(analogRead(A0));
  enterState(STATE_IDLE);
  printRanking();
}

// 状態マシンを1ステップ実行する。
void loop() {
  switch (currentState) {
    // 初期待機: スタート押下で READY へ。
    case STATE_IDLE:
      if (readButton()) {
        enterState(STATE_READY);
      }
      break;

    // READY表示中: 表示時間経過後の押下で WAIT へ。
    case STATE_READY:
      if (millis() - stateEnteredMs >= READY_DISPLAY_MS && readButton()) {
        enterState(STATE_WAIT);
      }
      break;

    // ランダム待機: 待機完了で GO、先押しで MISS。
    case STATE_WAIT:
      if (waitRandom()) {
        goSignalUs = micros();
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("PUSH!           ");
        lcd.setCursor(0, 1);
        lcd.print("                ");
        playSound(SOUND_CUE);
        enterState(STATE_GO);
      } else if (checkFalseStart()) {
        // 境界時刻では公平性を保つため、待機成立時はGOを優先する。
        enterState(STATE_MISS);
      }
      break;

    // 合図後の計測状態: 押下で記録、時間切れで TIMEOUT 扱い。
    case STATE_GO:
      if (readButton()) {
        reactionTimeUs = measureReaction();
        updateRanking(reactionTimeUs);
        bool isBest = updateBest(reactionTimeUs);
        lastResultReactionUs = reactionTimeUs;
        displayResult(reactionTimeUs);
        printRanking();
        if (isBest) {
          playSound(SOUND_BEST);
          if (LCD_E_PIN == BUZZER_PIN) {
            // ピン共有時はtone中にLCDが乱れるため、終了後に再描画する。
            needsResultRefresh = true;
            resultRefreshAtMs = millis() + 160;
          }
        }

        // 平均表示に使うデータをこの時点で確定する。
        showAverageInResult = (playCount > 0);
        averageShown = false;
        if (showAverageInResult) {
          averageUs = totalReactionUs / playCount;
        }
        enterState(STATE_RESULT);
      } else if (millis() - stateEnteredMs >= GO_TIMEOUT_MS) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("TIMEOUT");
        lcd.setCursor(0, 1);
        lcd.print("Try again");

        showAverageInResult = false;
        averageShown = false;
        needsResultRefresh = false;
        enterState(STATE_RESULT);
      }
      break;

    // 結果表示: Time/Best 表示後、Average を表示して READY へ戻る。
    case STATE_RESULT:
      if (needsResultRefresh && (long)(millis() - resultRefreshAtMs) >= 0) {
        displayResult(lastResultReactionUs);
        needsResultRefresh = false;
      }

      if (showAverageInResult) {
        unsigned long elapsedMs = millis() - stateEnteredMs;

        if (!averageShown && elapsedMs >= RESULT_DISPLAY_MS) {
          displayAverage(averageUs);
          averageShown = true;
        }

        if (averageShown && elapsedMs >= RESULT_DISPLAY_MS + AVERAGE_DISPLAY_MS) {
          enterState(STATE_READY);
        }
      } else if (millis() - stateEnteredMs >= RESULT_DISPLAY_MS) {
        enterState(STATE_READY);
      }
      break;

    // フライング表示: 1.5秒後の押下で READY へ。
    case STATE_MISS:
      if (millis() - stateEnteredMs >= 1500 && readButton()) {
        enterState(STATE_READY);
      }
      break;
  }
}

// デバウンス付きのボタン入力。押下イベントごとに1回だけtrueを返す。
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

// GO合図からの反応時間をマイクロ秒で計測する。
unsigned long measureReaction() {
  return micros() - goSignalUs;
}

// ランダム待機時間が経過したかを判定する。
bool waitRandom() {
  return (millis() - waitStartedMs) >= randomWaitMs;
}

// 現在の反応時間とベスト記録をLCDに表示する。
void displayResult(unsigned long reactionUs) {
  unsigned long reactionMs = reactionUs / 1000;
  unsigned long bestMs = bestTimeUs / 1000;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("                ");
  lcd.setCursor(0, 1);
  lcd.print("                ");

  lcd.setCursor(0, 0);
  lcd.print("Time:");
  lcd.print(reactionMs);
  lcd.print("ms");

  lcd.setCursor(0, 1);
  lcd.print("Best:");
  lcd.print(bestMs);
  lcd.print("ms");
}

// ベスト・合計・回数を更新し、今回が新記録かを返す。
bool updateBest(unsigned long reactionUs) {
  bool isBest = reactionUs < bestTimeUs;

  if (isBest) {
    bestTimeUs = reactionUs;
  }

  totalReactionUs += reactionUs;
  playCount++;

  return isBest;
}

// WAIT状態中の押下をフライングとして検出する。
bool checkFalseStart() {
  return readButton();
}

// 合図・ミス・ベスト更新時の効果音を鳴らす。
void playSound(int soundType) {
  // ブザーとLCDのEを共有するとtone()で表示が乱れるためスキップする。
  if (BUZZER_PIN == LCD_E_PIN) {
    return;
  }

  if (soundType == SOUND_CUE) {
    tone(BUZZER_PIN, 1760, 120);
  } else if (soundType == SOUND_MISS) {
    tone(BUZZER_PIN, 330, 250);
  } else if (soundType == SOUND_BEST) {
    // お祝いメロディー（ミ -> ソ -> 高いド）
    tone(BUZZER_PIN, 1319, 110);
    delay(130);
    tone(BUZZER_PIN, 1568, 110);
    delay(130);
    tone(BUZZER_PIN, 2093, 180);
  }
}

// 平均反応時間をLCDに表示する。
void displayAverage(unsigned long avgUs) {
  unsigned long avgMs = avgUs / 1000;
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("                ");
  lcd.setCursor(0, 1);
  lcd.print("                ");

  lcd.setCursor(0, 0);
  lcd.print("Average:");
  lcd.setCursor(0, 1);
  lcd.print(avgMs);
  lcd.print("ms");
}

// 次の状態へ遷移し、その状態に対応するLCD表示へ更新する。
void enterState(GameState next) {
  currentState = next;
  stateEnteredMs = millis();

  if (next != STATE_RESULT) {
    needsResultRefresh = false;
  }

  if (next == STATE_IDLE) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Press button");
    lcd.setCursor(0, 1);
    lcd.print("to start");
  } else if (next == STATE_READY) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("READY");
    lcd.setCursor(0, 1);
    lcd.print("Get set...");
  } else if (next == STATE_WAIT) {
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

// 反応時間ランキング（TOP5）を昇順で更新する。
void updateRanking(unsigned long reactionUs) {
  for (int i = 0; i < RANKING_SIZE; i++) {
    if (reactionUs < rankingUs[i]) {
      for (int j = RANKING_SIZE - 1; j > i; j--) {
        rankingUs[j] = rankingUs[j - 1];
      }
      rankingUs[i] = reactionUs;
      return;
    }
  }
}

// シリアルモニタにランキングを表示する（電源OFFでリセット）。
void printRanking() {
  Serial.println(F("=== RANKING TOP5 (ms) ==="));

  bool hasRecord = false;
  for (int i = 0; i < RANKING_SIZE; i++) {
    if (rankingUs[i] == EMPTY_RANK) {
      continue;
    }

    hasRecord = true;
    Serial.print(i + 1);
    Serial.print(F(". "));
    Serial.print(rankingUs[i] / 1000);
    Serial.println(F(" ms"));
  }

  if (!hasRecord) {
    Serial.println(F("No records yet"));
  }

  Serial.println();
}
