/**
 * @file gfx_paint.h
 * @brief Graphics Library for E-Paper Display
 *
 * Provides drawing primitives and text rendering for 7-color E-Paper.
 */

#ifndef GFX_PAINT_H
#define GFX_PAINT_H

#include "epd_driver.h"
#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Font size enumeration */
typedef enum {
    GFX_FONT_16 = 16,
    GFX_FONT_24 = 24,
    GFX_FONT_32 = 32
} gfx_font_size_t;

/** Text alignment */
typedef enum {
    GFX_ALIGN_LEFT,
    GFX_ALIGN_CENTER,
    GFX_ALIGN_RIGHT
} gfx_align_t;

/**
 * @brief Initialize graphics library
 *
 * Must be called after epd_driver_init(). Uses EPD framebuffer.
 *
 * @return ESP_OK on success
 * @return ESP_ERR_INVALID_STATE if EPD not initialized
 */
esp_err_t gfx_init(void);

/**
 * @brief Set a single pixel
 *
 * @param x X coordinate (0 to EPD_WIDTH-1)
 * @param y Y coordinate (0 to EPD_HEIGHT-1)
 * @param color Pixel color
 */
void gfx_set_pixel(uint16_t x, uint16_t y, epd_color_t color);

/**
 * @brief Get a single pixel value
 *
 * @param x X coordinate
 * @param y Y coordinate
 * @return Pixel color
 */
epd_color_t gfx_get_pixel(uint16_t x, uint16_t y);

/**
 * @brief Draw a line (Bresenham algorithm)
 *
 * @param x0, y0 Start point
 * @param x1, y1 End point
 * @param color Line color
 */
void gfx_draw_line(uint16_t x0, uint16_t y0,
                   uint16_t x1, uint16_t y1,
                   epd_color_t color);

/**
 * @brief Draw horizontal line (optimized)
 *
 * @param x Start X coordinate
 * @param y Y coordinate
 * @param w Width
 * @param color Line color
 */
void gfx_draw_hline(uint16_t x, uint16_t y, uint16_t w, epd_color_t color);

/**
 * @brief Draw vertical line (optimized)
 *
 * @param x X coordinate
 * @param y Start Y coordinate
 * @param h Height
 * @param color Line color
 */
void gfx_draw_vline(uint16_t x, uint16_t y, uint16_t h, epd_color_t color);

/**
 * @brief Draw rectangle outline
 *
 * @param x, y Top-left corner
 * @param w Width
 * @param h Height
 * @param color Outline color
 */
void gfx_draw_rect(uint16_t x, uint16_t y,
                   uint16_t w, uint16_t h,
                   epd_color_t color);

/**
 * @brief Draw filled rectangle
 *
 * @param x, y Top-left corner
 * @param w Width
 * @param h Height
 * @param color Fill color
 */
void gfx_fill_rect(uint16_t x, uint16_t y,
                   uint16_t w, uint16_t h,
                   epd_color_t color);

/**
 * @brief Draw circle outline (Midpoint algorithm)
 *
 * @param cx, cy Center point
 * @param radius Circle radius
 * @param color Outline color
 */
void gfx_draw_circle(uint16_t cx, uint16_t cy,
                     uint16_t radius,
                     epd_color_t color);

/**
 * @brief Draw filled circle
 *
 * @param cx, cy Center point
 * @param radius Circle radius
 * @param color Fill color
 */
void gfx_fill_circle(uint16_t cx, uint16_t cy,
                     uint16_t radius,
                     epd_color_t color);

/**
 * @brief Draw ellipse outline
 *
 * @param cx, cy Center point
 * @param rx X radius
 * @param ry Y radius
 * @param color Outline color
 */
void gfx_draw_ellipse(uint16_t cx, uint16_t cy,
                      uint16_t rx, uint16_t ry,
                      epd_color_t color);

/**
 * @brief Draw filled ellipse
 *
 * @param cx, cy Center point
 * @param rx X radius
 * @param ry Y radius
 * @param color Fill color
 */
void gfx_fill_ellipse(uint16_t cx, uint16_t cy,
                      uint16_t rx, uint16_t ry,
                      epd_color_t color);

/**
 * @brief Draw a single character
 *
 * @param x, y Top-left position
 * @param ch ASCII character (0x20-0x7E)
 * @param font Font size
 * @param fg_color Foreground color
 * @param bg_color Background color
 * @return Width of drawn character in pixels
 */
uint16_t gfx_draw_char(uint16_t x, uint16_t y,
                       char ch,
                       gfx_font_size_t font,
                       epd_color_t fg_color,
                       epd_color_t bg_color);

/**
 * @brief Draw a string
 *
 * @param x, y Position (interpretation depends on align)
 * @param str Null-terminated string
 * @param font Font size
 * @param fg_color Foreground color
 * @param bg_color Background color
 * @param align Text alignment relative to x position
 */
void gfx_draw_string(uint16_t x, uint16_t y,
                     const char *str,
                     gfx_font_size_t font,
                     epd_color_t fg_color,
                     epd_color_t bg_color,
                     gfx_align_t align);

/**
 * @brief Get width of string in pixels
 *
 * @param str Null-terminated string
 * @param font Font size
 * @return Width in pixels
 */
uint16_t gfx_get_string_width(const char *str, gfx_font_size_t font);

/**
 * @brief Format and draw time (HH:MM)
 *
 * @param x, y Position
 * @param timestamp Unix timestamp
 * @param font Font size
 * @param color Text color
 */
void gfx_draw_time(uint16_t x, uint16_t y,
                   time_t timestamp,
                   gfx_font_size_t font,
                   epd_color_t color);

/**
 * @brief Format and draw date (MM/DD or weekday)
 *
 * @param x, y Position
 * @param timestamp Unix timestamp
 * @param font Font size
 * @param color Text color
 * @param show_weekday If true, show weekday name instead of date
 */
void gfx_draw_date(uint16_t x, uint16_t y,
                   time_t timestamp,
                   gfx_font_size_t font,
                   epd_color_t color,
                   bool show_weekday);

#ifdef __cplusplus
}
#endif

#endif // GFX_PAINT_H
