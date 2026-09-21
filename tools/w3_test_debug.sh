#!/usr/bin/env bash
source /opt/ros/humble/setup.bash
source /home/xiaoyuan/ros2_ws/install/setup.bash
echo "==== helpers.py wait_state ===="
grep -n "require_match\|def wait_state\|return None" /home/xiaoyuan/ros2_ws/Bot/Bot_Project/sim/test/helpers.py
echo "==== normal task log ===="
tail -50 /home/xiaoyuan/ros2_ws/build/botcockpit_sim/launch_test/test_test_normal_task.py.txt 2>/dev/null
echo "==== estop log tail ===="
tail -30 /home/xiaoyuan/ros2_ws/build/botcockpit_sim/launch_test/test_test_estop_lock.py.txt 2>/dev/null
