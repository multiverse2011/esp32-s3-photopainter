/**
 * @file weather_service.h
 * @brief Weather Service API for ESP32-S3 Weather Calendar
 *
 * Provides weather data fetching from OpenWeatherMap API.
 */

#ifndef WEATHER_SERVICE_H
#define WEATHER_SERVICE_H

#include "weather_types.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

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

/**
 * @brief Calculate forecast time slots based on current time
 *
 * Generates 5 timestamps at 3-hour intervals starting from current time.
 *
 * @param base_time Current time (base for calculations)
 * @param times Output: Array of 5 timestamps
 */
void weather_service_get_forecast_times(time_t base_time, time_t *times);

#ifdef __cplusplus
}
#endif

#endif // WEATHER_SERVICE_H
