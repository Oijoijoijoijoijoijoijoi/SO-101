#!/bin/bash
set -e

# --- 1. ENVIRONMENT & DIRECTORY CONFIGURATION ---
WORKSPACE_DIR="/workspaces/SO-101"
ESP_EXPORT_SCRIPT="/opt/esp/idf/export.sh"

source "${ESP_EXPORT_SCRIPT}"
cd "${WORKSPACE_DIR}"

# --- 2. MIDDLEWARE & SUBMODULE RECONCILIATION ---
# Ensures the gitlink and metadata align with the custom fork
git config --global --add safe.directory "${WORKSPACE_DIR}"

echo "Reconciling submodules with custom fork configuration..."
git submodule sync --recursive
git submodule update --init --recursive

# Verification of Component Integrity (Mandatory if folders are wiped)
MICROROS_DIR="shared_components/micro_ros_espidf_component"
if [ ! -f "${MICROROS_DIR}/CMakeLists.txt" ]; then
    echo "Warning: micro-ROS component missing. Re-initializing from custom fork..."
    rm -rf "${MICROROS_DIR}"
    
    FORK_URL="https://github.com/Oijoijoijoijoijoijoijoi/micro_ros_espidf_component.git"
    CUSTOM_BRANCH="jazzy-custom-espnow"
    
    git submodule add --force -b "${CUSTOM_BRANCH}" "${FORK_URL}" "${MICROROS_DIR}"
    git submodule update --init --recursive
fi

# --- 3. BUILD SYSTEM PREPARATION ---
# Clean stale build artifacts that may persist across container rebuilds
echo "Cleaning workspace for build environment parity..."
find . -name "sdkconfig" -type f -delete
find . -name "dependencies.lock" -type f -delete

echo "Workspace initialization complete. Submodule hash: $(git submodule status | awk '{print $1}')"