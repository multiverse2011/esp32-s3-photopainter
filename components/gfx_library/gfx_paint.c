/**
 * @file gfx_paint.c
 * @brief Graphics Library implementation - Core painting functions
 */

#include "gfx_paint.h"
#include "epd_driver.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "gfx_paint";

// External font data declarations
extern const uint8_t font_16[];
extern const uint8_t font_24[];
extern const uint8_t font_32[];

// Font metadata
#define FONT_16_WIDTH   8
#define FONT_16_HEIGHT  16
#define FONT_24_WIDTH   12
#define FONT_24_HEIGHT  24
#define FONT_32_WIDTH   16
#define FONT_32_HEIGHT  32

// Framebuffer pointer
static uint8_t *s_buffer = NULL;
static bool s_initialized = false;

// Weekday names
static const char *weekday_names[] = {
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

esp_err_t gfx_init(void)
{
    s_buffer = epd_driver_get_buffer();
    if (s_buffer == NULL) {
        ESP_LOGE(TAG, "EPD buffer not available");
        return ESP_ERR_INVALID_STATE;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "Graphics library initialized");
    return ESP_OK;
}

void gfx_set_pixel(uint16_t x, uint16_t y, epd_color_t color)
{
    if (!s_initialized || x >= EPD_WIDTH || y >= EPD_HEIGHT) {
        return;
    }

    // Calculate byte position (2 pixels per byte, 4bpp)
    uint32_t idx = (y * EPD_WIDTH + x) / 2;
    uint8_t byte_val = s_buffer[idx];

    // Pack pixel: even pixels in high nibble, odd pixels in low nibble
    if (x % 2 == 0) {
        byte_val = (byte_val & 0x0F) | ((color & 0x07) << 4);
    } else {
        byte_val = (byte_val & 0xF0) | (color & 0x07);
    }

    s_buffer[idx] = byte_val;
}

epd_color_t gfx_get_pixel(uint16_t x, uint16_t y)
{
    if (!s_initialized || x >= EPD_WIDTH || y >= EPD_HEIGHT) {
        return EPD_COLOR_WHITE;
    }

    uint32_t idx = (y * EPD_WIDTH + x) / 2;
    uint8_t byte_val = s_buffer[idx];

    if (x % 2 == 0) {
        return (epd_color_t)((byte_val >> 4) & 0x07);
    } else {
        return (epd_color_t)(byte_val & 0x07);
    }
}

/**
 * @brief Get font data pointer for given size
 */
static const uint8_t *get_font_data(gfx_font_size_t font)
{
    switch (font) {
        case GFX_FONT_16: return font_16;
        case GFX_FONT_24: return font_24;
        case GFX_FONT_32: return font_32;
        default: return font_16;
    }
}

/**
 * @brief Get font width for given size
 */
static uint16_t get_font_width(gfx_font_size_t font)
{
    switch (font) {
        case GFX_FONT_16: return FONT_16_WIDTH;
        case GFX_FONT_24: return FONT_24_WIDTH;
        case GFX_FONT_32: return FONT_32_WIDTH;
        default: return FONT_16_WIDTH;
    }
}

/**
 * @brief Get font height for given size
 */
static uint16_t get_font_height(gfx_font_size_t font)
{
    switch (font) {
        case GFX_FONT_16: return FONT_16_HEIGHT;
        case GFX_FONT_24: return FONT_24_HEIGHT;
        case GFX_FONT_32: return FONT_32_HEIGHT;
        default: return FONT_16_HEIGHT;
    }
}

/**
 * @brief Get bytes per character for given font
 */
static uint16_t get_bytes_per_char(gfx_font_size_t font)
{
    uint16_t w = get_font_width(font);
    uint16_t h = get_font_height(font);
    return ((w + 7) / 8) * h;  // Round up width to byte boundary
}

uint16_t gfx_draw_char(uint16_t x, uint16_t y, char ch, gfx_font_size_t font,
                       epd_color_t fg_color, epd_color_t bg_color)
{
    if (!s_initialized) {
        return 0;
    }

    // Only support printable ASCII
    if (ch < 0x20 || ch > 0x7E) {
        ch = ' ';
    }

    const uint8_t *font_data = get_font_data(font);
    uint16_t char_width = get_font_width(font);
    uint16_t char_height = get_font_height(font);
    uint16_t bytes_per_char = get_bytes_per_char(font);
    uint16_t bytes_per_row = (char_width + 7) / 8;

    // Calculate character offset in font data
    uint16_t char_offset = (ch - 0x20) * bytes_per_char;
    const uint8_t *char_data = font_data + char_offset;

    // Draw character
    for (uint16_t row = 0; row < char_height; row++) {
        for (uint16_t col = 0; col < char_width; col++) {
            uint16_t byte_idx = row * bytes_per_row + col / 8;
            uint8_t bit_idx = 7 - (col % 8);
            bool is_set = (char_data[byte_idx] >> bit_idx) & 1;

            uint16_t px = x + col;
            uint16_t py = y + row;

            if (px < EPD_WIDTH && py < EPD_HEIGHT) {
                gfx_set_pixel(px, py, is_set ? fg_color : bg_color);
            }
        }
    }

    return char_width;
}

uint16_t gfx_get_string_width(const char *str, gfx_font_size_t font)
{
    if (str == NULL) {
        return 0;
    }

    uint16_t char_width = get_font_width(font);
    return strlen(str) * char_width;
}

void gfx_draw_string(uint16_t x, uint16_t y, const char *str, gfx_font_size_t font,
                     epd_color_t fg_color, epd_color_t bg_color, gfx_align_t align)
{
    if (!s_initialized || str == NULL) {
        return;
    }

    uint16_t str_width = gfx_get_string_width(str, font);
    uint16_t char_width = get_font_width(font);

    // Adjust x based on alignment
    uint16_t start_x;
    switch (align) {
        case GFX_ALIGN_CENTER:
            start_x = (x > str_width / 2) ? (x - str_width / 2) : 0;
            break;
        case GFX_ALIGN_RIGHT:
            start_x = (x > str_width) ? (x - str_width) : 0;
            break;
        case GFX_ALIGN_LEFT:
        default:
            start_x = x;
            break;
    }

    // Draw each character
    uint16_t cur_x = start_x;
    while (*str) {
        gfx_draw_char(cur_x, y, *str, font, fg_color, bg_color);
        cur_x += char_width;
        str++;
    }
}

void gfx_draw_time(uint16_t x, uint16_t y, time_t timestamp,
                   gfx_font_size_t font, epd_color_t color)
{
    struct tm timeinfo;
    localtime_r(&timestamp, &timeinfo);

    char time_str[6];
    snprintf(time_str, sizeof(time_str), "%02d:%02d",
             timeinfo.tm_hour, timeinfo.tm_min);

    gfx_draw_string(x, y, time_str, font, color, EPD_COLOR_WHITE, GFX_ALIGN_LEFT);
}

void gfx_draw_date(uint16_t x, uint16_t y, time_t timestamp,
                   gfx_font_size_t font, epd_color_t color, bool show_weekday)
{
    struct tm timeinfo;
    localtime_r(&timestamp, &timeinfo);

    char date_str[16];
    if (show_weekday) {
        snprintf(date_str, sizeof(date_str), "%s",
                 weekday_names[timeinfo.tm_wday]);
    } else {
        snprintf(date_str, sizeof(date_str), "%d/%d",
                 timeinfo.tm_mon + 1, timeinfo.tm_mday);
    }

    gfx_draw_string(x, y, date_str, font, color, EPD_COLOR_WHITE, GFX_ALIGN_LEFT);
}
