"""Week2 test: normal goto task accepted when console is online."""

import os
import sys
import time
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import launch
import launch_ros.actions
import launch_testing
import launch_testing.actions
import pytest


@pytest.mark.launch_test
def generate_test_description():
    fake_robot = launch_ros.actions.Node(
        package="botcockpit_sim",
        executable="fake_robot",
        name="fake_robot",
        output="screen",
    )
    return launch.LaunchDescription([fake_robot, launch_testing.actions.ReadyToTest()])


class TestNormalTask(unittest.TestCase):
    def test_goto_accepted_when_console_online(self):
        import rclpy
        from rclpy.node import Node
        from std_msgs.msg import String

        from helpers import ConsoleBeater, lifecycle_configure_activate, send_cmd, wait_state

        rclpy.init()
        node = Node("test_normal_task")
        beater = None
        try:
            self.assertTrue(lifecycle_configure_activate(node), "lifecycle failed")
            beater = ConsoleBeater(node, online=True)
            time.sleep(0.3)

            state = wait_state(
                node,
                lambda s: s.get("phase") == "IDLE" and s.get("control_enabled") is True,
                timeout=6,
            )
            self.assertIsNotNone(state, f"expected IDLE+control_enabled, got {state}")

            cmd_pub = node.create_publisher(String, "/botcockpit/cmd", 10)
            time.sleep(0.3)
            ack = send_cmd(
                node,
                cmd_pub,
                "CMD_TASK",
                401,
                {"task_id": "T-N1", "type": "goto", "x": 1.0, "y": 0.5},
            )
            self.assertIsNotNone(ack, "no CMD_ACK for goto")
            self.assertTrue(ack.get("ok"), f"goto rejected: {ack}")

            state = wait_state(
                node,
                lambda s: s.get("phase") == "RUNNING"
                or (s.get("task") or {}).get("status") in ("ACCEPTED", "EXECUTING", "DONE"),
                timeout=5,
            )
            self.assertIsNotNone(state, f"no task activity state={state}")
        finally:
            if beater:
                beater.stop()
            node.destroy_node()
            rclpy.shutdown()


@launch_testing.post_shutdown_test()
class TestProcessOutput(unittest.TestCase):
    def test_exit_codes(self, proc_info):
        launch_testing.asserts.assertExitCodes(proc_info)
