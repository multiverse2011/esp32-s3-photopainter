/**
 * @file weather_icons.c
 * @brief Weather icon drawing for ESP32-S3 Weather Calendar
 *
 * Draws weather icons using graphics primitives based on OpenWeatherMap icon codes.
 */

#include "calendar_ui.h"
#include "gfx_paint.h"
#include "epd_driver.h"
#include <string.h>

/**
 * @brief Draw sun icon
 */
static void draw_sun_icon(uint16_t cx, uint16_t cy, uint16_t size)
{
    uint16_t radius = size / 3;
    uint16_t ray_len = size / 4;
    uint16_t ray_start = radius + 4;

    // Sun body (yellow circle)
    gfx_fill_circle(cx, cy, radius, EPD_COLOR_YELLOW);
    gfx_draw_circle(cx, cy, radius, EPD_COLOR_ORANGE);

    // Sun rays (orange lines)
    // Top
    gfx_draw_line(cx, cy - ray_start, cx, cy - ray_start - ray_len, EPD_COLOR_ORANGE);
    // Bottom
    gfx_draw_line(cx, cy + ray_start, cx, cy + ray_start + ray_len, EPD_COLOR_ORANGE);
    // Left
    gfx_draw_line(cx - ray_start, cy, cx - ray_start - ray_len, cy, EPD_COLOR_ORANGE);
    // Right
    gfx_draw_line(cx + ray_start, cy, cx + ray_start + ray_len, cy, EPD_COLOR_ORANGE);

    // Diagonal rays
    uint16_t diag_start = (ray_start * 7) / 10;  // ~0.7 * ray_start
    uint16_t diag_len = (ray_len * 7) / 10;
    // Top-right
    gfx_draw_line(cx + diag_start, cy - diag_start,
                  cx + diag_start + diag_len, cy - diag_start - diag_len, EPD_COLOR_ORANGE);
    // Top-left
    gfx_draw_line(cx - diag_start, cy - diag_start,
                  cx - diag_start - diag_len, cy - diag_start - diag_len, EPD_COLOR_ORANGE);
    // Bottom-right
    gfx_draw_line(cx + diag_start, cy + diag_start,
                  cx + diag_start + diag_len, cy + diag_start + diag_len, EPD_COLOR_ORANGE);
    // Bottom-left
    gfx_draw_line(cx - diag_start, cy + diag_start,
                  cx - diag_start - diag_len, cy + diag_start + diag_len, EPD_COLOR_ORANGE);
}

/**
 * @brief Draw cloud icon
 */
static void draw_cloud_icon(uint16_t cx, uint16_t cy, uint16_t size, epd_color_t color)
{
    uint16_t r1 = size / 4;       // Main bubble
    uint16_t r2 = size / 5;       // Side bubbles
    uint16_t r3 = size / 6;       // Small bubbles

    // Main cloud body (overlapping circles)
    gfx_fill_circle(cx, cy, r1, color);
    gfx_fill_circle(cx - r1, cy + r3, r2, color);
    gfx_fill_circle(cx + r1, cy + r3, r2, color);
    gfx_fill_circle(cx - r1/2, cy - r3, r3, color);
    gfx_fill_circle(cx + r1/2, cy - r3, r3, color);

    // Fill bottom flat area
    gfx_fill_rect(cx - r1 - r2/2, cy, r1*2 + r2, r1/2 + r3, color);

    // Outline
    gfx_draw_circle(cx, cy, r1, EPD_COLOR_BLACK);
    gfx_draw_circle(cx - r1, cy + r3, r2, EPD_COLOR_BLACK);
    gfx_draw_circle(cx + r1, cy + r3, r2, EPD_COLOR_BLACK);
}

/**
 * @brief Draw rain drops
 */
static void draw_rain_drops(uint16_t cx, uint16_t cy, uint16_t size)
{
    uint16_t drop_len = size / 6;
    uint16_t start_y = cy + size / 4;
    uint16_t spacing = size / 5;

    // Draw 3 rain drops
    for (int i = -1; i <= 1; i++) {
        uint16_t x = cx + i * spacing;
        gfx_draw_line(x, start_y, x - drop_len/3, start_y + drop_len, EPD_COLOR_BLUE);
        gfx_draw_line(x + 1, start_y, x - drop_len/3 + 1, start_y + drop_len, EPD_COLOR_BLUE);
    }
}

/**
 * @brief Draw snow flakes
 */
static void draw_snow_flakes(uint16_t cx, uint16_t cy, uint16_t size)
{
    uint16_t start_y = cy + size / 4;
    uint16_t spacing = size / 4;
    uint16_t flake_size = size / 10;

    // Draw 3 snowflakes (as small circles)
    for (int i = -1; i <= 1; i++) {
        uint16_t x = cx + i * spacing;
        gfx_fill_circle(x, start_y, flake_size, EPD_COLOR_WHITE);
        gfx_draw_circle(x, start_y, flake_size, EPD_COLOR_BLUE);
    }
}

/**
 * @brief Draw lightning bolt
 */
static void draw_lightning(uint16_t cx, uint16_t cy, uint16_t size)
{
    uint16_t h = size / 3;
    uint16_t w = size / 6;
    uint16_t start_y = cy;

    // Simple zigzag lightning bolt
    gfx_draw_line(cx, start_y, cx - w, start_y + h/2, EPD_COLOR_YELLOW);
    gfx_draw_line(cx - w, start_y + h/2, cx, start_y + h/2, EPD_COLOR_YELLOW);
    gfx_draw_line(cx, start_y + h/2, cx - w/2, start_y + h, EPD_COLOR_YELLOW);

    // Second pass for thickness
    gfx_draw_line(cx + 1, start_y, cx - w + 1, start_y + h/2, EPD_COLOR_YELLOW);
    gfx_draw_line(cx - w + 1, start_y + h/2, cx + 1, start_y + h/2, EPD_COLOR_YELLOW);
    gfx_draw_line(cx + 1, start_y + h/2, cx - w/2 + 1, start_y + h, EPD_COLOR_YELLOW);
}

/**
 * @brief Draw fog/mist lines
 */
static void draw_fog_lines(uint16_t cx, uint16_t cy, uint16_t size)
{
    uint16_t line_width = size * 2 / 3;
    uint16_t spacing = size / 6;

    for (int i = -1; i <= 1; i++) {
        uint16_t y = cy + i * spacing;
        uint16_t x_start = cx - line_width / 2;
        gfx_draw_hline(x_start, y, line_width, EPD_COLOR_BLACK);
        gfx_draw_hline(x_start, y + 1, line_width, EPD_COLOR_BLACK);
    }
}

/**
 * @brief Draw partly cloudy (sun + cloud)
 */
static void draw_partly_cloudy(uint16_t cx, uint16_t cy, uint16_t size)
{
    // Sun in upper-left
    draw_sun_icon(cx - size/5, cy - size/5, size * 2 / 3);

    // Cloud in lower-right
    draw_cloud_icon(cx + size/6, cy + size/6, size * 2 / 3, EPD_COLOR_WHITE);
}

void calendar_ui_draw_weather_icon(uint16_t x, uint16_t y,
                                   uint16_t size,
                                   const char *icon_code)
{
    if (icon_code == NULL || strlen(icon_code) < 2) {
        // Default to cloud if no icon code
        draw_cloud_icon(x, y, size, EPD_COLOR_WHITE);
        return;
    }

    // Parse icon code (e.g., "01d", "10n")
    // First two characters indicate weather type
    char code[3] = {icon_code[0], icon_code[1], '\0'};

    if (strcmp(code, "01") == 0) {
        // Clear sky
        draw_sun_icon(x, y, size);
    }
    else if (strcmp(code, "02") == 0) {
        // Few clouds
        draw_partly_cloudy(x, y, size);
    }
    else if (strcmp(code, "03") == 0) {
        // Scattered clouds
        draw_cloud_icon(x, y, size, EPD_COLOR_WHITE);
    }
    else if (strcmp(code, "04") == 0) {
        // Broken/overcast clouds
        draw_cloud_icon(x, y, size, EPD_COLOR_WHITE);
        // Draw second smaller cloud
        draw_cloud_icon(x + size/4, y + size/4, size/2, EPD_COLOR_WHITE);
    }
    else if (strcmp(code, "09") == 0) {
        // Shower rain
        draw_cloud_icon(x, y - size/6, size, EPD_COLOR_WHITE);
        draw_rain_drops(x, y, size);
    }
    else if (strcmp(code, "10") == 0) {
        // Rain
        draw_cloud_icon(x, y - size/6, size, EPD_COLOR_WHITE);
        draw_rain_drops(x, y, size);
    }
    else if (strcmp(code, "11") == 0) {
        // Thunderstorm
        draw_cloud_icon(x, y - size/6, size, EPD_COLOR_WHITE);
        draw_lightning(x, y + size/6, size);
    }
    else if (strcmp(code, "13") == 0) {
        // Snow
        draw_cloud_icon(x, y - size/6, size, EPD_COLOR_WHITE);
        draw_snow_flakes(x, y, size);
    }
    else if (strcmp(code, "50") == 0) {
        // Mist/fog
        draw_fog_lines(x, y, size);
    }
    else {
        // Unknown - draw generic cloud
        draw_cloud_icon(x, y, size, EPD_COLOR_WHITE);
    }
}
