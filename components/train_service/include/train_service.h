/**
 * @file train_service.h
 * @brief Train Service API for ESP32-S3 Weather Calendar
 *
 * Provides train delay information from JR East website.
 */

#ifndef TRAIN_SERVICE_H
#define TRAIN_SERVICE_H

#include "train_types.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize train service
 *
 * Loads configuration from Kconfig.
 *
 * @return ESP_OK on success
 */
esp_err_t train_service_init(void);

/**
 * @brief Fetch train status from JR East website
 *
 * Makes HTTPS request and parses HTML response.
 * Requires WiFi to be connected.
 *
 * @param status Output: Parsed train status
 * @return ESP_OK on success
 * @return ESP_ERR_HTTP_* on HTTP errors
 * @return ESP_ERR_INVALID_RESPONSE on parse errors
 */
esp_err_t train_service_fetch(train_status_t *status);

/**
 * @brief Save train status to NVS cache
 *
 * @param status Train status to cache
 * @return ESP_OK on success
 */
esp_err_t train_service_save_cache(const train_status_t *status);

/**
 * @brief Load train status from NVS cache
 *
 * @param status Output: Cached train status
 * @return ESP_OK on success
 * @return ESP_ERR_NVS_NOT_FOUND if no cache exists
 */
esp_err_t train_service_load_cache(train_status_t *status);

/**
 * @brief Deinitialize train service
 */
void train_service_deinit(void);

#ifdef __cplusplus
}
#endif

#endif // TRAIN_SERVICE_H
