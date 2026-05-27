# 詳細設計書 — 組込み開発実習

作成者: 大野航太郎 / 日付: 2026-05-25 / グループ: 2-H

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
| 作品タイトル | 音マネゲーム |
| 状態の種類（1-2 状態遷移から） | モード選択、難易度選択、オリジナル入力待機、オリジナル入力記録、既存曲再生、マネ入力待機、正解通知、不正解通知 |
| 実装する関数の数（2-2 関数一覧から） | 13個 |
| グローバル変数の合計バイト数（2-1 SRAM確認から） | 71B（主要ゲーム制御の可変グローバルのみ） |

### 0-1. 想定フローとの一致確認（今回更新）

- 一致: keypad「0」を押すごとにモード（オリジナル/既存曲）を切り替える。
- 一致: モード選択はD12ボタン押下で確定する。
- 一致: モード確定後に難易度選択を行い、`A/B/C`で変更・D12で確定する。
- 一致: オリジナルモードは、D12で入力開始し、D12で入力終了してミミックへ進む。
- 一致: マネ側は1音入力ごとに即時比較する（入力確定ボタンは使わない）。
- 一致: 1音でも不一致ならそのターンを終了し、赤LED通知へ遷移する。
- 一致: すべて一致して記録キューを消化し、かつ missDetected=false なら成功とする。
- 一致: 不正解通知は赤LEDを0.2秒 x 3回点滅させる。
- 一致: 既存曲再生中は isPlaybackLocked=true とし、キー入力とボタン入力を受け付けない。
- 一致: 状態遷移時に `[TRANSITION] 遷移元 -> 遷移先 | reason=...` をシリアル出力する。
- 更新: 成功/失敗通知後は次ターンのために状態を初期化し、STATE_MODE_SELECTION に戻る。

---

## 1. グローバル変数・定数の設計

> ※ 基本設計書（2-1 データ設計）をもとに、**型と初期値まで**決めます。
> ここで設計した変数は、この後の関数設計でそのまま使います。

### 1-1. ピン定義（D番号との対応）

| 定数名 | 値 | 意味 |
|:--|:--|:--|
| PIN_KEY_1 | 9 | キー1入力ピン |
| PIN_KEY_2 | 8 | キー2入力ピン |
| PIN_KEY_3 | 7 | キー3入力ピン |
| PIN_KEY_4 | 6 | キー4入力ピン |
| PIN_KEY_5 | 5 | キー5入力ピン |
| PIN_KEY_6 | 4 | キー6入力ピン |
| PIN_KEY_7 | 3 | キー7入力ピン |
| PIN_KEY_8 | 2 | キー8入力ピン |
| PIN_BUZZER | 10 | 圧電ブザー出力ピン |
| PIN_LED_RED | 11 | 不正解通知用LED出力ピン |
| PIN_CONFIRM_BUTTON | 12 | 画面遷移の確定ボタン入力ピン（INPUT_PULLUP） |

### 1-2. 音定義（キー番号との対応）

| 定数名 | 周波数[Hz] | 対応キー | 意味 |
|:--|:--|:--|:--|
| TONE_DO | NOTE_C4 (262) | 1 | ド |
| TONE_RE | NOTE_D4 (294) | 2 | レ |
| TONE_MI | NOTE_E4 (330) | 3 | ミ |
| TONE_FA | NOTE_F4 (349) | 4 | ファ |
| TONE_SO | NOTE_G4 (392) | 5 | ソ |
| TONE_LA | NOTE_A4 (440) | 6 | ラ |
| TONE_SI | NOTE_B4 (494) | 7 | シ |

### 1-3. 状態管理変数

| 名前 | 型 | 初期値 | 意味 |
|:--|:--|:--|:--|
| currentState | int | STATE_MODE_SELECTION | 現在の状態（モード選択、オリジナル入力待機など） |
| modeSelected | bool | false | モード選択済みフラグ |

### 1-4. 入力・判定情報（可変グローバル）

| 名前 | 型 | 初期値 | 意味 |
|:--|:--|:--|:--|
| inputKey | int | 0 | 今ループで検出した入力キー（1-7、turn切替0、`*`/`#`、`A/B/C`、無効は0） |
| originalSeq | int[20]相当 | 空 | オリジナル側で入力した音列（最大20） |
| expectedKey | int | 0 | 比較時に参照する期待キー |
| originalLength | int | 0 | 記録済み音列の長さ |
| mimicCount | int | 0 | マネ側で一致した音の数 |
| compareIndex | int | 0 | 何音目を比較中か（0始まり） |
| lastMimicInputMs | unsigned long | 0 | マネ側の最終入力時刻 |
| isAllMatch | bool | false | 全一致が成立したか |
| missDetected | bool | false | 不一致が1回でも発生したか |
| lastDebounceTimeKey | unsigned long | 0 | キー入力が最後に確定した時刻 |
| lastDebounceTimeButton | unsigned long | 0 | 確定ボタンが最後に確定した時刻 |
| lastStableInputKey | int | 0 | 直近の安定キー状態 |
| lastButtonStableState | bool | false | 直近の安定ボタン状態 |
| currentMode | int | 0 | 現在のゲームモード（0:オリジナル入力, 1:デフォルト曲） |
| octaveShift | int | 0 | 現在のオクターブシフト（-1/0/+1） |
| difficultyLevel | int | 1 | 難易度ID（0:A, 1:B, 2:C） |
| distanceValue | int | 20 | 難易度調整用distance値（A=30, B=20, C=10） |
| mimicTimeoutMs | unsigned long | 3000 | 現在難易度でのタイムアウト閾値 |
| isPlaybackLocked | bool | false | 曲再生中の入力ロック |
| modeSelected | bool | false | モード選択済みフラグ（trueで難易度選択へ遷移済み） |

補足:
- lastStableInputKey / lastButtonStableState の「安定」とは、デバウンス時間を満たして確定した状態のキー入力を指す。

### 1-5. 処理用定数

| 定数名 | 値 | 意味 |
|:--|:--|:--|
| MAX_SEQUENCE_LENGTH | 20 | オリジナルで記録できる最大音数 |
| MIMIC_TIMEOUT_BASE_MS | 3000 | マネ入力タイムアウトの基準値 |
| INPUT_TONE_DURATION_MS | 100 | 通常入力音の鳴動時間 |
| DEFAULT_SONG_TONE_DURATION_MS | 180 | 基本曲の各音鳴動時間 |
| DEFAULT_SONG_GAP_MS | 80 | 基本曲の音間休符 |
| CLEAR_TUNE_GAP_MS | 90 | クリア音の音間休符 |
| WRONG_LED_BLINK_MS | 200 | 不正解通知時の赤LED1回点灯時間 |
| WRONG_LED_BLINK_COUNT | 3 | 不正解通知時の赤LED点滅回数 |
| DEBOUNCE_DELAY_MS | 50 | チャタリング除去時間 |
| ORIGINAL_MODE | 0 | recordAndCompareの記録モード |
| MIMIC_MODE | 1 | recordAndCompareの比較モード |
| MODE_ORIGINAL_INPUT | 0 | プレイヤー入力を記録してから比較するモード |
| MODE_DEFAULT_SONG | 1 | デフォルト曲を比較元として開始するモード |
| RESULT_CORRECT | 1 | 判定結果: 正解 |
| RESULT_WRONG | -1 | 判定結果: 不正解 |
| RESULT_CONTINUE | 0 | 判定結果: 継続 |

### 1-6. グローバル変数の合計バイト数（71B）の根拠

前提（Arduino UNO / ATmega328P）:
- int = 2B
- unsigned long = 4B
- bool = 1B

計算対象（可変グローバルのみ）:
- int型 28個: currentState, currentMode, inputKey, expectedKey, originalLength, mimicCount, compareIndex, lastStableInputKey の8個 + originalSeq[20] の20個
- unsigned long型 3個
- bool型 3個

計算:
- int: 28 x 2 = 56B
- unsigned long: 3 x 4 = 12B
- bool: 3 x 1 = 3B
- 合計: 56 + 12 + 3 = 71B

補足（実装との差分）:
- 実装コードでは、上記71Bに加えてカバレッジ記録用配列（coverage）と状態ログ用変数（lastStateLogMs, lastLoggedState）を追加している。

---

## 2. 各関数の詳細設計

> ※ 基本設計書（2-2 関数一覧）で定義した各関数の「中身」を設計します。
> **疑似コード**（日本語＋処理の流れ）で書いてください。実際のC++コードは書かなくてOKです。

---

### setup() — 初期化処理

**basic_design.md 2-2 との対応：** ピンモード設定・初期化

**引数：** なし

**戻り値：** void

```
【処理の流れ】
1. Keypadライブラリ初期化済みの customKeypad（rowPins/colPins）を使用する。
2. D12（確定ボタン）を INPUT_PULLUP に設定する。
3. D10（ブザー）と D11（LED）を出力設定する。
4. currentState を STATE_MODE_SELECTION に初期化する。
5. originalSeq を空にし、originalLength と mimicCount を 0 にする。
6. isAllMatch と missDetected を false にする。
```

---

### loop() — メインループ

**basic_design.md 2-2 との対応：** 状態に応じて各関数を呼び出すメインループ

**引数：** なし

**戻り値：** void

```
【処理の流れ】

＜毎ループ実行すること＞
1. inputKey = readInputKey() を実行する。
2. now = millis() を取得する。
3. 有効なキー入力（1-7）がある場合は playMappedTone(inputKey) を実行する。
4. `*`/`#` 入力はオリジナル記録中/マネ入力中のみ受理し、octaveShiftを-1〜+1で更新する。

＜currentState が STATE_MODE_SELECTION のとき＞
1. keypad「0」入力があれば currentMode（オリジナル/既存曲）をトグルし、現在モードをシリアル表示する。
2. D12押下でモードを確定し、STATE_DIFFICULTY_SELECTION へ遷移する。

＜currentState が STATE_DIFFICULTY_SELECTION のとき＞
1. `A/B/C` 入力で難易度を更新し、現在難易度をシリアル表示する。
2. D12押下で難易度を確定する。
3. currentMode がオリジナルなら STATE_ORIGINAL_WAIT へ遷移する。
4. currentMode が既存曲なら STATE_DEFAULT_PLAYBACK へ遷移する。

＜currentState が STATE_ORIGINAL_WAIT のとき＞
1. D12押下でオリジナル入力を開始し、STATE_ORIGINAL_RECORD へ遷移する。

＜currentState が STATE_ORIGINAL_RECORD のとき＞
1. 有効キー入力ごとに recordAndCompare(inputKey, ORIGINAL_MODE) を実行する。
2. D12押下かつ originalLength>0 でオリジナル入力を確定し、
   compareIndex = 0, mimicCount = 0, missDetected = false, isAllMatch = false を設定して
   STATE_MIMIC_WAIT に遷移する。

＜currentState が STATE_DEFAULT_PLAYBACK のとき＞
1. prepareDefaultSequence() を実行する。
2. playDefaultSequence() を実行し、再生中は isPlaybackLocked=true で入力を無効化する。
3. 再生終了後に STATE_MIMIC_WAIT へ遷移する。

＜currentState が STATE_MIMIC_WAIT のとき＞
1. 有効キー入力ごとに recordAndCompare(inputKey, MIMIC_MODE) を実行する。
2. result = judgeGameResult() を実行し、正解/不正解/継続を判定する。
3. result が RESULT_CORRECT なら STATE_CORRECT_NOTIFY へ遷移する。
4. result が RESULT_WRONG なら STATE_WRONG_NOTIFY へ遷移する。

＜currentState が STATE_CORRECT_NOTIFY のとき＞
1. 正解通知では自作クリア音（複数音 + 休符）を鳴らす。
2. originalSeq を空扱い（originalLength=0）にし、compareIndex, mimicCount, missDetected を初期化する。
3. STATE_MODE_SELECTION に戻る。

＜currentState が STATE_WRONG_NOTIFY のとき＞
1. 赤LEDを WRONG_LED_BLINK_MS（200ms）で3回点滅させる。
2. originalSeq を空扱い（originalLength=0）にし、compareIndex, mimicCount, missDetected を初期化する。
3. STATE_MODE_SELECTION に戻る。
```

---

### readInputKey() — キー入力値（1-7 と条件付き特殊キー）を取得する

**basic_design.md 2-2 との対応：** キー入力値（1-7 と条件付き特殊キー）を取得する

**引数：** なし

**戻り値：** int

```
【処理の流れ】
1. customKeypad.getKey() で押下イベントを取得する。
2. '1'〜'7' が押下された場合は 1〜7 に変換する。
3. keypad「0」が押下された場合はモード切替イベントとして内部コード（9）を返す。
4. `*` / `#` はオリジナル/マネ入力状態でのみ受理し、内部コードを返す。
5. `A/B/C` は難易度選択用の内部コードとして返す（実際の反映は難易度選択状態のみ）。
6. 押下なし（NO_KEY）の場合は 0 を返す。
7. 上記条件を満たさないキーは無効入力として 0 を返す。
8. now - lastDebounceTimeKey < DEBOUNCE_DELAY_MS なら無視して 0 を返す。
9. デバウンス通過後に lastStableInputKey = rawKey, lastDebounceTimeKey = now を更新する。
10. 確定値を返す。

【エラー・異常ケース】
- 複数キー同時入力: 基本設計書の対象外として無効扱いにする。
- キー押しっぱなし: Keypadライブラリのイベント取得（getKey）とデバウンスにより、誤連続入力を抑制する。

【用語】
- rawKey: その瞬間に読んだ未確定の生入力値。
- モード切替イベント: keypad「0」を内部コード（9）として扱う遷移入力。
```

---

### readOriginalEndButton() — 画面遷移の確定ボタン押下を取得する

**basic_design.md 2-2 との対応：** 画面遷移の確定ボタン（D12）の押下を取得する

**引数：** なし

**戻り値：** bool

```
【処理の流れ】
1. D12 を読み取る。
2. INPUT_PULLUP のため LOW を押下として判定する。
3. rawPressed と lastButtonStableState を比較する。
4. 状態変化があり、now - lastDebounceTimeButton >= DEBOUNCE_DELAY_MS のときのみ確定する。
5. 確定時に lastButtonStableState と lastDebounceTimeButton を更新する。
6. 「未押下 -> 押下（HIGH->LOW）」のエッジでのみ true を返す。
7. それ以外は false を返す。

【エラー・異常ケース】
- 押しっぱなし状態: 1回押下としてのみ受け付ける。
```

---

### prepareDefaultSequence() — デフォルト曲を比較用配列に読み込む

**basic_design.md 2-2 との対応：** 追加機能④（デフォルト曲モード準備）

**引数：** なし

**戻り値：** void

```
【処理の流れ】
1. originalLength を 0 に戻す。
2. 事前定義したデフォルト曲配列を先頭から順に originalSeq にコピーする。
3. MAX_SEQUENCE_LENGTH を超えない範囲で originalLength を更新する。
4. compareIndex=0, mimicCount=0, missDetected=false を設定して比較開始可能状態にする。

【エラー・異常ケース】
- デフォルト曲長が MAX_SEQUENCE_LENGTH を超える場合は、上限までで打ち切る。
```

---

### playMappedTone(key) — 入力キーに対応した音を鳴らす

**basic_design.md 2-2 との対応：** 入力キーに対応した音を鳴らす

**引数：** key（int）: 入力キー値（1-7）

**戻り値：** void

```
【処理の流れ】
1. key が 1-7 の範囲か確認する。
2. 対応表に従って周波数を選ぶ。
   - 1:NOTE_C4, 2:NOTE_D4, 3:NOTE_E4, 4:NOTE_F4, 5:NOTE_G4, 6:NOTE_A4, 7:NOTE_B4
3. octaveShift（-1/0/+1）を反映して周波数を補正する。
4. tone(PIN_BUZZER, 選択周波数, INPUT_TONE_DURATION_MS) を実行する。
4. 非ブロッキングを優先し、必要な待ち処理は入れない。
5. key が無効なら何もしない。

【エラー・異常ケース】
- key が 1-7 以外: 無効入力として無視する。

【用語】
- 非ブロッキング: delay() のようにCPUを待機させず、loop() を止めずに処理を進める方式。
```

---

### recordAndCompare(key, mode) — オリジナル記録とマネ即時比較を行う

**basic_design.md 2-2 との対応：** オリジナルを記録し、マネ入力のたびに1音ずつ即時比較する

**引数：**
- key（int）: 入力キー値（1-7）
- mode（int）: ORIGINAL_MODE または MIMIC_MODE

**戻り値：** bool（今回入力が一致なら true）

```
【処理の流れ】
1. key が 1-7 か確認し、範囲外なら false を返す。
2. mode が ORIGINAL_MODE の場合:
   - originalLength < MAX_SEQUENCE_LENGTH なら originalSeq[originalLength] = key を記録する。
   - 記録した場合のみ originalLength++ する。
   - originalLength >= MAX_SEQUENCE_LENGTH のときは追加せず true を返す（オーバーラン防止）。
   - true を返す。
3. mode が MIMIC_MODE の場合:
   - originalLength == 0 なら false を返す（比較対象なし）。
   - compareIndex が 0 以上かつ originalLength 未満か検証する。
   - compareIndex 位置の期待値を expectedKey として取り出す。
   - key と expectedKey を比較する。
   - 一致なら mimicCount++, compareIndex++, lastMimicInputMs=now として true を返す。
   - 不一致なら missDetected = true にして false を返す。

【エラー・異常ケース】
- originalLength が 0 のまま MIMIC_MODE に入った場合: false を返す。
- compareIndex が範囲外の場合: false を返す。
```

---

### judgeGameResult() — ゲーム結果を判定する

**basic_design.md 2-2 との対応：** 全一致で正解、1ミスで不正解を判定する

**引数：** なし

**戻り値：** int（RESULT_CORRECT: 正解, RESULT_WRONG: 不正解, RESULT_CONTINUE: 継続）

```
【処理の流れ】
1. missDetected が true なら RESULT_WRONG を返す。
2. originalLength > 0 かつ compareIndex == originalLength なら isAllMatch = true とし RESULT_CORRECT を返す。
3. currentState が STATE_COMPARE または STATE_MIMIC_WAIT のとき、現在時刻 - lastMimicInputMs が mimicTimeoutMs 以上なら RESULT_WRONG を返す。
4. それ以外は RESULT_CONTINUE を返す。

【エラー・異常ケース】
- originalLength が 0 の場合は継続（0）を返す。
```

---

### updateLcdStatus(turn, index) — 連続正解ターン数と入力位置を表示する

**basic_design.md 2-2 との対応：** 連続正解ターン数と何音目かを表示する（パラレル接続）

**引数：**
- turn（int）: 連続正解ターン数
- index（int）: 現在の入力位置

**戻り値：** void

```
【位置づけ】
   この関数はプラスアルファ機能（必須機能外）として実装する。

【処理の流れ】
1. LCD を使う構成の場合のみ表示処理を実行する。
2. 1行目に連続正解ターン数、2行目に何音目かを表示する。
3. 値更新があったタイミングで表示を書き換える。

【エラー・異常ケース】
- LCD 未接続時は処理を行わない。
```

---

### selectDifficulty() — 開始前に難易度を選択する

**basic_design.md 2-2 との対応：** 開始前に難易度を選択して各閾値を設定する

**引数：** なし

**戻り値：** int（選択された難易度ID）

```
【位置づけ】
   この関数はプラスアルファ機能（必須機能外）として実装する。

【処理の流れ】
1. 開始前の入力から難易度IDを取得する。
2. 難易度IDに応じて内部閾値（例: mimicTimeoutMs）を設定する。
3. 決定した難易度IDを返す。

【エラー・異常ケース】
- 無効な難易度入力時は既定値を返す。
```

---

### updateAccuracyStats(turnResult) — 直近5ターン正解率データを更新する

**basic_design.md 2-2 との対応：** 直近5ターンの正解率を計算して表示データを更新する

**引数：** turnResult（bool）: そのターンの結果（true: 正解, false: 不正解）

**戻り値：** void

```
【位置づけ】
   この関数はプラスアルファ機能（必須機能外）として実装する。

【処理の流れ】
1. turnResult を履歴へ追加する。
2. 履歴が5件を超える場合は最古データを削除する。
3. 履歴から正解率を再計算する。
4. 表示に使う統計データを更新する。

【エラー・異常ケース】
- 履歴件数が5未満の期間は、保持件数のみで正解率を計算する。
```

---

## 3. 重要ロジックの詳細設計

### 3-1. チャタリング防止（デバウンス処理）

> ※ ボタンを使う場合は必ず設計してください。

```
【考え方】
   機械スイッチは押下/離上の瞬間に数ms〜数十msの細かいON/OFF揺れが発生する。
   この揺れをそのまま読むと「1回押しただけなのに複数回押下」と誤判定する。
   そのため、最後に確定してから50ms未満の変化は無視し、50ms以上続いた変化だけ確定する。

【処理の流れ】
1. 現在の入力値を読む。
2. 前回の安定値との差分を確認する（差分なしなら何もしない）。
3. 差分があれば、現在時刻と前回確定時刻の差を計算する。
4. 経過時間 < DEBOUNCE_DELAY_MS の場合は無視する。
5. 経過時間 >= DEBOUNCE_DELAY_MS の場合は入力を確定し、
   前回安定値と前回確定時刻を更新する。

【必要な変数（Section 1 に追加済みか確認）】
  lastDebounceTimeKey : unsigned long
  lastDebounceTimeButton : unsigned long
  DEBOUNCE_DELAY_MS : const int = 50
  lastStableInputKey : int
  lastButtonStableState : bool
```

---

### 3-2. millis() を使ったタイマー管理

```
【考え方】
   delay()で処理を止めずに、「前回時刻」と「現在時刻」の差分で判定する。
   これにより入力監視を継続でき、取りこぼしとラグを抑えられる。

【自分のシステムで millis() を使う処理】
1. キー入力監視（常時）
   - 毎ループで入力を読み取り、取りこぼしを防ぐ。
2. 一致判定実行（入力ごと）
   - マネ入力イベントが発生するたびに recordAndCompare と judgeGameResult を実行する。
3. タイムアウト判定
   - now - lastMimicInputMs >= mimicTimeoutMs（難易度A/B/Cで可変） のとき不正解にする。
4. 不正解通知のLED点灯
   - 赤LEDを200msで3回点滅してターン終了通知を行う。
```

---

### 3-3. その他の重要ロジック（任意）

```
【処理の流れ】
（オリジナル入力終了と比較開始の切替）
1. モード選択では keypad「0」入力ごとに currentMode をトグルする。
2. D12押下でモードを確定し、難易度選択へ遷移する。
3. 難易度選択では `A/B/C` で候補を変更し、D12押下で確定する。
4. オリジナル側は D12押下で入力開始し、D12押下で originalLength を確定する。
3. compareIndex, mimicCount, missDetected, isAllMatch を初期化して
   マネ入力待機に遷移する。

【入力値と出力値の関係】
- key=1-7 入力: 対応する音を100ms鳴らす。
- マネ入力が期待値と不一致: その時点で不正解通知へ。
- マネ入力が全一致: 正解通知へ。
- マネ入力が難易度ごとの制限時間を超えて途切れ: 不正解通知へ。
```

### 3-4. ピン割り当て理由

```
【割り当て】
- D12: 画面遷移の確定ボタン（INPUT_PULLUP）
- D10: ブザー出力（tone）
- D11: 不正解通知LED

【理由】
1. キー入力（D2-D9）と干渉しないピンに制御部品を分離するため。
2. D10/D11 は出力専用として扱いやすく、配線を整理しやすいため。
3. D12 はボタン単独入力として使いやすく、INPUT_PULLUP運用で外付け抵抗を減らせるため。
4. 本教材の回路例に合わせた割り当てにし、実装/検証時の混乱を減らすため。
```

---

## 4. デバッグ出力計画（任意）

> **【任意】** 関数設計（Section 2）と並行して記入すると効果的です。
> 「動かない」ときに何を確認すればいいかを事前に計画しておきます。
> 実装後は不要な Serial.println() を削除すること。

| No | 確認したい内容 | 挿入する関数 | Serial.println の内容例 |
|:---|:---|:---|:---|
| 1 | キー入力が正しく取得できるか | readInputKey() | Serial.println(inputKey); |
| 2 | 確定ボタンが1回押下として取れるか | readOriginalEndButton() | Serial.println(confirmPressed); |
| 3 | 状態遷移が想定通りか | loop() | Serial.println(currentState); |
| 4 | 比較判定が想定通りか | recordAndCompare() | Serial.println(expectedKey); Serial.println(inputKey); |
| 5 | タイムアウト判定が動くか | judgeGameResult() | Serial.println(millis() - lastMimicInputMs); |

---

## 5. 単体テスト仕様書（V字モデル：詳細設計 ↔ 単体テスト）

> ※ 各関数・部品が「単体で正しく動くか」を確認するテスト項目を設計します。
> 「実際の結果」欄は実装後に記入します。

### 5-1. 入力系テスト

| No | テスト対象の関数 | 入力・操作 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | readInputKey() | キー1を1回押す | 1 が返る | | [ ] |
| 2 | readInputKey() | 1-7/0/*/#/A/B/C以外の入力を与える | 0 が返り無視される | | [ ] |
| 3 | readOriginalEndButton() | D12を1回押す | true が1回だけ返る | | [ ] |
| 4 | readOriginalEndButton() | ボタンを押しっぱなしにする | 連続して true にならない | | [ ] |
| 5 | readInputKey() | チャタリング相当の短時間変化を与える | 1回入力として扱われる | | [ ] |
| 6 | readInputKey() | STATE_MODE_SELECTION中に「0」を押す | 仕様上有効入力として受理され、モードトグルイベントとして扱われる | | [ ] |
| 7 | readOriginalEndButton() | STATE_MODE_SELECTION中にD12を押す | モードが確定し、難易度選択へ遷移する | | [ ] |

### 5-2. 出力系テスト

| No | テスト対象の関数 | 入力・操作 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | playMappedTone(1) | key=1 を渡す | NOTE_C4(262Hz) が100ms鳴る | | [ ] |
| 2 | playMappedTone(2) | key=2 を渡す | NOTE_D4 (294Hz) が100ms鳴る | | [ ] |
| 3 | playMappedTone(3) | key=3 を渡す | NOTE_E4 (330Hz) が100ms鳴る | | [ ] |
| 4 | playMappedTone(4) | key=4 を渡す | NOTE_F4 (349Hz) が100ms鳴る | | [ ] |
| 5 | playMappedTone(5) | key=5 を渡す | NOTE_G4 (392Hz) が100ms鳴る | | [ ] |
| 6 | playMappedTone(6) | key=6 を渡す | NOTE_A4 (440Hz) が100ms鳴る | | [ ] |
| 7 | playMappedTone(7) | key=7 を渡す | NOTE_B4 (494Hz) が100ms鳴る | | [ ] |
| 8 | playMappedTone(8) | key=8 を渡す | 無効入力として音が鳴らない | | [ ] |
| 9 | playMappedTone(0) | key=0 を渡す | 音が鳴らない | | [ ] |
| 10 | 正解通知処理 | 全一致条件を満たす | 休符を含む自作クリア音が鳴る | | [ ] |
| 11 | 不正解通知処理 | ミス条件を満たす | D11が0.2秒 x 3回点滅してターン終了する | | [ ] |

### 5-3. タイミング・並行動作テスト

| No | テスト内容 | テスト手順 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | 即時比較判定 | オリジナル登録後、マネ1音目を誤入力する | その時点で不正解判定になる | | [ ] |
| 2 | 全一致判定 | オリジナルと同順で全音入力する | 正解判定になる | | [ ] |
| 3 | タイムアウト判定 | A/B/Cで難易度を設定後、各制限時間を超えて停止する | 不正解判定になる | | [ ] |
| 4 | 入力取りこぼし確認 | 連続入力を行う | 入力が無視されずに処理される | | [ ] |
| 5 | 最大長境界値確認 | 20音入力後にさらに入力する | 20音を超えて記録しない | | [ ] |
| 6 | ターン切替時の論理クリア確認 | 正解または不正解後に次ターン開始し、比較開始位置を確認する | originalLength=0起点で前ターン配列値を参照しない | | [ ] |
| 7 | デフォルト曲モード比較 | モード選択で既存曲を選びD12で確定後、難易度をD12で確定してデフォルト曲どおりに入力する | オリジナル入力なしで正解判定になる | | [ ] |
| 8 | 入力ロック確認 | デフォルト曲再生中に任意キーを押す | すべての入力が無視される | | [ ] |
| 9 | 状態遷移ログ表示 | オリジナル入力開始・確定・マネ入力開始まで操作する | 遷移時のみ`[TRANSITION] 旧状態 -> 新状態 | reason=...`が表示される | | [ ] |

---

## 6. AIレビュー記録

> グループレビューの前に必ず実施してください。

### Q1: 実装上の問題確認

> 「この詳細設計書に書いた関数と処理フローをもとに Arduino でコードを書きます。バグになりやすい箇所・処理の抜け・型の問題はありますか？」

**AIの回答（要約）：**
- 判定戻り値コードを固定しないと loop 分岐で不整合が出る。
- チャタリング対策と押しっぱなし対策には時刻変数・前回状態変数が必要。
- 比較位置管理（compareIndex）がないと多音比較でずれが出る。
- STATE_MIMIC_WAIT が「待機だけ」に見える記述だと、初回入力の即時比較漏れが起きやすい。
- expectedKey と inputKey の役割が曖昧だと、比較ロジック実装で取り違えが起きやすい。

**対応した内容：**
- judgeGameResult() の戻り値を 1 / -1 / 0 で固定した。
- デバウンス関連変数と compareIndex を Section 1 に追加した。
- STATE_MIMIC_WAIT で受けた最初の入力を同ループで比較することを明記した。
- expectedKey は originalSeq[compareIndex] から取得する期待値であることを明記した。

---

### Q2: 単体テスト仕様の確認

> 「Section 5 の単体テスト仕様書で、各関数の動作が正しく検証できていますか？テストが不足している項目や、境界値テストが必要な箇所を教えてください。」

**AIの回答（要約）：**
- 正常系だけでなく、最大20音の境界値とタイムアウト判定を必ず確認すべき。
- 無効入力と押しっぱなし入力の試験を入力系テストに含めるべき。
- originalSeq を論理的に空へ戻す仕様（物理削除なし）を試験観点に含めるべき。
- オリジナル入力確定キー（keypad「0」）に時間制約を設けない仕様を確認すべき。

**対応した内容：**
- Section 5 に最大長境界値確認、タイムアウト判定、無効入力、押しっぱなし入力を追加した。
- originalSeq の論理クリアと、オリジナル入力確定キー（keypad「0」）の時間制約なしを設計記述に反映した。

---

## 7. グループレビュー記録

### 7-1. 指摘一覧

| No | 指摘内容 | 指摘者 | 対応 |
|:---|:---|:---|:---|
| 1 | マネのタイムアウト時間を3秒から5秒にするのはどうか | 吉田さん | オリジナルの音の数が増えるほどマネの負担も大きくなるため、ゲーム性が崩壊しそうであれば、延長予定 |
| 2 | 5-2の出力系テストで、すべての音の出力を確認しないのはなぜか | 白澤さん | 全ての音を鳴らすことで、ボタンとの対応と音階の確認を行うようにします |
| 3 |  |  |  |

### 7-2. レビューを受けて変更した点

- 実装後ゲーム性が担保されなければ変更します
- 5-2出力系テストで、すべての音を出力を確認するように変更

---

*初版: 2026-05-25 / AIレビュー: 2026-05-25 / グループレビュー後更新: 2026-05-25*
