idf_component_register(SRCS "telemetry_gateway.c"
                    INCLUDE_DIRS "."
                    REQUIRES micro_ros_espidf_component so_101_core esp_driver_uart joltwallet__littlefs cjson)

target_link_libraries(${COMPONENT_LIB} INTERFACE "-Wl,--allow-multiple-definition")