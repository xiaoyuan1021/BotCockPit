#include "botcockpit_bridge/bridge_node.hpp"

#include <sstream>

#include "botcockpit_bridge/json_util.hpp"
#include "botcockpit_bridge/protocol.hpp"

namespace botcockpit {

uint64_t BridgeNode::now_ms()
{
  using namespace std::chrono;
  return static_cast<uint64_t>(
      duration_cast<milliseconds>(system_clock::now().time_since_epoch())
          .count());
}

BridgeNode::BridgeNode(const rclcpp::NodeOptions& options)
    : Node("botcockpit_bridge", options)
{
  state_sub_ = create_subscription<std_msgs::msg::String>(
      "botcockpit/state", rclcpp::QoS(10),
      std::bind(&BridgeNode::on_robot_state, this, std::placeholders::_1));
  cmd_result_sub_ = create_subscription<std_msgs::msg::String>(
      "botcockpit/cmd_result", rclcpp::QoS(10),
      std::bind(&BridgeNode::on_cmd_result, this, std::placeholders::_1));
  cmd_pub_ = create_publisher<std_msgs::msg::String>("botcockpit/cmd",
                                                     rclcpp::QoS(10));
  console_pub_ = create_publisher<std_msgs::msg::String>("botcockpit/console",
                                                         rclcpp::QoS(10));
  delta_timer_ = create_wall_timer(
      std::chrono::milliseconds(STATE_DELTA_MS),
      std::bind(&BridgeNode::delta_timer_cb, this));
  RCLCPP_INFO(get_logger(), "bridge node created");
}

BridgeNode::~BridgeNode()
{
  stop_tcp();
}

void BridgeNode::log(const std::string& line)
{
  RCLCPP_INFO(get_logger(), "%s", line.c_str());
}

bool BridgeNode::start_tcp(int port)
{
  server_ = std::make_unique<TcpServer>(
      [this]() { return state_for_tcp(); },
      [this](const std::string& cmd, uint16_t seq, const std::string& payload) {
        return handle_command(cmd, seq, payload);
      },
      [this](const std::string& cmd, uint16_t seq, const std::string& payload) {
        urgent_estop(cmd, seq, payload);
      },
      [this](const std::string& line) { log(line); });
  return server_->start(port);
}

void BridgeNode::stop_tcp()
{
  if (server_) {
    server_->stop();
    server_.reset();
  }
}

bool BridgeNode::robot_state_fresh() const
{
  const uint64_t last = last_robot_state_ms_.load();
  return last > 0 && (now_ms() - last) < static_cast<uint64_t>(HB_TIMEOUT_MS);
}

std::string BridgeNode::state_for_tcp() const
{
  if (!robot_state_fresh()) {
    std::ostringstream oss;
    oss << "{\"ts_ms\":" << now_ms()
        << ",\"conn\":\"OFFLINE\",\"mode\":\"TELEOP\",\"phase\":\"BOOT\""
        << ",\"heartbeat_rtt_ms\":0,\"nodes\":[]"
        << ",\"pose\":{\"x\":0,\"y\":0,\"yaw\":0},\"battery\":0"
        << ",\"task\":{\"id\":null,\"type\":null,\"status\":\"NONE\"}"
        << ",\"faults\":[],\"estop\":false,\"control_enabled\":false}";
    return oss.str();
  }
  std::lock_guard<std::mutex> lock(state_mu_);
  return robot_state_json_;
}

void BridgeNode::on_robot_state(const std_msgs::msg::String::SharedPtr msg)
{
  if (!msg) {
    return;
  }
  {
    std::lock_guard<std::mutex> lock(state_mu_);
    robot_state_json_ = msg->data;
  }
  last_robot_state_ms_.store(now_ms());
}

void BridgeNode::on_cmd_result(const std_msgs::msg::String::SharedPtr msg)
{
  if (!msg) {
    return;
  }
  long long seq_ll = -1;
  if (!json::get_int(msg->data, "seq", seq_ll) || seq_ll < 0) {
    return;
  }
  const auto seq = static_cast<uint16_t>(seq_ll);

  // Async ESTOP completion path
  if (async_estop_seq_valid_.load() && async_estop_seq_.load() == seq) {
    estop_raised_.store(false);
    async_estop_seq_valid_.store(false);
    if (server_) {
      server_->complete_async_ack(seq, msg->data);
    }
    return;
  }

  {
    std::lock_guard<std::mutex> lock(pending_mu_);
    auto it = pending_.find(seq);
    if (it == pending_.end()) {
      return;
    }
    it->second.ack_json = msg->data;
    it->second.done = true;
  }
  pending_cv_.notify_all();
}

void BridgeNode::delta_timer_cb()
{
  if (server_) {
    server_->broadcast_state_delta();
  }
  if (console_pub_) {
    const bool online = server_ && server_->client_count() > 0;
    std_msgs::msg::String msg;
    msg.data = std::string("{\"online\":") + (online ? "true" : "false") + "}";
    console_pub_->publish(msg);
  }
}

void BridgeNode::publish_cmd_envelope(const std::string& cmd_name, uint16_t seq,
                                      const std::string& payload)
{
  if (!cmd_pub_) {
    return;
  }
  std_msgs::msg::String out;
  std::ostringstream oss;
  oss << "{\"seq\":" << seq << ",\"cmd\":\"" << json::escape(cmd_name)
      << "\",\"payload\":" << (payload.empty() ? "null" : payload) << "}";
  out.data = oss.str();
  cmd_pub_->publish(out);
}

void BridgeNode::urgent_estop(const std::string& cmd_name, uint16_t seq,
                              const std::string& payload)
{
  // PROTOCOL §7.1: ESTOP must not wait behind in-flight ACK on TCP recv path.
  estop_raised_.store(true);
  async_estop_seq_.store(seq);
  async_estop_seq_valid_.store(true);
  {
    std::lock_guard<std::mutex> lock(pending_mu_);
    PendingCmd pc;
    pc.done = false;
    pending_[seq] = pc;  // also tracked in case result path uses pending
  }
  publish_cmd_envelope(cmd_name.empty() ? "CMD_ESTOP" : cmd_name, seq, payload);
  log("[ros] URGENT publish " + (cmd_name.empty() ? "CMD_ESTOP" : cmd_name) +
      " seq=" + std::to_string(seq));
  pending_cv_.notify_all();
}

std::string BridgeNode::handle_command(const std::string& cmd_name,
                                       uint16_t seq,
                                       const std::string& payload)
{
  if (cmd_name == "CMD_ESTOP") {
    // Fallback if urgent path not used
    urgent_estop(cmd_name, seq, payload);
    return "{\"seq\":" + std::to_string(seq) +
           ",\"ok\":true,\"cmd\":\"CMD_ESTOP\",\"status\":\"EXECUTING\","
           "\"reason\":null}";
  }

  if (!robot_state_fresh()) {
    return "{\"seq\":" + std::to_string(seq) + ",\"ok\":false,\"cmd\":\"" +
           cmd_name + "\",\"status\":\"REJECTED\",\"reason\":\"timeout\"}";
  }
  if (!cmd_pub_) {
    return "{\"seq\":" + std::to_string(seq) + ",\"ok\":false,\"cmd\":\"" +
           cmd_name + "\",\"status\":\"REJECTED\",\"reason\":\"not_implemented\"}";
  }

  if (cmd_name == "CMD_TASK" ||
      (cmd_name == "CMD_MODE" && payload.find("\"AUTO\"") != std::string::npos)) {
    std::string snap;
    {
      std::lock_guard<std::mutex> lock(state_mu_);
      snap = robot_state_json_;
    }
    bool estop = false;
    json::get_bool(snap, "estop", estop);
    if (estop) {
      return "{\"seq\":" + std::to_string(seq) + ",\"ok\":false,\"cmd\":\"" +
             cmd_name +
             "\",\"status\":\"REJECTED\",\"reason\":\"estop_active\"}";
    }
  }

  {
    std::lock_guard<std::mutex> lock(pending_mu_);
    PendingCmd pc;
    pc.done = false;
    pending_[seq] = pc;
  }
  publish_cmd_envelope(cmd_name, seq, payload);
  log("[ros] publish cmd " + cmd_name + " seq=" + std::to_string(seq));
  return wait_cmd_result(seq, cmd_name);
}

std::string BridgeNode::wait_cmd_result(uint16_t seq, const std::string& cmd_name)
{
  std::unique_lock<std::mutex> lock(pending_cv_mu_);
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::milliseconds(ACK_TIMEOUT_MS);
  bool got = false;
  std::string ack;
  while (std::chrono::steady_clock::now() < deadline) {
    // Interruptible: ESTOP raised while waiting → abort this non-urgent wait
    if (estop_raised_.load()) {
      std::lock_guard<std::mutex> plock(pending_mu_);
      pending_.erase(seq);
      return "{\"seq\":" + std::to_string(seq) + ",\"ok\":false,\"cmd\":\"" +
             cmd_name +
             "\",\"status\":\"REJECTED\",\"reason\":\"estop_active\"}";
    }
    {
      std::lock_guard<std::mutex> plock(pending_mu_);
      auto it = pending_.find(seq);
      if (it != pending_.end() && it->second.done) {
        ack = it->second.ack_json;
        pending_.erase(it);
        got = true;
      }
    }
    if (got) {
      break;
    }
    pending_cv_.wait_until(lock, std::chrono::steady_clock::now() +
                                     std::chrono::milliseconds(30));
  }
  if (!got) {
    std::lock_guard<std::mutex> plock(pending_mu_);
    pending_.erase(seq);
    return "{\"seq\":" + std::to_string(seq) + ",\"ok\":false,\"cmd\":\"" +
           cmd_name + "\",\"status\":\"REJECTED\",\"reason\":\"timeout\"}";
  }
  return ack;
}

}  // namespace botcockpit
