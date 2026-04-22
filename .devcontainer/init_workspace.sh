#!/bin/bash
# Stop execution on any command failure
set -e

WORKSPACE_DIR="/workspaces/SO-101"
ESP_EXPORT_SCRIPT="/opt/esp/idf/export.sh"

# 1. Source ESP-IDF Environment
source "${ESP_EXPORT_SCRIPT}"
cd "${WORKSPACE_DIR}"

# 2. Submodule Synchronization
echo "Synchronizing Git submodules..."
git config --global --add safe.directory "${WORKSPACE_DIR}"

# Attempt standard initialization based on the Git index
git submodule update --init --recursive

# Deterministic validation of submodule presence via core file check
MICROROS_DIR="shared_components/micro_ros_espidf_component"
if [ ! -f "${MICROROS_DIR}/CMakeLists.txt" ]; then
    echo "Submodule index decoupling or missing directory detected."
    echo "Executing forceful registration and recovery..."
    
    # Purge existing empty directory if it exists to prevent git add conflicts
    rm -rf "${MICROROS_DIR}"
    
    # Re-establish the 160000 gitlink and pull the target branch
    git submodule add --force -b jazzy https://github.com/micro-ROS/micro_ros_espidf_component.git "${MICROROS_DIR}"
    
    # Re-invoke standard initialization to ensure nested dependencies are resolved
    git submodule update --init --recursive
fi

# 3. Shared Component Scaffolding
SHARED_CORE_DIR="shared_components/so_101_core"
if [ ! -f "${SHARED_CORE_DIR}/CMakeLists.txt" ]; then
    echo "Initializing shared_components scaffolding..."
    mkdir -p "${SHARED_CORE_DIR}/include"
    mkdir -p "${SHARED_CORE_DIR}/src"
    touch "${SHARED_CORE_DIR}/src/so_101_core.c"

    cat << 'EOF' > "${SHARED_CORE_DIR}/CMakeLists.txt"
idf_component_register(SRCS "src/so_101_core.c"
                       INCLUDE_DIRS "include"
                       REQUIRES micro_ros_espidf_component)                       
EOF

    # Generate Transitive Dependency Manifest
cat << 'EOF' > "${SHARED_CORE_DIR}/idf_component.yml"
dependencies:
  idf:
    version: ">=5.3.0"
  espressif/esp-dsp: "^1.4.9"
  joltwallet/littlefs: "^1.14.4"
EOF
fi

# 4. Node Project Scaffolding and Baseline Configurations

# --- LEADER NODE ---
if [ ! -d "leader" ]; then
    echo "Initializing 'leader' ESP-IDF project structure..."
    idf.py create-project leader
    
    # Overwrite default CMakeLists.txt to bind shared dependencies
    cat << 'EOF' > "leader/CMakeLists.txt"
cmake_minimum_required(VERSION 3.16)
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../shared_components")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(leader)
EOF

    # Generate Custom Partition Table (4MB Target)
    cat << 'EOF' > "leader/partitions.csv"
# Name,   Type, SubType, Offset,  Size, Flags
nvs,      data, nvs,     0x9000,  0x6000,
phy_init, data, phy,     0xf000,  0x1000,
factory,  app,  factory, 0x10000, 0x200000,
storage,  data, littlefs,0x210000,0x1E0000,
coredump, data, coredump,0x3F0000,0x10000,
EOF

    # Generate sdkconfig.defaults leveraging verified Kconfig parameters
    cat << 'EOF' > "leader/sdkconfig.defaults"
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
EOF
fi

# --- FOLLOWER NODE ---
if [ ! -d "follower" ]; then
    echo "Initializing 'follower' ESP-IDF project structure..."
    idf.py create-project follower
    
    # Overwrite default CMakeLists.txt to bind shared dependencies
    cat << 'EOF' > "follower/CMakeLists.txt"
cmake_minimum_required(VERSION 3.16)
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../shared_components")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(follower)
EOF

    # Generate Custom Partition Table (4MB Target)
    cat << 'EOF' > "follower/partitions.csv"
# Name,   Type, SubType, Offset,  Size, Flags
nvs,      data, nvs,     0x9000,  0x6000,
phy_init, data, phy,     0xf000,  0x1000,
factory,  app,  factory, 0x10000, 0x200000,
storage,  data, littlefs,0x210000,0x1E0000,
coredump, data, coredump,0x3F0000,0x10000,
EOF

    # Generate sdkconfig.defaults leveraging verified Kconfig parameters
    cat << 'EOF' > "follower/sdkconfig.defaults"
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
EOF
fi