idf_component_register(
    SRCS "train_service.c" "train_parser.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_client nvs_flash esp_tls
)
