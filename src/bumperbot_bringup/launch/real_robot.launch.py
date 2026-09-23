import os

from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription, DeclareLaunchArgument
from launch.conditions import IfCondition, UnlessCondition
from launch.substitutions import LaunchConfiguration

from launch_ros.actions import Node

from ament_index_python.packages import get_package_share_directory


def generate_launch_description():

    use_slam = LaunchConfiguration("use_slam")
    use_sim_time = LaunchConfiguration("use_sim_time")


    # =========================
    # Launch Arguments
    # =========================

    use_slam_arg = DeclareLaunchArgument(
        "use_slam",
        default_value="false"
    )

    # Real robot MUST use system time, not Gazebo /clock
    use_sim_time_arg = DeclareLaunchArgument(
        "use_sim_time",
        default_value="false"
    )


    # =========================
    # Hardware Interface
    # =========================

    hardware_interface = IncludeLaunchDescription(
        os.path.join(
            get_package_share_directory("bumperbot_firmware"),
            "launch",
            "hardware_interface.launch.py"
        ),
        launch_arguments={
            "use_sim_time": use_sim_time
        }.items(),
    )


    # =========================
    # LiDAR
    # =========================

    laser_driver = Node(
        package="rplidar_ros",
        executable="rplidar_node",
        name="rplidar_node",
        parameters=[
            os.path.join(
                get_package_share_directory("bumperbot_bringup"),
                "config",
                "rplidar_a1.yaml"
            ),
            {
                "use_sim_time": use_sim_time
            }
        ],
        output="screen"
    )


    # =========================
    # ros2_control Controllers
    # =========================

    controller = IncludeLaunchDescription(
        os.path.join(
            get_package_share_directory("bumperbot_controller"),
            "launch",
            "controller.launch.py"
        ),
        launch_arguments={
            "use_simple_controller": "false",
            "use_python": "false",
            "use_sim_time": use_sim_time
        }.items(),
    )


    # =========================
    # Joystick
    # =========================

    joystick = IncludeLaunchDescription(
        os.path.join(
            get_package_share_directory("bumperbot_controller"),
            "launch",
            "joystick_teleop_linux.launch.py"
        ),
        launch_arguments={
            "use_sim_time": use_sim_time
        }.items(),
    )


    # =========================
    # Localization
    # =========================

    localization = IncludeLaunchDescription(
        os.path.join(
            get_package_share_directory("bumperbot_localization"),
            "launch",
            "global_localization.launch.py"
        ),
        launch_arguments={
            "use_sim_time": use_sim_time
        }.items(),
        condition=UnlessCondition(use_slam)
    )


    # =========================
    # SLAM
    # =========================

    slam = IncludeLaunchDescription(
        os.path.join(
            get_package_share_directory("bumperbot_mapping"),
            "launch",
            "slam.launch.py"
        ),
        launch_arguments={
            "use_sim_time": use_sim_time
        }.items(),
        condition=IfCondition(use_slam)
    )


    # =========================
    # Navigation
    # =========================

    navigation = IncludeLaunchDescription(
        os.path.join(
            get_package_share_directory("bumperbot_navigation"),
            "launch",
            "navigation.launch.py"
        ),
        launch_arguments={
            "use_sim_time": use_sim_time
        }.items(),
    )


    return LaunchDescription([
        use_slam_arg,
        use_sim_time_arg,

        hardware_interface,
        # laser_driver,
        controller,
        joystick,

        # localization,
        # slam,
        # navigation,
    ])