idf_component_register(
    SRCS "main.cpp"
    INCLUDE_DIRS "."
    REQUIRES driver esp_tinyusb esp_timer spi_flash ina228
)
