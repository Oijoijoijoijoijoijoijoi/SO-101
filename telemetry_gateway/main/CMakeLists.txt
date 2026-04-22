idf_component_register(SRCS "telemetry_gateway.c"
                    INCLUDE_DIRS ".")

target_link_libraries(${COMPONENT_LIB} INTERFACE "-Wl,--allow-multiple-definition")