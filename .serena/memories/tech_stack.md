# 技術スタック

## 開発フレームワーク
- **ESP-IDF v5.5.1**: Espressif IoT Development Framework
- **ビルドシステム**: CMake
- **RTOS**: FreeRTOS

## プログラミング言語
- **メイン**: C言語
- **テスト**: Python (pytest)

## ハードウェア
- **MCU**: ESP32-S3
  - Dual Core Xtensa LX7
  - WiFi/BT/BLE対応
  - PSRAM: 8MB (SPIRAM)
  - Flash: 2MB
  - CPU周波数: 160MHz
- **ディスプレイ**: Waveshare 7.3インチ 7色E-Paper
  - 解像度: 800x480
  - 色数: 7色 (黒、白、緑、青、赤、黄、オレンジ)
  - インターフェース: SPI (SPI3_HOST, 10MHz)

## コンポーネント構成(予定)
- `epd_driver`: E-Paperディスプレイドライバ
- `gfx_library`: グラフィックライブラリ
- `weather_service`: 天気情報取得(HTTPS, JSON)
- `calendar_ui`: カレンダーUI
- `wifi_manager`: WiFi接続・SNTP同期

## 外部API
- **OpenWeatherMap API**: 天気予報データ取得
  - エンドポイント: `api.openweathermap.org/data/2.5/forecast`
  - 無料枠: 1000 calls/日

## ライブラリ
- ESP-IDF標準コンポーネント:
  - `esp_http_client`: HTTPSクライアント
  - `cJSON`: JSON解析
  - `nvs_flash`: 不揮発性ストレージ
  - `esp_wifi`: WiFi接続
  - `esp_sntp`: SNTP時刻同期
  - `driver/spi_master`: SPIドライバ
  - `driver/gpio`: GPIOドライバ

## メモリ管理
- **PSRAM**: フレームバッファ(384KB) + JSON解析(16-32KB)
- **内部RAM**: DMAバッファ(4KB) + 天気データ構造体
- **RTCメモリ**: 天気キャッシュ(高速起動用)
