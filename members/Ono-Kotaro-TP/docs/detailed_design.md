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
| 状態の種類（1-2 状態遷移から） | オリジナル入力待機、オリジナル入力記録、マネ入力待機、一致判定、正解通知、不正解通知 |
| 実装する関数の数（2-2 関数一覧から） | 10個 |
| グローバル変数の合計バイト数（2-1 SRAM確認から） | 69B（可変グローバル変数のみ） |

### 0-1. 想定フローとの一致確認（今回更新）

- 一致: オリジナル入力（最大20音）を記録し、完了ボタンで確定する。
- 一致: マネ側は1音入力ごとに即時比較する（入力確定ボタンは使わない）。
- 一致: 1音でも不一致ならそのターンを終了し、赤LED通知へ遷移する。
- 一致: すべて一致して記録キューを消化し、かつ missDetected=false なら成功とする。
- 更新: 不正解通知は「赤LEDの点滅」ではなく「赤LED点灯（ターン終了通知）」として扱う。
- 更新: 成功/失敗通知後は次ターンのために状態を初期化し、STATE_ORIGINAL_WAIT に戻る。

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
| PIN_ORIGINAL_END_BUTTON | 12 | オリジナル入力完了ボタン入力ピン（INPUT_PULLUP） |

### 1-2. 音定義（キー番号との対応）

| 定数名 | 周波数[Hz] | 対応キー | 意味 |
|:--|:--|:--|:--|
| TONE_DO | 262 | 1 | ド |
| TONE_RE | 294 | 2 | レ |
| TONE_MI | 330 | 3 | ミ |
| TONE_FA | 349 | 4 | ファ |
| TONE_SO | 392 | 5 | ソ |
| TONE_LA | 440 | 6 | ラ |
| TONE_SI | 494 | 7 | シ |
| TONE_HIGH_DO | 523 | 8 | 高いド |

注記:
- 上記Hzは一般的な12平均律の代表値であり、音階値として妥当。
- データシートに「音名ごとの固定Hz表」がある部品ではないため、厳密には楽音テーブル由来の値として扱う。

### 1-3. 状態管理変数

| 名前 | 型 | 初期値 | 意味 |
|:--|:--|:--|:--|
| STATE_ORIGINAL_WAIT | int | 0 | オリジナル入力開始待ち |
| STATE_ORIGINAL_RECORD | int | 1 | オリジナル入力記録中 |
| STATE_MIMIC_WAIT | int | 2 | マネ入力開始待ち |
| STATE_COMPARE | int | 3 | マネ入力を1音ずつ比較中 |
| STATE_CORRECT_NOTIFY | int | 4 | 正解通知中 |
| STATE_WRONG_NOTIFY | int | 5 | 不正解通知中 |
| currentState | int | STATE_ORIGINAL_WAIT | 現在の状態 |

### 1-4. 入力・判定情報（可変グローバル）

| 名前 | 型 | 初期値 | 意味 |
|:--|:--|:--|:--|
| inputKey | int | 0 | 今ループで検出した入力キー（1-8、無効は0） |
| originalSeq | int[20]相当 | 空 | オリジナル側で入力した音列（最大20） |
| expectedKey | int | 0 | 比較時に参照する期待キー |
| originalLength | int | 0 | 記録済み音列の長さ |
| mimicCount | int | 0 | マネ側で一致した音の数 |
| compareIndex | int | 0 | 何音目を比較中か（0始まり） |
| lastMimicInputMs | unsigned long | 0 | マネ側の最終入力時刻 |
| isAllMatch | bool | false | 全一致が成立したか |
| missDetected | bool | false | 不一致が1回でも発生したか |
| lastDebounceTimeKey | unsigned long | 0 | キー入力が最後に確定した時刻 |
| lastDebounceTimeButton | unsigned long | 0 | 完了ボタンが最後に確定した時刻 |
| lastStableInputKey | int | 0 | 直近の安定キー状態 |
| lastButtonStableState | bool | false | 直近の安定ボタン状態 |

補足:
- expectedKey は「originalSeq[compareIndex] から取り出した期待値」であり、入力値 inputKey そのものではない。
- 一致時は inputKey == expectedKey になるが、不一致時は異なる。
- lastStableInputKey / lastButtonStableState の「安定」は、デバウンス時間を満たして確定した状態を指す。

### 1-5. 処理用定数

| 定数名 | 値 | 意味 |
|:--|:--|:--|
| MAX_SEQUENCE_LENGTH | 20 | オリジナルで記録できる最大音数 |
| MIMIC_TIMEOUT_MS | 3000 | マネ入力のタイムアウト閾値 |
| INPUT_TONE_DURATION_MS | 100 | 通常入力音の鳴動時間 |
| CORRECT_TONE_DURATION_MS | 500 | 正解通知音の鳴動時間 |
| WRONG_LED_ON_MS | 1000 | 不正解通知時の赤LED点灯時間 |
| DEBOUNCE_DELAY_MS | 50 | チャタリング除去時間 |
| ORIGINAL_MODE | 0 | recordAndCompareの記録モード |
| MIMIC_MODE | 1 | recordAndCompareの比較モード |
| RESULT_CORRECT | 1 | 判定結果: 正解 |
| RESULT_WRONG | -1 | 判定結果: 不正解 |
| RESULT_CONTINUE | 0 | 判定結果: 継続 |

### 1-6. グローバル変数の合計バイト数（69B）の根拠

前提（Arduino UNO / ATmega328P）:
- int = 2B
- unsigned long = 4B
- bool = 1B

計算対象（可変グローバルのみ）:
- int型 27個: currentState, inputKey, expectedKey, originalLength, mimicCount, compareIndex, lastStableInputKey の7個 + originalSeq[20] の20個
- unsigned long型 3個
- bool型 3個

計算:
- int: 27 x 2 = 54B
- unsigned long: 3 x 4 = 12B
- bool: 3 x 1 = 3B
- 合計: 54 + 12 + 3 = 69B

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
1. D9-D2（キー入力）を入力設定する。
2. D12（オリジナル入力完了ボタン）を INPUT_PULLUP に設定する。
3. D10（ブザー）と D11（LED）を出力設定する。
4. currentState を STATE_ORIGINAL_WAIT に初期化する。
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
3. 有効なキー入力（1-8）がある場合は playMappedTone(inputKey) を実行する。

＜currentState が STATE_ORIGINAL_WAIT のとき＞
1. 有効キー入力があれば STATE_ORIGINAL_RECORD に遷移する。
2. recordAndCompare(inputKey, ORIGINAL_MODE) を実行する。
3. originalLength が MAX_SEQUENCE_LENGTH に達した場合は追加記録を止め、完了ボタン待ちを継続する。

＜currentState が STATE_ORIGINAL_RECORD のとき＞
1. 有効キー入力ごとに recordAndCompare(inputKey, ORIGINAL_MODE) を実行する。
2. readOriginalEndButton() が true なら originalLength を確定し、
   compareIndex = 0, mimicCount = 0, missDetected = false, isAllMatch = false を設定して
   STATE_MIMIC_WAIT に遷移する。
3. オリジナル入力完了ボタンには時間制約を設けない（押下されるまで待機）。

＜currentState が STATE_MIMIC_WAIT のとき＞
1. 有効キー入力があれば lastMimicInputMs を更新し、
   STATE_COMPARE に遷移する。
2. 同じ入力をそのまま recordAndCompare(inputKey, MIMIC_MODE) に渡して即時比較する。
3. result = judgeGameResult() を実行し、正解/不正解/継続を判定する。

＜currentState が STATE_COMPARE のとき＞
1. 有効キー入力時に recordAndCompare(inputKey, MIMIC_MODE) を実行する。
2. result = judgeGameResult() を実行する。
3. result が RESULT_CORRECT なら STATE_CORRECT_NOTIFY へ遷移する。
4. result が RESULT_WRONG なら STATE_WRONG_NOTIFY へ遷移する。
5. result が RESULT_CONTINUE なら比較を継続する。

＜currentState が STATE_CORRECT_NOTIFY のとき＞
1. 正解通知音（500ms）を鳴らす。
2. originalSeq を空扱い（originalLength=0）にし、compareIndex, mimicCount, missDetected を初期化する。
   （注: 配列要素をNULLで物理削除するのではなく、長さを0に戻して論理的に空にする）
3. STATE_ORIGINAL_WAIT に戻る。

＜currentState が STATE_WRONG_NOTIFY のとき＞
1. 赤LEDを点灯し、WRONG_LED_ON_MS（1000ms）経過で消灯する。
2. originalSeq を空扱い（originalLength=0）にし、compareIndex, mimicCount, missDetected を初期化する。
   （注: 配列要素をNULLで物理削除するのではなく、長さを0に戻して論理的に空にする）
3. STATE_ORIGINAL_WAIT に戻る。

【補足】
- 不一致が1回でも発生した時点で missDetected=true になり、即ターン終了する。
- compareIndex が originalLength に到達し、かつ missDetected=false のときのみ正解とする。
- mimicCount と compareIndex は一致入力ごとに同時に+1するため、基本的に同値で推移する。
- 2変数を分ける理由は役割分離（mimicCount=成功入力数、compareIndex=参照位置）と将来拡張のため。
```

---

### readInputKey() — キー入力値（1-8）を取得する

**basic_design.md 2-2 との対応：** キー入力値（1-8）を取得する

**引数：** なし

**戻り値：** int

```
【処理の流れ】
1. D9-D2 をスキャンし、押下されたキーに対応する 1-8 を判定する。
2. 押下なしの場合は 0 を返す。
3. 1-8 以外は無効入力として 0 を返す。
4. 現在値 rawKey と lastStableInputKey を比較し、変化がなければ 0 を返す。
5. 変化がある場合、now - lastDebounceTimeKey < DEBOUNCE_DELAY_MS なら無視して 0 を返す。
6. 変化があり、かつDEBOUNCE_DELAY_MS以上経過していれば入力確定し、
   lastStableInputKey = rawKey, lastDebounceTimeKey = now を更新する。
7. 確定値が1-8ならその値を返し、0なら0を返す。

【エラー・異常ケース】
- 複数キー同時入力: 基本設計書の対象外として無効扱いにする。
- キー押しっぱなし: 初回確定のみ返し、同一状態継続中は0を返す。

【用語】
- rawKey: その瞬間に読んだ未確定の生入力値。
- rawKey と lastStableInputKey の変化: 前回確定状態と現在生入力が異なること（押下/離上/別キーへの遷移）。
```

---

### readOriginalEndButton() — オリジナル入力完了ボタン押下を取得する

**basic_design.md 2-2 との対応：** オリジナル入力完了ボタン（D12）の押下を取得する

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

### playMappedTone(key) — 入力キーに対応した音を鳴らす

**basic_design.md 2-2 との対応：** 入力キーに対応した音を鳴らす

**引数：** key（int）: 入力キー値（1-8）

**戻り値：** void

```
【処理の流れ】
1. key が 1-8 の範囲か確認する。
2. 対応表に従って周波数を選ぶ。
   - 1:262, 2:294, 3:330, 4:349, 5:392, 6:440, 7:494, 8:523
3. tone(PIN_BUZZER, 選択周波数, INPUT_TONE_DURATION_MS) を実行する。
4. 非ブロッキングを優先し、必要な待ち処理は入れない。
5. key が無効なら何もしない。

【エラー・異常ケース】
- key が 1-8 以外: 無効入力として無視する。

【用語】
- 非ブロッキング: delay() のようにCPUを待機させず、loop() を止めずに処理を進める方式。
```

---

### recordAndCompare(key, mode) — オリジナル記録とマネ即時比較を行う

**basic_design.md 2-2 との対応：** オリジナルを記録し、マネ入力のたびに1音ずつ即時比較する

**引数：**
- key（int）: 入力キー値（1-8）
- mode（int）: ORIGINAL_MODE または MIMIC_MODE

**戻り値：** bool（今回入力が一致なら true）

```
【処理の流れ】
1. key が 1-8 か確認し、範囲外なら false を返す。
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
3. currentState が STATE_COMPARE のとき、現在時刻 - lastMimicInputMs が MIMIC_TIMEOUT_MS（3000ms）以上なら RESULT_WRONG を返す。
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
2. 難易度IDに応じて内部閾値（例: MIMIC_TIMEOUT_MS）を設定する。
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
   - now - lastMimicInputMs >= 3000ms のとき不正解にする。
4. 不正解通知のLED点灯
   - 赤LEDを1秒点灯してターン終了通知を行う。
```

---

### 3-3. その他の重要ロジック（任意）

```
【処理の流れ】
（オリジナル入力終了と比較開始の切替）
1. オリジナル側は D12 押下が来るまで入力記録を続ける。
2. D12 押下時に originalLength を確定する。
3. compareIndex, mimicCount, missDetected, isAllMatch を初期化して
   マネ入力待機に遷移する。

【入力値と出力値の関係】
- key=1-8 入力: 対応する音を100ms鳴らす。
- マネ入力が期待値と不一致: その時点で不正解通知へ。
- マネ入力が全一致: 正解通知へ。
- マネ入力が3秒途切れ: 不正解通知へ。
```

### 3-4. ピン割り当て理由

```
【割り当て】
- D12: オリジナル入力完了ボタン（INPUT_PULLUP）
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
| 2 | オリジナル終了ボタンが1回押下として取れるか | readOriginalEndButton() | Serial.println(originalEndPressed); |
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
| 2 | readInputKey() | 1-8以外の入力を与える | 0 が返り無視される | | [ ] |
| 3 | readOriginalEndButton() | D12を1回押す | true が1回だけ返る | | [ ] |
| 4 | readOriginalEndButton() | ボタンを押しっぱなしにする | 連続して true にならない | | [ ] |
| 5 | readInputKey() | チャタリング相当の短時間変化を与える | 1回入力として扱われる | | [ ] |
| 6 | readOriginalEndButton() | オリジナル入力中に長時間（例:10秒）待ってからD12を押す | 仕様上有効入力として受理される（時間制約なし） | | [ ] |

### 5-2. 出力系テスト

| No | テスト対象の関数 | 入力・操作 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | playMappedTone(1) | key=1 を渡す | 262Hz が100ms鳴る | | [ ] |
| 2 | playMappedTone(8) | key=8 を渡す | 523Hz が100ms鳴る | | [ ] |
| 3 | playMappedTone(0) | key=0 を渡す | 音が鳴らない | | [ ] |
| 4 | 正解通知処理 | 全一致条件を満たす | 500ms連続鳴動する | | [ ] |
| 5 | 不正解通知処理 | ミス条件を満たす | D11が1秒点灯しターン終了する | | [ ] |

### 5-3. タイミング・並行動作テスト

| No | テスト内容 | テスト手順 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | 即時比較判定 | オリジナル登録後、マネ1音目を誤入力する | その時点で不正解判定になる | | [ ] |
| 2 | 全一致判定 | オリジナルと同順で全音入力する | 正解判定になる | | [ ] |
| 3 | タイムアウト判定 | マネ入力中に3秒以上停止する | 不正解判定になる | | [ ] |
| 4 | 入力取りこぼし確認 | 連続入力を行う | 入力が無視されずに処理される | | [ ] |
| 5 | 最大長境界値確認 | 20音入力後にさらに入力する | 20音を超えて記録しない | | [ ] |
| 6 | ターン切替時の論理クリア確認 | 正解または不正解後に次ターン開始し、比較開始位置を確認する | originalLength=0起点で前ターン配列値を参照しない | | [ ] |

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
- オリジナル入力完了ボタンに時間制約を設けない仕様を確認すべき。

**対応した内容：**
- Section 5 に最大長境界値確認、タイムアウト判定、無効入力、押しっぱなし入力を追加した。
- originalSeq の論理クリアと、オリジナル入力完了ボタンの時間制約なしを設計記述に反映した。

---

## 7. グループレビュー記録

### 7-1. 指摘一覧

| No | 指摘内容 | 指摘者 | 対応 |
|:---|:---|:---|:---|
| 1 |  |  |  |
| 2 |  |  |  |
| 3 |  |  |  |

### 7-2. レビューを受けて変更した点

- なし
- なし

---

*初版: 2026-05-25 / AIレビュー: 2026-05-25 / グループレビュー後更新: 2026-05-25*
