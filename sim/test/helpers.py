"""Shared helpers for Week2 launch_testing (rclpy)."""

from __future__ import annotations

import json
import threading
import time
from typing import Any, Callable, Dict, Optional


class ConsoleBeater:
    """Continuously publish botcockpit/console (robot expects ~5Hz presence)."""

    def __init__(self, node, online: bool = True):
        from std_msgs.msg import String

        self._node = node
        self._pub = node.create_publisher(String, "/botcockpit/console", 10)
        self.online = online
        self._stop = False
        self._thread = threading.Thread(target=self._run, daemon=True)
        self._thread.start()

    def _run(self):
        from std_msgs.msg import String

        while not self._stop:
            msg = String()
            msg.data = '{"online":%s}' % ("true" if self.online else "false")
            try:
                self._pub.publish(msg)
            except Exception:
                break
            time.sleep(0.2)

    def set_online(self, online: bool) -> None:
        self.online = bool(online)

    def stop(self) -> None:
        self._stop = True


def lifecycle_configure_activate(node, node_name: str = "fake_robot", timeout: float = 5.0) -> bool:
    from lifecycle_msgs.msg import Transition
    from lifecycle_msgs.srv import ChangeState
    import rclpy

    client = node.create_client(ChangeState, f"/{node_name}/change_state")
    if not client.wait_for_service(timeout_sec=timeout):
        return False

    def _call(transition_id: int) -> bool:
        req = ChangeState.Request()
        req.transition.id = transition_id
        future = client.call_async(req)
        deadline = time.time() + timeout
        while time.time() < deadline and not future.done():
            rclpy.spin_once(node, timeout_sec=0.05)
        return future.done() and future.result() is not None and future.result().success

    return _call(Transition.TRANSITION_CONFIGURE) and _call(Transition.TRANSITION_ACTIVATE)


def wait_state(node, predicate, timeout: float = 5.0) -> Optional[Dict[str, Any]]:
    from std_msgs.msg import String
    import rclpy

    box: Dict[str, Any] = {}

    def _cb(msg: String) -> None:
        try:
            box["state"] = json.loads(msg.data)
        except Exception:
            return

    sub = node.create_subscription(String, "/botcockpit/state", _cb, 10)
    end = time.time() + timeout
    last = None
    while time.time() < end:
        rclpy.spin_once(node, timeout_sec=0.05)
        last = box.get("state")
        if last is not None and predicate(last):
            break
    node.destroy_subscription(sub)
    return last


def send_cmd(node, cmd_pub, cmd: str, seq: int, payload: dict, timeout: float = 4.0):
    """Subscribe to cmd_result first, publish repeatedly, wait for matching seq."""
    from std_msgs.msg import String
    import rclpy

    box: Dict[str, Any] = {}

    def _cb(msg: String) -> None:
        try:
            obj = json.loads(msg.data)
        except Exception:
            return
        if obj.get("seq") == seq:
            box["ack"] = obj

    sub = node.create_subscription(String, "/botcockpit/cmd_result", _cb, 20)
    msg = String()
    msg.data = json.dumps({"seq": seq, "cmd": cmd, "payload": payload})
    end = time.time() + timeout
    while time.time() < end and "ack" not in box:
        cmd_pub.publish(msg)
        rclpy.spin_once(node, timeout_sec=0.05)
        time.sleep(0.03)
    node.destroy_subscription(sub)
    return box.get("ack")
