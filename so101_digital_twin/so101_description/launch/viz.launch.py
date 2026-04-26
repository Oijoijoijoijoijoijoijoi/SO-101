import os
from launch import LaunchDescription
from launch_ros.actions import Node
from launch.substitutions import Command
from launch_ros.parameter_descriptions import ParameterValue
from ament_index_python.packages import get_package_share_directory

def generate_launch_description():
    pkg_share = get_package_share_directory('so101_description')
    xacro_file = os.path.join(pkg_share, 'urdf', 'so101.urdf.xacro')

    # Helper function to eliminate code duplication for identical robots
    def create_rsp_node(namespace, is_leader_flag):
        # 1. Evaluate Xacro dynamically
        robot_desc = ParameterValue(
            Command(['xacro ', xacro_file, f' is_leader:={is_leader_flag}']),
            value_type=str
        )
        
        # 2. Return the configured Node
        return Node(
            package='robot_state_publisher',
            executable='robot_state_publisher',
            name=f'{namespace}_rsp',
            output='screen',
            parameters=[{
                'robot_description': robot_desc,
                'frame_prefix': f'{namespace}/',
                'publish_frequency': 50.0
            }],
            remappings=[
                # Map hardware telemetry
                ('/joint_states', f'/{namespace}/joint_states'),
                # CRITICAL: Isolate the XML payloads so Foxglove renders both meshes
                ('/robot_description', f'/{namespace}/robot_description') 
            ]
        )

    return LaunchDescription([
        # Instantiate Leader and Follower using the helper
        create_rsp_node(namespace='leader', is_leader_flag='true'),
        create_rsp_node(namespace='follower', is_leader_flag='false'),

        # Bridge the two coordinate trees in TF2 (0.8m Y-axis offset)
	Node(
            package='tf2_ros',
            executable='static_transform_publisher',
            name='twin_bridge',
            arguments=[
                '--x', '0', '--y', '0.8', '--z', '0', 
                '--yaw', '0', '--pitch', '0', '--roll', '0', 
                '--frame-id', 'leader/world', 
                '--child-frame-id', 'follower/world'
            ]
        ),

        # Expose data and local assets to Foxglove Studio
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
