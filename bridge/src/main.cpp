#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include "botcockpit_bridge/bridge_node.hpp"
#include "botcockpit_bridge/protocol.hpp"

int main(int argc, char** argv)
{
  int port = botcockpit::DEFAULT_PORT;

  // Parse --port / -p before rclcpp so unknown args are not passed through.
  std::vector<char*> ros_argv;
  ros_argv.push_back(argv[0]);
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if ((arg == "--port" || arg == "-p") && i + 1 < argc) {
      port = std::atoi(argv[++i]);
      continue;
    }
    ros_argv.push_back(argv[i]);
  }
  int ros_argc = static_cast<int>(ros_argv.size());
  rclcpp::init(ros_argc, ros_argv.data());

  auto node = std::make_shared<botcockpit::BridgeNode>();
  if (!node->start_tcp(port)) {
    RCLCPP_FATAL(node->get_logger(), "failed to start TCP server on %d", port);
    node.reset();
    rclcpp::shutdown();
    return 1;
  }

  RCLCPP_INFO(node->get_logger(),
              "botcockpit_bridge/0.1.0 listening TCP :%d (Ctrl+C to stop)", port);

  // Use default ROS2 SIGINT handling: spin returns after rclcpp context stops.
  // Do NOT install a custom signal handler that skips shutdown — spin never exits.
  rclcpp::spin(node);

  node->stop_tcp();
  node.reset();
  if (rclcpp::ok()) {
    rclcpp::shutdown();
  }
  RCLCPP_INFO(rclcpp::get_logger("botcockpit_bridge"), "bridge stopped");
  return 0;
}
