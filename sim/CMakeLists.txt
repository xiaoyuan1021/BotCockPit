cmake_minimum_required(VERSION 3.16)
project(botcockpit_sim)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_lifecycle REQUIRED)
find_package(lifecycle_msgs REQUIRED)
find_package(std_msgs REQUIRED)

add_executable(fake_robot
  src/fake_robot.cpp
  src/main.cpp
)

target_include_directories(fake_robot PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

ament_target_dependencies(fake_robot
  rclcpp
  rclcpp_lifecycle
  lifecycle_msgs
  std_msgs
)

install(TARGETS fake_robot
  DESTINATION lib/${PROJECT_NAME}
)

ament_package()
