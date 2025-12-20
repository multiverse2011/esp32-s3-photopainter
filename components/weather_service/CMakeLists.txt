idf_component_register(
    SRCS "weather_http.c" "weather_parser.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_client json nvs_flash esp_tls
)
