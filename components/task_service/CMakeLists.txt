idf_component_register(
    SRCS "task_service.c" "task_parser.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_client json nvs_flash esp_tls
)
