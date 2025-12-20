/**
 * @file train_types.h
 * @brief Train status data structures for ESP32-S3 Weather Calendar
 */

#ifndef TRAIN_TYPES_H
#define TRAIN_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

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

#ifdef __cplusplus
}
#endif

#endif // TRAIN_TYPES_H
