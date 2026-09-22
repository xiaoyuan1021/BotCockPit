#include "botcockpit_sim/fake_robot.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <sstream>

#include "botcockpit_sim/nav.hpp"

namespace botcockpit_sim {
namespace {

std::string esc(const std::string& s)
{
  std::string out;
  out.reserve(s.size());
  for (char c : s) {
    if (c == '"' || c == '\\') {
      out.push_back('\\');
    }
    out.push_back(c);
  }
  return out;
}

bool extract_string(const std::string& obj, const std::string& key,
                    std::string& out)
{
  const std::string needle = "\"" + key + "\"";
  size_t k = obj.find(needle);
  if (k == std::string::npos) {
    return false;
  }
  size_t colon = obj.find(':', k + needle.size());
  if (colon == std::string::npos) {
    return false;
  }
  size_t i = colon + 1;
  while (i < obj.size() && (obj[i] == ' ' || obj[i] == '\t')) {
    ++i;
  }
  if (i >= obj.size() || obj[i] != '"') {
    return false;
  }
  ++i;
  out.clear();
  while (i < obj.size() && obj[i] != '"') {
    if (obj[i] == '\\' && i + 1 < obj.size()) {
      out.push_back(obj[i + 1]);
      i += 2;
      continue;
    }
    out.push_back(obj[i]);
    ++i;
  }
  return true;
}

bool extract_number(const std::string& obj, const std::string& key, double& out)
{
  const std::string needle = "\"" + key + "\"";
  size_t k = obj.find(needle);
  if (k == std::string::npos) {
    return false;
  }
  size_t colon = obj.find(':', k + needle.size());
  if (colon == std::string::npos) {
    return false;
  }
  size_t i = colon + 1;
  while (i < obj.size() && (obj[i] == ' ' || obj[i] == '\t')) {
    ++i;
  }
  if (i >= obj.size()) {
    return false;
  }
  char* end = nullptr;
  out = std::strtod(obj.c_str() + i, &end);
  return end != obj.c_str() + i;
}

bool extract_bool(const std::string& obj, const std::string& key, bool& out)
{
  const std::string needle = "\"" + key + "\"";
  size_t k = obj.find(needle);
  if (k == std::string::npos) {
    return false;
  }
  size_t colon = obj.find(':', k + needle.size());
  if (colon == std::string::npos) {
    return false;
  }
  size_t i = colon + 1;
  while (i < obj.size() && (obj[i] == ' ' || obj[i] == '\t')) {
    ++i;
  }
  if (obj.compare(i, 4, "true") == 0) {
    out = true;
    return true;
  }
  if (obj.compare(i, 5, "false") == 0) {
    out = false;
    return true;
  }
  return false;
}

uint16_t extract_seq(const std::string& obj)
{
  double v = 0;
  if (extract_number(obj, "seq", v)) {
    return static_cast<uint16_t>(v);
  }
  return 0;
}

}  // namespace

uint64_t FakeRobot::now_ms()
{
  using namespace std::chrono;
  return static_cast<uint64_t>(
      duration_cast<milliseconds>(system_clock::now().time_since_epoch())
          .count());
}

double FakeRobot::dist(double x0, double y0, double x1, double y1)
{
  const double dx = x1 - x0;
  const double dy = y1 - y0;
  return std::sqrt(dx * dx + dy * dy);
}

FakeRobot::FakeRobot() : rclcpp_lifecycle::LifecycleNode("fake_robot")
{
  nodes_ = {{"fake_robot", "OK"}, {"bridge", "OK"}};
}

FakeRobot::~FakeRobot()
{
  if (tick_timer_) {
    tick_timer_->cancel();
  }
}

bool FakeRobot::has_error_condition() const
{
  for (const auto& f : faults_) {
    if (f.active && f.level == "ERROR") {
      return true;
    }
  }
  // Only the robot itself (or explicit ERROR faults) force FAULT.
  // Console/bridge offline is a safety state (ctl off), not a robot fault.
  for (const auto& n : nodes_) {
    if (n.name == "fake_robot" && (n.status == "ERROR" || n.status == "LOST")) {
      return true;
    }
  }
  return false;
}

bool FakeRobot::has_warn_condition() const
{
  for (const auto& f : faults_) {
    if (f.active && f.level == "WARN") {
      return true;
    }
  }
  for (const auto& n : nodes_) {
    if (n.name == "fake_robot" && n.status == "WARN") {
      return true;
    }
  }
  return false;
}

void FakeRobot::recompute_phase_and_control()
{
  // ESTOP highest priority (PROTOCOL §4).
  if (estop_) {
    phase_ = "ESTOP";
    control_enabled_ = false;
    return;
  }
  if (phase_ == "BOOT" || phase_ == "INITIALIZING") {
    control_enabled_ = false;
    return;
  }
  if (has_error_condition()) {
    if (phase_ == "RUNNING") {
      task_status_ = "FAILED";
    }
    phase_ = "FAULT";
    control_enabled_ = false;
    nodes_[0].status = "ERROR";
    return;
  }
  if (has_warn_condition()) {
    if (phase_ != "RUNNING") {
      phase_ = "DEGRADED";
    }
    control_enabled_ = false;
    nodes_[0].status = "WARN";
    return;
  }
  nodes_[0].status = "OK";
  if (phase_ == "FAULT" || phase_ == "DEGRADED" || phase_ == "ESTOP") {
    if (!estop_ && !has_error_condition() && !has_warn_condition()) {
      phase_ = "IDLE";
    }
  }
  // New tasks only in IDLE with live console link (PROTOCOL safety).
  if (phase_ == "IDLE") {
    control_enabled_ = console_online_;
  } else if (phase_ == "RUNNING") {
    control_enabled_ = false;
  }
}

void FakeRobot::apply_safety_stop(const std::string& why)
{
  // SAFE stop: halt motion, reject new control until conditions clear.
  if (task_status_ == "EXECUTING" || task_status_ == "ACCEPTED") {
    task_status_ = "FAILED";
  }
  if (phase_ == "RUNNING") {
    phase_ = "IDLE";
  }
  task_type_.clear();
  RCLCPP_WARN(get_logger(), "SAFE stop: %s", why.c_str());
  recompute_phase_and_control();
  if (!console_online_) {
    control_enabled_ = false;
  }
  publish_state();
}

CallbackReturn FakeRobot::on_configure(const rclcpp_lifecycle::State&)
{
  std::lock_guard<std::mutex> lock(mu_);
  phase_ = "INITIALIZING";
  control_enabled_ = false;
  estop_ = false;
  console_online_ = false;
  battery_ = 100.0;
  // Place start pose on free corridor cell (not on map border)
  pose_x_ = 1.25;
  pose_y_ = 0.0;
  pose_yaw_ = 0.0;
  task_status_ = "NONE";
  task_id_.clear();
  task_type_.clear();
  nav_path_.clear();
  trail_.clear();
  nav_status_ = "IDLE";
  clear_faults();
  nodes_[0].status = "OK";
  nodes_[1].status = "OK";

  state_pub_ = create_publisher<std_msgs::msg::String>(
      "botcockpit/state", rclcpp::QoS(10));
  result_pub_ = create_publisher<std_msgs::msg::String>(
      "botcockpit/cmd_result", rclcpp::QoS(10));
  cmd_sub_ = create_subscription<std_msgs::msg::String>(
      "botcockpit/cmd", rclcpp::QoS(10),
      std::bind(&FakeRobot::on_cmd, this, std::placeholders::_1));
  console_sub_ = create_subscription<std_msgs::msg::String>(
      "botcockpit/console", rclcpp::QoS(10),
      std::bind(&FakeRobot::on_console, this, std::placeholders::_1));

  publish_state();
  RCLCPP_INFO(get_logger(), "configured: topics ready");
  return CallbackReturn::SUCCESS;
}

CallbackReturn FakeRobot::on_activate(const rclcpp_lifecycle::State&)
{
  {
    std::lock_guard<std::mutex> lock(mu_);
    active_ = true;
    phase_ = "INITIALIZING";
    control_enabled_ = false;
    idle_tp_ = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
    last_tick_tp_ = std::chrono::steady_clock::now();
    publish_state();
  }
  tick_timer_ = create_wall_timer(
      std::chrono::milliseconds(200),
      std::bind(&FakeRobot::tick, this));
  RCLCPP_INFO(get_logger(), "activated: sim loop 5 Hz");
  return CallbackReturn::SUCCESS;
}

CallbackReturn FakeRobot::on_deactivate(const rclcpp_lifecycle::State&)
{
  if (tick_timer_) {
    tick_timer_->cancel();
    tick_timer_.reset();
  }
  std::lock_guard<std::mutex> lock(mu_);
  active_ = false;
  control_enabled_ = false;
  phase_ = "INITIALIZING";
  return CallbackReturn::SUCCESS;
}

CallbackReturn FakeRobot::on_cleanup(const rclcpp_lifecycle::State&)
{
  if (tick_timer_) {
    tick_timer_->cancel();
    tick_timer_.reset();
  }
  cmd_sub_.reset();
  console_sub_.reset();
  state_pub_.reset();
  result_pub_.reset();
  std::lock_guard<std::mutex> lock(mu_);
  phase_ = "BOOT";
  active_ = false;
  return CallbackReturn::SUCCESS;
}

CallbackReturn FakeRobot::on_shutdown(const rclcpp_lifecycle::State&)
{
  return on_cleanup(rclcpp_lifecycle::State());
}

void FakeRobot::on_console(const std_msgs::msg::String::SharedPtr msg)
{
  if (!msg) {
    return;
  }
  bool online = false;
  extract_bool(msg->data, "online", online);
  std::lock_guard<std::mutex> lock(mu_);
  console_online_ = online;
  if (online) {
    last_console_ms_ = now_ms();
    nodes_[1].status = "OK";
  } else {
    // Console link lost → safety stop + ctl off; not a robot ERROR fault.
    nodes_[1].status = "WARN";
    if (phase_ == "RUNNING") {
      apply_safety_stop("console offline");
    }
  }
  recompute_phase_and_control();
  if (!console_online_) {
    control_enabled_ = false;
  }
  publish_state();
}

void FakeRobot::tick()
{
  std::lock_guard<std::mutex> lock(mu_);
  if (!active_) {
    return;
  }
  const auto now = std::chrono::steady_clock::now();
  double dt = 0.2;
  if (last_tick_tp_.time_since_epoch().count() > 0) {
    dt = std::chrono::duration<double>(now - last_tick_tp_).count();
  }
  last_tick_tp_ = now;

  // Console heartbeat freshness (bridge publishes botcockpit/console ~2 Hz).
  if (console_online_ && last_console_ms_ > 0 &&
      now_ms() - last_console_ms_ > 3000) {
    console_online_ = false;
    nodes_[1].status = "WARN";
    if (phase_ == "RUNNING") {
      apply_safety_stop("console heartbeat lost");
    }
  }

  if (phase_ == "INITIALIZING" && now >= idle_tp_) {
    phase_ = "IDLE";
  }

  recompute_phase_and_control();

  // Plan A: A* path + pure pursuit tracking (diff-drive kinematics)
  if (!estop_ && phase_ == "RUNNING" && task_type_ == "goto") {
    if (!nav_path_.empty()) {
      const auto pose_before = std::make_pair(pose_x_, pose_y_);
      double v = 0.0, w = 0.0;
      nav::pure_pursuit(nav_map_, pose_x_, pose_y_, pose_yaw_, nav_path_,
                        nav_idx_, 0.45, v, w);
      double nx = pose_x_ + v * std::cos(pose_yaw_) * dt;
      double ny = pose_y_ + v * std::sin(pose_yaw_) * dt;
      bool collided = false;
      // Final approach: allow creeping into the commanded goal sample even if
      // it sits in an inflated cell (common when goal is near a wall).
      const double d_goal_now = dist(pose_x_, pose_y_, goal_x_, goal_y_);
      const double d_goal_next = dist(nx, ny, goal_x_, goal_y_);
      const bool final_creep = d_goal_now < 0.55 && d_goal_next < d_goal_now;
      // Collision guard: never step into inflated obstacle
      if (!final_creep && !nav::is_free(nav_map_, nx, ny)) {
        collided = true;
        // Rotate toward the LOS-visible path aim point (not a fixed side)
        v = 0.0;
        const auto aim = nav::aim_point(nav_map_, nav_path_, nav_idx_, pose_x_,
                                        pose_y_, 0.45);
        const double err = nav::heading_err(pose_yaw_, pose_x_, pose_y_,
                                            aim.first, aim.second);
        w = (std::abs(err) < 0.15 ? (w >= 0.0 ? 1.0 : -1.0)
                                  : (err >= 0.0 ? 1.0 : -1.0)) *
            0.8;
        nx = pose_x_;
        ny = pose_y_;
      }
      pose_x_ = nx;
      pose_y_ = ny;
      pose_yaw_ += w * dt;
      while (pose_yaw_ > M_PI) {
        pose_yaw_ -= 2 * M_PI;
      }
      while (pose_yaw_ < -M_PI) {
        pose_yaw_ += 2 * M_PI;
      }
      // Breadcrumb trail (already-driven route) for console canvas
      if (trail_.empty() ||
          dist(trail_.back().first, trail_.back().second, pose_x_, pose_y_) >
              0.05) {
        trail_.emplace_back(pose_x_, pose_y_);
        if (trail_.size() > 800) {
          trail_.erase(trail_.begin(), trail_.begin() + 200);
        }
      }
      task_status_ = "EXECUTING";
      nav_status_ = "TRACKING";

      // Stuck = no translation while blocked (or fully frozen).
      // Free in-place alignment (large heading error) is not stuck.
      const double moved =
          dist(pose_before.first, pose_before.second, pose_x_, pose_y_);
      const bool translating = moved > 0.002 * std::max(1.0, dt / 0.2);
      const bool aligning = !collided && std::abs(w) > 0.05 && !translating;
      if (translating || aligning) {
        stuck_s_ = 0.0;
      } else {
        stuck_s_ += dt;
      }
      if (stuck_s_ > 1.2) {
        stuck_s_ = 0.0;
        ++stuck_count_;
        nav_map_ = nav::GridMap::make_corridor_demo();
        // First replans keep body inflation; after repeats, plan on a thinner
        // body so narrow gaps stay reachable (collision guard still applies).
        nav_map_.inflate(stuck_count_ >= 3 ? 0 : 1);
        auto replanned =
            nav::astar(nav_map_, pose_x_, pose_y_, goal_x_, goal_y_);
        if (!replanned.empty()) {
          nav_path_ = std::move(replanned);
          nav_idx_ = 0;
          nav::advance_idx(nav_path_, nav_idx_, pose_x_, pose_y_);
          nav_status_ = "TRACKING";
        }
      }

      // cross-track error to current segment
      if (nav_idx_ < nav_path_.size()) {
        const size_t j = std::min(nav_idx_ + 1, nav_path_.size() - 1);
        const double ax = nav_path_[nav_idx_].first;
        const double ay = nav_path_[nav_idx_].second;
        const double bx = nav_path_[j].first;
        const double by = nav_path_[j].second;
        const double abx = bx - ax;
        const double aby = by - ay;
        const double apx = pose_x_ - ax;
        const double apy = pose_y_ - ay;
        const double ab2 = abx * abx + aby * aby;
        double cr = 0.0;
        if (ab2 > 1e-6) {
          cr = std::abs(abx * apy - aby * apx) / std::sqrt(ab2);
        } else {
          cr = dist(pose_x_, pose_y_, ax, ay);
        }
        track_err_ = cr;
        if (cr > track_err_max_) {
          track_err_max_ = cr;
        }
        if (cr > 1.5) {
          task_status_ = "FAILED";
          phase_ = "IDLE";
          nav_status_ = "FAILED";
          faults_.push_back({"E_TASK_FAILED", "ERROR", "fake_robot",
                             "tracking error too large", true});
          recompute_phase_and_control();
        }
      }

      // Completion: snap exactly to commanded goal (end-point alignment)
      const double d_goal =
          dist(pose_x_, pose_y_, goal_x_, goal_y_);
      if (d_goal < 0.22) {
        pose_x_ = goal_x_;
        pose_y_ = goal_y_;
        if (trail_.empty() ||
            dist(trail_.back().first, trail_.back().second, pose_x_,
                 pose_y_) > 0.02) {
          trail_.emplace_back(pose_x_, pose_y_);
        }
        track_err_ = 0.0;
        task_status_ = "DONE";
        phase_ = "IDLE";
        nav_status_ = "IDLE";
        stuck_s_ = 0.0;
        stuck_count_ = 0;
        recompute_phase_and_control();
      }
    } else {
      task_status_ = "FAILED";
      phase_ = "IDLE";
      nav_status_ = "FAILED";
      recompute_phase_and_control();
    }
    battery_ -= 0.05 * dt;
  } else if (!estop_ && phase_ == "IDLE") {
    // Do not spin in place while idle (was causing "always turning" look)
    battery_ -= 0.02 * dt;
  }

  if (battery_ < 0.0) {
    battery_ = 0.0;
  }
  if (battery_ < 15.0) {
    bool has_low = false;
    for (const auto& f : faults_) {
      if (f.code == "W_BATTERY_LOW" && f.active) {
        has_low = true;
        break;
      }
    }
    if (!has_low) {
      faults_.push_back(
          {"W_BATTERY_LOW", "WARN", "fake_robot", "battery below 15%", true});
      recompute_phase_and_control();
    }
  }

  publish_state();
}

std::string FakeRobot::build_state_json() const
{
  std::ostringstream oss;
  const char* conn = "ONLINE";
  if (estop_) {
    conn = "ESTOP";
  } else if (phase_ == "FAULT") {
    conn = "OFFLINE";
  } else if (!console_online_) {
    conn = "OFFLINE";
  } else if (phase_ == "DEGRADED") {
    conn = "DEGRADED";
  }

  oss << "{";
  oss << "\"ts_ms\":" << now_ms();
  oss << ",\"conn\":\"" << conn << "\"";
  oss << ",\"mode\":\"" << esc(mode_) << "\"";
  oss << ",\"phase\":\"" << esc(phase_) << "\"";
  oss << ",\"heartbeat_rtt_ms\":0";
  oss << ",\"nodes\":[";
  for (size_t i = 0; i < nodes_.size(); ++i) {
    const auto& n = nodes_[i];
    if (i) {
      oss << ",";
    }
    oss << "{\"name\":\"" << esc(n.name) << "\",\"status\":\"" << esc(n.status)
        << "\",\"last_hb_ms\":" << now_ms() << "}";
  }
  oss << "]";
  oss << ",\"pose\":{\"x\":" << pose_x_ << ",\"y\":" << pose_y_ << ",\"yaw\":"
      << pose_yaw_ << "}";
  oss << ",\"battery\":" << battery_;
  oss << ",\"task\":{\"id\":";
  if (task_id_.empty()) {
    oss << "null";
  } else {
    oss << "\"" << esc(task_id_) << "\"";
  }
  oss << ",\"type\":";
  if (task_type_.empty()) {
    oss << "null";
  } else {
    oss << "\"" << esc(task_type_) << "\"";
  }
  oss << ",\"status\":\"" << esc(task_status_) << "\"}";
  oss << ",\"faults\":[";
  for (size_t i = 0; i < faults_.size(); ++i) {
    const auto& f = faults_[i];
    if (i) {
      oss << ",";
    }
    oss << "{\"ts_ms\":" << now_ms() << ",\"code\":\"" << esc(f.code)
        << "\",\"level\":\"" << esc(f.level) << "\",\"node\":\"" << esc(f.node)
        << "\",\"detail\":\"" << esc(f.detail) << "\",\"active\":"
        << (f.active ? "true" : "false") << "}";
  }
  oss << "]";
  oss << ",\"estop\":" << (estop_ ? "true" : "false");
  oss << ",\"control_enabled\":" << (control_enabled_ ? "true" : "false");
  // Plan A nav summary (path + trail + cursor for console canvas)
  oss << ",\"nav\":{\"status\":\"" << esc(nav_status_) << "\",\"path_len\":"
      << nav_path_.size() << ",\"idx\":" << nav_idx_
      << ",\"path\":" << nav::path_to_json(nav_path_)
      << ",\"path_flat\":" << nav::path_to_flat_json(nav_path_)
      << ",\"trail\":" << nav::path_to_json(trail_)
      << ",\"trail_flat\":" << nav::path_to_flat_json(trail_)
      << ",\"goal\":{\"x\":" << goal_x_ << ",\"y\":" << goal_y_ << "}"
      << ",\"track_err\":" << track_err_ << ",\"track_err_max\":" << track_err_max_
      << "}";
  oss << "}";
  return oss.str();
}

void FakeRobot::publish_state()
{
  if (!state_pub_) {
    return;
  }
  std_msgs::msg::String msg;
  msg.data = build_state_json();
  state_pub_->publish(msg);
}

void FakeRobot::publish_cmd_result(uint16_t seq, const std::string& cmd,
                                   bool ok, const std::string& status,
                                   const std::string& reason,
                                   const std::string& task_id)
{
  if (!result_pub_) {
    return;
  }
  std::ostringstream oss;
  oss << "{\"seq\":" << seq << ",\"ok\":" << (ok ? "true" : "false")
      << ",\"cmd\":\"" << esc(cmd) << "\",\"status\":\"" << esc(status)
      << "\",\"reason\":";
  if (reason.empty()) {
    oss << "null";
  } else {
    oss << "\"" << esc(reason) << "\"";
  }
  oss << ",\"task_id\":";
  if (task_id.empty()) {
    oss << "null";
  } else {
    oss << "\"" << esc(task_id) << "\"";
  }
  oss << "}";
  std_msgs::msg::String msg;
  msg.data = oss.str();
  result_pub_->publish(msg);
}

void FakeRobot::inject_fault(const std::string& detail)
{
  faults_.push_back({"E_NODE_TIMEOUT", "ERROR", "fake_robot", detail, true});
  nodes_[0].status = "ERROR";
  if (task_status_ == "EXECUTING" || task_status_ == "ACCEPTED") {
    task_status_ = "FAILED";
  }
  recompute_phase_and_control();
}

void FakeRobot::clear_faults()
{
  faults_.clear();
  for (auto& n : nodes_) {
    if (n.status == "ERROR" || n.status == "WARN" || n.status == "LOST") {
      // keep bridge LOST only if console actually offline — restored in on_console
      if (n.name == "fake_robot") {
        n.status = "OK";
      }
    }
  }
}

void FakeRobot::on_cmd(const std_msgs::msg::String::SharedPtr msg)
{
  if (!msg) {
    return;
  }
  const std::string& data = msg->data;
  std::string cmd;
  if (!extract_string(data, "cmd", cmd)) {
    return;
  }
  const uint16_t seq = extract_seq(data);

  std::lock_guard<std::mutex> lock(mu_);

  // PROTOCOL §4: ESTOP / FAULT / offline reject tasks & AUTO.
  const bool blocked = estop_ || phase_ == "FAULT" || phase_ == "DEGRADED" ||
                       phase_ == "BOOT" || phase_ == "INITIALIZING" ||
                       !console_online_;

  if (cmd == "CMD_ESTART" || cmd == "CMD_ESTOP") {
    estop_ = true;
    phase_ = "ESTOP";
    control_enabled_ = false;
    if (task_status_ == "EXECUTING" || task_status_ == "ACCEPTED") {
      task_status_ = "FAILED";
    }
    bool has_estop_fault = false;
    for (const auto& f : faults_) {
      if (f.code == "E_ESTOP_ACTIVE") {
        has_estop_fault = true;
        break;
      }
    }
    if (!has_estop_fault) {
      faults_.push_back({"E_ESTOP_ACTIVE", "WARN", "fake_robot",
                         "software estop engaged", true});
    }
    publish_cmd_result(seq, "CMD_ESTOP", true, "ACCEPTED", "");
    publish_state();
    return;
  }

  if (cmd == "CMD_RESET") {
    bool confirm = false;
    if (!extract_bool(data, "confirm", confirm) || !confirm) {
      publish_cmd_result(seq, cmd, false, "REJECTED", "confirm_required");
      return;
    }
    // Operator RESET must recover from ESTOP / task failures / battery warn.
    // Only keep safety-critical ERROR sources (e.g. E_NODE_TIMEOUT) as blocking.
    bool has_blocking = false;
    for (const auto& f : faults_) {
      if (f.code == "E_ESTOP_ACTIVE" || f.code == "W_BATTERY_LOW" ||
          f.code == "E_TASK_FAILED") {
        continue;  // symptom / task / warn — clearable by RESET
      }
      if (f.level == "ERROR") {
        has_blocking = true;
        break;
      }
    }
    if (has_blocking) {
      publish_cmd_result(seq, cmd, false, "REJECTED", "phase_busy");
      publish_state();
      return;
    }
    estop_ = false;
    clear_faults();
    if (!console_online_) {
      nodes_[1].status = "WARN";
    } else {
      nodes_[1].status = "OK";
    }
    phase_ = "IDLE";
    mode_ = "TELEOP";
    task_status_ = "NONE";
    task_id_.clear();
    task_type_.clear();
    recompute_phase_and_control();
    control_enabled_ = console_online_ && phase_ == "IDLE";
    publish_cmd_result(seq, cmd, true, "ACCEPTED", "");
    publish_state();
    return;
  }

  if (cmd == "CMD_MODE") {
    std::string mode;
    if (!extract_string(data, "mode", mode)) {
      publish_cmd_result(seq, cmd, false, "REJECTED", "invalid_payload");
      return;
    }
    if (mode != "TELEOP" && mode != "AUTO" && mode != "REMOTE") {
      publish_cmd_result(seq, cmd, false, "REJECTED", "invalid_payload");
      return;
    }
    if (mode == "AUTO" && blocked) {
      publish_cmd_result(seq, cmd, false, "REJECTED",
                         estop_ ? "estop_active" : "phase_busy");
      return;
    }
    mode_ = mode;
    publish_cmd_result(seq, cmd, true, "ACCEPTED", "");
    publish_state();
    return;
  }

  if (cmd == "CMD_TASK") {
    std::string type;
    std::string task_id;
    extract_string(data, "type", type);
    extract_string(data, "task_id", task_id);

    // Control verbs allowed while RUNNING (except resume start conditions).
    if (type == "pause") {
      if (phase_ == "RUNNING") {
        task_status_ = "ACCEPTED";
        phase_ = "IDLE";
        nav_status_ = "IDLE";
        recompute_phase_and_control();
        publish_cmd_result(seq, cmd, true, "ACCEPTED", "", task_id_);
      } else {
        publish_cmd_result(seq, cmd, false, "REJECTED", "unknown_task", task_id);
      }
      publish_state();
      return;
    }
    if (type == "cancel") {
      task_status_ = "NONE";
      task_id_.clear();
      task_type_.clear();
      nav_path_.clear();
      nav_status_ = "IDLE";
      stuck_s_ = 0.0;
      stuck_count_ = 0;
      if (phase_ == "RUNNING") {
        phase_ = "IDLE";
      }
      recompute_phase_and_control();
      publish_cmd_result(seq, cmd, true, "ACCEPTED", "");
      publish_state();
      return;
    }

    if (blocked) {
      publish_cmd_result(seq, cmd, false, "REJECTED",
                         estop_ ? "estop_active" : "phase_busy", task_id);
      return;
    }

    if (type == "goto") {
      if (phase_ == "RUNNING") {
        publish_cmd_result(seq, cmd, false, "REJECTED", "phase_busy", task_id);
        return;
      }
      double x = pose_x_;
      double y = pose_y_;
      extract_number(data, "x", x);
      extract_number(data, "y", y);
      // Map world domain: x∈[0,20], y∈[-5,5] (nav.hpp origin+size)
      if (x < 0.0 || x > 20.0 || y < -5.0 || y > 5.0) {
        publish_cmd_result(seq, cmd, false, "REJECTED", "invalid_payload",
                           task_id);
        return;
      }
      if (task_id.empty()) {
        ++task_seq_;
        task_id = "T-" + std::to_string(task_seq_);
      }
      if (!task_id_.empty() && task_id_ == task_id && task_status_ == "DONE") {
        publish_cmd_result(seq, cmd, true, "DONE", "", task_id);
        return;
      }
      nav_map_ = nav::GridMap::make_corridor_demo();
      nav_map_.inflate(1);
      nav_status_ = "PLANNING";
      nav_path_ = nav::astar(nav_map_, pose_x_, pose_y_, x, y);
      if (nav_path_.empty()) {
        nav_status_ = "FAILED";
        faults_.push_back({"E_TASK_FAILED", "ERROR", "fake_robot",
                           "A* no path to goal", true});
        publish_cmd_result(seq, cmd, false, "REJECTED", "unknown_task", task_id);
        recompute_phase_and_control();
        publish_state();
        return;
      }
      nav_idx_ = 0;
      nav_status_ = "TRACKING";
      nav_ready_ = true;
      stuck_s_ = 0.0;
      stuck_count_ = 0;
      trail_.clear();
      trail_.emplace_back(pose_x_, pose_y_);
      task_id_ = task_id;
      task_type_ = "goto";
      task_status_ = "ACCEPTED";
      goal_x_ = x;
      goal_y_ = y;
      phase_ = "RUNNING";
      control_enabled_ = false;
      publish_cmd_result(seq, cmd, true, "ACCEPTED", "", task_id);
      publish_state();
      return;
    }
    if (type == "resume") {
      if (!task_id_.empty() && task_type_ == "goto" && !estop_ &&
          phase_ == "IDLE" && console_online_ && !has_error_condition() &&
          !nav_path_.empty()) {
        phase_ = "RUNNING";
        task_status_ = "EXECUTING";
        nav_status_ = "TRACKING";
        control_enabled_ = false;
        publish_cmd_result(seq, cmd, true, "ACCEPTED", "", task_id_);
      } else {
        publish_cmd_result(seq, cmd, false, "REJECTED",
                           estop_ ? "estop_active" : "unknown_task", task_id);
      }
      publish_state();
      return;
    }
    if (type == "cancel") {
      task_status_ = "NONE";
      task_id_.clear();
      task_type_.clear();
      if (phase_ == "RUNNING") {
        phase_ = "IDLE";
      }
      recompute_phase_and_control();
      publish_cmd_result(seq, cmd, true, "ACCEPTED", "");
      publish_state();
      return;
    }
    publish_cmd_result(seq, cmd, false, "REJECTED", "unknown_task", task_id);
    return;
  }

  if (cmd == "INJECT_FAULT" || cmd == "CMD_INJECT_FAULT") {
    std::string detail = "injected by operator";
    extract_string(data, "detail", detail);
    inject_fault(detail);
    publish_cmd_result(seq, cmd, true, "ACCEPTED", "");
    publish_state();
    return;
  }

  if (cmd == "CLEAR_FAULT" || cmd == "CMD_CLEAR_FAULT") {
    clear_faults();
    nodes_[1].status = console_online_ ? "OK" : "WARN";
    recompute_phase_and_control();
    if (phase_ == "FAULT" && !has_error_condition()) {
      phase_ = "IDLE";
    }
    control_enabled_ = console_online_ && phase_ == "IDLE" && !estop_;
    publish_cmd_result(seq, cmd, true, "ACCEPTED", "");
    publish_state();
    return;
  }

  publish_cmd_result(seq, cmd, false, "REJECTED", "not_implemented");
}

}  // namespace botcockpit_sim
