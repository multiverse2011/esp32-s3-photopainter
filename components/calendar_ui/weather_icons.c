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
 *
 * Uses orange outline with yellow fill for 7-color E-Paper appeal.
 */
static void draw_sun_icon(uint16_t cx, uint16_t cy, uint16_t size)
{
    uint16_t radius = size / 3;
    uint16_t ray_len = size / 4;
    uint16_t outline = size / 18;
    uint16_t ray_thickness = size / 24;
    uint16_t ray_start = radius + outline + 2;

    if (outline < 2) {
        outline = 2;
    }
    if (ray_thickness < 2) {
        ray_thickness = 2;
    }

    // Sun with orange outline and yellow fill (7-color E-Paper colors)
    gfx_fill_circle(cx, cy, radius, EPD_COLOR_ORANGE);
    if (radius > outline) {
        gfx_fill_circle(cx, cy, radius - outline, EPD_COLOR_YELLOW);
    }

    // Draw rays in orange
    for (int i = -(int)(ray_thickness / 2); i <= (int)(ray_thickness / 2); i++) {
        // Top
        gfx_draw_line(cx + i, cy - ray_start, cx + i, cy - ray_start - ray_len, EPD_COLOR_ORANGE);
        // Bottom
        gfx_draw_line(cx + i, cy + ray_start, cx + i, cy + ray_start + ray_len, EPD_COLOR_ORANGE);
        // Left
        gfx_draw_line(cx - ray_start, cy + i, cx - ray_start - ray_len, cy + i, EPD_COLOR_ORANGE);
        // Right
        gfx_draw_line(cx + ray_start, cy + i, cx + ray_start + ray_len, cy + i, EPD_COLOR_ORANGE);
    }
}

static void draw_cloud_layer(uint16_t cx, uint16_t cy, uint16_t size, int16_t inflate, epd_color_t color)
{
    int r1 = (int)(size / 4) + inflate;
    int r2 = (int)(size / 5) + inflate;
    int base_h = (int)(size / 5) + inflate;

    if (r1 < 1) r1 = 1;
    if (r2 < 1) r2 = 1;
    if (base_h < 1) base_h = 1;

    // Minimal cloud: two bumps + base.
    gfx_fill_circle(cx - r2, cy, (uint16_t)r2, color);
    gfx_fill_circle(cx + r2, cy, (uint16_t)r1, color);
    gfx_fill_rect(cx - r2 - r1, cy, r1 + r2 * 2, base_h, color);
}

/**
 * @brief Draw cloud icon
 */
static void draw_cloud_icon(uint16_t cx, uint16_t cy, uint16_t size, epd_color_t color)
{
    int16_t border = (int16_t)(size / 24);

    if (border < 2) {
        border = 2;
    }

    // Minimal outline: black base then fill on top.
    draw_cloud_layer(cx, cy, size, border, EPD_COLOR_BLACK);
    draw_cloud_layer(cx, cy, size, 0, color);
}

/**
 * @brief Draw rain drops
 */
static void draw_rain_drops(uint16_t cx, uint16_t cy, uint16_t size)
{
    uint16_t drop_len = size / 5;
    uint16_t start_y = cy + size / 4;
    uint16_t spacing = size / 5;
    uint16_t thickness = size / 24;

    if (thickness < 2) {
        thickness = 2;
    }

    // Minimal rain: 3 short lines.
    for (int i = -1; i <= 1; i++) {
        uint16_t x = cx + i * spacing;
        for (int t = -(int)(thickness / 2); t <= (int)(thickness / 2); t++) {
            gfx_draw_line(x + t, start_y, x + t, start_y + drop_len, EPD_COLOR_BLUE);
        }
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

    if (flake_size < 2) {
        flake_size = 2;
    }

    // Minimal snow: dots with a white core.
    for (int i = -1; i <= 1; i++) {
        uint16_t x = cx + i * spacing;
        gfx_fill_circle(x, start_y, flake_size, EPD_COLOR_BLUE);
        if (flake_size > 1) {
            gfx_fill_circle(x, start_y, flake_size - 1, EPD_COLOR_WHITE);
        }
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
    uint16_t thickness = size / 20;

    if (thickness < 2) {
        thickness = 2;
    }

    // Minimal bolt: single yellow zigzag with slight thickness.
    for (int t = -(int)(thickness / 2); t <= (int)(thickness / 2); t++) {
        gfx_draw_line(cx + t, start_y, cx - w + t, start_y + h / 2, EPD_COLOR_YELLOW);
        gfx_draw_line(cx - w + t, start_y + h / 2, cx + t, start_y + h / 2, EPD_COLOR_YELLOW);
        gfx_draw_line(cx + t, start_y + h / 2, cx - w / 2 + t, start_y + h, EPD_COLOR_YELLOW);
    }
}

/**
 * @brief Draw fog/mist lines
 */
static void draw_fog_lines(uint16_t cx, uint16_t cy, uint16_t size)
{
    uint16_t line_width = size * 2 / 3;
    uint16_t spacing = size / 6;
    uint16_t thickness = size / 24;

    if (thickness < 2) {
        thickness = 2;
    }

    for (int i = -1; i <= 1; i++) {
        uint16_t y = cy + i * spacing;
        uint16_t x_start = cx - line_width / 2;
        for (int t = -(int)(thickness / 2); t <= (int)(thickness / 2); t++) {
            gfx_draw_hline(x_start, y + t, line_width, EPD_COLOR_BLACK);
        }
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
