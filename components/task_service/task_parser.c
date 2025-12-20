/**
 * @file task_parser.c
 * @brief Task JSON parser for ESP32-S3 Weather Calendar
 */

#include "task_types.h"
#include "esp_err.h"
#include "esp_log.h"
#include "cJSON.h"
#include <string.h>

static const char *TAG = "task_parser";

/**
 * @brief Parse Todoist API JSON response
 *
 * Response format:
 * [
 *   {
 *     "id": "123",
 *     "content": "Task name",
 *     "is_completed": false,
 *     "due": {"date": "2025-12-20"},
 *     "priority": 4
 *   }
 * ]
 */
esp_err_t task_parse_response(const char *json, task_list_t *tasks)
{
    if (json == NULL || tasks == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // Initialize output
    memset(tasks, 0, sizeof(task_list_t));

    // Parse JSON
    cJSON *root = cJSON_Parse(json);
    if (root == NULL) {
        ESP_LOGE(TAG, "Failed to parse JSON");
        return ESP_ERR_INVALID_RESPONSE;
    }

    // Response should be an array
    if (!cJSON_IsArray(root)) {
        ESP_LOGE(TAG, "Expected JSON array");
        cJSON_Delete(root);
        return ESP_ERR_INVALID_RESPONSE;
    }

    // Iterate through tasks
    int count = 0;
    cJSON *item = NULL;
    cJSON_ArrayForEach(item, root) {
        if (count >= MAX_TASKS) {
            ESP_LOGW(TAG, "Max tasks reached, skipping remaining");
            break;
        }

        // Get content
        cJSON *content = cJSON_GetObjectItem(item, "content");
        if (cJSON_IsString(content) && content->valuestring) {
            strncpy(tasks->tasks[count].name, content->valuestring,
                    sizeof(tasks->tasks[count].name) - 1);
            tasks->tasks[count].name[sizeof(tasks->tasks[count].name) - 1] = '\0';
        }

        // Get completion status
        cJSON *is_completed = cJSON_GetObjectItem(item, "is_completed");
        if (cJSON_IsBool(is_completed)) {
            tasks->tasks[count].completed = cJSON_IsTrue(is_completed);
        }

        // Only count non-completed tasks
        if (!tasks->tasks[count].completed && strlen(tasks->tasks[count].name) > 0) {
            count++;
        }
    }

    tasks->count = count;
    cJSON_Delete(root);

    ESP_LOGI(TAG, "Parsed %d tasks from JSON", count);
    return ESP_OK;
}
