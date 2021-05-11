#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""Launch description for scenario runner moc."""

# Copyright (c) 2020 Tier IV, Inc. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import os

from ament_index_python.packages import get_package_share_directory

from launch import LaunchDescription

from launch.actions import IncludeLaunchDescription

from launch.launch_description_sources import PythonLaunchDescriptionSource

from launch_ros.actions import Node


def generate_launch_description():
    """Launch description for scenario runner moc."""
    lanlet_path = os.path.join(
        get_package_share_directory("kashiwanoha_map"), "map", "lanelet2_map.osm"
    )
    rviz_config_dir = os.path.join(
        get_package_share_directory("cpp_mock_scenarios"),
        "rviz",
        "view_kashiwanoha.rviz",
    )
    joy_to_cmd_package_path = get_package_share_directory("joy_to_vehicle_cmd")
    joy_to_cmd_launch_dir = os.path.join(joy_to_cmd_package_path, "launch")
    return LaunchDescription(
        [
            Node(
                package="cpp_mock_scenarios",
                executable="manual_drive_kashiwanoha",
                name="manual_kashiwanoha",
                output="screen",
                parameters=[
                    {
                        "map_path": lanlet_path,
                        "origin_latitude": 35.903555800615614,
                        "origin_longitude": 139.93339979022568,
                        "port": 8080,
                    }
                ],
                arguments=[("__log_level:=info")],
            ),
            Node(
                package="simple_sensor_simulator",
                executable="simple_sensor_simulator_node",
                name="simple_sensor_simulator_node",
                output="screen",
                parameters=[{"port": 8080}],
                arguments=[("__log_level:=warn")],
            ),
            Node(
                package="rviz2",
                executable="rviz2",
                name="rviz2",
                arguments=["-d", rviz_config_dir],
                output="screen",
            ),
            Node(
                package="openscenario_visualization",
                executable="openscenario_visualization_node",
                name="openscenario_visualization_node",
                output="screen",
            ),
            IncludeLaunchDescription(
                PythonLaunchDescriptionSource(
                    [joy_to_cmd_launch_dir, "/joy_to_vehicle_cmd.launch.py"]
                ),
            ),
        ]
    )
