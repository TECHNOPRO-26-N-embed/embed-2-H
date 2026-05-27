# 詳細設計書 — 音程・音量チューニングシステム（改訂版）

<!-- 作成者: 吉田愛彩香 / 日付: 2026-05-27 / グループ: 2-H -->

## 0. 実装範囲

- エンコーダ入力（暴走抑制あり）
- ポテンショメータ音量更新（数値表示あり）
- 2メロディ切替（ドレミファソ/ソファミレド）
- LED表示（音有無連動）
- 全関数全分岐のシリアル出力

## 1. 主要定数と変数

### 1-1. 主要定数
- ENCODER_INTERVAL_MS = 10
- ENCODER_STEP_GUARD_MS = 25
- ENCODER_NOISE_GUARD_MS = 2
- SOUND_INTERVAL_MS = 50
- POT_INTERVAL_MS = 50
- NOTE_HOLD_TICKS = 4
- MELODY_LEN = 5

### 1-2. 主要状態変数
- currentState / previousState
- pitchIndex / volumeLevel / melodyMode / targetFreqHz
- isToneActive
- melodyNoteIndex / melodyTickCount
- encoderJumpDetected / encoderBurstCount

## 2. 関数詳細

### setup
1. ピン初期化
2. 状態初期化
3. シリアル開始
4. 起動LED表示

### loop
1. nowMs更新
2. readEncoder, handleButton, adjustVolume, checkError呼び出し
3. 状態遷移ログ出力
4. stateごとに処理

### readEncoder
1. 10ms周期判定
2. CLKエッジ検知
3. ノイズガード（極短時間エッジを無効化）
4. 受理間隔ガード（連続ステップ抑制）
5. バースト検知（3回以上でジャンプ検知）
6. -1/0/+1を返す

### handleButton
1. デバウンス
2. 立下りで `changeMelodyPattern` 呼び出し
3. 切替有無を返す

### changeMelodyP`attern
1. melodyMod`eを0/1で巡回
2. melodyNoteIndex と melodyTickCount をリセット

### updateSoundParams
1. pitchIndexにstep加算
2. クリップ
3. targetFreqHz再計算
4. 範囲外チェック

### playCurrentSound
1. melodyModeでオフセット配列選択
2. 周波数計算
3. 音量ゲートで tone/noTone 切替
4. isToneActive更新
5. NOTE_HOLD_TICKSごとに次ノートへ

### adjustVolume
1. 50ms周期でA0取得
2. 平滑化
3. 0..10へマップ
4. 変更時ログ（raw, percent, level）
5. 1秒周期モニタログ

### showStatusLed
1. 100ms周期で更新
2. 音あり: 緑ON/赤OFF
3. 音なし: 緑OFF/赤ON
4. ERROR時: 赤固定

### checkError
1. pitch/volume/freq/melody範囲確認
2. encoderJumpDetected確認
3. detected時errorFlag/errorCode設定

## 3. 分岐トレース設計
- マクロ TRACE_BRANCH("[COV] ...") を利用
- 各 if/else, switch case, return前でログ
- 同一分岐の重複ログを避けるため初回のみ表示

## 4. 単体テスト仕様（更新版）

| No | 対象 | 入力 | 期待結果 |
|:--|:--|:--|:--|
| 1 | readEncoder | ゆっくり1ノッチ回転 | stepが±1で増減 |
| 2 | readEncoder | 速回転/ノイズ入力 | burst step ignored が出る |
| 3 | handleButton/changeMelodyPattern | ボタン押下 | melodyModeが0/1で切替 |
| 4 | playCurrentSound | melodyMode=0 | ドレミファソ順で再生 |
| 5 | playCurrentSound | melodyMode=1 | ソファミレド順で再生 |
| 6 | adjustVolume | Potを可変 | raw, percent, level が出力 |
| 7 | showStatusLed | tone/noTone切替 | 音あり緑、音なし赤 |
| 8 | checkError | 範囲外値注入 | errorCode更新、ERROR遷移 |

## 5. テスト実施時の確認手順
1. シリアルモニタを9600bpsで開く
2. 操作ごとに [COV] ログが出るか確認する
3. すべての分岐ログが1回以上表示されたら全分岐通過と判定する
