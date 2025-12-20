# Task Service Contract

**Service**: task_service
**Version**: 1.0.0
**Date**: 2025-12-20

## Overview

Todoist APIから今日のタスクを取得するサービス。

## API Contract

### External API (Todoist REST API v2)

**Endpoint**: `GET https://api.todoist.com/rest/v2/tasks`

**Headers**:
| Header | Value |
|--------|-------|
| Authorization | Bearer {API_TOKEN} |

**Query Parameters**:
| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| filter | string | No | "today" for today's tasks |

**Response** (200 OK):
```json
[
  {
    "id": "123456789",
    "content": "ゴミ出し",
    "description": "",
    "is_completed": false,
    "due": {
      "date": "2025-12-20",
      "string": "today"
    },
    "priority": 4
  }
]
```

**Error Response** (401 Unauthorized):
```json
{
  "error": "Unauthorized",
  "error_code": 401
}
```

## Internal API (C Functions)

### task_service_init

```c
/**
 * @brief Initialize task service
 * @return ESP_OK on success
 */
esp_err_t task_service_init(void);
```

### task_service_fetch

```c
/**
 * @brief Fetch today's tasks from Todoist
 * @param[out] tasks Output task list structure
 * @return ESP_OK on success
 * @return ESP_ERR_TIMEOUT on network timeout
 * @return ESP_ERR_INVALID_STATE on auth failure
 */
esp_err_t task_service_fetch(task_list_t *tasks);
```

**Behavior**:
1. HTTPSリクエストを送信（Bearer認証）
2. JSONレスポンスをパース
3. 最大5件のタスクを抽出
4. `task_list_t`構造体に格納

### task_service_deinit

```c
/**
 * @brief Deinitialize task service
 */
void task_service_deinit(void);
```

## Configuration

**Kconfig options**:
```
CONFIG_TODOIST_API_TOKEN="your-api-token-here"
```

**Runtime configuration**:
```c
typedef struct {
    char api_token[128];
} task_service_config_t;

esp_err_t task_service_set_config(const task_service_config_t *config);
```

## Error Handling

| Error | Response | Recovery |
|-------|----------|----------|
| Network timeout | ESP_ERR_TIMEOUT | Use cached data |
| Auth failure (401) | ESP_ERR_INVALID_STATE | Log error, show empty |
| Invalid JSON | ESP_ERR_INVALID_RESPONSE | Use cached data |
| No tasks found | ESP_OK | Return count=0 |

## Cache Contract

**Storage**: NVS namespace "tasks"
**Key**: "today_cache"
**TTL**: 24 hours (tasks refresh daily)
**Size**: ~512 bytes

```c
esp_err_t task_service_save_cache(const task_list_t *tasks);
esp_err_t task_service_load_cache(task_list_t *tasks);
bool task_service_cache_valid(const task_list_t *tasks);
```

## Data Mapping

| Todoist Field | task_t Field | Transformation |
|---------------|--------------|----------------|
| content | name | Truncate to 63 chars |
| is_completed | completed | Direct map |
