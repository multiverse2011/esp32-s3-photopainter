# Train Service Contract

**Service**: train_service
**Version**: 1.0.0
**Date**: 2025-12-20

## Overview

JR東日本運行情報ページから電車遅延情報を取得するサービス。HTMLスクレイピングによるデータ抽出。

## API Contract

### External Source (JR East Web Page)

**URL**: `https://traininfo.jreast.co.jp/train_info/kanto.aspx`

**Method**: GET (HTTPS)

**Response**: HTML (Shift_JIS encoded)

**Page Structure** (relevant elements):
- 路線ごとの運行状況がテーブル形式で表示
- 「平常運転」: 通常運行
- 「遅延」「運転見合わせ」「運休」: 異常あり
- 路線名と状態が近接して配置

**Parsing Strategy**:
1. HTMLをストリーミングで受信
2. 対象路線名（例: "中央線"）を検索
3. 近接テキストから状態を判定
4. Shift_JIS → UTF-8変換

## Internal API (C Functions)

### train_service_init

```c
/**
 * @brief Initialize train service
 * @return ESP_OK on success
 */
esp_err_t train_service_init(void);
```

### train_service_fetch

```c
/**
 * @brief Fetch train delay status for configured line
 * @param[out] status Output train status structure
 * @return ESP_OK on success
 * @return ESP_ERR_TIMEOUT on network timeout
 * @return ESP_ERR_NOT_FOUND if line not found in page
 * @return ESP_ERR_INVALID_RESPONSE on parse error
 */
esp_err_t train_service_fetch(train_status_t *status);
```

**Behavior**:
1. HTTPSでページを取得（Shift_JIS）
2. 対象路線名を検索
3. 状態テキストをパース（平常運転/遅延/運転見合わせ）
4. `train_status_t`構造体に格納

### train_service_deinit

```c
/**
 * @brief Deinitialize train service
 */
void train_service_deinit(void);
```

## Configuration

**Kconfig options**:
```
CONFIG_TRAIN_LINE_NAME="中央線"
```

**Runtime configuration**:
```c
typedef struct {
    char line_name[32];     /**< Target line name to monitor */
} train_service_config_t;

esp_err_t train_service_set_config(const train_service_config_t *config);
```

## Error Handling

| Error | Response | Recovery |
|-------|----------|----------|
| Network timeout | ESP_ERR_TIMEOUT | Use cached data |
| SSL/TLS error | ESP_ERR_INVALID_STATE | Log error, use cache |
| Line not found | ESP_ERR_NOT_FOUND | Return UNKNOWN status |
| Parse error | ESP_ERR_INVALID_RESPONSE | Use cached data |
| Charset error | ESP_ERR_INVALID_RESPONSE | Use cached data |

## Cache Contract

**Storage**: NVS namespace "train"
**Key**: "status_cache"
**TTL**: 30 minutes
**Size**: ~256 bytes

```c
esp_err_t train_service_save_cache(const train_status_t *status);
esp_err_t train_service_load_cache(train_status_t *status);
bool train_service_cache_valid(const train_status_t *status);
```

## Status Mapping

| Page Text | train_status_code_t |
|-----------|---------------------|
| 平常運転 | TRAIN_STATUS_NORMAL |
| 遅延 | TRAIN_STATUS_DELAYED |
| 運転見合わせ | TRAIN_STATUS_SUSPENDED |
| 運休 | TRAIN_STATUS_SUSPENDED |
| (not found) | TRAIN_STATUS_UNKNOWN |
| (parse error) | TRAIN_STATUS_ERROR |

## Implementation Notes

### Character Encoding

JR東日本ページはShift_JISエンコード。ESP32での処理:

```c
/**
 * @brief Convert Shift_JIS to UTF-8
 * @param sjis Input Shift_JIS string
 * @param utf8 Output UTF-8 buffer
 * @param utf8_size Size of output buffer
 * @return ESP_OK on success
 */
esp_err_t train_service_sjis_to_utf8(const char *sjis, char *utf8, size_t utf8_size);
```

### Memory Management

HTMLページサイズが大きい（数十KB）ため:
- ストリーミングパースを推奨
- 全ページをメモリに読み込まない
- 対象路線発見後は即座に処理を終了
