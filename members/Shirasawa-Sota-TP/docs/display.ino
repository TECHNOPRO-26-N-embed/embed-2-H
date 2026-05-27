int latch = 9;
int clock = 10;
int data  = 8;

// segment 12,9,8,6 → Arduino D2,D3,D4,D5
int digitPins[4] = {2, 3, 4, 5}; // 1, 2, 3, 4

byte table[] = {
  0x3f, // 0
  0x06, // 1
  0x5b, // 2
  0x4f, // 3
  0x66, // 4
  0x6d, // 5
  0x7d, // 6
  0x07, // 7
  0x7f, // 8
  0x6f  // 9
};

int number = 1;
unsigned long lastTime = 0;

void setup() {
  pinMode(latch, OUTPUT);
  pinMode(clock, OUTPUT);
  pinMode(data, OUTPUT);

  for (int i = 0; i < 4; i++) {
    pinMode(digitPins[i], OUTPUT);
    digitalWrite(digitPins[i], HIGH); // 자리 OFF
  }
}

void sendData(byte value) {
  digitalWrite(latch, LOW);
  shiftOut(data, clock, MSBFIRST, value);
  digitalWrite(latch, HIGH);
}

void allDigitsOff() {
  for (int i = 0; i < 4; i++) {
    digitalWrite(digitPins[i], HIGH); // OFF
  }
}

void showOneDigit(int pos, int num) {
  allDigitsOff();

  sendData(table[num]);

  digitalWrite(digitPins[pos], LOW); // ON
  delay(2);
}

void showNumber(int num) {
  int d1 = num / 1000;
  int d2 = (num / 100) % 10;
  int d3 = (num / 10) % 10;
  int d4 = num % 10;

  if (num >= 1000) showOneDigit(0, d1);
  if (num >= 100)  showOneDigit(1, d2);
  if (num >= 10)   showOneDigit(2, d3);
  showOneDigit(3, d4);
}

void loop() {
  showNumber(number);

  if (millis() - lastTime >= 1) {
    lastTime = millis();

    number++;

    if (number > 9999) {
      number = 1;
    }
  }
}