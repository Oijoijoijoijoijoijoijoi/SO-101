#!/bin/bash
set -e

# --- 1. GLOBAL CONFIGURATION & INTERFACE DEFINITIONS ---
# Define the absolute mount root and the specific git project root
MOUNT_ROOT="/workspaces/SO-101"
PROJECT_ROOT="${MOUNT_ROOT}/esp32_dev"
ESP_EXPORT_SCRIPT="/opt/esp/idf/export.sh"

# Middleware Configuration
MICROROS_DIR="shared_components/micro_ros_espidf_component"
FORK_URL="https://github.com/Oijoijoijoijoijoijoijoi/micro_ros_espidf_component.git"
CUSTOM_BRANCH="jazzy-custom-espnow"

# --- 2. GIT SECURITY & BOUNDARY NEUTRALIZATION ---
# Bypass "Dubious Ownership" and "Filesystem Boundary" restrictions (Error 128)
export GIT_DISCOVERY_ACROSS_FILESYSTEM=1
git config --global --add safe.directory "*"

# --- 3. ENVIRONMENT INVOCATION ---
# Initialize ESP-IDF toolchain variables for the current shell session
if [ -f "${ESP_EXPORT_SCRIPT}" ]; then
    source "${ESP_EXPORT_SCRIPT}"
else
    echo "Fatal: ESP-IDF export script not found at ${ESP_EXPORT_SCRIPT}"
    exit 1
fi

# Navigate to the Project Root (containing .git metadata)
cd "${PROJECT_ROOT}"

# --- 4. SUBMODULE LIFECYCLE RECONCILIATION ---
echo "Initiating Submodule Synchronization Phase..."

# Synchronize remote URLs with local .gitmodules configuration
git submodule sync --recursive

# Update and initialize submodules according to the index
git submodule update --init --recursive

# Component Integrity Check: Re-initialize micro-ROS if metadata is absent
if [ ! -f "${MICROROS_DIR}/CMakeLists.txt" ]; then
    echo "Warning: Component ${MICROROS_DIR} invalid. Re-indexing from remote..."
    
    # Remove potentially corrupted directory
    rm -rf "${MICROROS_DIR}"
    
    # Inject the custom fork specifically for this workspace instance
    git submodule add --force -b "${CUSTOM_BRANCH}" "${FORK_URL}" "${MICROROS_DIR}"
    git submodule update --init --recursive
fi

# --- 5. BUILD ARTIFACT PURGE (STATELESS PARITY) ---
# Eliminate project-specific build configurations and locks to prevent 
# cache pollution across container rebuilds.
echo "Executing Artifact Purge..."

# Delete all sdkconfig and lock files recursively within the project root
find . -name "sdkconfig" -type f -delete
find . -name "dependencies.lock" -type f -delete

# --- 6. OPERATIONAL SUMMARY ---
SUBMODULE_HASH=$(git submodule status | awk '{print $1}' | head -n 1)
echo "------------------------------------------------------------"
echo "Workspace Initialization Success."
echo "Current Project Context: ${PROJECT_ROOT}"
echo "Submodule Head Hash: ${SUBMODULE_HASH}"
echo "------------------------------------------------------------"