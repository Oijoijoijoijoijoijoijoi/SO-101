idf_component_register(SRCS "telemetry_gateway.c"
                    INCLUDE_DIRS "."
                    REQUIRES micro_ros_espidf_component so_101_core)

target_link_libraries(${COMPONENT_LIB} INTERFACE "-Wl,--allow-multiple-definition")