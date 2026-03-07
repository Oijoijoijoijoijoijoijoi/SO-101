FROM mcr.microsoft.com/devcontainers/python:3.12

# Install system dependencies for micro-ROS
RUN apt-get update && apt-get install -y \
    cmake \
    build-essential \
    && rm -rf /var/lib/apt/lists/*

# Install the Python tools that caused us trouble earlier
# We do this inside the container so your Host OS stays clean
RUN pip3 install --no-cache-dir \
    platformio \
    empy==3.3.4 \
    catkin_pkg \
    lark

# Add PlatformIO to PATH
ENV PATH="/home/vscode/.local/bin:$PATH"