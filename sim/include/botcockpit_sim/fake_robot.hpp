#pragma once

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "rclcpp/rclcpp.hpp"
#include "rclcpp_lifecycle/lifecycle_node.hpp"
#include "std_msgs/msg/string.hpp"

namespace botcockpit_sim {

using CallbackReturn =
    rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn;

// PROTOCOL.md §4 state machine (week 2):
//   BOOT -> INITIALIZING -> IDLE ⇄ RUNNING -> FAULT/DEGRADED
//   ESTOP highest priority; CMD_RESET exits when no ERROR fault source.
class FakeRobot : public rclcpp_lifecycle::LifecycleNode {
 public:
  FakeRobot();
  ~FakeRobot() override;

  CallbackReturn on_configure(const rclcpp_lifecycle::State&) override;
  CallbackReturn on_activate(const rclcpp_lifecycle::State&) override;
  CallbackReturn on_deactivate(const rclcpp_lifecycle::State&) override;
  CallbackReturn on_cleanup(const rclcpp_lifecycle::State&) override;
  CallbackReturn on_shutdown(const rclcpp_lifecycle::State&) override;

 private:
  struct NodeHealth {
    std::string name;
    std::string status = "OK";  // OK/WARN/ERROR/LOST
  };

  struct FaultItem {
    std::string code;
    std::string level;  // INFO/WARN/ERROR
    std::string node;
    std::string detail;
    bool active = true;
  };

  void tick();
  void on_cmd(const std_msgs::msg::String::SharedPtr msg);
  void on_console(const std_msgs::msg::String::SharedPtr msg);
  void publish_state();
  void publish_cmd_result(uint16_t seq, const std::string& cmd, bool ok,
                          const std::string& status,
                          const std::string& reason,
                          const std::string& task_id = "");
  std::string build_state_json() const;
  void inject_fault(const std::string& detail);
  void clear_faults();
  void apply_safety_stop(const std::string& why);
  void recompute_phase_and_control();
  bool has_error_condition() const;
  bool has_warn_condition() const;
  static uint64_t now_ms();
  static double dist(double x0, double y0, double x1, double y1);

  rclcpp::TimerBase::SharedPtr tick_timer_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr state_pub_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr result_pub_;
  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr cmd_sub_;
  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr console_sub_;

  mutable std::mutex mu_;
  bool active_ = false;
  // BOOT/INITIALIZING/IDLE/RUNNING/DEGRADED/FAULT/ESTOP
  std::string phase_ = "BOOT";
  std::string mode_ = "TELEOP";
  bool estop_ = false;
  bool control_enabled_ = false;
  bool console_online_ = false;
  uint64_t last_console_ms_ = 0;

  double pose_x_ = 0.0;
  double pose_y_ = 0.0;
  double pose_yaw_ = 0.0;
  double battery_ = 100.0;

  std::string task_id_;
  std::string task_type_;
  std::string task_status_ = "NONE";
  double goal_x_ = 0.0;
  double goal_y_ = 0.0;

  std::vector<NodeHealth> nodes_;
  std::vector<FaultItem> faults_;

  std::chrono::steady_clock::time_point idle_tp_{};
  std::chrono::steady_clock::time_point last_tick_tp_{};
};

}  // namespace botcockpit_sim
