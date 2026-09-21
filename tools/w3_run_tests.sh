#!/usr/bin/env bash
set -e
source /opt/ros/humble/setup.bash
source /home/xiaoyuan/ros2_ws/install/setup.bash
pkill -9 -f 'fake_robot|botcockpit_bridge' 2>/dev/null || true
sleep 0.4
cd /home/xiaoyuan/ros2_ws/build/botcockpit_sim
ctest --output-on-failure 2>&1 | tail -40
