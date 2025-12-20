/**
 * @file epd_driver.c
 * @brief E-Paper Display Driver implementation for Waveshare 7.3inch 7-color E-Paper
 */

#include "epd_driver.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "epd_driver";

// Pin definitions for Waveshare ESP32-S3 PhotoPainter
#define EPD_PIN_DC    8
#define EPD_PIN_CS    9
#define EPD_PIN_RST   12
#define EPD_PIN_BUSY  13

// Timing constants
#define EPD_BUSY_TIMEOUT_MS  20000   // 20 seconds for refresh (Task T081)
#define EPD_RESET_DELAY_MS   200
#define EPD_MAX_RETRIES      2       // Auto-reset retry count (Task T082)

// Display commands (Waveshare 7.3inch 7-color e-Paper)
#define CMD_PANEL_SETTING        0x00
#define CMD_POWER_SETTING        0x01
#define CMD_POWER_OFF            0x02
#define CMD_POWER_OFF_SEQ        0x03
#define CMD_POWER_ON             0x04
#define CMD_POWER_ON_MEASURE     0x05
#define CMD_BOOSTER_SOFT_START   0x06
#define CMD_DEEP_SLEEP           0x07
#define CMD_DTM1                 0x08
#define CMD_DATA_START_TRANS     0x10
#define CMD_DISPLAY_REFRESH      0x12
#define CMD_PLL_CONTROL          0x30
#define CMD_TEMP_CALIB           0x41
#define CMD_VCOM_INTERVAL        0x50
#define CMD_TCON_SETTING         0x60
#define CMD_RESOLUTION_SETTING   0x61
#define CMD_GET_STATUS           0x71
#define CMD_VCOM_DC              0x82
#define CMD_PARTIAL_IN           0x84
#define CMD_CMDH                 0xAA
#define CMD_CASCADE              0xE3

// External SPI functions
extern esp_err_t epd_spi_init(void);
extern void epd_spi_deinit(void);
extern void epd_spi_send_command(uint8_t cmd);
extern void epd_spi_send_data(uint8_t data);
extern void epd_spi_send_data_burst(const uint8_t *data, size_t len);

// State variables
static uint8_t *s_framebuffer = NULL;
static bool s_initialized = false;

/**
 * @brief Configure GPIO pins
 */
static esp_err_t epd_gpio_init(void)
{
    ESP_LOGI(TAG, "Initializing GPIO pins");

    // Configure DC pin (output)
    gpio_config_t dc_conf = {
        .pin_bit_mask = (1ULL << EPD_PIN_DC),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&dc_conf);

    // Configure RST pin (output)
    gpio_config_t rst_conf = {
        .pin_bit_mask = (1ULL << EPD_PIN_RST),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&rst_conf);

    // Configure BUSY pin (input with pull-down, matching reference project)
    gpio_config_t busy_conf = {
        .pin_bit_mask = (1ULL << EPD_PIN_BUSY),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&busy_conf);

    // Set initial states
    gpio_set_level(EPD_PIN_DC, 0);
    gpio_set_level(EPD_PIN_RST, 1);

    return ESP_OK;
}

/**
 * @brief Wait for BUSY pin to go HIGH (display ready)
 * Note: Waveshare 7.3inch 7-color e-Paper uses BUSY=HIGH as ready signal
 */
static esp_err_t epd_wait_busy(uint32_t timeout_ms)
{
    int initial_level = gpio_get_level(EPD_PIN_BUSY);
    ESP_LOGI(TAG, "Waiting for BUSY HIGH (current=%d, timeout=%lums)", initial_level, timeout_ms);

    TickType_t start = xTaskGetTickCount();
    TickType_t timeout_ticks = pdMS_TO_TICKS(timeout_ms);

    while (gpio_get_level(EPD_PIN_BUSY) == 0) {
        if ((xTaskGetTickCount() - start) > timeout_ticks) {
            ESP_LOGE(TAG, "BUSY timeout after %lu ms (still LOW)", timeout_ms);
            return ESP_ERR_TIMEOUT;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    uint32_t elapsed = (xTaskGetTickCount() - start) * portTICK_PERIOD_MS;
    ESP_LOGI(TAG, "BUSY HIGH after %lu ms", elapsed);
    return ESP_OK;
}

/**
 * @brief Hardware reset sequence (matching reference project timing)
 */
static void epd_hw_reset(void)
{
    ESP_LOGI(TAG, "Performing hardware reset");

    gpio_set_level(EPD_PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(50));
    gpio_set_level(EPD_PIN_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(20));
    gpio_set_level(EPD_PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(50));
}

/**
 * @brief Send display initialization sequence (matching reference project)
 */
static void epd_init_sequence(void)
{
    ESP_LOGI(TAG, "Sending initialization sequence");

    // CMDH - Command header
    epd_spi_send_command(CMD_CMDH);  // 0xAA
    epd_spi_send_data(0x49);
    epd_spi_send_data(0x55);
    epd_spi_send_data(0x20);
    epd_spi_send_data(0x08);
    epd_spi_send_data(0x09);
    epd_spi_send_data(0x18);

    // Power setting
    epd_spi_send_command(CMD_POWER_SETTING);  // 0x01
    epd_spi_send_data(0x3F);

    // Panel setting
    epd_spi_send_command(CMD_PANEL_SETTING);  // 0x00
    epd_spi_send_data(0x5F);
    epd_spi_send_data(0x69);

    // Power off sequence setting
    epd_spi_send_command(CMD_POWER_OFF_SEQ);  // 0x03
    epd_spi_send_data(0x00);
    epd_spi_send_data(0x54);
    epd_spi_send_data(0x00);
    epd_spi_send_data(0x44);

    // Power on measure
    epd_spi_send_command(CMD_POWER_ON_MEASURE);  // 0x05
    epd_spi_send_data(0x40);
    epd_spi_send_data(0x1F);
    epd_spi_send_data(0x1F);
    epd_spi_send_data(0x2C);

    // Booster soft start
    epd_spi_send_command(CMD_BOOSTER_SOFT_START);  // 0x06
    epd_spi_send_data(0x6F);
    epd_spi_send_data(0x1F);
    epd_spi_send_data(0x17);
    epd_spi_send_data(0x49);

    // DTM1
    epd_spi_send_command(CMD_DTM1);  // 0x08
    epd_spi_send_data(0x6F);
    epd_spi_send_data(0x1F);
    epd_spi_send_data(0x1F);
    epd_spi_send_data(0x22);

    // PLL control
    epd_spi_send_command(CMD_PLL_CONTROL);  // 0x30
    epd_spi_send_data(0x03);

    // VCOM and data interval setting
    epd_spi_send_command(CMD_VCOM_INTERVAL);  // 0x50
    epd_spi_send_data(0x3F);

    // TCON setting
    epd_spi_send_command(CMD_TCON_SETTING);  // 0x60
    epd_spi_send_data(0x02);
    epd_spi_send_data(0x00);

    // Resolution setting: 800x480
    epd_spi_send_command(CMD_RESOLUTION_SETTING);  // 0x61
    epd_spi_send_data(0x03);  // 800 >> 8
    epd_spi_send_data(0x20);  // 800 & 0xFF
    epd_spi_send_data(0x01);  // 480 >> 8
    epd_spi_send_data(0xE0);  // 480 & 0xFF

    // Partial in
    epd_spi_send_command(CMD_PARTIAL_IN);  // 0x84
    epd_spi_send_data(0x01);

    // Cascade setting
    epd_spi_send_command(CMD_CASCADE);  // 0xE3
    epd_spi_send_data(0x2F);

    // Power on
    epd_spi_send_command(CMD_POWER_ON);  // 0x04
    epd_wait_busy(5000);

    ESP_LOGI(TAG, "Initialization sequence complete");
}

esp_err_t epd_driver_init(void)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "EPD driver already initialized");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Initializing EPD driver");

    // Initialize GPIO
    esp_err_t ret = epd_gpio_init();
    if (ret != ESP_OK) {
        return ret;
    }

    // Initialize SPI
    ret = epd_spi_init();
    if (ret != ESP_OK) {
        return ret;
    }

    // Allocate framebuffer in PSRAM
    s_framebuffer = heap_caps_malloc(EPD_BUFFER_SIZE, MALLOC_CAP_SPIRAM);
    if (s_framebuffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate framebuffer in PSRAM");
        epd_spi_deinit();
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Framebuffer allocated: %d bytes in PSRAM", EPD_BUFFER_SIZE);

    // Clear framebuffer to white
    memset(s_framebuffer, 0x11, EPD_BUFFER_SIZE);  // 0x11 = white|white

    // Hardware reset and initialization (matching reference project)
    epd_hw_reset();
    epd_wait_busy(5000);  // Wait for BUSY HIGH after reset
    vTaskDelay(pdMS_TO_TICKS(50));
    epd_init_sequence();

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

/**
 * @brief Internal refresh function (single attempt, matching reference project)
 */
static esp_err_t epd_refresh_internal(void)
{
    ESP_LOGI(TAG, "Sending data transmission command (0x10)");
    // Start data transmission (0x10)
    epd_spi_send_command(CMD_DATA_START_TRANS);

    ESP_LOGI(TAG, "Sending framebuffer (%d bytes)", EPD_BUFFER_SIZE);
    // Send framebuffer (4bpp packed)
    epd_spi_send_data_burst(s_framebuffer, EPD_BUFFER_SIZE);

    ESP_LOGI(TAG, "Sending POWER_ON command (0x04)");
    // Power on (0x04)
    epd_spi_send_command(CMD_POWER_ON);
    esp_err_t ret = epd_wait_busy(5000);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "POWER_ON busy timeout");
        return ret;
    }

    ESP_LOGI(TAG, "Sending BOOSTER_SOFT_START (0x06)");
    // Booster soft start - second setting (0x06)
    epd_spi_send_command(CMD_BOOSTER_SOFT_START);
    epd_spi_send_data(0x6F);
    epd_spi_send_data(0x1F);
    epd_spi_send_data(0x17);
    epd_spi_send_data(0x49);

    ESP_LOGI(TAG, "Sending DISPLAY_REFRESH (0x12)");
    // Display refresh (0x12)
    epd_spi_send_command(CMD_DISPLAY_REFRESH);
    epd_spi_send_data(0x00);

    ESP_LOGI(TAG, "Waiting for refresh complete (up to %d ms)...", EPD_BUSY_TIMEOUT_MS);
    // Wait for refresh to complete
    ret = epd_wait_busy(EPD_BUSY_TIMEOUT_MS);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "DISPLAY_REFRESH busy timeout");
        return ret;
    }

    ESP_LOGI(TAG, "Sending POWER_OFF (0x02)");
    // Power off (0x02)
    epd_spi_send_command(CMD_POWER_OFF);
    epd_spi_send_data(0x00);
    ret = epd_wait_busy(5000);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "POWER_OFF busy timeout");
        return ret;
    }

    ESP_LOGI(TAG, "Refresh internal complete");
    return ESP_OK;
}

esp_err_t epd_driver_refresh(void)
{
    if (!s_initialized || s_framebuffer == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Starting display refresh");

    esp_err_t ret;

    // Retry loop with auto-reset on timeout (Task T082)
    for (int attempt = 0; attempt <= EPD_MAX_RETRIES; attempt++) {
        if (attempt > 0) {
            ESP_LOGW(TAG, "Refresh retry %d/%d after BUSY timeout - performing auto-reset",
                     attempt, EPD_MAX_RETRIES);
            epd_hw_reset();
            epd_init_sequence();
        }

        ret = epd_refresh_internal();

        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Display refresh complete%s",
                     attempt > 0 ? " (after retry)" : "");
            return ESP_OK;
        }

        if (ret != ESP_ERR_TIMEOUT) {
            // Non-timeout error, don't retry
            ESP_LOGE(TAG, "Display refresh failed: %s", esp_err_to_name(ret));
            return ret;
        }

        ESP_LOGW(TAG, "Display refresh timeout on attempt %d", attempt + 1);
    }

    ESP_LOGE(TAG, "Display refresh failed after %d retries", EPD_MAX_RETRIES);
    return ESP_ERR_TIMEOUT;
}

esp_err_t epd_driver_sleep(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Putting display into sleep mode");

    // Power off
    epd_spi_send_command(CMD_POWER_OFF);
    epd_wait_busy(5000);

    // Deep sleep
    epd_spi_send_command(CMD_DEEP_SLEEP);
    epd_spi_send_data(0xA5);  // Check code

    return ESP_OK;
}

esp_err_t epd_driver_reset(void)
{
    ESP_LOGI(TAG, "Resetting display");

    epd_hw_reset();
    epd_init_sequence();

    return ESP_OK;
}

void epd_driver_deinit(void)
{
    if (!s_initialized) {
        return;
    }

    ESP_LOGI(TAG, "Deinitializing EPD driver");

    // Put display to sleep first
    epd_driver_sleep();

    // Free framebuffer
    if (s_framebuffer) {
        free(s_framebuffer);
        s_framebuffer = NULL;
    }

    // Deinit SPI
    epd_spi_deinit();

    s_initialized = false;
    ESP_LOGI(TAG, "EPD driver deinitialized");
}
