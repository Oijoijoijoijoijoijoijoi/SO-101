FROM ros:jazzy-ros-base

# 1. Permission Alignment Block
ARG UNAME=user
ARG UID=1000
ARG GID=1000

RUN groupadd -g ${GID} -o ${UNAME} \
    && useradd -m -u ${UID} -g ${GID} -o -s /bin/bash ${UNAME} \
    && apt-get update && apt-get install -y sudo \
    && echo "${UNAME} ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers

# 2. Jazzy-Specific Dependency Block
# Added 'ros-jazzy-xacro' for URDF processing
RUN apt-get update && apt-get install -y --no-install-recommends \
    ros-jazzy-foxglove-bridge \
    ros-jazzy-robot-state-publisher \
    ros-jazzy-joint-state-publisher \
    ros-jazzy-joint-state-publisher-gui \
    ros-jazzy-tf2-ros \
    ros-jazzy-xacro \
    ros-jazzy-ament-cmake \
    python3-colcon-common-extensions \
    build-essential \
    python3-pip \
    && rm -rf /var/lib/apt/lists/*

USER ${UNAME}
WORKDIR /home/${UNAME}/so101_ws

# 3. Environment Path Redirection
RUN echo "source /opt/ros/jazzy/setup.bash" >> ~/.bashrc

# Auto-source workspace overlay if compiled
RUN echo "if [ -f /home/${UNAME}/so101_ws/install/setup.bash ]; then source /home/${UNAME}/so101_ws/install/setup.bash; fi" >> ~/.bashrc

# 4. High-Frequency Telemetry Command
# 'send_buffer_limit' is increased to handle 200Hz bursts of JointTrajectory points
CMD ["ros2", "launch", "foxglove_bridge", "foxglove_bridge_launch.xml", "send_buffer_limit:=10000000"]
