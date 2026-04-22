cmake_minimum_required(VERSION 3.16)
set(EXTRA_COMPONENT_DIRS 
    "${CMAKE_CURRENT_LIST_DIR}/../shared_components/so_101_core"
    "${CMAKE_CURRENT_LIST_DIR}/../shared_components/micro_ros_espidf_component"
)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(telemetry_gateway)
