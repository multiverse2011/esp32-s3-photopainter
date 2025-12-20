# OpenWeatherMap API Contract

**Date**: 2025-12-20
**Feature**: 001-eink-weather-calendar

## Endpoint

**URL**: `https://api.openweathermap.org/data/2.5/forecast`

**Method**: GET

**Authentication**: API key via query parameter

## Request

### Query Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `lat` | float | Yes | Latitude (-90 to 90) |
| `lon` | float | Yes | Longitude (-180 to 180) |
| `appid` | string | Yes | API key (32 characters) |
| `units` | string | No | `metric` (Celsius) or `imperial` (Fahrenheit) |
| `cnt` | integer | No | Number of 3-hour intervals (1-40, default 40) |

### Example Request

```
GET /data/2.5/forecast?lat=35.6762&lon=139.6503&appid={API_KEY}&units=metric&cnt=32
Host: api.openweathermap.org
```

## Response

### Success (200 OK)

```json
{
  "cod": "200",
  "message": 0,
  "cnt": 32,
  "list": [
    {
      "dt": 1703145600,
      "main": {
        "temp": 12.5,
        "feels_like": 11.2,
        "temp_min": 10.5,
        "temp_max": 14.2,
        "pressure": 1015,
        "humidity": 65
      },
      "weather": [
        {
          "id": 800,
          "main": "Clear",
          "description": "clear sky",
          "icon": "01d"
        }
      ],
      "clouds": {
        "all": 0
      },
      "wind": {
        "speed": 3.5,
        "deg": 45
      },
      "visibility": 10000,
      "pop": 0,
      "sys": {
        "pod": "d"
      },
      "dt_txt": "2023-12-21 12:00:00"
    }
    // ... more entries
  ],
  "city": {
    "id": 1850147,
    "name": "Tokyo",
    "coord": {
      "lat": 35.6762,
      "lon": 139.6503
    },
    "country": "JP",
    "timezone": 32400,
    "sunrise": 1703108400,
    "sunset": 1703143200
  }
}
```

### Response Fields Used

| Field Path | Type | Description |
|------------|------|-------------|
| `list[].dt` | integer | Unix timestamp |
| `list[].main.temp` | float | Temperature (°C) |
| `list[].main.temp_min` | float | Min temperature (°C) |
| `list[].main.temp_max` | float | Max temperature (°C) |
| `list[].main.humidity` | integer | Humidity (%) |
| `list[].weather[0].description` | string | Description |
| `list[].weather[0].icon` | string | Icon code |
| `list[].wind.speed` | float | Wind speed (m/s) |
| `list[].wind.deg` | integer | Wind direction (°) |
| `city.name` | string | City name |

### Error Responses

| Status | Code | Description |
|--------|------|-------------|
| 401 | | Invalid API key |
| 404 | | City not found |
| 429 | | Rate limit exceeded |
| 5xx | | Server error |

```json
{
  "cod": "401",
  "message": "Invalid API key"
}
```

## Icon Codes

| Code | Day/Night | Description |
|------|-----------|-------------|
| 01d | Day | Clear sky |
| 01n | Night | Clear sky |
| 02d | Day | Few clouds |
| 02n | Night | Few clouds |
| 03d | Day | Scattered clouds |
| 03n | Night | Scattered clouds |
| 04d | Day | Broken clouds |
| 04n | Night | Broken clouds |
| 09d | Day | Shower rain |
| 09n | Night | Shower rain |
| 10d | Day | Rain |
| 10n | Night | Rain |
| 11d | Day | Thunderstorm |
| 11n | Night | Thunderstorm |
| 13d | Day | Snow |
| 13n | Night | Snow |
| 50d | Day | Mist |
| 50n | Night | Mist |

## Parsing Strategy

### Extract 4-Day Forecast

1. Get current date at start of day
2. For each of next 4 days:
   - Find entry closest to 12:00 local time
   - Extract required fields
3. If noon entry missing, use nearest available

### cJSON Parsing Example

```c
esp_err_t parse_weather_response(const char *json, weather_data_t *data) {
    cJSON *root = cJSON_Parse(json);
    if (!root) return ESP_ERR_INVALID_RESPONSE;

    cJSON *list = cJSON_GetObjectItem(root, "list");
    cJSON *city = cJSON_GetObjectItem(root, "city");

    if (!list || !city) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_RESPONSE;
    }

    // Extract city name
    cJSON *name = cJSON_GetObjectItem(city, "name");
    if (cJSON_IsString(name)) {
        strncpy(data->city_name, name->valuestring, sizeof(data->city_name)-1);
    }

    // Parse forecast entries
    int day_index = 0;
    time_t current_day = get_start_of_day(time(NULL));

    cJSON *entry;
    cJSON_ArrayForEach(entry, list) {
        cJSON *dt = cJSON_GetObjectItem(entry, "dt");
        if (!cJSON_IsNumber(dt)) continue;

        time_t timestamp = (time_t)dt->valuedouble;
        int hour = get_hour_of_day(timestamp);

        // Select noon entries (11:00-13:00)
        if (hour >= 11 && hour <= 13) {
            if (day_index >= 4) break;

            weather_forecast_t *forecast = &data->daily[day_index];
            forecast->timestamp = timestamp;

            cJSON *main = cJSON_GetObjectItem(entry, "main");
            if (main) {
                forecast->temp = cJSON_GetObjectItem(main, "temp")->valuedouble;
                forecast->temp_min = cJSON_GetObjectItem(main, "temp_min")->valuedouble;
                forecast->temp_max = cJSON_GetObjectItem(main, "temp_max")->valuedouble;
                forecast->humidity = cJSON_GetObjectItem(main, "humidity")->valueint;
            }

            cJSON *weather = cJSON_GetArrayItem(
                cJSON_GetObjectItem(entry, "weather"), 0);
            if (weather) {
                strncpy(forecast->description,
                    cJSON_GetObjectItem(weather, "description")->valuestring,
                    sizeof(forecast->description)-1);
                strncpy(forecast->icon_code,
                    cJSON_GetObjectItem(weather, "icon")->valuestring,
                    sizeof(forecast->icon_code)-1);
            }

            cJSON *wind = cJSON_GetObjectItem(entry, "wind");
            if (wind) {
                forecast->wind_speed = cJSON_GetObjectItem(wind, "speed")->valuedouble;
                forecast->wind_deg = cJSON_GetObjectItem(wind, "deg")->valueint;
            }

            day_index++;
        }
    }

    data->valid = (day_index == 4);
    data->last_update = time(NULL);

    cJSON_Delete(root);
    return ESP_OK;
}
```

## Rate Limiting

| Plan | Calls/day | Calls/minute |
|------|-----------|--------------|
| Free | 1,000 | 60 |
| Starter | 100,000 | 600 |

### Implementation Strategy

- Target: 8 calls/day (every 3 hours)
- Track last API call in RTC memory
- Skip API call if <3 hours since last call
- Use cached data when rate limited
