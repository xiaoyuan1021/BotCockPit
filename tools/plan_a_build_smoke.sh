#!/usr/bin/env bash
set -e
source /opt/ros/humble/setup.bash
export CC=/usr/bin/gcc CXX=/usr/bin/g++
cd /home/xiaoyuan/ros2_ws
colcon build --packages-select botcockpit_bridge botcockpit_sim
source install/setup.bash
cd Bot/Bot_Project/ui/botcockpit
cmake -B build -DCMAKE_CXX_COMPILER=/usr/bin/g++
cmake --build build -j4
echo BUILD_ALL_OK
# clean robot + run astar already passed on windows
pkill -9 -f 'fake_robot|botcockpit_bridge' 2>/dev/null || true
sleep 0.3
ros2 run botcockpit_sim fake_robot >/tmp/fr.log 2>&1 &
sleep 1
ros2 lifecycle set /fake_robot configure
ros2 lifecycle set /fake_robot activate
python3 - <<'PY'
import json, time
import rclpy
from rclpy.node import Node
from std_msgs.msg import String
rclpy.init()
n = Node("nav_smoke")
# console online
pc = n.create_publisher(String, "/botcockpit/console", 10)
cmdp = n.create_publisher(String, "/botcockpit/cmd", 10)
last = {}
def scb(m):
    try:
        last.clear(); last.update(json.loads(m.data))
    except Exception:
        pass
n.create_subscription(String, "/botcockpit/state", scb, 10)
acks = {}
def acb(m):
    try:
        o = json.loads(m.data)
        if o.get("seq")==9002:
            acks["ack"]=o
    except Exception:
        pass
n.create_subscription(String, "/botcockpit/cmd_result", acb, 10)
t0=time.time()
while time.time()-t0<5:
    pc.publish(String(data='{"online":true}'))
    rclpy.spin_once(n, timeout_sec=0.05)
    if last.get("phase")=="IDLE" and last.get("control_enabled"):
        break
cmdp.publish(String(data=json.dumps({"seq":9002,"cmd":"CMD_TASK","payload":{"task_id":"T-nav","type":"goto","x":18.0,"y":0.0}})))
t1=time.time()
samples=[]
while time.time()-t1<8:
    rclpy.spin_once(n, timeout_sec=0.05)
    nav=last.get("nav") or {}
    samples.append((last.get("phase"), last.get("task",{}).get("status"), nav.get("status"), nav.get("path_len"), (last.get("pose") or {}).get("x")))
print("ack", acks.get("ack"))
print("samples", samples[:15])
ok = acks.get("ack") and acks["ack"].get("ok") and any(s[2] in ("TRACKING","PLANNING") for s in samples)
print("NAV_SMOKE", "PASS" if ok else "CHECK")
n.destroy_node(); rclpy.shutdown()
PY
pkill -9 -f 'fake_robot' 2>/dev/null || true
