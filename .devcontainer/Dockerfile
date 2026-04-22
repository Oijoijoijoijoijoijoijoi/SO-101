# 1. Base Layer: Official ROS 2 Jazzy (Ubuntu 24.04 Noble)
FROM ros:jazzy-ros-base

# 2. System Dependency Layer
RUN apt-get update && export DEBIAN_FRONTEND=noninteractive \
    && apt-get install -y \
    python3-pip python3-venv \
    cmake ninja-build dfu-util \
    flex bison gperf \
    ccache libffi-dev libssl-dev libusb-1.0-0 \
    git wget curl sudo \
    python3-colcon-common-extensions \
    python3-vcstool \
    && rm -rf /var/lib/apt/lists/*

# 3. User Identity Layer
# Synchronizes UID/GID 1000 with the 'vscode' identity
ARG USERNAME=vscode
ARG USER_UID=1000
ARG USER_GID=1000

RUN if getent passwd $USER_UID > /dev/null; then \
        # UID exists: Rename user and update home directory
        OLD_USER=$(getent passwd $USER_UID | cut -d: -f1); \
        usermod -l $USERNAME $OLD_USER; \
        usermod -d /home/$USERNAME -m $USERNAME; \
    else \
        useradd --uid $USER_UID -m $USERNAME; \
    fi \
    && if getent group $USER_GID > /dev/null; then \
        # GID exists: Rename group to match $USERNAME
        OLD_GROUP=$(getent group $USER_GID | cut -d: -f1); \
        groupmod -n $USERNAME $OLD_GROUP; \
    else \
        groupadd --gid $USER_GID $USERNAME; \
    fi \
    && usermod -g $USERNAME $USERNAME \
    && echo $USERNAME ALL=\(root\) NOPASSWD:ALL > /etc/sudoers.d/$USERNAME \
    && chmod 0440 /etc/sudoers.d/$USERNAME

# 4. ESP-IDF Toolchain Layer
ENV IDF_PATH=/opt/esp/idf
RUN mkdir -p /opt/esp && chown $USERNAME:$USERNAME /opt/esp

USER $USERNAME
RUN cd /opt/esp \
    && git clone --recursive --branch v5.3.1 https://github.com/espressif/esp-idf.git idf \
    && $IDF_PATH/install.sh all

# 5. Middleware Injection Layer (Jazzy/micro-ROS requirements)
RUN /bin/bash -c "source $IDF_PATH/export.sh && \
    pip install --no-cache-dir \
    catkin_pkg \
    lark \
    pyparsing \
    empy==3.3.4 \
    colcon-library-path \
    colcon-mixin \
    colcon-common-extensions \
    colcon-cmake \
    colcon-ros"

# 6. Environment & QOL Layer
RUN echo 'source /opt/ros/jazzy/setup.bash' >> ~/.bashrc \
    && echo "alias get_idf='. $IDF_PATH/export.sh'" >> ~/.bashrc \
    && echo "source $IDF_PATH/export.sh > /dev/null 2>&1" >> ~/.bashrc

WORKDIR /workspaces/SO-101