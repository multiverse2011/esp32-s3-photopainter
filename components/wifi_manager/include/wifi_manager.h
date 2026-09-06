/**
 * @file wifi_manager.h
 * @brief WiFi Manager component for ESP32-S3 Weather Calendar
 *
 * Handles WiFi connection, disconnection, and SNTP time synchronization.
 */

#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "esp_err.h"
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

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

/** Read the current AP RSSI, or return ESP_ERR_INVALID_STATE when offline. */
esp_err_t wifi_manager_get_rssi(int *rssi_dbm);

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

#ifdef __cplusplus
}
#endif

#endif // WIFI_MANAGER_H
