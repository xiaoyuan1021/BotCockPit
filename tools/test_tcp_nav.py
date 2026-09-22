#!/usr/bin/env python3
"""TCP-path conformance smoke for BotCockpit bridge (no Qt).

Covers: HELLO fail-closed semantics, CMD_TASK nav, sticky frames, ESTOP flags.
Run after fake_robot+bridge are up:
  python3 tools/test_tcp_nav.py --host 127.0.0.1 --port 8765
"""

from __future__ import annotations

import argparse
import json
import socket
import sys
import time

from fake_client import (
    FLAG_NEED_ACK,
    FLAG_URGENT,
    MSG_CMD_ACK,
    MSG_CMD_ESTOP,
    MSG_CMD_TASK,
    MSG_HEARTBEAT,
    MSG_HELLO,
    MSG_HELLO_ACK,
    MSG_STATE_DELTA,
    MSG_STATE_SNAPSHOT,
    encode_frame,
    try_decode_frames,
)


def recv_until(sock, pred, timeout=5.0):
    sock.settimeout(0.2)
    buf = bytearray()
    t0 = time.time()
    found = []
    while time.time() - t0 < timeout:
        try:
            chunk = sock.recv(4096)
        except socket.timeout:
            chunk = b""
        if chunk:
            buf.extend(chunk)
            frames, buf = try_decode_frames(buf)
            found.extend(frames)
            if any(pred(f) for f in found):
                return found
    return found


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=8765)
    args = ap.parse_args()

    sock = socket.create_connection((args.host, args.port), timeout=5)
    seq = 1

    def send(t, flags, payload):
        nonlocal seq
        sock.sendall(encode_frame(t, flags, seq, json.dumps(payload, separators=(",", ":")).encode()))
        s = seq
        seq += 1
        return s

    # 1) sticky: HELLO + HEARTBEAT
    hello = encode_frame(MSG_HELLO, 0, seq, json.dumps({
        "client": "BotCockpit", "ui_version": "0.1.0",
        "proto_min": "0.1", "proto_max": "0.1"}, separators=(",", ":")).encode())
    seq += 1
    hb = encode_frame(MSG_HEARTBEAT, 0, seq, json.dumps(
        {"ts_ms": int(time.time() * 1000), "role": "console"}, separators=(",", ":")).encode())
    seq += 1
    sock.sendall(hello + hb)

    frames = recv_until(sock, lambda f: f[0] == MSG_HELLO_ACK, 4.0)
    acks = [json.loads(p.decode()) for t, fl, s, p in frames if t == MSG_HELLO_ACK]
    assert acks and acks[0].get("ok"), f"hello failed: {acks}"
    print("HELLO_ACK ok", acks[0])

    frames = recv_until(sock, lambda f: f[0] in (MSG_STATE_SNAPSHOT, MSG_STATE_DELTA), 4.0)
    states = [json.loads(p.decode()) for t, fl, s, p in frames if t in (MSG_STATE_SNAPSHOT, MSG_STATE_DELTA)]
    assert states, "no state snapshot"
    print("SNAPSHOT phase", states[0].get("phase"), "nav", (states[0].get("nav") or {}).get("status"))

    # 2) CMD_TASK goto far goal
    seq_task = send(MSG_CMD_TASK, FLAG_NEED_ACK,
                    {"task_id": "T-tcp", "type": "goto", "x": 18.0, "y": 0.0})
    frames = recv_until(sock, lambda f: f[0] == MSG_CMD_ACK and f[2] == seq_task, 5.0)
    task_acks = [json.loads(p.decode()) for t, fl, s, p in frames if t == MSG_CMD_ACK and s == seq_task]
    assert task_acks, "no task ACK"
    print("TASK ACK", task_acks[0])
    assert task_acks[0].get("ok"), task_acks[0]

    frames = recv_until(
        sock,
        lambda f: (
            f[0] in (MSG_STATE_SNAPSHOT, MSG_STATE_DELTA)
            and json.loads(f[3].decode() or "{}").get("phase") == "RUNNING"
        ),
        6.0,
    )
    nav_states = [json.loads(p.decode()) for t, fl, s, p in frames if t in (MSG_STATE_SNAPSHOT, MSG_STATE_DELTA)]
    tracking = [s for s in nav_states if (s.get("nav") or {}).get("status") == "TRACKING"]
    print("TRACKING samples", len(tracking), "path_len", (tracking[-1].get("nav") or {}).get("path_len") if tracking else None)
    assert tracking, "nav not TRACKING"

    # 3) ESTOP urgent + ACK
    seq_e = send(MSG_CMD_ESTOP, FLAG_URGENT | FLAG_NEED_ACK, {"reason": "tcp_conformance"})
    frames = recv_until(sock, lambda f: f[0] == MSG_CMD_ACK and f[2] == seq_e, 5.0)
    e_acks = [json.loads(p.decode()) for t, fl, s, p in frames if t == MSG_CMD_ACK and s == seq_e]
    print("ESTOP ACK", e_acks[:1])
    assert e_acks and e_acks[0].get("ok"), e_acks

    print("TCP_NAV_CONFORMANCE PASS")
    sock.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
