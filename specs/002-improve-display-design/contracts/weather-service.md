# Weather Service Contract

**Service**: weather_service
**Version**: 2.0.0
**Date**: 2025-12-20

## Overview

OpenWeatherMap 3-hour forecast APIから5時間帯の天気予報を取得するサービス。

## API Contract

### External API (OpenWeatherMap)

**Endpoint**: `GET https://api.openweathermap.org/data/2.5/forecast`

**Query Parameters**:
| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| lat | float | Yes | Latitude |
| lon | float | Yes | Longitude |
| appid | string | Yes | API Key |
| units | string | Yes | "metric" for Celsius |
| cnt | int | No | Number of timestamps (default: 40) |

**Response** (200 OK):
```json
{
  "cod": "200",
  "city": {
    "name": "Tokyo"
  },
  "list": [
    {
      "dt": 1702281600,
      "main": {
        "temp": 5.0,
        "humidity": 95
      },
      "weather": [
        {
          "icon": "01d",
          "description": "clear sky"
        }
      ],
      "wind": {
        "speed": 2.0,
        "deg": 315
      }
    }
  ]
}
```

## Internal API (C Functions)

### weather_service_init

```c
/**
 * @brief Initialize weather service
 * @return ESP_OK on success
 */
esp_err_t weather_service_init(void);
```

### weather_service_fetch_hourly

```c
/**
 * @brief Fetch 5 hourly forecasts from current time
 * @param[out] weather Output weather data structure
 * @return ESP_OK on success
 * @return ESP_ERR_TIMEOUT on network timeout
 * @return ESP_ERR_INVALID_RESPONSE on parse error
 */
esp_err_t weather_service_fetch_hourly(weather_data_t *weather);
```

**Behavior**:
1. HTTPSリクエストを送信
2. JSONレスポンスをパース
3. 現在時刻に最も近い5つの3時間予報を選択
4. `weather_data_t`構造体に格納

### weather_service_get_forecast_times

```c
/**
 * @brief Calculate 5 forecast timestamps based on base time
 * @param base_time Base time (usually current time)
 * @param[out] times Array of 5 timestamps
 */
void weather_service_get_forecast_times(time_t base_time, time_t times[5]);
```

**Calculation**:
- times[0] = base_time rounded to nearest 3-hour slot
- times[n] = times[0] + (n * 3 * 3600)

## Error Handling

| Error | Response | Recovery |
|-------|----------|----------|
| Network timeout | ESP_ERR_TIMEOUT | Use cached data |
| Invalid JSON | ESP_ERR_INVALID_RESPONSE | Use cached data |
| API rate limit | ESP_ERR_INVALID_RESPONSE | Wait, use cache |
| No valid forecasts | ESP_ERR_NOT_FOUND | Use cached data |

## Cache Contract

**Storage**: NVS namespace "weather"
**Key**: "hourly_cache"
**TTL**: 3 hours
**Size**: ~1KB

```c
esp_err_t weather_service_save_cache(const weather_data_t *weather);
esp_err_t weather_service_load_cache(weather_data_t *weather);
bool weather_service_cache_valid(const weather_data_t *weather);
```
