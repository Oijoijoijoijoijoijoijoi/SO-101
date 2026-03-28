# 1. Base Layer (Immutable)
FROM ros:humble-ros-base

# 2. System Dependency Layer (Low Volatility)
# Consolidates all binary requirements to minimize image layers.
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

# 3. User Identity Layer (Static)
ARG USERNAME=vscode
ARG USER_UID=1000
ARG USER_GID=$USER_UID
RUN groupadd --gid $USER_GID $USERNAME \
    && useradd --uid $USER_UID --gid $USER_GID -m $USERNAME \
    && echo $USERNAME ALL=\(root\) NOPASSWD:ALL > /etc/sudoers.d/$USERNAME

# 4. Toolchain Layer (High Latency / High Inertia)
# We perform the heavy IDF installation before any volatile shell tweaks.
ENV IDF_PATH=/opt/esp/idf
ENV CCACHE_DIR=/workspaces/SO-101/.ccache

RUN mkdir -p /opt/esp && chown $USERNAME:$USERNAME /opt/esp
USER $USERNAME

RUN cd /opt/esp \
    && git clone --recursive --branch v5.3.1 https://github.com/espressif/esp-idf.git idf \
    && $IDF_PATH/install.sh all

# 5. Middleware Injection Layer (Moderate Volatility)
# This depends on the IDF installation but is independent of shell QOL.
RUN /bin/bash -c "source $IDF_PATH/export.sh && \
    pip install catkin_pkg lark pyparsing empy==3.3.4"

# 6. User Environment & QOL Layer (High Volatility)
# This is at the end. Tweaking aliases or shell history will NOT trigger a re-clone of the IDF.
# 6. User Environment & QOL Layer
RUN mkdir -p ~/command_history

# Use ~/ instead of /home/$USERNAME/ for runtime safety
RUN echo 'source /opt/ros/humble/setup.bash' >> ~/.bashrc \
    && echo 'if [ ! -d "~/command_history" ]; then mkdir -p ~/command_history; fi' >> ~/.bashrc \
    && echo 'export HISTFILE=~/command_history/.bash_history' >> ~/.bashrc \
    && echo "alias get_idf='. $IDF_PATH/export.sh'" >> ~/.bashrc \
    && echo "source $IDF_PATH/export.sh > /dev/null 2>&1" >> ~/.bashrc

WORKDIR /workspaces/SO-101