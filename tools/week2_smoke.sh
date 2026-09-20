#!/usr/bin/env bash
set -e
source /opt/ros/humble/setup.bash
source /home/xiaoyuan/ros2_ws/install/setup.bash
pkill -9 -f 'botcockpit_bridge|fake_robot' 2>/dev/null || true
sleep 0.5
ros2 run botcockpit_sim fake_robot >/tmp/fake_robot.log 2>&1 &
FRPID=$!
sleep 1
ros2 lifecycle set /fake_robot configure || true
ros2 lifecycle set /fake_robot activate || true
ros2 run botcockpit_bridge bridge --port 8765 >/tmp/bridge.log 2>&1 &
BRPID=$!
sleep 1
cd /home/xiaoyuan/ros2_ws/Bot/Bot_Project
python3 tools/fake_client.py --host 127.0.0.1 --port 8765 --duration 5 >/tmp/fc.log 2>&1 || true
echo "==== fake_client ===="
grep -E "HELLO_ACK|summary|CMD_" /tmp/fc.log | head -15
echo "==== inject ===="
python3 tools/inject_fault.py --detail week2-smoke || true
sleep 1.2
ros2 topic echo /botcockpit/state --once 2>/dev/null | head -c 400 || true
echo
python3 tools/inject_fault.py --clear || true
kill $FRPID $BRPID 2>/dev/null || true
sleep 0.5
pkill -9 -f 'botcockpit_bridge|fake_robot' 2>/dev/null || true
ss -lptn | grep 8765 || echo "port 8765 free"
echo SMOKE_DONE
