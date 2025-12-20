# ESP32-S3 フルカラーE-inkカレンダー 実装計画書

> **プロジェクト**: esp32-s3-photopainter
> **ハードウェア**: Waveshare ESP32-S3 PhotoPainter + 7.3インチ 7色E-Paperディスプレイ(800x480)
> **開発期間**: 約21日間
> **最終更新**: 2025-12-20

---

## 📋 プロジェクト概要

Waveshare 7.3インチ 7色e-paperディスプレイ(800x480)を使用した、**4日間天気予報付きカレンダー**をESP32-S3上に実装します。

サンプルコード([xiaozhi-esp32-sample](file:///C:/Users/iris/Projects/xiaozhi-esp32-sample))を参考にしながら、本プロジェクト専用に最適化された新規コードを作成します。

### 主要機能
- ✅ 4日間の天気予報表示
- ✅ 現在の日付・時刻表示
- ✅ 温度、湿度、風速・風向表示
- ✅ 天気アイコン表示(7色カラー)
- ✅ WiFi経由でOpenWeatherMap APIから天気データ取得
- ✅ SNTP時刻同期
- ✅ ディープスリープによる省電力動作(30分〜2時間周期)

---

## 🎯 実装方針

| 項目 | 内容 |
|------|------|
| **実装範囲** | 4日間天気予報、日付・時刻表示を含むフルカレンダー機能 |
| **コードアプローチ** | サンプルを参考に新規実装(理解して実装、コピペNG) |
| **天気API** | OpenWeatherMap API (無料枠1000calls/日) |
| **アーキテクチャ** | ESP-IDFコンポーネント方式でモジュール化 |
| **ハードウェア** | Waveshare ESP32-S3 PhotoPainterボード(サンプルと同じピン配置) |
| **認証情報管理** | menuconfig経由で設定(開発時の利便性重視) |

---

## 🏗️ アーキテクチャ設計

### コンポーネント構成

```
components/
├── epd_driver/          # E-Paperディスプレイドライバ
│   ├── epd_driver.c     # SPI通信、GPIO制御、コマンド送信
│   ├── epd_spi.c        # SPI初期化・データ転送
│   └── include/epd_driver.h
│
├── gfx_library/         # グラフィックライブラリ
│   ├── gfx_paint.c      # 描画関数、バッファ管理
│   ├── gfx_primitives.c # 図形描画(線、矩形、円)
│   ├── fonts/           # フォントデータ
│   │   ├── font_16.c    # 16pxフォント
│   │   ├── font_24.c    # 24pxフォント
│   │   └── font_32.c    # 32pxフォント
│   └── include/gfx_paint.h
│
├── weather_service/     # 天気情報取得
│   ├── weather_http.c   # HTTPSクライアント
│   ├── weather_parser.c # JSON解析
│   └── include/
│       ├── weather_service.h
│       └── weather_types.h
│
├── calendar_ui/         # カレンダーUI
│   ├── calendar_ui.c    # レイアウト描画
│   ├── weather_icons.c  # 天気アイコン
│   └── include/calendar_ui.h
│
└── wifi_manager/        # WiFi接続管理
    ├── wifi_manager.c   # WiFi接続、SNTP時刻同期
    └── include/wifi_manager.h
```

### メモリ戦略

| メモリ領域 | 用途 | サイズ |
|-----------|------|--------|
| **PSRAM** | フレームバッファ | 384KB (800×480) |
| **PSRAM** | JSON解析バッファ | 16-32KB |
| **内部RAM** | DMAバッファ(SPI転送) | 4KB |
| **内部RAM** | 天気データ構造体 | 数KB(高速アクセス) |
| **RTCメモリ** | 天気キャッシュ | 数KB(高速起動用) |

### ハードウェアピン配置

Waveshare ESP32-S3 PhotoPainter標準ピン配置:

| 信号 | GPIO | 説明 |
|------|------|------|
| DC | 8 | Data/Command制御 |
| CS | 9 | Chip Select |
| SCK | 10 | SPI Clock |
| MOSI | 11 | SPI Data Out |
| RST | 12 | Reset |
| BUSY | 13 | Busy Status |

**SPI設定**: SPI3_HOST, 10MHz, DMA有効

---

## 📅 実装ステップ (21日間)

### フェーズ1: 基盤セットアップ (1-2日目)

**1.1 プロジェクト設定**
- [ ] `idf.py menuconfig`でPSRAM有効化
- [ ] WiFi、HTTPS、cJSONコンポーネント有効化
- [ ] SPI設定確認
- [ ] パーティションテーブル設定(NVS用)
- [ ] `sdkconfig.defaults`作成

**1.2 コンポーネント構造作成**
- [ ] `components/`配下に各ディレクトリ作成
- [ ] 各コンポーネントの`CMakeLists.txt`作成
- [ ] ルート`CMakeLists.txt`の`MINIMAL_BUILD`削除

**1.3 WiFiマネージャ実装**
- [ ] WiFi Station接続機能
- [ ] NVSから認証情報読み込み
- [ ] SNTP時刻同期
- [ ] 接続ステータスAPI

**成果物**:
- [components/wifi_manager/wifi_manager.c](components/wifi_manager/wifi_manager.c)
- [components/wifi_manager/include/wifi_manager.h](components/wifi_manager/include/wifi_manager.h)
- [main/Kconfig.projbuild](main/Kconfig.projbuild)

---

### フェーズ2: ディスプレイドライバ (3-5日目)

**2.1 SPI & GPIO セットアップ**
- [ ] SPI3_HOST設定(10MHz、DMA有効)
- [ ] GPIOピン設定(上記ピン配置表参照)
- [ ] ハードウェアリセット処理
- [ ] BUSYピン監視機能(タイムアウト付き)

**2.2 ディスプレイコマンド実装**
- [ ] コマンド送信関数(DCピン制御)
- [ ] データ送信関数
- [ ] 初期化シーケンス実装
  ```
  Power Setting → Booster Soft Start → Power On →
  Panel Setting(7-color) → Resolution(800x480) →
  VCOM/Data Interval → TCON Setting
  ```

**2.3 バッファ & リフレッシュ**
- [ ] PSRAM バッファ割り当て(384KB)
- [ ] バッファ→ディスプレイ転送(SPI DMA)
- [ ] ディスプレイリフレッシュコマンド
- [ ] ディスプレイスリープモード

**成果物**:
- [components/epd_driver/epd_driver.c](components/epd_driver/epd_driver.c)
- [components/epd_driver/epd_spi.c](components/epd_driver/epd_spi.c)
- [components/epd_driver/include/epd_driver.h](components/epd_driver/include/epd_driver.h)

---

### フェーズ3: グラフィックライブラリ (6-8日目)

**3.1 描画プリミティブ**
- [ ] バッファ初期化・クリア関数
- [ ] ピクセル描画(7色パレット対応)
- [ ] 直線描画(Bresenhamアルゴリズム)
- [ ] 矩形描画(塗りつぶし/枠線)
- [ ] 円描画(Midpointアルゴリズム)

**3.2 フォントシステム**
- [ ] ビットマップフォント作成(16px, 24px, 32px)
- [ ] ASCII文字描画
- [ ] 文字列描画
- [ ] テキスト幅測定ユーティリティ

**3.3 ユーティリティ関数**
- [ ] 時刻フォーマット(HH:MM:SS)
- [ ] 日付フォーマット(YYYY/MM/DD)
- [ ] 温度フォーマット(度記号付き)
- [ ] テキスト位置揃え(中央、右揃え)

**7色パレット定義**:
```c
EPD_COLOR_BLACK = 0   // 黒
EPD_COLOR_WHITE = 1   // 白
EPD_COLOR_GREEN = 2   // 緑
EPD_COLOR_BLUE = 3    // 青
EPD_COLOR_RED = 4     // 赤
EPD_COLOR_YELLOW = 5  // 黄
EPD_COLOR_ORANGE = 6  // オレンジ
```

**成果物**:
- [components/gfx_library/gfx_paint.c](components/gfx_library/gfx_paint.c)
- [components/gfx_library/gfx_primitives.c](components/gfx_library/gfx_primitives.c)
- [components/gfx_library/fonts/font_16.c](components/gfx_library/fonts/font_16.c)
- [components/gfx_library/fonts/font_24.c](components/gfx_library/fonts/font_24.c)
- [components/gfx_library/fonts/font_32.c](components/gfx_library/fonts/font_32.c)
- [components/gfx_library/include/gfx_paint.h](components/gfx_library/include/gfx_paint.h)

---

### フェーズ4: 天気サービス統合 (9-11日目)

**4.1 HTTPクライアント**
- [ ] ESP-IDF HTTP clientコンポーネント設定
- [ ] SSL/TLS設定(HTTPS対応)
- [ ] OpenWeatherMap API呼び出し
  - エンドポイント: `api.openweathermap.org/data/2.5/forecast`
  - パラメータ: `lat`, `lon`, `appid`, `units=metric`, `cnt=32`

**4.2 JSONパーサー**
- [ ] cJSONコンポーネント追加
- [ ] 天気データ構造体定義

```c
typedef struct {
    time_t timestamp;
    float temp, temp_min, temp_max;
    int humidity;
    char description[64];
    char icon_code[4];
    float wind_speed;
    int wind_deg;
} weather_forecast_t;

typedef struct {
    weather_forecast_t daily[4];  // 4日分
    char city_name[64];
    time_t last_update;
    bool valid;
} weather_data_t;
```

- [ ] JSON解析実装(3時間毎データ→正午データ抽出で4日分)

**4.3 データ処理**
- [ ] 日次予報フィルタリング(正午のデータを選択)
- [ ] エラーハンドリング(タイムアウト、HTTP エラー)
- [ ] リトライロジック(指数バックオフ)
- [ ] NVSキャッシュ(オフライン時のフォールバック)

**成果物**:
- [components/weather_service/weather_http.c](components/weather_service/weather_http.c)
- [components/weather_service/weather_parser.c](components/weather_service/weather_parser.c)
- [components/weather_service/include/weather_service.h](components/weather_service/include/weather_service.h)
- [components/weather_service/include/weather_types.h](components/weather_service/include/weather_types.h)

---

### フェーズ5: カレンダーUI (12-15日目)

**5.1 レイアウト設計**

800x480ディスプレイ分割:
```
┌─────────────────────────────────────────────────┐
│ [現在日時]                     [天気アイコン]  │ 0-80px
├─────────────────────────────────────────────────┤
│ 1日目      2日目      3日目      4日目         │ 80-480px
│ [アイコン] [アイコン] [アイコン] [アイコン]    │
│ 月 21日    火 22日    水 23日    木 24日       │
│ 12°-18°    10°-16°    14°-20°    15°-22°      │
│ 湿度65%    湿度70%    湿度55%    湿度60%        │
│ NE 12km/h  E 8km/h    SW 15km/h  W 10km/h      │
│ 晴れ時々曇り 雨        快晴       曇り          │
└─────────────────────────────────────────────────┘
```

**レイアウト仕様**:
- 各予報カラム幅: 200px (800÷4)
- アイコンエリア: y=80-200 (120×120px)
- テキストエリア: y=200-480

**5.2 描画関数実装**
- [ ] `draw_header()` - 日付、時刻、現在天気
- [ ] `draw_forecast_day()` - 1日分の予報カラム
- [ ] `draw_weather_icon()` - 天気アイコン描画
- [ ] `draw_temperature_bar()` - 温度範囲ビジュアル表示
- [ ] `draw_full_calendar()` - 全体レイアウト統合

**5.3 天気アイコン**
- [ ] シンプルな幾何学図形でアイコン作成
  - 太陽: 黄色い円+放射線
  - 雲: 白い楕円+黒枠
  - 雨: 雲の下に青い線
  - 雷: 雲の下に黄色のジグザグ
- [ ] OpenWeatherMapアイコンコードマッピング
  - `01d/01n` → 晴れ
  - `02d/02n` → やや曇り
  - `10d/10n` → 雨
  - `11d/11n` → 雷雨

**5.4 色使い**
- 背景: 白 (1)
- テキスト: 黒 (0)
- 高温: 赤(4)またはオレンジ(6)
- 低温: 青(3)
- 良好状態: 緑(2)
- 警告: 赤(4)

**成果物**:
- [components/calendar_ui/calendar_ui.c](components/calendar_ui/calendar_ui.c)
- [components/calendar_ui/weather_icons.c](components/calendar_ui/weather_icons.c)
- [components/calendar_ui/include/calendar_ui.h](components/calendar_ui/include/calendar_ui.h)

---

### フェーズ6: メインアプリケーション (16-17日目)

**6.1 ステートマシン**
```c
typedef enum {
    STATE_INIT,
    STATE_WIFI_CONNECT,
    STATE_SYNC_TIME,
    STATE_FETCH_WEATHER,
    STATE_RENDER_DISPLAY,
    STATE_DEEP_SLEEP,
    STATE_ERROR
} app_state_t;
```

**6.2 メインループ実装**
- [ ] NVS初期化
- [ ] 起動理由チェック(タイマー/コールドブート)
- [ ] WiFi接続
- [ ] SNTP時刻同期(24時間未同期の場合のみ)
- [ ] EPDドライバ初期化
- [ ] 天気データ取得
- [ ] グラフィックライブラリ初期化
- [ ] カレンダーUI描画
- [ ] バッファ転送→ディスプレイリフレッシュ
- [ ] ディスプレイスリープ
- [ ] WiFi切断
- [ ] ディープスリープ設定(30分)
- [ ] ディープスリープ開始

**6.3 更新戦略**
- **昼間**(6:00-22:00): 30分毎に更新
- **夜間**(22:00-6:00): 2時間毎に更新
- **天気API呼び出し**: 3時間毎(1日8回、上限1000回に余裕あり)
- **RTCメモリ**: 天気キャッシュ保存(高速起動)

**成果物**:
- [main/main.c](main/main.c)

---

### フェーズ7: 電源管理 (18日目)

**7.1 ディープスリープ設定**
- [ ] タイマー起動設定(30分/2時間)
- [ ] RTCメモリに天気データ保存
- [ ] WiFi状態保存
- [ ] NVSへのキャッシュ保存

**7.2 省電力最適化**
- [ ] ディスプレイスリープコマンド
- [ ] BUSY待機時間最小化
- [ ] SPI初期化解除
- [ ] WiFi切断

**消費電力見積もり(バッテリー動作想定)**:
- アクティブ時間: 約200mA × 20-30秒
- ディープスリープ: 約10μA
- **3000mAhバッテリーで約650日動作(理論値)**

---

### フェーズ8: エラーハンドリング (19日目)

**8.1 ネットワークエラー**
- [ ] WiFi接続タイムアウト(30秒)
- [ ] HTTPリクエストタイムアウト(10秒)
- [ ] 指数バックオフリトライ(3回)
- [ ] キャッシュデータへのフォールバック
- [ ] エラー表示画面

**8.2 ディスプレイエラー**
- [ ] SPI通信確認
- [ ] BUSYタイムアウト処理(最大20秒)
- [ ] ディスプレイリセット処理

**8.3 メモリ安全性**
- [ ] バッファ境界チェック
- [ ] NULLポインタチェック
- [ ] メモリリーク防止(適切なfree呼び出し)
- [ ] スタックオーバーフロー保護

---

### フェーズ9: テスト & 最適化 (20-21日目)

**9.1 ユニットテスト**
- [ ] 描画プリミティブテスト
- [ ] JSON解析テスト(モックデータ)
- [ ] 時刻フォーマットテスト
- [ ] カラーパレット動作確認

**9.2 統合テスト**
- [ ] フルワークフローテスト(起動→表示→スリープ)
- [ ] ネットワーク障害シナリオ
- [ ] ディスプレイリフレッシュサイクル
- [ ] メモリ使用量モニタリング

**9.3 最適化**
- [ ] ヒープ断片化の最小化
- [ ] SPI転送速度最適化
- [ ] 起動→スリープ時間短縮(目標30秒以内)

---

## ⚙️ 設定ファイル

### sdkconfig.defaults

プロジェクトルートに作成:

```ini
# PSRAM設定
CONFIG_SPIRAM=y
CONFIG_SPIRAM_MODE_OCT=y
CONFIG_SPIRAM_SPEED_80M=y
CONFIG_SPIRAM_BOOT_INIT=y
CONFIG_SPIRAM_USE_MALLOC=y
CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=16384

# WiFi設定
CONFIG_ESP_WIFI_STATIC_RX_BUFFER_NUM=10
CONFIG_ESP_WIFI_DYNAMIC_RX_BUFFER_NUM=32
CONFIG_ESP_WIFI_DYNAMIC_TX_BUFFER_NUM=32

# FreeRTOS
CONFIG_FREERTOS_HZ=1000

# パーティションテーブル
CONFIG_PARTITION_TABLE_CUSTOM=y

# メインタスクスタック
CONFIG_ESP_MAIN_TASK_STACK_SIZE=8192
```

### main/Kconfig.projbuild

menuconfig設定用:

```kconfig
menu "Weather Calendar Configuration"

    config WIFI_SSID
        string "WiFi SSID"
        default "myssid"

    config WIFI_PASSWORD
        string "WiFi Password"
        default "mypassword"

    config WEATHER_API_KEY
        string "OpenWeatherMap API Key"
        default "your_api_key_here"
        help
            Get your API key from https://openweathermap.org/api

    config WEATHER_LATITUDE
        string "Location Latitude"
        default "35.6762"
        help
            Latitude for weather location (Tokyo default)

    config WEATHER_LONGITUDE
        string "Location Longitude"
        default "139.6503"
        help
            Longitude for weather location (Tokyo default)

endmenu
```

設定方法:
```bash
idf.py menuconfig
# Weather Calendar Configuration メニューで設定
```

---

## 📁 重要ファイル一覧

実装時に最も重要な5つのファイル:

| 優先度 | ファイルパス | 役割 |
|--------|-------------|------|
| ⭐⭐⭐ | [components/epd_driver/epd_driver.c](components/epd_driver/epd_driver.c) | ディスプレイドライバの中核 |
| ⭐⭐⭐ | [components/gfx_library/gfx_paint.c](components/gfx_library/gfx_paint.c) | グラフィック描画エンジン |
| ⭐⭐⭐ | [components/weather_service/weather_parser.c](components/weather_service/weather_parser.c) | 天気データ解析 |
| ⭐⭐ | [components/calendar_ui/calendar_ui.c](components/calendar_ui/calendar_ui.c) | UIレイアウトロジック |
| ⭐⭐ | [main/main.c](main/main.c) | アプリケーションメインロジック |

---

## 🔧 技術的考慮事項

### ディスプレイリフレッシュ特性
- **フルリフレッシュ時間**: 15-20秒
- **ゴースト防止**: 毎回フルリフレッシュ(部分更新なし)
- **推奨更新間隔**: 30分以上
- **ディスプレイ寿命**: 約10万回のリフレッシュ

### メモリマップ
| 領域 | 用途 |
|------|------|
| PSRAM | フレームバッファ(384KB) + JSON解析(16-32KB) |
| 内部RAM | DMAバッファ(4KB) + 天気データ構造体 |
| RTCメモリ | 天気キャッシュ、最終API呼び出し時刻 |

### データフロー
```
起動 → WiFi接続 → NTP同期 → 天気取得 → UI描画 →
ディスプレイ転送 → リフレッシュ → ディープスリープ
```

---

## 💡 開発のヒント

1. **インクリメンタルテスト**: 各コンポーネントを独立してテスト
2. **ESP_LOGを活用**: デバッグ出力を豊富に
3. **モックデータ優先**: UI開発時はハードコードした天気データを使用
4. **メモリプロファイリング**: 各段階でヒープ使用量を監視
5. **GPIO確認**: ピン配置を再確認
6. **SPI速度**: 最初は1MHzで動作確認、安定後に10MHzへ

---

## 🚀 オプション機能(MVP後)

実装優先度が高い順:

- [ ] 複数地域サポート
- [ ] 月相表示
- [ ] 日の出/日の入時刻
- [ ] 大気質指数(AQI)
- [ ] バッテリー電圧表示
- [ ] OTAアップデート対応

---

## ⚠️ 想定される課題

| 課題 | 対策 |
|------|------|
| **ディスプレイタイミング** | BUSYシグナルの正確な処理、タイムアウト実装 |
| **JSON解析** | 大きなレスポンスでメモリ不足 → ストリーミングパーサー検討 |
| **HTTPS証明書** | ルートCA証明書の埋め込み必要な場合あり |
| **タイムゾーン** | POSIX形式のタイムゾーン文字列設定 |
| **フォント容量** | ビットマップフォントはフラッシュ容量消費 → 必要最小限に |

---

## 📊 進捗管理

実装の進捗は各フェーズのチェックリストで管理してください。

**現在のステータス**: 計画完了 → 実装開始待ち

---

## 📚 参考資料

- [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/)
- [OpenWeatherMap API Documentation](https://openweathermap.org/api)
- [Waveshare E-Paper Display Datasheet](https://www.waveshare.com/wiki/7.3inch_e-Paper_HAT)
- サンプルコード: `C:\Users\iris\Projects\xiaozhi-esp32-sample`

---

## まとめ

この計画に従って**21日間**で実装を完了します。

モジュール化されたアーキテクチャにより、各コンポーネントを独立して開発・テスト可能です。

**成功の鍵**:
- ✅ 堅牢なEPDドライバ基盤(ハードウェアタイミングが重要)
- ✅ 効率的なPSRAM使用(384KBバッファ + JSON解析)
- ✅ 堅実なエラーハンドリング(ネットワーク障害は頻発)
- ✅ モジュール化されたアーキテクチャ(テスト・デバッグが容易)
- ✅ 省電力最適化(ディープスリープでバッテリー動作)

---

**作成日**: 2025-12-20
**プロジェクトリード**: Claude Sonnet 4.5
**ライセンス**: プロジェクトに準拠
