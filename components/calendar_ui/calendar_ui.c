/**
 * @file calendar_ui.c
 * @brief Calendar UI implementation for ESP32-S3 Weather Calendar
 */

#include "calendar_ui.h"
#include "gfx_paint.h"
#include "epd_driver.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "calendar_ui";

// Layout constants
#define SCREEN_WIDTH        EPD_WIDTH   // 800
#define SCREEN_HEIGHT       EPD_HEIGHT  // 480
#define HEADER_HEIGHT       80
#define FORECAST_TOP        (HEADER_HEIGHT + 10)
#define COLUMN_WIDTH        (SCREEN_WIDTH / 4)  // 200px each
#define ICON_Y_OFFSET       60
#define TEMP_Y_OFFSET       160
#define DETAIL_Y_OFFSET     220

// Weekday names for display
static const char *weekday_names[] = {
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

// Month names
static const char *month_names[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

static bool s_initialized = false;

esp_err_t calendar_ui_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing calendar UI");
    s_initialized = true;
    return ESP_OK;
}

/**
 * @brief Get temperature color
 */
static epd_color_t get_temp_color(float temp)
{
    if (temp < 0) {
        return EPD_COLOR_BLUE;  // Cold
    } else if (temp < 15) {
        return EPD_COLOR_GREEN; // Cool
    } else if (temp < 25) {
        return EPD_COLOR_BLACK; // Normal
    } else if (temp < 30) {
        return EPD_COLOR_ORANGE; // Warm
    } else {
        return EPD_COLOR_RED;    // Hot
    }
}

/**
 * @brief Format temperature string
 */
static void format_temp(float temp, char *buf, size_t buf_size)
{
    snprintf(buf, buf_size, "%.0f", temp);
}

/**
 * @brief Get wind direction string
 */
static const char* get_wind_direction(int deg)
{
    if (deg >= 337 || deg < 22) return "N";
    if (deg < 67) return "NE";
    if (deg < 112) return "E";
    if (deg < 157) return "SE";
    if (deg < 202) return "S";
    if (deg < 247) return "SW";
    if (deg < 292) return "W";
    return "NW";
}

void calendar_ui_draw_header(time_t current_time)
{
    struct tm timeinfo;
    localtime_r(&current_time, &timeinfo);

    // Clear header area
    gfx_fill_rect(0, 0, SCREEN_WIDTH, HEADER_HEIGHT, EPD_COLOR_WHITE);

    // Draw date on left side
    char date_str[32];
    snprintf(date_str, sizeof(date_str), "%s, %s %d",
             weekday_names[timeinfo.tm_wday],
             month_names[timeinfo.tm_mon],
             timeinfo.tm_mday);
    gfx_draw_string(20, 20, date_str, GFX_FONT_32, EPD_COLOR_BLACK, EPD_COLOR_WHITE, GFX_ALIGN_LEFT);

    // Draw time on right side
    char time_str[16];
    snprintf(time_str, sizeof(time_str), "%02d:%02d",
             timeinfo.tm_hour, timeinfo.tm_min);
    gfx_draw_string(SCREEN_WIDTH - 20, 20, time_str, GFX_FONT_32, EPD_COLOR_BLACK, EPD_COLOR_WHITE, GFX_ALIGN_RIGHT);

    // Draw separator line
    gfx_draw_hline(0, HEADER_HEIGHT - 2, SCREEN_WIDTH, EPD_COLOR_BLACK);
    gfx_draw_hline(0, HEADER_HEIGHT - 1, SCREEN_WIDTH, EPD_COLOR_BLACK);
}

void calendar_ui_draw_forecast(int column, const weather_forecast_t *forecast)
{
    if (column < 0 || column > 3 || forecast == NULL) {
        return;
    }

    uint16_t col_x = column * COLUMN_WIDTH;
    uint16_t col_center = col_x + COLUMN_WIDTH / 2;

    // Get day info
    struct tm timeinfo;
    localtime_r(&forecast->timestamp, &timeinfo);

    // Draw day name
    char day_str[16];
    if (column == 0) {
        snprintf(day_str, sizeof(day_str), "Today");
    } else {
        snprintf(day_str, sizeof(day_str), "%s", weekday_names[timeinfo.tm_wday]);
    }
    gfx_draw_string(col_center, FORECAST_TOP + 5, day_str, GFX_FONT_24,
                    EPD_COLOR_BLACK, EPD_COLOR_WHITE, GFX_ALIGN_CENTER);

    // Draw date (M/D)
    char date_str[8];
    snprintf(date_str, sizeof(date_str), "%d/%d", timeinfo.tm_mon + 1, timeinfo.tm_mday);
    gfx_draw_string(col_center, FORECAST_TOP + 30, date_str, GFX_FONT_16,
                    EPD_COLOR_BLACK, EPD_COLOR_WHITE, GFX_ALIGN_CENTER);

    // Draw weather icon
    calendar_ui_draw_weather_icon(col_center, FORECAST_TOP + ICON_Y_OFFSET + UI_ICON_SIZE/2,
                                  UI_ICON_SIZE, forecast->icon_code);

    // Draw temperature (high/low)
    char temp_str[32];
    format_temp(forecast->temp_max, temp_str, sizeof(temp_str));
    gfx_draw_string(col_center - 30, FORECAST_TOP + TEMP_Y_OFFSET, temp_str, GFX_FONT_32,
                    get_temp_color(forecast->temp_max), EPD_COLOR_WHITE, GFX_ALIGN_CENTER);

    // Degree symbol (using 'o')
    gfx_draw_string(col_center + 10, FORECAST_TOP + TEMP_Y_OFFSET, "C", GFX_FONT_24,
                    EPD_COLOR_BLACK, EPD_COLOR_WHITE, GFX_ALIGN_LEFT);

    // Low temperature
    format_temp(forecast->temp_min, temp_str, sizeof(temp_str));
    snprintf(temp_str + strlen(temp_str), sizeof(temp_str) - strlen(temp_str), "C");
    gfx_draw_string(col_center, FORECAST_TOP + TEMP_Y_OFFSET + 40, temp_str, GFX_FONT_16,
                    EPD_COLOR_BLUE, EPD_COLOR_WHITE, GFX_ALIGN_CENTER);

    // Draw humidity
    char humidity_str[16];
    snprintf(humidity_str, sizeof(humidity_str), "%d%%", forecast->humidity);
    gfx_draw_string(col_center, FORECAST_TOP + DETAIL_Y_OFFSET, humidity_str, GFX_FONT_16,
                    EPD_COLOR_BLUE, EPD_COLOR_WHITE, GFX_ALIGN_CENTER);

    // Draw wind
    char wind_str[24];
    snprintf(wind_str, sizeof(wind_str), "%s %.0fm/s",
             get_wind_direction(forecast->wind_deg), forecast->wind_speed);
    gfx_draw_string(col_center, FORECAST_TOP + DETAIL_Y_OFFSET + 25, wind_str, GFX_FONT_16,
                    EPD_COLOR_BLACK, EPD_COLOR_WHITE, GFX_ALIGN_CENTER);

    // Draw column separator (except for last column)
    if (column < 3) {
        gfx_draw_vline(col_x + COLUMN_WIDTH - 1, HEADER_HEIGHT, SCREEN_HEIGHT - HEADER_HEIGHT, EPD_COLOR_BLACK);
    }
}

esp_err_t calendar_ui_draw_ex(const weather_data_t *weather, time_t current_time, bool using_cache)
{
    if (!s_initialized) {
        ESP_LOGE(TAG, "Calendar UI not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (weather == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Drawing calendar UI (cached=%d)", using_cache);

    // Clear screen to white
    epd_driver_clear(EPD_COLOR_WHITE);

    // Draw header with current time
    calendar_ui_draw_header(current_time);

    // Draw 4 forecast columns
    for (int i = 0; i < 4; i++) {
        calendar_ui_draw_forecast(i, &weather->daily[i]);
    }

    // Draw bottom status area
    uint16_t bottom_y = SCREEN_HEIGHT - 35;

    // Draw city name on left
    if (strlen(weather->city_name) > 0) {
        gfx_draw_string(20, bottom_y, weather->city_name,
                        GFX_FONT_16, EPD_COLOR_BLACK, EPD_COLOR_WHITE, GFX_ALIGN_LEFT);
    }

    // Draw cache status and last update on right
    if (using_cache && weather->last_update > 0) {
        // Calculate staleness
        time_t now = current_time;
        time_t elapsed = now - weather->last_update;
        int elapsed_hours = elapsed / 3600;
        int elapsed_mins = (elapsed % 3600) / 60;

        char status_str[48];
        if (elapsed_hours > 0) {
            snprintf(status_str, sizeof(status_str), "Cached (%dh %dm old)", elapsed_hours, elapsed_mins);
        } else {
            snprintf(status_str, sizeof(status_str), "Cached (%dm old)", elapsed_mins);
        }

        // Draw in orange to indicate cached data
        gfx_draw_string(SCREEN_WIDTH - 20, bottom_y, status_str,
                        GFX_FONT_16, EPD_COLOR_ORANGE, EPD_COLOR_WHITE, GFX_ALIGN_RIGHT);
    } else if (weather->last_update > 0) {
        // Show last update time
        struct tm timeinfo;
        localtime_r(&weather->last_update, &timeinfo);

        char update_str[32];
        snprintf(update_str, sizeof(update_str), "Updated %02d:%02d",
                 timeinfo.tm_hour, timeinfo.tm_min);
        gfx_draw_string(SCREEN_WIDTH - 20, bottom_y, update_str,
                        GFX_FONT_16, EPD_COLOR_BLACK, EPD_COLOR_WHITE, GFX_ALIGN_RIGHT);
    }

    ESP_LOGI(TAG, "Calendar UI drawing complete");
    return ESP_OK;
}

esp_err_t calendar_ui_draw(const weather_data_t *weather, time_t current_time)
{
    return calendar_ui_draw_ex(weather, current_time, false);
}

esp_err_t calendar_ui_draw_error(const char *error_message, time_t last_update)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Drawing error screen");

    // Clear screen to white
    epd_driver_clear(EPD_COLOR_WHITE);

    // Draw error header
    gfx_fill_rect(0, 0, SCREEN_WIDTH, 60, EPD_COLOR_RED);
    gfx_draw_string(SCREEN_WIDTH / 2, 15, "Error", GFX_FONT_32,
                    EPD_COLOR_WHITE, EPD_COLOR_RED, GFX_ALIGN_CENTER);

    // Draw error message
    if (error_message) {
        gfx_draw_string(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 - 30, error_message,
                        GFX_FONT_24, EPD_COLOR_RED, EPD_COLOR_WHITE, GFX_ALIGN_CENTER);
    }

    // Draw last update time if available
    if (last_update > 0) {
        struct tm timeinfo;
        localtime_r(&last_update, &timeinfo);

        char update_str[64];
        snprintf(update_str, sizeof(update_str), "Last update: %04d-%02d-%02d %02d:%02d",
                 timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                 timeinfo.tm_hour, timeinfo.tm_min);
        gfx_draw_string(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 + 30, update_str,
                        GFX_FONT_16, EPD_COLOR_BLACK, EPD_COLOR_WHITE, GFX_ALIGN_CENTER);
    }

    return ESP_OK;
}
