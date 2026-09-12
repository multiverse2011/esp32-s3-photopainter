#include <string.h>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "epaper_port.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"

#define EPD_DC_PIN 8
#define EPD_CS_PIN 9
#define EPD_SCK_PIN 10
#define EPD_MOSI_PIN 11
#define EPD_RST_PIN 12
#define EPD_BUSY_PIN 13

#define EPD_SPI_CHUNK 5000u
#define EPD_BUSY_TIMEOUT_MS 60000u
#define EPD_SPI_TIMEOUT_MS 5000u

#define epaper_rst_1 gpio_set_level(EPD_RST_PIN, 1)
#define epaper_rst_0 gpio_set_level(EPD_RST_PIN, 0)
#define epaper_cs_1 gpio_set_level(EPD_CS_PIN, 1)
#define epaper_cs_0 gpio_set_level(EPD_CS_PIN, 0)
#define epaper_dc_1 gpio_set_level(EPD_DC_PIN, 1)
#define epaper_dc_0 gpio_set_level(EPD_DC_PIN, 0)
#define ReadBusy gpio_get_level(EPD_BUSY_PIN)

static const char *TAG = "epaper_port";
static spi_device_handle_t s_spi;
static bool s_spi_ready;
static uint8_t *s_dma_buffer;
static int64_t s_operation_deadline_us;
static spi_transaction_t s_transaction;
static uint8_t s_command_byte;
static bool s_transaction_inflight;
static bool s_poisoned;

void epaper_port_set_deadline_us(int64_t absolute_deadline_us)
{
    s_operation_deadline_us = absolute_deadline_us;
}

static TickType_t spi_timeout_ticks(void)
{
    uint32_t timeout_ms = EPD_SPI_TIMEOUT_MS;
    if (s_operation_deadline_us > 0) {
        int64_t remaining = s_operation_deadline_us - esp_timer_get_time();
        if (remaining <= 0) return 0;
        uint64_t remaining_ms = (uint64_t)remaining / 1000u;
        if (remaining_ms < timeout_ms) timeout_ms = (uint32_t)remaining_ms;
    }
    return pdMS_TO_TICKS(timeout_ms);
}

static esp_err_t drain_transaction(void)
{
    if (!s_transaction_inflight) return ESP_OK;
    spi_transaction_t *completed = NULL;
    esp_err_t ret = spi_device_get_trans_result(s_spi, &completed, spi_timeout_ticks());
    if (ret == ESP_OK) s_transaction_inflight = false;
    return ret;
}

static esp_err_t drain_transaction_for_cleanup(void)
{
    if (!s_transaction_inflight) return ESP_OK;
    spi_transaction_t *completed = NULL;
    /* The normal operation deadline may already have expired. Give an
       already-queued transfer its own bounded chance to finish before
       preserving the bus allocation for safety. */
    esp_err_t ret = spi_device_get_trans_result(s_spi, &completed,
                                                pdMS_TO_TICKS(EPD_SPI_TIMEOUT_MS));
    if (ret == ESP_OK) s_transaction_inflight = false;
    return ret;
}

static esp_err_t epaper_gpio_init(void)
{
    gpio_config_t gpio_conf = {0};
    gpio_conf.intr_type = GPIO_INTR_DISABLE;
    gpio_conf.mode = GPIO_MODE_OUTPUT;
    gpio_conf.pin_bit_mask = ((uint64_t)1 << EPD_RST_PIN) |
                             ((uint64_t)1 << EPD_DC_PIN) |
                             ((uint64_t)1 << EPD_CS_PIN);
    gpio_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    esp_err_t ret = gpio_config(&gpio_conf);
    if (ret != ESP_OK) {
        return ret;
    }
    gpio_conf.mode = GPIO_MODE_INPUT;
    gpio_conf.pin_bit_mask = ((uint64_t)1 << EPD_BUSY_PIN);
    gpio_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
    ret = gpio_config(&gpio_conf);
    if (ret == ESP_OK) {
        epaper_cs_1;
        epaper_dc_1;
        epaper_rst_1;
    }
    return ret;
}

static void epaper_reset(void)
{
    epaper_rst_1;
    vTaskDelay(pdMS_TO_TICKS(50));
    epaper_rst_0;
    vTaskDelay(pdMS_TO_TICKS(20));
    epaper_rst_1;
    vTaskDelay(pdMS_TO_TICKS(50));
}

static esp_err_t epaper_spi_init(void)
{
    if (s_spi_ready) {
        return ESP_OK;
    }
    spi_bus_config_t buscfg = {0};
    buscfg.miso_io_num = -1;
    buscfg.mosi_io_num = EPD_MOSI_PIN;
    buscfg.sclk_io_num = EPD_SCK_PIN;
    buscfg.quadwp_io_num = -1;
    buscfg.quadhd_io_num = -1;
    buscfg.max_transfer_sz = EPD_SPI_CHUNK;
    spi_device_interface_config_t devcfg = {0};
    devcfg.spics_io_num = -1;
    devcfg.clock_speed_hz = 10 * 1000 * 1000;
    devcfg.mode = 0;
    devcfg.queue_size = 2;
    devcfg.flags = SPI_DEVICE_HALFDUPLEX;
    esp_err_t ret = spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = spi_bus_add_device(SPI3_HOST, &devcfg, &s_spi);
    if (ret != ESP_OK) {
        spi_bus_free(SPI3_HOST);
        return ret;
    }
    s_dma_buffer = heap_caps_malloc(EPD_SPI_CHUNK, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (s_dma_buffer == NULL) {
        spi_bus_remove_device(s_spi);
        s_spi = NULL;
        spi_bus_free(SPI3_HOST);
        return ESP_ERR_NO_MEM;
    }
    s_spi_ready = true;
    return ESP_OK;
}

static esp_err_t epaper_readbusyh(uint32_t timeout_ms)
{
    if (s_operation_deadline_us > 0) {
        int64_t remaining = s_operation_deadline_us - esp_timer_get_time();
        if (remaining <= 0) return ESP_ERR_TIMEOUT;
        uint64_t remaining_ms = (uint64_t)remaining / 1000u;
        if (remaining_ms < timeout_ms) timeout_ms = (uint32_t)remaining_ms;
    }
    TickType_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(timeout_ms);
    while (!ReadBusy) {
        if ((int32_t)(xTaskGetTickCount() - deadline) >= 0) {
            ESP_LOGE(TAG, "panel BUSY timeout after %lu ms", (unsigned long)timeout_ms);
            return ESP_ERR_TIMEOUT;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return ESP_OK;
}

static esp_err_t spi_send_byte(uint8_t command)
{
    if (s_poisoned) return ESP_ERR_INVALID_STATE;
    esp_err_t ret = drain_transaction();
    if (ret != ESP_OK) return ret;
    memset(&s_transaction, 0, sizeof(s_transaction));
    s_command_byte = command;
    s_transaction.length = 8;
    s_transaction.tx_buffer = &s_command_byte;
    TickType_t timeout = spi_timeout_ticks();
    if (timeout == 0) return ESP_ERR_TIMEOUT;
    ret = spi_device_queue_trans(s_spi, &s_transaction, timeout);
    if (ret == ESP_OK) {
        s_transaction_inflight = true;
        ret = drain_transaction();
        if (ret != ESP_OK) s_poisoned = true;
    }
    return ret;
}

static esp_err_t epaper_send_command(uint8_t command)
{
    epaper_dc_0;
    epaper_cs_0;
    esp_err_t ret = spi_send_byte(command);
    epaper_cs_1;
    return ret;
}

static esp_err_t epaper_send_data(uint8_t data)
{
    epaper_dc_1;
    epaper_cs_0;
    esp_err_t ret = spi_send_byte(data);
    epaper_cs_1;
    return ret;
}

static esp_err_t epaper_send_buffer(const uint8_t *data, size_t length)
{
    if (data == NULL || length == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    epaper_dc_1;
    epaper_cs_0;
    esp_err_t ret = ESP_OK;
    for (size_t offset = 0; offset < length && ret == ESP_OK; offset += EPD_SPI_CHUNK) {
        size_t chunk = length - offset;
        if (chunk > EPD_SPI_CHUNK) {
            chunk = EPD_SPI_CHUNK;
        }
        if (s_poisoned) {
            ret = ESP_ERR_INVALID_STATE;
            break;
        }
        ret = drain_transaction();
        if (ret != ESP_OK) {
            s_poisoned = true;
            break;
        }
        memset(&s_transaction, 0, sizeof(s_transaction));
        s_transaction.length = chunk * 8;
        memcpy(s_dma_buffer, data + offset, chunk);
        s_transaction.tx_buffer = s_dma_buffer;
        TickType_t timeout = spi_timeout_ticks();
        if (timeout == 0) {
            ret = ESP_ERR_TIMEOUT;
            break;
        }
        ret = spi_device_queue_trans(s_spi, &s_transaction, timeout);
        if (ret == ESP_OK) {
            s_transaction_inflight = true;
            ret = drain_transaction();
            if (ret != ESP_OK) s_poisoned = true;
        }
    }
    epaper_cs_1;
    return ret;
}

static esp_err_t epaper_turn_on_display(void)
{
    esp_err_t ret = epaper_send_command(0x04);
    if (ret == ESP_OK) ret = epaper_readbusyh(EPD_BUSY_TIMEOUT_MS);
    if (ret == ESP_OK) ret = epaper_send_command(0x06);
    if (ret == ESP_OK) ret = epaper_send_data(0x6F);
    if (ret == ESP_OK) ret = epaper_send_data(0x1F);
    if (ret == ESP_OK) ret = epaper_send_data(0x17);
    if (ret == ESP_OK) ret = epaper_send_data(0x49);
    if (ret == ESP_OK) ret = epaper_send_command(0x12);
    if (ret == ESP_OK) ret = epaper_send_data(0x00);
    if (ret == ESP_OK) ret = epaper_readbusyh(EPD_BUSY_TIMEOUT_MS);
    if (ret == ESP_OK) ret = epaper_send_command(0x02);
    if (ret == ESP_OK) ret = epaper_send_data(0x00);
    if (ret == ESP_OK) ret = epaper_readbusyh(EPD_BUSY_TIMEOUT_MS);
    return ret;
}

esp_err_t epaper_port_init(void)
{
    if (s_poisoned) {
        esp_err_t recovery = drain_transaction_for_cleanup();
        if (recovery != ESP_OK) {
            ESP_LOGE(TAG, "cannot recover queued SPI transaction: %s",
                     esp_err_to_name(recovery));
            return recovery;
        }
        s_poisoned = false;
    }
    esp_err_t ret = epaper_spi_init();
    if (ret != ESP_OK) {
        return ret;
    }
    ret = epaper_gpio_init();
    if (ret != ESP_OK) {
        return ret;
    }
    epaper_reset();
    ret = epaper_readbusyh(EPD_BUSY_TIMEOUT_MS);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = epaper_send_command(0xAA);
    const uint8_t cmdh[] = {0x49, 0x55, 0x20, 0x08, 0x09, 0x18};
    for (size_t i = 0; ret == ESP_OK && i < sizeof(cmdh); ++i) ret = epaper_send_data(cmdh[i]);
    const uint8_t sequence[][5] = {
        {0x01, 0x3F, 0, 0, 0}, {0x00, 0x5F, 0x69, 0, 0},
        {0x03, 0x00, 0x54, 0x00, 0x44}, {0x05, 0x40, 0x1F, 0x1F, 0x2C},
        {0x06, 0x6F, 0x1F, 0x17, 0x49}, {0x08, 0x6F, 0x1F, 0x1F, 0x22},
        {0x30, 0x03, 0, 0, 0}, {0x50, 0x3F, 0, 0, 0},
        {0x60, 0x02, 0x00, 0, 0}, {0x61, 0x03, 0x20, 0x01, 0xE0},
        {0x84, 0x01, 0, 0, 0}, {0xE3, 0x2F, 0, 0, 0},
    };
    static const uint8_t lengths[] = {2, 3, 5, 5, 5, 5, 2, 2, 3, 5, 2, 2};
    for (size_t row = 0; ret == ESP_OK && row < sizeof(sequence) / sizeof(sequence[0]); ++row) {
        ret = epaper_send_command(sequence[row][0]);
        for (size_t byte = 1; ret == ESP_OK && byte < lengths[row]; ++byte) {
            ret = epaper_send_data(sequence[row][byte]);
        }
    }
    if (ret == ESP_OK) ret = epaper_send_command(0x04);
    if (ret == ESP_OK) ret = epaper_readbusyh(EPD_BUSY_TIMEOUT_MS);
    return ret;
}

esp_err_t epaper_port_clear(uint8_t *image, uint8_t color)
{
    if (!s_spi_ready || image == NULL || color > 0x0Fu) return ESP_ERR_INVALID_ARG;
    for (size_t i = 0; i < (EXAMPLE_LCD_WIDTH * EXAMPLE_LCD_HEIGHT) / 2u; ++i) {
        image[i] = (uint8_t)((color << 4) | color);
    }
    esp_err_t ret = epaper_send_command(0x10);
    if (ret == ESP_OK) ret = epaper_send_buffer(image, (EXAMPLE_LCD_WIDTH * EXAMPLE_LCD_HEIGHT) / 2u);
    if (ret == ESP_OK) ret = epaper_turn_on_display();
    return ret;
}

esp_err_t epaper_port_display(uint8_t *image)
{
    if (!s_spi_ready || image == NULL) return ESP_ERR_INVALID_ARG;
    esp_err_t ret = epaper_send_command(0x10);
    if (ret == ESP_OK) ret = epaper_send_buffer(image, (EXAMPLE_LCD_WIDTH * EXAMPLE_LCD_HEIGHT) / 2u);
    if (ret == ESP_OK) ret = epaper_turn_on_display();
    return ret;
}

esp_err_t epaper_port_sleep(void)
{
    if (!s_spi_ready) return ESP_ERR_INVALID_STATE;
    esp_err_t ret = epaper_send_command(0x02);
    if (ret == ESP_OK) ret = epaper_send_data(0x00);
    if (ret == ESP_OK) ret = epaper_readbusyh(EPD_BUSY_TIMEOUT_MS);
    return ret;
}

void epaper_port_deinit(void)
{
    if (s_spi_ready && s_spi != NULL) {
        if (s_transaction_inflight) {
            if (drain_transaction_for_cleanup() != ESP_OK) {
                ESP_LOGE(TAG, "SPI transaction still owned by driver; preserving port allocation");
                s_poisoned = true;
                return;
            }
        }
        spi_bus_remove_device(s_spi);
        s_spi = NULL;
        spi_bus_free(SPI3_HOST);
    }
    if (s_dma_buffer != NULL) {
        heap_caps_free(s_dma_buffer);
        s_dma_buffer = NULL;
    }
    s_spi_ready = false;
    s_operation_deadline_us = 0;
    s_poisoned = false;
}
