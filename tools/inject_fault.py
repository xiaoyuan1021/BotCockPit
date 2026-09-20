#!/usr/bin/env python3
"""Week2 fault injection into fake_robot via ROS2 topic botcockpit/cmd.

Examples:
  python3 tools/inject_fault.py
  python3 tools/inject_fault.py --detail "lidar lost"
  python3 tools/inject_fault.py --clear
"""

from __future__ import annotations

import argparse
import json
import sys
import time


def main() -> int:
    parser = argparse.ArgumentParser(description="Inject/clear fake_robot faults")
    parser.add_argument("--detail", default="injected by tools/inject_fault.py")
    parser.add_argument("--clear", action="store_true", help="CLEAR_FAULT instead of inject")
    parser.add_argument("--seq", type=int, default=int(time.time()) % 65000)
    args = parser.parse_args()

    try:
        import rclpy
        from rclpy.node import Node
        from std_msgs.msg import String
    except ImportError:
        print("rclpy not available — run on ROS2 host after sourcing setup.bash", file=sys.stderr)
        return 1

    cmd = "CLEAR_FAULT" if args.clear else "INJECT_FAULT"
    payload = {} if args.clear else {"detail": args.detail}
    envelope = {"seq": args.seq, "cmd": cmd, "payload": payload}

    rclpy.init()
    node = Node("botcockpit_inject_fault")
    pub = node.create_publisher(String, "botcockpit/cmd", 10)
    # ensure discovery
    time.sleep(0.5)
    msg = String()
    msg.data = json.dumps(envelope, separators=(",", ":"))
    for _ in range(5):
        pub.publish(msg)
        rclpy.spin_once(node, timeout_sec=0.05)
        time.sleep(0.05)
    print(f"published {cmd} seq={args.seq} -> botcockpit/cmd")
    print(msg.data)
    node.destroy_node()
    rclpy.shutdown()
    return 0


if __name__ == "__main__":
    sys.exit(main())
