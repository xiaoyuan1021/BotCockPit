#!/usr/bin/env python3
"""BotCockpit protocol smoke client (no UI).

Implements PROTOCOL.md v0.1 client side:
  TCP connect -> HELLO -> HELLO_ACK -> STATE_SNAPSHOT -> periodic HEARTBEAT

Usage:
  python3 tools/fake_client.py --host 127.0.0.1 --port 8765
  python3 tools/fake_client.py --send-sticky
  python3 tools/fake_client.py --self-test
"""

from __future__ import annotations

import argparse
import json
import socket
import struct
import sys
import time
from typing import List, Optional, Tuple

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
MSG_EVENT_FAULT = 0x30

FLAG_NEED_ACK = 0x01
FLAG_URGENT = 0x02

HB_INTERVAL_S = 1.0
DEFAULT_PORT = 8765

TYPE_NAMES = {
    MSG_HEARTBEAT: "HEARTBEAT",
    MSG_HELLO: "HELLO",
    MSG_HELLO_ACK: "HELLO_ACK",
    MSG_STATE_SNAPSHOT: "STATE_SNAPSHOT",
    MSG_STATE_DELTA: "STATE_DELTA",
    MSG_CMD_MODE: "CMD_MODE",
    MSG_CMD_TASK: "CMD_TASK",
    MSG_CMD_ESTOP: "CMD_ESTOP",
    MSG_CMD_RESET: "CMD_RESET",
    MSG_CMD_ACK: "CMD_ACK",
    MSG_EVENT_FAULT: "EVENT_FAULT",
}


def encode_frame(msg_type: int, flags: int, seq: int, payload: bytes) -> bytes:
    """Build one protocol frame (little-endian)."""
    length = 4 + len(payload)
    header = struct.pack("<IBBH", length, msg_type & 0xFF, flags & 0xFF, seq & 0xFFFF)
    return header + payload


def try_decode_frames(buffer: bytearray) -> Tuple[List[Tuple[int, int, int, bytes]], bytearray]:
    """Extract complete frames from a sticky TCP buffer."""
    frames: List[Tuple[int, int, int, bytes]] = []
    while True:
        if len(buffer) < 4:
            break
        (length,) = struct.unpack_from("<I", buffer, 0)
        if length < 4:
            # protocol error — drop buffer
            return frames, bytearray()
        if len(buffer) < 4 + length:
            break
        msg_type, flags, seq = struct.unpack_from("<BBH", buffer, 4)
        payload = bytes(buffer[8 : 4 + length])
        del buffer[: 4 + length]
        frames.append((msg_type, flags, seq, payload))
    return frames, buffer


def type_name(t: int) -> str:
    return TYPE_NAMES.get(t, f"UNKNOWN(0x{t:02x})")


def pretty_payload(payload: bytes) -> str:
    if not payload:
        return "{}"
    try:
        obj = json.loads(payload.decode("utf-8"))
        return json.dumps(obj, ensure_ascii=False, separators=(",", ":"))
    except Exception:
        return payload.decode("utf-8", errors="replace")


def print_frame(direction: str, msg_type: int, flags: int, seq: int, payload: bytes) -> None:
    print(f"{direction} {type_name(msg_type):16s} seq={seq:5d} flags=0x{flags:02x} "
          f"payload={pretty_payload(payload)}")


def self_test() -> int:
    """Encode/decode unit checks (no network)."""
    payload = b'{"proto_min":"0.1","proto_max":"0.1","client":"BotCockpit"}'
    raw = encode_frame(MSG_HELLO, 0, 7, payload)
    assert len(raw) == 8 + len(payload)
    assert raw[4] == MSG_HELLO
    assert raw[5] == 0
    assert raw[6] | (raw[7] << 8) == 7
    frames, buf = try_decode_frames(bytearray(raw))
    assert len(frames) == 1 and not buf
    t, f, s, p = frames[0]
    assert t == MSG_HELLO and f == 0 and s == 7 and p == payload

    # sticky: three frames in one buffer
    a = encode_frame(MSG_HEARTBEAT, 0, 1, b'{"role":"console"}')
    b = encode_frame(MSG_HELLO, FLAG_NEED_ACK, 2, payload)
    c = encode_frame(MSG_CMD_ESTOP, FLAG_URGENT | FLAG_NEED_ACK, 3, b'{"reason":"operator"}')
    frames, buf = try_decode_frames(bytearray(a + b + c))
    assert len(frames) == 3 and not buf
    assert frames[0][0] == MSG_HEARTBEAT
    assert frames[1][0] == MSG_HELLO
    assert frames[2][0] == MSG_CMD_ESTOP
    assert frames[2][1] == (FLAG_URGENT | FLAG_NEED_ACK)

    # half packet
    half = bytearray(encode_frame(MSG_HELLO, 0, 9, payload)[:5])
    frames, buf = try_decode_frames(half)
    assert frames == [] and len(buf) == 5
    rest = encode_frame(MSG_HELLO, 0, 9, payload)[5:]
    frames, buf = try_decode_frames(buf + bytearray(rest))
    assert len(frames) == 1 and frames[0][2] == 9

    print("self-test: PASS (frame encode/decode, sticky, half-packet)")
    return 0


class FakeClient:
    def __init__(self, host: str, port: int, send_sticky: bool = False, duration: float = 0.0):
        self.host = host
        self.port = port
        self.send_sticky = send_sticky
        self.duration = duration
        self.sock: Optional[socket.socket] = None
        self.rx = bytearray()
        self.seq = 1
        self.last_hb = 0.0
        self.got_hello_ack = False
        self.got_snapshot = False
        self.running = False

    def next_seq(self) -> int:
        s = self.seq
        self.seq = 1 if self.seq >= 0xFFFF else self.seq + 1
        return s

    def send_frame(self, msg_type: int, flags: int, payload: dict) -> int:
        assert self.sock is not None
        seq = self.next_seq()
        raw = encode_frame(msg_type, flags, seq, json.dumps(payload, separators=(",", ":")).encode("utf-8"))
        print_frame("→ TX", msg_type, flags, seq, json.dumps(payload, separators=(",", ":")).encode("utf-8"))
        self.sock.sendall(raw)
        return seq

    def send_sticky_burst(self) -> None:
        """One write containing multiple frames — bridge sticky test."""
        assert self.sock is not None
        hello = encode_frame(
            MSG_HELLO,
            0,
            self.next_seq(),
            json.dumps(
                {
                    "client": "BotCockpit",
                    "ui_version": "0.1.0",
                    "proto_min": "0.1",
                    "proto_max": "0.1",
                },
                separators=(",", ":"),
            ).encode("utf-8"),
        )
        hb = encode_frame(
            MSG_HEARTBEAT,
            0,
            self.next_seq(),
            json.dumps({"ts_ms": int(time.time() * 1000), "role": "console"}, separators=(",", ":")).encode("utf-8"),
        )
        # NACK probe: CMD_MODE while robot may be unavailable
        cmd = encode_frame(
            MSG_CMD_MODE,
            FLAG_NEED_ACK,
            self.next_seq(),
            json.dumps({"mode": "TELEOP"}, separators=(",", ":")).encode("utf-8"),
        )
        blob = hello + hb + cmd
        print(f"→ TX STICKY write {len(blob)} bytes (HELLO+HEARTBEAT+CMD_MODE)")
        self.sock.sendall(blob)

    def handle_frame(self, msg_type: int, flags: int, seq: int, payload: bytes) -> None:
        print_frame("← RX", msg_type, flags, seq, payload)
        if msg_type == MSG_HELLO_ACK:
            self.got_hello_ack = True
            try:
                obj = json.loads(payload.decode("utf-8"))
                print(f"   HELLO_ACK ok={obj.get('ok')} proto={obj.get('proto')} server={obj.get('server')}")
            except Exception:
                pass
        elif msg_type == MSG_STATE_SNAPSHOT:
            self.got_snapshot = True
            self._print_state_fields(payload)
        elif msg_type == MSG_STATE_DELTA:
            self._print_state_fields(payload, brief=True)

    def _print_state_fields(self, payload: bytes, brief: bool = False) -> None:
        try:
            obj = json.loads(payload.decode("utf-8"))
        except Exception:
            return
        keys = ["conn", "mode", "phase", "battery", "estop", "control_enabled", "heartbeat_rtt_ms"]
        summary = {k: obj.get(k) for k in keys if k in obj}
        pose = obj.get("pose")
        if isinstance(pose, dict):
            summary["pose"] = pose
        nodes = obj.get("nodes")
        if isinstance(nodes, list):
            summary["nodes"] = [
                {"name": n.get("name"), "status": n.get("status")} for n in nodes
            ]
        if brief:
            print(f"   state: phase={summary.get('phase')} mode={summary.get('mode')} "
                  f"battery={summary.get('battery')} pose={summary.get('pose')}")
        else:
            print(f"   state: {json.dumps(summary, ensure_ascii=False)}")

    def run(self) -> int:
        print(f"fake_client connect {self.host}:{self.port}"
              + (" sticky-burst" if self.send_sticky else ""))
        self.sock = socket.create_connection((self.host, self.port), timeout=5)
        self.sock.settimeout(0.2)
        self.running = True
        started = time.time()

        if self.send_sticky:
            self.send_sticky_burst()
        else:
            self.send_frame(
                MSG_HELLO,
                0,
                {
                    "client": "BotCockpit",
                    "ui_version": "0.1.0",
                    "proto_min": "0.1",
                    "proto_max": "0.1",
                },
            )

        self.last_hb = time.time()
        try:
            while self.running:
                if self.duration > 0 and (time.time() - started) >= self.duration:
                    print("duration reached, exiting")
                    break
                try:
                    chunk = self.sock.recv(4096)
                except socket.timeout:
                    chunk = b""
                if chunk:
                    self.rx.extend(chunk)
                    frames, self.rx = try_decode_frames(self.rx)
                    for fr in frames:
                        self.handle_frame(*fr)
                now = time.time()
                if self.got_hello_ack and now - self.last_hb >= HB_INTERVAL_S:
                    self.send_frame(
                        MSG_HEARTBEAT,
                        0,
                        {"ts_ms": int(now * 1000), "role": "console"},
                    )
                    self.last_hb = now
        except KeyboardInterrupt:
            print("\nCtrl+C — closing")
        finally:
            self.close()
        print(f"summary: hello_ack={self.got_hello_ack} snapshot={self.got_snapshot}")
        return 0 if self.got_hello_ack else 2

    def close(self) -> None:
        self.running = False
        if self.sock is not None:
            try:
                self.sock.shutdown(socket.SHUT_RDWR)
            except Exception:
                pass
            try:
                self.sock.close()
            except Exception:
                pass
            self.sock = None


def main(argv: Optional[List[str]] = None) -> int:
    parser = argparse.ArgumentParser(description="BotCockpit fake_client protocol smoke tool")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=DEFAULT_PORT)
    parser.add_argument("--send-sticky", action="store_true",
                        help="one write of multiple frames to test bridge sticky handling")
    parser.add_argument("--duration", type=float, default=0.0,
                        help="exit after N seconds (0 = run until Ctrl+C)")
    parser.add_argument("--self-test", action="store_true",
                        help="run offline frame codec tests and exit")
    args = parser.parse_args(argv)

    if args.self_test:
        return self_test()

    client = FakeClient(args.host, args.port, send_sticky=args.send_sticky, duration=args.duration)
    try:
        return client.run()
    except OSError as exc:
        print(f"connection failed: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
