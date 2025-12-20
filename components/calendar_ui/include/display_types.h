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
#include "task_types.h"
#include "train_types.h"

#ifdef __cplusplus
extern "C" {
#endif

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
