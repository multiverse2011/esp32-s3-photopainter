# Implementation Plan: E-ink Display Design Improvement

**Branch**: `002-improve-display-design` | **Date**: 2025-12-20 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification from `/specs/002-improve-display-design/spec.md`

## Summary

E-ink天気カレンダーのUIデザインを刷新し、左サイドバー（日付大表示 + Tasksセクション）と右コンテンツエリア（5時間帯の天気 + Trainセクション + 更新日時）の2カラムレイアウトを実装します。既存のcalendar_ui.cを拡張し、新しいデータソース（タスクAPI、電車遅延スクレイピング）を統合します。

## Technical Context

**Language/Version**: C (ESP-IDF v5.x, FreeRTOS)
**Primary Dependencies**: ESP-IDF (WiFi, HTTPS, SNTP), cJSON, SPI Driver, gfx_library
**Storage**: NVS (Non-Volatile Storage) + RTC Memory (deep sleep survive)
**Testing**: Manual testing on hardware (ESP32-S3 + E-Paper)
**Target Platform**: ESP32-S3 (Waveshare PhotoPainter board)
**Project Type**: Embedded single project
**Performance Goals**: ディスプレイ更新15秒以内、起動から表示完了30秒以内
**Constraints**: 800x480 7色E-Paper、PSRAM 8MB、Flash 16MB、バッテリー駆動
**Scale/Scope**: 単一デバイス、1日8回以下のAPI呼び出し

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

Constitution未定義（テンプレート状態）のため、以下の基本原則を適用:

| Gate | Status | Notes |
|------|--------|-------|
| コード品質 | PASS | 既存コードスタイルを維持 |
| テスト可能性 | PASS | モジュール分離された設計 |
| 複雑性制限 | PASS | 既存アーキテクチャを拡張 |

## Project Structure

### Documentation (this feature)

```text
specs/002-improve-display-design/
├── plan.md              # This file
├── research.md          # Phase 0 output
├── data-model.md        # Phase 1 output
├── quickstart.md        # Phase 1 output
├── contracts/           # Phase 1 output (API contracts)
└── tasks.md             # Phase 2 output (/speckit.tasks command)
```

### Source Code (repository root)

```text
components/
├── calendar_ui/
│   ├── calendar_ui.c        # UI描画ロジック (大幅改修)
│   ├── weather_icons.c      # アイコン描画 (改良)
│   └── include/
│       └── calendar_ui.h    # UIヘッダー (拡張)
├── task_service/            # NEW: タスクデータ取得
│   ├── task_service.c
│   ├── task_parser.c
│   └── include/
│       ├── task_service.h
│       └── task_types.h
├── train_service/           # NEW: 電車遅延情報取得
│   ├── train_service.c
│   ├── train_parser.c
│   └── include/
│       ├── train_service.h
│       └── train_types.h
├── weather_service/         # 既存 (3時間予報対応に改修)
│   ├── weather_http.c
│   ├── weather_parser.c
│   └── include/
│       ├── weather_service.h
│       └── weather_types.h  # 5時間帯対応に拡張
├── gfx_library/             # 既存
├── epd_driver/              # 既存
└── wifi_manager/            # 既存

main/
└── main.c                   # メインアプリ (状態機械拡張)
```

**Structure Decision**: 既存のESP-IDFコンポーネント構造を維持し、task_serviceとtrain_serviceを新規コンポーネントとして追加。calendar_ui.cは大幅に改修して新レイアウトを実装。

## Complexity Tracking

| Violation | Why Needed | Simpler Alternative Rejected Because |
|-----------|------------|-------------------------------------|
| 2つの新規外部サービス統合 | ユーザー要件（Tasks + Train表示） | 単一機能では価値提供が不十分 |
| HTMLスクレイピング | JR東日本公式ページしか選択肢がない | 公式APIが存在しない |

## Key Technical Decisions

### 1. レイアウト実装

- 画面を左サイドバー（約200px幅）と右コンテンツエリア（約600px幅）に分割
- 左サイドバー: 日付（大）+ 月名 + Tasksセクション
- 右コンテンツエリア: Weather（5列）+ Train + Updated

### 2. 天気データ構造変更

- 現在: `weather_forecast_t daily[4]` (4日間日別)
- 変更後: `weather_forecast_t hourly[5]` (5時間帯)
- 取得タイミング: 更新時の現在時刻から3時間間隔で5つ

### 3. 新規サービス統合

- **task_service**: Google Calendar API または Todoist API からタスク取得
- **train_service**: JR東日本運行情報ページをHTTPSでフェッチしHTMLパース

### 4. キャッシュ戦略

- 天気データ: 既存のNVS + RTC キャッシュを継続
- タスクデータ: NVSキャッシュ（1日有効）
- 電車遅延: NVSキャッシュ（30分有効）
