/**
 * @file main.c
 * @brief ESP32-S3 E-ink Weather Calendar - Main Application
 *
 * Main application entry point implementing the weather calendar display.
 * Flow: init -> wifi -> fetch weather -> render -> display -> deep sleep
 */

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "nvs_flash.h"

#include "wifi_manager.h"
#include "epd_driver.h"
#include "gfx_paint.h"
#include "weather_service.h"
#include "task_service.h"
#include "train_service.h"
#include "calendar_ui.h"
#include "display_types.h"
#include "i2c_bsp.h"
#include "axp_prot.h"

static const char *TAG = "main";

// Deep sleep configuration
#define SLEEP_DURATION_DAY_MIN      CONFIG_UPDATE_INTERVAL_DAY_MIN
#define SLEEP_DURATION_NIGHT_MIN    CONFIG_UPDATE_INTERVAL_NIGHT_MIN
#define DAY_START_HOUR              6
#define DAY_END_HOUR                22

// Timeout configuration
#define WIFI_CONNECT_TIMEOUT_MS     30000
#define SNTP_SYNC_TIMEOUT_MS        15000

// API throttling configuration
#define API_THROTTLE_HOURS          3
#define API_THROTTLE_SECONDS        (API_THROTTLE_HOURS * 3600)

// RTC memory data structure (survives deep sleep)
typedef struct {
    uint32_t magic;                 /**< Magic number for validity check */
    time_t last_api_call;           /**< Last successful API call timestamp */
    uint32_t boot_count;            /**< Boot counter */
    weather_data_t cached_weather;  /**< Cached weather data */
} rtc_cache_t;

#define RTC_MAGIC_NUMBER    0xCAFE0002  // Updated for hourly[5] weather data

// RTC memory - survives deep sleep
static RTC_DATA_ATTR rtc_cache_t s_rtc_cache;

// Application state
typedef enum {
    STATE_INIT,
    STATE_WIFI_CONNECT,
    STATE_SYNC_TIME,
    STATE_FETCH_DATA,         // Fetch all data (weather, tasks, train)
    STATE_RENDER_DISPLAY,
    STATE_REFRESH_DISPLAY,
    STATE_DEEP_SLEEP,
    STATE_ERROR
} app_state_t;

static app_state_t s_current_state = STATE_INIT;
static display_data_t s_display_data;   // Aggregate display data
static char s_error_message[64];
static bool s_using_cached_data = false;

// Forward declarations
static bool should_throttle_api_call(void);

/**
 * @brief Initialize power management (AXP2101)
 */
static void init_power_management(void)
{
    i2c_master_Init();
    axp_i2c_prot_init();
    axp_cmd_init();
    xTaskCreate(axp2101_isCharging_task, "axp2101_isCharging_task", 3 * 1024, NULL, 2, NULL);
}

/**
 * @brief Check if RTC cache is valid
 */
static bool rtc_cache_valid(void)
{
    return (s_rtc_cache.magic == RTC_MAGIC_NUMBER);
}

/**
 * @brief Initialize RTC cache (first boot)
 */
static void rtc_cache_init(void)
{
    if (!rtc_cache_valid()) {
        ESP_LOGI(TAG, "Initializing RTC cache (first boot)");
        memset(&s_rtc_cache, 0, sizeof(s_rtc_cache));
        s_rtc_cache.magic = RTC_MAGIC_NUMBER;
        s_rtc_cache.boot_count = 0;
        s_rtc_cache.last_api_call = 0;
        s_rtc_cache.cached_weather.valid = false;
    }
    s_rtc_cache.boot_count++;
    ESP_LOGI(TAG, "Boot count: %lu", s_rtc_cache.boot_count);
}

/**
 * @brief Save weather data to RTC cache
 */
static void rtc_cache_save_weather(const weather_data_t *weather)
{
    if (weather != NULL && weather->valid) {
        memcpy(&s_rtc_cache.cached_weather, weather, sizeof(weather_data_t));
        s_rtc_cache.last_api_call = time(NULL);
        ESP_LOGI(TAG, "Weather data saved to RTC cache");
    }
}

/**
 * @brief Load weather data from RTC cache
 */
static bool rtc_cache_load_weather(weather_data_t *weather)
{
    if (!rtc_cache_valid() || !s_rtc_cache.cached_weather.valid) {
        return false;
    }
    memcpy(weather, &s_rtc_cache.cached_weather, sizeof(weather_data_t));
    ESP_LOGI(TAG, "Weather data loaded from RTC cache");
    return true;
}

/**
 * @brief Fetch all data (weather, tasks, train) with graceful degradation
 */
static esp_err_t fetch_all_data(void)
{
    esp_err_t ret;
    bool any_success = false;

    // Fetch weather (required)
    ESP_LOGI(TAG, "Fetching weather data...");
    if (should_throttle_api_call() && rtc_cache_load_weather(&s_display_data.weather)) {
        ESP_LOGI(TAG, "Using RTC cached weather (API throttled)");
        s_using_cached_data = true;
        any_success = true;
    } else {
        ret = weather_service_fetch(&s_display_data.weather);
        if (ret == ESP_OK) {
            weather_service_save_cache(&s_display_data.weather);
            rtc_cache_save_weather(&s_display_data.weather);
            s_using_cached_data = false;
            any_success = true;
        } else {
            ESP_LOGW(TAG, "Weather fetch failed, trying cache");
            if (rtc_cache_load_weather(&s_display_data.weather) && s_display_data.weather.valid) {
                s_using_cached_data = true;
                any_success = true;
            } else if (weather_service_load_cache(&s_display_data.weather) == ESP_OK && s_display_data.weather.valid) {
                s_using_cached_data = true;
                any_success = true;
            }
        }
    }

    // Fetch tasks (optional - graceful degradation)
    ESP_LOGI(TAG, "Fetching tasks...");
    ret = task_service_fetch(&s_display_data.tasks);
    if (ret == ESP_OK) {
        task_service_save_cache(&s_display_data.tasks);
        ESP_LOGI(TAG, "Tasks fetched: %d items", s_display_data.tasks.count);
    } else {
        ESP_LOGW(TAG, "Task fetch failed, trying cache");
        ret = task_service_load_cache(&s_display_data.tasks);
        if (ret != ESP_OK) {
            // No cached tasks - that's OK
            memset(&s_display_data.tasks, 0, sizeof(task_list_t));
            s_display_data.tasks.valid = false;
        }
    }

    // Fetch train status (optional - graceful degradation)
    ESP_LOGI(TAG, "Fetching train status...");
    ret = train_service_fetch(&s_display_data.train);
    if (ret == ESP_OK) {
        train_service_save_cache(&s_display_data.train);
        ESP_LOGI(TAG, "Train status: %d", s_display_data.train.status);
    } else {
        ESP_LOGW(TAG, "Train fetch failed, trying cache");
        ret = train_service_load_cache(&s_display_data.train);
        if (ret != ESP_OK) {
            // No cached train info - that's OK
            memset(&s_display_data.train, 0, sizeof(train_status_t));
            s_display_data.train.valid = false;
        }
    }

    // Set update time
    s_display_data.update_time = time(NULL);

    return any_success ? ESP_OK : ESP_FAIL;
}

/**
 * @brief Check if API call should be throttled
 */
static bool should_throttle_api_call(void)
{
    if (!rtc_cache_valid()) {
        return false;
    }

    time_t now = time(NULL);
    time_t last_call = s_rtc_cache.last_api_call;

    if (last_call == 0) {
        return false;  // Never called before
    }

    time_t elapsed = now - last_call;
    if (elapsed < API_THROTTLE_SECONDS) {
        ESP_LOGI(TAG, "API throttled: %lld seconds since last call (threshold: %d)",
                 (long long)elapsed, API_THROTTLE_SECONDS);
        return true;
    }

    return false;
}

/**
 * @brief Get appropriate sleep duration based on time of day
 */
static uint32_t get_sleep_duration_minutes(void)
{
    time_t now = time(NULL);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    // Use shorter interval during daytime
    if (timeinfo.tm_hour >= DAY_START_HOUR && timeinfo.tm_hour < DAY_END_HOUR) {
        return SLEEP_DURATION_DAY_MIN;
    } else {
        return SLEEP_DURATION_NIGHT_MIN;
    }
}

/**
 * @brief Enter deep sleep
 */
static void enter_deep_sleep(void)
{
    uint32_t sleep_minutes = get_sleep_duration_minutes();
    uint64_t sleep_us = (uint64_t)sleep_minutes * 60 * 1000000;

    ESP_LOGI(TAG, "Entering deep sleep for %lu minutes", sleep_minutes);

    // Configure timer wake
    esp_sleep_enable_timer_wakeup(sleep_us);

    // Enter deep sleep
    esp_deep_sleep_start();
}

/**
 * @brief Initialize all components
 */
static esp_err_t init_components(void)
{
    esp_err_t ret;

    ESP_LOGI(TAG, "Initializing components...");

    // Initialize power management first (I2C + AXP2101)
    init_power_management();

    // Initialize WiFi manager
    ret = wifi_manager_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize WiFi manager: %s", esp_err_to_name(ret));
        return ret;
    }

    // Initialize EPD driver
    ret = epd_driver_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize EPD driver: %s", esp_err_to_name(ret));
        return ret;
    }

    // Initialize graphics library
    ret = gfx_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize graphics library: %s", esp_err_to_name(ret));
        return ret;
    }

    // Initialize calendar UI
    ret = calendar_ui_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize calendar UI: %s", esp_err_to_name(ret));
        return ret;
    }

    // Initialize weather service
    ret = weather_service_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize weather service: %s", esp_err_to_name(ret));
        return ret;
    }

    // Initialize task service (optional - don't fail if it fails)
    ret = task_service_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to initialize task service: %s (continuing)", esp_err_to_name(ret));
        // Don't return error - task service is optional
    }

    // Initialize train service (optional - don't fail if it fails)
    ret = train_service_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to initialize train service: %s (continuing)", esp_err_to_name(ret));
        // Don't return error - train service is optional
    }

    ESP_LOGI(TAG, "All components initialized successfully");
    return ESP_OK;
}

/**
 * @brief Cleanup before sleep
 */
static void cleanup_components(void)
{
    ESP_LOGI(TAG, "Cleaning up components...");

    // Put display to sleep
    epd_driver_sleep();

    // Disconnect WiFi
    wifi_manager_disconnect();

    // Deinitialize components
    train_service_deinit();
    task_service_deinit();
    weather_service_deinit();
    epd_driver_deinit();
    wifi_manager_deinit();

    ESP_LOGI(TAG, "Cleanup complete");
}

/**
 * @brief Main application state machine
 */
static void run_state_machine(void)
{
    esp_err_t ret;

    while (1) {
        switch (s_current_state) {
            case STATE_INIT:
                ESP_LOGI(TAG, "STATE: INIT");
                ret = init_components();
                if (ret == ESP_OK) {
                    s_current_state = STATE_WIFI_CONNECT;
                } else {
                    snprintf(s_error_message, sizeof(s_error_message),
                             "Init failed: %s", esp_err_to_name(ret));
                    s_current_state = STATE_ERROR;
                }
                break;

            case STATE_WIFI_CONNECT:
                ESP_LOGI(TAG, "STATE: WIFI_CONNECT");
                ret = wifi_manager_connect(WIFI_CONNECT_TIMEOUT_MS);
                if (ret == ESP_OK) {
                    s_current_state = STATE_SYNC_TIME;
                } else {
                    // Try to use cached data
                    ESP_LOGW(TAG, "WiFi connection failed, trying cache");
                    ret = weather_service_load_cache(&s_display_data.weather);
                    if (ret == ESP_OK && weather_service_cache_valid(&s_display_data.weather)) {
                        s_using_cached_data = true;
                        s_display_data.update_time = s_display_data.weather.last_update;
                        s_current_state = STATE_RENDER_DISPLAY;
                    } else {
                        snprintf(s_error_message, sizeof(s_error_message),
                                 "WiFi failed, no cache");
                        s_current_state = STATE_ERROR;
                    }
                }
                break;

            case STATE_SYNC_TIME:
                ESP_LOGI(TAG, "STATE: SYNC_TIME");
                ret = wifi_manager_sync_time(SNTP_SYNC_TIMEOUT_MS);
                if (ret == ESP_OK) {
                    s_current_state = STATE_FETCH_DATA;
                } else {
                    // Continue anyway, time might be stale but usable
                    ESP_LOGW(TAG, "Time sync failed, continuing...");
                    s_current_state = STATE_FETCH_DATA;
                }
                break;

            case STATE_FETCH_DATA:
                ESP_LOGI(TAG, "STATE: FETCH_DATA");
                ret = fetch_all_data();
                if (ret == ESP_OK) {
                    s_current_state = STATE_RENDER_DISPLAY;
                } else {
                    snprintf(s_error_message, sizeof(s_error_message),
                             "No data available");
                    s_current_state = STATE_ERROR;
                }
                break;

            case STATE_RENDER_DISPLAY:
                ESP_LOGI(TAG, "STATE: RENDER_DISPLAY (cached=%d)", s_using_cached_data);
                ret = calendar_ui_draw_full(&s_display_data, time(NULL), s_using_cached_data);
                if (ret == ESP_OK) {
                    s_current_state = STATE_REFRESH_DISPLAY;
                } else {
                    snprintf(s_error_message, sizeof(s_error_message),
                             "Render failed");
                    s_current_state = STATE_ERROR;
                }
                break;

            case STATE_REFRESH_DISPLAY:
                ESP_LOGI(TAG, "STATE: REFRESH_DISPLAY");
                ret = epd_driver_refresh();
                if (ret == ESP_OK) {
                    ESP_LOGI(TAG, "Display refresh complete");
                } else {
                    ESP_LOGW(TAG, "Display refresh failed: %s", esp_err_to_name(ret));
                }
                s_current_state = STATE_DEEP_SLEEP;
                break;

            case STATE_ERROR:
                ESP_LOGE(TAG, "STATE: ERROR - %s", s_error_message);
                // Draw error screen
                calendar_ui_draw_error(s_error_message, s_display_data.weather.last_update);
                epd_driver_refresh();
                s_current_state = STATE_DEEP_SLEEP;
                break;

            case STATE_DEEP_SLEEP:
                ESP_LOGI(TAG, "STATE: DEEP_SLEEP");
                cleanup_components();
#if CONFIG_DISABLE_DEEP_SLEEP
                {
                    uint32_t sleep_minutes = get_sleep_duration_minutes();
                    ESP_LOGW(TAG, "Deep sleep disabled; waiting %lu minutes", sleep_minutes);
                    vTaskDelay(pdMS_TO_TICKS(sleep_minutes * 60U * 1000U));
                    s_current_state = STATE_INIT;
                }
#else
                enter_deep_sleep();
#endif
                // Never returns
                break;

            default:
                ESP_LOGE(TAG, "Unknown state: %d", s_current_state);
                s_current_state = STATE_ERROR;
                break;
        }

        // Small delay between states for stability
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/**
 * @brief Application entry point
 */
void app_main(void)
{
    ESP_LOGI(TAG, "=================================");
    ESP_LOGI(TAG, "ESP32-S3 E-ink Weather Calendar");
    ESP_LOGI(TAG, "=================================");

    // Initialize RTC cache (increments boot counter)
    rtc_cache_init();

    // Check wake reason
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    switch (wakeup_reason) {
        case ESP_SLEEP_WAKEUP_TIMER:
            ESP_LOGI(TAG, "Wakeup: Timer (scheduled wake)");
            break;
        case ESP_SLEEP_WAKEUP_UNDEFINED:
            ESP_LOGI(TAG, "Wakeup: Power on / Reset");
            break;
        default:
            ESP_LOGI(TAG, "Wakeup: Unknown (%d)", wakeup_reason);
            break;
    }

    // Log heap info
    ESP_LOGI(TAG, "Free heap: %lu bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "Minimum free heap: %lu bytes", esp_get_minimum_free_heap_size());

    // Log RTC cache status
    if (rtc_cache_valid() && s_rtc_cache.last_api_call > 0) {
        time_t now = time(NULL);
        time_t elapsed = now - s_rtc_cache.last_api_call;
        ESP_LOGI(TAG, "Last API call: %lld seconds ago", (long long)elapsed);
    }

    // Initialize data
    memset(&s_display_data, 0, sizeof(s_display_data));
    memset(s_error_message, 0, sizeof(s_error_message));
    s_using_cached_data = false;

    // Run main state machine
    s_current_state = STATE_INIT;
    run_state_machine();
}
