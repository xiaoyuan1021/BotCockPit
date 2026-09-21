#!/usr/bin/env bash
source /opt/ros/humble/setup.bash
source /home/xiaoyuan/ros2_ws/install/setup.bash

# Ensure console online so robot accepts tasks
python3 - <<'PY' &
import json, time
import rclpy
from rclpy.node import Node
from std_msgs.msg import String
rclpy.init()
n = Node("console_beater_dbg")
p = n.create_publisher(String, "/botcockpit/console", 10)
end = time.time() + 12
while time.time() < end:
    p.publish(String(data='{"online":true}'))
    rclpy.spin_once(n, timeout_sec=0.05)
    time.sleep(0.2)
n.destroy_node()
rclpy.shutdown()
PY
BEATER=$!

sleep 0.6
echo "==== state before ===="
timeout 2 ros2 topic echo /botcockpit/state --once

python3 - <<'PY'
import json, time
import rclpy
from rclpy.node import Node
from std_msgs.msg import String
rclpy.init()
n = Node("goto_dbg")
pub = n.create_publisher(String, "/botcockpit/cmd", 10)
states = []

def cb(msg):
    try:
        states.append(json.loads(msg.data))
    except Exception:
        pass

sub = n.create_subscription(String, "/botcockpit/state", cb, 10)
cmd = json.dumps({
    "seq": 9001,
    "cmd": "CMD_TASK",
    "payload": {"task_id": "T-DBG", "type": "goto", "x": 6.0, "y": 0.0, "timeout_s": 30},
})
acks = []
def rcb(msg):
    try:
        obj = json.loads(msg.data)
    except Exception:
        return
    if obj.get("seq") == 9001:
        acks.append(obj)
sub2 = n.create_subscription(String, "/botcockpit/cmd_result", rcb, 10)

# wait idleness + control
t0 = time.time()
last = None
while time.time() - t0 < 6:
    rclpy.spin_once(n, timeout_sec=0.05)
    if states:
        last = states[-1]
        if last.get("phase") == "IDLE" and last.get("control_enabled"):
            break
print("pre-state:", {k: last.get(k) for k in ("phase","control_enabled","conn","mode")} if last else None)

for _ in range(8):
    pub.publish(String(data=cmd))
    rclpy.spin_once(n, timeout_sec=0.05)
    time.sleep(0.05)

t1 = time.time()
samples = []
while time.time() - t1 < 4:
    rclpy.spin_once(n, timeout_sec=0.05)
    if states:
        s = states[-1]
        samples.append((s.get("phase"), (s.get("task") or {}).get("status"), s.get("control_enabled"), (s.get("pose") or {}).get("x")))

print("ack:", acks[:1])
print("phase samples:", samples[:12])
n.destroy_node()
rclpy.shutdown()
PY

wait $BEATER 2>/dev/null || true
echo DBG_DONE
