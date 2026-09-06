idf_component_register(
    SRCS "epaper_port.c"
    INCLUDE_DIRS "."
    REQUIRES driver esp_timer esp_mm
)
