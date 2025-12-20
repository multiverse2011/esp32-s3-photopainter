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
 * @brief Single time slot weather forecast (3-hour interval)
 */
typedef struct {
    time_t timestamp;           /**< Forecast time */
    float temp;                 /**< Temperature (C) */
    float temp_min;             /**< Minimum temperature (C) - for compatibility */
    float temp_max;             /**< Maximum temperature (C) - for compatibility */
    int humidity;               /**< Relative humidity (%) */
    char description[64];       /**< Weather description */
    char icon_code[4];          /**< OpenWeatherMap icon code */
    float wind_speed;           /**< Wind speed (m/s) */
    int wind_deg;               /**< Wind direction (degrees) */
} weather_forecast_t;

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

#ifdef __cplusplus
}
#endif

#endif // WEATHER_TYPES_H
