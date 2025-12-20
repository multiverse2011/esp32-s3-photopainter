/**
 * @file task_types.h
 * @brief Task data structures for ESP32-S3 Weather Calendar
 */

#ifndef TASK_TYPES_H
#define TASK_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

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

#ifdef __cplusplus
}
#endif

#endif // TASK_TYPES_H
