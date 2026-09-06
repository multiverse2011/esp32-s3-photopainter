idf_component_register(
    SRCS "ha_frame_client.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_client esp-tls json mbedtls frame_store esp_timer
)
