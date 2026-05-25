# 詳細設計書 — 組込み開発実習

<!-- 作成者: 白澤颯汰 / 日付: 2026-05-25 / グループ: 2-H -->

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
| 作品タイトル | デジタルスロットマシン |
| 状態の種類（1-2 状態遷移から） | 待機(初期数字表示)、回転中、左停止、中央停止、右停止、全停止・判定、リーチ演出（任意）、当選音再生、結果表示待機 |
| 実装する関数の数（2-2 関数一覧から） | 10個 |
| グローバル変数の合計バイト数（2-1 SRAM確認から） | 40B |

---

## 1. グローバル変数・定数の設計

> ※ 基本設計書（2-1 データ設計）をもとに、**型と初期値まで**決めます。
> ここで設計した変数は、この後の関数設計でそのまま使います。

```
【ピン定義】（basic_design.md 3-1 から転記）
	btnPins[3]     : constexpr uint8_t配列 = {4,5,6}
	buzzerPin      : constexpr uint8_t = 9
	displayClkPin  : constexpr uint8_t = 2
	displayDioPin  : constexpr uint8_t = 3

【状態管理】（basic_design.md 1-2 の状態名から転記）
	SlotState(enum)    : enum class SlotState : uint8_t
	列挙子一覧         : STATE_WAIT, STATE_SPINNING, STATE_LEFT_STOP, STATE_CENTER_STOP, STATE_RIGHT_STOP, STATE_JUDGE, STATE_REACH, STATE_WIN_SOUND, STATE_RESULT_WAIT
	currentState       : SlotState = STATE_WAIT
	isSpinning         : bool = false
	stoppedCount       : uint8_t = 0
	isReachSpinActive  : bool = false

【タイマー（millis()用）】（basic_design.md 2-3 から転記）
	lastSpinUpdateMs   : uint32_t = 0
	spinIntervalMs     : constexpr uint16_t = 50
	lastDebounceMs[3]  : uint32_t配列 = {0,0,0}
	debounceIntervalMs : constexpr uint16_t = 50

【センサー・入力値】（basic_design.md 2-1 から転記）
	prevButtonLevel[3]   : uint8_t配列 = {HIGH,HIGH,HIGH}
	stableButtonLevel[3] : uint8_t配列 = {HIGH,HIGH,HIGH}

【その他のフラグ・カウンター】
	slotDigits[3]    : uint8_t配列 = {0,0,0}
	slotStopped[3]   : bool配列 = {false,false,false}
	reachEnabled     : bool = true
	isReach          : bool = false
	reachSlotIndex   : int8_t = -1
	hitFlag          : bool = false
	hitCount         : uint16_t = 0
	playCount        : uint16_t = 0
```

---

## 2. 各関数の詳細設計

> ※ 基本設計書（2-2 関数一覧）で定義した各関数の「中身」を設計します。
> **疑似コード**（日本語＋処理の流れ）で書いてください。実際のC++コードは書かなくてOKです。

---

### `setup()` — 初期化処理

```
【処理の流れ】
1. ピンモードを設定する
	 - btnPins[0..2] → INPUT_PULLUP
	 - buzzerPin     → OUTPUT
	 - displayClkPin/displayDioPin → OUTPUT

2. ライブラリの初期化（使うものだけ）
	 - 4桁ディスプレイ初期化

3. 状態変数を初期化
	 - currentState = STATE_WAIT
	 - stoppedCount = 0
	 - slotStopped[] = false

4. 起動表示
	 - 各スロットに0〜9の初期値を表示
```

**↓ 自分の setup() を設計してください**
```
【処理の流れ】
1. btnPins/buzzerPin/displayピンを初期化する
2. 表示と各種状態変数を初期値にそろえる
3. 待機状態（STATE_WAIT）で開始する
```

---

### `loop()` — メインループ

> ※ loop() は「状態ごとに何をするか」だけ書く。細かい処理は各関数に任せる。

```
【処理の流れ】

＜毎ループ実行すること＞
	- 現在時刻を取得: now = millis()
	- リーチ演出以外の状態でボタン入力を読む（デバウンス付き、1ループで1入力のみ有効）

＜currentState が 待機(初期数字表示) のとき＞
	- 任意ボタン押下で startSpin() を呼び、回転中へ遷移

＜currentState が 回転中/左停止/中央停止/右停止 のとき＞
	- 50ms周期で未停止スロットを更新（updateSpinningDigits）
	- ボタン押下に応じて該当スロットを停止（handleButtonPress）
	- 3スロットすべて停止したら 全停止・判定 へ遷移

＜currentState が 全停止・判定 のとき＞
	- 3桁一致を判定（checkHitAndPlaySound）
	- 一致時は 当選音再生 へ遷移
	- 非一致時、2桁一致かつ任意機能ONならリーチ演出へ
	- それ以外は結果表示待機へ

＜currentState が リーチ演出（任意） のとき＞
	- リーチ音再生後、任意ボタン押下で未一致1桁の回転開始（spinLastSlotForReach）
	- 回転開始後、未一致桁に対応するボタン押下で停止
	- リーチ演出中の入力処理は spinLastSlotForReach のみで扱う
	- 再判定のため 全停止・判定 に戻る

＜currentState が 当選音再生 のとき＞
	- 当選音を再生
	- 再生完了後、結果表示待機へ遷移

＜currentState が 結果表示待機 のとき＞
	- 任意ボタン押下で startSpin() を呼び再スタート
```

**↓ 自分の loop() を設計してください**
```
【処理の流れ】

＜毎ループ実行すること＞
	- now = millis() 取得
	- currentState != リーチ演出 のときのみ handleButtonPress() 呼び出し

＜currentState が 回転系状態のとき＞
	- updateSpinningDigits()
	- 全停止なら 全停止・判定 へ

＜currentState が 判定/演出/待機状態のとき＞
	- 当選判定時は当選音再生状態へ遷移
	- リーチ時は任意ボタンで回転開始し、対応ボタンで停止して再判定へ
	- 待機時は任意ボタンで再スタート
```

---

### （関数ごとに以下のブロックをコピーして追加してください）

> ※ 基本設計書 2-2 の関数一覧に記載した関数を1つずつ設計します。

---

### `startSpin()` — 全スロットを回転状態へ遷移し、停止フラグをリセット

**basic_design.md 2-2 との対応：** 全スロットを回転状態へ遷移し、停止フラグをリセット

**引数：** なし

**戻り値：** void

```
【処理の流れ】
1. slotStopped[] をすべて false にする
2. stoppedCount を 0 にする
3. isSpinning を true にする
4. isReach=false, isReachSpinActive=false, reachSlotIndex=-1, hitFlag=false にリセットする
5. currentState を 回転中 にする

【エラー・異常ケース】
- 異常な値が来た場合: なし（内部状態のみ更新）
```

---

### `updateSpinningDigits()` — 未停止スロットの数字を50msごとに更新

**basic_design.md 2-2 との対応：** 未停止スロットの数字を50msごとに更新

**引数：** なし

**戻り値：** void

```
【処理の流れ】
1. 関数内で now = millis() を取得し、now - lastSpinUpdateMs >= spinIntervalMs を確認
2. 条件成立なら、slotStopped[i]==false の桁だけ乱数更新
3. lastSpinUpdateMs = now に更新
4. renderDisplay() で表示更新

【エラー・異常ケース】
- 異常な値が来た場合: slotDigitsが範囲外なら0〜9へ丸める
```

---

### `handleButtonPress()` — 立ち下がり押下を検知し、該当スロット停止または再スタートを行う

**basic_design.md 2-2 との対応：** 立ち下がり押下を検知し、該当スロット停止または再スタートを行う

**引数：** なし

**戻り値：** void

```
【処理の流れ】
1. currentState が リーチ演出 の場合は何もしない（入力はspinLastSlotForReachで処理）
2. 左/中央/右ボタンについて isDebouncedPress(pin) を確認
3. 同一ループで複数押下を検知した場合は、btnPins配列の走査順（左→中央→右）で最初の1入力だけ有効化する
4. 回転系状態なら、押されたボタンに対応するスロットを停止
5. 停止済みでないスロットのみ stoppedCount を加算
6. 停止済みスロットが1個の時は currentState を 左停止/中央停止/右停止 の対応状態へ更新
7. stoppedCount == 2 の時は currentState を 回転中（残り1桁回転中）として扱う
8. stoppedCount == 3 なら isSpinning=false, currentState を 全停止・判定 にする
9. 結果表示待機で押下された場合は startSpin() を実行

【エラー・異常ケース】
- 異常な値が来た場合: 同一スロット重複停止は無視する
```

---

### `checkHitAndPlaySound()` — 全停止後に3桁一致を判定し、アタリなら当選音を再生

**basic_design.md 2-2 との対応：** 全停止後に3桁一致を判定し、アタリなら当選音を再生

**引数：** なし

**戻り値：** bool（アタリ可否）

```
【処理の流れ】
1. slotDigits[0], [1], [2] が全一致か判定
2. 一致なら hitFlag=true、hitCount++、currentState を 当選音再生 にする
3. 不一致なら hitFlag=false
4. 判定結果を戻り値で返す

【エラー・異常ケース】
- 異常な値が来た場合: 表示値を0〜9に補正して判定する
```

---

### `checkReachAndPlayCue()` — 2桁一致(リーチ)を判定し、任意機能ON時にリーチ音を再生

**basic_design.md 2-2 との対応：** 2桁一致(リーチ)を判定し、任意機能ON時にリーチ音を再生

**引数：** なし

**戻り値：** bool（リーチ可否）

```
【処理の流れ】
1. reachEnabled が true か確認
2. 3桁中2桁だけ一致する組み合わせを判定
3. 成立時は isReach=true、isReachSpinActive=false、未一致桁を reachSlotIndex に記録
4. currentState を リーチ演出 にしてリーチ音を再生し、true を返す
5. 不成立時は isReach=false、false を返す

【エラー・異常ケース】
- 異常な値が来た場合: reachEnabled=false扱いで処理継続
```

---

### `spinLastSlotForReach()` — リーチ時に任意ボタン押下で未一致1桁の回転を開始し、対応ボタン押下で停止して再判定へ進める

**basic_design.md 2-2 との対応：** リーチ時に任意ボタン押下で未一致1桁の回転を開始し、対応ボタン押下で停止して再判定へ進める

**引数：** なし

**戻り値：** void

```
【処理の流れ】
1. リーチ音再生後、任意ボタン押下を待つ
2. 任意ボタン押下で isReachSpinActive=true、slotStopped[reachSlotIndex]=false、stoppedCount=2、isSpinning=true にして回転開始
3. isReachSpinActive==true の間、reachSlotIndex の桁のみ50ms周期で数字更新する
4. 未一致桁に対応するボタン押下を待つ
5. 対応ボタン押下で slotStopped[reachSlotIndex]=true、stoppedCount=3、isReachSpinActive=false、isSpinning=false にする
6. currentState を 全停止・判定 にして再判定する

【エラー・異常ケース】
- 異常な値が来た場合: reachSlotIndexが範囲外なら isReach=false, isReachSpinActive=false, isSpinning=false にして currentState を 結果表示待機 に戻す
```

---

### `renderDisplay()` — 現在の3スロット値を4桁ディスプレイに反映

**basic_design.md 2-2 との対応：** 現在の3スロット値を4桁ディスプレイに反映

**引数：** なし

**戻り値：** void

```
【処理の流れ】
1. slotDigits[0..2] を表示用データへ変換
2. 4桁ディスプレイ右3桁に反映
3. 左1桁は空白または固定記号を表示

【エラー・異常ケース】
- 異常な値が来た場合: 範囲外値は0〜9へ補正して表示
```

---

### `isDebouncedPress(pin)` — 指定ボタンのデバウンス済み押下イベントを返す

**basic_design.md 2-2 との対応：** 指定ボタンのデバウンス済み押下イベントを返す

**引数：** `pin`（int）: ボタンピン番号

**戻り値：** bool

```
【処理の流れ】
1. digitalRead(pin) で現在レベルを取得
2. 前回値と変化したら lastDebounceMs を更新
3. debounceIntervalMs 経過後に stableButtonLevel を更新
4. HIGH→LOW の立ち下がり時のみ true を返す

【エラー・異常ケース】
- 異常な値が来た場合: pinが対象外ならfalseを返す
```

---

## 3. 重要ロジックの詳細設計

### 3-1. チャタリング防止（デバウンス処理）

> ※ ボタンを使う場合は必ず設計してください。

```
【考え方】
	ボタンが押されたとき、50ms 以内の連続入力は「同じ1回の押下」として無視する。

【処理の流れ】
	1. ボタンのデジタル値を読む（digitalRead）
	2. 前回確定した時刻（lastDebounceMs[buttonIndex]）からの経過時間を計算する
	3. 経過時間 < debounceIntervalMs（50ms）→ 無視する
	4. 経過時間 ≥ debounceIntervalMs → ボタンの状態として確定する
	5. 立ち下がりエッジのみ押下イベントとして返す

【必要な変数（Section 1 に追加済みか確認）】
	lastDebounceMs[3]  : uint32_t配列
	debounceIntervalMs : constexpr uint16_t = 50
```

---

### 3-2. millis() を使ったタイマー管理

```
【考え方】
	「前回実行した時刻」を記録しておき、「今の時刻 − 前回時刻 ≥ 周期」なら実行する。

【処理の流れ（例: LED点滅）】
	1. now = millis()
	2. now - lastMillis_LED >= LED_INTERVAL かどうか確認
	3. 条件を満たした場合: LEDのON/OFFを切り替え、lastMillis_LED = now
	4. 条件を満たさない場合: 何もしない（次のループで再チェック）

【自分のシステムで millis() を使う処理】
	- スロット数字更新（50ms周期）
	- リーチ時の未一致1桁の回転（50ms周期、任意ボタンで開始後に対応ボタン押下まで）
	- ボタンデバウンス（50ms）
```

---

### 3-3. その他の重要ロジック（任意）

> **【任意】** 複雑なロジックがある場合のみ記入してください。
> 例：「距離に応じたLED点灯パターン」「ゲームの衝突判定」「温度の閾値判定」

```
【処理の流れ】
1. リーチ判定で未一致1桁を特定する
2. リーチ音再生後、任意ボタン押下で未一致1桁の回転を開始する
3. 未一致1桁の対応ボタン押下で停止後、再度全停止・判定へ戻して当選可否を確定する

【入力値と出力値の関係】
2桁一致かつreachEnabled=trueのときのみリーチ演出を行う
```

---

## 4. デバッグ出力計画（任意）

> **【任意】** 関数設計（Section 2）と並行して記入すると効果的です。
> 「動かない」ときに何を確認すればいいかを事前に計画しておきます。
> 実装後は不要な Serial.println() を削除すること。

| No | 確認したい内容 | 挿入する関数 | Serial.println の内容例 |
|:---|:---|:---|:---|
| 1 | ボタン押下イベントが正しく取れているか | `isDebouncedPress(pin)` | `Serial.println("btn pressed");` |
| 2 | 状態遷移が正しく起きているか | `loop()` | `Serial.println(currentState);` |
| 3 | 停止桁数のカウントが正しいか | `handleButtonPress()` | `Serial.println(stoppedCount);` |
| 4 | 一致判定の結果が正しいか | `checkHitAndPlaySound()` | `Serial.println(hitFlag);` |

---

## 5. 単体テスト仕様書（V字モデル：詳細設計 ↔ 単体テスト）

> ※ 各関数・部品が「単体で正しく動くか」を確認するテスト項目を設計します。
> 「実際の結果」欄は実装後に記入します。

### 5-1. 入力系テスト

| No | テスト対象の関数 | 入力・操作 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | isDebouncedPress() | ボタンを1回押す | true が1回だけ返る | | [ ] |
| 2 | isDebouncedPress() | ボタンを素早く2回押す | 50ms未満の連打は1回に抑制される | | [ ] |
| 3 | handleButtonPress() | 左/中央/右を順不同で押す | 対応桁がそれぞれ停止する | | [ ] |
| 4 | handleButtonPress() | 同じボタンを連続押下 | 同一桁の重複停止は無視される | | [ ] |
| 5 | startSpin() | 全停止後に任意ボタン押下 | stoppedCountが0に戻り再回転する | | [ ] |
| 6 | handleButtonPress() | 複数ボタンを同時押し | 先に検知した1入力のみ有効になる | | [ ] |
| 7 | isDebouncedPress() | 対象外のpin番号を渡す | falseを返し、内部状態を変更しない | | [ ] |
| 8 | startSpin() | リーチ中/当選済み状態から呼ぶ | isReach=false, isReachSpinActive=false, reachSlotIndex=-1, hitFlag=falseにリセットされる | | [ ] |

### 5-2. 出力系テスト

| No | テスト対象の関数 | 入力・操作 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | renderDisplay() | slotDigits={1,2,3}を設定 | 右3桁に1,2,3が表示される | | [ ] |
| 2 | checkHitAndPlaySound() | slotDigitsが3桁一致 | hitFlag=trueかつ状態が当選音再生へ遷移する | | [ ] |
| 3 | checkReachAndPlayCue() | 2桁一致かつreachEnabled=true | リーチ音が鳴り、isReach=trueになる | | [ ] |
| 4 | checkHitAndPlaySound() | slotDigitsが不一致 | hitFlag=falseかつ当選音再生へ遷移しない | | [ ] |
| 5 | checkReachAndPlayCue() | reachEnabled=falseかつ2桁一致 | falseを返し、リーチ演出へ遷移しない | | [ ] |
| 6 | checkReachAndPlayCue() | 3桁すべて不一致 | falseを返し、リーチ演出へ遷移しない | | [ ] |

### 5-3. タイミング・並行動作テスト

| No | テスト内容 | テスト手順 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | millis()更新周期の確認 | 回転中の数字変化を観測 | 約50msごとに数字が変化する | | [ ] |
| 2 | 入力取りこぼし確認 | 回転中に素早くボタン操作 | 各押下が正しく停止に反映される | | [ ] |
| 3 | 全停止遷移確認 | 3桁を順不同で停止 | stoppedCount=3で全停止・判定へ遷移する | | [ ] |
| 4 | リーチ演出開始確認 | 2桁一致を作り、任意ボタンを押す | 任意ボタン押下で未一致1桁の回転が開始する | | [ ] |
| 5 | リーチ演出停止確認 | リーチ回転中に未一致桁の対応ボタンを押す | 対応ボタン押下で未一致桁が停止し再判定へ戻る | | [ ] |
| 6 | 1桁停止時の状態更新確認 | 左/中央/右のいずれか1桁だけ停止する | currentStateが左停止/中央停止/右停止の対応状態になる | | [ ] |
| 7 | リーチ演出中の入力責務確認 | リーチ演出中に通常停止処理を監視する | handleButtonPressは入力を処理せず、spinLastSlotForReachのみが処理する | | [ ] |
| 8 | 同時押し優先確認 | 同一ループで左と右を同時押し相当で入力する | 左→中央→右の走査順で最初の1入力だけ有効になる | | [ ] |
| 9 | リーチ時停止数整合確認 | リーチ開始から停止までを実行する | 回転開始時stoppedCount=2、停止時stoppedCount=3になる | | [ ] |
| 10 | デバウンス境界値確認 | 49ms/50ms/51ms間隔で押下する | 49msは無効、50ms以上で有効になる | | [ ] |
| 11 | リーチ異常復帰確認 | reachSlotIndex=-1または3でspinLastSlotForReachを実行する | isReach=false, isReachSpinActive=false, isSpinning=false, currentState=結果表示待機へ復帰する | | [ ] |

---

## 6. AIレビュー記録

> グループレビューの前に必ず実施してください。

### Q1: 実装上の問題確認

> 「この詳細設計書に書いた関数と処理フローをもとに Arduino でコードを書きます。バグになりやすい箇所・処理の抜け・型の問題はありますか？」

**AIの回答（要約）：**
・now のスコープが未定義で、更新判定が壊れる可能性
・当選時の状態遷移が曖昧
・デバウンス変数名が章内で不一致
・同時押し時の仕様が基本設計書と未整合
・リーチ演出の停止条件が未定義
・startSpin で演出系フラグのリセットが不足
**対応した内容：**

- updateSpinningDigits() 内で millis() を取得する仕様に統一
- 当選時は currentState を当選音再生へ遷移させる仕様を明記
- デバウンス変数名を lastDebounceMs / debounceIntervalMs に統一
- 同時押し時は1ループ1入力のみ有効と明記
- リーチ時は任意ボタンで未一致桁の回転開始、対応ボタンで停止する仕様へ変更
- startSpin() で isReach / reachSlotIndex / hitFlag をリセット
- isReachSpinActive を追加して、リーチ開始済みフェーズを明確化
- リーチ演出中の入力責務を spinLastSlotForReach に統一
- 1桁停止時の currentState 更新規則（左停止/中央停止/右停止）を明記
- 全停止時に isSpinning=false へ更新する規則を明記
- currentStateの型をSlotState(enum)で管理する方針を明記
- 同時押し時の1入力有効化ルールを「左→中央→右の走査順」で固定
- リーチ演出開始/停止時のstoppedCount更新規則（2→3）を明記

---

### Q2: 単体テスト仕様の確認

> 「Section 5 の単体テスト仕様書で、各関数の動作が正しく検証できていますか？テストが不足している項目や、境界値テストが必要な箇所を教えてください。」

**AIの回答（要約）：**

- 主要フローは確認できるが、異常系テストと境界値テストが不足
- isDebouncedPressの50ms境界（49/50/51ms）を明示して検証すべき
- checkReachAndPlayCueの非成立分岐（reachEnabled=false、不一致）を追加すべき
- startSpinの演出系フラグ初期化を検証すべき
- reachSlotIndex範囲外時の復帰処理を単体テスト化すべき

**対応した内容：**

- 5-1に対象外pinの戻り値確認を追加（No.7）
- 5-1にstartSpinの演出系フラグ初期化確認を追加（No.8）
- 5-2にcheckHitAndPlaySoundの不一致分岐確認を追加（No.4）
- 5-2にcheckReachAndPlayCueの非成立分岐確認を追加（No.5, No.6）
- 5-3にデバウンス境界値（49/50/51ms）確認を追加（No.10）
- 5-3にreachSlotIndex範囲外時の異常復帰確認を追加（No.11）

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

*初版: 2026-05-25 / AIレビュー: YYYY-MM-DD / グループレビュー後更新: YYYY-MM-DD*
