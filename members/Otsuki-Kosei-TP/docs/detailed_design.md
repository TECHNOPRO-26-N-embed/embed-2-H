# 詳細設計書 — 組込み開発実習

<!-- 作成者: あなたの名前 / 日付: YYYY-MM-DD / グループ: 〇-〇 -->

> **このドキュメントの目的**
> 基本設計書（basic_design.md）で「**どのような構造で作るか**」を決めました。
> この詳細設計書では「**各処理を具体的にどう実装するか**」を決めます。
> 書き終わったとき、**コードの骨格がほぼ完成している**状態を目指してください。

> [!NOTE]
> **V字モデルにおける位置づけ**
> 詳細設計書 ←→ **単体テスト**（関数・部品ごとのテスト）が対応します。
> 「この関数が正しく動くか」の確認は Section 5 の単体テスト仕様書で計画します。
> ※ 必須機能全体が動くかの「結合テスト」は基本設計書（Section 6）に記載します。

---

## 0. 基本設計書との接続確認

| 項目 | basic_design.md から転記 |
|:--|:--|
| 作品タイトル | ブザーが鳴った長さを覚えて同じ長さを再現しようゲーム |
| 状態の種類（1-2 状態遷移から） | 5種類（電源ON / 初期化、待機中、計測中、判定中、結果表示）|
| 実装する関数の数（2-2 関数一覧から） | 　9個（setup, loop, readButton, updateOutput, generateTargetTime, checkElapsedTime, displayResult, restartGame, acceptInputTiming） |
| グローバル変数の合計バイト数（2-1 SRAM確認から） | 35B |

---

## 1. グローバル変数・定数の設計

> ※ 基本設計書（2-1 データ設計）をもとに、**型と初期値まで**決めます。
> ここで設計した変数は、この後の関数設計でそのまま使います。

```

  （自分のものを追加）
```
【ピン定義】（basic_design.md 3-1 から転記）
  const int PIN_BUTTON    = 2;   // タクトスイッチ（INPUT_PULLUP）
  const int PIN_LED_RED   = 9;   // 赤LED
  const int PIN_BUZZER    = 10;  // パッシブブザー

【状態管理】（basic_design.md 1-2 の状態名から転記）
  int currentState = 0;   // 0:初期化 1:待機 2:計測 3:判定 4:結果表示

【タイマー（millis()用）】（basic_design.md 2-3 から転記）
  unsigned long lastMillis_LED = 0;

【センサー・入力値】（basic_design.md 2-1 から転記）
  bool buttonPressed = false;
  bool buttonReleasedEvent = false;
  unsigned long startTime = 0;
  unsigned long elapsedTime = 0;
  unsigned long targetTime = 0;
  bool isCorrect = false;
  int resultRank = 0;   // 0:Miss 1:Good 2:Excellent
  bool inputAllowed = false;
  int ledState = 0; // 0:消灯 1:赤点灯 2:緑点灯

【その他のフラグ・カウンター】
  unsigned long lastDebounceTime = 0;  // 前回押下を確定した時刻
  const int DEBOUNCE_DELAY = 50;       // チャタリング判定時間（ms）
  const unsigned long INPUT_GUARD_MS = 20; // 入力受付の境界ガード時間
  int lastButtonReading = HIGH;        // 直前に読んだ生のボタン値

---

## 2. 各関数の詳細設計

> ※ 基本設計書（2-2 関数一覧）で定義した各関数の「中身」を設計します。
> **疑似コード**（日本語＋処理の流れ）で書いてください。実際のC++コードは書かなくてOKです。

---

### `setup()` — 初期化処理

```
【処理の流れ】
1. ピンモードを設定する
   - PIN_BUTTON  → INPUT_PULLUP
   - PIN_LED_*   → OUTPUT
   - PIN_BUZZER  → OUTPUT

2. ライブラリの初期化（使うものだけ）
   - 例: lcd.begin(16, 2)
   - 例: servo.attach(PIN_SERVO)

3. Serial.begin(9600)（デバッグ用）

4. 起動確認（任意）: 緑LEDを1秒点灯して消灯
```

**↓ 自分の setup() を設計してください**
```
【処理の流れ】
1. ピンモードを設定する
  - PIN_BUTTON を INPUT_PULLUP に設定する
  - PIN_LED_RED を OUTPUT に設定する
  - PIN_BUZZER を OUTPUT に設定する

2. 初期状態を設定する
  - currentState を 0（初期化）にする
  - LEDを消灯、ブザーを停止する
  - buttonPressed, buttonReleasedEvent, startTime, elapsedTime, targetTime, isCorrect, resultRank, inputAllowed, ledState を初期値にそろえる

3. 乱数シードを初期化する
  - randomSeed() を1回だけ呼び、起動ごとに目標時間系列が偏らないようにする

4. デバッグ用シリアル通信を開始する
  - Serial.begin(9600) を呼ぶ
  - 「setup complete」などの起動ログを1回だけ出力する

5. 起動確認を行う（任意）
  - 赤LEDを短時間点灯して消灯し、初期化完了を確認する
```

---

### `loop()` — メインループ

> ※ loop() は「状態ごとに何をするか」だけ書く。細かい処理は各関数に任せる。

**記入例（参考）：**

```
【処理の流れ】

＜毎ループ実行すること＞
  - 入力を読む（readButton(), acceptInputTiming() などを呼ぶ）
  - 現在時刻を取得: now = millis()

＜currentState が 0（初期化）のとき＞
  - 初期化処理を行う
  - 初期化完了で → currentState = 1

＜currentState が 1（待機中）のとき＞
  - 入力待機を行う
  - 押下確定で → currentState = 2

＜currentState が 2（計測中）のとき＞
  - 押下時間を計測する
  - 離上検出で → currentState = 3

＜currentState が 3（判定中）のとき＞
  - 正誤判定を行う
  - 判定完了で → currentState = 4

＜currentState が 4（結果表示）のとき＞
  - 結果を表示する
  - 再スタート操作で → currentState = 1
```

**↓ 自分の loop() を設計してください**
```
【処理の流れ】

＜毎ループ実行すること＞
- now = millis() を取得する
- buttonEvent = readButton() を呼んで押下イベントを取得する
- inputAllowed = acceptInputTiming(currentState, now) を呼んで入力可否を取得する
- updateOutput(currentState) を呼んで出力を反映する


＜currentState が 0（初期化）のとき＞
- 起動直後の初期化完了判定を行う
- generateTargetTime() を呼んで目標時間を決める
- 初期化が完了したら currentState を 1（待機中）にする


＜currentState が 1（待機中）のとき＞
- 入力受付条件を満たすまで待つ
- inputAllowed が true かつ buttonEvent が true なら startTime = now を記録する
- currentState を 2（計測中）にする


＜currentState が 2（計測中）のとき＞
- ボタンが押されている間の経過を監視する
- buttonReleasedEvent が true のときのみ
  - elapsedTime = now - startTime を計算する
  - currentState を 3（判定中）にする


＜currentState が 3（判定中）のとき＞
- resultRank = checkElapsedTime(elapsedTime, targetTime) で判定ランクを計算する
- isCorrect = (resultRank >= 1) で正誤フラグを更新する
- displayResult(resultRank) を呼ぶ
- currentState を 4（結果表示）にする


＜currentState が 4（結果表示）のとき＞
- 判定結果の表示を一定時間維持する
- 再スタート条件（ボタン押下）を満たしたら restartGame() を呼ぶ
- restartGame() 内で currentState を 1（待機中）へ戻す

```

---

### `readButton()` — チャタリングを除去したボタン状態の確定

**basic_design.md 2-2 との対応：** チャタリング処理済みのボタン状態を返す

**引数：** なし

**戻り値：** bool（押下が確定したとき true）

> [!CAUTION]
> `readButton()` の戻り値 `false` は「押下イベントなし」を意味し、
> それ単体では「離上イベント」を意味しない。
> 離上は `buttonReleasedEvent` を参照して判定すること。

```
【処理の流れ】
1. ボタンの生値を読み取る（INPUT_PULLUP のため押下時は LOW）
2. 前回確定時刻との差を確認し、50ms未満ならイベントを無視する
3. 50ms以上経過し、LOWエッジを検出したとき:
  - buttonPressed = true
  - buttonReleasedEvent = false
  - true（押下イベント）を返す
4. 50ms以上経過し、HIGHエッジを検出したとき:
  - buttonPressed = false
  - buttonReleasedEvent = true
  - false を返す（押下イベントは発生していない）
5. それ以外はイベントなしとして false を返す

【エラー・異常ケース】
- ピン値が不安定な場合: デバウンス条件を満たすまで未確定として扱う
```

---

### `updateOutput(int state)` — 状態に応じたLED/ブザー出力制御

**basic_design.md 2-2 との対応：** 現在の state に応じて LED/ブザーを制御

**引数：** state（int）: 現在状態

**戻り値：** なし（void）

```
【処理の流れ】
1. state を見て出力パターンを選択する
2. 待機中はLED消灯・ブザー停止、計測中はブザー鳴動、結果表示は resultRank に応じた点灯/点滅を行う
3. 点滅が必要な出力は millis() の経過時間で切り替える

【エラー・異常ケース】
- 未定義の state 値が来た場合: すべての出力を停止して安全側に倒す
```

---

### `generateTargetTime()` — 目標時間の生成

**basic_design.md 2-2 との対応：** ランダムな目標時間を生成

**引数：** なし

**戻り値：** unsigned long（生成した目標時間）

```
【処理の流れ】
1. 目標時間の範囲（例: 1000ms〜5000ms）を決める
2. その範囲で乱数を1つ生成する（乱数シードは setup() で初期化済み）
3. targetTime に保存し、同じ値を戻り値として返す

【エラー・異常ケース】
- 関数は引数を持たないため、範囲逆転のような設定不整合は単体テスト入力として直接注入できない
- そのため MIN/MAX 定数の整合性（MIN <= MAX）は設計レビュー項目として確認する

【テスト性確保の方針】
- 乱数依存のままだと下限/上限の到達確認が非決定になるため、テスト時は seed を固定して境界値（1000ms/5000ms）到達を再現確認する
```

---

### `checkElapsedTime(unsigned long elapsedTime, unsigned long targetTime)` — 判定ランク算出

**basic_design.md 2-2 との対応：** ボタン押下時間と目標時間を比較

**引数：**
- elapsedTime（unsigned long）: 計測した押下時間
- targetTime（unsigned long）: 目標時間

**戻り値：** int（0:Miss 1:Good 2:Excellent）

```
【処理の流れ】
1. まず大小比較し、次で誤差を計算する
  - elapsedTime >= targetTime のとき: error = elapsedTime - targetTime
  - elapsedTime <  targetTime のとき: error = targetTime - elapsedTime
2. error が 0〜100ms なら 2（Excellent）を返す
3. error が 101〜300ms なら 1（Good）を返す
4. それ以外は 0（Miss）を返す

【エラー・異常ケース】
- targetTime が 0 の場合: 判定不能として 0（Miss）を返す
```

---

### `displayResult(int resultRank)` — 判定結果の表示

**basic_design.md 2-2 との対応：** 判定結果をLEDで表示

**引数：** resultRank（int）: 判定ランク（0:Miss 1:Good 2:Excellent）

**戻り値：** なし（void）

```
【処理の流れ】
1. resultRank が 2 の場合は Excellent の表示パターンを適用する
2. resultRank が 1 の場合は Good の表示パターンを適用する
3. resultRank が 0 の場合は Miss の表示パターンを適用する
4. resultRank が 0/1/2 以外の場合は Miss 表示にフォールバックする
5. 表示状態を ledState に反映し、updateOutput() が参照できるようにする

【エラー・異常ケース】
- 表示更新に失敗した場合: LEDを消灯して再表示待ちにする
```

---

### `restartGame()` — ゲームを初期状態へ戻す

**basic_design.md 2-2 との対応：** ゲームの再スタート

**引数：** なし

**戻り値：** なし（void）

```
【処理の流れ】
1. 計測用の値（startTime, elapsedTime）をリセットする
2. 判定表示用の値（ledState, buttonPressed, buttonReleasedEvent, isCorrect, resultRank, inputAllowed）を初期化する
3. 新しい targetTime を生成する
4. currentState を 1（待機中）に戻す

【エラー・異常ケース】
- 再初期化途中で値不整合が出た場合: currentState を 0（初期化）に戻す
```

---

### `acceptInputTiming(int state, unsigned long now)` — 入力受付タイミングの制御

**basic_design.md 2-2 との対応：** 入力受付のタイミング制御

**引数：**
- state（int）: 現在状態
- now（unsigned long）: 現在時刻

**戻り値：** bool（入力受付可能なら true）

```
【処理の流れ】
1. state と now を確認し、入力受付可能区間か判定する
2. 待機中（state=1）かつ now - lastDebounceTime >= INPUT_GUARD_MS のときのみ true を返す
3. 上記条件を満たさない場合は false を返す

【エラー・異常ケース】
- タイミング境界付近で判定が揺れる場合: false を返して次ループで再判定する
- 前回エッジから20ms未満の入力は仕様として無効（反応しないように見えることがある）
```

---

## 3. 重要ロジックの詳細設計

### 3-1. チャタリング防止（デバウンス処理）

> ※ ボタンを使う場合は必ず設計してください。

```
【考え方】
  INPUT_PULLUP のため、ボタン押下は LOW として読む。
  LOW が読まれても、前回確定から 50ms 未満なら同一押下として無視する。
  50ms 以上経過した LOW のみ「新しい押下」として確定する。

【処理の流れ】
  1. now = millis() を取得する
  2. PIN_BUTTON を digitalRead し、reading に入れる
  3. reading が LOW かつ lastButtonReading が HIGH なら「押下エッジ候補」とする
  4. reading が HIGH かつ lastButtonReading が LOW なら「離上エッジ候補」とする
  5. 押下エッジ候補または離上エッジ候補のとき、now - lastDebounceTime を計算する
  6. 差が DEBOUNCE_DELAY 未満ならイベントを無視する
  7. 差が DEBOUNCE_DELAY 以上かつ押下エッジ候補なら buttonPressed = true, buttonReleasedEvent = false
  8. 差が DEBOUNCE_DELAY 以上かつ離上エッジ候補なら buttonPressed = false, buttonReleasedEvent = true
  9. エッジ確定時は lastDebounceTime = now に更新する
 10. 最後に lastButtonReading = reading を保存する

【必要な変数（Section 1 に追加済みか確認）】
  lastDebounceTime : unsigned long   // 前回確定した時刻
  DEBOUNCE_DELAY   : const int = 50  // チャタリング判定時間（ms）
  INPUT_GUARD_MS   : const unsigned long = 20 // 入力受付ガード時間
  lastButtonReading: int             // 直前の生入力（HIGH/LOW）
  buttonReleasedEvent: bool          // 離上イベントフラグ
```

---

### 3-2. millis() を使ったタイマー管理

```
【考え方】
  delay() は使わず、すべて「経過時間判定」で処理を進める。
  それぞれの処理ごとに「前回実行時刻」を持ち、
  now - lastXxx >= intervalXxx のときだけ実行する。

【処理の流れ（共通）】
  1. 各ループの先頭で now = millis() を取得する
  2. 処理A/B/Cごとに now - lastMillis_X を計算する
  3. 差が interval_X 以上なら、その処理を1回実行して lastMillis_X = now に更新する
  4. 差が interval_X 未満なら何もしない（次ループで再判定）

【自分のシステムで millis() を使う処理】
  1. LED点滅（500ms周期）
     - 対象: 結果表示中のLED点滅制御
     - 条件: now - lastMillis_LED >= 500
     - 実行: LED状態を反転し、lastMillis_LED = now

  2. ボタン入力監視（常時）
     - 対象: readButton() / acceptInputTiming()
     - 条件: 毎ループ実行（実質 interval 0ms）
     - 実行: ボタン生値読取、デバウンス判定、受付可能区間チェック

  3. ブザー鳴動制御（ボタン押下中のみ）
     - 対象: 計測中のブザーON/OFF
     - 条件: currentState が計測中かつ buttonPressed が true
     - 実行: 押下中はブザーON、離したらブザーOFF
     - 補足: 必要なら安全のため最大鳴動時間を設け、
             now - startTime が上限を超えたら自動停止する

【必要な定数・変数】
  LED_INTERVAL    : const unsigned long = 500
  lastMillis_LED  : unsigned long
  now             : unsigned long（loop内の一時変数）
```

---

### 3-3. その他の重要ロジック（任意）

> **【任意】** 複雑なロジックがある場合のみ記入してください。
> 例：「距離に応じたLED点灯パターン」「ゲームの衝突判定」「温度の閾値判定」

```
【処理の流れ】
1. resultRank は checkElapsedTime() の戻り値をそのまま使う
  - 判定しきい値の正本は Section 2 の checkElapsedTime() とする
2. 結果ランクに応じて表示パターンを切り替える
  - Excellent: LEDを短く2回点滅、短いブザー1回
  - Good: LEDを1回点滅、短いブザー2回
  - Miss: LEDを長く1回点灯、長いブザー1回
3. 結果表示後、restartGame() で次ラウンドへ進む

【入力値と出力値の関係】
  入力: targetTime（目標時間）, elapsedTime（実測時間）
  中間値: resultRank（Excellent/Good/Miss）
  出力: ledState とブザー鳴動パターン
```

---

## 4. デバッグ出力計画（任意）

> **【任意】** 関数設計（Section 2）と並行して記入すると効果的です。
> 「動かない」ときに何を確認すればいいかを事前に計画しておきます。
> 実装後は不要な Serial.println() を削除すること。

| No | 確認したい内容 | 挿入する関数 | Serial.println の内容例 |
|:---|:---|:---|:---|
| 1 | 状態遷移が正しく起きているか | `loop()` | `Serial.println(currentState);` |
| 2 | チャタリング処理が効いているか | `readButton()` | `Serial.println("btn confirmed");` |
| 3 | 目標時間が正しく生成されているか | `generateTargetTime()` | `Serial.println(targetTime);` |
| 4 | 計測時間と判定結果が妥当か | `checkElapsedTime()` | `Serial.println((elapsedTime >= targetTime) ? (elapsedTime - targetTime) : (targetTime - elapsedTime));` |

---

## 5. 単体テスト仕様書（V字モデル：詳細設計 ↔ 単体テスト）

> ※ 各関数・部品が「単体で正しく動くか」を確認するテスト項目を設計します。
> 「実際の結果」欄は実装後に記入します。

### 5-1. 入力系テスト

| No | テスト対象の関数 | 入力・操作 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | readButton() | タクトスイッチを1回押す | true が返る | | [ ] |
| 2 | readButton() | スイッチを素早く2回押す | 1回分だけ true になる | | [ ] |
| 3 | readButton() | ボタンを押して離す | 離上時に buttonReleasedEvent が true になる | | [ ] |
| 4 | readButton() | ボタンを離上せず待機する | false は「押下イベントなし」を意味し、離上イベントとしては扱わない | | [ ] |
| 5 | acceptInputTiming(state, now) | state=2（計測中）で呼ぶ | false が返る | | [ ] |
| 6 | acceptInputTiming(state, now) | state=1 かつ now - lastDebounceTime = 10ms で呼ぶ | false が返る（20ms未満は無効） | | [ ] |
| 7 | acceptInputTiming(state, now) | state=1 かつ now - lastDebounceTime = 20ms で呼ぶ | true が返る（20ms以上で有効） | | [ ] |
| 8 | generateTargetTime() | 10回連続で目標時間を生成する | すべて 1000〜5000ms の範囲内になる | | [ ] |
| 9 | generateTargetTime() | setup() 実行時の randomSeed(seedValue) 呼び出し有無をコードレビューで確認する | randomSeed() が1回以上呼ばれ、seedValue が未初期化の定数でない | | [ ] |
| 10 | checkElapsedTime() | target=2000ms, elapsed=2000ms を与える | 2（Excellent）を返す | | [ ] |
| 11 | checkElapsedTime() | target=2000ms, elapsed=1900ms を与える | 2（Excellent: 逆側境界値100ms）を返す | | [ ] |
| 12 | checkElapsedTime() | target=2000ms, elapsed=2100ms を与える | 2（Excellent: 境界値100ms）を返す | | [ ] |
| 13 | checkElapsedTime() | target=2000ms, elapsed=1899ms を与える | 1（Good: 逆側境界値101ms）を返す | | [ ] |
| 14 | checkElapsedTime() | target=2000ms, elapsed=1700ms を与える | 1（Good: 逆側境界値300ms）を返す | | [ ] |
| 15 | checkElapsedTime() | target=2000ms, elapsed=2101ms を与える | 1（Good: 境界値101ms）を返す | | [ ] |
| 16 | checkElapsedTime() | target=2000ms, elapsed=2300ms を与える | 1（Good: 境界値300ms）を返す | | [ ] |
| 17 | checkElapsedTime() | target=2000ms, elapsed=1699ms を与える | 0（Miss: 逆側境界値301ms）を返す | | [ ] |
| 18 | checkElapsedTime() | target=2000ms, elapsed=2301ms を与える | 0（Miss: 正側境界値301ms）を返す | | [ ] |
| 19 | checkElapsedTime() | target=0ms, elapsed=2000ms を与える | 0（Miss: 異常系）を返す | | [ ] |
| 20 | readButton() | 前回確定から49msで押下エッジを与える | false が返る（49ms はデバウンス無効） | | [ ] |
| 21 | readButton() | 前回確定から50msで押下エッジを与える | true が返る（50ms はデバウンス有効） | | [ ] |
| 22 | acceptInputTiming(state, now) | state=1 かつ now - lastDebounceTime = 19ms で呼ぶ | false が返る（20ms未満は無効） | | [ ] |
| 23 | generateTargetTime() | MIN_TARGET_MS と MAX_TARGET_MS の定数設定をレビューする | MIN_TARGET_MS <= MAX_TARGET_MS が成立し、設定不整合がない | | [ ] |
| 24 | acceptInputTiming(state, now) | state=1 かつ now - lastDebounceTime = 21ms で呼ぶ | true が返る（20ms以上で有効） | | [ ] |
| 25 | readButton() | 前回確定から49msで離上エッジを与える | buttonReleasedEvent=false を維持する（49ms はデバウンス無効） | | [ ] |
| 26 | readButton() | 前回確定から50msで離上エッジを与える | buttonReleasedEvent=true になる（50ms はデバウンス有効） | | [ ] |
| 27 | acceptInputTiming(state, now) | state=0（初期化）で呼ぶ | false が返る | | [ ] |
| 28 | acceptInputTiming(state, now) | state=4（結果表示）で呼ぶ | false が返る | | [ ] |
| 29 | acceptInputTiming(state, now) | state=3（判定中）で呼ぶ | false が返る | | [ ] |
| 30 | generateTargetTime() | テストビルドで `randomSeed(12345)` に固定し、500回生成して最小値・最大値を記録する（必要なら seed 値を変えて再実行） | 記録した最小値が1000ms、最大値が5000msとなり、下限/上限到達を再現確認できる | | [ ] |

### 5-2. 出力系テスト

| No | テスト対象の関数 | 入力・操作 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | updateOutput(1) | state=1（待機中）を渡す | LED消灯、ブザー停止になる | | [ ] |
| 2 | updateOutput(2) | state=2（計測中）を渡す | ブザーが鳴動する | | [ ] |
| 3 | updateOutput(0) | state=0（初期化）を渡す | LED/ブザーが全停止する | | [ ] |
| 4 | updateOutput(4) | state=4（結果表示）かつ resultRank=2 で呼ぶ | Excellent表示が維持される | | [ ] |
| 5 | displayResult(2) | Excellent判定で呼び出す | Excellent用のLED/ブザー表示になる | | [ ] |
| 6 | displayResult(0) | Miss判定で呼び出す | Miss用のLED/ブザー表示になる | | [ ] |
| 7 | displayResult(99) | 値域外ランクで呼び出す | Miss表示にフォールバックする | | [ ] |
| 8 | restartGame() | 結果表示後に呼び出す | startTime/elapsedTime/buttonReleasedEvent/isCorrect/resultRank/inputAllowed が初期化され、targetTime が 1000〜5000ms で再生成され、状態が待機中へ戻る | | [ ] |
| 9 | updateOutput(99) | 未定義stateを渡す | LED/ブザーが全停止（フェイルセーフ）する | | [ ] |
| 10 | displayResult(1) | Good判定で呼び出す | Good用のLED/ブザー表示になる | | [ ] |
| 11 | updateOutput(4) | state=4（結果表示）かつ resultRank=1 で呼ぶ | Good表示が維持される | | [ ] |
| 12 | updateOutput(4) | state=4（結果表示）かつ resultRank=0 で呼ぶ | Miss表示が維持される | | [ ] |
| 13 | restartGame() | 再初期化途中で不整合が発生した条件を与えて呼び出す | currentState が 0（初期化）に戻り、安全側へ遷移する | | [ ] |

### 5-3. タイミング・並行動作テスト

| No | テスト内容 | テスト手順 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | delay()による処理停止がないか | 結果表示点滅中にボタンを押す | ボタン入力が無視されない | | [ ] |
| 2 | millis()タイマーの周期精度 | 結果表示の点滅をストップウォッチで確認 | 500ms周期（許容誤差 ±50ms）で点滅する | | [ ] |
| 3 | デバウンス閾値の境界動作 | 押下間隔を 40ms / 60ms で比較する | 40ms は無効、60ms は有効になる | | [ ] |
| 4 | 計測時間の更新精度 | 約2秒押し続けて離す | elapsedTime が 2000ms ± 100ms になる | | [ ] |
| 5 | 計測状態から判定状態への遷移 | state=2で buttonReleasedEvent=true を立てて loop() を1周期実行する | state が 3（判定）へ遷移し、elapsedTime が確定する | | [ ] |
| 6 | setup() の初期化確認 | setup() 実行直後の変数と出力状態を確認する | currentState=0、buttonPressed=false、buttonReleasedEvent=false、startTime=0、elapsedTime=0、isCorrect=false、resultRank=0、inputAllowed=false、LED消灯、ブザー停止になる | | [ ] |
| 7 | loop() の状態遷移（0→1） | currentState=0 で loop() を1周期実行する | generateTargetTime() 実行後、state が 1（待機中）へ遷移する | | [ ] |
| 8 | loop() の状態遷移（1→2） | currentState=1 で inputAllowed=true かつ buttonEvent=true の条件を与えて loop() を1周期実行する | startTime が now で記録され、state が 2（計測中）へ遷移する | | [ ] |
| 9 | loop() の状態遷移（3→4） | currentState=3 で loop() を1周期実行する | checkElapsedTime() と displayResult() 実行後、state が 4（結果表示）へ遷移する | | [ ] |
| 10 | loop() の状態遷移（4→1） | currentState=4 で再スタート条件（ボタン押下）を満たして loop() を1周期実行する | restartGame() が呼ばれ、state が 1（待機中）へ戻る | | [ ] |
| 11 | loop() 判定副作用（resultRank=0） | currentState=3 で checkElapsedTime() が 0 を返す条件を与えて loop() を1周期実行する | isCorrect=false となる | | [ ] |
| 12 | loop() 判定副作用（resultRank=1/2） | currentState=3 で checkElapsedTime() が 1 または 2 を返す条件を与えて loop() を1周期実行する | isCorrect=true となる | | [ ] |
| 13 | millis() 巻き戻り境界の耐性 | テスト用に `getNowForTest()` を導入し、`0xFFFFFFF0 -> 0x00000020` の順で時刻を注入して `updateOutput()` と入力判定を連続実行する | `now - lastMillis_X` の差分判定が破綻せず、点滅・入力判定が継続できる | | [ ] |
| 14 | loop() の状態維持（state=1） | currentState=1 で inputAllowed=false または buttonEvent=false の条件を与えて loop() を1周期実行する | state が 1（待機中）のまま変化しない | | [ ] |
| 15 | loop() の状態維持（state=2） | currentState=2 で buttonReleasedEvent=false の条件を与えて loop() を1周期実行する | state が 2（計測中）のまま変化しない | | [ ] |
| 16 | loop() の状態維持（state=0） | currentState=0 で初期化完了条件を満たさない状態を与えて loop() を1周期実行する | state が 0（初期化）のまま変化しない | | [ ] |
| 17 | loop() の状態維持（state=4） | currentState=4 で再スタート条件を満たさない状態を与えて loop() を1周期実行する | state が 4（結果表示）のまま変化しない | | [ ] |

---

## 6. AIレビュー記録

> グループレビューの前に必ず実施してください。

### Q1: 実装上の問題確認

> 「この詳細設計書に書いた関数と処理フローをもとに Arduino でコードを書きます。バグになりやすい箇所・処理の抜け・型の問題はありますか？」

**AIの回答（要約）：**乱数シード初期化の記載がない

**対応した内容：**実装時に setup で乱数シードを1回初期化する

---

### Q2: 単体テスト仕様の確認

> 「Section 5 の単体テスト仕様書で、各関数の動作が正しく検証できていますか？テストが不足している項目や、境界値テストが必要な箇所を教えてください。」

**AIの回答（要約）：**関数網羅は概ね十分だが、乱数境界の再現性、状態遷移の否定ケース、デバウンス離上境界、acceptInputTiming の状態網羅に補強余地がある

**対応した内容：**No.25〜No.30（離上49/50ms、state=0/3/4、seed固定境界確認）と No.14〜No.17（state維持否定ケース）を追加し、No.13 を時刻注入方式で具体化した

---

## 7. グループレビュー記録

### 7-1. 指摘一覧

| No | 指摘内容 | 指摘者 | 対応 |
|:---|:---|:---|:---|
| 1 |  |  |  |
| 2 |  |  |  |
| 3 |  |  |  |

### 7-2. レビューを受けて変更した点

-
-

---

*初版: 2026-05-26 / AIレビュー: 2026-05-26 / グループレビュー後更新: 2026-05-26*
