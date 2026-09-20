"""Week2 test: ESTOP locks AUTO/tasks until successful RESET."""

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


class TestEstopLock(unittest.TestCase):
    def test_estop_rejects_until_reset(self):
        import rclpy
        from rclpy.node import Node
        from std_msgs.msg import String

        from helpers import ConsoleBeater, lifecycle_configure_activate, send_cmd, wait_state

        rclpy.init()
        node = Node("test_estop_lock")
        beater = None
        try:
            self.assertTrue(lifecycle_configure_activate(node))
            beater = ConsoleBeater(node, True)
            state = wait_state(
                node,
                lambda s: s.get("phase") == "IDLE" and s.get("control_enabled") is True,
                timeout=6,
            )
            self.assertIsNotNone(state, f"setup state={state}")

            cmd_pub = node.create_publisher(String, "/botcockpit/cmd", 10)
            time.sleep(0.3)

            ack = send_cmd(node, cmd_pub, "CMD_ESTOP", 501, {"reason": "test"})
            self.assertIsNotNone(ack, "no ACK for ESTOP")
            self.assertTrue(ack.get("ok"), ack)

            state = wait_state(
                node,
                lambda s: s.get("estop") is True and s.get("phase") == "ESTOP",
                timeout=3,
            )
            self.assertIsNotNone(state, f"estop state={state}")

            ack_task = send_cmd(
                node,
                cmd_pub,
                "CMD_TASK",
                502,
                {"task_id": "T-E1", "type": "goto", "x": 0.2, "y": 0.0},
            )
            self.assertIsNotNone(ack_task)
            self.assertFalse(ack_task.get("ok"), f"task should reject under ESTOP: {ack_task}")

            ack_mode = send_cmd(node, cmd_pub, "CMD_MODE", 503, {"mode": "AUTO"})
            self.assertIsNotNone(ack_mode)
            self.assertFalse(ack_mode.get("ok"), f"AUTO should reject under ESTOP: {ack_mode}")

            ack_r0 = send_cmd(node, cmd_pub, "CMD_RESET", 504, {})
            self.assertIsNotNone(ack_r0)
            self.assertFalse(ack_r0.get("ok"))
            self.assertEqual(ack_r0.get("reason"), "confirm_required")

            ack_r1 = send_cmd(node, cmd_pub, "CMD_RESET", 505, {"confirm": True})
            self.assertIsNotNone(ack_r1)
            self.assertTrue(ack_r1.get("ok"), f"reset failed: {ack_r1}")

            state = wait_state(
                node,
                lambda s: s.get("estop") is False
                and s.get("phase") == "IDLE"
                and s.get("control_enabled") is True,
                timeout=5,
            )
            self.assertIsNotNone(state, f"after reset state={state}")
        finally:
            if beater:
                beater.stop()
            node.destroy_node()
            rclpy.shutdown()


@launch_testing.post_shutdown_test()
class TestProcessOutput(unittest.TestCase):
    def test_exit_codes(self, proc_info):
        launch_testing.asserts.assertExitCodes(proc_info)
