/**
 * @file weather_http.c
 * @brief Weather Service HTTP client implementation
 */

#include "weather_service.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_tls.h"
#include "esp_crt_bundle.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "weather_http";

// API configuration
#define OPENWEATHERMAP_HOST "api.openweathermap.org"
#define OPENWEATHERMAP_PATH "/data/2.5/forecast"
#define HTTP_BUFFER_SIZE    32768  // 32KB for JSON response

// Retry configuration
#define HTTP_MAX_RETRIES    3
#define HTTP_INITIAL_BACKOFF_MS  1000  // 1 second
#define HTTP_MAX_BACKOFF_MS      8000  // 8 seconds

// NVS configuration
#define NVS_NAMESPACE       "weather"
#define NVS_KEY_CACHE       "cache"

// External parser function
extern esp_err_t weather_parse_response(const char *json, weather_data_t *data);

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
            if (!esp_http_client_is_chunked_response(evt->client)) {
                if (s_http_buffer && (s_http_buffer_len + evt->data_len < HTTP_BUFFER_SIZE)) {
                    memcpy(s_http_buffer + s_http_buffer_len, evt->data, evt->data_len);
                    s_http_buffer_len += evt->data_len;
                }
            }
            break;
        default:
            break;
    }
    return ESP_OK;
}

esp_err_t weather_service_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing weather service");

    // Allocate HTTP buffer in PSRAM
    s_http_buffer = heap_caps_malloc(HTTP_BUFFER_SIZE, MALLOC_CAP_SPIRAM);
    if (s_http_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate HTTP buffer");
        return ESP_ERR_NO_MEM;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "Weather service initialized");
    return ESP_OK;
}

/**
 * @brief Internal function to perform single HTTP request
 */
static esp_err_t http_fetch_internal(const char *url)
{
    // Clear buffer
    memset(s_http_buffer, 0, HTTP_BUFFER_SIZE);
    s_http_buffer_len = 0;

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
    int status = esp_http_client_get_status_code(client);
    int content_length = esp_http_client_get_content_length(client);
    ESP_LOGI(TAG, "HTTP Status: %d, Content-Length: %d", status, content_length);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP request failed: %s (status=%d)", esp_err_to_name(err), status);
        if (s_http_buffer_len > 0) {
            size_t snippet_len = s_http_buffer_len > 256 ? 256 : s_http_buffer_len;
            s_http_buffer[snippet_len] = '\0';
            ESP_LOGW(TAG, "HTTP error body (first %u bytes): %s", (unsigned)snippet_len, s_http_buffer);
        }
        esp_http_client_cleanup(client);
        return err;
    }

    esp_http_client_cleanup(client);

    if (status != 200) {
        ESP_LOGE(TAG, "API returned error status: %d", status);
        if (s_http_buffer_len > 0) {
            size_t snippet_len = s_http_buffer_len > 256 ? 256 : s_http_buffer_len;
            s_http_buffer[snippet_len] = '\0';
            ESP_LOGW(TAG, "API error body (first %u bytes): %s", (unsigned)snippet_len, s_http_buffer);
        }
        return ESP_ERR_HTTP_BASE + status;
    }

    if (s_http_buffer_len == 0) {
        ESP_LOGE(TAG, "Empty response received");
        return ESP_ERR_INVALID_RESPONSE;
    }

    // Null-terminate the buffer
    s_http_buffer[s_http_buffer_len] = '\0';

    return ESP_OK;
}

esp_err_t weather_service_fetch(weather_data_t *data)
{
    if (!s_initialized) {
        ESP_LOGE(TAG, "Weather service not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // Build URL
    char url[256];
    snprintf(url, sizeof(url),
             "https://%s%s?lat=%s&lon=%s&appid=%s&units=metric&cnt=32",
             OPENWEATHERMAP_HOST,
             OPENWEATHERMAP_PATH,
             CONFIG_LOCATION_LATITUDE,
             CONFIG_LOCATION_LONGITUDE,
             CONFIG_OPENWEATHERMAP_API_KEY);

    {
        size_t key_len = strlen(CONFIG_OPENWEATHERMAP_API_KEY);
        const char *key_tail = (key_len >= 4) ? (CONFIG_OPENWEATHERMAP_API_KEY + key_len - 4) : CONFIG_OPENWEATHERMAP_API_KEY;
        ESP_LOGI(TAG, "OpenWeatherMap API key length: %u, tail: %s", (unsigned)key_len, key_tail);
    }
    ESP_LOGI(TAG, "Fetching weather data from OpenWeatherMap");

    // Retry loop with exponential backoff
    esp_err_t err = ESP_FAIL;
    uint32_t backoff_ms = HTTP_INITIAL_BACKOFF_MS;

    for (int attempt = 0; attempt < HTTP_MAX_RETRIES; attempt++) {
        if (attempt > 0) {
            ESP_LOGW(TAG, "Retry attempt %d/%d after %lu ms backoff",
                     attempt + 1, HTTP_MAX_RETRIES, backoff_ms);
            vTaskDelay(pdMS_TO_TICKS(backoff_ms));

            // Exponential backoff (double each time, up to max)
            backoff_ms *= 2;
            if (backoff_ms > HTTP_MAX_BACKOFF_MS) {
                backoff_ms = HTTP_MAX_BACKOFF_MS;
            }
        }

        err = http_fetch_internal(url);
        if (err == ESP_OK) {
            break;  // Success
        }

        ESP_LOGW(TAG, "HTTP fetch attempt %d failed: %s", attempt + 1, esp_err_to_name(err));
    }

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "All HTTP fetch attempts failed");
        return err;
    }

    ESP_LOGI(TAG, "Received %d bytes, parsing response", s_http_buffer_len);

    // Parse JSON response
    err = weather_parse_response(s_http_buffer, data);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to parse weather data");
        return err;
    }

    data->last_update = time(NULL);
    data->valid = true;

    ESP_LOGI(TAG, "Weather data fetched successfully for %s", data->city_name);
    return ESP_OK;
}

esp_err_t weather_service_save_cache(const weather_data_t *data)
{
    if (data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_blob(handle, NVS_KEY_CACHE, data, sizeof(weather_data_t));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save cache: %s", esp_err_to_name(err));
        nvs_close(handle);
        return err;
    }

    err = nvs_commit(handle);
    nvs_close(handle);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Weather cache saved to NVS");
    }

    return err;
}

esp_err_t weather_service_load_cache(weather_data_t *data)
{
    if (data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return err;
    }

    size_t size = sizeof(weather_data_t);
    err = nvs_get_blob(handle, NVS_KEY_CACHE, data, &size);
    nvs_close(handle);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Weather cache loaded from NVS");
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "No weather cache found in NVS");
    }

    return err;
}

bool weather_service_cache_valid(const weather_data_t *data)
{
    if (data == NULL || !data->valid) {
        return false;
    }

    // Cache is valid for 24 hours
    time_t now = time(NULL);
    time_t age = now - data->last_update;

    bool valid = (age >= 0 && age < 24 * 60 * 60);
    ESP_LOGI(TAG, "Cache age: %ld seconds, valid: %s", (long)age, valid ? "yes" : "no");

    return valid;
}

void weather_service_deinit(void)
{
    if (!s_initialized) {
        return;
    }

    if (s_http_buffer) {
        free(s_http_buffer);
        s_http_buffer = NULL;
    }

    s_initialized = false;
    ESP_LOGI(TAG, "Weather service deinitialized");
}

void weather_service_get_forecast_times(time_t base_time, time_t *times)
{
    if (times == NULL) {
        return;
    }

    // Generate 5 timestamps at 3-hour intervals
    // 0: now, 1: +3h, 2: +6h, 3: +9h, 4: +12h
    for (int i = 0; i < 5; i++) {
        times[i] = base_time + (i * 3 * 3600);
    }
}
