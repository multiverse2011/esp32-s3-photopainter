/**
 * @file epd_spi.c
 * @brief SPI communication for E-Paper Display
 */

#include "epd_driver.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "epd_spi";

// Pin definitions for Waveshare ESP32-S3 PhotoPainter
#define EPD_PIN_DC    8
#define EPD_PIN_CS    9
#define EPD_PIN_SCK   10
#define EPD_PIN_MOSI  11
#define EPD_PIN_RST   12
#define EPD_PIN_BUSY  13

// SPI Configuration
#define EPD_SPI_HOST  SPI3_HOST
#define EPD_SPI_CLOCK_HZ  (10 * 1000 * 1000)  // 10 MHz
#define EPD_DMA_CHANNEL   SPI_DMA_CH_AUTO

// DMA buffer size
#define DMA_BUFFER_SIZE   4096

static spi_device_handle_t s_spi_handle = NULL;
static uint8_t *s_dma_buffer = NULL;

/**
 * @brief Initialize CS pin as GPIO for manual control
 */
static void epd_cs_gpio_init(void)
{
    gpio_config_t cs_conf = {
        .pin_bit_mask = (1ULL << EPD_PIN_CS),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&cs_conf);
    gpio_set_level(EPD_PIN_CS, 1);  // CS idle high
}

/**
 * @brief Initialize SPI bus and device
 */
esp_err_t epd_spi_init(void)
{
    ESP_LOGI(TAG, "Initializing SPI bus");

    // Initialize CS GPIO for manual control
    epd_cs_gpio_init();

    // SPI bus configuration
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = EPD_PIN_MOSI,
        .miso_io_num = -1,  // Not used
        .sclk_io_num = EPD_PIN_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = EPD_WIDTH * EPD_HEIGHT,  // Match reference project
    };

    // Initialize SPI bus
    esp_err_t ret = spi_bus_initialize(EPD_SPI_HOST, &bus_cfg, EPD_DMA_CHANNEL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus: %s", esp_err_to_name(ret));
        return ret;
    }

    // SPI device configuration - manual CS control, half-duplex mode
    spi_device_interface_config_t dev_cfg = {
        .clock_speed_hz = EPD_SPI_CLOCK_HZ,
        .mode = 0,  // SPI Mode 0 (CPOL=0, CPHA=0)
        .spics_io_num = -1,  // Manual CS control
        .queue_size = 7,
        .flags = SPI_DEVICE_HALFDUPLEX,
    };

    // Add SPI device
    ret = spi_bus_add_device(EPD_SPI_HOST, &dev_cfg, &s_spi_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add SPI device: %s", esp_err_to_name(ret));
        spi_bus_free(EPD_SPI_HOST);
        return ret;
    }

    // Allocate DMA buffer in internal RAM
    s_dma_buffer = heap_caps_malloc(DMA_BUFFER_SIZE, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (s_dma_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate DMA buffer");
        spi_bus_remove_device(s_spi_handle);
        spi_bus_free(EPD_SPI_HOST);
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "SPI initialized successfully");
    return ESP_OK;
}

/**
 * @brief Deinitialize SPI
 */
void epd_spi_deinit(void)
{
    if (s_dma_buffer) {
        free(s_dma_buffer);
        s_dma_buffer = NULL;
    }

    if (s_spi_handle) {
        spi_bus_remove_device(s_spi_handle);
        s_spi_handle = NULL;
    }

    spi_bus_free(EPD_SPI_HOST);
    ESP_LOGI(TAG, "SPI deinitialized");
}

/**
 * @brief Send command byte to display (with manual CS control)
 */
void epd_spi_send_command(uint8_t cmd)
{
    gpio_set_level(EPD_PIN_DC, 0);  // Command mode
    gpio_set_level(EPD_PIN_CS, 0);  // CS active

    spi_transaction_t trans = {
        .length = 8,
        .tx_buffer = &cmd,
    };

    spi_device_polling_transmit(s_spi_handle, &trans);

    gpio_set_level(EPD_PIN_CS, 1);  // CS idle
}

/**
 * @brief Send data byte to display (with manual CS control)
 */
void epd_spi_send_data(uint8_t data)
{
    gpio_set_level(EPD_PIN_DC, 1);  // Data mode
    gpio_set_level(EPD_PIN_CS, 0);  // CS active

    spi_transaction_t trans = {
        .length = 8,
        .tx_buffer = &data,
    };

    spi_device_polling_transmit(s_spi_handle, &trans);

    gpio_set_level(EPD_PIN_CS, 1);  // CS idle
}

/**
 * @brief Send multiple data bytes to display using chunked transfers
 * Uses the same chunking strategy as the reference project (5000 bytes per chunk)
 */
void epd_spi_send_data_burst(const uint8_t *data, size_t len)
{
    gpio_set_level(EPD_PIN_DC, 1);  // Data mode
    gpio_set_level(EPD_PIN_CS, 0);  // CS active

    size_t remaining = len;
    const uint8_t *ptr = data;
    const size_t CHUNK_SIZE = 5000;  // Match reference project

    while (remaining > 0) {
        size_t chunk = (remaining > CHUNK_SIZE) ? CHUNK_SIZE : remaining;

        spi_transaction_t trans = {
            .length = chunk * 8,
            .tx_buffer = ptr,
        };

        spi_device_polling_transmit(s_spi_handle, &trans);

        ptr += chunk;
        remaining -= chunk;
    }

    gpio_set_level(EPD_PIN_CS, 1);  // CS idle
}

/**
 * @brief Get SPI handle (for advanced use)
 */
spi_device_handle_t epd_spi_get_handle(void)
{
    return s_spi_handle;
}
