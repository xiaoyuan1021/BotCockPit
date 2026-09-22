#!/usr/bin/env bash
set -e
source /opt/ros/humble/setup.bash
export CC=/usr/bin/gcc CXX=/usr/bin/g++
cd /home/xiaoyuan/ros2_ws
colcon build --packages-select botcockpit_sim botcockpit_bridge
source install/setup.bash
cd Bot/Bot_Project/ui/botcockpit
cmake --build build -j4
echo BUILD_OK
bash /tmp/plan_a_build_smoke.sh 2>/dev/null | tail -12
