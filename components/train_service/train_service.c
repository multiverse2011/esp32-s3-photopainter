/**
 * @file train_service.c
 * @brief Train Service implementation for ESP32-S3 Weather Calendar
 */

#include "train_service.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_tls.h"
#include "esp_crt_bundle.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "train_service";

// JR East configuration
#define JR_EAST_HOST "traininfo.jreast.co.jp"
#define JR_EAST_PATH "/train_info/kanto.aspx"
#define HTTP_BUFFER_SIZE    65536  // 64KB for HTML response

// NVS configuration
#define NVS_NAMESPACE       "train"
#define NVS_KEY_CACHE       "cache"

// External parser function
extern esp_err_t train_parse_html(const char *html, size_t len, const char *line_name, train_status_t *status);

// Shift_JIS to UTF-8 conversion
extern esp_err_t sjis_to_utf8(const char *sjis, size_t sjis_len, char *utf8, size_t utf8_size);

static char *s_http_buffer = NULL;
static size_t s_http_buffer_len = 0;
static bool s_initialized = false;

/**
 * @brief HTTP event handler
 */
static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            if (s_http_buffer && (s_http_buffer_len + evt->data_len < HTTP_BUFFER_SIZE)) {
                memcpy(s_http_buffer + s_http_buffer_len, evt->data, evt->data_len);
                s_http_buffer_len += evt->data_len;
            }
            break;
        default:
            break;
    }
    return ESP_OK;
}

esp_err_t train_service_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing train service");

    // Check if line name is configured
    const char *line_name = CONFIG_TRAIN_LINE_NAME;
    if (line_name == NULL || strlen(line_name) == 0) {
        ESP_LOGW(TAG, "Train line name not configured");
    }

    // Allocate HTTP buffer in PSRAM
    s_http_buffer = heap_caps_malloc(HTTP_BUFFER_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (s_http_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate HTTP buffer");
        return ESP_ERR_NO_MEM;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "Train service initialized");
    return ESP_OK;
}

esp_err_t train_service_fetch(train_status_t *status)
{
    if (!s_initialized) {
        ESP_LOGE(TAG, "Train service not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (status == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // Check if line name is configured
    const char *line_name = CONFIG_TRAIN_LINE_NAME;
    if (line_name == NULL || strlen(line_name) == 0) {
        ESP_LOGW(TAG, "Train line name not configured, returning unknown status");
        memset(status, 0, sizeof(train_status_t));
        status->status = TRAIN_STATUS_UNKNOWN;
        status->valid = false;
        return ESP_OK;
    }

    // Clear buffer
    memset(s_http_buffer, 0, HTTP_BUFFER_SIZE);
    s_http_buffer_len = 0;

    // Build URL
    char url[256];
    snprintf(url, sizeof(url), "https://%s%s", JR_EAST_HOST, JR_EAST_PATH);

    ESP_LOGI(TAG, "Fetching train info from JR East");

    // Configure HTTP client
    esp_http_client_config_t config = {
        .url = url,
        .event_handler = http_event_handler,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 30000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        return ESP_ERR_NO_MEM;
    }

    // Perform request
    esp_err_t err = esp_http_client_perform(client);
    int http_status = esp_http_client_get_status_code(client);

    ESP_LOGI(TAG, "HTTP Status: %d, Content-Length: %d", http_status,
             (int)esp_http_client_get_content_length(client));

    esp_http_client_cleanup(client);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP request failed: %s", esp_err_to_name(err));
        return err;
    }

    if (http_status != 200) {
        ESP_LOGE(TAG, "API returned error status: %d", http_status);
        return ESP_ERR_HTTP_BASE + http_status;
    }

    if (s_http_buffer_len == 0) {
        ESP_LOGE(TAG, "Empty response received");
        return ESP_ERR_INVALID_RESPONSE;
    }

    // Null-terminate for parsing
    s_http_buffer[s_http_buffer_len] = '\0';

    ESP_LOGI(TAG, "Received %d bytes, parsing HTML", (int)s_http_buffer_len);

    // Parse HTML response
    err = train_parse_html(s_http_buffer, s_http_buffer_len, line_name, status);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to parse train data");
        return err;
    }

    status->last_update = time(NULL);
    status->valid = true;
    strncpy(status->line_name, line_name, sizeof(status->line_name) - 1);
    status->line_name[sizeof(status->line_name) - 1] = '\0';

    ESP_LOGI(TAG, "Train status: %s - %d", status->line_name, status->status);
    return ESP_OK;
}

esp_err_t train_service_save_cache(const train_status_t *status)
{
    if (status == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_blob(handle, NVS_KEY_CACHE, status, sizeof(train_status_t));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save cache: %s", esp_err_to_name(err));
        nvs_close(handle);
        return err;
    }

    err = nvs_commit(handle);
    nvs_close(handle);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Train cache saved to NVS");
    }

    return err;
}

esp_err_t train_service_load_cache(train_status_t *status)
{
    if (status == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return err;
    }

    size_t size = sizeof(train_status_t);
    err = nvs_get_blob(handle, NVS_KEY_CACHE, status, &size);
    nvs_close(handle);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Train cache loaded from NVS");
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "No train cache found in NVS");
    }

    return err;
}

void train_service_deinit(void)
{
    if (!s_initialized) {
        return;
    }

    if (s_http_buffer) {
        free(s_http_buffer);
        s_http_buffer = NULL;
    }

    s_initialized = false;
    ESP_LOGI(TAG, "Train service deinitialized");
}
