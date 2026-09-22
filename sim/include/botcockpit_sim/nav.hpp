#pragma once

// Lightweight navigation for Plan A: occupancy grid + A* + pure pursuit.
// Used by fake_robot goto; console only displays results via STATE JSON.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <queue>
#include <string>
#include <utility>
#include <vector>

namespace botcockpit_sim {
namespace nav {

struct Cell {
  int x = 0;
  int y = 0;
};

struct GridMap {
  int width = 40;   // cells
  int height = 20;
  double resolution = 0.5;  // m / cell
  double origin_x = 0.0;
  double origin_y = -5.0;
  // 1 = occupied
  std::vector<uint8_t> occ;

  static GridMap make_corridor_demo()
  {
    GridMap m;
    m.width = 40;
    m.height = 20;
    m.resolution = 0.5;
    m.origin_x = 0.0;
    m.origin_y = -5.0;
    m.occ.assign(static_cast<size_t>(m.width * m.height), 0);
    // Borders
    for (int x = 0; x < m.width; ++x) {
      m.set(x, 0, 1);
      m.set(x, m.height - 1, 1);
    }
    for (int y = 0; y < m.height; ++y) {
      m.set(0, y, 1);
      m.set(m.width - 1, y, 1);
    }
    // Vertical wall with wide gap (forces A* detour).
    // 5 free cells so inflate(1) still leaves a 3-cell corridor (~1.5 m).
    for (int y = 2; y < m.height - 2; ++y) {
      if (y >= 7 && y <= 11) {
        continue;  // gap
      }
      m.set(20, y, 1);
    }
    // Block near start to show inflation matter
    m.set(5, 4, 1);
    m.set(6, 4, 1);
    return m;
  }

  void set(int x, int y, uint8_t v)
  {
    if (x < 0 || y < 0 || x >= width || y >= height) {
      return;
    }
    occ[static_cast<size_t>(y * width + x)] = v;
  }

  uint8_t at(int x, int y) const
  {
    if (x < 0 || y < 0 || x >= width || y >= height) {
      return 1;
    }
    return occ[static_cast<size_t>(y * width + x)];
  }

  bool world_to_cell(double wx, double wy, int& cx, int& cy) const
  {
    cx = static_cast<int>(std::floor((wx - origin_x) / resolution));
    cy = static_cast<int>(std::floor((wy - origin_y) / resolution));
    return cx >= 0 && cy >= 0 && cx < width && cy < height;
  }

  void cell_to_world(int cx, int cy, double& wx, double& wy) const
  {
    wx = origin_x + (cx + 0.5) * resolution;
    wy = origin_y + (cy + 0.5) * resolution;
  }

  void inflate(int radius_cells)
  {
    if (radius_cells <= 0) {
      return;
    }
    auto src = occ;
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        if (!src[static_cast<size_t>(y * width + x)]) {
          continue;
        }
        for (int dy = -radius_cells; dy <= radius_cells; ++dy) {
          for (int dx = -radius_cells; dx <= radius_cells; ++dx) {
            if (dx * dx + dy * dy <= radius_cells * radius_cells) {
              set(x + dx, y + dy, 1);
            }
          }
        }
      }
    }
  }
};

// A* on grid; returns world waypoints (cell centers) or empty if failed.
inline std::vector<std::pair<double, double>> astar(const GridMap& map,
                                                    double sx, double sy,
                                                    double gx, double gy)
{
  int scx, scy, gcx, gcy;
  if (!map.world_to_cell(sx, sy, scx, scy) ||
      !map.world_to_cell(gx, gy, gcx, gcy)) {
    return {};
  }
  // Snap start/goal to nearest free cell (keeps demo robust if pose sits on border)
  auto snap_free = [&](int x, int y, int& ox, int& oy) {
    if (!map.at(x, y)) {
      ox = x;
      oy = y;
      return true;
    }
    int best_d = 1e9;
    bool found = false;
    for (int r = 1; r <= 6 && !found; ++r) {
      for (int dy = -r; dy <= r; ++dy) {
        for (int dx = -r; dx <= r; ++dx) {
          const int nx = x + dx;
          const int ny = y + dy;
          if (nx < 0 || ny < 0 || nx >= map.width || ny >= map.height) {
            continue;
          }
          if (map.at(nx, ny)) {
            continue;
          }
          const int d = dx * dx + dy * dy;
          if (d < best_d) {
            best_d = d;
            ox = nx;
            oy = ny;
            found = true;
          }
        }
      }
    }
    return found;
  };
  if (!snap_free(scx, scy, scx, scy) || !snap_free(gcx, gcy, gcx, gcy)) {
    return {};
  }
  const int W = map.width;
  const int H = map.height;
  const int N = W * H;
  std::vector<int> came(static_cast<size_t>(N), -1);
  std::vector<float> gscore(static_cast<size_t>(N),
                            std::numeric_limits<float>::infinity());
  auto idx = [W](int x, int y) { return y * W + x; };
  auto h = [&](int x, int y) {
    const int dx = std::abs(x - gcx);
    const int dy = std::abs(y - gcy);
    // Octile distance — admissible for 8-connected (1 / sqrt2) steps
    return static_cast<float>(std::max(dx, dy) + 0.4142f * std::min(dx, dy));
  };
  using QItem = std::pair<float, int>;  // f, index
  std::priority_queue<QItem, std::vector<QItem>, std::greater<QItem>> open;
  const int s = idx(scx, scy);
  gscore[static_cast<size_t>(s)] = 0.f;
  open.emplace(h(scx, scy), s);
  const int dirs[8][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1},
                          {1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
  const int goal = idx(gcx, gcy);
  while (!open.empty()) {
    const int cur = open.top().second;
    open.pop();
    if (cur == goal) {
      std::vector<std::pair<double, double>> path;
      int c = cur;
      while (c >= 0) {
        const int cx = c % W;
        const int cy = c / W;
        double wx, wy;
        map.cell_to_world(cx, cy, wx, wy);
        path.emplace_back(wx, wy);
        c = came[static_cast<size_t>(c)];
      }
      std::reverse(path.begin(), path.end());
      if (!path.empty()) {
        // Exact commanded goal for end-point alignment (even if that sample
        // sits in an inflated cell — body inflation is for transit only).
        path.back() = {gx, gy};
      }
      return path;
    }
    const int cx = cur % W;
    const int cy = cur / W;
    for (const auto& d : dirs) {
      const int nx = cx + d[0];
      const int ny = cy + d[1];
      if (nx < 0 || ny < 0 || nx >= W || ny >= H) {
        continue;
      }
      if (map.at(nx, ny)) {
        continue;
      }
      // No diagonal corner-cutting through inflated/occupied cells
      if (d[0] != 0 && d[1] != 0 &&
          (map.at(cx + d[0], cy) || map.at(cx, cy + d[1]))) {
        continue;
      }
      const int ni = idx(nx, ny);
      const float step = (d[0] != 0 && d[1] != 0) ? 1.4142f : 1.f;
      const float tentative = gscore[static_cast<size_t>(cur)] + step;
      if (tentative < gscore[static_cast<size_t>(ni)]) {
        came[static_cast<size_t>(ni)] = cur;
        gscore[static_cast<size_t>(ni)] = tentative;
        open.emplace(tentative + h(nx, ny), ni);
      }
    }
  }
  return {};
}

// True if world point is in a free (non-inflated) cell.
inline bool is_free(const GridMap& map, double wx, double wy)
{
  int cx = 0;
  int cy = 0;
  if (!map.world_to_cell(wx, wy, cx, cy)) {
    return false;
  }
  return map.at(cx, cy) == 0;
}

// Bresenham-ish LOS: true if the straight segment stays in free cells.
inline bool segment_free(const GridMap& map, double x0, double y0, double x1,
                         double y1)
{
  const double d = std::hypot(x1 - x0, y1 - y0);
  const int n = std::max(1, static_cast<int>(std::ceil(d / 0.08)));
  for (int i = 0; i <= n; ++i) {
    const double t = static_cast<double>(i) / static_cast<double>(n);
    if (!is_free(map, x0 + (x1 - x0) * t, y0 + (y1 - y0) * t)) {
      return false;
    }
  }
  return true;
}

// Heading error to a world point (normalized to (-pi, pi]).
inline double heading_err(double yaw, double x, double y, double tx, double ty)
{
  double err = std::atan2(ty - y, tx - x) - yaw;
  while (err > M_PI) {
    err -= 2.0 * M_PI;
  }
  while (err < -M_PI) {
    err += 2.0 * M_PI;
  }
  return err;
}

// Advance path index when past current wp or next wp is closer.
inline void advance_idx(const std::vector<std::pair<double, double>>& path,
                        size_t& idx, double x, double y)
{
  while (idx < path.size()) {
    const double d0 = std::hypot(path[idx].first - x, path[idx].second - y);
    if (idx + 1 < path.size()) {
      const double d1 =
          std::hypot(path[idx + 1].first - x, path[idx + 1].second - y);
      if (d0 < 0.35 || d1 + 0.05 < d0) {
        ++idx;
        continue;
      }
    } else if (d0 < 0.20) {
      // hold on last point; completion is decided by the caller
      break;
    }
    break;
  }
}

// First path point (from idx) within path-length `lookahead` that is LOS-free.
// Falls back to the next raw waypoint so we never aim through a wall corner.
inline std::pair<double, double> aim_point(const GridMap& map,
                                           const std::vector<std::pair<double, double>>& path,
                                           size_t idx, double x, double y,
                                           double lookahead)
{
  if (path.empty()) {
    return {x, y};
  }
  if (idx >= path.size()) {
    return path.back();
  }
  // Final approach: commanded goal is always aimable even if it sits in an
  // inflated sample (LOS would otherwise lock onto the free cell before it).
  if (std::hypot(path.back().first - x, path.back().second - y) < 0.60) {
    return path.back();
  }
  // Farthest LOS-visible point within path length budget
  double budget = lookahead;
  std::pair<double, double> best = path[idx];
  bool have = false;
  for (size_t i = idx; i < path.size(); ++i) {
    if (i > idx) {
      const double seg = std::hypot(path[i].first - path[i - 1].first,
                                    path[i].second - path[i - 1].second);
      budget -= seg;
      if (budget < 0.0) {
        break;
      }
    }
    if (!segment_free(map, x, y, path[i].first, path[i].second)) {
      break;
    }
    best = path[i];
    have = true;
    if (std::hypot(path[i].first - x, path[i].second - y) >= lookahead) {
      break;
    }
  }
  if (have) {
    return best;
  }
  return path[idx];
}

// Pure pursuit step for diff-drive (gap-aware, uses map for LOS aim).
inline void pure_pursuit(const GridMap& map, double x, double y, double yaw,
                         const std::vector<std::pair<double, double>>& path,
                         size_t& idx, double lookahead, double& v, double& w)
{
  v = 0.0;
  w = 0.0;
  if (path.empty() || idx >= path.size()) {
    return;
  }
  advance_idx(path, idx, x, y);
  if (idx >= path.size()) {
    return;
  }
  // Tight local segments (gap / staircase) → shorter lookahead, never longer
  double ld = lookahead;
  if (idx + 1 < path.size()) {
    const double seg = std::hypot(path[idx + 1].first - path[idx].first,
                                  path[idx + 1].second - path[idx].second);
    if (seg > 1e-3 && seg < 0.55) {
      ld = std::min(lookahead, std::max(0.28, seg * 1.2));
    }
  }
  const auto aim = aim_point(map, path, idx, x, y, ld);
  const double tx = aim.first;
  const double ty = aim.second;
  const double dx = tx - x;
  const double dy = ty - y;
  const double local_x = std::cos(yaw) * dx + std::sin(yaw) * dy;
  const double local_y = -std::sin(yaw) * dx + std::cos(yaw) * dy;
  const double dist = std::max(0.05, std::hypot(dx, dy));
  const double alpha = std::atan2(local_y, local_x);

  // Large heading error → rotate in place first (avoid wall-plowing circles)
  if (std::abs(alpha) > 0.9) {
    v = 0.0;
    w = (alpha > 0.0 ? 1.0 : -1.0) * 0.9;
    return;
  }
  // Target roughly behind → arc turn with limited v
  if (local_x < 0.0) {
    v = 0.10;
    w = (alpha >= 0.0 ? 1.0 : -1.0) * 0.6;
    return;
  }
  if (std::abs(alpha) < 0.5) {
    v = (std::abs(alpha) > 0.4 ? 0.12 : 0.28);
    w = 2.0 * v * std::sin(alpha) / dist;
    if (w > 1.0) w = 1.0;
    if (w < -1.0) w = -1.0;
    return;
  }
  v = 0.10;
  w = (alpha >= 0.0 ? 1.0 : -1.0) * 0.7;
}

inline std::string path_to_json(const std::vector<std::pair<double, double>>& path)
{
  std::string s = "[";
  for (size_t i = 0; i < path.size(); ++i) {
    if (i) {
      s += ",";
    }
    s += "[" + std::to_string(path[i].first) + "," + std::to_string(path[i].second) + "]";
  }
  s += "]";
  return s;
}

// Flat [x0,y0,x1,y1,...] for console canvas (avoids nested-array QML pitfalls).
inline std::string path_to_flat_json(
    const std::vector<std::pair<double, double>>& path)
{
  std::string s = "[";
  for (size_t i = 0; i < path.size(); ++i) {
    if (i) {
      s += ",";
    }
    s += std::to_string(path[i].first) + "," + std::to_string(path[i].second);
  }
  s += "]";
  return s;
}

}  // namespace nav
}  // namespace botcockpit_sim
