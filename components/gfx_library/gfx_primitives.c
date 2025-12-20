/**
 * @file gfx_primitives.c
 * @brief Graphics Library - Geometric primitives (lines, shapes)
 */

#include "gfx_paint.h"
#include "epd_driver.h"
#include <stdlib.h>

// External set_pixel function
extern void gfx_set_pixel(uint16_t x, uint16_t y, epd_color_t color);

void gfx_draw_hline(uint16_t x, uint16_t y, uint16_t w, epd_color_t color)
{
    for (uint16_t i = 0; i < w; i++) {
        gfx_set_pixel(x + i, y, color);
    }
}

void gfx_draw_vline(uint16_t x, uint16_t y, uint16_t h, epd_color_t color)
{
    for (uint16_t i = 0; i < h; i++) {
        gfx_set_pixel(x, y + i, color);
    }
}

void gfx_draw_line(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, epd_color_t color)
{
    // Optimized cases
    if (y0 == y1) {
        // Horizontal line
        uint16_t start = (x0 < x1) ? x0 : x1;
        uint16_t len = abs((int)x1 - (int)x0) + 1;
        gfx_draw_hline(start, y0, len, color);
        return;
    }

    if (x0 == x1) {
        // Vertical line
        uint16_t start = (y0 < y1) ? y0 : y1;
        uint16_t len = abs((int)y1 - (int)y0) + 1;
        gfx_draw_vline(x0, start, len, color);
        return;
    }

    // Bresenham's line algorithm
    int dx = abs((int)x1 - (int)x0);
    int dy = abs((int)y1 - (int)y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;

    int x = x0;
    int y = y0;

    while (1) {
        gfx_set_pixel(x, y, color);

        if (x == x1 && y == y1) break;

        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x += sx;
        }
        if (e2 < dx) {
            err += dx;
            y += sy;
        }
    }
}

void gfx_draw_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, epd_color_t color)
{
    // Top and bottom lines
    gfx_draw_hline(x, y, w, color);
    gfx_draw_hline(x, y + h - 1, w, color);

    // Left and right lines
    gfx_draw_vline(x, y, h, color);
    gfx_draw_vline(x + w - 1, y, h, color);
}

void gfx_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, epd_color_t color)
{
    for (uint16_t row = 0; row < h; row++) {
        gfx_draw_hline(x, y + row, w, color);
    }
}

/**
 * @brief Draw 8 symmetric points for circle algorithm
 */
static void draw_circle_points(uint16_t cx, uint16_t cy, uint16_t x, uint16_t y, epd_color_t color)
{
    gfx_set_pixel(cx + x, cy + y, color);
    gfx_set_pixel(cx - x, cy + y, color);
    gfx_set_pixel(cx + x, cy - y, color);
    gfx_set_pixel(cx - x, cy - y, color);
    gfx_set_pixel(cx + y, cy + x, color);
    gfx_set_pixel(cx - y, cy + x, color);
    gfx_set_pixel(cx + y, cy - x, color);
    gfx_set_pixel(cx - y, cy - x, color);
}

void gfx_draw_circle(uint16_t cx, uint16_t cy, uint16_t radius, epd_color_t color)
{
    // Midpoint circle algorithm
    int x = 0;
    int y = radius;
    int d = 3 - 2 * radius;

    draw_circle_points(cx, cy, x, y, color);

    while (y >= x) {
        x++;
        if (d > 0) {
            y--;
            d = d + 4 * (x - y) + 10;
        } else {
            d = d + 4 * x + 6;
        }
        draw_circle_points(cx, cy, x, y, color);
    }
}

/**
 * @brief Draw horizontal lines for filled circle
 */
static void fill_circle_lines(uint16_t cx, uint16_t cy, uint16_t x, uint16_t y, epd_color_t color)
{
    gfx_draw_hline(cx - x, cy + y, 2 * x + 1, color);
    gfx_draw_hline(cx - x, cy - y, 2 * x + 1, color);
    gfx_draw_hline(cx - y, cy + x, 2 * y + 1, color);
    gfx_draw_hline(cx - y, cy - x, 2 * y + 1, color);
}

void gfx_fill_circle(uint16_t cx, uint16_t cy, uint16_t radius, epd_color_t color)
{
    int x = 0;
    int y = radius;
    int d = 3 - 2 * radius;

    fill_circle_lines(cx, cy, x, y, color);

    while (y >= x) {
        x++;
        if (d > 0) {
            y--;
            d = d + 4 * (x - y) + 10;
        } else {
            d = d + 4 * x + 6;
        }
        fill_circle_lines(cx, cy, x, y, color);
    }
}

void gfx_draw_ellipse(uint16_t cx, uint16_t cy, uint16_t rx, uint16_t ry, epd_color_t color)
{
    int x = 0;
    int y = ry;

    // Region 1
    int rx2 = rx * rx;
    int ry2 = ry * ry;
    int p1 = ry2 - rx2 * ry + rx2 / 4;

    while (2 * ry2 * x < 2 * rx2 * y) {
        gfx_set_pixel(cx + x, cy + y, color);
        gfx_set_pixel(cx - x, cy + y, color);
        gfx_set_pixel(cx + x, cy - y, color);
        gfx_set_pixel(cx - x, cy - y, color);

        x++;
        if (p1 < 0) {
            p1 += 2 * ry2 * x + ry2;
        } else {
            y--;
            p1 += 2 * ry2 * x - 2 * rx2 * y + ry2;
        }
    }

    // Region 2
    int p2 = ry2 * (x * x + x) + rx2 * (y - 1) * (y - 1) - rx2 * ry2;

    while (y >= 0) {
        gfx_set_pixel(cx + x, cy + y, color);
        gfx_set_pixel(cx - x, cy + y, color);
        gfx_set_pixel(cx + x, cy - y, color);
        gfx_set_pixel(cx - x, cy - y, color);

        y--;
        if (p2 > 0) {
            p2 += rx2 - 2 * rx2 * y;
        } else {
            x++;
            p2 += 2 * ry2 * x - 2 * rx2 * y + rx2;
        }
    }
}

void gfx_fill_ellipse(uint16_t cx, uint16_t cy, uint16_t rx, uint16_t ry, epd_color_t color)
{
    int x = 0;
    int y = ry;

    int rx2 = rx * rx;
    int ry2 = ry * ry;
    int p1 = ry2 - rx2 * ry + rx2 / 4;

    // Region 1
    while (2 * ry2 * x < 2 * rx2 * y) {
        gfx_draw_hline(cx - x, cy + y, 2 * x + 1, color);
        gfx_draw_hline(cx - x, cy - y, 2 * x + 1, color);

        x++;
        if (p1 < 0) {
            p1 += 2 * ry2 * x + ry2;
        } else {
            y--;
            p1 += 2 * ry2 * x - 2 * rx2 * y + ry2;
        }
    }

    // Region 2
    int p2 = ry2 * (x * x + x) + rx2 * (y - 1) * (y - 1) - rx2 * ry2;

    while (y >= 0) {
        gfx_draw_hline(cx - x, cy + y, 2 * x + 1, color);
        gfx_draw_hline(cx - x, cy - y, 2 * x + 1, color);

        y--;
        if (p2 > 0) {
            p2 += rx2 - 2 * rx2 * y;
        } else {
            x++;
            p2 += 2 * ry2 * x - 2 * rx2 * y + rx2;
        }
    }
}
