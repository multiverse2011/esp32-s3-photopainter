/**
 * @file epd_driver.c
 * @brief E-Paper Display Driver implementation (epaper_port backend)
 */

#include "epd_driver.h"
#include "epaper_port.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include <string.h>

static const char *TAG = "epd_driver";

// State variables
static uint8_t *s_framebuffer = NULL;
static bool s_initialized = false;

esp_err_t epd_driver_init(void)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "EPD driver already initialized");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Initializing EPD driver (epaper_port)");

    // Initialize display driver and propagate SPI/BUSY failures.
    esp_err_t ret = epaper_port_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Display port initialization failed: %s", esp_err_to_name(ret));
        epaper_port_deinit();
        return ret;
    }

    // Allocate framebuffer in PSRAM
    s_framebuffer = heap_caps_malloc(EPD_BUFFER_SIZE, MALLOC_CAP_SPIRAM);
    if (s_framebuffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate framebuffer in PSRAM");
        epaper_port_deinit();
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Framebuffer allocated: %d bytes in PSRAM", EPD_BUFFER_SIZE);

    // Clear framebuffer to white
    memset(s_framebuffer, 0x11, EPD_BUFFER_SIZE);  // 0x11 = white|white

    s_initialized = true;
    ESP_LOGI(TAG, "EPD driver initialized successfully");

    return ESP_OK;
}

uint8_t *epd_driver_get_buffer(void)
{
    return s_framebuffer;
}

size_t epd_driver_get_buffer_size(void)
{
    return EPD_BUFFER_SIZE;
}

esp_err_t epd_driver_clear(epd_color_t color)
{
    if (!s_initialized || s_framebuffer == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    // Pack two pixels of the same color
    uint8_t packed = ((color & 0x07) << 4) | (color & 0x07);
    memset(s_framebuffer, packed, EPD_BUFFER_SIZE);

    ESP_LOGI(TAG, "Framebuffer cleared with color %d", color);
    return ESP_OK;
}

esp_err_t epd_driver_refresh(void)
{
    if (!s_initialized || s_framebuffer == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Starting display refresh (epaper_port)");
    esp_err_t ret = epaper_port_display(s_framebuffer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Display refresh failed: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "Display refresh complete");
    return ESP_OK;
}

esp_err_t epd_driver_sleep(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    return epaper_port_sleep();
}

esp_err_t epd_driver_reset(void)
{
    ESP_LOGI(TAG, "Resetting display (epaper_port)");
    return epaper_port_init();
}

void epd_driver_set_deadline_us(int64_t absolute_deadline_us)
{
    epaper_port_set_deadline_us(absolute_deadline_us);
}

void epd_driver_deinit(void)
{
    if (!s_initialized) {
        return;
    }

    ESP_LOGI(TAG, "Deinitializing EPD driver");

    // Free framebuffer
    if (s_framebuffer) {
        free(s_framebuffer);
        s_framebuffer = NULL;
    }

    epaper_port_deinit();

    s_initialized = false;
    ESP_LOGI(TAG, "EPD driver deinitialized");
}
