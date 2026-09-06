#include "ha_frame_client.h"

#include "cJSON.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "frame_store.h"
#include "mbedtls/sha256.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TAG "photopainter_http"
#define API_PREFIX "/api/photopainter/v1/devices/"
#define API_MANIFEST_SUFFIX "/manifest"
#define API_FRAMES_SUFFIX "/frames/"
#define API_REPORTS_SUFFIX "/reports"
#define BODY_IO_CHUNK 1024u

typedef struct {
    uint8_t *data;
    size_t length;
    size_t capacity;
    int status_code;
    int64_t content_length;
    uint32_t retry_after_seconds;
    bool unsupported_encoding;
} http_body_t;

static esp_err_t http_event_handler(esp_http_client_event_t *event)
{
    if (event == NULL || event->user_data == NULL || event->event_id != HTTP_EVENT_ON_HEADER) {
        return ESP_OK;
    }
    http_body_t *response = (http_body_t *)event->user_data;
    if (event->header_key == NULL || event->header_value == NULL) {
        return ESP_OK;
    }
    if (strcasecmp(event->header_key, "Content-Encoding") == 0 &&
        strcasecmp(event->header_value, "identity") != 0) {
        response->unsupported_encoding = true;
    } else if (strcasecmp(event->header_key, "Retry-After") == 0) {
        char *end = NULL;
        unsigned long seconds = strtoul(event->header_value, &end, 10);
        if (end != event->header_value && *end == '\0' && seconds <= UINT32_MAX) {
            response->retry_after_seconds = (uint32_t)seconds;
        }
    }
    return ESP_OK;
}

static bool string_nonempty(const char *value, size_t max)
{
    return value != NULL && value[0] != '\0' && strnlen(value, max + 1u) <= max;
}

static bool valid_device_id(const char *value)
{
    size_t length = value == NULL ? 0 : strnlen(value, HA_FRAME_DEVICE_ID_MAX + 1u);
    if (length == 0 || length > HA_FRAME_DEVICE_ID_MAX ||
        (!islower((unsigned char)value[0]) && !isdigit((unsigned char)value[0]))) {
        return false;
    }
    for (size_t i = 0; i < length; ++i) {
        unsigned char c = (unsigned char)value[i];
        if (!(islower(c) || isdigit(c) || c == '-')) {
            return false;
        }
    }
    return true;
}

static bool valid_origin(const char *origin, bool allow_insecure)
{
    if (!string_nonempty(origin, HA_FRAME_ORIGIN_MAX - 1u)) {
        return false;
    }
    const char *host = NULL;
    if (strncmp(origin, "https://", 8) == 0) {
        host = origin + 8;
    } else if (allow_insecure && strncmp(origin, "http://", 7) == 0) {
        host = origin + 7;
    } else {
        return false;
    }
    if (*host == '\0' || *host == '/' || *host == ':' || strchr(host, '?') != NULL ||
        strchr(host, '#') != NULL || strchr(host, '@') != NULL || strchr(host, '\\') != NULL) {
        return false;
    }
    for (const unsigned char *cursor = (const unsigned char *)origin; *cursor != '\0'; ++cursor) {
        if (iscntrl(*cursor) || isspace(*cursor)) {
            return false;
        }
    }
    return strchr(host, '/') == NULL;
}

static bool valid_hex_id(const char *value)
{
    if (value == NULL || strlen(value) != HA_FRAME_ID_LENGTH) {
        return false;
    }
    for (size_t i = 0; i < HA_FRAME_ID_LENGTH; ++i) {
        if (!isxdigit((unsigned char)value[i]) ||
            (isalpha((unsigned char)value[i]) && !islower((unsigned char)value[i]))) {
            return false;
        }
    }
    return true;
}

static bool valid_nibble(uint8_t value)
{
    return value == 0u || value == 1u || value == 2u || value == 3u ||
           value == 5u || value == 6u;
}

bool ha_frame_client_packed4_valid(const uint8_t *frame, size_t frame_size)
{
    if (frame == NULL || frame_size != HA_FRAME_BYTES) {
        return false;
    }
    for (size_t i = 0; i < frame_size; ++i) {
        if (!valid_nibble(frame[i] >> 4) || !valid_nibble(frame[i] & 0x0Fu)) {
            return false;
        }
    }
    return true;
}

static esp_err_t sha256_frame(const uint8_t *frame, size_t frame_size,
                              uint8_t digest[32])
{
    mbedtls_sha256_context context;
    mbedtls_sha256_init(&context);
    int ret = mbedtls_sha256_starts(&context, 0);
    if (ret == 0) {
        ret = mbedtls_sha256_update(&context, frame, frame_size);
    }
    if (ret == 0) {
        ret = mbedtls_sha256_finish(&context, digest);
    }
    mbedtls_sha256_free(&context);
    return ret == 0 ? ESP_OK : ESP_FAIL;
}

static void hex_encode(const uint8_t *bytes, size_t length, char *output, size_t output_size)
{
    static const char alphabet[] = "0123456789abcdef";
    if (output_size < length * 2u + 1u) {
        return;
    }
    for (size_t i = 0; i < length; ++i) {
        output[i * 2u] = alphabet[bytes[i] >> 4];
        output[i * 2u + 1u] = alphabet[bytes[i] & 0x0Fu];
    }
    output[length * 2u] = '\0';
}

static bool parse_iso8601(const char *value, int64_t *seconds)
{
    if (value == NULL || seconds == NULL) {
        return false;
    }
    int year, month, day, hour, minute, second;
    int consumed = 0;
    if (sscanf(value, "%4d-%2d-%2dT%2d:%2d:%2d%n", &year, &month, &day,
               &hour, &minute, &second, &consumed) != 6 || consumed <= 0) {
        return false;
    }
    const char *cursor = value + consumed;
    if (*cursor == '.') {
        ++cursor;
        const char *fraction_start = cursor;
        while (isdigit((unsigned char)*cursor)) {
            ++cursor;
        }
        if (cursor == fraction_start) {
            return false;
        }
    }
    int offset_hour = 0;
    int offset_minute = 0;
    int sign = 1;
    if (*cursor == 'Z' && cursor[1] == '\0') {
        ++cursor;
    } else if ((*cursor == '+' || *cursor == '-') &&
               sscanf(cursor + 1, "%2d:%2d%n", &offset_hour, &offset_minute, &consumed) == 2) {
        if (cursor[0] == '-') {
            sign = -1;
        }
        cursor += 1 + consumed;
        if (offset_hour > 23 || offset_minute > 59 || *cursor != '\0') {
            return false;
        }
    } else {
        return false;
    }
    int leap = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
    static const uint8_t month_days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month < 1 || month > 12 || day < 1 ||
        day > (int)month_days[month - 1] + (month == 2 ? leap : 0) ||
        hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 60) {
        return false;
    }
    /* Proleptic Gregorian days relative to 1970-01-01; avoids changing TZ. */
    int adjusted_year = year - (month <= 2);
    int era = (adjusted_year >= 0 ? adjusted_year : adjusted_year - 399) / 400;
    unsigned year_of_era = (unsigned)(adjusted_year - era * 400);
    unsigned day_of_year = (153u * (unsigned)(month + (month > 2 ? -3 : 9)) + 2u) / 5u +
                           (unsigned)day - 1u;
    unsigned day_of_era = year_of_era * 365u + year_of_era / 4u - year_of_era / 100u + day_of_year;
    int64_t days = (int64_t)era * 146097 + (int64_t)day_of_era - 719468;
    if (year < 1970 || year > 9999) {
        return false;
    }
    int64_t offset = (int64_t)(offset_hour * 3600 + offset_minute * 60);
    *seconds = days * 86400 + (int64_t)hour * 3600 + (int64_t)minute * 60 + second - sign * offset;
    return true;
}

static const cJSON *required_object(const cJSON *parent, const char *name)
{
    const cJSON *value = cJSON_GetObjectItemCaseSensitive(parent, name);
    return cJSON_IsObject(value) ? value : NULL;
}

static const cJSON *required_string(const cJSON *parent, const char *name)
{
    const cJSON *value = cJSON_GetObjectItemCaseSensitive(parent, name);
    return cJSON_IsString(value) && value->valuestring != NULL ? value : NULL;
}

static bool required_integer(const cJSON *parent, const char *name, int64_t *result)
{
    const cJSON *value = cJSON_GetObjectItemCaseSensitive(parent, name);
    if (!cJSON_IsNumber(value) || value->valuedouble != (double)value->valueint) {
        return false;
    }
    *result = value->valueint;
    return true;
}

static bool required_bool(const cJSON *parent, const char *name, bool *result)
{
    const cJSON *value = cJSON_GetObjectItemCaseSensitive(parent, name);
    if (!cJSON_IsBool(value)) {
        return false;
    }
    *result = cJSON_IsTrue(value);
    return true;
}

static esp_err_t parse_manifest(const uint8_t *body, size_t length,
                                const char *expected_device_id,
                                ha_frame_manifest_t *manifest)
{
    const char *parse_end = NULL;
    cJSON *root = cJSON_ParseWithLengthOpts((const char *)body, length, &parse_end, 0);
    if (root == NULL || parse_end == NULL) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_RESPONSE;
    }
    while ((size_t)(parse_end - (const char *)body) < length &&
           isspace((unsigned char)*parse_end)) {
        ++parse_end;
    }
    if ((size_t)(parse_end - (const char *)body) != length) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_RESPONSE;
    }
    esp_err_t result = ESP_ERR_INVALID_RESPONSE;
    int64_t schema, width, height, byte_length, generated_at;
    int64_t overlay_x, overlay_y, overlay_width, overlay_height;
    const cJSON *frame = required_object(root, "frame");
    const cJSON *device = required_string(root, "device_id");
    const cJSON *server_time = required_string(root, "server_time");
    const cJSON *id = frame ? required_string(frame, "id") : NULL;
    const cJSON *path = frame ? required_string(frame, "path") : NULL;
    const cJSON *palette = frame ? required_string(frame, "palette_id") : NULL;
    const cJSON *sha256 = frame ? required_string(frame, "sha256") : NULL;
    const cJSON *format = frame ? required_string(frame, "format") : NULL;
    const cJSON *generated = frame ? required_string(frame, "generated_at") : NULL;
    const cJSON *overlay = frame ? required_object(frame, "status_overlay") : NULL;
    bool redisplay = false;
    if (!required_integer(root, "schema_version", &schema) || schema != HA_FRAME_SCHEMA_VERSION ||
        device == NULL || strcmp(device->valuestring, expected_device_id) != 0 ||
        server_time == NULL || !parse_iso8601(server_time->valuestring, &generated_at) ||
        frame == NULL || id == NULL || path == NULL || palette == NULL || sha256 == NULL ||
        format == NULL || generated == NULL || overlay == NULL ||
        !required_integer(frame, "width", &width) || !required_integer(frame, "height", &height) ||
        !required_integer(frame, "byte_length", &byte_length) || !parse_iso8601(generated->valuestring, &generated_at) ||
        !required_integer(overlay, "x", &overlay_x) || !required_integer(overlay, "y", &overlay_y) ||
        !required_integer(overlay, "width", &overlay_width) || !required_integer(overlay, "height", &overlay_height) ||
        !required_bool(root, "redisplay_required", &redisplay) ||
        !valid_hex_id(id->valuestring) || strcmp(sha256->valuestring, id->valuestring) != 0 ||
        strcmp(format->valuestring, "packed4") != 0 || width != HA_FRAME_WIDTH || height != HA_FRAME_HEIGHT ||
        byte_length != HA_FRAME_BYTES || strcmp(palette->valuestring, FRAME_STORE_PALETTE_ID) != 0 ||
        overlay_x != 344 || overlay_y != 24 || overlay_width != 432 || overlay_height != 40 ||
        strlen(path->valuestring) >= HA_FRAME_PATH_MAX) {
        goto done;
    }
    const cJSON *fresh = cJSON_GetObjectItemCaseSensitive(frame, "fresh_until");
    int64_t fresh_until = 0;
    bool has_fresh_until = false;
    if (fresh == NULL) {
        goto done;
    }
    if (!cJSON_IsNull(fresh)) {
        if (!cJSON_IsString(fresh) || !parse_iso8601(fresh->valuestring, &fresh_until)) {
            goto done;
        }
        has_fresh_until = true;
    }
    const cJSON *schedule = required_object(root, "schedule");
    const cJSON *next_poll = schedule ? cJSON_GetObjectItemCaseSensitive(schedule, "next_poll_at") : NULL;
    int64_t retry_after, min_refresh;
    if (schedule == NULL || next_poll == NULL ||
        !required_integer(schedule, "retry_after_seconds", &retry_after) ||
        !required_integer(schedule, "min_refresh_seconds", &min_refresh) ||
        retry_after < 0 || min_refresh < 300) {
        goto done;
    }
    int64_t next_poll_at = 0;
    bool has_next_poll_at = false;
    if (!cJSON_IsNull(next_poll)) {
        if (!cJSON_IsString(next_poll) || !parse_iso8601(next_poll->valuestring, &next_poll_at)) {
            goto done;
        }
        has_next_poll_at = true;
    }
    memset(manifest, 0, sizeof(*manifest));
    memcpy(manifest->frame_id, id->valuestring, HA_FRAME_ID_LENGTH + 1u);
    memcpy(manifest->frame_path, path->valuestring, strlen(path->valuestring) + 1u);
    memcpy(manifest->palette_id, palette->valuestring, strlen(palette->valuestring) + 1u);
    manifest->width = (uint16_t)width;
    manifest->height = (uint16_t)height;
    manifest->byte_length = (uint32_t)byte_length;
    manifest->generated_at = generated_at;
    manifest->fresh_until = fresh_until;
    manifest->has_fresh_until = has_fresh_until;
    manifest->next_poll_at = next_poll_at;
    manifest->has_next_poll_at = has_next_poll_at;
    manifest->redisplay_required = redisplay;
    result = ESP_OK;
done:
    cJSON_Delete(root);
    return result;
}

static esp_err_t build_url(const ha_frame_client_t *client, const char *suffix,
                           char *url, size_t url_size)
{
    int written = snprintf(url, url_size, "%s%s%s%s", client->config.origin,
                           API_PREFIX, client->config.device_id, suffix);
    return written > 0 && (size_t)written < url_size ? ESP_OK : ESP_ERR_INVALID_SIZE;
}

static esp_err_t build_frame_url(const ha_frame_client_t *client,
                                 const ha_frame_manifest_t *manifest,
                                 char *url, size_t url_size)
{
    char expected_path[HA_FRAME_PATH_MAX];
    int path_len = snprintf(expected_path, sizeof(expected_path), "%s%s%s%s%s",
                            API_PREFIX, client->config.device_id, API_FRAMES_SUFFIX,
                            manifest->frame_id, "");
    if (path_len <= 0 || (size_t)path_len >= sizeof(expected_path) ||
        strcmp(expected_path, manifest->frame_path) != 0) {
        return ESP_ERR_INVALID_RESPONSE;
    }
    int written = snprintf(url, url_size, "%s%s", client->config.origin, expected_path);
    return written > 0 && (size_t)written < url_size ? ESP_OK : ESP_ERR_INVALID_SIZE;
}

static esp_err_t http_request(const ha_frame_client_t *client, const char *url,
                              const char *method, const uint8_t *request_body,
                              size_t request_length, size_t response_limit,
                              http_body_t *response)
{
    memset(response, 0, sizeof(*response));
    response->capacity = response_limit + 1u;
    response->data = calloc(1, response->capacity);
    if (response->data == NULL) {
        return ESP_ERR_NO_MEM;
    }
    esp_http_client_config_t config = {
        .url = url,
        .method = method[0] == 'G' ? HTTP_METHOD_GET : HTTP_METHOD_POST,
        .timeout_ms = client->config.timeout_ms,
        .buffer_size = BODY_IO_CHUNK,
        .buffer_size_tx = BODY_IO_CHUNK,
        .disable_auto_redirect = true,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .event_handler = http_event_handler,
        .user_data = response,
    };
    esp_http_client_handle_t http = esp_http_client_init(&config);
    if (http == NULL) {
        free(response->data);
        response->data = NULL;
        return ESP_ERR_NO_MEM;
    }
    char authorization[sizeof("Bearer ") + HA_FRAME_KEY_MAX + 1u];
    snprintf(authorization, sizeof(authorization), "Bearer %s", client->config.bearer_key);
    esp_http_client_set_header(http, "Authorization", authorization);
    esp_http_client_set_header(http, "Accept-Encoding", "identity");
    if (request_body != NULL) {
        esp_http_client_set_header(http, "Content-Type", "application/json");
    }
    esp_err_t ret = esp_http_client_open(http, request_length);
    if (ret == ESP_OK && request_body != NULL && request_length > 0) {
        int written = esp_http_client_write(http, (const char *)request_body, request_length);
        if (written != (int)request_length) {
            ret = ESP_FAIL;
        }
    }
    int64_t content_length = -1;
    int64_t deadline_us = esp_timer_get_time() + (int64_t)client->config.timeout_ms * 1000;
    if (ret == ESP_OK) {
        content_length = esp_http_client_fetch_headers(http);
        response->content_length = content_length;
        response->status_code = esp_http_client_get_status_code(http);
        if (content_length > (int64_t)response_limit) {
            ret = ESP_ERR_INVALID_SIZE;
        }
        if (ret == ESP_OK && response->unsupported_encoding) {
            ret = ESP_ERR_NOT_SUPPORTED;
        }
    }
    if (response->status_code == 0) {
        response->status_code = esp_http_client_get_status_code(http);
    }
    while (ret == ESP_OK) {
        if (esp_timer_get_time() > deadline_us) {
            ret = ESP_ERR_TIMEOUT;
            break;
        }
        if (response->length >= response_limit + 1u) {
            ret = ESP_ERR_INVALID_SIZE;
            break;
        }
        int read = esp_http_client_read(http, (char *)response->data + response->length,
                                        response_limit + 1u - response->length);
        if (read < 0) {
            ret = ESP_FAIL;
            break;
        }
        if (read == 0) {
            break;
        }
        response->length += (size_t)read;
        if (response->length > response_limit) {
            ret = ESP_ERR_INVALID_SIZE;
            break;
        }
    }
    if (ret == ESP_OK && !esp_http_client_is_complete_data_received(http)) {
        ret = ESP_ERR_INVALID_RESPONSE;
    }
    esp_http_client_close(http);
    esp_http_client_cleanup(http);
    if (ret == ESP_OK) {
        if (content_length >= 0 && response->length != (size_t)content_length) {
            ret = ESP_ERR_INVALID_RESPONSE;
        }
        response->data[response->length] = '\0';
    }
    return ret;
}

esp_err_t ha_frame_client_init(ha_frame_client_t *client,
                               const ha_frame_client_config_t *config)
{
    if (client == NULL || config == NULL || !valid_origin(config->origin, config->allow_insecure_http) ||
        !valid_device_id(config->device_id) || !string_nonempty(config->bearer_key, HA_FRAME_KEY_MAX) ||
        config->timeout_ms < 1000u || config->timeout_ms > 30000u) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(client, 0, sizeof(*client));
    snprintf(client->config.origin, sizeof(client->config.origin), "%s", config->origin);
    snprintf(client->config.device_id, sizeof(client->config.device_id), "%s", config->device_id);
    snprintf(client->config.bearer_key, sizeof(client->config.bearer_key), "%s", config->bearer_key);
    client->config.allow_insecure_http = config->allow_insecure_http;
    client->config.timeout_ms = config->timeout_ms;
    client->initialized = true;
    return ESP_OK;
}

esp_err_t ha_frame_client_get_manifest(ha_frame_client_t *client,
                                       ha_frame_manifest_t *manifest)
{
    if (client == NULL || manifest == NULL || !client->initialized) {
        return ESP_ERR_INVALID_ARG;
    }
    char url[HA_FRAME_ORIGIN_MAX + HA_FRAME_PATH_MAX];
    esp_err_t ret = build_url(client, API_MANIFEST_SUFFIX, url, sizeof(url));
    if (ret != ESP_OK) {
        return ret;
    }
    http_body_t response;
    ret = http_request(client, url, "GET", NULL, 0, HA_FRAME_MANIFEST_MAX, &response);
    client->last_http_status = response.status_code;
    client->retry_after_seconds = response.retry_after_seconds;
    if (ret == ESP_OK && response.status_code == 200) {
        ret = parse_manifest(response.data, response.length, client->config.device_id, manifest);
    } else if (ret == ESP_OK) {
        ret = response.status_code == 401 ? ESP_ERR_INVALID_STATE : ESP_ERR_INVALID_RESPONSE;
    }
    free(response.data);
    return ret;
}

esp_err_t ha_frame_client_get_frame(ha_frame_client_t *client,
                                    const ha_frame_manifest_t *manifest,
                                    uint8_t *frame, size_t frame_size)
{
    if (client == NULL || manifest == NULL || frame == NULL || frame_size != HA_FRAME_BYTES ||
        !client->initialized || !valid_hex_id(manifest->frame_id) ||
        manifest->byte_length != HA_FRAME_BYTES || manifest->width != HA_FRAME_WIDTH ||
        manifest->height != HA_FRAME_HEIGHT || strcmp(manifest->palette_id, FRAME_STORE_PALETTE_ID) != 0) {
        return ESP_ERR_INVALID_ARG;
    }
    char url[HA_FRAME_ORIGIN_MAX + HA_FRAME_PATH_MAX];
    esp_err_t ret = build_frame_url(client, manifest, url, sizeof(url));
    if (ret != ESP_OK) {
        return ret;
    }
    http_body_t response;
    ret = http_request(client, url, "GET", NULL, 0, HA_FRAME_BYTES, &response);
    client->last_http_status = response.status_code;
    client->retry_after_seconds = response.retry_after_seconds;
    if (ret == ESP_OK) {
        if (response.status_code != 200 || response.length != HA_FRAME_BYTES ||
            !ha_frame_client_packed4_valid(response.data, response.length)) {
            ret = response.status_code == 401 ? ESP_ERR_INVALID_STATE : ESP_ERR_INVALID_RESPONSE;
        } else {
            uint8_t digest[32];
            ret = sha256_frame(response.data, response.length, digest);
            char frame_id[HA_FRAME_ID_BUFFER];
            hex_encode(digest, sizeof(digest), frame_id, sizeof(frame_id));
            if (ret != ESP_OK || strcmp(frame_id, manifest->frame_id) != 0) {
                ret = ESP_ERR_INVALID_CRC;
            } else {
                memcpy(frame, response.data, frame_size);
            }
        }
    }
    free(response.data);
    return ret;
}

static const char *overlay_string(ha_frame_overlay_t overlay)
{
    switch (overlay) {
    case HA_FRAME_OVERLAY_OFFLINE:
        return "offline";
    case HA_FRAME_OVERLAY_TIME_UNKNOWN:
        return "time_unknown";
    default:
        return "none";
    }
}

static const char *result_string(ha_frame_display_result_t result)
{
    switch (result) {
    case HA_FRAME_RESULT_SUCCESS:
        return "success";
    case HA_FRAME_RESULT_SKIPPED:
        return "skipped";
    default:
        return "failed";
    }
}

static void add_iso_or_null(cJSON *object, const char *name, time_t value)
{
    if (value == 0) {
        cJSON_AddNullToObject(object, name);
        return;
    }
    char iso[32];
    struct tm tm_value;
    gmtime_r(&value, &tm_value);
    if (strftime(iso, sizeof(iso), "%Y-%m-%dT%H:%M:%SZ", &tm_value) == 0) {
        cJSON_AddNullToObject(object, name);
    } else {
        cJSON_AddStringToObject(object, name, iso);
    }
}

static esp_err_t build_report(const ha_frame_report_t *report, uint8_t **body,
                              size_t *body_length)
{
    if (report == NULL || !string_nonempty(report->report_id, 128) ||
        !string_nonempty(report->boot_id, 128) || !string_nonempty(report->firmware_version, 64) ||
        (report->received_frame_id[0] != '\0' && !valid_hex_id(report->received_frame_id)) ||
        (report->displayed_frame_id[0] != '\0' && !valid_hex_id(report->displayed_frame_id)) ||
        report->display_result > HA_FRAME_RESULT_FAILED || report->local_overlay > HA_FRAME_OVERLAY_TIME_UNKNOWN) {
        return ESP_ERR_INVALID_ARG;
    }
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return ESP_ERR_NO_MEM;
    }
    cJSON_AddNumberToObject(root, "schema_version", HA_FRAME_SCHEMA_VERSION);
    cJSON_AddStringToObject(root, "report_id", report->report_id);
    cJSON_AddStringToObject(root, "boot_id", report->boot_id);
    cJSON_AddStringToObject(root, "firmware_version", report->firmware_version);
    if (report->received_frame_id[0] == '\0') cJSON_AddNullToObject(root, "received_frame_id");
    else cJSON_AddStringToObject(root, "received_frame_id", report->received_frame_id);
    if (report->displayed_frame_id[0] == '\0') cJSON_AddNullToObject(root, "displayed_frame_id");
    else cJSON_AddStringToObject(root, "displayed_frame_id", report->displayed_frame_id);
    cJSON_AddStringToObject(root, "display_result", result_string(report->display_result));
    add_iso_or_null(root, "display_completed_at", report->display_completed_at);
    cJSON_AddStringToObject(root, "local_overlay", overlay_string(report->local_overlay));
    add_iso_or_null(root, "overlay_source_time", report->overlay_source_time);
    add_iso_or_null(root, "next_wake_at", report->next_wake_at);
    if (report->battery_percent < 0) cJSON_AddNullToObject(root, "battery_percent");
    else cJSON_AddNumberToObject(root, "battery_percent", report->battery_percent);
    if (report->wifi_rssi_dbm == 0) cJSON_AddNullToObject(root, "wifi_rssi_dbm");
    else cJSON_AddNumberToObject(root, "wifi_rssi_dbm", report->wifi_rssi_dbm);
    if (report->error_code[0] != '\0') cJSON_AddStringToObject(root, "error_code", report->error_code);
    char *printed = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (printed == NULL) {
        return ESP_ERR_NO_MEM;
    }
    size_t length = strlen(printed);
    if (length > HA_FRAME_REPORT_MAX) {
        free(printed);
        return ESP_ERR_INVALID_SIZE;
    }
    *body = (uint8_t *)printed;
    *body_length = length;
    return ESP_OK;
}

esp_err_t ha_frame_client_post_report(ha_frame_client_t *client,
                                      const ha_frame_report_t *report)
{
    if (client == NULL || report == NULL || !client->initialized) {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t *request_body = NULL;
    size_t request_length = 0;
    esp_err_t ret = build_report(report, &request_body, &request_length);
    if (ret != ESP_OK) {
        return ret;
    }
    char url[HA_FRAME_ORIGIN_MAX + HA_FRAME_PATH_MAX];
    ret = build_url(client, API_REPORTS_SUFFIX, url, sizeof(url));
    if (ret == ESP_OK) {
        http_body_t response;
        ret = http_request(client, url, "POST", request_body, request_length,
                           HA_FRAME_REPORT_MAX, &response);
        client->last_http_status = response.status_code;
        client->retry_after_seconds = response.retry_after_seconds;
        if (ret == ESP_OK && response.status_code != 200) {
            ret = response.status_code == 401 ? ESP_ERR_INVALID_STATE :
                  response.status_code == 409 ? ESP_ERR_INVALID_STATE : ESP_ERR_INVALID_RESPONSE;
        }
        free(response.data);
    }
    free(request_body);
    return ret;
}
