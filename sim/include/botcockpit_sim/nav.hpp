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
    // Vertical wall with gap (forces A* detour)
    for (int y = 2; y < m.height - 2; ++y) {
      if (y == 8 || y == 9 || y == 10) {
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
    return static_cast<float>(std::abs(x - gcx) + std::abs(y - gcy));
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

// Pure pursuit step for diff-drive: returns (v, w) toward lookahead point.
inline void pure_pursuit(double x, double y, double yaw,
                         const std::vector<std::pair<double, double>>& path,
                         size_t& idx, double lookahead, double& v, double& w)
{
  v = 0.0;
  w = 0.0;
  if (path.empty() || idx >= path.size()) {
    return;
  }
  // Advance index when close to current waypoint
  while (idx + 1 < path.size()) {
    const double dx = path[idx].first - x;
    const double dy = path[idx].second - y;
    if (std::hypot(dx, dy) < 0.25) {
      ++idx;
    } else {
      break;
    }
  }
  // Find lookahead point
  size_t target = idx;
  for (size_t i = idx; i < path.size(); ++i) {
    const double dx = path[i].first - x;
    const double dy = path[i].second - y;
    if (std::hypot(dx, dy) >= lookahead) {
      target = i;
      break;
    }
    target = i;
  }
  const double tx = path[target].first;
  const double ty = path[target].second;
  const double dx = tx - x;
  const double dy = ty - y;
  const double local_x = std::cos(yaw) * dx + std::sin(yaw) * dy;
  const double local_y = -std::sin(yaw) * dx + std::cos(yaw) * dy;
  const double Ld = std::max(0.15, std::hypot(dx, dy));
  v = 0.45;
  w = 2.0 * v * local_y / (Ld * Ld);
  if (local_x < 0.0) {
    // goal behind: rotate in place
    v = 0.05;
    w = (local_y >= 0 ? 1.0 : -1.0) * 0.8;
  }
  // clamp
  if (w > 1.2) w = 1.2;
  if (w < -1.2) w = -1.2;
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

}  // namespace nav
}  // namespace botcockpit_sim
