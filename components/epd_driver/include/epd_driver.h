/**
 * @file epd_driver.h
 * @brief E-Paper Display Driver for Waveshare 7.3inch 7-color E-Paper
 *
 * Driver for ACeP (Advanced Color ePaper) 7-color display at 800x480 resolution.
 */

#ifndef EPD_DRIVER_H
#define EPD_DRIVER_H

#include "esp_err.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Display dimensions */
#define EPD_WIDTH  800
#define EPD_HEIGHT 480

/** Buffer size: 4 bits per pixel, 2 pixels per byte */
#define EPD_BUFFER_SIZE ((EPD_WIDTH * EPD_HEIGHT) / 2)

/** 7-color palette */
typedef enum {
    EPD_COLOR_BLACK  = 0,
    EPD_COLOR_WHITE  = 1,
    EPD_COLOR_GREEN  = 2,
    EPD_COLOR_BLUE   = 3,
    EPD_COLOR_RED    = 4,
    EPD_COLOR_YELLOW = 5,
    EPD_COLOR_ORANGE = 6
} epd_color_t;

/**
 * @brief Initialize E-Paper display driver
 *
 * Configures SPI, GPIO, and sends display initialization sequence.
 *
 * @return ESP_OK on success
 * @return ESP_ERR_NO_MEM if buffer allocation fails
 * @return ESP_ERR_INVALID_STATE if already initialized
 */
esp_err_t epd_driver_init(void);

/**
 * @brief Get pointer to framebuffer
 *
 * Returns the PSRAM-allocated framebuffer for direct pixel manipulation.
 * Buffer uses 4-bit per pixel packing (2 pixels per byte).
 *
 * @return Pointer to framebuffer, or NULL if not initialized
 */
uint8_t *epd_driver_get_buffer(void);

/**
 * @brief Get framebuffer size in bytes
 *
 * @return Buffer size (192000 bytes for 800x480 @ 4bpp)
 */
size_t epd_driver_get_buffer_size(void);

/**
 * @brief Clear framebuffer with specified color
 *
 * @param color Fill color (epd_color_t)
 * @return ESP_OK on success
 */
esp_err_t epd_driver_clear(epd_color_t color);

/**
 * @brief Transfer framebuffer to display and refresh
 *
 * Sends buffer via SPI and triggers full display refresh.
 * Blocks until refresh complete (15-20 seconds) or timeout.
 *
 * @return ESP_OK on success
 * @return ESP_ERR_TIMEOUT if BUSY signal times out (>20s)
 */
esp_err_t epd_driver_refresh(void);

/**
 * @brief Put display into sleep mode
 *
 * Reduces display power consumption. Call before deep sleep.
 *
 * @return ESP_OK on success
 */
esp_err_t epd_driver_sleep(void);

/**
 * @brief Hardware reset the display
 *
 * Toggles RST pin and reinitializes display.
 * Use for error recovery.
 *
 * @return ESP_OK on success
 */
esp_err_t epd_driver_reset(void);

/**
 * @brief Deinitialize EPD driver
 *
 * Releases SPI and GPIO resources. Frees framebuffer.
 */
void epd_driver_deinit(void);

#ifdef __cplusplus
}
#endif

#endif // EPD_DRIVER_H
