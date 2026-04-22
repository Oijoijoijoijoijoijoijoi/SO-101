#!/bin/bash
set -e

WORKSPACE_DIR="/workspaces/SO-101"
ESP_EXPORT_SCRIPT="/opt/esp/idf/export.sh"

source "${ESP_EXPORT_SCRIPT}"
cd "${WORKSPACE_DIR}"

# 1. Middleware & Submodule Reconciliation
git config --global --add safe.directory "${WORKSPACE_DIR}"
git submodule update --init --recursive

MICROROS_DIR="shared_components/micro_ros_espidf_component"
if [ ! -f "${MICROROS_DIR}/CMakeLists.txt" ]; then
    rm -rf "${MICROROS_DIR}"
    git submodule add --force -b jazzy https://github.com/micro-ROS/micro_ros_espidf_component.git "${MICROROS_DIR}"
    git submodule update --init --recursive
fi

# 2. Shared Core Scaffolding (Transport Agnostic)
SHARED_CORE_DIR="shared_components/so_101_core"
if [ ! -f "${SHARED_CORE_DIR}/CMakeLists.txt" ]; then
    mkdir -p "${SHARED_CORE_DIR}/include" "${SHARED_CORE_DIR}/src"
    touch "${SHARED_CORE_DIR}/src/so_101_core.c"
    cat << 'EOF' > "${SHARED_CORE_DIR}/CMakeLists.txt"
idf_component_register(SRCS "src/so_101_core.c" INCLUDE_DIRS "include")                       
EOF
    cat << 'EOF' > "${SHARED_CORE_DIR}/idf_component.yml"
dependencies:
  idf: { version: ">=5.3.0" }
  espressif/esp-dsp: "^1.4.9"
  joltwallet/littlefs: "^1.14.4"
EOF
fi

# 3. Common Partition Specification
COMMON_PARTITIONS=$(cat << 'EOF'
# Name,   Type, SubType, Offset,  Size, Flags
nvs,      data, nvs,     0x9000,  0x6000,
phy_init, data, phy,     0xf000,  0x1000,
factory,  app,  factory, 0x10000, 0x200000,
storage,  data, littlefs,0x210000,0x1E0000,
coredump, data, coredump,0x3F0000,0x10000,
EOF
)

# 4. Common Hardware Baseline (Shared by both Nodes)
COMMON_DEFAULTS=$(cat << 'EOF'
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"
CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y
CONFIG_ESPTOOLPY_FLASHSIZE="4MB"
CONFIG_ESP_COREDUMP_ENABLE=y
CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH=y
CONFIG_ESP_COREDUMP_DATA_FORMAT_ELF=y
CONFIG_ESP_COREDUMP_CHECKSUM_CRC32=y
CONFIG_FREERTOS_HZ=1000
CONFIG_NEWLIB_NANO_FORMAT=n
CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_240=y
CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ=240
CONFIG_ESP_MAIN_TASK_AFFINITY_CPU1=y
CONFIG_ESP_WIFI_TASK_PINNED_TO_CORE_0=y
CONFIG_FREERTOS_TIMER_TASK_AFFINITY_CPU0=y
CONFIG_LWIP_TCPIP_TASK_AFFINITY_CPU0=y
CONFIG_PTHREAD_DEFAULT_CORE_0=y
CONFIG_ESP_WIFI_STA_DISCONNECTED_PM_ENABLE=n
CONFIG_LWIP_IRAM_OPTIMIZATION=y
CONFIG_LWIP_MAX_SOCKETS=10
EOF
)

# --- PROJECT 1: ROBOT CONTROLLER ---
if [ ! -d "robot_controller" ]; then
    echo "Initializing 'robot_controller'..."
    idf.py create-project robot_controller
    cat << 'EOF' > "robot_controller/CMakeLists.txt"
cmake_minimum_required(VERSION 3.16)
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../shared_components/so_101_core")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(robot_controller)
EOF
    echo "$COMMON_PARTITIONS" > "robot_controller/partitions.csv"
    echo "$COMMON_DEFAULTS" > "robot_controller/sdkconfig.defaults"
fi

# --- PROJECT 2: TELEMETRY GATEWAY ---
if [ ! -d "telemetry_gateway" ]; then
    echo "Initializing 'telemetry_gateway'..."
    idf.py create-project telemetry_gateway
    cat << 'EOF' > "telemetry_gateway/CMakeLists.txt"
cmake_minimum_required(VERSION 3.16)
set(EXTRA_COMPONENT_DIRS 
    "${CMAKE_CURRENT_LIST_DIR}/../shared_components/so_101_core"
    "${CMAKE_CURRENT_LIST_DIR}/../shared_components/micro_ros_espidf_component"
)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(telemetry_gateway)
EOF
    echo "$COMMON_PARTITIONS" > "telemetry_gateway/partitions.csv"
    # Append micro-ROS Serial Transport specific settings to Gateway
    echo "$COMMON_DEFAULTS" > "telemetry_gateway/sdkconfig.defaults"
    cat << 'EOF' >> "telemetry_gateway/sdkconfig.defaults"
# micro-ROS Gateway Specifics (UART Transport)
CONFIG_MICRO_ROS_ESP_XRCE_DDS_MIDDLEWARE=y
CONFIG_MICRO_ROS_ESP_UART_TRANSPORT=y
EOF
fi