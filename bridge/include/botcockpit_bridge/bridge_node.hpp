#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "botcockpit_bridge/tcp_server.hpp"
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"

namespace botcockpit {

class BridgeNode : public rclcpp::Node {
 public:
  explicit BridgeNode(const rclcpp::NodeOptions& options = rclcpp::NodeOptions());
  ~BridgeNode() override;

  bool start_tcp(int port);
  void stop_tcp();

  // JSON for TCP clients: ONLINE/ESTOP/... from robot when fresh, else OFFLINE.
  std::string state_for_tcp() const;
  bool robot_state_fresh() const;

 private:
  void on_robot_state(const std_msgs::msg::String::SharedPtr msg);
  void on_cmd_result(const std_msgs::msg::String::SharedPtr msg);
  void delta_timer_cb();

  std::string handle_command(const std::string& cmd_name, uint16_t seq,
                             const std::string& payload);
  std::string wait_cmd_result(uint16_t seq, const std::string& cmd_name);
  void log(const std::string& line);
  static uint64_t now_ms();

  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr state_sub_;
  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr cmd_result_sub_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr cmd_pub_;
  rclcpp::TimerBase::SharedPtr delta_timer_;

  mutable std::mutex state_mu_;
  std::string robot_state_json_{"{}"};
  std::atomic<uint64_t> last_robot_state_ms_{0};

  std::mutex pending_mu_;
  struct PendingCmd {
    bool done = false;
    std::string ack_json;
  };
  std::map<uint16_t, PendingCmd> pending_;
  std::mutex pending_cv_mu_;
  std::condition_variable pending_cv_;

  std::unique_ptr<TcpServer> server_;
  std::atomic<bool> cmd_forward_{true};
};

}  // namespace botcockpit
