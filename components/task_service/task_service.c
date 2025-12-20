/**
 * @file task_service.c
 * @brief Task Service implementation for ESP32-S3 Weather Calendar
 */

#include "task_service.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_crt_bundle.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "task_service";

// API configuration
#define TODOIST_HOST "api.todoist.com"
#define TODOIST_PATH "/rest/v2/tasks"
#define HTTP_BUFFER_SIZE    8192  // 8KB for JSON response

// NVS configuration
#define NVS_NAMESPACE       "tasks"
#define NVS_KEY_CACHE       "cache"

// External parser function
extern esp_err_t task_parse_response(const char *json, task_list_t *tasks);

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

esp_err_t task_service_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing task service");

    // Check if API token is configured
    const char *token = CONFIG_TODOIST_API_TOKEN;
    if (token == NULL || strlen(token) == 0) {
        ESP_LOGW(TAG, "Todoist API token not configured");
    }

    // Allocate HTTP buffer
    s_http_buffer = heap_caps_malloc(HTTP_BUFFER_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (s_http_buffer == NULL) {
        // Fallback to internal RAM
        s_http_buffer = malloc(HTTP_BUFFER_SIZE);
        if (s_http_buffer == NULL) {
            ESP_LOGE(TAG, "Failed to allocate HTTP buffer");
            return ESP_ERR_NO_MEM;
        }
    }

    s_initialized = true;
    ESP_LOGI(TAG, "Task service initialized");
    return ESP_OK;
}

esp_err_t task_service_fetch(task_list_t *tasks)
{
    if (!s_initialized) {
        ESP_LOGE(TAG, "Task service not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (tasks == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // Check if API token is configured
    const char *token = CONFIG_TODOIST_API_TOKEN;
    if (token == NULL || strlen(token) == 0) {
        ESP_LOGW(TAG, "Todoist API token not configured, returning empty task list");
        memset(tasks, 0, sizeof(task_list_t));
        tasks->count = 0;
        tasks->valid = true;
        tasks->last_update = time(NULL);
        return ESP_OK;
    }

    // Clear buffer
    memset(s_http_buffer, 0, HTTP_BUFFER_SIZE);
    s_http_buffer_len = 0;

    // Build URL with filter for today's tasks
    char url[256];
    snprintf(url, sizeof(url), "https://%s%s?filter=today", TODOIST_HOST, TODOIST_PATH);

    // Build authorization header
    char auth_header[256];
    snprintf(auth_header, sizeof(auth_header), "Bearer %s", token);

    ESP_LOGI(TAG, "Fetching tasks from Todoist");

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

    // Set authorization header
    esp_http_client_set_header(client, "Authorization", auth_header);

    // Perform request
    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);

    ESP_LOGI(TAG, "HTTP Status: %d, Content-Length: %d", status,
             (int)esp_http_client_get_content_length(client));

    esp_http_client_cleanup(client);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP request failed: %s", esp_err_to_name(err));
        return err;
    }

    if (status != 200) {
        ESP_LOGE(TAG, "API returned error status: %d", status);
        return ESP_ERR_HTTP_BASE + status;
    }

    if (s_http_buffer_len == 0) {
        ESP_LOGW(TAG, "Empty response, assuming no tasks");
        memset(tasks, 0, sizeof(task_list_t));
        tasks->count = 0;
        tasks->valid = true;
        tasks->last_update = time(NULL);
        return ESP_OK;
    }

    // Null-terminate the buffer
    s_http_buffer[s_http_buffer_len] = '\0';

    // Parse JSON response
    err = task_parse_response(s_http_buffer, tasks);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to parse task data");
        return err;
    }

    tasks->last_update = time(NULL);
    tasks->valid = true;

    ESP_LOGI(TAG, "Fetched %d tasks", tasks->count);
    return ESP_OK;
}

esp_err_t task_service_save_cache(const task_list_t *tasks)
{
    if (tasks == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_blob(handle, NVS_KEY_CACHE, tasks, sizeof(task_list_t));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save cache: %s", esp_err_to_name(err));
        nvs_close(handle);
        return err;
    }

    err = nvs_commit(handle);
    nvs_close(handle);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Task cache saved to NVS");
    }

    return err;
}

esp_err_t task_service_load_cache(task_list_t *tasks)
{
    if (tasks == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return err;
    }

    size_t size = sizeof(task_list_t);
    err = nvs_get_blob(handle, NVS_KEY_CACHE, tasks, &size);
    nvs_close(handle);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Task cache loaded from NVS");
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "No task cache found in NVS");
    }

    return err;
}

void task_service_deinit(void)
{
    if (!s_initialized) {
        return;
    }

    if (s_http_buffer) {
        free(s_http_buffer);
        s_http_buffer = NULL;
    }

    s_initialized = false;
    ESP_LOGI(TAG, "Task service deinitialized");
}
