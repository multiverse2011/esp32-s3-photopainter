idf_component_register(
    SRCS "epd_driver.c" "epd_spi.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_psram
)
