cmake_minimum_required(VERSION 3.16)
project(botcockpit_bridge)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)

add_executable(bridge
  src/frame_codec.cpp
  src/json_util.cpp
  src/tcp_server.cpp
  src/bridge_node.cpp
  src/main.cpp
)

target_include_directories(bridge PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

ament_target_dependencies(bridge rclcpp std_msgs)

install(TARGETS bridge
  DESTINATION lib/${PROJECT_NAME}
)

ament_package()
