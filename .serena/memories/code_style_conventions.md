# コードスタイルと規約

## ファイル構造
- ヘッダーファイルに SPDX ライセンス情報を含める
  ```c
  /*
   * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
   *
   * SPDX-License-Identifier: CC0-1.0
   */
  ```

## 命名規約

### ファイル名
- C言語ソース: `snake_case.c`
- ヘッダー: `snake_case.h`
- コンポーネント名: `snake_case`

### 関数・変数名
- 関数: `snake_case` (例: `epd_init()`, `weather_fetch()`)
- 変数: `snake_case` (例: `frame_buffer`, `weather_data`)
- グローバル定数: `UPPER_SNAKE_CASE` (例: `EPD_COLOR_BLACK`)
- マクロ: `UPPER_SNAKE_CASE`

### コンポーネント別プレフィックス
各コンポーネントの関数には専用プレフィックスを付与:
- EPDドライバ: `epd_*`
- グラフィック: `gfx_*`
- 天気サービス: `weather_*`
- カレンダーUI: `ui_*`
- WiFiマネージャ: `wifi_*`

### 型名
- 構造体: `typedef struct { ... } name_t;`
- 列挙型: `typedef enum { ... } name_t;`

例:
```c
typedef struct {
    time_t timestamp;
    float temp;
    int humidity;
} weather_forecast_t;

typedef enum {
    EPD_COLOR_BLACK = 0,
    EPD_COLOR_WHITE = 1
} epd_color_t;
```

## コーディングスタイル

### インデント
- スペース4つ(タブは使用しない)

### ブレース
- K&Rスタイル(関数定義は次の行、制御文は同じ行)
```c
void function_name(void)
{
    if (condition) {
        // code
    }
}
```

### コメント
- C形式 `/* */` を使用
- 複数行コメントは各行に `*` を付与
- 関数の上にブロックコメントで説明を記載

### ヘッダーガード
- `#ifndef`, `#define`, `#endif` を使用
- 命名: `COMPONENT_NAME_H`

例:
```c
#ifndef EPD_DRIVER_H
#define EPD_DRIVER_H

// code

#endif // EPD_DRIVER_H
```

## ESP-IDF特有の規約

### ログ出力
- `ESP_LOGI()`, `ESP_LOGW()`, `ESP_LOGE()` を使用
- TAGマクロを定義: `static const char *TAG = "component_name";`

### エラーハンドリング
- `esp_err_t` 型を使用
- 戻り値: `ESP_OK` (成功), `ESP_FAIL` (失敗), その他エラーコード

### メモリ割り当て
- PSRAM: `heap_caps_malloc(size, MALLOC_CAP_SPIRAM)`
- DMA対応メモリ: `heap_caps_malloc(size, MALLOC_CAP_DMA)`
- 通常ヒープ: `malloc()` または `calloc()`

## ドキュメント

### 関数ドキュメント
重要な関数には説明を記載:
```c
/**
 * @brief Initialize EPD driver
 * 
 * @return ESP_OK on success, ESP_FAIL on failure
 */
esp_err_t epd_init(void);
```

### TODO/FIXME
- 未実装: `// TODO: description`
- 修正必要: `// FIXME: description`
- 最適化必要: `// OPTIMIZE: description`

## デザインパターン

### コンポーネント分離
- 各コンポーネントは独立した責任を持つ
- ヘッダーファイルで公開API、ソースファイルで実装

### エラーハンドリング
- 早期リターン: エラー時は即座に return
- NULLチェック: ポインタ引数は常にチェック
- 境界チェック: 配列アクセス時は範囲確認

### リソース管理
- 割り当てたメモリは必ず解放
- 初期化した資源は必ず後始末(SPI, GPIO等)

## 実装ガイドライン

### セキュリティ
- バッファオーバーフローに注意
- 入力値の検証
- センシティブ情報(API Key等)はソースコードに直接記載しない
- menuconfigやNVSから読み込む

### パフォーマンス
- PSRAM使用で大容量バッファを確保
- DMA転送でSPI高速化
- ディープスリープで省電力化

### 可読性
- 関数は単一責任
- 1関数は50行程度を目安(複雑な場合は分割)
- マジックナンバーは定数化
