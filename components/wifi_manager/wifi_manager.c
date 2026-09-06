/**
 * @file wifi_manager.c
 * @brief WiFi Manager implementation for ESP32-S3 Weather Calendar
 */

#include "wifi_manager.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_sntp.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include <string.h>

static const char *TAG = "wifi_manager";

// Default timeouts
#define DEFAULT_CONNECT_TIMEOUT_MS  30000
#define DEFAULT_SNTP_TIMEOUT_MS     15000
#define WIFI_MAXIMUM_RETRY          3

// Event bits
#define WIFI_CONNECTED_BIT          BIT0
#define WIFI_FAIL_BIT               BIT1
#define SNTP_SYNC_BIT               BIT2

// State variables
static EventGroupHandle_t s_wifi_event_group = NULL;
static esp_netif_t *s_sta_netif = NULL;
static int s_retry_num = 0;
static bool s_is_connected = false;
static time_t s_last_sync_time = 0;
static bool s_initialized = false;

// Forward declarations
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data);
static void ip_event_handler(void *arg, esp_event_base_t event_base,
                             int32_t event_id, void *event_data);
static void sntp_sync_callback(struct timeval *tv);

/**
 * @brief WiFi event handler
 */
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
        s_is_connected = false;
        if (s_retry_num < WIFI_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "Retrying WiFi connection... attempt %d/%d",
                     s_retry_num, WIFI_MAXIMUM_RETRY);
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            ESP_LOGW(TAG, "WiFi connection failed after %d attempts", WIFI_MAXIMUM_RETRY);
        }
    }
}

/**
 * @brief IP event handler
 */
static void ip_event_handler(void *arg, esp_event_base_t event_base,
                             int32_t event_id, void *event_data)
{
    if (event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        s_is_connected = true;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

/**
 * @brief SNTP sync notification callback
 */
static void sntp_sync_callback(struct timeval *tv)
{
    ESP_LOGI(TAG, "SNTP time synchronized");
    s_last_sync_time = tv->tv_sec;
    if (s_wifi_event_group) {
        xEventGroupSetBits(s_wifi_event_group, SNTP_SYNC_BIT);
    }
}

esp_err_t wifi_manager_init(void)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "WiFi manager already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing WiFi manager");

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "NVS initialization failed: %s; refusing implicit erase", esp_err_to_name(ret));
        return ret;
    }

    // Create event group
    s_wifi_event_group = xEventGroupCreate();
    if (s_wifi_event_group == NULL) {
        ESP_LOGE(TAG, "Failed to create event group");
        return ESP_ERR_NO_MEM;
    }

    // Initialize TCP/IP stack
    ESP_ERROR_CHECK(esp_netif_init());

    // Create default event loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Create default WiFi station
    s_sta_netif = esp_netif_create_default_wifi_sta();
    if (s_sta_netif == NULL) {
        ESP_LOGE(TAG, "Failed to create WiFi STA netif");
        return ESP_FAIL;
    }

    // Initialize WiFi with default config
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Register event handlers
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &ip_event_handler, NULL, NULL));

    // Configure WiFi
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_WIFI_SSID,
            .password = CONFIG_WIFI_PASSWORD,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

    s_initialized = true;
    ESP_LOGI(TAG, "WiFi manager initialized");

    return ESP_OK;
}

esp_err_t wifi_manager_connect(uint32_t timeout_ms)
{
    if (!s_initialized) {
        ESP_LOGE(TAG, "WiFi manager not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (s_is_connected) {
        ESP_LOGI(TAG, "Already connected");
        return ESP_OK;
    }

    uint32_t actual_timeout = timeout_ms > 0 ? timeout_ms : DEFAULT_CONNECT_TIMEOUT_MS;

    ESP_LOGI(TAG, "Connecting to WiFi SSID: %s", CONFIG_WIFI_SSID);

    // Clear event bits
    xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);
    s_retry_num = 0;

    // Start WiFi
    ESP_ERROR_CHECK(esp_wifi_start());

    // Wait for connection
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        pdFALSE, pdFALSE,
        pdMS_TO_TICKS(actual_timeout));

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "WiFi connected successfully");
        return ESP_OK;
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "WiFi connection failed");
        return ESP_ERR_WIFI_NOT_CONNECT;
    } else {
        ESP_LOGE(TAG, "WiFi connection timeout");
        return ESP_ERR_TIMEOUT;
    }
}

esp_err_t wifi_manager_disconnect(void)
{
    if (!s_initialized) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Disconnecting from WiFi");

    esp_wifi_disconnect();
    esp_wifi_stop();
    s_is_connected = false;

    return ESP_OK;
}

bool wifi_manager_is_connected(void)
{
    return s_is_connected;
}

esp_err_t wifi_manager_get_rssi(int *rssi_dbm)
{
    if (rssi_dbm == NULL || !s_is_connected) {
        return ESP_ERR_INVALID_STATE;
    }
    wifi_ap_record_t record;
    esp_err_t ret = esp_wifi_sta_get_ap_info(&record);
    if (ret == ESP_OK) {
        *rssi_dbm = record.rssi;
    }
    return ret;
}

esp_err_t wifi_manager_sync_time(uint32_t timeout_ms)
{
    if (!s_is_connected) {
        ESP_LOGE(TAG, "Cannot sync time: WiFi not connected");
        return ESP_ERR_INVALID_STATE;
    }

    uint32_t actual_timeout = timeout_ms > 0 ? timeout_ms : DEFAULT_SNTP_TIMEOUT_MS;

    ESP_LOGI(TAG, "Initializing SNTP time synchronization");

    // Set timezone from config
    setenv("TZ", CONFIG_TIMEZONE, 1);
    tzset();

    // Configure SNTP
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_setservername(1, "time.nist.gov");
    esp_sntp_set_time_sync_notification_cb(sntp_sync_callback);
    esp_sntp_init();

    // Clear sync bit
    xEventGroupClearBits(s_wifi_event_group, SNTP_SYNC_BIT);

    // Wait for sync
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
        SNTP_SYNC_BIT,
        pdFALSE, pdFALSE,
        pdMS_TO_TICKS(actual_timeout));

    if (bits & SNTP_SYNC_BIT) {
        // Log current time
        time_t now;
        struct tm timeinfo;
        time(&now);
        localtime_r(&now, &timeinfo);
        ESP_LOGI(TAG, "Current time: %04d-%02d-%02d %02d:%02d:%02d",
                 timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                 timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
        return ESP_OK;
    } else {
        ESP_LOGW(TAG, "SNTP sync timeout");
        return ESP_ERR_TIMEOUT;
    }
}

time_t wifi_manager_get_last_sync_time(void)
{
    return s_last_sync_time;
}

void wifi_manager_deinit(void)
{
    if (!s_initialized) {
        return;
    }

    ESP_LOGI(TAG, "Deinitializing WiFi manager");

    // Stop SNTP
    if (esp_sntp_enabled()) {
        esp_sntp_stop();
    }

    // Stop WiFi
    wifi_manager_disconnect();
    esp_wifi_deinit();

    // Destroy netif
    if (s_sta_netif) {
        esp_netif_destroy(s_sta_netif);
        s_sta_netif = NULL;
    }

    // Delete event group
    if (s_wifi_event_group) {
        vEventGroupDelete(s_wifi_event_group);
        s_wifi_event_group = NULL;
    }

    s_initialized = false;
    ESP_LOGI(TAG, "WiFi manager deinitialized");
}
