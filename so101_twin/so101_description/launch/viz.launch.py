import os
from launch import LaunchDescription
from launch_ros.actions import Node

def generate_launch_description():
    # Internal container path
    urdf_path = '/home/user/so101_ws/so101_description/urdf/so101.urdf'
    
    if not os.path.exists(urdf_path):
        raise FileNotFoundError(f"URDF not found at internal path: {urdf_path}")

    with open(urdf_path, 'r') as infp:
        robot_desc = infp.read()

    return LaunchDescription([
        Node(
            package='robot_state_publisher',
            executable='robot_state_publisher',
            name='robot_state_publisher',
            output='screen',
            parameters=[{'robot_description': robot_desc,
			'publish_frequency': 30.0}]
        ),
        Node(
            package='foxglove_bridge',
            executable='foxglove_bridge',
            name='foxglove_bridge',
            parameters=[{
                'port': 8765,
                'asset_uri_allowlist': ['^package://.*']
            }]
        )
    ])
