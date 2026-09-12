#include "ha_frame_runtime.h"
#include "sdkconfig.h"

#include "axp_prot.h"
#include "epd_driver.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_sleep.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "frame_store.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ha_frame_client.h"
#include "ha_frame_policy.h"
#include "driver/uart.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "wifi_manager.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

#ifndef CONFIG_PHOTOPAINTER_HA_ORIGIN
#define CONFIG_PHOTOPAINTER_HA_ORIGIN "https://homeassistant.local"
#endif
#ifndef CONFIG_PHOTOPAINTER_HA_DEVICE_ID
#define CONFIG_PHOTOPAINTER_HA_DEVICE_ID "hall-display"
#endif
#ifndef CONFIG_PHOTOPAINTER_HA_ALLOW_INSECURE_HTTP
#define CONFIG_PHOTOPAINTER_HA_ALLOW_INSECURE_HTTP 0
#endif
#ifndef CONFIG_PHOTOPAINTER_HA_TIMEOUT_MS
#define CONFIG_PHOTOPAINTER_HA_TIMEOUT_MS 10000
#endif

#define TAG "photopainter_ha"
#define FIRMWARE_VERSION "0.2.0"
#define WIFI_CONNECT_TIMEOUT_MS 30000u
#define SNTP_SYNC_TIMEOUT_MS 15000u
#define FALLBACK_POLL_SECONDS 1800u
#define MIN_PANEL_REFRESH_SECONDS 300u
#define MIN_SLEEP_SECONDS 30u
#define HA_KEY_NVS_NAMESPACE "photopainter"
#define HA_KEY_NVS_NAME "ha_key"
#define HA_KEY_MAX 256u
#define DISPLAY_STATE_NVS_NAME "display_state"
#define PENDING_REPORT_NVS_NAME "pending_report"
#define FORCE_REDISPLAY_NVS_NAME "force_redisplay"
#define DISPLAY_STATE_MAGIC 0x50504431u
#define RUNTIME_DEADLINE_US 180000000LL

static uint32_t s_report_sequence;
static bool s_cold_boot_recovery_checked;

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint8_t frame_sha256[FRAME_STORE_SHA256_BYTES];
    int64_t displayed_at;
    int64_t overlay_source_time;
    uint8_t overlay;
    uint8_t reserved[7];
} durable_display_state_t;

static esp_err_t load_display_state(durable_display_state_t *state)
{
    if (state == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(state, 0, sizeof(*state));
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(HA_KEY_NVS_NAMESPACE, NVS_READONLY, &handle);
    if (ret != ESP_OK) {
        return ret;
    }
    size_t length = sizeof(*state);
    ret = nvs_get_blob(handle, DISPLAY_STATE_NVS_NAME, state, &length);
    nvs_close(handle);
    if (ret != ESP_OK || length != sizeof(*state) || state->magic != DISPLAY_STATE_MAGIC) {
        memset(state, 0, sizeof(*state));
        return ret == ESP_OK ? ESP_ERR_INVALID_CRC : ret;
    }
    return ESP_OK;
}

static esp_err_t save_display_state(const durable_display_state_t *state)
{
    if (state == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(HA_KEY_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (ret == ESP_OK) {
        ret = nvs_set_blob(handle, DISPLAY_STATE_NVS_NAME, state, sizeof(*state));
        if (ret == ESP_OK) {
            ret = nvs_commit(handle);
        }
        nvs_close(handle);
    }
    return ret;
}

static esp_err_t load_pending_report(ha_frame_report_t *report)
{
    if (report == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(HA_KEY_NVS_NAMESPACE, NVS_READONLY, &handle);
    if (ret != ESP_OK) {
        return ret;
    }
    size_t length = sizeof(*report);
    ret = nvs_get_blob(handle, PENDING_REPORT_NVS_NAME, report, &length);
    nvs_close(handle);
    return length == sizeof(*report) ? ret : ESP_ERR_INVALID_SIZE;
}

static esp_err_t save_pending_report(const ha_frame_report_t *report)
{
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(HA_KEY_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (ret == ESP_OK) {
        ret = nvs_set_blob(handle, PENDING_REPORT_NVS_NAME, report, sizeof(*report));
        if (ret == ESP_OK) {
            ret = nvs_commit(handle);
        }
        nvs_close(handle);
    }
    return ret;
}

static esp_err_t clear_pending_report(void)
{
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(HA_KEY_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (ret == ESP_OK) {
        ret = nvs_erase_key(handle, PENDING_REPORT_NVS_NAME);
        if (ret == ESP_ERR_NVS_NOT_FOUND) {
            ret = ESP_OK;
        }
        if (ret == ESP_OK) {
            ret = nvs_commit(handle);
        }
        nvs_close(handle);
    }
    return ret;
}

static bool load_force_redisplay(void)
{
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(HA_KEY_NVS_NAMESPACE, NVS_READONLY, &handle);
    if (ret != ESP_OK) {
        return false;
    }
    uint8_t value = 0;
    ret = nvs_get_u8(handle, FORCE_REDISPLAY_NVS_NAME, &value);
    nvs_close(handle);
    return ret == ESP_OK && value != 0;
}

static esp_err_t save_force_redisplay(void)
{
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(HA_KEY_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (ret == ESP_OK) {
        ret = nvs_set_u8(handle, FORCE_REDISPLAY_NVS_NAME, 1);
        if (ret == ESP_OK) {
            ret = nvs_commit(handle);
        }
        nvs_close(handle);
    }
    return ret;
}

static esp_err_t clear_force_redisplay(void)
{
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(HA_KEY_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (ret == ESP_OK) {
        ret = nvs_erase_key(handle, FORCE_REDISPLAY_NVS_NAME);
        if (ret == ESP_ERR_NVS_NOT_FOUND) {
            ret = ESP_OK;
        }
        if (ret == ESP_OK) {
            ret = nvs_commit(handle);
        }
        nvs_close(handle);
    }
    return ret;
}

static bool persist_force_redisplay(bool *force_redisplay)
{
    if (force_redisplay == NULL) {
        return false;
    }
    if (*force_redisplay) {
        return true;
    }
    esp_err_t ret = save_force_redisplay();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "could not persist forced redisplay marker: %s", esp_err_to_name(ret));
        return false;
    }
    *force_redisplay = true;
    return true;
}

static bool acknowledge_unknown_report(bool *force_redisplay)
{
    /* The marker is the recovery record.  Persist it before deleting the
       report so a power loss cannot turn a 409 into a lost recovery. */
    if (!persist_force_redisplay(force_redisplay)) {
        return false;
    }
    return clear_pending_report() == ESP_OK;
}

static bool deadline_expired(int64_t deadline_us)
{
    return esp_timer_get_time() >= deadline_us;
}

static bool valid_bearer_key(const char *key)
{
    if (key == NULL || key[0] == '\0') {
        return false;
    }
    size_t length = strnlen(key, HA_KEY_MAX + 1u);
    if (length > HA_KEY_MAX) {
        return false;
    }
    for (size_t i = 0; i < length; ++i) {
        if ((unsigned char)key[i] < 0x21u || (unsigned char)key[i] > 0x7Eu) {
            return false;
        }
    }
    return true;
}

static esp_err_t provision_bearer_key(char key[HA_KEY_MAX + 1u])
{
    ESP_LOGW(TAG, "HA key is not provisioned; paste it on UART and press Enter (input is not echoed)");
    esp_err_t uart_status = uart_driver_install(UART_NUM_0, 1024, 0, 0, NULL, 0);
    if (uart_status != ESP_OK && uart_status != ESP_ERR_INVALID_STATE) {
        return uart_status;
    }
    uart_flush_input(UART_NUM_0);
    uint8_t byte;
    size_t length = 0;
    int64_t deadline = esp_timer_get_time() + 60000000LL;
    while (esp_timer_get_time() < deadline) {
        int received = uart_read_bytes(UART_NUM_0, &byte, 1, pdMS_TO_TICKS(250));
        if (received != 1) {
            continue;
        }
        if (byte == '\r' || byte == '\n') {
            if (length > 0) {
                key[length] = '\0';
                break;
            }
            continue;
        }
        if (length >= HA_KEY_MAX || byte < 0x21u || byte > 0x7Eu) {
            length = 0;
            ESP_LOGW(TAG, "invalid HA key input; try again");
            continue;
        }
        key[length++] = (char)byte;
    }
    if (!valid_bearer_key(key)) {
        memset(key, 0, HA_KEY_MAX + 1u);
        return ESP_ERR_TIMEOUT;
    }
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(HA_KEY_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (ret == ESP_OK) {
        ret = nvs_set_str(handle, HA_KEY_NVS_NAME, key);
        if (ret == ESP_OK) {
            ret = nvs_commit(handle);
        }
        nvs_close(handle);
    }
    if (ret != ESP_OK) {
        memset(key, 0, HA_KEY_MAX + 1u);
    }
    return ret;
}

static esp_err_t load_bearer_key(char key[HA_KEY_MAX + 1u])
{
    memset(key, 0, HA_KEY_MAX + 1u);
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(HA_KEY_NVS_NAMESPACE, NVS_READONLY, &handle);
    if (ret == ESP_OK) {
        size_t length = HA_KEY_MAX + 1u;
        ret = nvs_get_str(handle, HA_KEY_NVS_NAME, key, &length);
        nvs_close(handle);
    }
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        ret = provision_bearer_key(key);
    }
    if (ret == ESP_OK && !valid_bearer_key(key)) {
        memset(key, 0, HA_KEY_MAX + 1u);
        ret = ESP_ERR_INVALID_ARG;
    }
    return ret;
}

static bool frame_id_matches_meta(const ha_frame_manifest_t *manifest,
                                  const frame_store_meta_t *meta)
{
    uint8_t digest[FRAME_STORE_SHA256_BYTES];
    char hex[HA_FRAME_ID_BUFFER];
    size_t high = 0;
    for (size_t i = 0; i < FRAME_STORE_SHA256_BYTES; ++i) {
        char pair[3] = {manifest->frame_id[high], manifest->frame_id[high + 1], '\0'};
        unsigned value = 0;
        if (sscanf(pair, "%2x", &value) != 1) {
            return false;
        }
        digest[i] = (uint8_t)value;
        high += 2;
    }
    static const char alphabet[] = "0123456789abcdef";
    for (size_t i = 0; i < sizeof(digest); ++i) {
        hex[i * 2] = alphabet[digest[i] >> 4];
        hex[i * 2 + 1] = alphabet[digest[i] & 0x0F];
    }
    hex[HA_FRAME_ID_LENGTH] = '\0';
    return strcmp(hex, manifest->frame_id) == 0 &&
           memcmp(meta->frame_sha256, digest, sizeof(digest)) == 0 &&
           strcmp(meta->palette_id, FRAME_STORE_PALETTE_ID) == 0;
}

static bool hex_id_to_digest(const char *id, uint8_t digest[FRAME_STORE_SHA256_BYTES])
{
    if (id == NULL || strlen(id) != HA_FRAME_ID_LENGTH) {
        return false;
    }
    for (size_t i = 0; i < FRAME_STORE_SHA256_BYTES; ++i) {
        unsigned value;
        if (sscanf(id + i * 2, "%2x", &value) != 1) {
            return false;
        }
        digest[i] = (uint8_t)value;
    }
    return true;
}

static void digest_to_hex(const uint8_t digest[FRAME_STORE_SHA256_BYTES],
                          char output[HA_FRAME_ID_BUFFER])
{
    static const char alphabet[] = "0123456789abcdef";
    for (size_t i = 0; i < FRAME_STORE_SHA256_BYTES; ++i) {
        output[i * 2] = alphabet[digest[i] >> 4];
        output[i * 2 + 1] = alphabet[digest[i] & 0x0Fu];
    }
    output[HA_FRAME_ID_LENGTH] = '\0';
}

static void manifest_from_meta(const frame_store_meta_t *meta, ha_frame_manifest_t *manifest)
{
    memset(manifest, 0, sizeof(*manifest));
    digest_to_hex(meta->frame_sha256, manifest->frame_id);
    manifest->width = meta->width;
    manifest->height = meta->height;
    manifest->byte_length = meta->byte_length;
    snprintf(manifest->palette_id, sizeof(manifest->palette_id), "%s", meta->palette_id);
    manifest->generated_at = meta->generated_at;
    manifest->fresh_until = meta->fresh_until;
    manifest->has_fresh_until = meta->fresh_until > 0;
    manifest->redisplay_required = false;
}

static uint8_t *frame_pixel(uint8_t *frame, unsigned x, unsigned y)
{
    return &frame[(y * FRAME_STORE_WIDTH + x) / 2u];
}

static void set_frame_pixel(uint8_t *frame, unsigned x, unsigned y, uint8_t color)
{
    uint8_t *byte = frame_pixel(frame, x, y);
    if ((x & 1u) == 0) {
        *byte = (uint8_t)((*byte & 0x0Fu) | (color << 4));
    } else {
        *byte = (uint8_t)((*byte & 0xF0u) | color);
    }
}

typedef struct {
    char letter;
    uint8_t rows[7];
} overlay_glyph_t;

/* Five-pixel glyphs used by the device-owned status overlay. */
static const overlay_glyph_t s_overlay_glyphs[] = {
    {'A', {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}},
    {'E', {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F}},
    {'F', {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10}},
    {'I', {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x1F}},
    {'L', {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F}},
    {'M', {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11}},
    {'N', {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11}},
    {'O', {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}},
    {'T', {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}},
    {'U', {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}},
    {'W', {0x11, 0x11, 0x11, 0x15, 0x15, 0x1B, 0x11}},
};

static const uint8_t *overlay_glyph(char letter)
{
    for (size_t i = 0; i < sizeof(s_overlay_glyphs) / sizeof(s_overlay_glyphs[0]); ++i) {
        if (s_overlay_glyphs[i].letter == letter) {
            return s_overlay_glyphs[i].rows;
        }
    }
    return NULL;
}

static void draw_local_overlay(uint8_t *frame, ha_frame_overlay_t overlay)
{
    const char *text = overlay == HA_FRAME_OVERLAY_OFFLINE ? "OFFLINE" : "TIME UNKNOWN";
    const unsigned scale = 2u;
    const unsigned glyph_width = 5u * scale;
    const unsigned advance = 7u * scale;
    const unsigned text_width = (unsigned)strlen(text) * advance - 2u * scale;
    const unsigned origin_x = 344u + (432u - text_width) / 2u;
    const unsigned origin_y = 24u + (40u - 7u * scale) / 2u;
    for (unsigned y = 24u; y < 64u; ++y) {
        for (unsigned x = 344u; x < 776u; ++x) {
            set_frame_pixel(frame, x, y, 1u);
        }
    }
    unsigned x = origin_x;
    for (const char *cursor = text; *cursor != '\0'; ++cursor) {
        if (*cursor == ' ') {
            x += advance;
            continue;
        }
        const uint8_t *rows = overlay_glyph(*cursor);
        if (rows == NULL) {
            x += advance;
            continue;
        }
        for (unsigned row = 0; row < 7u; ++row) {
            for (unsigned col = 0; col < 5u; ++col) {
                if (rows[row] & (1u << (4u - col))) {
                    for (unsigned dy = 0; dy < scale; ++dy) {
                        for (unsigned dx = 0; dx < scale; ++dx) {
                            if (x + col * scale + dx < 776u && origin_y + row * scale + dy < 64u) {
                                set_frame_pixel(frame, x + col * scale + dx,
                                                origin_y + row * scale + dy, 0u);
                            }
                        }
                    }
                }
            }
        }
        x += advance;
    }
    (void)glyph_width;
}

static void make_boot_id(char output[129])
{
    uint32_t random_a = esp_random();
    uint32_t random_b = esp_random();
    snprintf(output, 129, "boot-%08lx-%08lx", (unsigned long)random_a, (unsigned long)random_b);
}

static time_t manifest_time(int64_t value)
{
    return value > 0 ? (time_t)value : (time_t)0;
}

static void fill_report(ha_frame_report_t *report, const char *boot_id,
                        const ha_frame_manifest_t *manifest,
                        const char *displayed_id,
                        ha_frame_display_result_t result,
                        ha_frame_overlay_t overlay, time_t overlay_source_time,
                        int battery_percent, int wifi_rssi_dbm, const char *error)
{
    memset(report, 0, sizeof(*report));
    snprintf(report->report_id, sizeof(report->report_id), "%s-%lu", boot_id,
             (unsigned long)++s_report_sequence);
    snprintf(report->boot_id, sizeof(report->boot_id), "%s", boot_id);
    snprintf(report->firmware_version, sizeof(report->firmware_version), "%s", FIRMWARE_VERSION);
    if (manifest != NULL) {
        snprintf(report->received_frame_id, sizeof(report->received_frame_id), "%s", manifest->frame_id);
    }
    if (displayed_id != NULL) {
        snprintf(report->displayed_frame_id, sizeof(report->displayed_frame_id), "%s", displayed_id);
    }
    report->display_result = result;
    report->local_overlay = overlay;
    report->display_completed_at = result == HA_FRAME_RESULT_SUCCESS ? time(NULL) : 0;
    report->overlay_source_time = overlay == HA_FRAME_OVERLAY_NONE ? 0 : overlay_source_time;
    report->next_wake_at = manifest != NULL && manifest->has_next_poll_at ?
                           manifest_time(manifest->next_poll_at) : 0;
    report->battery_percent = battery_percent;
    report->wifi_rssi_dbm = wifi_rssi_dbm;
    if (error != NULL) {
        snprintf(report->error_code, sizeof(report->error_code), "%s", error);
    }
}

static uint32_t sleep_seconds_for_manifest(const ha_frame_manifest_t *manifest)
{
    if (manifest == NULL || !manifest->has_next_poll_at) {
        return FALLBACK_POLL_SECONDS;
    }
    time_t now = time(NULL);
    int64_t delta = manifest->next_poll_at - (int64_t)now;
    if (delta < MIN_SLEEP_SECONDS) {
        return MIN_SLEEP_SECONDS;
    }
    if (delta > 12 * 3600) {
        return 12 * 3600;
    }
    return (uint32_t)delta;
}

static void sleep_until_next_poll(const ha_frame_manifest_t *manifest)
{
#if CONFIG_DISABLE_DEEP_SLEEP
    vTaskDelay(pdMS_TO_TICKS(sleep_seconds_for_manifest(manifest) * 1000u));
#else
    esp_sleep_enable_timer_wakeup((uint64_t)sleep_seconds_for_manifest(manifest) * 1000000ULL);
    esp_deep_sleep_start();
#endif
}

static esp_err_t init_network(bool *initialized, bool *connected)
{
    *initialized = false;
    *connected = false;
    esp_err_t ret = wifi_manager_init();
    if (ret != ESP_OK) {
        return ret;
    }
    *initialized = true;
    ret = wifi_manager_connect(WIFI_CONNECT_TIMEOUT_MS);
    if (ret != ESP_OK) {
        return ret;
    }
    *connected = true;
    /* A valid clock is required for TLS certificate verification. */
    return wifi_manager_sync_time(SNTP_SYNC_TIMEOUT_MS);
}

static void apply_http_status_policy(const ha_frame_client_t *client,
                                     uint32_t *cooldown_seconds,
                                     bool *explicit_cooldown,
                                     bool *block_requests)
{
    if (client == NULL || cooldown_seconds == NULL || explicit_cooldown == NULL ||
        block_requests == NULL || client->last_http_status <= 0) {
        return;
    }
    bool status_is_explicit = false;
    uint32_t fallback = FALLBACK_POLL_SECONDS;
    uint32_t selected = ha_frame_http_cooldown_seconds(
        client->last_http_status, client->retry_after_seconds, fallback,
        &status_is_explicit);
    if (status_is_explicit) {
        *cooldown_seconds = selected;
        *explicit_cooldown = true;
    } else if (*cooldown_seconds < selected) {
        *cooldown_seconds = selected;
    }
    if (ha_frame_http_blocks_wake(client->last_http_status)) {
        *block_requests = true;
    }
}

static bool consume_cold_boot_recovery(void)
{
    return ha_frame_consume_cold_boot_recovery(
        &s_cold_boot_recovery_checked,
        (int)esp_reset_reason(),
        (int)ESP_RST_DEEPSLEEP);
}

static void run_once(void)
{
#ifndef CONFIG_PHOTOPAINTER_ENABLE_CACHE
#define CONFIG_PHOTOPAINTER_ENABLE_CACHE 0
#endif
    const int64_t deadline_us = esp_timer_get_time() + RUNTIME_DEADLINE_US;
    char bearer_key[HA_KEY_MAX + 1u];
    char boot_id[129];
    ha_frame_client_t client;
    ha_frame_manifest_t manifest;
    ha_frame_report_t pending_report;
    ha_frame_report_t current_report;
    durable_display_state_t display_state;
    frame_store_meta_t stored_meta;
    frame_store_meta_t current_meta;
    memset(&manifest, 0, sizeof(manifest));
    memset(&stored_meta, 0, sizeof(stored_meta));
    memset(&current_meta, 0, sizeof(current_meta));
    bool client_ready = false;
    bool display_ready = false;
    bool wifi_initialized = false;
    bool wifi_connected = false;
    bool network_ready = false;
    bool store_ready = false;
    bool cache_configured = CONFIG_PHOTOPAINTER_ENABLE_CACHE != 0;
    bool cache_enabled = false;
    bool have_manifest = false;
    bool frame_ready = false;
    bool cold_boot_recovery = consume_cold_boot_recovery();
    ESP_LOGI(TAG, "wake cycle: reset_reason=%d cold_boot_recovery=%d", (int)esp_reset_reason(), cold_boot_recovery);
    bool pending_valid = load_pending_report(&pending_report) == ESP_OK;
    bool state_valid = load_display_state(&display_state) == ESP_OK;
    bool force_redisplay = load_force_redisplay();
    bool current_report_sent = false;
    bool explicit_cooldown = false;
    bool block_requests = false;
    bool panel_refresh_succeeded = false;
    bool display_state_persisted = false;
    ha_frame_display_result_t display_result = HA_FRAME_RESULT_FAILED;
    ha_frame_overlay_t local_overlay = HA_FRAME_OVERLAY_OFFLINE;
    time_t overlay_source_time = 0;
    const char *error_code = "startup";
    uint8_t *frame = NULL;
    int battery_percent = -1;
    int wifi_rssi_dbm = 0;
    uint32_t cooldown_seconds = FALLBACK_POLL_SECONDS;

    if (cold_boot_recovery) {
        /* Keep the recovery request durable if this boot loses power or
           cannot reach HA before the first successful panel state save. */
        (void)persist_force_redisplay(&force_redisplay);
    }

    epd_driver_set_deadline_us(deadline_us);

    /* Bring the PMIC up before anything else can fail: the charger enable has
       to be applied on every wake, and a missing PMIC must not stop the run. */
    esp_err_t power_status = axp_power_init();
    if (power_status != ESP_OK) {
        ESP_LOGW(TAG, "PMIC unavailable (%s); battery level reported as unknown",
                 esp_err_to_name(power_status));
    } else {
        axp_power_status_t power;
        if (axp_power_read_status(&power) == ESP_OK) {
            battery_percent = power.percent;
            ESP_LOGI(TAG,
                     "battery: connected=%d percent=%d voltage_mv=%d charging=%d vbus=%d state=%s",
                     power.battery_connected, power.percent, power.voltage_mv,
                     power.charging, power.vbus_present,
                     axp_charge_state_str(power.charge_state));
        }
    }

    make_boot_id(boot_id);
    if (load_bearer_key(bearer_key) != ESP_OK) {
        ESP_LOGE(TAG, "HA key unavailable; retrying after the normal wake interval");
        sleep_until_next_poll(NULL);
        return;
    }
    ha_frame_client_config_t client_config = {
        .allow_insecure_http = CONFIG_PHOTOPAINTER_HA_ALLOW_INSECURE_HTTP,
        .timeout_ms = CONFIG_PHOTOPAINTER_HA_TIMEOUT_MS,
    };
    snprintf(client_config.origin, sizeof(client_config.origin), "%s", CONFIG_PHOTOPAINTER_HA_ORIGIN);
    snprintf(client_config.device_id, sizeof(client_config.device_id), "%s", CONFIG_PHOTOPAINTER_HA_DEVICE_ID);
    snprintf(client_config.bearer_key, sizeof(client_config.bearer_key), "%s", bearer_key);
    if (ha_frame_client_init(&client, &client_config) != ESP_OK) {
        ESP_LOGE(TAG, "invalid HA client configuration");
        sleep_until_next_poll(NULL);
        return;
    }
    client_ready = true;

    if (epd_driver_init() != ESP_OK) {
        ESP_LOGE(TAG, "display initialization failed");
        goto cleanup;
    }
    display_ready = true;
    frame = epd_driver_get_buffer();
    if (frame == NULL || deadline_expired(deadline_us)) {
        error_code = "display_buffer";
        goto cleanup;
    }

    esp_err_t store_status = frame_store_init();
    if (store_status == ESP_OK) {
        store_ready = true;
        cache_enabled = cache_configured;
    } else if (store_status != ESP_ERR_NOT_SUPPORTED && store_status != ESP_ERR_NOT_FOUND) {
        ESP_LOGW(TAG, "frame cache initialization failed: %s", esp_err_to_name(store_status));
    }

    esp_err_t network_status = init_network(&wifi_initialized, &wifi_connected);
    if (wifi_connected && (network_status == ESP_OK || time(NULL) > 1577836800)) {
        network_ready = true;
        (void)wifi_manager_get_rssi(&wifi_rssi_dbm);
    } else {
        ESP_LOGW(TAG, "network unavailable; using the last committed frame when present");
        error_code = "network";
    }

    if (network_ready && !deadline_expired(deadline_us)) {
        esp_err_t manifest_status = ha_frame_client_get_manifest(&client, &manifest);
        if (manifest_status == ESP_OK) {
            have_manifest = true;
            ESP_LOGI(TAG, "manifest: frame=%s generated_at=%lld next_poll_at=%lld redisplay=%d",
                     manifest.frame_id, (long long)manifest.generated_at,
                     (long long)(manifest.has_next_poll_at ? manifest.next_poll_at : 0),
                     manifest.redisplay_required);
        } else {
            ESP_LOGW(TAG, "manifest request failed: status=%d error=%s",
                     client.last_http_status, esp_err_to_name(manifest_status));
            error_code = "manifest";
            apply_http_status_policy(&client, &cooldown_seconds, &explicit_cooldown,
                                     &block_requests);
        }
    }

    if (have_manifest && frame != NULL && !deadline_expired(deadline_us)) {
        if (store_ready && frame_store_has_frame() && frame_store_get_meta(&stored_meta) == ESP_OK &&
            frame_id_matches_meta(&manifest, &stored_meta) &&
            frame_store_load(frame, FRAME_STORE_FRAME_BYTES, &stored_meta) == ESP_OK) {
            current_meta = stored_meta;
            frame_ready = true;
        } else {
            esp_err_t frame_status = ha_frame_client_get_frame(&client, &manifest, frame, FRAME_STORE_FRAME_BYTES);
            bool manifest_refresh_failed = false;
            if (frame_status != ESP_OK &&
                (client.last_http_status == 404 || client.last_http_status == 410) &&
                !deadline_expired(deadline_us)) {
                ESP_LOGW(TAG, "frame %s unavailable (HTTP %d); refreshing manifest",
                         manifest.frame_id, client.last_http_status);
                ha_frame_manifest_t refreshed_manifest;
                memset(&refreshed_manifest, 0, sizeof(refreshed_manifest));
                esp_err_t refreshed_status = ha_frame_client_get_manifest(&client, &refreshed_manifest);
                if (refreshed_status == ESP_OK) {
                    manifest = refreshed_manifest;
                    ESP_LOGI(TAG, "manifest refreshed: frame=%s generated_at=%lld next_poll_at=%lld redisplay=%d",
                             manifest.frame_id, (long long)manifest.generated_at,
                             (long long)(manifest.has_next_poll_at ? manifest.next_poll_at : 0),
                             manifest.redisplay_required);
                    frame_status = ha_frame_client_get_frame(&client, &manifest, frame, FRAME_STORE_FRAME_BYTES);
                } else {
                    ESP_LOGW(TAG, "manifest refresh failed: status=%d error=%s",
                             client.last_http_status, esp_err_to_name(refreshed_status));
                    manifest_refresh_failed = true;
                    error_code = "manifest";
                    apply_http_status_policy(&client, &cooldown_seconds, &explicit_cooldown,
                                             &block_requests);
                }
            }
            if (frame_status == ESP_OK) {
                memset(&current_meta, 0, sizeof(current_meta));
                if (!hex_id_to_digest(manifest.frame_id, current_meta.frame_sha256)) {
                    error_code = "frame_id";
                } else {
                    snprintf(current_meta.palette_id, sizeof(current_meta.palette_id), "%s", FRAME_STORE_PALETTE_ID);
                    current_meta.generated_at = manifest.generated_at;
                    current_meta.fresh_until = manifest.has_fresh_until ? manifest.fresh_until : 0;
                    current_meta.width = manifest.width;
                    current_meta.height = manifest.height;
                    current_meta.byte_length = manifest.byte_length;
                    current_meta.reserved_status_overlay = 0;
                    if (cache_configured && !store_ready) {
                        ESP_LOGW(TAG, "frame cache unavailable; displaying validated network frame");
                        frame_ready = true;
                    } else if (cache_enabled) {
                        esp_err_t save_status = frame_store_save(frame, FRAME_STORE_FRAME_BYTES, &current_meta);
                        if (save_status != ESP_OK) {
                            ESP_LOGW(TAG, "frame cache commit failed: %s; displaying network frame",
                                     esp_err_to_name(save_status));
                            error_code = "cache_commit";
                            frame_ready = true;
                        } else {
                            ESP_LOGI(TAG, "frame source=http frame=%s cache=committed", manifest.frame_id);
                            frame_ready = true;
                        }
                    } else {
                        ESP_LOGI(TAG, "frame source=http frame=%s cache=disabled", manifest.frame_id);
                        frame_ready = true;
                    }
                }
            } else if (!manifest_refresh_failed) {
                error_code = "frame";
                apply_http_status_policy(&client, &cooldown_seconds, &explicit_cooldown,
                                         &block_requests);
            }
        }
    }

    if (!frame_ready && store_ready && frame_store_has_frame() && frame_store_get_meta(&stored_meta) == ESP_OK &&
        frame_store_load(frame, FRAME_STORE_FRAME_BYTES, &stored_meta) == ESP_OK) {
        current_meta = stored_meta;
        manifest_from_meta(&stored_meta, &manifest);
        have_manifest = true;
        frame_ready = true;
        network_ready = false;
    }

    char displayed_id[HA_FRAME_ID_BUFFER] = {0};
    if (state_valid) {
        digest_to_hex(display_state.frame_sha256, displayed_id);
    }
    if (frame_ready) {
        char candidate_id[HA_FRAME_ID_BUFFER];
        digest_to_hex(current_meta.frame_sha256, candidate_id);
        time_t now = time(NULL);
        bool time_valid = now > 1577836800;
        ha_frame_overlay_t desired_overlay = network_ready ?
            (time_valid ? HA_FRAME_OVERLAY_NONE : HA_FRAME_OVERLAY_TIME_UNKNOWN) : HA_FRAME_OVERLAY_OFFLINE;
        bool same_frame = state_valid && memcmp(display_state.frame_sha256, current_meta.frame_sha256,
                                                 FRAME_STORE_SHA256_BYTES) == 0;
        bool same_overlay = state_valid &&
                            display_state.overlay == (uint8_t)desired_overlay;
        bool online_force_redisplay = force_redisplay && network_ready && have_manifest;
        bool should_refresh = ha_frame_should_refresh(
            frame_ready, same_frame, same_overlay, manifest.redisplay_required,
            online_force_redisplay, cold_boot_recovery);
        bool refresh_blocked = ha_frame_refresh_blocked(
            (int64_t)now, state_valid ? display_state.displayed_at : 0,
            MIN_PANEL_REFRESH_SECONDS, cold_boot_recovery);
        if (!should_refresh) {
            ESP_LOGI(TAG, "panel refresh skipped: frame and overlay unchanged");
            display_result = HA_FRAME_RESULT_SKIPPED;
            snprintf(displayed_id, sizeof(displayed_id), "%s", candidate_id);
            local_overlay = (ha_frame_overlay_t)display_state.overlay;
            overlay_source_time = (time_t)display_state.overlay_source_time;
        } else if (refresh_blocked) {
            ESP_LOGI(TAG, "panel refresh deferred: minimum interval");
            error_code = "min_refresh";
            local_overlay = (ha_frame_overlay_t)display_state.overlay;
            overlay_source_time = (time_t)display_state.overlay_source_time;
        } else {
            local_overlay = desired_overlay;
            overlay_source_time = local_overlay == HA_FRAME_OVERLAY_OFFLINE ?
                                  (time_t)(current_meta.generated_at > 0 ? current_meta.generated_at :
                                           (state_valid ? display_state.overlay_source_time : 0)) : 0;
            if (local_overlay != HA_FRAME_OVERLAY_NONE) {
                draw_local_overlay(frame, local_overlay);
            }
            if (!deadline_expired(deadline_us) && epd_driver_refresh() == ESP_OK) {
                panel_refresh_succeeded = true;
                display_result = HA_FRAME_RESULT_SUCCESS;
                snprintf(displayed_id, sizeof(displayed_id), "%s", candidate_id);
                durable_display_state_t new_state = {
                    .magic = DISPLAY_STATE_MAGIC,
                    .displayed_at = time(NULL),
                    .overlay_source_time = overlay_source_time,
                    .overlay = (uint8_t)local_overlay,
                };
                memcpy(new_state.frame_sha256, current_meta.frame_sha256, FRAME_STORE_SHA256_BYTES);
                esp_err_t state_status = save_display_state(&new_state);
                display_state_persisted = state_status == ESP_OK;
                if (!display_state_persisted) {
                    ESP_LOGW(TAG, "display state could not be persisted; report remains retryable");
                } else {
                    display_state = new_state;
                    state_valid = true;
                    if (force_redisplay &&
                        ha_frame_recovery_clear_allowed(
                            true, network_ready && have_manifest,
                            panel_refresh_succeeded, display_state_persisted)) {
                        if (clear_force_redisplay() == ESP_OK) {
                            force_redisplay = false;
                        } else {
                            ESP_LOGW(TAG, "forced redisplay marker could not be cleared");
                        }
                    }
                }
            } else {
                error_code = "display_refresh";
            }
        }
    }

    if (display_result == HA_FRAME_RESULT_FAILED && state_valid && displayed_id[0] == '\0') {
        digest_to_hex(display_state.frame_sha256, displayed_id);
    }
    fill_report(&current_report, boot_id, have_manifest ? &manifest : NULL,
                displayed_id[0] == '\0' ? NULL : displayed_id, display_result,
                local_overlay, overlay_source_time, battery_percent, wifi_rssi_dbm,
                display_result == HA_FRAME_RESULT_FAILED ? error_code : NULL);

    if (network_ready && client_ready && pending_valid && !block_requests &&
        !deadline_expired(deadline_us)) {
        esp_err_t pending_status = ha_frame_client_post_report(&client, &pending_report);
        apply_http_status_policy(&client, &cooldown_seconds, &explicit_cooldown,
                                 &block_requests);
        if (pending_status == ESP_OK) {
            esp_err_t clear_status = clear_pending_report();
            if (clear_status == ESP_OK) {
                pending_valid = false;
            } else {
                ESP_LOGW(TAG, "accepted pending report could not be cleared: %s",
                         esp_err_to_name(clear_status));
            }
        } else if (client.last_http_status == 409) {
            if (acknowledge_unknown_report(&force_redisplay)) {
                pending_valid = false;
            } else {
                ESP_LOGW(TAG, "unknown-frame pending report remains durable");
                block_requests = true;
            }
        }
    }
    if (network_ready && client_ready && !pending_valid && !block_requests &&
        !deadline_expired(deadline_us)) {
        if (save_pending_report(&current_report) == ESP_OK) {
            pending_valid = true;
            esp_err_t report_status = ha_frame_client_post_report(&client, &current_report);
            apply_http_status_policy(&client, &cooldown_seconds, &explicit_cooldown,
                                     &block_requests);
            if (report_status == ESP_OK) {
                current_report_sent = true;
                esp_err_t clear_status = clear_pending_report();
                if (clear_status == ESP_OK) {
                    pending_valid = false;
                } else {
                    ESP_LOGW(TAG, "accepted current report could not be cleared: %s",
                             esp_err_to_name(clear_status));
                    block_requests = true;
                }
            } else if (client.last_http_status == 409) {
                if (acknowledge_unknown_report(&force_redisplay)) {
                    pending_valid = false;
                } else {
                    ESP_LOGW(TAG, "unknown-frame current report remains durable");
                    block_requests = true;
                }
            }
        }
    } else if (ha_frame_should_persist_report(network_ready, pending_valid,
                                               block_requests)) {
        (void)save_pending_report(&current_report);
    }

cleanup:
    if (display_ready) {
        esp_err_t sleep_status = epd_driver_sleep();
        if (sleep_status != ESP_OK) {
            ESP_LOGW(TAG, "display sleep failed: %s; continuing cleanup",
                     esp_err_to_name(sleep_status));
        }
        epd_driver_deinit();
    }
    if (store_ready) {
        frame_store_deinit();
    }
    if (wifi_initialized) {
        wifi_manager_disconnect();
        wifi_manager_deinit();
    }
    if (!current_report_sent && network_ready && pending_valid) {
        ESP_LOGW(TAG, "display report remains durable for the next wake");
    }
    if (cooldown_seconds < MIN_SLEEP_SECONDS) {
        cooldown_seconds = MIN_SLEEP_SECONDS;
    }
    if (have_manifest && manifest.has_next_poll_at && network_ready && !explicit_cooldown) {
        int64_t planned = manifest.next_poll_at - (int64_t)time(NULL);
        cooldown_seconds = planned < MIN_SLEEP_SECONDS ? MIN_SLEEP_SECONDS : (uint32_t)planned;
    }
    if (cooldown_seconds > 12u * 3600u) {
        cooldown_seconds = 12u * 3600u;
    }
    ESP_LOGI(TAG, "next wake in %lu seconds", (unsigned long)cooldown_seconds);
#if CONFIG_DISABLE_DEEP_SLEEP
    vTaskDelay(pdMS_TO_TICKS(cooldown_seconds * 1000u));
#else
    esp_sleep_enable_timer_wakeup((uint64_t)cooldown_seconds * 1000000ULL);
    esp_deep_sleep_start();
#endif
}

void ha_frame_runtime_app_main(void)
{
    esp_err_t nvs_status = nvs_flash_init();
    if (nvs_status != ESP_OK) {
        ESP_LOGE(TAG, "NVS initialization failed: %s", esp_err_to_name(nvs_status));
        return;
    }
    while (true) {
        run_once();
    }
}
