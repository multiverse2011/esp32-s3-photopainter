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
#include "epd_driver.h"
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/** UI Layout Constants - 2-column layout (sidebar + content) */
#define UI_SCREEN_WIDTH      800
#define UI_SCREEN_HEIGHT     480
#define UI_SIDEBAR_WIDTH     200   /**< Left sidebar width (pixels) */
#define UI_CONTENT_WIDTH     600   /**< Right content width (pixels) */
#define UI_PADDING           10    /**< General padding (pixels) */
#define UI_WEATHER_COL_WIDTH 120   /**< Weather column width (5 columns in 600px) */
#define UI_WEATHER_ROW_HEIGHT 200  /**< Weather section height */
#define UI_HEADER_HEIGHT     80    /**< Legacy - for compatibility */
#define UI_COLUMN_WIDTH      200   /**< Legacy - for compatibility */
#define UI_ICON_SIZE         70    /**< Weather icon size (pixels) */

/** Weather-to-color mapping */
#define UI_COLOR_SUN_PRIMARY    EPD_COLOR_ORANGE  /**< Sunny weather - primary */
#define UI_COLOR_SUN_SECONDARY  EPD_COLOR_YELLOW  /**< Sunny weather - secondary */
#define UI_COLOR_RAIN           EPD_COLOR_BLUE    /**< Rain weather */
#define UI_COLOR_CLOUD_FILL     EPD_COLOR_WHITE   /**< Cloud fill */
#define UI_COLOR_CLOUD_OUTLINE  EPD_COLOR_BLACK   /**< Cloud outline */
#define UI_COLOR_SNOW           EPD_COLOR_BLUE    /**< Snow weather */

/** Temperature-to-color thresholds */
#define UI_TEMP_COLD_THRESHOLD  10   /**< Below this = cold (blue) */
#define UI_TEMP_WARM_THRESHOLD  25   /**< Above this = warm (orange/red) */

/** Train status colors */
#define UI_COLOR_TRAIN_NORMAL   EPD_COLOR_GREEN   /**< No delay */
#define UI_COLOR_TRAIN_DELAYED  EPD_COLOR_RED     /**< Delayed/suspended */

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

/**
 * @brief Draw complete display with all data
 *
 * New API for drawing with weather, tasks, and train data.
 * Include display_types.h before using this function.
 *
 * @param data Complete display data (weather + tasks + train)
 * @param current_time Current time
 * @param using_cache True if displaying cached data
 * @return ESP_OK on success
 */
esp_err_t calendar_ui_draw_full(const void *data, time_t current_time, bool using_cache);

#ifdef __cplusplus
}
#endif

#endif // CALENDAR_UI_H
