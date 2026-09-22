#!/usr/bin/env bash
# One-shot: fake_robot + bridge + QML for local desktop VM
set -e
source /opt/ros/humble/setup.bash
source /home/xiaoyuan/ros2_ws/install/setup.bash

pkill -9 -f 'botcockpit_bridge|fake_robot|botcockpit' 2>/dev/null || true
sleep 0.4

echo "[1/3] fake_robot"
ros2 run botcockpit_sim fake_robot >/tmp/fake_robot.log 2>&1 &
sleep 1
ros2 lifecycle set /fake_robot configure
ros2 lifecycle set /fake_robot activate
echo "      state: $(timeout 2 ros2 topic echo /botcockpit/state --once 2>/dev/null | head -c 120)"

echo "[2/3] bridge :8765"
ros2 run botcockpit_bridge bridge --port 8765 >/tmp/bridge.log 2>&1 &
sleep 1
ss -lptn | grep 8765 || echo "      WARN: 8765 not listening — see /tmp/bridge.log"

echo "[3/3] QML console"
cd /home/xiaoyuan/ros2_ws/Bot/Bot_Project/ui/botcockpit
if [[ ! -x build/botcockpit ]]; then
  cmake -B build -DCMAKE_CXX_COMPILER=/usr/bin/g++
  cmake --build build -j4
fi
export QT_QPA_PLATFORM=xcb
./build/botcockpit >/tmp/botcockpit_ui.log 2>&1 &
echo "      UI pid $!"
echo "Logs: /tmp/fake_robot.log /tmp/bridge.log /tmp/botcockpit_ui.log"
echo "Open Control tab to see map+robot."
