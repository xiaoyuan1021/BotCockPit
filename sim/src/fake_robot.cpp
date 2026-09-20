#include "botcockpit_sim/fake_robot.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <sstream>

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

// Tiny helpers for command envelope: {"seq":n,"cmd":"...","payload":{...}}
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

CallbackReturn FakeRobot::on_configure(const rclcpp_lifecycle::State&)
{
  std::lock_guard<std::mutex> lock(mu_);
  phase_ = "INITIALIZING";
  control_enabled_ = false;
  estop_ = false;
  battery_ = 100.0;
  task_status_ = "NONE";
  task_id_.clear();
  task_type_.clear();
  clear_faults();
  nodes_[0].status = "OK";
  configure_tp_ = std::chrono::steady_clock::now();

  state_pub_ = create_publisher<std_msgs::msg::String>(
      "botcockpit/state", rclcpp::QoS(10));
  result_pub_ = create_publisher<std_msgs::msg::String>(
      "botcockpit/cmd_result", rclcpp::QoS(10));
  cmd_sub_ = create_subscription<std_msgs::msg::String>(
      "botcockpit/cmd", rclcpp::QoS(10),
      std::bind(&FakeRobot::on_cmd, this, std::placeholders::_1));

  // WEEK1: configure 后即可发布状态（activate 前 bridge/QML 也能看到字段）
  publish_state();
  RCLCPP_INFO(get_logger(),
              "configured: state publisher ready on botcockpit/state");
  return CallbackReturn::SUCCESS;
}

CallbackReturn FakeRobot::on_activate(const rclcpp_lifecycle::State&)
{
  {
    std::lock_guard<std::mutex> lock(mu_);
    active_ = true;
    // Brief INIT then IDLE (week-1 simplified).
    phase_ = "INITIALIZING";
    control_enabled_ = false;
    idle_tp_ = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
    last_tick_tp_ = std::chrono::steady_clock::now();
    // 立刻发一帧，避免 UI 在首个 tick 前一直显示 0
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
  RCLCPP_INFO(get_logger(), "deactivated");
  return CallbackReturn::SUCCESS;
}

CallbackReturn FakeRobot::on_cleanup(const rclcpp_lifecycle::State&)
{
  if (tick_timer_) {
    tick_timer_->cancel();
    tick_timer_.reset();
  }
  cmd_sub_.reset();
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

  // INITIALIZING -> IDLE after short delay
  if (phase_ == "INITIALIZING" && now >= idle_tp_) {
    phase_ = "IDLE";
    control_enabled_ = !estop_;
  }

  if (!estop_) {
    // Simulate idle drift + battery drain
    if (phase_ == "IDLE") {
      pose_x_ += 0.1 * dt;  // ~0.1 m/s demo drift, wraps
      if (pose_x_ > 10.0) {
        pose_x_ = 0.0;
      }
      battery_ -= 0.02 * dt;
    } else if (phase_ == "RUNNING" && task_type_ == "goto") {
      const double speed = 0.5;  // m/s
      const double d = dist(pose_x_, pose_y_, goal_x_, goal_y_);
      if (d < 0.05) {
        pose_x_ = goal_x_;
        pose_y_ = goal_y_;
        task_status_ = "DONE";
        phase_ = "IDLE";
      } else {
        const double step = speed * dt;
        const double ratio = (d > 1e-6) ? (step / d) : 1.0;
        pose_x_ += (goal_x_ - pose_x_) * std::min(1.0, ratio);
        pose_y_ += (goal_y_ - pose_y_) * std::min(1.0, ratio);
        pose_yaw_ = std::atan2(goal_y_ - pose_y_, goal_x_ - pose_x_);
        task_status_ = "EXECUTING";
      }
      battery_ -= 0.05 * dt;
    }
    if (battery_ < 0.0) {
      battery_ = 0.0;
      if (faults_.empty()) {
        faults_.push_back({"W_BATTERY_LOW", "WARN", "fake_robot",
                           "battery below threshold", true});
      }
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
  if (task_id_.empty() || task_id_ == "null") {
    oss << "null";
  } else {
    oss << "\"" << esc(task_id_) << "\"";
  }
  oss << ",\"type\":";
  if (task_type_.empty() || task_type_ == "null") {
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
  nodes_[0].status = "ERROR";
  faults_.push_back(
      {"E_NODE_TIMEOUT", "ERROR", "fake_robot", detail, true});
  if (phase_ != "ESTOP") {
    phase_ = "FAULT";
    control_enabled_ = false;
  }
  task_status_ = "FAILED";
}

void FakeRobot::clear_faults()
{
  faults_.clear();
  for (auto& n : nodes_) {
    if (n.status == "ERROR" || n.status == "WARN") {
      n.status = "OK";
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

  // Safety rules (PROTOCOL.md §4): ESTOP / FAULT reject new tasks & AUTO mode.
  const bool blocked = estop_ || phase_ == "FAULT" || phase_ == "BOOT" ||
                       phase_ == "INITIALIZING" || phase_ == "DEGRADED";

  if (cmd == "CMD_ESTOP") {
    estop_ = true;
    phase_ = "ESTOP";
    control_enabled_ = false;
    task_status_ = (task_status_ == "EXECUTING") ? "FAILED" : task_status_;
    faults_.push_back({"E_ESTOP_ACTIVE", "WARN", "fake_robot",
                       "software estop engaged", true});
    nodes_[0].status = "OK";  // estop is not node failure
    publish_cmd_result(seq, cmd, true, "ACCEPTED", "");
    publish_state();
    return;
  }

  if (cmd == "CMD_RESET") {
    bool confirm = false;
    if (!extract_bool(data, "confirm", confirm) || !confirm) {
      publish_cmd_result(seq, cmd, false, "REJECTED", "confirm_required");
      return;
    }
    if (!faults_.empty()) {
      publish_cmd_result(seq, cmd, false, "REJECTED", "estop_active");
      return;
    }
    // No active faults -> leave ESTOP/FAULT to IDLE
    estop_ = false;
    clear_faults();
    phase_ = "IDLE";
    mode_ = "TELEOP";
    task_status_ = "NONE";
    task_id_.clear();
    task_type_.clear();
    control_enabled_ = true;
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

    if (blocked) {
      publish_cmd_result(seq, cmd, false, "REJECTED",
                         estop_ ? "estop_active" : "phase_busy");
      return;
    }
    if (phase_ == "RUNNING") {
      publish_cmd_result(seq, cmd, false, "REJECTED", "phase_busy", task_id);
      return;
    }
    if (mode_ != "AUTO" && type == "goto") {
      // Allow goto in TELEOP for week-1 sim convenience? PROTOCOL: new tasks
      // allowed in IDLE if mode suitable. goto is AUTO-oriented; still accept
      // in TELEOP for lab debug — record decision in ACCEPTANCE.
    }

    if (type == "goto") {
      double x = pose_x_;
      double y = pose_y_;
      extract_number(data, "x", x);
      extract_number(data, "y", y);
      if (task_id.empty()) {
        task_id = "T-sim";
      }
      if (!task_id_.empty() && task_id_ == task_id &&
          task_status_ == "DONE") {
        publish_cmd_result(seq, cmd, true, "DONE", "", task_id);
        return;
      }
      task_id_ = task_id;
      task_type_ = "goto";
      task_status_ = "ACCEPTED";
      goal_x_ = x;
      goal_y_ = y;
      phase_ = "RUNNING";
      publish_cmd_result(seq, cmd, true, "ACCEPTED", "", task_id);
      publish_state();
      return;
    }
    if (type == "pause") {
      if (phase_ == "RUNNING") {
        task_status_ = "ACCEPTED";
        phase_ = "IDLE";
        publish_cmd_result(seq, cmd, true, "ACCEPTED", "", task_id_);
      } else {
        publish_cmd_result(seq, cmd, false, "REJECTED", "unknown_task",
                           task_id);
      }
      return;
    }
    if (type == "resume") {
      if (!task_id_.empty() && task_type_ == "goto") {
        phase_ = "RUNNING";
        task_status_ = "EXECUTING";
        publish_cmd_result(seq, cmd, true, "ACCEPTED", "", task_id_);
      } else {
        publish_cmd_result(seq, cmd, false, "REJECTED", "unknown_task",
                           task_id);
      }
      return;
    }
    if (type == "cancel") {
      task_status_ = "NONE";
      task_id_.clear();
      task_type_.clear();
      if (phase_ == "RUNNING") {
        phase_ = "IDLE";
      }
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
    if (phase_ == "FAULT") {
      phase_ = "IDLE";
      control_enabled_ = !estop_;
    }
    publish_cmd_result(seq, cmd, true, "ACCEPTED", "");
    publish_state();
    return;
  }

  publish_cmd_result(seq, cmd, false, "REJECTED", "not_implemented");
}

}  // namespace botcockpit_sim
