#!/usr/bin/env python3
"""Fail if bridge and UI protocol constants drift apart."""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def parse_constants(path: Path) -> dict:
    text = path.read_text(encoding="utf-8", errors="replace")
    out = {}
    for m in re.finditer(
        r"constexpr\s+\w+\s+(MSG_[A-Z0-9_]+|FLAG_[A-Z0-9_]+|HB_\w+|DEFAULT_PORT)\s*=\s*([^;]+);",
        text,
    ):
        out[m.group(1)] = m.group(2).strip()
    return out


def main() -> int:
    a = parse_constants(ROOT / "bridge/include/botcockpit_bridge/protocol.hpp")
    b = parse_constants(ROOT / "ui/botcockpit/src/protocol.hpp")
    keys = sorted(set(a) | set(b))
    bad = []
    for k in keys:
        if k not in a or k not in b:
            bad.append(f"{k}: only in {'bridge' if k in a else 'ui'}")
        elif a[k].replace(" ", "") != b[k].replace(" ", ""):
            bad.append(f"{k}: bridge={a[k]} ui={b[k]}")
    if bad:
        print("PROTOCOL SYNC FAIL")
        for line in bad:
            print(" ", line)
        return 1
    print(f"PROTOCOL SYNC PASS ({len(keys)} constants)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
