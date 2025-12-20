/**
 * @file task_service.h
 * @brief Task Service API for ESP32-S3 Weather Calendar
 *
 * Provides task data fetching from Todoist API.
 */

#ifndef TASK_SERVICE_H
#define TASK_SERVICE_H

#include "task_types.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize task service
 *
 * Loads API configuration from Kconfig.
 *
 * @return ESP_OK on success
 */
esp_err_t task_service_init(void);

/**
 * @brief Fetch tasks from Todoist API
 *
 * Makes HTTPS request to Todoist and parses response.
 * Requires WiFi to be connected.
 *
 * @param tasks Output: Parsed task list
 * @return ESP_OK on success
 * @return ESP_ERR_HTTP_* on HTTP errors
 * @return ESP_ERR_INVALID_RESPONSE on parse errors
 */
esp_err_t task_service_fetch(task_list_t *tasks);

/**
 * @brief Save task data to NVS cache
 *
 * @param tasks Task data to cache
 * @return ESP_OK on success
 */
esp_err_t task_service_save_cache(const task_list_t *tasks);

/**
 * @brief Load task data from NVS cache
 *
 * @param tasks Output: Cached task data
 * @return ESP_OK on success
 * @return ESP_ERR_NVS_NOT_FOUND if no cache exists
 */
esp_err_t task_service_load_cache(task_list_t *tasks);

/**
 * @brief Deinitialize task service
 */
void task_service_deinit(void);

#ifdef __cplusplus
}
#endif

#endif // TASK_SERVICE_H
