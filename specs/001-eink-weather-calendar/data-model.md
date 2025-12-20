# Data Model: ESP32-S3 E-ink Weather Calendar

**Date**: 2025-12-20
**Feature**: 001-eink-weather-calendar

## Overview

This document defines the data structures and relationships for the Weather Calendar firmware. All structures are defined in C for ESP-IDF compatibility.

---

## Core Entities

### 1. Weather Forecast (Single Day)

Represents weather data for a single day's forecast.

```c
/**
 * @brief Single day weather forecast data
 */
typedef struct {
    time_t timestamp;           // Unix timestamp (noon of the day)
    float temp;                 // Current/representative temperature (°C)
    float temp_min;             // Minimum temperature (°C)
    float temp_max;             // Maximum temperature (°C)
    int humidity;               // Relative humidity (0-100%)
    char description[64];       // Weather description (e.g., "clear sky")
    char icon_code[4];          // OpenWeatherMap icon code (e.g., "01d")
    float wind_speed;           // Wind speed (m/s)
    int wind_deg;               // Wind direction (degrees, 0-360)
} weather_forecast_t;
```

**Validation Rules**:
- `temp`, `temp_min`, `temp_max`: Valid range -50°C to 60°C
- `humidity`: Valid range 0-100
- `icon_code`: 2-3 characters + null terminator
- `wind_deg`: Valid range 0-359
- `wind_speed`: Non-negative

**State Transitions**: N/A (immutable after parsing)

---

### 2. Weather Data Set

Aggregates 4 days of forecast data with metadata.

```c
/**
 * @brief Complete weather data set for display
 */
typedef struct {
    weather_forecast_t daily[4];  // 4-day forecast array
    char city_name[64];           // Location name from API
    time_t last_update;           // When data was fetched (Unix timestamp)
    bool valid;                   // Data validity flag
} weather_data_t;
```

**Validation Rules**:
- `daily`: All 4 entries must be populated for `valid=true`
- `last_update`: Must be non-zero for cached data
- `valid`: Set to `false` if parsing fails or data is stale (>24h)

**State Transitions**:
```
EMPTY -> FETCHING -> VALID -> STALE -> INVALID
         |                      |
         +-> FETCH_FAILED ------+
```

---

### 3. Display Framebuffer

Represents the full-screen pixel buffer for E-Paper display.

```c
/**
 * @brief 7-color E-Paper framebuffer
 *
 * Each pixel uses 4 bits (values 0-6 for 7 colors)
 * Two pixels packed per byte: [high nibble | low nibble]
 * Buffer size: 800 * 480 / 2 = 192,000 bytes
 */
typedef struct {
    uint8_t *buffer;              // Pixel buffer (PSRAM allocated)
    uint16_t width;               // Display width (800)
    uint16_t height;              // Display height (480)
    size_t buffer_size;           // Buffer size in bytes (192000)
} gfx_canvas_t;
```

**Color Palette**:
```c
typedef enum {
    EPD_COLOR_BLACK  = 0,
    EPD_COLOR_WHITE  = 1,
    EPD_COLOR_GREEN  = 2,
    EPD_COLOR_BLUE   = 3,
    EPD_COLOR_RED    = 4,
    EPD_COLOR_YELLOW = 5,
    EPD_COLOR_ORANGE = 6
} epd_color_t;
```

**Validation Rules**:
- `buffer`: Must be non-NULL after initialization
- `width`: Fixed at 800
- `height`: Fixed at 480
- Pixel values: Must be 0-6 (7 colors)

---

### 4. Application Configuration

Runtime configuration loaded from NVS/Kconfig.

```c
/**
 * @brief Application configuration
 */
typedef struct {
    char wifi_ssid[32];           // WiFi network name
    char wifi_password[64];       // WiFi password
    char api_key[48];             // OpenWeatherMap API key
    float latitude;               // Location latitude
    float longitude;              // Location longitude
    char timezone[32];            // POSIX timezone string (e.g., "JST-9")
} app_config_t;
```

**Validation Rules**:
- `wifi_ssid`: 1-31 characters
- `wifi_password`: 8-63 characters (WPA2)
- `api_key`: 32 characters (OpenWeatherMap format)
- `latitude`: -90.0 to 90.0
- `longitude`: -180.0 to 180.0

---

### 5. Application State

Current state of the application state machine.

```c
/**
 * @brief Application state machine
 */
typedef enum {
    STATE_INIT,             // Initial boot, NVS loading
    STATE_WIFI_CONNECT,     // Connecting to WiFi
    STATE_SYNC_TIME,        // SNTP time synchronization
    STATE_FETCH_WEATHER,    // Fetching weather data
    STATE_RENDER_DISPLAY,   // Drawing to framebuffer
    STATE_REFRESH_DISPLAY,  // Transferring to E-Paper
    STATE_DEEP_SLEEP,       // Entering deep sleep
    STATE_ERROR             // Error state (display error screen)
} app_state_t;

/**
 * @brief Application context
 */
typedef struct {
    app_state_t state;            // Current state
    app_config_t config;          // Configuration
    weather_data_t weather;       // Weather data
    gfx_canvas_t canvas;          // Display buffer
    time_t boot_time;             // Boot timestamp
    uint8_t error_code;           // Last error code
    uint8_t retry_count;          // Current retry count
} app_context_t;
```

**State Transition Rules**:
```
STATE_INIT
  -> STATE_WIFI_CONNECT (config loaded)
  -> STATE_ERROR (config invalid)

STATE_WIFI_CONNECT
  -> STATE_SYNC_TIME (connected)
  -> STATE_ERROR (3 retries failed)

STATE_SYNC_TIME
  -> STATE_FETCH_WEATHER (time synced or skip if <24h)
  -> STATE_ERROR (sync failed, no cached time)

STATE_FETCH_WEATHER
  -> STATE_RENDER_DISPLAY (data fetched or cache available)
  -> STATE_ERROR (no data available)

STATE_RENDER_DISPLAY
  -> STATE_REFRESH_DISPLAY (buffer ready)
  -> STATE_ERROR (render failed)

STATE_REFRESH_DISPLAY
  -> STATE_DEEP_SLEEP (refresh complete)
  -> STATE_ERROR (display timeout)

STATE_DEEP_SLEEP
  -> STATE_INIT (timer wake)

STATE_ERROR
  -> STATE_RENDER_DISPLAY (show error screen)
  -> STATE_DEEP_SLEEP (after error display)
```

---

### 6. RTC Memory Cache

Data preserved across deep sleep cycles.

```c
/**
 * @brief RTC memory structure (survives deep sleep)
 * Must be placed in RTC memory using RTC_DATA_ATTR
 */
typedef struct {
    uint32_t magic;               // Magic number for validity check
    time_t last_sync_time;        // Last SNTP sync timestamp
    time_t last_api_call;         // Last API call timestamp
    weather_data_t weather_cache; // Cached weather data
    uint8_t boot_count;           // Wakeup counter
} rtc_cache_t;

#define RTC_CACHE_MAGIC 0xCAFE0001
```

**Validation Rules**:
- `magic`: Must equal `RTC_CACHE_MAGIC` for valid cache
- `last_sync_time`: Used to skip SNTP if <24h old
- `last_api_call`: Used to throttle API calls (3h minimum)

---

## Entity Relationships

```
                    ┌─────────────────┐
                    │  app_context_t  │
                    └────────┬────────┘
                             │
         ┌───────────────────┼───────────────────┐
         │                   │                   │
         ▼                   ▼                   ▼
┌─────────────────┐ ┌─────────────────┐ ┌─────────────────┐
│  app_config_t   │ │ weather_data_t  │ │  gfx_canvas_t   │
└─────────────────┘ └────────┬────────┘ └─────────────────┘
                             │
                             │ contains 4x
                             ▼
                    ┌─────────────────┐
                    │weather_forecast_t│
                    └─────────────────┘

┌─────────────────┐
│  rtc_cache_t    │ ── (persisted copy of weather_data_t)
└─────────────────┘
```

---

## Storage Mapping

| Entity | Storage Location | Persistence |
|--------|-----------------|-------------|
| `app_config_t` | NVS (read-only after boot) | Permanent |
| `weather_data_t` | Internal RAM + NVS backup | Session + backup |
| `gfx_canvas_t.buffer` | PSRAM | Session only |
| `rtc_cache_t` | RTC Memory | Across deep sleep |

---

## Data Flow

```
1. Boot
   ┌──────────┐    ┌──────────┐    ┌──────────┐
   │   NVS    │───>│ config   │───>│ WiFi     │
   └──────────┘    └──────────┘    └──────────┘

2. Fetch Weather
   ┌──────────┐    ┌──────────┐    ┌──────────┐
   │ HTTP GET │───>│ JSON     │───>│ weather  │
   │ Response │    │ Parse    │    │ _data_t  │
   └──────────┘    └──────────┘    └──────────┘

3. Render
   ┌──────────┐    ┌──────────┐    ┌──────────┐
   │ weather  │───>│ UI       │───>│ canvas   │
   │ _data_t  │    │ Render   │    │ buffer   │
   └──────────┘    └──────────┘    └──────────┘

4. Display
   ┌──────────┐    ┌──────────┐    ┌──────────┐
   │ canvas   │───>│ SPI      │───>│ E-Paper  │
   │ buffer   │    │ Transfer │    │ Display  │
   └──────────┘    └──────────┘    └──────────┘

5. Sleep
   ┌──────────┐    ┌──────────┐
   │ weather  │───>│ RTC      │
   │ _data_t  │    │ cache    │
   └──────────┘    └──────────┘
```

---

## Size Estimates

| Structure | Size (bytes) | Notes |
|-----------|-------------|-------|
| `weather_forecast_t` | ~100 | Single day |
| `weather_data_t` | ~500 | 4 days + metadata |
| `gfx_canvas_t.buffer` | 192,000 | PSRAM |
| `app_config_t` | ~200 | Configuration |
| `rtc_cache_t` | ~600 | RTC memory limit ~8KB |
| JSON buffer | 32,000 | API response |
