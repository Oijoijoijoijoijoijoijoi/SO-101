# 1. Start from the Official ROS 2 Humble Base
FROM ros:humble-ros-base

# 2. Mandatory Tools & ESP-IDF Prerequisites
RUN apt-get update && export DEBIAN_FRONTEND=noninteractive \
    && apt-get install -y \
    python3-pip python3-venv \
    cmake ninja-build dfu-util \
    flex bison gperf \
    ccache libffi-dev libssl-dev libusb-1.0-0 \
    git wget curl sudo \
    # Mandatory build tools for micro-ROS compilation
    python3-colcon-common-extensions \
    python3-vcstool \
    && rm -rf /var/lib/apt/lists/*

# 3. User Context Management
ARG USERNAME=vscode
ARG USER_UID=1000
ARG USER_GID=$USER_UID
RUN groupadd --gid $USER_GID $USERNAME \
    && useradd --uid $USER_UID --gid $USER_GID -m $USERNAME \
    && echo $USERNAME ALL=\(root\) NOPASSWD:ALL > /etc/sudoers.d/$USERNAME

# 4. ESP-IDF v5.3.1 Installation
ENV IDF_PATH=/opt/esp/idf
RUN mkdir -p /opt/esp && chown $USERNAME:$USERNAME /opt/esp
USER $USERNAME
RUN cd /opt/esp \
    && git clone --recursive --branch v5.3.1 https://github.com/espressif/esp-idf.git idf \
    && $IDF_PATH/install.sh all

# 5. Environment Activation
RUN echo "alias get_idf='. $IDF_PATH/export.sh'" >> ~/.bashrc

WORKDIR /workspace