# Data Model: E-ink Display Design Improvement

**Feature**: 002-improve-display-design
**Date**: 2025-12-20

## Entity Overview

```
+------------------+       +------------------+       +------------------+
|   display_data_t |------>| weather_data_t   |       |   ui_layout_t    |
|                  |       |                  |       |                  |
| - weather        |       | - hourly[5]      |       | - sidebar_width  |
| - tasks          |       | - city_name      |       | - content_width  |
| - train          |       | - last_update    |       | - padding        |
| - update_time    |       | - valid          |       | - colors         |
+------------------+       +------------------+       +------------------+
        |                          |
        v                          v
+------------------+       +------------------+
|   task_list_t    |       | weather_forecast_t|
|                  |       |                  |
| - tasks[MAX]     |       | - timestamp      |
| - count          |       | - temp           |
| - last_update    |       | - humidity       |
| - valid          |       | - icon_code      |
+------------------+       | - wind_speed     |
        |                  | - wind_deg       |
        v                  +------------------+
+------------------+
|     task_t       |
|                  |
| - name[64]       |
| - completed      |
+------------------+

+------------------+
| train_status_t   |
|                  |
| - line_name[32]  |
| - status         |
| - delay_minutes  |
| - message[128]   |
| - last_update    |
| - valid          |
+------------------+
```

## Entity Definitions

### 1. weather_forecast_t (Modified)

**変更点**: 4日間日別 → 5時間帯

```c
/**
 * @brief Single time slot weather forecast (3-hour interval)
 */
typedef struct {
    time_t timestamp;           /**< Forecast time */
    float temp;                 /**< Temperature (C) */
    int humidity;               /**< Relative humidity (%) */
    char description[64];       /**< Weather description */
    char icon_code[4];          /**< OpenWeatherMap icon code */
    float wind_speed;           /**< Wind speed (m/s) */
    int wind_deg;               /**< Wind direction (degrees) */
} weather_forecast_t;
```

**Validation Rules**:
- `timestamp`: 過去24時間以内〜未来5日以内
- `temp`: -50.0 〜 60.0 の範囲
- `humidity`: 0 〜 100 の範囲
- `icon_code`: 2-3文字の英数字（例: "01d", "10n"）

### 2. weather_data_t (Modified)

**変更点**: `daily[4]` → `hourly[5]`

```c
/**
 * @brief Complete weather data set (5 time slots)
 */
typedef struct {
    weather_forecast_t hourly[5]; /**< 5 time slots (3-hour intervals) */
    char city_name[64];           /**< Location name */
    time_t last_update;           /**< Fetch timestamp */
    time_t base_time;             /**< Base time for hourly slots */
    bool valid;                   /**< Data validity flag */
} weather_data_t;
```

**State Transitions**:
```
[INVALID] ---(fetch success)---> [VALID]
[VALID] ---(cache expired)---> [STALE]
[STALE] ---(fetch success)---> [VALID]
[STALE] ---(fetch fail)---> [STALE] (continue using)
```

### 3. task_t (New)

```c
/**
 * @brief Single task item
 */
typedef struct {
    char name[64];              /**< Task name/description */
    bool completed;             /**< Completion status */
} task_t;
```

### 4. task_list_t (New)

```c
#define MAX_TASKS 5

/**
 * @brief Task list data
 */
typedef struct {
    task_t tasks[MAX_TASKS];    /**< Task array */
    int count;                  /**< Number of tasks (0-5) */
    time_t last_update;         /**< Fetch timestamp */
    bool valid;                 /**< Data validity flag */
} task_list_t;
```

**Validation Rules**:
- `count`: 0 〜 MAX_TASKS
- `name`: 空文字列不可、最大63文字

### 5. train_status_t (New)

```c
/**
 * @brief Train delay status
 */
typedef enum {
    TRAIN_STATUS_UNKNOWN = 0,   /**< Unknown/fetching */
    TRAIN_STATUS_NORMAL,        /**< No delay */
    TRAIN_STATUS_DELAYED,       /**< Delayed */
    TRAIN_STATUS_SUSPENDED,     /**< Service suspended */
    TRAIN_STATUS_ERROR          /**< Fetch error */
} train_status_code_t;

/**
 * @brief Train line status data
 */
typedef struct {
    char line_name[32];         /**< Line name (e.g., "中央線") */
    train_status_code_t status; /**< Status code */
    int delay_minutes;          /**< Delay in minutes (0 if normal) */
    char message[128];          /**< Status message */
    time_t last_update;         /**< Fetch timestamp */
    bool valid;                 /**< Data validity flag */
} train_status_t;
```

**State Transitions**:
```
[UNKNOWN] ---(fetch success)---> [NORMAL|DELAYED|SUSPENDED]
[NORMAL] ---(fetch success)---> [NORMAL|DELAYED|SUSPENDED]
[*] ---(fetch fail)---> [ERROR] (show cached if available)
```

### 6. display_data_t (New - Aggregate)

```c
/**
 * @brief Aggregate data for display rendering
 */
typedef struct {
    weather_data_t weather;     /**< Weather data */
    task_list_t tasks;          /**< Task list */
    train_status_t train;       /**< Train status */
    time_t update_time;         /**< Display update timestamp */
} display_data_t;
```

### 7. ui_layout_t (New - Configuration)

```c
/**
 * @brief UI Layout configuration
 */
typedef struct {
    uint16_t sidebar_width;     /**< Left sidebar width (pixels) */
    uint16_t content_width;     /**< Right content width (pixels) */
    uint16_t padding;           /**< General padding (pixels) */
    uint16_t weather_col_width; /**< Weather column width (pixels) */
    uint16_t weather_row_height;/**< Weather section height (pixels) */
} ui_layout_t;

/**
 * @brief Default layout constants
 */
#define UI_SIDEBAR_WIDTH        200
#define UI_CONTENT_WIDTH        600
#define UI_PADDING              10
#define UI_WEATHER_COL_WIDTH    120
#define UI_WEATHER_ROW_HEIGHT   200
```

## Relationships

| From | To | Relationship | Cardinality |
|------|----|--------------|-------------|
| display_data_t | weather_data_t | contains | 1:1 |
| display_data_t | task_list_t | contains | 1:1 |
| display_data_t | train_status_t | contains | 1:1 |
| weather_data_t | weather_forecast_t | contains | 1:5 |
| task_list_t | task_t | contains | 1:0..5 |

## Data Flow

```
+----------------+     +----------------+     +----------------+
| OpenWeatherMap |---->| weather_service|---->|                |
+----------------+     +----------------+     |                |
                                              |  display_data_t|
+----------------+     +----------------+     |                |
| Todoist API    |---->| task_service   |---->|                |
+----------------+     +----------------+     |                |
                                              |                |
+----------------+     +----------------+     |                |
| JR East Page   |---->| train_service  |---->|                |
+----------------+     +----------------+     +-------+--------+
                                                      |
                                                      v
                                              +----------------+
                                              | calendar_ui    |
                                              | (rendering)    |
                                              +----------------+
                                                      |
                                                      v
                                              +----------------+
                                              | E-Paper Display|
                                              +----------------+
```

## Cache Strategy

| Entity | Storage | TTL | Fallback |
|--------|---------|-----|----------|
| weather_data_t | NVS + RTC | 3時間 | 古いキャッシュを表示 |
| task_list_t | NVS | 24時間 | 空リスト表示 |
| train_status_t | NVS | 30分 | UNKNOWN表示 |
