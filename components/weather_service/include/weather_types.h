/**
 * @file weather_types.h
 * @brief Weather data structures for ESP32-S3 Weather Calendar
 */

#ifndef WEATHER_TYPES_H
#define WEATHER_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Single day weather forecast
 */
typedef struct {
    time_t timestamp;           /**< Forecast time (noon) */
    float temp;                 /**< Representative temperature (C) */
    float temp_min;             /**< Minimum temperature (C) */
    float temp_max;             /**< Maximum temperature (C) */
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

#ifdef __cplusplus
}
#endif

#endif // WEATHER_TYPES_H
