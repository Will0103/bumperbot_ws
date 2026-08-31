cmake_minimum_required(VERSION 3.8)
project(bumperbot_utils)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()


find_package(ament_cmake REQUIRED)


find_package(rclcpp REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)


add_executable(bumperbot_trajectory src/bumperbot_trajectory.cpp)
ament_target_dependencies(bumperbot_trajectory rclcpp nav_msgs geometry_msgs)




install(TARGETS
  bumperbot_trajectory

  DESTINATION lib/${PROJECT_NAME}
)


ament_package()
