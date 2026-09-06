idf_component_register(
    SRCS "frame_store.c"
    INCLUDE_DIRS "include"
    REQUIRES app_update esp_partition esp_common spi_flash mbedtls
)
