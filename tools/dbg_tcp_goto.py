#!/usr/bin/env python3
"""Send goto over TCP like the QML client and print state phase samples."""
import json
import socket
import struct
import time
import sys
sys.path.insert(0, "/home/xiaoyuan/ros2_ws/Bot/Bot_Project/tools")
from fake_client import encode_frame, try_decode_frames, MSG_HELLO, MSG_CMD_TASK, MSG_STATE_DELTA, MSG_STATE_SNAPSHOT, MSG_CMD_ACK, FLAG_NEED_ACK

def main():
    sock = socket.create_connection(("127.0.0.1", 8765), timeout=5)
    sock.settimeout(0.2)
    seq = 1
    def send(t, flags, payload):
        nonlocal seq
        raw = encode_frame(t, flags, seq, json.dumps(payload, separators=(",", ":")).encode())
        sock.sendall(raw)
        s = seq
        seq += 1
        return s

    send(MSG_HELLO, 0, {"client":"BotCockpit","ui_version":"0.1.0","proto_min":"0.1","proto_max":"0.1"})
    rx = bytearray()
    phases = []
    acks = []
    sent = False
    t0 = time.time()
    while time.time() - t0 < 8:
        try:
            chunk = sock.recv(4096)
        except socket.timeout:
            chunk = b""
        if chunk:
            rx.extend(chunk)
            frames, rx = try_decode_frames(rx)
            for t, fl, s, p in frames:
                obj = json.loads(p.decode() or "{}")
                if t in (MSG_STATE_SNAPSHOT, MSG_STATE_DELTA):
                    phases.append((round(time.time()-t0,2), obj.get("phase"), (obj.get("task") or {}).get("status"), obj.get("control_enabled"), (obj.get("pose") or {}).get("x")))
                if t == MSG_CMD_ACK:
                    acks.append(obj)
        if not sent and time.time() - t0 > 2:
            send(MSG_CMD_TASK, FLAG_NEED_ACK, {"task_id":"T-TCP","type":"goto","x":7.0,"y":0.0,"timeout_s":30})
            sent = True
            print("sent goto")
        if acks and len(phases) > 8:
            break
    print("acks", acks)
    print("states:")
    for row in phases[-15:]:
        print(row)
    sock.close()

if __name__ == "__main__":
    main()
