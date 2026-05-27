# 詳細設計書 — 組込み開発実習

<!-- 作成者: 有村 / 日付: 2026-05-25 / グループ: 〇-〇 -->

> **このドキュメントの目的**
> 基本設計書（basic_design_template.md）で「**どのような構造で作るか**」を決めました。
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
| 作品タイトル | 反射神経を測ろうゲーム |
| 状態の種類（1-2 状態遷移から） | 開始待ち / READY表示 / 待機状態 / 合図状態 / 結果状態 / 失敗状態 |
| 実装する関数の数（2-2 関数一覧から） | 13個 |
| グローバル変数の合計バイト数（2-1 SRAM確認から） | 約55B（現行コードの可変グローバル概算） |

---

## 1. グローバル変数・定数の設計

> ※ 基本設計書（2-1 データ設計）をもとに、**型と初期値まで**決めます。
> ここで設計した変数は、この後の関数設計でそのまま使います。

```
【ピン定義】（basic_design.md 3-1 から転記）
  PIN_BUTTON   = 2    // タクトスイッチ（INPUT_PULLUP）
  PIN_BUZZER   = 6    // パッシブブザー
  PIN_LCD_RS   = 7
  PIN_LCD_E    = 8
  PIN_LCD_D4   = 9
  PIN_LCD_D5   = 10
  PIN_LCD_D6   = 11
  PIN_LCD_D7   = 12

【状態管理】（basic_design.md 1-2 の状態名から転記）
  currentState   : int = 0   // 0:開始待ち 1:READY表示 2:待機 3:合図 4:結果 5:失敗

【タイマー（millis()/micros()用）】
  stateEnteredMs : unsigned long = 0
  waitStartedMs  : unsigned long = 0
  randomWaitMs   : unsigned long = 0
  goSignalUs     : unsigned long = 0

【計測・記録】
  reactionTimeUs : unsigned long = 0
  bestTimeUs     : unsigned long = 4294967295
  totalReactionUs: unsigned long = 0
  playCount      : unsigned long = 0
  averageUs      : unsigned long = 0
  lastResultReactionUs : unsigned long = 0
  resultRefreshAtMs    : unsigned long = 0

【結果表示制御】
  showAverageInResult : bool = false
  averageShown        : bool = false
  needsResultRefresh  : bool = false

【ボタン入力（デバウンス）】
  stableButtonLevel : bool = HIGH
  lastRawLevel      : bool = HIGH
  lastDebounceMs    : unsigned long = 0
  DEBOUNCE_MS       : const unsigned long = 50

【表示/遷移制御】
  READY_DISPLAY_MS  : const unsigned long = 800
  GO_TIMEOUT_MS     : const unsigned long = 3000
  RESULT_DISPLAY_MS : const unsigned long = 2000
  AVERAGE_DISPLAY_MS: const unsigned long = 2000

【ランキング表示（非永続）】
  RANKING_SIZE      : const int = 5
  EMPTY_RANK        : const unsigned long = 4294967295
  rankingUs[5]      : unsigned long 配列（初期値はすべて EMPTY_RANK）
```

---

## 2. 各関数の詳細設計

> ※ 基本設計書（2-2 関数一覧）で定義した各関数の「中身」を設計します。
> **疑似コード**（日本語＋処理の流れ）で書いてください。実際のC++コードは書かなくてOKです。

---

### setup() — 初期化処理

```
【処理の流れ】
0. シリアル通信を初期化する
  - Serial.begin(9600)

1. ピンモードを設定する
   - PIN_BUTTON → INPUT_PULLUP
   - PIN_BUZZER → OUTPUT

2. LCDライブラリを初期化する
   - lcd.begin(16, 2)

3. 乱数シードを初期化する
   - randomSeed(analogRead(A0))

4. 初期状態へ遷移する
   - enterState(STATE_IDLE)

5. 初期ランキングをシリアルへ表示する
  - printRanking()
```

---

### loop() — メインループ

> ※ loop() は「状態ごとに何をするか」だけ書く。細かい処理は各関数に任せる。

```
【処理の流れ】

＜毎ループ実行すること＞
  - ボタン入力を読む（readButton）
  - 現在時刻を使って状態時間を判定する（millis）

＜currentState が STATE_IDLE（開始待ち）のとき＞
  - ボタンが押されたら STATE_READY へ遷移

＜currentState が STATE_READY（READY表示）のとき＞
  - READY表示を一定時間表示
  - 0.8秒経過後、ボタン押下で STATE_WAIT へ遷移

＜currentState が STATE_WAIT（待機）のとき＞
  - waitRandom() が成立したら合図を出して STATE_GO へ遷移
  - waitRandom() が未成立の場合のみ checkFalseStart() で早押し検出
  - 早押しなら STATE_MISS へ遷移
  - 境界時刻（成立と押下が同一ループ）では GO を優先して誤フライングを防ぐ

＜currentState が STATE_GO（合図中）のとき＞
  - ボタン押下で reactionTimeUs を計測
  - updateRanking(reactionTimeUs) でTOP5を更新
  - best更新、結果表示、平均表示、必要ならベスト音再生
  - printRanking() でランキングをシリアル表示
  - STATE_RESULT へ遷移
  - 一定時間押下なしなら TIMEOUT 表示して STATE_RESULT へ遷移

＜currentState が STATE_RESULT（結果表示）のとき＞
  - Time/Best を2秒表示し、Average を2秒表示（表示対象がある場合）
  - 表示シーケンス完了後に STATE_READY へ遷移

＜currentState が STATE_MISS（失敗）のとき＞
  - 1.5秒経過後、ボタン押下で STATE_READY へ遷移
```

---

### readButton() — チャタリング処理済みの押下判定

**basic_design.md 2-2 との対応：** （共通）ボタン読取

**引数：** なし

**戻り値：** bool（押下確定時 true）

```
【処理の流れ】
1. raw = digitalRead(PIN_BUTTON)
2. raw が前回値と違えば lastDebounceMs を更新
3. 経過時間 >= DEBOUNCE_MS かつ確定値が変化したら確定
4. 確定値が LOW なら true を返す（INPUT_PULLUPのため）
5. それ以外は false

【エラー・異常ケース】
- ノイズで瞬間的に値が変わる場合:
  DEBOUNCE_MS 未満は無視する
```

---

### waitRandom() — ランダム待機成立判定

**basic_design.md 2-2 との対応：** F02 ランダム待機

**引数：** なし

**戻り値：** bool（待機完了なら true）

```
【処理の流れ】
1. now = millis()
2. now - waitStartedMs >= randomWaitMs を判定
3. 成立時 true / 未成立 false

【エラー・異常ケース】
- randomWaitMs が未設定の場合:
  enterState(STATE_WAIT) 時に必ず設定する
```

---

### checkFalseStart() — 待機中の早押し検出

**basic_design.md 2-2 との対応：** A01 フライング判定

**引数：** なし

**戻り値：** bool（早押しなら true）

```
【処理の流れ】
1. readButton() を呼ぶ
2. true なら早押しと判定
3. false なら待機継続

【エラー・異常ケース】
- チャタリングで誤判定しそうな場合:
  readButton() のデバウンス結果のみを使用
```

---

### measureReaction() — 反応時間計測

**basic_design.md 2-2 との対応：** F01 反射測定

**引数：** なし

**戻り値：** unsigned long（μs）

```
【処理の流れ】
1. nowUs = micros()
2. reactionUs = nowUs - goSignalUs
3. reactionUs を返す

【エラー・異常ケース】
- micros オーバーフロー:
  unsigned long 差分計算で扱う
```

---

### updateBest(reactionUs) — 最速記録更新

**basic_design.md 2-2 との対応：** F04 記録更新

**引数：** reactionUs（unsigned long）

**戻り値：** bool（更新されたら true）

```
【処理の流れ】
1. reactionUs < bestTimeUs なら bestTimeUs を更新
2. totalReactionUs に reactionUs を加算
3. playCount を +1
4. best更新有無を返す

【エラー・異常ケース】
- 初回計測（best未初期化）:
  1回目は必ず best 更新扱いになる
```

---

### displayResult(reactionUs) — 結果表示

**basic_design.md 2-2 との対応：** F03 表示処理

**引数：** reactionUs（unsigned long）

**戻り値：** なし（void）

```
【処理の流れ】
1. reactionMs = reactionUs / 1000 に変換
2. bestMs = bestTimeUs / 1000 に変換
3. 1行目に Time を表示
4. 2行目に Best を表示

【エラー・異常ケース】
- LCD桁あふれ:
  単位表示を短くし桁数を制限する
```

---

### displayAverage(avgUs) — 平均値表示

**basic_design.md 2-2 との対応：** A03 平均値表示

**引数：** avgUs（unsigned long）

**戻り値：** なし（void）

```
【処理の流れ】
1. avgMs = avgUs / 1000
2. LCD 1行目に "Average:" を表示
3. LCD 2行目に avgMs と "ms" を表示

【エラー・異常ケース】
- playCount = 0 の場合:
  呼び出し側で表示しないようにする
```

---

### playSound(soundType) — 効果音出力

**basic_design.md 2-2 との対応：** A02 ブザー出力

**引数：** soundType（int）

**戻り値：** なし（void）

```
【処理の流れ】
1. BUZZER_PIN と LCD_E_PIN が同一の場合は何もせず終了する（表示破損回避）
2. soundType に応じて tone(PIN_BUZZER, 周波数, 長さ) を呼ぶ
3. CUE / MISS / BEST の3種類を使い分ける
4. BEST は短い3音メロディー（例: 1319Hz → 1568Hz → 2093Hz）を鳴らす

【エラー・異常ケース】
- 未定義 soundType:
  何も鳴らさず終了する
- ブザーとLCD Enableが同一ピンの場合:
  効果音を無効化してLCD表示破損を防ぐ
```

---

### enterState(next) — 状態遷移処理

**basic_design.md 2-2 との対応：** メイン制御補助

**引数：** next（GameState）

**戻り値：** なし（void）

```
【処理の流れ】
1. currentState = next
2. stateEnteredMs = millis() を保存
3. nextごとに表示・初期化を実施
   - IDLE: Press button to start
   - READY: READY / Get set...
   - WAIT: randomWaitMs を 2000〜5000ms で設定、WAIT表示
   - MISS: MISS表示 + 失敗音

【エラー・異常ケース】
- 不正な next:
  表示更新せず状態値のみ保持
```

---

### updateRanking(reactionUs) — ランキング更新

**basic_design.md 2-2 との対応：** A04 ランキング更新

**引数：** reactionUs（unsigned long）

**戻り値：** なし（void）

```
【処理の流れ】
1. rankingUs[0..4] を先頭（最速）から順に比較
2. reactionUs が入る位置を見つけたら、後ろの要素を1つずつ後方へシフト
3. 該当位置に reactionUs を挿入
4. TOP5圏外なら何も変更しない

【エラー・異常ケース】
- ランキング空き要素:
  EMPTY_RANK（4294967295UL）を空きとして扱う
```

---

### printRanking() — シリアルランキング表示

**basic_design.md 2-2 との対応：** A05 ランキング表示

**引数：** なし

**戻り値：** なし（void）

```
【処理の流れ】
1. シリアルモニタへ "RANKING TOP5" ヘッダを表示
2. rankingUs を先頭から走査し、記録がある順位だけ ms で表示
3. 記録がない場合は "No records yet" を表示

【エラー・異常ケース】
- 電源再投入後:
  RAM保持のためランキングは空になる（非永続仕様）
```

---

## 3. 重要ロジックの詳細設計

### 3-1. チャタリング防止（デバウンス処理）

> ※ ボタンを使う場合は必ず設計してください。

```
【考え方】
  ボタンが押されたとき、50ms 以内の連続入力は「同じ1回の押下」として無視する。

【処理の流れ】
  1. digitalRead(PIN_BUTTON) で値を取得
  2. 値が変化したら lastDebounceMs を更新
  3. 経過時間 < 50ms は確定しない
  4. 経過時間 >= 50ms かつ確定状態変化時のみ押下判定
  5. INPUT_PULLUP のため LOW を押下として扱う

【必要な変数（Section 1 に追加済みか確認）】
  lastDebounceMs  : unsigned long
  DEBOUNCE_MS     : const unsigned long = 50
  stableButtonLevel : bool
  lastRawLevel      : bool
```

---

### 3-2. millis() を使ったタイマー管理

```
【考え方】
  「前回時刻を記録し、経過時間で分岐する」方式で並行動作を維持する。

【処理の流れ】
  1. now = millis()
  2. READY状態: now - stateEnteredMs >= READY_DISPLAY_MS かつボタン押下で遷移
  3. WAIT状態: now - waitStartedMs >= randomWaitMs で合図へ
  4. GO状態: now - stateEnteredMs >= GO_TIMEOUT_MS でTIMEOUT表示
  5. RESULT状態: RESULT_DISPLAY_MS と AVERAGE_DISPLAY_MS の2段階で表示後、READYへ戻す
  6. MISS状態: 1.5秒経過後、ボタン押下でREADYへ戻す

【自分のシステムで millis() を使う処理】
  - READY表示時間管理
  - ランダム待機成立判定
  - 待機成立境界では GO 遷移を優先（waitRandom未成立時のみフライング判定）
  - GOタイムアウト判定
  - RESULTの2段階表示管理（Time/Best→Average）
  - MISSからの復帰待ち管理

【補足】
  - SOUND_BEST の3音メロディーは視認性を優先し、短い delay を使って区切っている。
```

---

### 3-3. 平均反応時間の計算ロジック

```
【処理の流れ】
1. 各プレイ完了時に totalReactionUs += reactionUs
2. playCount を +1
3. 平均が必要なとき avgUs = totalReactionUs / playCount
4. displayAverage(avgUs) で表示

【入力値と出力値の関係】
- 入力: 各回の reactionUs
- 出力: 平均値 avgUs（表示時はms換算）
```

---

## 4. デバッグ出力計画（任意）

| No | 確認したい内容 | 挿入する関数 | Serial.println の内容例 |
|:---|:---|:---|:---|
| 1 | 状態遷移が正しく起きるか | loop() | Serial.println(currentState); |
| 2 | ボタン確定判定が正しいか | readButton() | Serial.println("btn confirmed"); |
| 3 | 反応時間が取得できるか | measureReaction() | Serial.println(reactionTimeUs); |
| 4 | 平均値計算が正しいか | updateBest() | Serial.println(totalReactionUs / playCount); |

---

## 5. 単体テスト仕様書（V字モデル：詳細設計 ↔ 単体テスト）

> ※ 各関数・部品が「単体で正しく動くか」を確認するテスト項目を設計します。
> 「実際の結果」欄は実装後に記入します。

### 5-1. 入力系テスト

| No | テスト対象の関数 | 入力・操作 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | readButton() | タクトスイッチを1回押す | true が1回だけ返る | | [ ] |
| 2 | readButton() | スイッチを素早く2回押す | チャタリングが無視される | | [ ] |
| 3 | checkFalseStart() | WAIT中に押す | true を返し MISS に遷移する | | [ ] |
| 4 | waitRandom() | WAIT開始後に時間を監視 | 2〜5秒で true になる | | [ ] |
| 5 | measureReaction() | 合図後すぐ押す/遅れて押す | 反応時間が大小関係どおり取得される | | [ ] |

### 5-2. 出力系テスト

| No | テスト対象の関数 | 入力・操作 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | displayResult() | reactionUs を与える | Time / Best がLCDに表示される | | [ ] |
| 2 | displayAverage() | avgUs を与える | Average がLCDに表示される | | [ ] |
| 3 | playSound(SOUND_CUE) | 合図状態で呼び出す | 短い合図音が鳴る | | [ ] |
| 4 | playSound(SOUND_MISS) | 失敗状態で呼び出す | 失敗音が鳴る | | [ ] |

### 5-3. タイミング・並行動作テスト

| No | テスト内容 | テスト手順 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | READY表示時間 | 起動後0.8秒待ってボタンを押す | 0.8秒経過前は遷移せず、押下後に WAIT へ遷移 | | [ ] |
| 2 | GOタイムアウト | 合図後に押さない | TIMEOUT表示後に RESULT へ遷移 | | [ ] |
| 3 | RESULT/MISS復帰 | 結果表示シーケンス完了、またはMISS後1.5秒経過で押下 | RESULTは自動でREADYへ、MISSは押下でREADYへ戻る | | [ ] |
| 4 | 平均表示 | 3回以上プレイする | Time/Bestを2秒表示後、Averageを2秒表示する | | [ ] |
| 5 | WAIT境界押下 | randomWaitMs 到達直前/到達ちょうどで押下する | 直前はMISS、到達ちょうどはGOとして扱われる | | [ ] |

---

## 6. AIレビュー記録

> グループレビューの前に必ず実施してください。

### Q1: 実装上の問題確認

> 「この詳細設計書に書いた関数と処理フローをもとに Arduino でコードを書きます。バグになりやすい箇所・処理の抜け・型の問題はありますか？」

**AIの回答（要約）：**

- unsigned long を使った millis/micros 差分計算は妥当。
- INPUT_PULLUP なので押下判定は LOW で統一する必要がある。
- LCD16ピン配線は1本ミスで表示崩れするため、配線チェック手順を持つべき。
- 平均値計算は playCount=0 のガードを入れておくべき。

**対応した内容：**

- 型をすべて unsigned long / bool / int に整理した。
- readButton のLOW判定を明記した。
- ハード設計に配線ルール（R/W固定、VO調整）を反映済み。
- displayAverage 呼び出し前に playCount > 0 を条件化した。

---

### Q2: 単体テスト仕様の確認

> 「Section 5 の単体テスト仕様書で、各関数の動作が正しく検証できていますか？テストが不足している項目や、境界値テストが必要な箇所を教えてください。」

**AIの回答（要約）：**

- 入力系・出力系・タイミング系に分かれていて検証しやすい。
- 境界値として「GOタイムアウト直前/直後」「最初の1回目（best初期化）」を確認するとよりよい。
- 平均値は複数回実施で更新確認が必要。

**対応した内容：**

- タイムアウト系テストを追加した。
- 1回目と複数回プレイを比較する観点を追加した。
- 平均表示テストを単体テスト表へ追加した。

---

## 7. グループレビュー記録

### 7-1. 指摘一覧

| No | 指摘内容 | 指摘者 | 対応 |
|:---|:---|:---|:---|
| 1 | 反射神経が良すぎてランダム待機時間が変わったタイミングでフライングになってしまう可能性があるとは思うのだがその対応は？ | Mr.Ono | WAIT中の判定順を変更し、waitRandom成立時はGO遷移を優先。waitRandom未成立時のみフライング判定を実行する仕様に修正 |
| 2 | なし | - | なし |
| 3 | なし | - | なし |

### 7-2. レビューを受けて変更した点

- WAIT状態の処理順を「waitRandom判定→（未成立時のみ）フライング判定」に変更した。
- 境界時刻での誤フライング回避を Section 2 と Section 3 に明記した。
- 単体テストに「WAIT境界押下（直前/ちょうど）」の検証項目を追加した。

---

*初版: 2026-05-25 / AIレビュー: 2026-05-25 / グループレビュー後更新: 2026-05-26*