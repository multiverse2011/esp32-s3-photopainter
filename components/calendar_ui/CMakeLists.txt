idf_component_register(
    SRCS "calendar_ui.c" "weather_icons.c"
    INCLUDE_DIRS "include"
    REQUIRES gfx_library weather_service
)
