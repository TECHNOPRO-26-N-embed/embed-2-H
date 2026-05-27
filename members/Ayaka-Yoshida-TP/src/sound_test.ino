const int buttonPin = 5;   // ボタン D5
const int ledPin    = 6;   // LED D6
const int buzzerPin = 7;   // ブザー D7

int ledState = LOW;
int lastButtonState = HIGH;

int notes[] = {262, 294, 330, 349, 392}; // ド(C),レ(D),ミ(E),ファ(F),ソ(G)の周波数
int noteCount = sizeof(notes)/sizeof(notes[0]);
int noteIndex = 0;
unsigned long lastNoteChange = 0;
const unsigned long noteDuration = 300; // 1音 300ms

void setup() {
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(ledPin, OUTPUT);
  pinMode(buzzerPin, OUTPUT);
  digitalWrite(ledPin, ledState);
  Serial.begin(9600);
}

void loop() {
  int buttonState = digitalRead(buttonPin);

  // トグル動作
  if (lastButtonState == HIGH && buttonState == LOW) {
    ledState = !ledState;
    digitalWrite(ledPin, ledState);
    Serial.print("LED State: ");
    Serial.println(ledState == HIGH ? "ON" : "OFF");
    delay(50); // チャタリング防止
  }
  lastButtonState = buttonState;

  if (ledState == HIGH) {
    // LED点灯中にドレミファソをループ
    unsigned long now = millis();
    if (now - lastNoteChange >= noteDuration) {
      tone(buzzerPin, notes[noteIndex]);
      lastNoteChange = now;
      noteIndex = (noteIndex + 1) % noteCount;
    }
  } else {
    noTone(buzzerPin); // LED消灯時は音も停止
    noteIndex = 0;     // ドからリセット
    lastNoteChange = millis();
  }
}