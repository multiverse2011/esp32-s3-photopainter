/**
 * @file calendar_ui.c
 * @brief Calendar UI implementation for ESP32-S3 Weather Calendar
 *
 * Implements 2-column layout:
 * - Left sidebar (200px): Large date + Tasks section
 * - Right content (600px): Weather (5 columns) + Train + Updated timestamp
 */

#include "calendar_ui.h"
#include "display_types.h"
#include "gfx_paint.h"
#include "epd_driver.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "calendar_ui";

// Layout constants (from calendar_ui.h)
#define SCREEN_WIDTH        UI_SCREEN_WIDTH   // 800
#define SCREEN_HEIGHT       UI_SCREEN_HEIGHT  // 480
#define SIDEBAR_WIDTH       UI_SIDEBAR_WIDTH  // 200
#define CONTENT_WIDTH       UI_CONTENT_WIDTH  // 600
#define PADDING             UI_PADDING        // 10
#define WEATHER_COL_WIDTH   UI_WEATHER_COL_WIDTH  // 120

// New layout positions
#define CONTENT_X           SIDEBAR_WIDTH
#define WEATHER_SECTION_Y   0
#define WEATHER_SECTION_H   280
#define TRAIN_SECTION_Y     (WEATHER_SECTION_H)
#define TRAIN_SECTION_H     140
#define TIMESTAMP_Y         (SCREEN_HEIGHT - 40)

// Sidebar positions
#define DATE_SECTION_Y      20
#define DATE_SECTION_H      200
#define TASKS_SECTION_Y     (DATE_SECTION_H + 20)

// Weather icon size for 5-column layout
#define WEATHER_ICON_SIZE   60

// Color scheme
#define UI_COLOR_ACCENT     EPD_COLOR_RED
#define UI_COLOR_COOL       EPD_COLOR_BLUE
#define UI_COLOR_MUTED      EPD_COLOR_GREEN
#define UI_COLOR_NORMAL     EPD_COLOR_BLACK

// Month names
static const char *month_names[] = {
    "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December"
};

// Weekday names
static const char *weekday_names[] = {
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

static bool s_initialized = false;

esp_err_t calendar_ui_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing calendar UI");
    esp_err_t ret = gfx_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize graphics library: %s", esp_err_to_name(ret));
        return ret;
    }
    s_initialized = true;
    return ESP_OK;
}

/**
 * @brief Get temperature color based on value
 */
static epd_color_t get_temp_color(float temp)
{
    if (temp < UI_TEMP_COLD_THRESHOLD) {
        return EPD_COLOR_BLUE;   // Cold
    } else if (temp > UI_TEMP_WARM_THRESHOLD) {
        return EPD_COLOR_ORANGE; // Warm
    } else {
        return EPD_COLOR_BLACK;  // Normal
    }
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

/**
 * @brief Draw 7-segment style large digit (0-9)
 *
 * Draws a single digit using filled rectangles to create a segment display look.
 *
 * @param x Left position
 * @param y Top position
 * @param digit Digit to draw (0-9)
 * @param height Digit height in pixels
 * @param color Drawing color
 */
static void draw_large_digit(uint16_t x, uint16_t y, int digit, uint16_t height, epd_color_t color)
{
    // Segment dimensions based on height
    uint16_t seg_w = height / 2;      // Width of horizontal segments
    uint16_t seg_h = height / 10;     // Height of horizontal segments
    uint16_t seg_vw = seg_h;          // Width of vertical segments
    uint16_t seg_vh = (height - 3 * seg_h) / 2; // Height of vertical segments

    // Segment positions
    // a = top horizontal
    // b = top right vertical
    // c = bottom right vertical
    // d = bottom horizontal
    // e = bottom left vertical
    // f = top left vertical
    // g = middle horizontal

    // Segment encoding: abcdefg for each digit
    // 1 = segment on, 0 = segment off
    static const uint8_t segments[10] = {
        0b1111110, // 0: a,b,c,d,e,f
        0b0110000, // 1: b,c
        0b1101101, // 2: a,b,d,e,g
        0b1111001, // 3: a,b,c,d,g
        0b0110011, // 4: b,c,f,g
        0b1011011, // 5: a,c,d,f,g
        0b1011111, // 6: a,c,d,e,f,g
        0b1110000, // 7: a,b,c
        0b1111111, // 8: all
        0b1111011, // 9: a,b,c,d,f,g
    };

    if (digit < 0 || digit > 9) {
        return;
    }

    uint8_t segs = segments[digit];

    // Draw segments
    // a - top horizontal
    if (segs & 0b1000000) {
        gfx_fill_rect(x + seg_vw, y, seg_w - 2 * seg_vw, seg_h, color);
    }
    // b - top right vertical
    if (segs & 0b0100000) {
        gfx_fill_rect(x + seg_w - seg_vw, y + seg_h, seg_vw, seg_vh, color);
    }
    // c - bottom right vertical
    if (segs & 0b0010000) {
        gfx_fill_rect(x + seg_w - seg_vw, y + seg_h + seg_vh + seg_h, seg_vw, seg_vh, color);
    }
    // d - bottom horizontal
    if (segs & 0b0001000) {
        gfx_fill_rect(x + seg_vw, y + height - seg_h, seg_w - 2 * seg_vw, seg_h, color);
    }
    // e - bottom left vertical
    if (segs & 0b0000100) {
        gfx_fill_rect(x, y + seg_h + seg_vh + seg_h, seg_vw, seg_vh, color);
    }
    // f - top left vertical
    if (segs & 0b0000010) {
        gfx_fill_rect(x, y + seg_h, seg_vw, seg_vh, color);
    }
    // g - middle horizontal
    if (segs & 0b0000001) {
        gfx_fill_rect(x + seg_vw, y + seg_h + seg_vh, seg_w - 2 * seg_vw, seg_h, color);
    }
}

/**
 * @brief Draw large multi-digit number
 *
 * @param x Left position
 * @param y Top position
 * @param number Number to draw (0-99)
 * @param height Digit height
 * @param color Drawing color
 * @return Width of drawn number
 */
static uint16_t draw_large_number(uint16_t x, uint16_t y, int number, uint16_t height, epd_color_t color)
{
    uint16_t digit_width = height / 2 + height / 10;  // Width per digit + spacing
    uint16_t total_width = 0;

    if (number < 0) number = 0;
    if (number > 99) number = 99;

    if (number >= 10) {
        // Draw tens digit
        draw_large_digit(x, y, number / 10, height, color);
        x += digit_width;
        total_width += digit_width;
    }

    // Draw ones digit
    draw_large_digit(x, y, number % 10, height, color);
    total_width += digit_width;

    return total_width;
}

/**
 * @brief Draw the 2-column layout frame
 *
 * Draws the sidebar/content divider line.
 */
static void draw_layout_frame(void)
{
    // Draw vertical divider between sidebar and content
    gfx_draw_vline(SIDEBAR_WIDTH - 1, 0, SCREEN_HEIGHT, EPD_COLOR_BLACK);
}

/**
 * @brief Draw sidebar date section with large day number
 *
 * @param current_time Current time
 */
static void draw_sidebar_date(time_t current_time)
{
    struct tm timeinfo;
    localtime_r(&current_time, &timeinfo);

    uint16_t center_x = SIDEBAR_WIDTH / 2;

    // Draw large day number (segment display style)
    uint16_t digit_height = 100;
    uint16_t digit_width = digit_height / 2 + digit_height / 10;
    uint16_t num_width = (timeinfo.tm_mday >= 10) ? digit_width * 2 : digit_width;
    uint16_t num_x = center_x - num_width / 2;

    draw_large_number(num_x, DATE_SECTION_Y, timeinfo.tm_mday, digit_height, EPD_COLOR_BLACK);

    // Draw month name below
    gfx_draw_string(center_x, DATE_SECTION_Y + digit_height + 15, month_names[timeinfo.tm_mon],
                    GFX_FONT_24, EPD_COLOR_BLACK, EPD_COLOR_WHITE, GFX_ALIGN_CENTER);

    // Draw weekday below month
    gfx_draw_string(center_x, DATE_SECTION_Y + digit_height + 50, weekday_names[timeinfo.tm_wday],
                    GFX_FONT_16, UI_COLOR_MUTED, EPD_COLOR_WHITE, GFX_ALIGN_CENTER);
}

/**
 * @brief Check if cached data is stale (older than threshold)
 */
static bool is_cache_stale(time_t cache_time, int threshold_hours)
{
    if (cache_time == 0) return true;
    time_t now = time(NULL);
    return (now - cache_time) > (threshold_hours * 3600);
}

/**
 * @brief Draw sidebar tasks section
 *
 * @param tasks Task list to display (can be NULL)
 */
static void draw_sidebar_tasks(const task_list_t *tasks)
{
    uint16_t start_y = TASKS_SECTION_Y;

    // Draw section header with staleness indicator
    const char *header = "Tasks";
    epd_color_t header_color = EPD_COLOR_BLACK;
    if (tasks != NULL && tasks->valid && is_cache_stale(tasks->last_update, 6)) {
        header = "Tasks*"; // Asterisk indicates stale data (>6 hours old)
        header_color = UI_COLOR_MUTED;
    }
    gfx_draw_string(PADDING, start_y, header, GFX_FONT_24, header_color, EPD_COLOR_WHITE, GFX_ALIGN_LEFT);

    // Draw horizontal line under header
    gfx_draw_hline(PADDING, start_y + 30, SIDEBAR_WIDTH - 2 * PADDING, EPD_COLOR_BLACK);

    start_y += 40;

    if (tasks == NULL || !tasks->valid || tasks->count == 0) {
        // No tasks
        gfx_draw_string(PADDING, start_y, "No tasks", GFX_FONT_16, UI_COLOR_MUTED, EPD_COLOR_WHITE, GFX_ALIGN_LEFT);
        return;
    }

    // Draw task list with bullets
    for (int i = 0; i < tasks->count && i < MAX_TASKS; i++) {
        char task_str[70];
        // Truncate task name if too long (max ~18 chars to fit in sidebar)
        char truncated_name[24];
        strncpy(truncated_name, tasks->tasks[i].name, sizeof(truncated_name) - 1);
        truncated_name[sizeof(truncated_name) - 1] = '\0';
        if (strlen(tasks->tasks[i].name) > sizeof(truncated_name) - 1) {
            truncated_name[sizeof(truncated_name) - 4] = '.';
            truncated_name[sizeof(truncated_name) - 3] = '.';
            truncated_name[sizeof(truncated_name) - 2] = '.';
        }

        snprintf(task_str, sizeof(task_str), "- %s", truncated_name);
        gfx_draw_string(PADDING, start_y, task_str, GFX_FONT_16,
                        EPD_COLOR_BLACK, EPD_COLOR_WHITE, GFX_ALIGN_LEFT);
        start_y += 25;
    }
}

/**
 * @brief Draw weather section with 5 time slot columns
 *
 * @param weather Weather data
 */
static void draw_weather_section(const weather_data_t *weather)
{
    if (weather == NULL || !weather->valid) {
        gfx_draw_string(CONTENT_X + CONTENT_WIDTH / 2, WEATHER_SECTION_Y + 100,
                        "Weather unavailable", GFX_FONT_24, UI_COLOR_MUTED,
                        EPD_COLOR_WHITE, GFX_ALIGN_CENTER);
        return;
    }

    // Draw section header with staleness indicator
    const char *header = "Weather";
    epd_color_t header_color = EPD_COLOR_BLACK;
    if (is_cache_stale(weather->last_update, 3)) {
        header = "Weather*"; // Asterisk indicates stale data (>3 hours old)
        header_color = UI_COLOR_MUTED;
    }
    gfx_draw_string(CONTENT_X + PADDING, WEATHER_SECTION_Y + PADDING, header,
                    GFX_FONT_24, header_color, EPD_COLOR_WHITE, GFX_ALIGN_LEFT);

    // Draw horizontal line under header
    gfx_draw_hline(CONTENT_X + PADDING, WEATHER_SECTION_Y + 40, CONTENT_WIDTH - 2 * PADDING, EPD_COLOR_BLACK);

    // Draw 5 weather columns
    uint16_t col_start_x = CONTENT_X;
    uint16_t col_start_y = WEATHER_SECTION_Y + 50;

    for (int i = 0; i < 5; i++) {
        const weather_forecast_t *forecast = &weather->hourly[i];
        uint16_t col_x = col_start_x + (i * WEATHER_COL_WIDTH);
        uint16_t col_center = col_x + WEATHER_COL_WIDTH / 2;

        // Get time info
        struct tm timeinfo;
        localtime_r(&forecast->timestamp, &timeinfo);

        // Draw time label (HH:00 format)
        char time_str[8];
        snprintf(time_str, sizeof(time_str), "%02d:00", timeinfo.tm_hour);
        gfx_draw_string(col_center, col_start_y, time_str, GFX_FONT_16,
                        EPD_COLOR_BLACK, EPD_COLOR_WHITE, GFX_ALIGN_CENTER);

        // Draw weather icon
        calendar_ui_draw_weather_icon(col_center, col_start_y + 30 + WEATHER_ICON_SIZE / 2,
                                      WEATHER_ICON_SIZE, forecast->icon_code);

        // Draw temperature (large font)
        // Handle extreme temperatures (-40 to 50 typical, but allow -99 to 999)
        char temp_str[16];
        float display_temp = forecast->temp;
        if (display_temp < -99) display_temp = -99;
        if (display_temp > 99) display_temp = 99;
        snprintf(temp_str, sizeof(temp_str), "%.0fC", display_temp);
        gfx_draw_string(col_center, col_start_y + 30 + WEATHER_ICON_SIZE + 15, temp_str,
                        GFX_FONT_24, get_temp_color(forecast->temp), EPD_COLOR_WHITE, GFX_ALIGN_CENTER);

        // Draw humidity (small font)
        char humidity_str[8];
        snprintf(humidity_str, sizeof(humidity_str), "%d%%", forecast->humidity);
        gfx_draw_string(col_center, col_start_y + 30 + WEATHER_ICON_SIZE + 50, humidity_str,
                        GFX_FONT_16, UI_COLOR_COOL, EPD_COLOR_WHITE, GFX_ALIGN_CENTER);

        // Draw wind (small font)
        char wind_str[16];
        snprintf(wind_str, sizeof(wind_str), "%s%.0f", get_wind_direction(forecast->wind_deg), forecast->wind_speed);
        gfx_draw_string(col_center, col_start_y + 30 + WEATHER_ICON_SIZE + 70, wind_str,
                        GFX_FONT_16, UI_COLOR_MUTED, EPD_COLOR_WHITE, GFX_ALIGN_CENTER);

        // Draw column separator (except for last column)
        if (i < 4) {
            gfx_draw_vline(col_x + WEATHER_COL_WIDTH - 1, col_start_y, WEATHER_SECTION_H - 60, UI_COLOR_MUTED);
        }
    }
}

/**
 * @brief Draw train section
 *
 * @param train Train status data (can be NULL)
 */
static void draw_train_section(const train_status_t *train)
{
    uint16_t section_x = CONTENT_X;
    uint16_t section_y = TRAIN_SECTION_Y;

    // Draw section header with staleness indicator
    const char *header = "Train";
    epd_color_t header_color = EPD_COLOR_BLACK;
    if (train != NULL && train->valid && is_cache_stale(train->last_update, 1)) {
        header = "Train*"; // Asterisk indicates stale data (>1 hour old)
        header_color = UI_COLOR_MUTED;
    }
    gfx_draw_string(section_x + PADDING, section_y + PADDING, header,
                    GFX_FONT_24, header_color, EPD_COLOR_WHITE, GFX_ALIGN_LEFT);

    // Draw horizontal line under header
    gfx_draw_hline(section_x + PADDING, section_y + 40, CONTENT_WIDTH - 2 * PADDING, EPD_COLOR_BLACK);

    uint16_t text_y = section_y + 60;

    if (train == NULL || !train->valid) {
        gfx_draw_string(section_x + PADDING, text_y, "Train info unavailable",
                        GFX_FONT_16, UI_COLOR_MUTED, EPD_COLOR_WHITE, GFX_ALIGN_LEFT);
        return;
    }

    // Draw line name
    if (strlen(train->line_name) > 0) {
        gfx_draw_string(section_x + PADDING, text_y, train->line_name,
                        GFX_FONT_24, EPD_COLOR_BLACK, EPD_COLOR_WHITE, GFX_ALIGN_LEFT);
    }

    // Draw status with color coding
    const char *status_text = "Unknown";
    epd_color_t status_color = UI_COLOR_MUTED;

    switch (train->status) {
        case TRAIN_STATUS_NORMAL:
            status_text = "Normal operation";
            status_color = UI_COLOR_TRAIN_NORMAL;
            break;
        case TRAIN_STATUS_DELAYED:
            status_text = "Delayed";
            status_color = UI_COLOR_TRAIN_DELAYED;
            break;
        case TRAIN_STATUS_SUSPENDED:
            status_text = "Service suspended";
            status_color = UI_COLOR_TRAIN_DELAYED;
            break;
        case TRAIN_STATUS_UNKNOWN:
        case TRAIN_STATUS_ERROR:
        default:
            status_text = "Status unknown";
            status_color = UI_COLOR_MUTED;
            break;
    }

    gfx_draw_string(section_x + CONTENT_WIDTH / 2, text_y, status_text,
                    GFX_FONT_24, status_color, EPD_COLOR_WHITE, GFX_ALIGN_CENTER);

    // Draw additional message if available
    if (strlen(train->message) > 0 && train->status != TRAIN_STATUS_NORMAL) {
        gfx_draw_string(section_x + PADDING, text_y + 35, train->message,
                        GFX_FONT_16, EPD_COLOR_BLACK, EPD_COLOR_WHITE, GFX_ALIGN_LEFT);
    }
}

/**
 * @brief Draw update timestamp in bottom-right corner
 *
 * @param update_time Timestamp to display
 * @param using_cache True if showing cached data
 */
static void draw_updated_timestamp(time_t update_time, bool using_cache)
{
    if (update_time == 0) {
        return;
    }

    struct tm timeinfo;
    localtime_r(&update_time, &timeinfo);

    char update_str[48];
    if (using_cache) {
        snprintf(update_str, sizeof(update_str), "Cached %04d/%02d/%02d %02d:%02d",
                 timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                 timeinfo.tm_hour, timeinfo.tm_min);
    } else {
        snprintf(update_str, sizeof(update_str), "Updated %04d/%02d/%02d %02d:%02d",
                 timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                 timeinfo.tm_hour, timeinfo.tm_min);
    }

    // Draw in bottom-right of content area
    epd_color_t color = using_cache ? UI_COLOR_ACCENT : UI_COLOR_MUTED;
    gfx_draw_string(SCREEN_WIDTH - PADDING, TIMESTAMP_Y, update_str,
                    GFX_FONT_16, color, EPD_COLOR_WHITE, GFX_ALIGN_RIGHT);
}

/**
 * @brief Draw full layout with all sections
 *
 * @param data Display data containing weather, tasks, train info
 * @param current_time Current time
 * @param using_cache True if displaying cached data
 */
static void draw_full_layout(const display_data_t *data, time_t current_time, bool using_cache)
{
    // Clear screen
    epd_driver_clear(EPD_COLOR_WHITE);

    // Draw layout frame (divider lines)
    draw_layout_frame();

    // Draw sidebar sections
    draw_sidebar_date(current_time);
    draw_sidebar_tasks(data ? &data->tasks : NULL);

    // Draw content sections
    draw_weather_section(data ? &data->weather : NULL);
    draw_train_section(data ? &data->train : NULL);

    // Draw timestamp
    time_t update_time = data ? data->update_time : current_time;
    draw_updated_timestamp(update_time, using_cache);
}

// Legacy compatibility - draw old header
void calendar_ui_draw_header(time_t current_time)
{
    struct tm timeinfo;
    localtime_r(&current_time, &timeinfo);

    // Clear header area (legacy behavior - no longer used in new layout)
    gfx_fill_rect(0, 0, SCREEN_WIDTH, UI_HEADER_HEIGHT, EPD_COLOR_WHITE);

    // Draw date on left side
    char date_str[32];
    snprintf(date_str, sizeof(date_str), "%s, %s %d",
             weekday_names[timeinfo.tm_wday],
             month_names[timeinfo.tm_mon],
             timeinfo.tm_mday);
    gfx_draw_string(20, 20, date_str, GFX_FONT_24, UI_COLOR_ACCENT, EPD_COLOR_WHITE, GFX_ALIGN_LEFT);

    // Draw time on right side
    char time_str[16];
    snprintf(time_str, sizeof(time_str), "%02d:%02d",
             timeinfo.tm_hour, timeinfo.tm_min);
    gfx_draw_string(SCREEN_WIDTH - 20, 20, time_str, GFX_FONT_24, UI_COLOR_COOL, EPD_COLOR_WHITE, GFX_ALIGN_RIGHT);
}

// Legacy compatibility - draw single forecast column
void calendar_ui_draw_forecast(int column, const weather_forecast_t *forecast)
{
    if (column < 0 || column > 4 || forecast == NULL) {
        return;
    }

    // This function is kept for compatibility but no longer used in new layout
    // The new layout uses draw_weather_section() instead
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

    // Create display_data_t from weather-only data (for backward compatibility)
    display_data_t data;
    memset(&data, 0, sizeof(data));
    memcpy(&data.weather, weather, sizeof(weather_data_t));
    data.update_time = weather->last_update;
    // tasks and train will be empty/invalid

    // Use new full layout
    draw_full_layout(&data, current_time, using_cache);

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
    gfx_draw_string(SCREEN_WIDTH / 2, 15, "Error", GFX_FONT_24,
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

/**
 * @brief Draw complete display with all data
 *
 * New API for drawing with tasks and train data.
 *
 * @param data_ptr Complete display data (cast to display_data_t*)
 * @param current_time Current time
 * @param using_cache True if displaying cached data
 * @return ESP_OK on success
 */
esp_err_t calendar_ui_draw_full(const void *data_ptr, time_t current_time, bool using_cache)
{
    if (!s_initialized) {
        ESP_LOGE(TAG, "Calendar UI not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (data_ptr == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    const display_data_t *data = (const display_data_t *)data_ptr;

    ESP_LOGI(TAG, "Drawing full calendar UI (cached=%d)", using_cache);

    draw_full_layout(data, current_time, using_cache);

    ESP_LOGI(TAG, "Full calendar UI drawing complete");
    return ESP_OK;
}
