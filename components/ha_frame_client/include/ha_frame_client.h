/**
 * Strict HTTPS client for the PhotoPainter Home Assistant device API.
 *
 * The client only constructs URLs under the configured origin and fixed
 * device prefix. Manifest and report bodies are bounded while being read;
 * compressed responses and redirects are rejected.
 */
#ifndef PHOTOPAINTER_HA_FRAME_CLIENT_H
#define PHOTOPAINTER_HA_FRAME_CLIENT_H

#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HA_FRAME_SCHEMA_VERSION 1u
#define HA_FRAME_WIDTH 800u
#define HA_FRAME_HEIGHT 480u
#define HA_FRAME_BYTES ((HA_FRAME_WIDTH * HA_FRAME_HEIGHT) / 2u)
#define HA_FRAME_MANIFEST_MAX 8192u
#define HA_FRAME_REPORT_MAX 4096u
#define HA_FRAME_ID_LENGTH 64u
#define HA_FRAME_ID_BUFFER (HA_FRAME_ID_LENGTH + 1u)
#define HA_FRAME_DEVICE_ID_MAX 64u
#define HA_FRAME_ORIGIN_MAX 192u
#define HA_FRAME_KEY_MAX 256u
#define HA_FRAME_PATH_MAX 256u
#define HA_FRAME_PALETTE_MAX 32u

typedef struct {
    char origin[HA_FRAME_ORIGIN_MAX];
    char device_id[HA_FRAME_DEVICE_ID_MAX + 1u];
    char bearer_key[HA_FRAME_KEY_MAX + 1u];
    bool allow_insecure_http;
    uint32_t timeout_ms;
} ha_frame_client_config_t;

typedef struct {
    char frame_id[HA_FRAME_ID_BUFFER];
    char frame_path[HA_FRAME_PATH_MAX];
    char palette_id[HA_FRAME_PALETTE_MAX];
    uint16_t width;
    uint16_t height;
    uint32_t byte_length;
    int64_t generated_at;
    int64_t fresh_until;
    bool has_fresh_until;
    int64_t next_poll_at;
    bool has_next_poll_at;
    bool redisplay_required;
} ha_frame_manifest_t;

typedef enum {
    HA_FRAME_RESULT_SUCCESS = 0,
    HA_FRAME_RESULT_SKIPPED,
    HA_FRAME_RESULT_FAILED,
} ha_frame_display_result_t;

typedef enum {
    HA_FRAME_OVERLAY_NONE = 0,
    HA_FRAME_OVERLAY_OFFLINE,
    HA_FRAME_OVERLAY_TIME_UNKNOWN,
} ha_frame_overlay_t;

typedef struct {
    char report_id[129];
    char boot_id[129];
    char firmware_version[65];
    char received_frame_id[HA_FRAME_ID_BUFFER];
    char displayed_frame_id[HA_FRAME_ID_BUFFER];
    ha_frame_display_result_t display_result;
    ha_frame_overlay_t local_overlay;
    time_t display_completed_at;
    time_t overlay_source_time;
    time_t next_wake_at;
    int battery_percent;
    int wifi_rssi_dbm;
    char error_code[65];
} ha_frame_report_t;

typedef struct {
    ha_frame_client_config_t config;
    bool initialized;
    int last_http_status;
    uint32_t retry_after_seconds;
} ha_frame_client_t;

esp_err_t ha_frame_client_init(ha_frame_client_t *client,
                               const ha_frame_client_config_t *config);

esp_err_t ha_frame_client_get_manifest(ha_frame_client_t *client,
                                       ha_frame_manifest_t *manifest);

esp_err_t ha_frame_client_get_frame(ha_frame_client_t *client,
                                    const ha_frame_manifest_t *manifest,
                                    uint8_t *frame, size_t frame_size);

esp_err_t ha_frame_client_post_report(ha_frame_client_t *client,
                                      const ha_frame_report_t *report);

/** Validate a packed 4bpp frame against the provisional six-color palette. */
bool ha_frame_client_packed4_valid(const uint8_t *frame, size_t frame_size);

#ifdef __cplusplus
}
#endif

#endif
