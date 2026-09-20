#!/usr/bin/env python3
"""Lightweight TCP mock of botcockpit_bridge (protocol v0.1).

For environments without ROS2/Qt (e.g. Windows smoke tests). Not a product
component — production path is C++ botcockpit_bridge + fake_robot.

Usage:
  python tools/mock_bridge.py --port 8765
"""

from __future__ import annotations

import argparse
import json
import socket
import struct
import threading
import time
from typing import Dict, List, Optional

MSG_HEARTBEAT = 0x01
MSG_HELLO = 0x02
MSG_HELLO_ACK = 0x03
MSG_STATE_SNAPSHOT = 0x10
MSG_STATE_DELTA = 0x11
MSG_CMD_MODE = 0x20
MSG_CMD_TASK = 0x21
MSG_CMD_ESTOP = 0x22
MSG_CMD_RESET = 0x23
MSG_CMD_ACK = 0x2F

HB_TIMEOUT_MS = 3000
STATE_DELTA_MS = 200
PROTO = "0.1"
SERVER = "botcockpit_bridge/0.1.0"


def encode_frame(msg_type: int, flags: int, seq: int, payload: bytes) -> bytes:
    length = 4 + len(payload)
    return struct.pack("<IBBH", length, msg_type & 0xFF, flags & 0xFF, seq & 0xFFFF) + payload


def try_decode_frames(buffer: bytearray):
    frames = []
    while True:
        if len(buffer) < 4:
            break
        (length,) = struct.unpack_from("<I", buffer, 0)
        if length < 4:
            return frames, bytearray()
        if len(buffer) < 4 + length:
            break
        msg_type, flags, seq = struct.unpack_from("<BBH", buffer, 4)
        payload = bytes(buffer[8 : 4 + length])
        del buffer[: 4 + length]
        frames.append((msg_type, flags, seq, payload))
    return frames, buffer


class RobotSim:
    def __init__(self) -> None:
        self.lock = threading.Lock()
        self.mode = "TELEOP"
        self.phase = "IDLE"
        self.estop = False
        self.control_enabled = True
        self.pose_x = 0.0
        self.pose_y = 0.0
        self.pose_yaw = 0.0
        self.battery = 83.0
        self.task_id = None
        self.task_type = None
        self.task_status = "NONE"
        self.goal_x = 0.0
        self.goal_y = 0.0
        self.faults: List[dict] = []
        self.nodes = [
            {"name": "bridge", "status": "OK"},
            {"name": "fake_robot", "status": "OK"},
        ]
        self.start = time.time()

    def tick(self) -> None:
        with self.lock:
            if self.estop:
                return
            if self.phase == "RUNNING" and self.task_type == "goto":
                dx = self.goal_x - self.pose_x
                dy = self.goal_y - self.pose_y
                dist = (dx * dx + dy * dy) ** 0.5
                step = 0.1  # per tick
                if dist < 0.05:
                    self.pose_x, self.pose_y = self.goal_x, self.goal_y
                    self.task_status = "DONE"
                    self.phase = "IDLE"
                else:
                    ratio = min(1.0, step / dist)
                    self.pose_x += dx * ratio
                    self.pose_y += dy * ratio
                    self.task_status = "EXECUTING"
                self.battery = max(0.0, self.battery - 0.02)
            else:
                self.pose_x += 0.02
                if self.pose_x > 10.0:
                    self.pose_x = 0.0
                self.battery = max(0.0, self.battery - 0.002)

    def state_json(self) -> str:
        with self.lock:
            now = int(time.time() * 1000)
            conn = "ESTOP" if self.estop else "ONLINE"
            nodes = [
                {
                    "name": n["name"],
                    "status": n["status"],
                    "last_hb_ms": now,
                }
                for n in self.nodes
            ]
            obj = {
                "ts_ms": now,
                "conn": conn,
                "mode": self.mode,
                "phase": self.phase,
                "heartbeat_rtt_ms": 0,
                "nodes": nodes,
                "pose": {"x": self.pose_x, "y": self.pose_y, "yaw": self.pose_yaw},
                "battery": self.battery,
                "task": {
                    "id": self.task_id,
                    "type": self.task_type,
                    "status": self.task_status,
                },
                "faults": list(self.faults),
                "estop": self.estop,
                "control_enabled": self.control_enabled,
            }
            return json.dumps(obj, separators=(",", ":"))

    def handle_cmd(self, cmd: str, seq: int, payload: dict) -> str:
        with self.lock:
            if cmd == "CMD_ESTOP":
                self.estop = True
                self.phase = "ESTOP"
                self.control_enabled = False
                return json.dumps(
                    {
                        "seq": seq,
                        "ok": True,
                        "cmd": cmd,
                        "status": "ACCEPTED",
                        "reason": None,
                        "task_id": None,
                    }
                )
            if cmd == "CMD_RESET":
                if not payload.get("confirm", False):
                    return json.dumps(
                        {
                            "seq": seq,
                            "ok": False,
                            "cmd": cmd,
                            "status": "REJECTED",
                            "reason": "confirm_required",
                        }
                    )
                if self.faults:
                    return json.dumps(
                        {
                            "seq": seq,
                            "ok": False,
                            "cmd": cmd,
                            "status": "REJECTED",
                            "reason": "estop_active",
                        }
                    )
                self.estop = False
                self.phase = "IDLE"
                self.mode = "TELEOP"
                self.control_enabled = True
                self.task_status = "NONE"
                return json.dumps(
                    {
                        "seq": seq,
                        "ok": True,
                        "cmd": cmd,
                        "status": "ACCEPTED",
                        "reason": None,
                    }
                )
            if cmd == "CMD_MODE":
                mode = payload.get("mode")
                if mode not in ("TELEOP", "AUTO", "REMOTE"):
                    return json.dumps(
                        {
                            "seq": seq,
                            "ok": False,
                            "cmd": cmd,
                            "status": "REJECTED",
                            "reason": "invalid_payload",
                        }
                    )
                if mode == "AUTO" and (self.estop or self.phase in ("FAULT", "ESTOP")):
                    return json.dumps(
                        {
                            "seq": seq,
                            "ok": False,
                            "cmd": cmd,
                            "status": "REJECTED",
                            "reason": "estop_active" if self.estop else "phase_busy",
                        }
                    )
                self.mode = mode
                return json.dumps(
                    {
                        "seq": seq,
                        "ok": True,
                        "cmd": cmd,
                        "status": "ACCEPTED",
                        "reason": None,
                    }
                )
            if cmd == "CMD_TASK":
                ttype = payload.get("type")
                tid = payload.get("task_id") or "T-mock"
                if self.estop or self.phase in ("FAULT", "ESTOP", "DEGRADED"):
                    return json.dumps(
                        {
                            "seq": seq,
                            "ok": False,
                            "cmd": cmd,
                            "status": "REJECTED",
                            "reason": "estop_active" if self.estop else "phase_busy",
                            "task_id": tid,
                        }
                    )
                if self.phase == "RUNNING":
                    return json.dumps(
                        {
                            "seq": seq,
                            "ok": False,
                            "cmd": cmd,
                            "status": "REJECTED",
                            "reason": "phase_busy",
                            "task_id": tid,
                        }
                    )
                if ttype == "goto":
                    self.task_id = tid
                    self.task_type = "goto"
                    self.task_status = "ACCEPTED"
                    self.goal_x = float(payload.get("x", 0.0))
                    self.goal_y = float(payload.get("y", 0.0))
                    self.phase = "RUNNING"
                    return json.dumps(
                        {
                            "seq": seq,
                            "ok": True,
                            "cmd": cmd,
                            "status": "ACCEPTED",
                            "reason": None,
                            "task_id": tid,
                        }
                    )
                return json.dumps(
                    {
                        "seq": seq,
                        "ok": False,
                        "cmd": cmd,
                        "status": "REJECTED",
                        "reason": "unknown_task",
                        "task_id": tid,
                    }
                )
            return json.dumps(
                {
                    "seq": seq,
                    "ok": False,
                    "cmd": cmd,
                    "status": "REJECTED",
                    "reason": "not_implemented",
                }
            )


class ClientSession(threading.Thread):
    def __init__(self, sock: socket.socket, addr, robot: RobotSim, server: "MockBridge"):
        super().__init__(daemon=True)
        self.sock = sock
        self.addr = addr
        self.robot = robot
        self.server = server
        self.rx = bytearray()
        self.hello_ok = False
        self.last_hb = time.time()
        self.send_lock = threading.Lock()
        self.alive = True

    def send(self, msg_type: int, flags: int, seq: int, payload: dict) -> None:
        raw = encode_frame(msg_type, flags, seq, json.dumps(payload, separators=(",", ":")).encode("utf-8"))
        with self.send_lock:
            try:
                self.sock.sendall(raw)
            except OSError:
                self.alive = False

    def run(self) -> None:
        self.sock.settimeout(0.2)
        try:
            while self.server.running and self.alive:
                if self.hello_ok and time.time() - self.last_hb > HB_TIMEOUT_MS / 1000.0:
                    print(f"[tcp] heartbeat timeout {self.addr}")
                    break
                try:
                    chunk = self.sock.recv(4096)
                except socket.timeout:
                    chunk = b""
                if chunk:
                    self.last_hb = time.time()
                    self.rx.extend(chunk)
                    frames, self.rx = try_decode_frames(self.rx)
                    for fr in frames:
                        self.handle(*fr)
                elif chunk == b"" and not self.alive:
                    break
        finally:
            self.alive = False
            try:
                self.sock.close()
            except OSError:
                pass
            print(f"[tcp] client disconnected {self.addr}")

    def handle(self, msg_type: int, flags: int, seq: int, payload: bytes) -> None:
        try:
            obj = json.loads(payload.decode("utf-8")) if payload else {}
        except Exception:
            obj = {}
        print(f"[tcp] rx type=0x{msg_type:02x} seq={seq} payload={obj}")

        if msg_type == MSG_HELLO:
            proto_min = obj.get("proto_min", "")
            proto_max = obj.get("proto_max", "")
            ok = PROTO in (proto_min, proto_max) or (not proto_min and not proto_max)
            if ok:
                self.send(MSG_HELLO_ACK, 0, seq, {"ok": True, "proto": PROTO, "server": SERVER})
                self.hello_ok = True
                self.send(MSG_STATE_SNAPSHOT, 0, 0, json.loads(self.robot.state_json()))
            else:
                self.send(MSG_HELLO_ACK, 0, seq, {"ok": False, "reason": "proto_unsupported"})
            return

        if msg_type == MSG_HEARTBEAT:
            self.send(
                MSG_HEARTBEAT,
                0,
                seq,
                {"ts_ms": int(time.time() * 1000), "role": "bridge"},
            )
            return

        if msg_type in (MSG_CMD_MODE, MSG_CMD_TASK, MSG_CMD_ESTOP, MSG_CMD_RESET):
            names = {
                MSG_CMD_MODE: "CMD_MODE",
                MSG_CMD_TASK: "CMD_TASK",
                MSG_CMD_ESTOP: "CMD_ESTOP",
                MSG_CMD_RESET: "CMD_RESET",
            }
            cmd = names[msg_type]
            if not self.hello_ok:
                ack = {
                    "seq": seq,
                    "ok": False,
                    "cmd": cmd,
                    "status": "REJECTED",
                    "reason": "proto_error",
                }
            else:
                ack = json.loads(self.robot.handle_cmd(cmd, seq, obj))
            self.send(MSG_CMD_ACK, 0, seq, ack)
            return

    def send_delta(self) -> None:
        if self.hello_ok and self.alive:
            self.send(MSG_STATE_DELTA, 0, 0, json.loads(self.robot.state_json()))


class MockBridge:
    def __init__(self, port: int) -> None:
        self.port = port
        self.running = False
        self.robot = RobotSim()
        self.clients: List[ClientSession] = []
        self.lock = threading.Lock()
        self.listen_sock: Optional[socket.socket] = None

    def start(self) -> None:
        self.listen_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.listen_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.listen_sock.bind(("0.0.0.0", self.port))
        self.listen_sock.listen(8)
        self.listen_sock.settimeout(0.5)
        self.running = True
        print(f"[tcp] mock bridge listening 0.0.0.0:{self.port}")
        threading.Thread(target=self.accept_loop, daemon=True).start()
        threading.Thread(target=self.delta_loop, daemon=True).start()
        threading.Thread(target=self.sim_loop, daemon=True).start()

    def accept_loop(self) -> None:
        while self.running:
            try:
                conn, addr = self.listen_sock.accept()
            except socket.timeout:
                continue
            except OSError:
                break
            print(f"[tcp] client connected {addr}")
            sess = ClientSession(conn, addr, self.robot, self)
            with self.lock:
                self.clients.append(sess)
            sess.start()

    def sim_loop(self) -> None:
        while self.running:
            time.sleep(0.2)
            self.robot.tick()

    def delta_loop(self) -> None:
        while self.running:
            time.sleep(STATE_DELTA_MS / 1000.0)
            with self.lock:
                alive = [c for c in self.clients if c.alive]
                self.clients = alive
            for c in alive:
                c.send_delta()

    def stop(self) -> None:
        self.running = False
        if self.listen_sock:
            try:
                self.listen_sock.close()
            except OSError:
                pass
        with self.lock:
            for c in self.clients:
                try:
                    c.sock.close()
                except OSError:
                    pass


def main() -> int:
    parser = argparse.ArgumentParser(description="BotCockpit mock bridge (no ROS2)")
    parser.add_argument("--port", type=int, default=8765)
    args = parser.parse_args()
    bridge = MockBridge(args.port)
    bridge.start()
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("stopping mock bridge")
    finally:
        bridge.stop()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
