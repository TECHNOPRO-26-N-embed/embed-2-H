# 詳細設計書 — 組込み開発実習

<!-- 作成者: 吉田愛彩香 / 日付: 2026-05-25 / グループ: 2-H -->

> **このドキュメントの目的**
> 基本設計書（basic_design_template2.md）で「**どのような構造で作るか**」を決めました。
> この詳細設計書では「**各処理を具体的にどう実装するか**」を決めます。
> 書き終わったとき、**コードの骨格がほぼ完成している**状態を目指してください。

> [!NOTE]
> **V字モデルにおける位置づけ**
> 詳細設計書 ←→ **単体テスト**（関数・部品ごとのテスト）が対応します。
> 「この関数が正しく動くか」の確認は Section 5 の単体テスト仕様書で計画します。
> ※ 必須機能全体が動くかの「結合テスト」は基本設計書（Section 6）に記載します。

---

## 0. 基本設計書との接続確認

| 項目 | basic_design_template2.md から転記 |
|:--|:--|
| 作品タイトル | 音程調整システム |
| 状態の種類（1-2 状態遷移から） | 待機中 / 設定変更中 / 再生中 / エラー / 音量調整中 |
| 実装する関数の数（2-2 関数一覧から） | 11個 |
| グローバル変数の合計バイト数（2-1 SRAM確認から） | 39B（見積） |

### 0-1. 今回の実装範囲（簡易版）

- 実装する機能は次の3つのみとする。
  - 音色変更
  - 音程調整
  - 音量調整（再生中も変更可能）
  - 配線は basic_design_template2.md の部品のみを使用し、ピン重複なしで構成する。
  - 実装難易度を下げるため、保存/呼出機能は今回の版では対象外（将来拡張）とする。
  - 動作確認とテストではシリアルモニタを利用する。

---

## 1. グローバル変数・定数の設計

> ※ 基本設計書（2-1 データ設計）をもとに、**型と初期値まで**決めます。
> ここで設計した変数は、この後の関数設計でそのまま使います。

```
【今回の設計値（basic_design_template2.md 反映）】
【ピン定義】
  PIN_ENC_CLK      = 2
  PIN_ENC_DT       = 3
  PIN_ENC_SW       = 4
  PIN_MODE_BUTTON  = 5
  PIN_LED_GREEN    = 6
  PIN_LED_RED      = 7
  PIN_BUZZER       = 9
  PIN_POT          = A0

【状態管理】
  currentState     : int = 0   // 0:待機中 1:設定変更中 2:再生中 3:エラー 4:音量調整中
  previousState    : int = 0

【タイマー（millis()用）】
  nowMs            : unsigned long = 0
  lastMillis_Enc   : unsigned long = 0
  lastMillis_Btn   : unsigned long = 0
  lastMillis_Sound : unsigned long = 0
  lastMillis_Pot   : unsigned long = 0
  lastMillis_Led   : unsigned long = 0

【入力値】
  encoderStep      : int  = 0
  modeButtonState  : bool = false
  potRaw           : int  = 0

【音パラメータ】
  pitchIndex       : int  = 12
  volumeLevel      : int  = 8
  timbreMode       : int  = 0
  targetFreqHz     : int  = 523

【エラー・補助】
  errorFlag        : bool = false
  errorCode        : int  = 0
  lastDebounceTimeMode : unsigned long = 0
  lastDebounceTimeEnc  : unsigned long = 0
  lastButtonStateMode  : int = HIGH
  lastButtonStateEnc   : int = HIGH
  DEBOUNCE_DELAY   : const int = 50

【状態表示用LED】
  ledStateGreen : bool = false  // 緑LEDの状態（再生中に点灯）
  ledStateRed   : bool = false  // 赤LEDの状態（エラー時に点滅）
```

---

## 2. 各関数の詳細設計

> ※ 基本設計書（2-2 関数一覧）で定義した各関数の「中身」を設計します。
> **疑似コード**（日本語＋処理の流れ）で書いてください。実際のC++コードは書かなくてOKです。

---

### `setup()` — 初期化処理

```
【処理の流れ】
1. PIN_ENC_SW と PIN_MODE_BUTTON を INPUT_PULLUP、LED2本と BUZZER を OUTPUT に設定する。
2. Encoderライブラリ初期化、状態変数を currentState=0 / errorFlag=false で初期化する。
3. Serial.begin(9600) を実行し、シリアルモニタでデバッグ値を確認できる状態にする。
4. 緑LEDを短く点灯し、起動確認を行う。
```

---

### `loop()` — メインループ

> ※ loop() は「状態ごとに何をするか」だけ書く。細かい処理は各関数に任せる。

```
【処理の流れ】

＜毎ループ実行すること＞
  - now = millis() を取得する。
  - readEncoder(), handleButton(), adjustVolume(), checkError() を順に呼ぶ。


＜currentState が 0（待機中） のとき＞
  - showStatusLed(0) を実行する。
  - エンコーダ入力があれば currentState = 1 に遷移する。
  - モードボタン押下があれば changeMelodyPattern() を実行し currentState = 2 に遷移する。


＜currentState が 1（設定変更中） のとき＞
  - updateSoundParams(encoderStep) を実行する。
  - 設定値反映後に currentState = 2 へ遷移する。


＜currentState が 2（再生中） のとき＞
  - playCurrentSound() を 50ms 周期で実行する。
  - 無操作が続く場合は currentState = 0 に戻る。

＜currentState が 3（エラー） のとき＞
  - noTone() で音を停止し、showStatusLed(3) で赤LED点滅を行う。
  - 復帰操作（エンコーダSW長押し）で currentState = 0 に戻る。

＜currentState が 4（音量調整中） のとき＞
  - adjustVolume() を優先し、反映後 currentState = 2 に遷移する。

```

---

### `readEncoder()` — エンコーダの回転方向と増分を取得する

**basic_design_template2.md 3-2 との対応：** エンコーダの回転方向と増分を取得する

**引数：** なし

**戻り値：** int（-1 / 0 / +1）

```
【処理の流れ】
1. 10msごとにエンコーダ現在値を読み取る。
2. 前回値との差分を計算する。
3. 差分の符号から -1 / 0 / +1 を返す。

【エラー・異常ケース】
- 差分が急変閾値を超えた場合はノイズとして0を返す。
```

---

### `handleButton()` — ボタン押下で音色モードを切り替える

**basic_design_template2.md 3-2 との対応：** ボタン押下で音色モードを切り替える

**引数：** なし

**戻り値：** bool（切替有無）

```
【処理の流れ】
1. ボタン状態を読む（押下時LOW）。
2. 直前確定から50ms未満なら無効化する。
3. 押下確定時に timbreMode を 0→1→2→0 で更新し true を返す。

【エラー・異常ケース】
- 不安定入力が続く場合はその周期を無効にする。
```

---

### `updateSoundParams(step)` — 入力に応じて音量・音程パラメータを更新する

**basic_design_template2.md 3-2 との対応：** 入力に応じて音量・音程パラメータを更新する

**引数：** step（int）: エンコーダ増分

**戻り値：** なし

```
【処理の流れ】
1. pitchIndex に step を加算する。
2. pitchIndex を最小値〜最大値にクリップする。
3. pitchIndex から targetFreqHz を再計算する。

【エラー・異常ケース】
- 周波数が範囲外なら errorFlag=true にする。
```

---

### `changeMelodyPattern()` — メロディパターンを切り替える

**basic_design_template2.md 3-2 との対応：** メロディパターンを切り替える

**引数：** なし

**戻り値：** なし

```
【処理の流れ】
1. timbreMode を巡回更新する。
2. 確認音を短く鳴らす。
3. Serialに現在モードを出力する（デバッグ時）。

【エラー・異常ケース】
- mode値不正時は0に戻す。
```

---

### `saveSoundPattern()` — 設定した音パターンを保存する

**basic_design_template2.md 3-2 との対応：** 設定した音パターンを保存する

**引数：** なし

**戻り値：** なし

```
【処理の流れ】
1. 今回の実装では未対応とする（将来拡張）。
2. 必要になった場合のみ EEPROM 保存を追加する。
3. 現段階では関数定義のみ確保する。

【エラー・異常ケース】
- 未使用のため、呼ばれない設計にする。
```

---

### `loadSoundPattern()` — 保存した音パターンを呼び出す

**basic_design_template2.md 3-2 との対応：** 保存した音パターンを呼び出す

**引数：** なし

**戻り値：** なし

```
【処理の流れ】
1. 今回の実装では未対応とする（将来拡張）。
2. 必要になった場合のみ EEPROM 読み出しを追加する。
3. 現段階では関数定義のみ確保する。

【エラー・異常ケース】
- 未使用のため、呼ばれない設計にする。
```

---

### `playCurrentSound()` — 現在の設定でブザーを鳴らす

**basic_design_template2.md 3-2 との対応：** 現在の設定でブザーを鳴らす

**引数：** なし

**戻り値：** なし

```
【処理の流れ】
1. 50ms周期で実行する。
2. timbreModeに応じて音出力パターンを切替える。
3. targetFreqHz と volumeLevel を反映して出力する。

【エラー・異常ケース】
- 周波数異常時は noTone() してエラー遷移する。
```

---

### `checkError()` — 設定値の範囲外や入力異常を検知する

**basic_design_template2.md 3-2 との対応：** 設定値の範囲外や入力異常を検知する

**引数：** なし

**戻り値：** bool

```
【処理の流れ】
1. pitchIndex / volumeLevel / targetFreqHz の範囲を確認する。
2. エンコーダ急変入力を監視する。
3. 異常時は errorFlag=true を返す。

【エラー・異常ケース】
- 連続異常時は errorCode を更新してエラー確定にする。
```

---

### `showStatusLed(state)` — 状態に応じてLED表示を更新する

**basic_design_template2.md 3-2 との対応：** 状態に応じてLED表示を更新する

**引数：** state（int）: 現在状態

**戻り値：** なし

```
【処理の流れ】
1. 100ms周期でLED表示処理を行う。
2. 待機/設定/再生/音量調整/エラーで表示を分岐する。
3. 不正stateはエラー表示へフォールバックする。

【エラー・異常ケース】
- 不正状態値は赤LED点滅固定にする。
```

---

### `adjustVolume()` — ポテンショメータの値を読み取り、音量を更新する

**basic_design_template2.md 3-2 との対応：** ポテンショメータの値を読み取り、音量を更新する

**引数：** なし

**戻り値：** なし

```
【処理の流れ】
1. 50ms周期で analogRead(A0) を行う。
2. 0〜1023 を 0〜10 にマッピングする。
3. 変化があれば volumeLevel を更新し、状態を音量調整中にする。

【エラー・異常ケース】
- 入力揺れが大きい場合は移動平均で平滑化する。
```

---

## 3. 重要ロジックの詳細設計

### 3-1. チャタリング防止（デバウンス処理）

> ※ ボタンを使う場合は必ず設計してください。

```
【考え方】
  INPUT_PULLUP のため、非押下は HIGH、押下時は LOW。
  押下イベントは立下りエッジ（HIGH→LOW）のみ有効とし、
  50ms 以内の連続入力は「同じ1回の押下」として無視する。

【処理の流れ】
  1. 各ボタン（mode/encoderSW）のデジタル値を読む（digitalRead）
  2. 前回確定時刻からの経過時間を計算する
  3. 経過時間 < DEBOUNCE_DELAY（50ms）なら無視する
  4. 経過時間 >= DEBOUNCE_DELAY かつ 立下りエッジ（HIGH→LOW）なら押下確定する
  5. 押下確定時に前回確定時刻と前回状態を更新する
【処理の流れ（例: LED状態更新）】
  1. now = millis()
  2. 状態に応じてLEDを制御する：
    - currentState=2（再生中）なら緑LEDを点灯
    - currentState=3（エラー）なら赤LEDを点滅
    - それ以外は両方消灯

【必要な変数（Section 1 に追加済みか確認）】
  lastDebounceTimeMode : unsigned long   // モードボタン前回確定時刻
  lastDebounceTimeEnc  : unsigned long   // エンコーダSW前回確定時刻
  lastButtonStateMode  : int             // モードボタン前回状態
  lastButtonStateEnc   : int             // エンコーダSW前回状態
  DEBOUNCE_DELAY   : const int = 50  // チャタリング判定時間（ms）
```

---

### 3-2. millis() を使ったタイマー管理

```
【考え方】
  「前回実行した時刻」を記録しておき、「今の時刻 − 前回時刻 ≥ 周期」なら実行する。

【処理の流れ（例: 音出力更新）】
  1. now = millis()
  2. now - lastMillis_Sound >= SOUND_INTERVAL かどうか確認
  3. 条件を満たした場合: playCurrentSound() を実行し、lastMillis_Sound = now
  4. 条件を満たさない場合: 何もしない（次のループで再チェック）

【自分のシステムで millis() を使う処理】

  - エンコーダ読み取り: 10ms（millis）
  - ボタンデバウンス: 20ms（millis）
  - 音出力更新: 50ms（millis）
  - ポテンショメータ読み取り: 50ms（millis）
  - LED表示更新: 100ms（millis）
```

---

### 3-3. その他の重要ロジック（任意）

> **【任意】** 複雑なロジックがある場合のみ記入してください。
> 例：「距離に応じたLED点灯パターン」「ゲームの衝突判定」「温度の閾値判定」

```
【処理の流れ】
1. 音色モード（timbreMode）を 0→1→2→0 で循環させる。
2. playCurrentSound() で modeごとに出力パターン（単音/トレモロ/アルペジオ）を分岐する。
3. errorFlag=true になったら noTone() と赤LED点滅に切替える。

【入力値と出力値の関係】
- timbreMode = 0: 単音
- timbreMode = 1: トレモロ
- timbreMode = 2: アルペジオ

```

---

## 4. デバッグ出力計画（任意）

> **【任意】** 関数設計（Section 2）と並行して記入すると効果的です。
> 「動かない」ときに何を確認すればいいかを事前に計画しておきます。
> 実装後は不要な Serial.println() を削除すること。

> 本プロジェクトではシリアルモニタを利用して値確認を行う。

| No | 確認したい内容 | 挿入する関数 | Serial.println の内容例 |
|:---|:---|:---|:---|
| 1 | エンコーダ回転量が正しく取れているか | `readEncoder()` | `Serial.println(encoderStep);` |
| 2 | 状態遷移が正しく起きているか | `loop()` | `Serial.println(currentState);` |
| 3 | 音量・音程・音色の現在値を確認する | `adjustVolume()` / `updateSoundParams()` | `Serial.println("Volume: " + volumeLevel);` |
| 4 | エラー時の状態を確認する | `checkError()` | `Serial.println("Error: " + errorCode);` |

---

## 5. 単体テスト仕様書（V字モデル：詳細設計 ↔ 単体テスト）

> ※ 各関数・部品が「単体で正しく動くか」を確認するテスト項目を設計します。
> 「実際の結果」欄は実装後に記入します。

### 5-1. 入力系テスト

| No | テスト対象の関数 | 入力・操作 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | readEncoder() | エンコーダを右に1ノッチ回す | +1 が返る | | [ ] |
| 2 | readEncoder() | エンコーダを左に1ノッチ回す | -1 が返る | | [ ] |
| 3 | handleButton() | ボタンを1回押す | true が1回だけ返る | | [ ] |
| 4 | handleButton() | スイッチを素早く2回押す | 1回分だけ true になる | | [ ] |
| 5 | adjustVolume() | ポテンショメータを最小/最大に回す | 音量が0/10へ更新される | | [ ] |

### 5-2. 出力系テスト

| No | テスト対象の関数 | 入力・操作 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | showStatusLed(0) | state=0（待機中）を渡す | 緑LEDが点滅し、赤LEDは消灯する | | [ ] |
| 2 | updateSoundParams() + playCurrentSound() | エンコーダを右に回して再生する | 回転方向に応じて音程が変化する | | [ ] |
| 3 | adjustVolume() + playCurrentSound() | ポテンショメータを最小/最大にして再生する | 音量が最小/最大に変化する | | [ ] |
| 4 | handleButton() + changeMelodyPattern() + playCurrentSound() | モードボタンを押して再生する | 音色が 0→1→2→0 の順で切り替わる | | [ ] |
| 5 | showStatusLed(3) | state=3（エラー）を渡す | 赤LEDが点滅し、緑LEDは消灯、ブザーは停止する | | [ ] |
| 6 | showStatusLed(0) | state=0（待機中）を渡す | 緑LEDが消灯し、赤LEDも消灯する | | [ ] |
| 7 | showStatusLed(2) | state=2（再生中）を渡す | 緑LEDが点灯し、赤LEDは消灯する | | [ ] |
| 8 | showStatusLed(3) | state=3（エラー）を渡す | 赤LEDが点滅し、緑LEDは消灯する | | [ ] |

### 5-3. タイミング・並行動作テスト

| No | テスト内容 | テスト手順 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | delay()による処理停止がないか | 再生中にエンコーダを回し、ボタンを押す | 入力操作が無視されず、音程/音色変更が反映される | | [ ] |
| 2 | millis()タイマーの周期精度 | 音出力更新周期をシリアル出力で確認 | 設計した周期（50ms）で更新される | | [ ] |
| 3 | エラー復帰条件の確認 | 異常入力後に復帰操作を実施 | エラー解除され待機へ戻る | | [ ] |

---

## 6. AIレビュー記録

> グループレビューの前に必ず実施してください。

### Q1: 実装上の問題確認

> 「この詳細設計書に書いた関数と処理フローをもとに Arduino でコードを書きます。バグになりやすい箇所・処理の抜け・型の問題はありますか？」

**AIの回答（要約）：**
- UNO R3のピン数で実装可能。D0/D1未使用は妥当。
- tone() と周期処理の競合を避けるため、音出力更新は millis で統一した方がよい。
- 状態遷移にエラー復帰条件（復帰操作）を明示すると実装が安定する。

**対応した内容：**
- エラー状態からの復帰操作を loop() 設計に追記。
- 音出力・入力処理を millis 周期設計として明記。
- エンコーダ急変入力は checkError() で検知する方針を追記。

---

### Q2: 単体テスト仕様の確認

> 「Section 5 の単体テスト仕様書で、各関数の動作が正しく検証できていますか？テストが不足している項目や、境界値テストが必要な箇所を教えてください。」

**AIの回答（要約）：**
- 境界値テスト（音量最小/最大、音程最小/最大）を追加すると妥当性が上がる。
- エラー遷移と復帰の両方を確認するテストを追加すると抜け漏れが減る。

**対応した内容：**
- 入力系テストに adjustVolume() の境界値テストを追加。
- タイミング・並行動作テストにエラー復帰確認を追加。

---

## 7. グループレビュー記録

### 7-1. 指摘一覧

| No | 指摘内容 | 指摘者 | 対応 |
|:---|:---|:---|:---|
| 1 | エンコーダが何を行うか、ボタンが何を行うかの確認 | 有村さん | 【解答】質問内容だったため、解答ではエンコーダが音程、ボタンが音色（メロディ）、ポテンショメータが音量の調節を行うと回答いたしました。 |
| 2 | 音色の変更はもともと入っている音源を利用して行うのか | 大野さん | 【解答】質問内容だったため、解答では音色がいくつか元から入っているという内容を聞いて思いついた内容なのでその様にする予定だと答えました。 |
| 3 |  |  |  |

### 7-2. レビューを受けて変更した点

- 質問内容だったため、設計自体の修正は不要と判断
- 

---

*初版: 2026-05-25 / AIレビュー: 2026-05-25 / グループレビュー後更新: 2026-05-25*
