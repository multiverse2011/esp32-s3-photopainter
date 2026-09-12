/**
 * A/B storage for a validated PhotoPainter 4bpp frame.
 *
 * Caching is enabled by default and uses the photocache partition in the
 * supplied partition table. Disable caching for custom layouts without it.
 */
#ifndef PHOTOPAINTER_FRAME_STORE_H
#define PHOTOPAINTER_FRAME_STORE_H

#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FRAME_STORE_WIDTH 800u
#define FRAME_STORE_HEIGHT 480u
#define FRAME_STORE_FRAME_BYTES ((FRAME_STORE_WIDTH * FRAME_STORE_HEIGHT) / 2u)
#define FRAME_STORE_SHA256_BYTES 32u
#define FRAME_STORE_PALETTE_ID_MAX 31u

/** The provisional palette used by the HA renderer. Hardware is not verified. */
#define FRAME_STORE_PALETTE_ID "spectra6-ws73-v1"

typedef struct {
    uint8_t frame_sha256[FRAME_STORE_SHA256_BYTES];
    char palette_id[FRAME_STORE_PALETTE_ID_MAX + 1u];
    int64_t generated_at;
    int64_t fresh_until;
    uint16_t width;
    uint16_t height;
    uint32_t byte_length;
    uint8_t reserved_status_overlay;
} frame_store_meta_t;

/**
 * Initialize and scan the optional cache partition.
 *
 * Returns ESP_ERR_NOT_SUPPORTED when caching is disabled at build time and
 * ESP_ERR_NOT_FOUND when the configured partition is absent or too small.
 */
esp_err_t frame_store_init(void);

/** Return whether a valid frame is currently stored. */
bool frame_store_has_frame(void);

/** Return metadata for the newest valid frame. */
esp_err_t frame_store_get_meta(frame_store_meta_t *meta);

/** Read the newest frame into an exact FRAME_STORE_FRAME_BYTES buffer. */
esp_err_t frame_store_load(uint8_t *frame, size_t frame_size,
                           frame_store_meta_t *meta);

/**
 * Atomically save a validated frame to the inactive A/B slot.
 *
 * The caller supplies the expected SHA-256 in meta. The implementation checks
 * every packed nibble, verifies the digest after writing, and publishes the
 * commit marker only after the readback succeeds.
 */
esp_err_t frame_store_save(const uint8_t *frame, size_t frame_size,
                           const frame_store_meta_t *meta);

/** Forget the in-memory scan result. The flash contents remain intact. */
void frame_store_deinit(void);

#ifdef __cplusplus
}
#endif

#endif
