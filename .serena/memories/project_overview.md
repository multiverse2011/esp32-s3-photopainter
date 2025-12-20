# プロジェクト概要

## プロジェクト名
esp32-s3-photopainter

## 目的
Waveshare ESP32-S3 PhotoPainterボード + 7.3インチ 7色E-Paperディスプレイ(800x480)を使用した、**4日間天気予報付きカレンダー**の実装。

## 主要機能
- 4日間の天気予報表示(OpenWeatherMap API使用)
- 現在の日付・時刻表示(SNTP時刻同期)
- 温度、湿度、風速・風向の表示
- 天気アイコン表示(7色カラー対応)
- WiFi経由でのデータ取得
- ディープスリープによる省電力動作(30分〜2時間周期)

## ハードウェア
- **ボード**: Waveshare ESP32-S3 PhotoPainter
- **ディスプレイ**: 7.3インチ 7色E-Paper (800x480)
- **MCU**: ESP32-S3 (Dual Core, PSRAM搭載)

## 開発期間
約21日間(9フェーズに分割)

## プロジェクトステータス
現在は「Hello World」テンプレートベースの初期状態。
IMPLEMENTATION_PLAN.mdに従って段階的に実装予定。
