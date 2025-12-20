/**
 * @file display_types.h
 * @brief Shared display data types for ESP32-S3 Weather Calendar
 */

#ifndef DISPLAY_TYPES_H
#define DISPLAY_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include "weather_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Maximum number of tasks to display
 */
#define MAX_TASKS 5

/**
 * @brief Single task item
 */
typedef struct {
    char name[64];              /**< Task name/description */
    bool completed;             /**< Completion status */
} task_t;

/**
 * @brief Task list data
 */
typedef struct {
    task_t tasks[MAX_TASKS];    /**< Task array */
    int count;                  /**< Number of tasks (0-5) */
    time_t last_update;         /**< Fetch timestamp */
    bool valid;                 /**< Data validity flag */
} task_list_t;

/**
 * @brief Train delay status codes
 */
typedef enum {
    TRAIN_STATUS_UNKNOWN = 0,   /**< Unknown/fetching */
    TRAIN_STATUS_NORMAL,        /**< No delay */
    TRAIN_STATUS_DELAYED,       /**< Delayed */
    TRAIN_STATUS_SUSPENDED,     /**< Service suspended */
    TRAIN_STATUS_ERROR          /**< Fetch error */
} train_status_code_t;

/**
 * @brief Train line status data
 */
typedef struct {
    char line_name[32];         /**< Line name (e.g., "中央線") */
    train_status_code_t status; /**< Status code */
    int delay_minutes;          /**< Delay in minutes (0 if normal) */
    char message[128];          /**< Status message */
    time_t last_update;         /**< Fetch timestamp */
    bool valid;                 /**< Data validity flag */
} train_status_t;

/**
 * @brief Aggregate data for display rendering
 */
typedef struct {
    weather_data_t weather;     /**< Weather data */
    task_list_t tasks;          /**< Task list */
    train_status_t train;       /**< Train status */
    time_t update_time;         /**< Display update timestamp */
} display_data_t;

/**
 * @brief UI Layout configuration
 */
typedef struct {
    uint16_t sidebar_width;     /**< Left sidebar width (pixels) */
    uint16_t content_width;     /**< Right content width (pixels) */
    uint16_t padding;           /**< General padding (pixels) */
    uint16_t weather_col_width; /**< Weather column width (pixels) */
    uint16_t weather_row_height;/**< Weather section height (pixels) */
} ui_layout_t;

#ifdef __cplusplus
}
#endif

#endif // DISPLAY_TYPES_H
