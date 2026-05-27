# テスト仕様書 — 音マネゲーム

作成者: 大野航太郎 / 日付: 2026-05-27 / 対象: sound_mimic_game.ino

## 0. 文書目的

本書は、members/Ono-Kotaro-TP/docs/detailed_design.md の Section 5（単体テスト仕様）をもとに、
members/Ono-Kotaro-TP/src/sound_mimic_game.ino のテスト観点と結果を整理する。

---

## 1. テスト対象

- 対象コード: members/Ono-Kotaro-TP/src/sound_mimic_game.ino
- 対象設計: members/Ono-Kotaro-TP/docs/detailed_design.md
- 主対象機能:
  - キー入力取得（1-8, keypad「0」）
  - オリジナル記録とミミック即時比較
  - 正解/不正解通知（ブザー・LED点滅）
  - デフォルト曲モード切替（D12）
  - タイムアウト判定

---

## 2. テスト環境

- ボード: Arduino UNO R3
- ライブラリ: Keypad, pitches
- 入力:
  - Membrane Switch Module（D9-D2）
  - モード切替ボタン（D12, INPUT_PULLUP）
- 出力:
  - Passive Buzzer（D10）
  - 赤LED（D11）

---

## 3. 判定ルール

- 期待結果を満たしたものを Pass とする。
- 実機未実施の項目は N/A（未実機）とする。
- 今回はソースコード確認による静的確認結果を記載する。

---

## 4. 単体テスト結果

### 4-1. 入力系テスト

| No | テスト対象 | 入力・操作 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | readInputKey() | キー1を1回押す | 1 が返る | コード上、'1'は1に変換して返却する | Pass（静的確認） |
| 2 | readInputKey() | 1-8/0以外の入力を与える | 0 が返り無視される | コード上、NO_KEY以外の無効キーは0返却 | Pass（静的確認） |
| 3 | readOriginalEndButton() | D12を1回押す | true が1回だけ返る | 立上りエッジでのみtrue返却 | Pass（静的確認） |
| 4 | readOriginalEndButton() | ボタンを押しっぱなしにする | 連続して true にならない | 前回安定値比較で連続trueを防止 | Pass（静的確認） |
| 5 | readInputKey() | チャタリング相当の短時間変化を与える | 1回入力として扱われる | DEBOUNCE_DELAY_MS(50ms)未満は無視 | Pass（静的確認） |
| 6 | readInputKey() | オリジナル入力中に長時間待ってから「0」を押す | 仕様上有効入力として受理され、マネ入力待機へ遷移する | STATE_ORIGINAL_RECORDでinputKey==9を受理 | Pass（静的確認） |
| 7 | readOriginalEndButton() | STATE_ORIGINAL_WAIT中にD12を押す | デフォルト曲モードへ遷移し、比較元が初期化される | prepareDefaultSequence()後に固定曲再生してSTATE_MIMIC_WAITへ | Pass（静的確認） |

### 4-2. 出力系テスト

| No | テスト対象 | 入力・操作 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | playMappedTone(1) | key=1 | NOTE_C4(262Hz)が100ms鳴る | tone(..., NOTE_C4, 100) | Pass（静的確認） |
| 2 | playMappedTone(2) | key=2 | NOTE_D4(294Hz)が100ms鳴る | tone(..., NOTE_D4, 100) | Pass（静的確認） |
| 3 | playMappedTone(3) | key=3 | NOTE_E4(330Hz)が100ms鳴る | tone(..., NOTE_E4, 100) | Pass（静的確認） |
| 4 | playMappedTone(4) | key=4 | NOTE_F4(349Hz)が100ms鳴る | tone(..., NOTE_F4, 100) | Pass（静的確認） |
| 5 | playMappedTone(5) | key=5 | NOTE_G4(392Hz)が100ms鳴る | tone(..., NOTE_G4, 100) | Pass（静的確認） |
| 6 | playMappedTone(6) | key=6 | NOTE_A4(440Hz)が100ms鳴る | tone(..., NOTE_A4, 100) | Pass（静的確認） |
| 7 | playMappedTone(7) | key=7 | NOTE_B4(494Hz)が100ms鳴る | tone(..., NOTE_B4, 100) | Pass（静的確認） |
| 8 | playMappedTone(8) | key=8 | NOTE_C5(523Hz)が100ms鳴る | tone(..., NOTE_C5, 100) | Pass（静的確認） |
| 9 | playMappedTone(0) | key=0 | 音が鳴らない | default分岐で無処理 | Pass（静的確認） |
| 10 | 正解通知処理 | 全一致条件を満たす | 500ms連続鳴動する | STATE_CORRECT_NOTIFYで500ms鳴動 | Pass（静的確認） |
| 11 | 不正解通知処理 | ミス条件を満たす | D11が0.2秒x3回点滅 | blinkWrongLed()で200ms x 3回点滅 | Pass（静的確認） |

### 4-3. タイミング・並行動作テスト

| No | テスト内容 | テスト手順 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | 即時比較判定 | オリジナル登録後、マネ1音目を誤入力 | その時点で不正解判定 | missDetected=trueでRESULT_WRONG | Pass（静的確認） |
| 2 | 全一致判定 | オリジナルと同順で全音入力 | 正解判定 | compareIndex==originalLengthでRESULT_CORRECT | Pass（静的確認） |
| 3 | タイムアウト判定 | マネ入力中に3秒以上停止 | 不正解判定 | STATE_COMPAREで3秒超過時RESULT_WRONG | Pass（静的確認） |
| 4 | 入力取りこぼし確認 | 連続入力を行う | 入力が無視されずに処理される | 実機未確認 | N/A（未実機） |
| 5 | 最大長境界値確認 | 20音入力後にさらに入力 | 20音を超えて記録しない | originalLength上限チェックあり | Pass（静的確認） |
| 6 | ターン切替時の論理クリア確認 | 正解/不正解後に次ターン開始 | 前ターン配列値を参照しない | resetGameState()で長さ0に戻す | Pass（静的確認） |
| 7 | デフォルト曲モード比較 | STATE_ORIGINAL_WAITでD12押下後、デフォルト曲どおりに入力 | オリジナル入力なしで正解判定 | デフォルト配列コピー+固定曲再生後に比較開始 | Pass（静的確認） |

---

## 5. 実施メモ

- 本結果は実機接続なしの静的確認結果。
- 実機での最終確認時は、音階・入力体感・LED点滅周期を再測定すること。
