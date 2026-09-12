idf_component_register(
    SRCS "main.c" "ha_frame_runtime.c"
    INCLUDE_DIRS ""
    PRIV_REQUIRES
        spi_flash
        wifi_manager
        epd_driver
        i2c_bsp
        axpPower
        nvs_flash
        esp_hw_support
        esp_timer
        driver
        frame_store
        ha_frame_client
)
