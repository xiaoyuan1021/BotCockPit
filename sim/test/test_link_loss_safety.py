"""Week2 test: console link loss → SAFE stop + control_enabled false."""

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


class TestLinkLossSafety(unittest.TestCase):
    def test_console_offline_disables_control(self):
        import rclpy
        from rclpy.node import Node
        from std_msgs.msg import String

        from helpers import ConsoleBeater, lifecycle_configure_activate, send_cmd, wait_state

        rclpy.init()
        node = Node("test_link_loss")
        beater = None
        try:
            self.assertTrue(lifecycle_configure_activate(node))
            beater = ConsoleBeater(node, True)
            state = wait_state(
                node,
                lambda s: s.get("phase") == "IDLE" and s.get("control_enabled") is True,
                timeout=6,
            )
            self.assertIsNotNone(state, f"setup={state}")

            cmd_pub = node.create_publisher(String, "/botcockpit/cmd", 10)
            time.sleep(0.3)
            ack = send_cmd(
                node,
                cmd_pub,
                "CMD_TASK",
                601,
                {"task_id": "T-L1", "type": "goto", "x": 5.0, "y": 0.0},
            )
            self.assertIsNotNone(ack, "no ACK for goto")
            self.assertTrue(ack.get("ok"), ack)

            state = wait_state(node, lambda s: s.get("phase") == "RUNNING", timeout=4)
            self.assertIsNotNone(state, f"expected RUNNING, got {state}")

            # Simulate bridge/console loss (stop beating + explicit offline)
            beater.set_online(False)
            state = wait_state(
                node,
                lambda s: s.get("control_enabled") is False,
                timeout=4,
            )
            self.assertIsNotNone(state)
            self.assertFalse(state.get("control_enabled"), state)
            self.assertIn(state.get("phase"), ("IDLE", "FAULT", "DEGRADED", "ESTOP", "RUNNING"))

            ack2 = send_cmd(
                node,
                cmd_pub,
                "CMD_TASK",
                602,
                {"task_id": "T-L2", "type": "goto", "x": 0.1, "y": 0.0},
            )
            self.assertIsNotNone(ack2)
            self.assertFalse(ack2.get("ok"), f"task should reject when console offline: {ack2}")

            beater.set_online(True)
            state = wait_state(
                node,
                lambda s: s.get("control_enabled") is True and s.get("phase") == "IDLE",
                timeout=6,
            )
            self.assertIsNotNone(state, f"after reconnect={state}")
        finally:
            if beater:
                beater.stop()
            node.destroy_node()
            rclpy.shutdown()


@launch_testing.post_shutdown_test()
class TestProcessOutput(unittest.TestCase):
    def test_exit_codes(self, proc_info):
        launch_testing.asserts.assertExitCodes(proc_info)
