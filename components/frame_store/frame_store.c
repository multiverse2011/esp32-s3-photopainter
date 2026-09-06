#include "frame_store.h"

#include "esp_log.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "mbedtls/sha256.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#ifndef CONFIG_PHOTOPAINTER_ENABLE_CACHE
#define CONFIG_PHOTOPAINTER_ENABLE_CACHE 0
#endif

#ifndef CONFIG_PHOTOPAINTER_CACHE_PARTITION
#define CONFIG_PHOTOPAINTER_CACHE_PARTITION "photocache"
#endif

#ifndef CONFIG_PHOTOPAINTER_CACHE_SLOT_SIZE
#define CONFIG_PHOTOPAINTER_CACHE_SLOT_SIZE 262144
#endif

#define TAG "photopainter_store"
#define STORE_MAGIC 0x50504631u /* PPF1 */
#define STORE_VERSION 1u
#define STORE_COMMIT_MARKER 0xA5u
#define STORE_SLOT_COUNT 2u
#define STORE_ERASE_SECTOR 4096u
#define STORE_IO_CHUNK 4096u

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint16_t version;
    uint16_t header_size;
    uint32_t sequence;
    uint32_t byte_length;
    uint16_t width;
    uint16_t height;
    int64_t generated_at;
    int64_t fresh_until;
    uint8_t frame_sha256[FRAME_STORE_SHA256_BYTES];
    char palette_id[FRAME_STORE_PALETTE_ID_MAX + 1u];
    uint8_t reserved_status_overlay;
    uint8_t reserved[3];
    uint32_t header_crc32;
    uint8_t commit_marker;
    uint8_t padding[3];
} frame_store_header_t;

_Static_assert(sizeof(frame_store_header_t) <= STORE_ERASE_SECTOR,
               "frame store header must fit one erase sector");

typedef struct {
    const esp_partition_t *partition;
    SemaphoreHandle_t lock;
    uint32_t slot_size;
    int active_slot;
    uint32_t active_sequence;
    frame_store_meta_t active_meta;
    bool initialized;
} frame_store_state_t;

static frame_store_state_t s_store;

static uint32_t crc32_update(uint32_t crc, const uint8_t *data, size_t len)
{
    crc = ~crc;
    while (len-- > 0) {
        crc ^= *data++;
        for (unsigned bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ (0xEDB88320u & (uint32_t)-(int32_t)(crc & 1u));
        }
    }
    return ~crc;
}

static uint32_t header_crc(const frame_store_header_t *header)
{
    frame_store_header_t copy = *header;
    copy.header_crc32 = 0;
    copy.commit_marker = 0;
    return crc32_update(0, (const uint8_t *)&copy, offsetof(frame_store_header_t, commit_marker));
}

static bool digest_equal(const uint8_t a[FRAME_STORE_SHA256_BYTES],
                         const uint8_t b[FRAME_STORE_SHA256_BYTES])
{
    uint8_t diff = 0;
    for (size_t i = 0; i < FRAME_STORE_SHA256_BYTES; ++i) {
        diff |= (uint8_t)(a[i] ^ b[i]);
    }
    return diff == 0;
}

static bool packed_frame_valid(const uint8_t *frame, size_t length)
{
    if (frame == NULL || length != FRAME_STORE_FRAME_BYTES) {
        return false;
    }
    for (size_t i = 0; i < length; ++i) {
        uint8_t value = frame[i];
        uint8_t high = value >> 4;
        uint8_t low = value & 0x0Fu;
        if (high == 4u || high == 7u || high > 7u ||
            low == 4u || low == 7u || low > 7u) {
            return false;
        }
    }
    return true;
}

static bool packed_chunk_valid(const uint8_t *frame, size_t length)
{
    for (size_t i = 0; i < length; ++i) {
        uint8_t high = frame[i] >> 4;
        uint8_t low = frame[i] & 0x0Fu;
        if (high == 4u || high == 7u || high > 7u ||
            low == 4u || low == 7u || low > 7u) {
            return false;
        }
    }
    return true;
}

static esp_err_t sha256_buffer(const uint8_t *data, size_t length,
                               uint8_t digest[FRAME_STORE_SHA256_BYTES])
{
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    int ret = mbedtls_sha256_starts(&ctx, 0);
    if (ret == 0) {
        ret = mbedtls_sha256_update(&ctx, data, length);
    }
    if (ret == 0) {
        ret = mbedtls_sha256_finish(&ctx, digest);
    }
    mbedtls_sha256_free(&ctx);
    return ret == 0 ? ESP_OK : ESP_FAIL;
}

static esp_err_t sha256_partition(const esp_partition_t *partition,
                                  size_t offset, size_t length,
                                  uint8_t digest[FRAME_STORE_SHA256_BYTES])
{
    uint8_t *buffer = malloc(STORE_IO_CHUNK);
    if (buffer == NULL) {
        return ESP_ERR_NO_MEM;
    }
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    int ret = mbedtls_sha256_starts(&ctx, 0);
    size_t cursor = 0;
    while (ret == 0 && cursor < length) {
        size_t chunk = length - cursor;
        if (chunk > STORE_IO_CHUNK) {
            chunk = STORE_IO_CHUNK;
        }
        esp_err_t read_ret = esp_partition_read(partition, offset + cursor, buffer, chunk);
        if (read_ret != ESP_OK) {
            ret = -1;
            break;
        }
        ret = mbedtls_sha256_update(&ctx, buffer, chunk);
        cursor += chunk;
    }
    if (ret == 0) {
        ret = mbedtls_sha256_finish(&ctx, digest);
    }
    mbedtls_sha256_free(&ctx);
    free(buffer);
    return ret == 0 ? ESP_OK : ESP_FAIL;
}

static esp_err_t validate_partition_frame(const esp_partition_t *partition,
                                           size_t offset, size_t length,
                                           uint8_t digest[FRAME_STORE_SHA256_BYTES])
{
    uint8_t *buffer = malloc(STORE_IO_CHUNK);
    if (buffer == NULL) {
        return ESP_ERR_NO_MEM;
    }
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    int ret = mbedtls_sha256_starts(&ctx, 0);
    size_t cursor = 0;
    while (ret == 0 && cursor < length) {
        size_t chunk = length - cursor;
        if (chunk > STORE_IO_CHUNK) {
            chunk = STORE_IO_CHUNK;
        }
        if (esp_partition_read(partition, offset + cursor, buffer, chunk) != ESP_OK ||
            !packed_chunk_valid(buffer, chunk)) {
            ret = -1;
            break;
        }
        ret = mbedtls_sha256_update(&ctx, buffer, chunk);
        cursor += chunk;
    }
    if (ret == 0) {
        ret = mbedtls_sha256_finish(&ctx, digest);
    }
    mbedtls_sha256_free(&ctx);
    free(buffer);
    return ret == 0 ? ESP_OK : ESP_ERR_INVALID_CRC;
}

static void header_to_meta(const frame_store_header_t *header,
                           frame_store_meta_t *meta)
{
    memset(meta, 0, sizeof(*meta));
    memcpy(meta->frame_sha256, header->frame_sha256, sizeof(meta->frame_sha256));
    memcpy(meta->palette_id, header->palette_id, sizeof(meta->palette_id));
    meta->palette_id[FRAME_STORE_PALETTE_ID_MAX] = '\0';
    meta->generated_at = header->generated_at;
    meta->fresh_until = header->fresh_until;
    meta->width = header->width;
    meta->height = header->height;
    meta->byte_length = header->byte_length;
    meta->reserved_status_overlay = header->reserved_status_overlay;
}

static bool header_is_well_formed(const frame_store_header_t *header)
{
    return header->magic == STORE_MAGIC &&
           header->version == STORE_VERSION &&
           header->header_size == sizeof(frame_store_header_t) &&
           header->commit_marker == STORE_COMMIT_MARKER &&
           header->byte_length == FRAME_STORE_FRAME_BYTES &&
           header->width == FRAME_STORE_WIDTH &&
           header->height == FRAME_STORE_HEIGHT &&
           strnlen(header->palette_id, sizeof(header->palette_id)) < sizeof(header->palette_id) &&
           strcmp(header->palette_id, FRAME_STORE_PALETTE_ID) == 0 &&
           header->reserved_status_overlay <= 2u &&
           header->header_crc32 == header_crc(header);
}

static esp_err_t scan_slot(unsigned slot, frame_store_header_t *header)
{
    size_t offset = (size_t)slot * s_store.slot_size;
    esp_err_t ret = esp_partition_read(s_store.partition, offset, header, sizeof(*header));
    if (ret != ESP_OK || !header_is_well_formed(header)) {
        return ESP_ERR_NOT_FOUND;
    }
    uint8_t digest[FRAME_STORE_SHA256_BYTES];
    ret = validate_partition_frame(s_store.partition, offset + sizeof(*header),
                                   FRAME_STORE_FRAME_BYTES, digest);
    if (ret != ESP_OK || !digest_equal(digest, header->frame_sha256)) {
        return ESP_ERR_INVALID_CRC;
    }
    return ESP_OK;
}

static bool sequence_is_newer(uint32_t candidate, uint32_t current)
{
    return (int32_t)(candidate - current) > 0;
}

static esp_err_t scan_locked(void)
{
    s_store.active_slot = -1;
    s_store.active_sequence = 0;
    memset(&s_store.active_meta, 0, sizeof(s_store.active_meta));

    for (unsigned slot = 0; slot < STORE_SLOT_COUNT; ++slot) {
        frame_store_header_t header;
        if (scan_slot(slot, &header) != ESP_OK) {
            continue;
        }
        if (s_store.active_slot < 0 || sequence_is_newer(header.sequence, s_store.active_sequence)) {
            s_store.active_slot = (int)slot;
            s_store.active_sequence = header.sequence;
            header_to_meta(&header, &s_store.active_meta);
        }
    }
    return s_store.active_slot >= 0 ? ESP_OK : ESP_ERR_NOT_FOUND;
}

esp_err_t frame_store_init(void)
{
#if !CONFIG_PHOTOPAINTER_ENABLE_CACHE
    return ESP_ERR_NOT_SUPPORTED;
#else
    if (s_store.initialized) {
        return ESP_OK;
    }
    memset(&s_store, 0, sizeof(s_store));
    s_store.active_slot = -1;
    s_store.slot_size = CONFIG_PHOTOPAINTER_CACHE_SLOT_SIZE;
    if ((s_store.slot_size % STORE_ERASE_SECTOR) != 0 ||
        s_store.slot_size < sizeof(frame_store_header_t) + FRAME_STORE_FRAME_BYTES) {
        return ESP_ERR_INVALID_SIZE;
    }
    s_store.partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
                                                   ESP_PARTITION_SUBTYPE_ANY,
                                                   CONFIG_PHOTOPAINTER_CACHE_PARTITION);
    if (s_store.partition == NULL || s_store.partition->size < STORE_SLOT_COUNT * s_store.slot_size) {
        return ESP_ERR_NOT_FOUND;
    }
    s_store.lock = xSemaphoreCreateMutex();
    if (s_store.lock == NULL) {
        return ESP_ERR_NO_MEM;
    }
    s_store.initialized = true;
    esp_err_t ret = scan_locked();
    if (ret != ESP_OK && ret != ESP_ERR_NOT_FOUND) {
        ESP_LOGW(TAG, "cache scan failed: %s", esp_err_to_name(ret));
    }
    ESP_LOGI(TAG, "cache %s; partition=%s size=%lu", ret == ESP_OK ? "ready" : "empty",
             s_store.partition->label, (unsigned long)s_store.partition->size);
    return ESP_OK;
#endif
}

bool frame_store_has_frame(void)
{
#if !CONFIG_PHOTOPAINTER_ENABLE_CACHE
    return false;
#else
    return s_store.initialized && s_store.active_slot >= 0;
#endif
}

esp_err_t frame_store_get_meta(frame_store_meta_t *meta)
{
#if !CONFIG_PHOTOPAINTER_ENABLE_CACHE
    (void)meta;
    return ESP_ERR_NOT_SUPPORTED;
#else
    if (meta == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_store.initialized || s_store.active_slot < 0) {
        return ESP_ERR_NOT_FOUND;
    }
    if (s_store.lock != NULL) {
        xSemaphoreTake(s_store.lock, portMAX_DELAY);
    }
    *meta = s_store.active_meta;
    if (s_store.lock != NULL) {
        xSemaphoreGive(s_store.lock);
    }
    return ESP_OK;
#endif
}

esp_err_t frame_store_load(uint8_t *frame, size_t frame_size,
                           frame_store_meta_t *meta)
{
#if !CONFIG_PHOTOPAINTER_ENABLE_CACHE
    (void)frame;
    (void)frame_size;
    (void)meta;
    return ESP_ERR_NOT_SUPPORTED;
#else
    if (frame == NULL || meta == NULL || frame_size != FRAME_STORE_FRAME_BYTES) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_store.initialized || s_store.active_slot < 0) {
        return ESP_ERR_NOT_FOUND;
    }
    xSemaphoreTake(s_store.lock, portMAX_DELAY);
    size_t offset = (size_t)s_store.active_slot * s_store.slot_size + sizeof(frame_store_header_t);
    esp_err_t ret = esp_partition_read(s_store.partition, offset, frame, frame_size);
    if (ret == ESP_OK && !packed_frame_valid(frame, frame_size)) {
        ret = ESP_ERR_INVALID_CRC;
    }
    if (ret == ESP_OK) {
        uint8_t digest[FRAME_STORE_SHA256_BYTES];
        ret = sha256_buffer(frame, frame_size, digest);
        if (ret == ESP_OK && !digest_equal(digest, s_store.active_meta.frame_sha256)) {
            ret = ESP_ERR_INVALID_CRC;
        }
    }
    if (ret == ESP_OK) {
        *meta = s_store.active_meta;
    }
    xSemaphoreGive(s_store.lock);
    return ret;
#endif
}

esp_err_t frame_store_save(const uint8_t *frame, size_t frame_size,
                           const frame_store_meta_t *meta)
{
#if !CONFIG_PHOTOPAINTER_ENABLE_CACHE
    (void)frame;
    (void)frame_size;
    (void)meta;
    return ESP_ERR_NOT_SUPPORTED;
#else
    if (frame == NULL || meta == NULL || !packed_frame_valid(frame, frame_size) ||
        meta->width != FRAME_STORE_WIDTH || meta->height != FRAME_STORE_HEIGHT ||
        meta->byte_length != FRAME_STORE_FRAME_BYTES ||
        strnlen(meta->palette_id, sizeof(meta->palette_id)) >= sizeof(meta->palette_id) ||
        strcmp(meta->palette_id, FRAME_STORE_PALETTE_ID) != 0 ||
        meta->reserved_status_overlay > 2u) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_store.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    uint8_t digest[FRAME_STORE_SHA256_BYTES];
    esp_err_t ret = sha256_buffer(frame, frame_size, digest);
    if (ret != ESP_OK || !digest_equal(digest, meta->frame_sha256)) {
        return ESP_ERR_INVALID_CRC;
    }

    xSemaphoreTake(s_store.lock, portMAX_DELAY);
    unsigned slot = s_store.active_slot == 0 ? 1u : 0u;
    uint32_t sequence = s_store.active_slot < 0 ? 1u : s_store.active_sequence + 1u;
    size_t slot_offset = (size_t)slot * s_store.slot_size;
    ret = esp_partition_erase_range(s_store.partition, slot_offset, s_store.slot_size);
    if (ret != ESP_OK) {
        goto done;
    }

    frame_store_header_t header;
    memset(&header, 0, sizeof(header));
    header.magic = STORE_MAGIC;
    header.version = STORE_VERSION;
    header.header_size = sizeof(header);
    header.sequence = sequence;
    header.byte_length = meta->byte_length;
    header.width = meta->width;
    header.height = meta->height;
    header.generated_at = meta->generated_at;
    header.fresh_until = meta->fresh_until;
    memcpy(header.frame_sha256, meta->frame_sha256, sizeof(header.frame_sha256));
    memcpy(header.palette_id, meta->palette_id, sizeof(header.palette_id));
    header.reserved_status_overlay = meta->reserved_status_overlay;
    header.header_crc32 = header_crc(&header);
    /* NOR flash can only clear bits without erase; publish from erased 0xff. */
    header.commit_marker = 0xFFu;
    ret = esp_partition_write(s_store.partition, slot_offset, &header, sizeof(header));
    if (ret != ESP_OK) {
        goto done;
    }
    for (size_t cursor = 0; cursor < frame_size && ret == ESP_OK;) {
        size_t chunk = frame_size - cursor;
        if (chunk > STORE_IO_CHUNK) {
            chunk = STORE_IO_CHUNK;
        }
        ret = esp_partition_write(s_store.partition, slot_offset + sizeof(header) + cursor,
                                  frame + cursor, chunk);
        cursor += chunk;
    }
    if (ret != ESP_OK) {
        goto done;
    }
    ret = validate_partition_frame(s_store.partition, slot_offset + sizeof(header), frame_size, digest);
    if (ret != ESP_OK || !digest_equal(digest, meta->frame_sha256)) {
        ret = ESP_ERR_INVALID_CRC;
        goto done;
    }
    uint8_t marker = STORE_COMMIT_MARKER;
    ret = esp_partition_write(s_store.partition,
                              slot_offset + offsetof(frame_store_header_t, commit_marker),
                              &marker, sizeof(marker));
    if (ret == ESP_OK) {
        header.commit_marker = STORE_COMMIT_MARKER;
        frame_store_header_t readback_header;
        ret = esp_partition_read(s_store.partition, slot_offset, &readback_header,
                                 sizeof(readback_header));
        if (ret == ESP_OK && (!header_is_well_formed(&readback_header) ||
                              !digest_equal(readback_header.frame_sha256, meta->frame_sha256))) {
            ret = ESP_ERR_INVALID_CRC;
        }
        if (ret == ESP_OK) {
            s_store.active_slot = (int)slot;
            s_store.active_sequence = sequence;
            header_to_meta(&readback_header, &s_store.active_meta);
        }
    }
done:
    xSemaphoreGive(s_store.lock);
    return ret;
#endif
}

void frame_store_deinit(void)
{
#if CONFIG_PHOTOPAINTER_ENABLE_CACHE
    if (s_store.lock != NULL) {
        vSemaphoreDelete(s_store.lock);
    }
#endif
    memset(&s_store, 0, sizeof(s_store));
    s_store.active_slot = -1;
}
