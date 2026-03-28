idf_component_register(
    SRCS "esp32_firmware.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES esp_timer driver joltwallet__littlefs micro_ros_espidf_component
)
