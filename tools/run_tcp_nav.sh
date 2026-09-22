#!/usr/bin/env bash
set -e
source /opt/ros/humble/setup.bash
source /home/xiaoyuan/ros2_ws/install/setup.bash
pkill -9 -f 'fake_robot|botcockpit_bridge' 2>/dev/null || true
sleep 0.4
ros2 run botcockpit_sim fake_robot >/tmp/fr.log 2>&1 &
sleep 1
ros2 lifecycle set /fake_robot configure
ros2 lifecycle set /fake_robot activate
ros2 run botcockpit_bridge bridge --port 8765 >/tmp/br.log 2>&1 &
sleep 1
cd /home/xiaoyuan/ros2_ws/Bot/Bot_Project/tools
python3 test_tcp_nav.py
pkill -9 -f 'fake_robot|botcockpit_bridge' 2>/dev/null || true
echo TCP_DONE
