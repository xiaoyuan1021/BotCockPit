#!/usr/bin/env bash
source /opt/ros/humble/setup.bash
source /home/xiaoyuan/ros2_ws/install/setup.bash
pkill -9 -f 'botcockpit_bridge|fake_robot' 2>/dev/null || true
sleep 0.5
ros2 run botcockpit_sim fake_robot >/tmp/fake_robot.log 2>&1 &
sleep 1.2
ros2 lifecycle set /fake_robot configure
ros2 lifecycle set /fake_robot activate
ros2 run botcockpit_bridge bridge --port 8765 >/tmp/bridge.log 2>&1 &
sleep 1.2
echo "--- port ---"
ss -lptn | grep 8765 || echo NO_LISTEN
echo "--- state ---"
timeout 2 ros2 topic echo /botcockpit/state --once 2>/dev/null | head -c 260
echo
echo BACKEND_READY
echo "open UI:"
echo "  cd /home/xiaoyuan/ros2_ws/Bot/Bot_Project/ui/botcockpit && ./build/botcockpit"
