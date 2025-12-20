/**
 * @file weather_parser.c
 * @brief Weather JSON parser using cJSON
 */

#include "weather_service.h"
#include "cJSON.h"
#include "esp_log.h"
#include <string.h>
#include <time.h>

static const char *TAG = "weather_parser";

/**
 * @brief Get hour of day from Unix timestamp
 */
static int get_hour_of_day(time_t timestamp)
{
    struct tm timeinfo;
    localtime_r(&timestamp, &timeinfo);
    return timeinfo.tm_hour;
}

/**
 * @brief Get day of year from Unix timestamp
 */
static int get_day_of_year(time_t timestamp)
{
    struct tm timeinfo;
    localtime_r(&timestamp, &timeinfo);
    return timeinfo.tm_yday;
}

/**
 * @brief Parse a single forecast entry
 */
static void parse_forecast_entry(cJSON *entry, weather_forecast_t *forecast)
{
    // Parse timestamp
    cJSON *dt = cJSON_GetObjectItem(entry, "dt");
    if (cJSON_IsNumber(dt)) {
        forecast->timestamp = (time_t)dt->valuedouble;
    }

    // Parse main data
    cJSON *main = cJSON_GetObjectItem(entry, "main");
    if (main) {
        cJSON *temp = cJSON_GetObjectItem(main, "temp");
        if (cJSON_IsNumber(temp)) {
            forecast->temp = (float)temp->valuedouble;
        }

        cJSON *temp_min = cJSON_GetObjectItem(main, "temp_min");
        if (cJSON_IsNumber(temp_min)) {
            forecast->temp_min = (float)temp_min->valuedouble;
        }

        cJSON *temp_max = cJSON_GetObjectItem(main, "temp_max");
        if (cJSON_IsNumber(temp_max)) {
            forecast->temp_max = (float)temp_max->valuedouble;
        }

        cJSON *humidity = cJSON_GetObjectItem(main, "humidity");
        if (cJSON_IsNumber(humidity)) {
            forecast->humidity = humidity->valueint;
        }
    }

    // Parse weather array (use first element)
    cJSON *weather_array = cJSON_GetObjectItem(entry, "weather");
    if (cJSON_IsArray(weather_array)) {
        cJSON *weather = cJSON_GetArrayItem(weather_array, 0);
        if (weather) {
            cJSON *description = cJSON_GetObjectItem(weather, "description");
            if (cJSON_IsString(description) && description->valuestring) {
                strncpy(forecast->description, description->valuestring,
                        sizeof(forecast->description) - 1);
                forecast->description[sizeof(forecast->description) - 1] = '\0';
            }

            cJSON *icon = cJSON_GetObjectItem(weather, "icon");
            if (cJSON_IsString(icon) && icon->valuestring) {
                strncpy(forecast->icon_code, icon->valuestring,
                        sizeof(forecast->icon_code) - 1);
                forecast->icon_code[sizeof(forecast->icon_code) - 1] = '\0';
            }
        }
    }

    // Parse wind
    cJSON *wind = cJSON_GetObjectItem(entry, "wind");
    if (wind) {
        cJSON *speed = cJSON_GetObjectItem(wind, "speed");
        if (cJSON_IsNumber(speed)) {
            forecast->wind_speed = (float)speed->valuedouble;
        }

        cJSON *deg = cJSON_GetObjectItem(wind, "deg");
        if (cJSON_IsNumber(deg)) {
            forecast->wind_deg = deg->valueint;
        }
    }
}

esp_err_t weather_parse_response(const char *json, weather_data_t *data)
{
    if (json == NULL || data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Parsing weather JSON response");

    // Initialize data structure
    memset(data, 0, sizeof(weather_data_t));

    // Parse JSON
    cJSON *root = cJSON_Parse(json);
    if (root == NULL) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL) {
            ESP_LOGE(TAG, "JSON parse error before: %s", error_ptr);
        }
        return ESP_ERR_INVALID_RESPONSE;
    }

    // Check for API error
    cJSON *cod = cJSON_GetObjectItem(root, "cod");
    if (cJSON_IsString(cod) && strcmp(cod->valuestring, "200") != 0) {
        ESP_LOGE(TAG, "API error: %s", cod->valuestring);
        cJSON_Delete(root);
        return ESP_ERR_INVALID_RESPONSE;
    }

    // Parse city name
    cJSON *city = cJSON_GetObjectItem(root, "city");
    if (city) {
        cJSON *name = cJSON_GetObjectItem(city, "name");
        if (cJSON_IsString(name) && name->valuestring) {
            strncpy(data->city_name, name->valuestring,
                    sizeof(data->city_name) - 1);
            data->city_name[sizeof(data->city_name) - 1] = '\0';
        }
    }

    // Parse forecast list
    cJSON *list = cJSON_GetObjectItem(root, "list");
    if (!cJSON_IsArray(list)) {
        ESP_LOGE(TAG, "Missing or invalid 'list' array");
        cJSON_Delete(root);
        return ESP_ERR_INVALID_RESPONSE;
    }

    // Get current time and calculate target timestamps
    // We want 5 forecasts at 3-hour intervals: base, +3h, +6h, +9h, +12h
    time_t now = time(NULL);
    data->base_time = now;

    // Calculate target timestamps (3-hour intervals)
    time_t target_times[5];
    for (int i = 0; i < 5; i++) {
        target_times[i] = now + (i * 3 * 3600);  // 0, 3, 6, 9, 12 hours from now
    }

    // Track which slots we've filled
    bool slot_filled[5] = {false, false, false, false, false};
    int hourly_index = 0;

    // Iterate through forecast entries and find closest matches
    cJSON *entry;
    cJSON_ArrayForEach(entry, list) {
        cJSON *dt = cJSON_GetObjectItem(entry, "dt");
        if (!cJSON_IsNumber(dt)) {
            continue;
        }

        time_t timestamp = (time_t)dt->valuedouble;

        // Find which slot this entry is closest to
        for (int i = 0; i < 5; i++) {
            if (slot_filled[i]) {
                continue;
            }

            // Check if this entry is within 1.5 hours of our target
            time_t diff = timestamp - target_times[i];
            if (diff < 0) diff = -diff;  // Absolute value

            if (diff <= 5400) {  // 1.5 hours = 5400 seconds
                ESP_LOGD(TAG, "Found entry for slot %d (target=%ld, actual=%ld, diff=%ld)",
                         i, (long)target_times[i], (long)timestamp, (long)diff);
                parse_forecast_entry(entry, &data->hourly[i]);
                slot_filled[i] = true;
                hourly_index++;
                break;
            }
        }

        // Stop if we have all 5 slots filled
        if (hourly_index >= 5) {
            break;
        }
    }

    cJSON_Delete(root);

    // Check if we got enough data
    if (hourly_index < 5) {
        ESP_LOGW(TAG, "Only found %d hourly forecast entries", hourly_index);
        // Still consider it valid if we have at least 1 entry
        if (hourly_index == 0) {
            return ESP_ERR_INVALID_RESPONSE;
        }
    }

    data->valid = true;
    ESP_LOGI(TAG, "Successfully parsed %d hourly forecasts for %s", hourly_index, data->city_name);

    return ESP_OK;
}
