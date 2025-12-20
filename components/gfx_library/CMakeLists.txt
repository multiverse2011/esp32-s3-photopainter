idf_component_register(
    SRCS "gfx_paint.c" "gfx_primitives.c" "fonts/font_16.c" "fonts/font_24.c" "fonts/font_32.c"
    INCLUDE_DIRS "include"
    REQUIRES epd_driver
)
