#!/usr/bin/env bash
cd /home/xiaoyuan/ros2_ws/Bot/Bot_Project/ui/botcockpit
cmake --build build -j4
echo "--- processes ---"
ps aux | grep -E 'fake_robot|botcockpit_bridge|botcockpit' | grep -v grep || echo NO_BACKEND_OR_UI
echo "--- bridge.log ---"
tail -20 /tmp/bridge.log 2>/dev/null || true
echo "--- fake_robot.log ---"
tail -15 /tmp/fake_robot.log 2>/dev/null || true
echo "--- port ---"
ss -lptn | grep 8765 || echo PORT_8765_FREE
