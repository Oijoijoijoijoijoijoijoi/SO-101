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
# Executes only if the custom component CMakeLists.txt is absent
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
  espressif/esp_littlefs: "^1.14.4"
EOF
fi

# 4. Node Project Scaffolding
# Executes only if the project directories are absent
if [ ! -d "leader" ]; then
    echo "Initializing 'leader' ESP-IDF project structure..."
    idf.py create-project leader
fi

if [ ! -d "follower" ]; then
    echo "Initializing 'follower' ESP-IDF project structure..."
    idf.py create-project follower
fi