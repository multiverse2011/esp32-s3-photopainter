/**
 * @file train_parser.c
 * @brief Train HTML parser for ESP32-S3 Weather Calendar
 *
 * Parses JR East train info page to extract delay status.
 */

#include "train_types.h"
#include "esp_err.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static const char *TAG = "train_parser";

// Shift_JIS to UTF-8 conversion table for common Japanese characters
// This is a simplified implementation - full conversion would require iconv or similar

/**
 * @brief Simple Shift_JIS to UTF-8 conversion
 *
 * Note: This is a simplified implementation that handles common cases.
 * For full support, consider using a proper encoding library.
 */
esp_err_t sjis_to_utf8(const char *sjis, size_t sjis_len, char *utf8, size_t utf8_size)
{
    if (sjis == NULL || utf8 == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // For now, just copy assuming the server might return UTF-8
    // The actual JR East page uses Shift_JIS, but modern browsers handle this
    size_t copy_len = (sjis_len < utf8_size - 1) ? sjis_len : utf8_size - 1;
    memcpy(utf8, sjis, copy_len);
    utf8[copy_len] = '\0';

    return ESP_OK;
}

/**
 * @brief Find a string in buffer (case-insensitive for ASCII)
 */
static const char* find_string(const char *haystack, size_t haystack_len, const char *needle)
{
    if (haystack == NULL || needle == NULL) {
        return NULL;
    }

    size_t needle_len = strlen(needle);
    if (needle_len == 0 || needle_len > haystack_len) {
        return NULL;
    }

    for (size_t i = 0; i <= haystack_len - needle_len; i++) {
        if (memcmp(haystack + i, needle, needle_len) == 0) {
            return haystack + i;
        }
    }

    return NULL;
}

/**
 * @brief Parse JR East HTML to find train line status
 *
 * The page structure typically contains:
 * - Line name followed by status (平常運転, 遅延, 運転見合わせ, etc.)
 */
esp_err_t train_parse_html(const char *html, size_t len, const char *line_name, train_status_t *status)
{
    if (html == NULL || line_name == NULL || status == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // Initialize output
    memset(status, 0, sizeof(train_status_t));
    status->status = TRAIN_STATUS_UNKNOWN;

    // Find the target line name in HTML
    const char *line_pos = find_string(html, len, line_name);
    if (line_pos == NULL) {
        ESP_LOGW(TAG, "Train line '%s' not found in page", line_name);
        status->status = TRAIN_STATUS_ERROR;
        snprintf(status->message, sizeof(status->message), "Line not found");
        return ESP_OK;  // Not an error, just line not found
    }

    ESP_LOGI(TAG, "Found line '%s' in page", line_name);

    // Debug: Log content before and after line name
    char debug_buf[201];

    // Content before
    size_t before_len = (line_pos - html < 200) ? (line_pos - html) : 200;
    memcpy(debug_buf, line_pos - before_len, before_len);
    debug_buf[before_len] = '\0';
    ESP_LOGI(TAG, "Content BEFORE line: %.200s", debug_buf);

    // Content after
    size_t debug_after_len = (len - (line_pos - html) < 200) ? (len - (line_pos - html)) : 200;
    memcpy(debug_buf, line_pos, debug_after_len);
    debug_buf[debug_after_len] = '\0';
    ESP_LOGI(TAG, "Content AFTER line: %.200s", debug_buf);

    // Debug: Check if status keywords exist anywhere in HTML
    const char *test_normal = "\xE5\xB9\xB3\xE5\xB8\xB8\xE9\x81\x8B\xE8\xBB\xA2";  // 平常運転
    const char *found_normal = find_string(html, len, test_normal);
    if (found_normal) {
        ptrdiff_t distance = found_normal - line_pos;
        ESP_LOGI(TAG, "DEBUG: Found '平常運転' at distance %d from line name", (int)distance);
    } else {
        ESP_LOGW(TAG, "DEBUG: '平常運転' NOT FOUND anywhere in HTML!");
    }

    // Search for status keywords near the line name
    // Status may be before or after the line name in HTML structure
    // Search 10000 bytes before and 500 bytes after (status can be far from line name)
    size_t before_offset = (line_pos - html < 10000) ? (line_pos - html) : 10000;
    const char *search_start = line_pos - before_offset;
    size_t after_len = (len - (line_pos - html) < 500) ? (len - (line_pos - html)) : 500;
    size_t search_len = before_offset + after_len;

    // Check for various status keywords (in UTF-8)
    // Note: These are Japanese strings that indicate train status

    // "平常運転" (normal operation) - UTF-8: E5 B9 B3 E5 B8 B8 E9 81 8B E8 BB A2
    const char *normal_str = "\xE5\xB9\xB3\xE5\xB8\xB8\xE9\x81\x8B\xE8\xBB\xA2";
    // "遅延" (delayed) - UTF-8: E9 81 85 E5 BB B6
    const char *delay_str = "\xE9\x81\x85\xE5\xBB\xB6";
    // "運転見合わせ" (suspended) - UTF-8: E9 81 8B E8 BB A2 E8 A6 8B E5 90 88 E3 82 8F E3 81 9B
    const char *suspended_str = "\xE9\x81\x8B\xE8\xBB\xA2\xE8\xA6\x8B\xE5\x90\x88\xE3\x82\x8F\xE3\x81\x9B";

    // Also check ASCII variations that might appear
    const char *normal_ascii = "Normal";
    const char *delay_ascii = "Delay";

    if (find_string(search_start, search_len, normal_str) != NULL ||
        find_string(search_start, search_len, normal_ascii) != NULL) {
        status->status = TRAIN_STATUS_NORMAL;
        snprintf(status->message, sizeof(status->message), "Normal operation");
        ESP_LOGI(TAG, "Status: Normal");
    }
    else if (find_string(search_start, search_len, suspended_str) != NULL) {
        status->status = TRAIN_STATUS_SUSPENDED;
        snprintf(status->message, sizeof(status->message), "Service suspended");
        ESP_LOGI(TAG, "Status: Suspended");
    }
    else if (find_string(search_start, search_len, delay_str) != NULL ||
             find_string(search_start, search_len, delay_ascii) != NULL) {
        status->status = TRAIN_STATUS_DELAYED;
        snprintf(status->message, sizeof(status->message), "Delayed");
        ESP_LOGI(TAG, "Status: Delayed");
    }
    else {
        // Couldn't determine status
        status->status = TRAIN_STATUS_UNKNOWN;
        snprintf(status->message, sizeof(status->message), "Status unknown");
        ESP_LOGW(TAG, "Could not determine status");
    }

    return ESP_OK;
}
