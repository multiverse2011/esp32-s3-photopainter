/**
 * @file calendar_ui.h
 * @brief Calendar UI API for ESP32-S3 Weather Calendar
 *
 * Provides UI rendering for weather calendar display.
 */

#ifndef CALENDAR_UI_H
#define CALENDAR_UI_H

#include "weather_types.h"
#include "esp_err.h"
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/** UI Layout Constants */
#define UI_HEADER_HEIGHT     80
#define UI_COLUMN_WIDTH      200
#define UI_ICON_SIZE         80
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
 * @brief Draw complete calendar screen with cache indicator
 *
 * Same as calendar_ui_draw() but shows cache staleness indicator.
 *
 * @param weather Weather data to display
 * @param current_time Current time for header
 * @param using_cache True if displaying cached data
 * @return ESP_OK on success
 */
esp_err_t calendar_ui_draw_ex(const weather_data_t *weather, time_t current_time, bool using_cache);

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

#ifdef __cplusplus
}
#endif

#endif // CALENDAR_UI_H
