cmake_minimum_required(VERSION 3.16)
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../shared_components/so_101_core")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(robot_controller)
