# Component API Contracts: ESP32-S3 E-ink Weather Calendar

**Date**: 2025-12-20
**Feature**: 001-eink-weather-calendar

## Overview

This document defines the public APIs for each ESP-IDF component. All functions follow ESP-IDF conventions with `esp_err_t` return codes.

---

## 1. WiFi Manager (`wifi_manager`)

### Header: `wifi_manager.h`

```c
#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "esp_err.h"
#include <stdbool.h>
#include <time.h>

/**
 * @brief Initialize WiFi manager
 *
 * Must be called once at startup. Initializes NVS, WiFi driver, and event handlers.
 *
 * @return ESP_OK on success
 * @return ESP_ERR_WIFI_* on WiFi initialization failure
 */
esp_err_t wifi_manager_init(void);

/**
 * @brief Connect to configured WiFi network
 *
 * Blocks until connected or timeout. Uses SSID/password from Kconfig.
 *
 * @param timeout_ms Maximum time to wait for connection (0 = default 30s)
 * @return ESP_OK on successful connection
 * @return ESP_ERR_TIMEOUT if connection times out
 * @return ESP_ERR_WIFI_* on WiFi errors
 */
esp_err_t wifi_manager_connect(uint32_t timeout_ms);

/**
 * @brief Disconnect from WiFi network
 *
 * Gracefully disconnects and stops WiFi driver to save power.
 *
 * @return ESP_OK on success
 */
esp_err_t wifi_manager_disconnect(void);

/**
 * @brief Check if currently connected to WiFi
 *
 * @return true if connected with valid IP
 * @return false if disconnected
 */
bool wifi_manager_is_connected(void);

/**
 * @brief Synchronize time via SNTP
 *
 * Connects to NTP servers and synchronizes system time.
 * Sets timezone from Kconfig.
 *
 * @param timeout_ms Maximum time to wait for sync (0 = default 15s)
 * @return ESP_OK on successful sync
 * @return ESP_ERR_TIMEOUT if sync times out
 */
esp_err_t wifi_manager_sync_time(uint32_t timeout_ms);

/**
 * @brief Get last successful time sync timestamp
 *
 * @return Unix timestamp of last sync, or 0 if never synced
 */
time_t wifi_manager_get_last_sync_time(void);

/**
 * @brief Deinitialize WiFi manager
 *
 * Releases all WiFi resources. Call before deep sleep.
 */
void wifi_manager_deinit(void);

#endif // WIFI_MANAGER_H
```

---

## 2. EPD Driver (`epd_driver`)

### Header: `epd_driver.h`

```c
#ifndef EPD_DRIVER_H
#define EPD_DRIVER_H

#include "esp_err.h"
#include <stdint.h>
#include <stddef.h>

/** Display dimensions */
#define EPD_WIDTH  800
#define EPD_HEIGHT 480

/** 7-color palette */
typedef enum {
    EPD_COLOR_BLACK  = 0,
    EPD_COLOR_WHITE  = 1,
    EPD_COLOR_GREEN  = 2,
    EPD_COLOR_BLUE   = 3,
    EPD_COLOR_RED    = 4,
    EPD_COLOR_YELLOW = 5,
    EPD_COLOR_ORANGE = 6
} epd_color_t;

/**
 * @brief Initialize E-Paper display driver
 *
 * Configures SPI, GPIO, and sends display initialization sequence.
 *
 * @return ESP_OK on success
 * @return ESP_ERR_NO_MEM if buffer allocation fails
 * @return ESP_ERR_INVALID_STATE if already initialized
 */
esp_err_t epd_driver_init(void);

/**
 * @brief Get pointer to framebuffer
 *
 * Returns the PSRAM-allocated framebuffer for direct pixel manipulation.
 * Buffer uses 4-bit per pixel packing (2 pixels per byte).
 *
 * @return Pointer to framebuffer, or NULL if not initialized
 */
uint8_t *epd_driver_get_buffer(void);

/**
 * @brief Get framebuffer size in bytes
 *
 * @return Buffer size (192000 bytes for 800x480 @ 4bpp)
 */
size_t epd_driver_get_buffer_size(void);

/**
 * @brief Clear framebuffer with specified color
 *
 * @param color Fill color (epd_color_t)
 * @return ESP_OK on success
 */
esp_err_t epd_driver_clear(epd_color_t color);

/**
 * @brief Transfer framebuffer to display and refresh
 *
 * Sends buffer via SPI and triggers full display refresh.
 * Blocks until refresh complete (15-20 seconds) or timeout.
 *
 * @return ESP_OK on success
 * @return ESP_ERR_TIMEOUT if BUSY signal times out (>20s)
 */
esp_err_t epd_driver_refresh(void);

/**
 * @brief Put display into sleep mode
 *
 * Reduces display power consumption. Call before deep sleep.
 *
 * @return ESP_OK on success
 */
esp_err_t epd_driver_sleep(void);

/**
 * @brief Hardware reset the display
 *
 * Toggles RST pin and reinitializes display.
 * Use for error recovery.
 *
 * @return ESP_OK on success
 */
esp_err_t epd_driver_reset(void);

/**
 * @brief Deinitialize EPD driver
 *
 * Releases SPI and GPIO resources. Frees framebuffer.
 */
void epd_driver_deinit(void);

#endif // EPD_DRIVER_H
```

---

## 3. Graphics Library (`gfx_library`)

### Header: `gfx_paint.h`

```c
#ifndef GFX_PAINT_H
#define GFX_PAINT_H

#include "epd_driver.h"
#include <stdint.h>
#include <stdbool.h>

/** Font size enumeration */
typedef enum {
    GFX_FONT_16 = 16,
    GFX_FONT_24 = 24,
    GFX_FONT_32 = 32
} gfx_font_size_t;

/** Text alignment */
typedef enum {
    GFX_ALIGN_LEFT,
    GFX_ALIGN_CENTER,
    GFX_ALIGN_RIGHT
} gfx_align_t;

/**
 * @brief Initialize graphics library
 *
 * Must be called after epd_driver_init(). Uses EPD framebuffer.
 *
 * @return ESP_OK on success
 * @return ESP_ERR_INVALID_STATE if EPD not initialized
 */
esp_err_t gfx_init(void);

/**
 * @brief Set a single pixel
 *
 * @param x X coordinate (0 to EPD_WIDTH-1)
 * @param y Y coordinate (0 to EPD_HEIGHT-1)
 * @param color Pixel color
 */
void gfx_set_pixel(uint16_t x, uint16_t y, epd_color_t color);

/**
 * @brief Draw a line (Bresenham algorithm)
 *
 * @param x0, y0 Start point
 * @param x1, y1 End point
 * @param color Line color
 */
void gfx_draw_line(uint16_t x0, uint16_t y0,
                   uint16_t x1, uint16_t y1,
                   epd_color_t color);

/**
 * @brief Draw rectangle outline
 *
 * @param x, y Top-left corner
 * @param w Width
 * @param h Height
 * @param color Outline color
 */
void gfx_draw_rect(uint16_t x, uint16_t y,
                   uint16_t w, uint16_t h,
                   epd_color_t color);

/**
 * @brief Draw filled rectangle
 *
 * @param x, y Top-left corner
 * @param w Width
 * @param h Height
 * @param color Fill color
 */
void gfx_fill_rect(uint16_t x, uint16_t y,
                   uint16_t w, uint16_t h,
                   epd_color_t color);

/**
 * @brief Draw circle outline (Midpoint algorithm)
 *
 * @param cx, cy Center point
 * @param radius Circle radius
 * @param color Outline color
 */
void gfx_draw_circle(uint16_t cx, uint16_t cy,
                     uint16_t radius,
                     epd_color_t color);

/**
 * @brief Draw filled circle
 *
 * @param cx, cy Center point
 * @param radius Circle radius
 * @param color Fill color
 */
void gfx_fill_circle(uint16_t cx, uint16_t cy,
                     uint16_t radius,
                     epd_color_t color);

/**
 * @brief Draw a single character
 *
 * @param x, y Top-left position
 * @param ch ASCII character (0x20-0x7E)
 * @param font Font size
 * @param fg_color Foreground color
 * @param bg_color Background color
 * @return Width of drawn character in pixels
 */
uint16_t gfx_draw_char(uint16_t x, uint16_t y,
                       char ch,
                       gfx_font_size_t font,
                       epd_color_t fg_color,
                       epd_color_t bg_color);

/**
 * @brief Draw a string
 *
 * @param x, y Position (interpretation depends on align)
 * @param str Null-terminated string
 * @param font Font size
 * @param fg_color Foreground color
 * @param bg_color Background color
 * @param align Text alignment relative to x position
 */
void gfx_draw_string(uint16_t x, uint16_t y,
                     const char *str,
                     gfx_font_size_t font,
                     epd_color_t fg_color,
                     epd_color_t bg_color,
                     gfx_align_t align);

/**
 * @brief Get width of string in pixels
 *
 * @param str Null-terminated string
 * @param font Font size
 * @return Width in pixels
 */
uint16_t gfx_get_string_width(const char *str, gfx_font_size_t font);

/**
 * @brief Format and draw time (HH:MM)
 *
 * @param x, y Position
 * @param time Unix timestamp
 * @param font Font size
 * @param color Text color
 */
void gfx_draw_time(uint16_t x, uint16_t y,
                   time_t time,
                   gfx_font_size_t font,
                   epd_color_t color);

/**
 * @brief Format and draw date (MM/DD or weekday)
 *
 * @param x, y Position
 * @param time Unix timestamp
 * @param font Font size
 * @param color Text color
 * @param show_weekday If true, show weekday name instead of date
 */
void gfx_draw_date(uint16_t x, uint16_t y,
                   time_t time,
                   gfx_font_size_t font,
                   epd_color_t color,
                   bool show_weekday);

#endif // GFX_PAINT_H
```

---

## 4. Weather Service (`weather_service`)

### Header: `weather_service.h`

```c
#ifndef WEATHER_SERVICE_H
#define WEATHER_SERVICE_H

#include "weather_types.h"
#include "esp_err.h"

/**
 * @brief Initialize weather service
 *
 * Loads API configuration from Kconfig.
 *
 * @return ESP_OK on success
 */
esp_err_t weather_service_init(void);

/**
 * @brief Fetch weather data from API
 *
 * Makes HTTPS request to OpenWeatherMap and parses response.
 * Requires WiFi to be connected.
 *
 * @param data Output: Parsed weather data
 * @return ESP_OK on success
 * @return ESP_ERR_HTTP_* on HTTP errors
 * @return ESP_ERR_INVALID_RESPONSE on parse errors
 * @return ESP_ERR_NO_MEM on memory allocation failure
 */
esp_err_t weather_service_fetch(weather_data_t *data);

/**
 * @brief Save weather data to NVS cache
 *
 * @param data Weather data to cache
 * @return ESP_OK on success
 * @return ESP_ERR_NVS_* on NVS errors
 */
esp_err_t weather_service_save_cache(const weather_data_t *data);

/**
 * @brief Load weather data from NVS cache
 *
 * @param data Output: Cached weather data
 * @return ESP_OK on success
 * @return ESP_ERR_NVS_NOT_FOUND if no cache exists
 * @return ESP_ERR_NVS_* on NVS errors
 */
esp_err_t weather_service_load_cache(weather_data_t *data);

/**
 * @brief Check if cache is still valid
 *
 * Cache is considered valid if less than 24 hours old.
 *
 * @param data Cached weather data
 * @return true if cache is valid
 * @return false if cache is stale or invalid
 */
bool weather_service_cache_valid(const weather_data_t *data);

/**
 * @brief Deinitialize weather service
 */
void weather_service_deinit(void);

#endif // WEATHER_SERVICE_H
```

### Header: `weather_types.h`

```c
#ifndef WEATHER_TYPES_H
#define WEATHER_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

/**
 * @brief Single day weather forecast
 */
typedef struct {
    time_t timestamp;           /**< Forecast time (noon) */
    float temp;                 /**< Representative temperature (°C) */
    float temp_min;             /**< Minimum temperature (°C) */
    float temp_max;             /**< Maximum temperature (°C) */
    int humidity;               /**< Relative humidity (%) */
    char description[64];       /**< Weather description */
    char icon_code[4];          /**< OpenWeatherMap icon code */
    float wind_speed;           /**< Wind speed (m/s) */
    int wind_deg;               /**< Wind direction (degrees) */
} weather_forecast_t;

/**
 * @brief Complete weather data set
 */
typedef struct {
    weather_forecast_t daily[4]; /**< 4-day forecast */
    char city_name[64];          /**< Location name */
    time_t last_update;          /**< Fetch timestamp */
    bool valid;                  /**< Data validity flag */
} weather_data_t;

#endif // WEATHER_TYPES_H
```

---

## 5. Calendar UI (`calendar_ui`)

### Header: `calendar_ui.h`

```c
#ifndef CALENDAR_UI_H
#define CALENDAR_UI_H

#include "weather_types.h"
#include "esp_err.h"
#include <time.h>

/** UI Layout Constants */
#define UI_HEADER_HEIGHT     80
#define UI_COLUMN_WIDTH      200
#define UI_ICON_SIZE         120
#define UI_PADDING           10

/**
 * @brief Initialize calendar UI
 *
 * Must be called after gfx_init().
 *
 * @return ESP_OK on success
 */
esp_err_t calendar_ui_init(void);

/**
 * @brief Draw complete calendar screen
 *
 * Draws header (date/time) and 4-day forecast columns.
 * Clears buffer before drawing.
 *
 * @param weather Weather data to display
 * @param current_time Current time for header
 * @return ESP_OK on success
 */
esp_err_t calendar_ui_draw(const weather_data_t *weather, time_t current_time);

/**
 * @brief Draw error screen
 *
 * Shows error message with last update time if available.
 *
 * @param error_message Error description
 * @param last_update Last successful update time (0 if none)
 * @return ESP_OK on success
 */
esp_err_t calendar_ui_draw_error(const char *error_message, time_t last_update);

/**
 * @brief Draw header section only
 *
 * @param current_time Current time
 */
void calendar_ui_draw_header(time_t current_time);

/**
 * @brief Draw single forecast column
 *
 * @param column Column index (0-3)
 * @param forecast Forecast data for this day
 */
void calendar_ui_draw_forecast(int column, const weather_forecast_t *forecast);

/**
 * @brief Draw weather icon
 *
 * @param x, y Center position
 * @param size Icon size in pixels
 * @param icon_code OpenWeatherMap icon code (e.g., "01d")
 */
void calendar_ui_draw_weather_icon(uint16_t x, uint16_t y,
                                   uint16_t size,
                                   const char *icon_code);

#endif // CALENDAR_UI_H
```

---

## Error Codes

Custom error codes (starting at 0x10000 to avoid ESP-IDF conflicts):

```c
#define ESP_ERR_WEATHER_BASE         0x10000
#define ESP_ERR_WEATHER_HTTP_FAILED  (ESP_ERR_WEATHER_BASE + 1)
#define ESP_ERR_WEATHER_PARSE_FAILED (ESP_ERR_WEATHER_BASE + 2)
#define ESP_ERR_WEATHER_NO_DATA      (ESP_ERR_WEATHER_BASE + 3)

#define ESP_ERR_EPD_BASE             0x10100
#define ESP_ERR_EPD_BUSY_TIMEOUT     (ESP_ERR_EPD_BASE + 1)
#define ESP_ERR_EPD_SPI_FAILED       (ESP_ERR_EPD_BASE + 2)
```

---

## Usage Example

```c
void app_main(void) {
    // Initialize
    wifi_manager_init();
    epd_driver_init();
    gfx_init();
    calendar_ui_init();
    weather_service_init();

    // Connect and fetch
    wifi_manager_connect(30000);
    wifi_manager_sync_time(15000);

    weather_data_t weather;
    if (weather_service_fetch(&weather) == ESP_OK) {
        weather_service_save_cache(&weather);
    } else {
        weather_service_load_cache(&weather);
    }

    // Render and display
    calendar_ui_draw(&weather, time(NULL));
    epd_driver_refresh();

    // Cleanup and sleep
    epd_driver_sleep();
    wifi_manager_disconnect();
    epd_driver_deinit();
    wifi_manager_deinit();

    esp_deep_sleep(30 * 60 * 1000000ULL);  // 30 minutes
}
```
