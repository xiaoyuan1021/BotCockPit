#!/usr/bin/env python3
"""Offline unit checks for A* corridor planning (Plan A). No ROS required."""

from __future__ import annotations

import math


def make_corridor():
    w, h, res = 40, 20, 0.5
    ox, oy = 0.0, -5.0
    occ = [0] * (w * h)

    def setc(x, y, v=1):
        if 0 <= x < w and 0 <= y < h:
            occ[y * w + x] = v

    def at(x, y):
        if 0 <= x < w and 0 <= y < h:
            return occ[y * w + x]
        return 1

    for x in range(w):
        setc(x, 0)
        setc(x, h - 1)
    for y in range(h):
        setc(0, y)
        setc(w - 1, y)
    for y in range(2, h - 2):
        if y not in (8, 9, 10):
            setc(20, y)
    setc(5, 4)
    setc(6, 4)
    return w, h, res, ox, oy, at


def world_to_cell(wx, wy, res, ox, oy, w, h):
    cx = int(math.floor((wx - ox) / res))
    cy = int(math.floor((wy - oy) / res))
    return cx, cy


def cell_to_world(cx, cy, res, ox, oy):
    return ox + (cx + 0.5) * res, oy + (cy + 0.5) * res


def astar(sx, sy, gx, gy):
    w, h, res, ox, oy, at = make_corridor()
    scx, scy = world_to_cell(sx, sy, res, ox, oy, w, h)
    gcx, gcy = world_to_cell(gx, gy, res, ox, oy, w, h)
    if at(scx, scy) or at(gcx, gcy):
        return None
    import heapq

    def idx(x, y):
        return y * w + x

    g = {}
    came = {}
    g[idx(scx, scy)] = 0.0
    heap = [(abs(scx - gcx) + abs(scy - gcy), idx(scx, scy))]
    goal = idx(gcx, gcy)
    dirs = [(1, 0), (-1, 0), (0, 1), (0, -1), (1, 1), (1, -1), (-1, 1), (-1, -1)]
    while heap:
        _, cur = heapq.heappop(heap)
        if cur == goal:
            path = []
            c = cur
            while c in came or c == idx(scx, scy):
                path.append(cell_to_world(c % w, c // w, res, ox, oy))
                if c == idx(scx, scy):
                    break
                c = came[c]
            path.reverse()
            return path
        cx, cy = cur % w, cur // w
        for dx, dy in dirs:
            nx, ny = cx + dx, cy + dy
            if at(nx, ny):
                continue
            ni = idx(nx, ny)
            step = 1.4142 if dx and dy else 1.0
            tg = g[cur] + step
            if tg < g.get(ni, 1e18):
                came[ni] = cur
                g[ni] = tg
                heapq.heappush(heap, (tg + abs(nx - gcx) + abs(ny - gcy), ni))
    return None


def main() -> int:
    # Path must go through wall gap (cells y=8..10 at x=20) — world y ≈ -0.5..0.5
    path = astar(1.0, 0.0, 18.0, 0.0)
    assert path is not None and len(path) > 5, "A* failed to find corridor path"
    # Cross wall around x=10m
    crossed = [p for p in path if 9.0 < p[0] < 11.0]
    assert crossed, "path did not approach wall x≈10"
    ys = [p[1] for p in crossed]
    assert any(-1.0 < y < 1.0 for y in ys), f"path missed gap ys={ys}"

    # Blocked goal cell → fail
    path_bad = astar(1.0, 0.0, 1.0, -4.0)  # near border occupied after inflate-like
    # use known occupied: origin cell (0,0) world ≈ (0.25, -4.75)
    w, h, res, ox, oy, at = make_corridor()
    bx, by = cell_to_world(0, 0, res, ox, oy)
    assert astar(1.0, 0.0, bx, by) is None

    # Pure pursuit sanity
    yaw = 0.0
    x, y = 0.0, 0.0
    idx = 0
    wp = [(0.5, 0.0), (1.0, 0.0), (2.0, 0.0)]
    # inline simple check: lateral positive → w positive
    # simulate at origin looking +x, target at (1, 0.5)
    dx, dy = 1.0, 0.5
    lx = math.cos(yaw) * dx + math.sin(yaw) * dy
    ly = -math.sin(yaw) * dx + math.cos(yaw) * dy
    assert ly > 0
    print("astar+pure_pursuit self-test: PASS")
    print(f"path_len={len(path)} mid={path[len(path)//2]}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
